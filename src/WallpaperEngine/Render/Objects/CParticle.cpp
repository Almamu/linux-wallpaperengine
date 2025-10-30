#include "CParticle.h"
#include "WallpaperEngine/Logging/Log.h"

#include <GL/glew.h>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>
#include <unordered_set>

extern float g_Time;

using namespace WallpaperEngine::Render::Objects;
using namespace WallpaperEngine::Data::Model;

namespace {
    // Helper: Random float in range
    inline float randomFloat (std::mt19937& rng, float min, float max) {
        std::uniform_real_distribution<float> dist (min, max);
        return dist (rng);
    }

    // Helper: Random vec3 in range
    inline glm::vec3 randomVec3 (std::mt19937& rng, const glm::vec3& min, const glm::vec3& max) {
        return glm::vec3 (
            randomFloat (rng, min.x, max.x),
            randomFloat (rng, min.y, max.y),
            randomFloat (rng, min.z, max.z)
        );
    }

    // Helper: Linear interpolation
    inline float lerp (float t, float a, float b) {
        return a + t * (b - a);
    }

    // Helper: Fade value change over lifetime
    inline float fadeValue (float life, float startTime, float endTime, float startValue, float endValue) {
        if (life <= startTime)
            return startValue;
        else if (life >= endTime)
            return endValue;
        else {
            float t = (life - startTime) / (endTime - startTime);
            return lerp (t, startValue, endValue);
        }
    }
}

CParticle::CParticle (Wallpapers::CScene& scene, const Particle& particle) :
    CObject (scene, particle),
    m_particle (particle) {
    // Initialize random number generator with time-based seed
    std::random_device rd;
    m_rng.seed (rd ());

    // Reserve particle pool
    m_particles.resize (std::min (particle.maxCount, MAX_PARTICLES));
}

void CParticle::setup () {
    if (m_initialized)
        return;

    // Convert origin from screen space to centered space
    // Projection uses ortho(-width/2, width/2, -height/2, height/2)
    // but particle origins are in screen space where (0,0) is top-left
    float screenWidth = getScene ().getCamera ().getWidth ();
    float screenHeight = getScene ().getCamera ().getHeight ();

    glm::vec3 origin = m_particle.origin->value->getVec3 ();
    origin.x -= screenWidth / 2.0f;
    origin.y = screenHeight / 2.0f - origin.y;
    m_particle.origin->value->update (origin);

    // Load particle material texture and blending mode
    if (m_particle.material && m_particle.material->material && !m_particle.material->material->passes.empty ()) {
        auto& firstPass = *m_particle.material->material->passes.begin ();

        m_blendingMode = firstPass->blending;

        auto& textures = firstPass->textures;
        if (!textures.empty ()) {
            std::string textureName = textures.begin ()->second;
            if (textureName.find ("_rt_") == 0 || textureName.find ("_alias_") == 0) {
                m_texture = getScene ().findFBO (textureName);
            } else {
                m_texture = getContext ().resolveTexture (textureName);
            }
            if (m_texture) {
                m_textureFormat = m_texture->getFormat ();
            }
        }
    }

    setupEmitters ();
    setupInitializers ();
    setupOperators ();
    setupBuffers ();

    // Setup control points (max 8)
    m_controlPoints.resize (8);
    for (const auto& cp : m_particle.controlPoints) {
        if (cp.id >= 0 && cp.id < 8) {
            m_controlPoints [cp.id].offset = cp.offset;
            m_controlPoints [cp.id].linkMouse = (cp.flags & 1) != 0;
            m_controlPoints [cp.id].worldSpace = (cp.flags & 2) != 0;
        }
    }

    m_initialized = true;
}

void CParticle::render () {
    if (!m_initialized || !m_particle.visible->value->getBool ())
        return;

    // Update particles
    float dt = g_Time - static_cast<float> (m_time);
    m_time = g_Time;

    if (dt > 0.0f && dt < 1.0f) {
        update (dt);
    }

    // Render particles
    if (m_particleCount > 0 && m_particle.material) {
        renderSprites ();
    }
}

void CParticle::update (float dt) {
    // Emit particles
    for (auto& emitter : m_emitters) {
        emitter (m_particles, m_particleCount, dt);
    }

    // Update particle lifetime and remove dead particles
    for (uint32_t i = 0; i < m_particleCount; ) {
        auto& p = m_particles [i];
        p.age += dt;

        if (!p.isAlive ()) {
            // Swap with last particle and reduce count
            if (i < m_particleCount - 1) {
                p = m_particles [m_particleCount - 1];
            }
            m_particleCount--;
        } else {
            i++;
        }
    }

    // Apply operators to living particles
    for (auto& op : m_operators) {
        op (m_particles, m_controlPoints, static_cast<float> (m_time), dt);
    }
}

const Particle& CParticle::getParticle () const {
    return m_particle;
}

// ========== EMITTERS ==========

void CParticle::setupEmitters () {
    for (const auto& emitter : m_particle.emitters) {
        EmitterFunc func;

        if (emitter.name == "boxrandom") {
            func = createBoxEmitter (emitter);
        } else if (emitter.name == "sphererandom") {
            func = createSphereEmitter (emitter);
        } else {
            sLog.out ("Unknown emitter type: ", emitter.name);
            continue;
        }

        if (func) {
            m_emitters.push_back (std::move (func));
        }
    }
}

EmitterFunc CParticle::createBoxEmitter (const ParticleEmitter& emitter) {
    float rate = emitter.rate * m_particle.instanceOverride.rate;
    float lifetime = 1.0f * m_particle.instanceOverride.lifetime;

    return [this, emitter, rate, lifetime, emissionTimer = 0.0f](std::vector<ParticleInstance>& particles, uint32_t& count, float dt) mutable {
        if (count >= particles.size ())
            return;

        emissionTimer += dt * rate;

        uint32_t toEmit = static_cast<uint32_t> (emissionTimer);
        emissionTimer -= static_cast<float> (toEmit);

        if (emitter.instantaneous > 0) {
            toEmit = emitter.instantaneous;
        }

        for (uint32_t i = 0; i < toEmit && count < particles.size (); i++) {
            auto& p = particles [count];

            // Spawn at random position within box volume
            glm::vec3 randomPos = randomVec3 (m_rng, emitter.distanceMin, emitter.distanceMax);
            p.position = emitter.origin + randomPos;

            // Velocity based on position direction and emitter settings
            glm::vec3 direction = glm::length (randomPos) > 0.0f ? glm::normalize (randomPos) : glm::vec3 (0, 1, 0);
            direction = direction * emitter.directions;

            float speed = randomFloat (m_rng, emitter.speedMin, emitter.speedMax);
            p.velocity = direction * speed;

            p.acceleration = glm::vec3 (0.0f);
            p.rotation = glm::vec3 (0.0f);
            p.angularVelocity = glm::vec3 (0.0f);
            p.angularAcceleration = glm::vec3 (0.0f);

            // Default properties (will be overridden by initializers)
            p.color = glm::vec3 (1.0f);
            p.alpha = 1.0f;
            p.size = 20.0f;
            p.lifetime = lifetime;
            p.age = 0.0f;
            p.alive = true;

            // Store initial values
            p.initial.color = p.color;
            p.initial.alpha = p.alpha;
            p.initial.size = p.size;
            p.initial.lifetime = p.lifetime;

            // Apply initializers
            for (auto& init : m_initializers) {
                init (p);
            }

            count++;
        }
    };
}

EmitterFunc CParticle::createSphereEmitter (const ParticleEmitter& emitter) {
    float rate = emitter.rate * m_particle.instanceOverride.rate;
    float lifetime = 1.0f * m_particle.instanceOverride.lifetime;

    return [this, emitter, rate, lifetime, emissionTimer = 0.0f](std::vector<ParticleInstance>& particles, uint32_t& count, float dt) mutable {
        if (count >= particles.size ())
            return;

        emissionTimer += dt * rate;

        uint32_t toEmit = static_cast<uint32_t> (emissionTimer);
        emissionTimer -= static_cast<float> (toEmit);

        if (emitter.instantaneous > 0) {
            toEmit = emitter.instantaneous;
        }

        for (uint32_t i = 0; i < toEmit && count < particles.size (); i++) {
            auto& p = particles [count];

            // Spawn at random position within sphere volume
            float theta = randomFloat (m_rng, 0.0f, 2.0f * M_PI);
            float phi = randomFloat (m_rng, 0.0f, M_PI);
            float radius = randomFloat (m_rng, emitter.distanceMin.x, emitter.distanceMax.x);

            glm::vec3 randomPos (
                radius * std::sin (phi) * std::cos (theta),
                radius * std::sin (phi) * std::sin (theta),
                radius * std::cos (phi)
            );
            p.position = emitter.origin + randomPos;

            // Velocity pointing outward from sphere center
            glm::vec3 direction = glm::normalize (randomPos);
            float speed = randomFloat (m_rng, emitter.speedMin, emitter.speedMax);
            p.velocity = direction * speed * emitter.directions;

            p.acceleration = glm::vec3 (0.0f);
            p.rotation = glm::vec3 (0.0f);
            p.angularVelocity = glm::vec3 (0.0f);
            p.angularAcceleration = glm::vec3 (0.0f);

            p.color = glm::vec3 (1.0f);
            p.alpha = 1.0f;
            p.size = 20.0f;
            p.lifetime = lifetime;
            p.age = 0.0f;
            p.alive = true;

            p.initial.color = p.color;
            p.initial.alpha = p.alpha;
            p.initial.size = p.size;
            p.initial.lifetime = p.lifetime;

            for (auto& init : m_initializers) {
                init (p);
            }

            count++;
        }
    };
}

// ========== INITIALIZERS ==========

void CParticle::setupInitializers () {
    for (const auto& initializer : m_particle.initializers) {
        InitializerFunc func;

        const auto& name = initializer.name;
        const auto& json = initializer.json;

        if (name == "colorrandom") {
            func = createColorRandomInitializer (json);
        } else if (name == "sizerandom") {
            func = createSizeRandomInitializer (json);
        } else if (name == "alpharandom") {
            func = createAlphaRandomInitializer (json);
        } else if (name == "lifetimerandom") {
            func = createLifetimeRandomInitializer (json);
        } else if (name == "velocityrandom") {
            func = createVelocityRandomInitializer (json);
        } else if (name == "rotationrandom") {
            func = createRotationRandomInitializer (json);
        } else if (name == "angularvelocityrandom") {
            func = createAngularVelocityRandomInitializer (json);
        }

        if (func) {
            m_initializers.push_back (std::move (func));
        }
    }
}

InitializerFunc CParticle::createColorRandomInitializer (const JSON& json) {
    glm::vec3 min = json.optional ("min", glm::vec3 (0.0f));
    glm::vec3 max = json.optional ("max", glm::vec3 (255.0f));

    // Convert from 0-255 to 0-1
    min /= 255.0f;
    max /= 255.0f;

    return [this, min, max](ParticleInstance& p) {
        p.color = randomVec3 (m_rng, min, max);
        p.initial.color = p.color;
    };
}

InitializerFunc CParticle::createSizeRandomInitializer (const JSON& json) {
    float min = json.optional ("min", 0.0f);
    float max = json.optional ("max", 20.0f);

    return [this, min, max](ParticleInstance& p) {
        p.size = randomFloat (m_rng, min, max) * m_particle.instanceOverride.size;
        p.initial.size = p.size;
    };
}

InitializerFunc CParticle::createAlphaRandomInitializer (const JSON& json) {
    float min = json.optional ("min", 0.05f);
    float max = json.optional ("max", 1.0f);

    return [this, min, max](ParticleInstance& p) {
        p.alpha = randomFloat (m_rng, min, max) * m_particle.instanceOverride.alpha;
        p.initial.alpha = p.alpha;
    };
}

InitializerFunc CParticle::createLifetimeRandomInitializer (const JSON& json) {
    float min = json.optional ("min", 0.0f);
    float max = json.optional ("max", 1.0f);

    return [this, min, max](ParticleInstance& p) {
        p.lifetime = randomFloat (m_rng, min, max) * m_particle.instanceOverride.lifetime;
        p.initial.lifetime = p.lifetime;
    };
}

InitializerFunc CParticle::createVelocityRandomInitializer (const JSON& json) {
    glm::vec3 min = json.optional ("min", glm::vec3 (-32.0f));
    glm::vec3 max = json.optional ("max", glm::vec3 (32.0f));

    return [this, min, max](ParticleInstance& p) {
        glm::vec3 vel = randomVec3 (m_rng, min, max);
        p.velocity += vel * m_particle.instanceOverride.speed;
    };
}

InitializerFunc CParticle::createRotationRandomInitializer (const JSON& json) {
    glm::vec3 min = json.optional ("min", glm::vec3 (0.0f));
    glm::vec3 max = json.optional ("max", glm::vec3 (0.0f, 0.0f, 2.0f * M_PI));

    return [this, min, max](ParticleInstance& p) {
        p.rotation = randomVec3 (m_rng, min, max);
    };
}

InitializerFunc CParticle::createAngularVelocityRandomInitializer (const JSON& json) {
    glm::vec3 min = json.optional ("min", glm::vec3 (0.0f, 0.0f, -5.0f));
    glm::vec3 max = json.optional ("max", glm::vec3 (0.0f, 0.0f, 5.0f));

    return [this, min, max](ParticleInstance& p) {
        p.angularVelocity = randomVec3 (m_rng, min, max);
    };
}

// ========== OPERATORS ==========

void CParticle::setupOperators () {
    for (const auto& op : m_particle.operators) {
        OperatorFunc func;

        const auto& name = op.name;
        const auto& json = op.json;

        if (name == "movement") {
            func = createMovementOperator (json);
        } else if (name == "angularmovement") {
            func = createAngularMovementOperator (json);
        } else if (name == "alphafade") {
            func = createAlphaFadeOperator (json);
        } else if (name == "sizechange") {
            func = createSizeChangeOperator (json);
        } else if (name == "alphachange") {
            func = createAlphaChangeOperator (json);
        } else if (name == "colorchange") {
            func = createColorChangeOperator (json);
        }

        if (func) {
            m_operators.push_back (std::move (func));
        }
    }
}

OperatorFunc CParticle::createMovementOperator (const JSON& json) {
    float drag = json.optional ("drag", 0.0f);
    glm::vec3 gravity = json.optional ("gravity", glm::vec3 (0.0f));
    float speed = m_particle.instanceOverride.speed;

    return [drag, gravity, speed](
        std::vector<ParticleInstance>& particles,
        const std::vector<ControlPointData>&,
        float,
        float dt
    ) {
        for (uint32_t i = 0; i < particles.size (); i++) {
            auto& p = particles [i];
            if (!p.alive)
                continue;

            // Apply drag force
            glm::vec3 dragForce = -drag * p.velocity;

            // Total acceleration
            glm::vec3 totalAccel = (dragForce + gravity) * speed;

            // Update velocity and position
            p.velocity += totalAccel * dt;
            p.position += p.velocity * dt;
        }
    };
}

OperatorFunc CParticle::createAngularMovementOperator (const JSON& json) {
    float drag = json.optional ("drag", 0.0f);
    glm::vec3 force = json.optional ("force", glm::vec3 (0.0f));

    return [drag, force](
        std::vector<ParticleInstance>& particles,
        const std::vector<ControlPointData>&,
        float,
        float dt
    ) {
        for (uint32_t i = 0; i < particles.size (); i++) {
            auto& p = particles [i];
            if (!p.alive)
                continue;

            glm::vec3 dragForce = -drag * p.angularVelocity;
            glm::vec3 totalAccel = dragForce + force;

            p.angularVelocity += totalAccel * dt;
            p.rotation += p.angularVelocity * dt;
        }
    };
}

OperatorFunc CParticle::createAlphaFadeOperator (const JSON& json) {
    float fadeInTime = json.optional ("fadeintime", 0.5f);
    float fadeOutTime = json.optional ("fadeouttime", 0.5f);

    return [fadeInTime, fadeOutTime](
        std::vector<ParticleInstance>& particles,
        const std::vector<ControlPointData>&,
        float,
        float
    ) {
        for (uint32_t i = 0; i < particles.size (); i++) {
            auto& p = particles [i];
            if (!p.alive)
                continue;

            float life = p.getLifetimePos ();

            if (life <= fadeInTime) {
                float fade = fadeValue (life, 0.0f, fadeInTime, 0.0f, 1.0f);
                p.alpha = p.initial.alpha * fade;
            } else if (life > fadeOutTime) {
                float fade = 1.0f - fadeValue (life, fadeOutTime, 1.0f, 0.0f, 1.0f);
                p.alpha = p.initial.alpha * fade;
            } else {
                p.alpha = p.initial.alpha;
            }
        }
    };
}

OperatorFunc CParticle::createSizeChangeOperator (const JSON& json) {
    float startTime = json.optional ("starttime", 0.0f);
    float endTime = json.optional ("endtime", 1.0f);
    float startValue = json.optional ("startvalue", 1.0f);
    float endValue = json.optional ("endvalue", 0.0f);

    return [startTime, endTime, startValue, endValue](
        std::vector<ParticleInstance>& particles,
        const std::vector<ControlPointData>&,
        float,
        float
    ) {
        for (uint32_t i = 0; i < particles.size (); i++) {
            auto& p = particles [i];
            if (!p.alive)
                continue;

            float life = p.getLifetimePos ();
            float multiplier = fadeValue (life, startTime, endTime, startValue, endValue);
            p.size = p.initial.size * multiplier;
        }
    };
}

OperatorFunc CParticle::createAlphaChangeOperator (const JSON& json) {
    float startTime = json.optional ("starttime", 0.0f);
    float endTime = json.optional ("endtime", 1.0f);
    float startValue = json.optional ("startvalue", 1.0f);
    float endValue = json.optional ("endvalue", 0.0f);

    return [startTime, endTime, startValue, endValue](
        std::vector<ParticleInstance>& particles,
        const std::vector<ControlPointData>&,
        float,
        float
    ) {
        for (uint32_t i = 0; i < particles.size (); i++) {
            auto& p = particles [i];
            if (!p.alive)
                continue;

            float life = p.getLifetimePos ();
            float multiplier = fadeValue (life, startTime, endTime, startValue, endValue);
            p.alpha = p.initial.alpha * multiplier;
        }
    };
}

OperatorFunc CParticle::createColorChangeOperator (const JSON& json) {
    float startTime = json.optional ("starttime", 0.0f);
    float endTime = json.optional ("endtime", 1.0f);
    glm::vec3 startValue = json.optional ("startvalue", glm::vec3 (1.0f));
    glm::vec3 endValue = json.optional ("endvalue", glm::vec3 (1.0f));

    return [startTime, endTime, startValue, endValue](
        std::vector<ParticleInstance>& particles,
        const std::vector<ControlPointData>&,
        float,
        float
    ) {
        for (uint32_t i = 0; i < particles.size (); i++) {
            auto& p = particles [i];
            if (!p.alive)
                continue;

            float life = p.getLifetimePos ();

            glm::vec3 color;
            color.r = fadeValue (life, startTime, endTime, startValue.r, endValue.r);
            color.g = fadeValue (life, startTime, endTime, startValue.g, endValue.g);
            color.b = fadeValue (life, startTime, endTime, startValue.b, endValue.b);

            p.color = p.initial.color * color;
        }
    };
}

// ========== RENDERING ==========

GLuint CParticle::compileShader (GLenum type, const char* source) {
    GLuint shader = glCreateShader (type);
    glShaderSource (shader, 1, &source, nullptr);
    glCompileShader (shader);

    GLint success;
    glGetShaderiv (shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog (shader, 512, nullptr, infoLog);
        sLog.error ("Particle shader compilation failed: ", infoLog);
        return 0;
    }
    return shader;
}

GLuint CParticle::createShaderProgram () {
    const char* vertexShaderSource = R"(
        #version 330 core
        layout (location = 0) in vec3 aPos;
        layout (location = 1) in vec2 aTexCoord;
        layout (location = 2) in float aRotation;
        layout (location = 3) in float aSize;
        layout (location = 4) in vec4 aColor;

        out vec2 vTexCoord;
        out vec4 vColor;

        uniform mat4 g_ModelViewProjectionMatrix;

        void main() {
            // Calculate billboard offset based on texture coordinates
            // Offset from center: (0,0)-(1,1) becomes (-0.5,-0.5)-(0.5,0.5)
            vec2 offset = aTexCoord - 0.5;

            // Apply rotation around Z axis
            float c = cos(aRotation);
            float s = sin(aRotation);
            vec2 rotated = vec2(
                offset.x * c - offset.y * s,
                offset.x * s + offset.y * c
            );

            // Scale by particle size
            vec3 billboardPos = aPos + vec3(rotated * aSize, 0.0);

            gl_Position = g_ModelViewProjectionMatrix * vec4(billboardPos, 1.0);
            vTexCoord = aTexCoord;
            vColor = aColor;
        }
    )";

    const char* fragmentShaderSource = R"(
        #version 330 core
        in vec2 vTexCoord;
        in vec4 vColor;

        out vec4 FragColor;

        uniform sampler2D g_Texture0;
        uniform int u_HasTexture;
        uniform int u_TextureFormat; // 8 = RG88

        void main() {
            vec4 texColor;
            if (u_HasTexture == 1) {
                // Sample texture
                vec4 sample = texture(g_Texture0, vTexCoord);

                // Convert texture format like common_fragment.h does
                if (u_TextureFormat == 8) {
                    // RG88: R channel is color (grayscale), G channel is alpha
                    texColor = vec4(sample.rrr, sample.g);
                } else if (u_TextureFormat == 9) {
                    // R8: R channel is alpha, white color
                    texColor = vec4(1.0, 1.0, 1.0, sample.r);
                } else {
                    // Normal RGBA
                    texColor = sample;
                }
            } else {
                // No texture - create circular particle fallback
                vec2 coord = vTexCoord - vec2(0.5);
                float dist = length(coord) * 2.0;
                if (dist > 1.0) discard;
                float alpha = 1.0 - dist;
                texColor = vec4(1.0, 1.0, 1.0, alpha);
            }
            FragColor = vColor * texColor;
        }
    )";

    GLuint vertexShader = compileShader (GL_VERTEX_SHADER, vertexShaderSource);
    GLuint fragmentShader = compileShader (GL_FRAGMENT_SHADER, fragmentShaderSource);

    if (vertexShader == 0 || fragmentShader == 0) {
        return 0;
    }

    GLuint program = glCreateProgram ();
    glAttachShader (program, vertexShader);
    glAttachShader (program, fragmentShader);
    glLinkProgram (program);

    GLint success;
    glGetProgramiv (program, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog (program, 512, nullptr, infoLog);
        sLog.error ("Particle shader program linking failed: ", infoLog);
        glDeleteShader (vertexShader);
        glDeleteShader (fragmentShader);
        return 0;
    }

    glDeleteShader (vertexShader);
    glDeleteShader (fragmentShader);

    return program;
}

void CParticle::setupBuffers () {
    // Create shader program
    m_shaderProgram = createShaderProgram ();
    if (m_shaderProgram == 0) {
        sLog.error ("Failed to create particle shader program for ", m_particle.name);
        return;
    }

    glGenVertexArrays (1, &m_vao);
    glGenBuffers (1, &m_vbo);

    glBindVertexArray (m_vao);
    glBindBuffer (GL_ARRAY_BUFFER, m_vbo);

    // Vertex format: pos(3) + texcoord(2) + rotation(1) + size(1) + color(4) = 11 floats
    const int stride = sizeof (float) * 11;

    // Position (location 0)
    glEnableVertexAttribArray (0);
    glVertexAttribPointer (0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);

    // Texture coordinates (location 1)
    glEnableVertexAttribArray (1);
    glVertexAttribPointer (1, 2, GL_FLOAT, GL_FALSE, stride, (void*)(sizeof (float) * 3));

    // Rotation (location 2)
    glEnableVertexAttribArray (2);
    glVertexAttribPointer (2, 1, GL_FLOAT, GL_FALSE, stride, (void*)(sizeof (float) * 5));

    // Size (location 3)
    glEnableVertexAttribArray (3);
    glVertexAttribPointer (3, 1, GL_FLOAT, GL_FALSE, stride, (void*)(sizeof (float) * 6));

    // Color (location 4) - includes alpha as 4th component
    glEnableVertexAttribArray (4);
    glVertexAttribPointer (4, 4, GL_FLOAT, GL_FALSE, stride, (void*)(sizeof (float) * 7));

    glBindVertexArray (0);
}

void CParticle::renderSprites () {
    if (m_particleCount == 0)
        return;

    // Count alive particles
    uint32_t aliveCount = 0;
    for (uint32_t i = 0; i < m_particleCount; i++) {
        if (m_particles[i].alive) aliveCount++;
    }

    if (aliveCount == 0)
        return;

    // Prepare vertex data - 6 vertices per particle (2 triangles forming a quad)
    // Vertex format: pos(3) + texcoord(2) + rotation(1) + size(1) + color(4) = 11 floats per vertex
    std::vector<float> vertices;
    vertices.reserve (aliveCount * 6 * 11);

    for (uint32_t i = 0; i < m_particleCount; i++) {
        const auto& p = m_particles [i];
        if (!p.alive)
            continue;

        float size = p.size / 2.0f;
        float rz = p.rotation.z;

        // Create 6 vertices forming 2 triangles (GL_QUADS not available in core profile)

        auto addVertex = [&](float u, float v) {
            vertices.push_back (p.position.x);
            vertices.push_back (p.position.y);
            vertices.push_back (p.position.z);
            vertices.push_back (u);
            vertices.push_back (v);
            vertices.push_back (rz);
            vertices.push_back (size);
            vertices.push_back (p.color.r);
            vertices.push_back (p.color.g);
            vertices.push_back (p.color.b);
            vertices.push_back (p.alpha);
        };

        // Triangle 1
        addVertex (0.0f, 1.0f);  // Bottom-left
        addVertex (1.0f, 1.0f);  // Bottom-right
        addVertex (1.0f, 0.0f);  // Top-right

        // Triangle 2
        addVertex (1.0f, 0.0f);  // Top-right
        addVertex (0.0f, 0.0f);  // Top-left
        addVertex (0.0f, 1.0f);  // Bottom-left
    }

    // Upload to GPU
    glBindBuffer (GL_ARRAY_BUFFER, m_vbo);
    glBufferData (GL_ARRAY_BUFFER, vertices.size () * sizeof (float), vertices.data (), GL_DYNAMIC_DRAW);

    if (m_shaderProgram == 0) {
        return; // Shader not available
    }

    // Clear any existing GL errors before we start
    while (glGetError () != GL_NO_ERROR);

    // Save current GL state
    GLint prevProgram = 0;
    GLint prevVAO = 0;
    GLint prevTexture = 0;
    GLboolean prevBlendEnabled = glIsEnabled (GL_BLEND);
    GLboolean prevDepthMask = GL_TRUE;
    glGetIntegerv (GL_CURRENT_PROGRAM, &prevProgram);
    glGetIntegerv (GL_VERTEX_ARRAY_BINDING, &prevVAO);
    glGetIntegerv (GL_TEXTURE_BINDING_2D, &prevTexture);
    glGetBooleanv (GL_DEPTH_WRITEMASK, &prevDepthMask);

    // Use particle shader
    glUseProgram (m_shaderProgram);

    // Bind particle texture
    GLint hasTextureLoc = glGetUniformLocation (m_shaderProgram, "u_HasTexture");
    GLint texFormatLoc = glGetUniformLocation (m_shaderProgram, "u_TextureFormat");
    if (m_texture) {
        glActiveTexture (GL_TEXTURE0);
        glBindTexture (GL_TEXTURE_2D, m_texture->getTextureID (0));
        GLint texLoc = glGetUniformLocation (m_shaderProgram, "g_Texture0");
        if (texLoc != -1) {
            glUniform1i (texLoc, 0);
        }
        if (hasTextureLoc != -1) {
            glUniform1i (hasTextureLoc, 1);
        }
        if (texFormatLoc != -1) {
            glUniform1i (texFormatLoc, static_cast<int> (m_textureFormat));
        }
    } else {
        if (hasTextureLoc != -1) {
            glUniform1i (hasTextureLoc, 0);
        }
    }

    // Build model matrix from particle object transform
    glm::vec3 origin = m_particle.origin->value->getVec3 ();
    glm::vec3 scale = m_particle.scale->value->getVec3 ();
    glm::vec3 angles = m_particle.angles->value->getVec3 ();

    glm::mat4 model = glm::mat4 (1.0f);
    model = glm::translate (model, origin);
    model = glm::rotate (model, glm::radians (angles.z), glm::vec3 (0, 0, 1));
    model = glm::rotate (model, glm::radians (angles.y), glm::vec3 (0, 1, 0));
    model = glm::rotate (model, glm::radians (angles.x), glm::vec3 (1, 0, 0));
    model = glm::scale (model, scale);

    // Apply camera transform
    glm::mat4 mvp = getScene ().getCamera ().getProjection () * getScene ().getCamera ().getLookAt () * model;
    GLint mvpLoc = glGetUniformLocation (m_shaderProgram, "g_ModelViewProjectionMatrix");
    if (mvpLoc != -1) {
        glUniformMatrix4fv (mvpLoc, 1, GL_FALSE, &mvp[0][0]);
    }

    // Enable blending for particles
    glEnable (GL_BLEND);
    // Apply blending mode from material
    switch (m_blendingMode) {
        case Data::Model::BlendingMode_Additive:
            glBlendFunc (GL_SRC_ALPHA, GL_ONE);
            break;
        case Data::Model::BlendingMode_Translucent:
            glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            break;
        case Data::Model::BlendingMode_Normal:
        default:
            glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            break;
    }
    glDepthMask (GL_FALSE); // Don't write to depth buffer for transparent particles

    // Render triangles (6 vertices per particle, 2 triangles forming a quad)
    glBindVertexArray (m_vao);
    glDrawArrays (GL_TRIANGLES, 0, aliveCount * 6);
    glBindVertexArray (0);

    // Restore state
    glDepthMask (prevDepthMask);
    if (prevBlendEnabled) {
        glEnable (GL_BLEND);
    } else {
        glDisable (GL_BLEND);
    }
    glUseProgram (prevProgram);
    glBindTexture (GL_TEXTURE_2D, prevTexture);
    glBindVertexArray (prevVAO);
}
