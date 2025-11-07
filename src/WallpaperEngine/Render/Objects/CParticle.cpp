#include "CParticle.h"
#include "WallpaperEngine/Logging/Log.h"
#include "WallpaperEngine/Data/Model/Property.h"
#include "WallpaperEngine/Render/Utils/NoiseUtils.h"

#include <GL/glew.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>
#include <algorithm>
#include <cmath>
#include <unordered_set>

extern float g_Time;

using namespace WallpaperEngine::Render::Objects;
using namespace WallpaperEngine::Render::Utils;
using namespace WallpaperEngine::Data::Model;

namespace {
    // Helper: Random float in range
    inline float randomFloat (std::mt19937& rng, float min, float max) {
        if (max < min) std::swap (min, max);
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

    // Apply count instance override to particle pool size
    float countMultiplier = particle.instanceOverride.count->value->getFloat ();
    uint32_t adjustedMaxCount = static_cast<uint32_t>(particle.maxCount * countMultiplier);
    // Use wallpaper's specified count, or default if maxCount is 0
    m_maxParticles = (adjustedMaxCount > 0) ? adjustedMaxCount : DEFAULT_MAX_PARTICLES;
    m_particles.resize (m_maxParticles);

    sLog.out ("Particle '", particle.name, "' max particles: ", m_maxParticles,
              " (maxCount=", particle.maxCount, " * countMultiplier=", countMultiplier, ")");
}

CParticle::~CParticle () {
    if (m_vao != 0) {
        glDeleteVertexArrays (1, &m_vao);
    }
    if (m_vbo != 0) {
        glDeleteBuffers (1, &m_vbo);
    }
    if (m_shaderProgram != 0) {
        glDeleteProgram (m_shaderProgram);
    }
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
    m_transformedOrigin = origin;

    // Load particle material texture and blending mode
    if (m_particle.material && m_particle.material->material && !m_particle.material->material->passes.empty ()) {
        auto& firstPass = *m_particle.material->material->passes.begin ();

        m_blendingMode = firstPass->blending;

        // Read overbright constant (brightness multiplier for additive particles)
        auto overbrightIt = firstPass->constants.find ("ui_editor_properties_overbright");
        if (overbrightIt != firstPass->constants.end ()) {
            m_overbright = overbrightIt->second->value->getFloat ();
            sLog.out ("Particle '", m_particle.name, "' overbright: ", m_overbright);
        }

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

                // Get spritesheet data from texture (parsed in TextureParser)
                m_spritesheetCols = static_cast<int> (m_texture->getSpritesheetCols ());
                m_spritesheetRows = static_cast<int> (m_texture->getSpritesheetRows ());
                m_spritesheetFrames = static_cast<int> (m_texture->getSpritesheetFrames ());
                m_spritesheetDuration = m_texture->getSpritesheetDuration ();

                sLog.out ("Particle '", m_particle.name, "' texture: ", textureName,
                          " | cols=", m_spritesheetCols, " rows=", m_spritesheetRows,
                          " frames=", m_spritesheetFrames, " duration=", m_spritesheetDuration);
            }
        }
    }

    // Read renderer configuration
    if (!m_particle.renderers.empty ()) {
        const auto& renderer = m_particle.renderers[0];
        if (renderer.name == "spritetrail" || renderer.name == "ropetrail") {
            m_useTrailRenderer = true;
            m_trailLength = renderer.length;
            m_trailMaxLength = renderer.maxLength;
            sLog.out ("Particle '", m_particle.name, "' using trail renderer: length=", m_trailLength, " maxLength=", m_trailMaxLength);
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

    // Initialize time on first render to avoid huge dt spike
    if (m_time == 0.0) {
        m_time = g_Time;
    }

    // Update particles
    float dt = g_Time - static_cast<float> (m_time);
    m_time = g_Time;

    if (dt > 0.0f) {
        dt = std::min (dt, 0.1f);
        update (dt);
    }

    // Render particles
    if (m_particleCount > 0 && m_particle.material) {
        renderSprites ();
    }
}

void CParticle::update (float dt) {
    // Update control points with mouse position
    const glm::vec2* mousePos = getScene().getMousePosition();
    if (mousePos) {
        float screenWidth = static_cast<float>(getScene().getWidth());
        float screenHeight = static_cast<float>(getScene().getHeight());

        for (auto& cp : m_controlPoints) {
            if (cp.linkMouse) {
                if (cp.worldSpace) {
                    // World space: use normalized [0,1] mouse coordinates scaled to screen dimensions
                    cp.position.x = mousePos->x * screenWidth;
                    cp.position.y = mousePos->y * screenHeight;
                    cp.position.z = 0.0f;
                } else {
                    // Centered space: convert from [0,1] to scene coordinates with origin at center
                    cp.position.x = (mousePos->x * screenWidth) - (screenWidth / 2.0f);
                    cp.position.y = (mousePos->y * screenHeight) - (screenHeight / 2.0f);
                    cp.position.z = 0.0f;
                }

                // Apply control point offset
                cp.position += cp.offset;
            }
        }
    }

    // Emit particles
    for (auto& emitter : m_emitters) {
        emitter (m_particles, m_particleCount, dt);
    }

    // Update particle lifetime and remove dead particles
    for (uint32_t i = 0; i < m_particleCount; ) {
        auto& p = m_particles [i];
        p.age += dt;

        // Update animation frame if we have a spritesheet
        if (m_spritesheetFrames > 0) {
            // Calculate frame based on particle lifetime
            float lifetimePos = p.getLifetimePos();

            // Apply sequence multiplier if present
            float animSpeed = m_particle.sequenceMultiplier > 0.0f ? m_particle.sequenceMultiplier : 1.0f;

            // Calculate frame based on animation mode
            if (m_particle.animationMode == "randomframe") {
                // Random frame mode: each particle gets a fixed random frame based on initial lifetime
                p.frame = std::fmod(std::floor(p.initial.lifetime), static_cast<float>(m_spritesheetFrames));
            } else if (m_particle.animationMode == "once") {
                // Play animation once over particle lifetime
                p.frame = std::min(lifetimePos * m_spritesheetFrames * animSpeed, static_cast<float>(m_spritesheetFrames - 1));
            } else {
                // Default to "loop" or "sequence" mode - loop animation based on duration
                if (m_spritesheetDuration > 0.0f) {
                    float timeInCycle = std::fmod(p.age * animSpeed, m_spritesheetDuration);
                    float cyclePos = timeInCycle / m_spritesheetDuration;
                    p.frame = std::fmod(cyclePos * m_spritesheetFrames, static_cast<float>(m_spritesheetFrames));
                } else {
                    // No duration, use lifetime-based for sequence mode
                    p.frame = std::fmod(lifetimePos * m_spritesheetFrames * animSpeed, static_cast<float>(m_spritesheetFrames));
                }
            }
        }

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
        op (m_particles, m_particleCount, m_controlPoints, static_cast<float> (m_time), dt);
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
    float rate = emitter.rate * m_particle.instanceOverride.rate->value->getFloat ();
    float lifetime = 1.0f * m_particle.instanceOverride.lifetime->value->getFloat ();

    // Convert emitter origin from screen space (Y down) to centered space (Y up)
    glm::vec3 transformedEmitterOrigin = emitter.origin;
    transformedEmitterOrigin.y = -transformedEmitterOrigin.y;

    int controlPointIndex = emitter.controlPoint;

    // Auto-detect control point 0 usage if controlPoint field not specified and CP0 has linkMouse
    if (controlPointIndex == -1 && !m_particle.controlPoints.empty()) {
        const auto& cp0 = m_particle.controlPoints[0];
        if ((cp0.flags & 1) != 0) {  // Bit 0: linkMouse flag
            controlPointIndex = 0;
        }
    }

    return [this, emitter, transformedEmitterOrigin, controlPointIndex, rate, lifetime, emissionTimer = 0.0f, remaining = emitter.instantaneous](std::vector<ParticleInstance>& particles, uint32_t& count, float dt) mutable {
        if (count >= particles.size ())
            return;

        emissionTimer += dt * rate;

        uint32_t toEmit = static_cast<uint32_t> (emissionTimer);
        emissionTimer -= static_cast<float> (toEmit);

        if (remaining > 0) {
            toEmit = remaining;
            remaining = 0;
        }

        for (uint32_t i = 0; i < toEmit && count < particles.size (); i++) {
            auto& p = particles [count];

            // Determine spawn origin (control point or emitter origin)
            glm::vec3 spawnOrigin = transformedEmitterOrigin;
            if (controlPointIndex >= 0 && controlPointIndex < static_cast<int>(m_controlPoints.size())) {
                spawnOrigin = m_controlPoints[controlPointIndex].position;
            }

            // Spawn at random position within box volume
            glm::vec3 randomPos = randomVec3 (m_rng, emitter.distanceMin, emitter.distanceMax);
            // Flip Y to convert random offset from screen space to centered space
            randomPos.y = -randomPos.y;
            p.position = spawnOrigin + randomPos;

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
            p.color = glm::vec3 (1.0f) * m_particle.instanceOverride.colorn->value->getVec3 ();
            p.alpha = 1.0f * m_particle.instanceOverride.alpha->value->getFloat ();
            p.size = 20.0f * m_particle.instanceOverride.size->value->getFloat ();
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
    float rate = emitter.rate * m_particle.instanceOverride.rate->value->getFloat ();
    float lifetime = 1.0f * m_particle.instanceOverride.lifetime->value->getFloat ();

    // Convert emitter origin from screen space (Y down) to centered space (Y up)
    glm::vec3 transformedEmitterOrigin = emitter.origin;
    transformedEmitterOrigin.y = -transformedEmitterOrigin.y;

    int controlPointIndex = emitter.controlPoint;

    // Auto-detect control point 0 usage if controlPoint field not specified and CP0 has linkMouse
    if (controlPointIndex == -1 && !m_particle.controlPoints.empty()) {
        const auto& cp0 = m_particle.controlPoints[0];
        if ((cp0.flags & 1) != 0) {  // Bit 0: linkMouse flag
            controlPointIndex = 0;
        }
    }

    return [this, emitter, transformedEmitterOrigin, controlPointIndex, rate, lifetime, emissionTimer = 0.0f, remaining = emitter.instantaneous](std::vector<ParticleInstance>& particles, uint32_t& count, float dt) mutable {
        if (count >= particles.size ())
            return;

        emissionTimer += dt * rate;

        uint32_t toEmit = static_cast<uint32_t> (emissionTimer);
        emissionTimer -= static_cast<float> (toEmit);

        if (remaining > 0) {
            toEmit = remaining;
            remaining = 0;
        }

        for (uint32_t i = 0; i < toEmit && count < particles.size (); i++) {
            auto& p = particles [count];

            // Determine spawn origin (control point or emitter origin)
            glm::vec3 spawnOrigin = transformedEmitterOrigin;
            if (controlPointIndex >= 0 && controlPointIndex < static_cast<int>(m_controlPoints.size())) {
                spawnOrigin = m_controlPoints[controlPointIndex].position;
            }

            // Spawn at random position within sphere volume
            float theta = randomFloat (m_rng, 0.0f, glm::two_pi<float>());
            float phi = randomFloat (m_rng, 0.0f, glm::pi<float>());
            float radius = randomFloat (m_rng, emitter.distanceMin.x, emitter.distanceMax.x);

            glm::vec3 randomPos (
                radius * std::sin (phi) * std::cos (theta),
                radius * std::sin (phi) * std::sin (theta),
                radius * std::cos (phi)
            );
            // Flip Y to convert random offset from screen space to centered space
            randomPos.y = -randomPos.y;
            p.position = spawnOrigin + randomPos;

            // Velocity pointing outward from sphere center
            glm::vec3 direction = glm::length (randomPos) > 0.0f ? glm::normalize (randomPos) : glm::vec3 (0.0f, 1.0f, 0.0f);
            float speed = randomFloat (m_rng, emitter.speedMin, emitter.speedMax);
            p.velocity = direction * speed * emitter.directions;

            p.acceleration = glm::vec3 (0.0f);
            p.rotation = glm::vec3 (0.0f);
            p.angularVelocity = glm::vec3 (0.0f);
            p.angularAcceleration = glm::vec3 (0.0f);

            p.color = glm::vec3 (1.0f) * m_particle.instanceOverride.colorn->value->getVec3 ();
            p.alpha = 1.0f * m_particle.instanceOverride.alpha->value->getFloat ();
            p.size = 20.0f * m_particle.instanceOverride.size->value->getFloat ();
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
        if (!initializer) {
            continue;
        }

        InitializerFunc func;

        if (initializer->is<ColorRandomInitializer> ()) {
            func = createColorRandomInitializer (*initializer->as<ColorRandomInitializer> ());
        } else if (initializer->is<SizeRandomInitializer> ()) {
            func = createSizeRandomInitializer (*initializer->as<SizeRandomInitializer> ());
        } else if (initializer->is<AlphaRandomInitializer> ()) {
            func = createAlphaRandomInitializer (*initializer->as<AlphaRandomInitializer> ());
        } else if (initializer->is<LifetimeRandomInitializer> ()) {
            func = createLifetimeRandomInitializer (*initializer->as<LifetimeRandomInitializer> ());
        } else if (initializer->is<VelocityRandomInitializer> ()) {
            func = createVelocityRandomInitializer (*initializer->as<VelocityRandomInitializer> ());
        } else if (initializer->is<RotationRandomInitializer> ()) {
            func = createRotationRandomInitializer (*initializer->as<RotationRandomInitializer> ());
        } else if (initializer->is<AngularVelocityRandomInitializer> ()) {
            func = createAngularVelocityRandomInitializer (*initializer->as<AngularVelocityRandomInitializer> ());
        } else if (initializer->is<TurbulentVelocityRandomInitializer> ()) {
            func = createTurbulentVelocityRandomInitializer (*initializer->as<TurbulentVelocityRandomInitializer> ());
        } else {
            sLog.out ("Unknown initializer type");
        }

        if (func) {
            m_initializers.push_back (std::move (func));
        }
    }
}

InitializerFunc CParticle::createColorRandomInitializer (const ColorRandomInitializer& init) {
    DynamicValue* minValue = init.min->value.get ();
    DynamicValue* maxValue = init.max->value.get ();

    return [this, minValue, maxValue](ParticleInstance& p) {
        p.color = randomVec3 (m_rng, minValue->getVec3 (), maxValue->getVec3 ()) * m_particle.instanceOverride.colorn->value->getVec3 ();
        p.initial.color = p.color;
    };
}

InitializerFunc CParticle::createSizeRandomInitializer (const SizeRandomInitializer& init) {
    DynamicValue* minValue = init.min->value.get ();
    DynamicValue* maxValue = init.max->value.get ();
    DynamicValue* exponentValue = init.exponent->value.get ();

    return [this, minValue, maxValue, exponentValue](ParticleInstance& p) {
        float t = randomFloat (m_rng, 0.0f, 1.0f);
        float exponent = exponentValue->getFloat ();
        float min = minValue->getFloat ();
        float max = maxValue->getFloat ();

        // Apply exponent for non-linear distribution
        float adjustedT = std::pow (t, exponent);
        p.size = (min + adjustedT * (max - min)) * m_particle.instanceOverride.size->value->getFloat ();
        p.initial.size = p.size;
    };
}

InitializerFunc CParticle::createAlphaRandomInitializer (const AlphaRandomInitializer& init) {
    DynamicValue* minValue = init.min->value.get ();
    DynamicValue* maxValue = init.max->value.get ();

    return [this, minValue, maxValue](ParticleInstance& p) {
        p.alpha = randomFloat (m_rng, minValue->getFloat (), maxValue->getFloat ()) * m_particle.instanceOverride.alpha->value->getFloat ();
        p.initial.alpha = p.alpha;
    };
}

InitializerFunc CParticle::createLifetimeRandomInitializer (const LifetimeRandomInitializer& init) {
    DynamicValue* minValue = init.min->value.get ();
    DynamicValue* maxValue = init.max->value.get ();

    return [this, minValue, maxValue](ParticleInstance& p) {
        p.lifetime = randomFloat (m_rng, minValue->getFloat (), maxValue->getFloat ()) * m_particle.instanceOverride.lifetime->value->getFloat ();
        p.initial.lifetime = p.lifetime;
    };
}

InitializerFunc CParticle::createVelocityRandomInitializer (const VelocityRandomInitializer& init) {
    DynamicValue* minValue = init.min->value.get ();
    DynamicValue* maxValue = init.max->value.get ();

    return [this, minValue, maxValue](ParticleInstance& p) {
        glm::vec3 vel = randomVec3 (m_rng, minValue->getVec3 (), maxValue->getVec3 ());
        // Flip Y velocity for centered space
        vel.y = -vel.y;
        p.velocity += vel * m_particle.instanceOverride.speed->value->getFloat ();
    };
}

InitializerFunc CParticle::createRotationRandomInitializer (const RotationRandomInitializer& init) {
    DynamicValue* minValue = init.min->value.get ();
    DynamicValue* maxValue = init.max->value.get ();

    return [this, minValue, maxValue](ParticleInstance& p) {
        p.rotation = randomVec3 (m_rng, minValue->getVec3 (), maxValue->getVec3 ());
    };
}

InitializerFunc CParticle::createAngularVelocityRandomInitializer (const AngularVelocityRandomInitializer& init) {
    DynamicValue* minValue = init.min->value.get ();
    DynamicValue* maxValue = init.max->value.get ();

    return [this, minValue, maxValue](ParticleInstance& p) {
        p.angularVelocity = randomVec3 (m_rng, minValue->getVec3 (), maxValue->getVec3 ());
    };
}

InitializerFunc CParticle::createTurbulentVelocityRandomInitializer (const TurbulentVelocityRandomInitializer& init) {
    DynamicValue* speedMin = init.speedMin->value.get ();
    DynamicValue* speedMax = init.speedMax->value.get ();
    DynamicValue* offset = init.offset->value.get ();
    DynamicValue* scale = init.scale->value.get ();

    return [this, speedMin, speedMax, offset, scale](ParticleInstance& p) {
        // Random speed in specified range
        float speed = randomFloat (m_rng, speedMin->getFloat (), speedMax->getFloat ());

        // Initialize random position in noise field (0-10 range for good variety)
        p.noisePos = randomVec3 (m_rng, glm::vec3(0.0f), glm::vec3(10.0f));

        // Apply offset to noise position (shifts sampling region in noise field)
        glm::vec3 noisePosWithOffset = p.noisePos + glm::vec3(offset->getFloat ());

        // Sample curl noise to get turbulent direction
        glm::vec3 direction = curlNoise(noisePosWithOffset);

        // Normalize for consistent velocity magnitude
        if (glm::length(direction) > 0.0001f) {
            direction = glm::normalize(direction);
        } else {
            // Fallback to random direction if noise returns zero
            float theta = randomFloat (m_rng, 0.0f, glm::two_pi<float>());
            float phi = randomFloat (m_rng, 0.0f, glm::pi<float>());
            direction = glm::vec3(
                std::sin(phi) * std::cos(theta),
                std::sin(phi) * std::sin(theta),
                std::cos(phi)
            );
        }

        // Apply scale to control turbulence intensity
        direction *= scale->getFloat ();

        // Apply speed and instance override
        glm::vec3 turbulentVel = direction * speed * m_particle.instanceOverride.speed->value->getFloat ();

        // Flip Y for centered space (like velocity initializer does)
        turbulentVel.y = -turbulentVel.y;

        p.velocity += turbulentVel;
    };
}

// ========== OPERATORS ==========

void CParticle::setupOperators () {
    for (const auto& op : m_particle.operators) {
        if (!op) {
            continue;
        }

        OperatorFunc func;

        if (op->is<MovementOperator> ()) {
            func = createMovementOperator (*op->as<MovementOperator> ());
        } else if (op->is<AngularMovementOperator> ()) {
            func = createAngularMovementOperator (*op->as<AngularMovementOperator> ());
        } else if (op->is<AlphaFadeOperator> ()) {
            func = createAlphaFadeOperator (*op->as<AlphaFadeOperator> ());
        } else if (op->is<SizeChangeOperator> ()) {
            func = createSizeChangeOperator (*op->as<SizeChangeOperator> ());
        } else if (op->is<AlphaChangeOperator> ()) {
            func = createAlphaChangeOperator (*op->as<AlphaChangeOperator> ());
        } else if (op->is<ColorChangeOperator> ()) {
            func = createColorChangeOperator (*op->as<ColorChangeOperator> ());
        } else if (op->is<TurbulenceOperator> ()) {
            func = createTurbulenceOperator (*op->as<TurbulenceOperator> ());
        } else if (op->is<VortexOperator> ()) {
            func = createVortexOperator (*op->as<VortexOperator> ());
        } else if (op->is<ControlPointAttractOperator> ()) {
            func = createControlPointAttractOperator (*op->as<ControlPointAttractOperator> ());
        } else {
            sLog.out ("Unknown operator type");
        }

        if (func) {
            m_operators.push_back (std::move (func));
        }
    }
}

OperatorFunc CParticle::createMovementOperator (const MovementOperator& op) {
    DynamicValue* speedOverride = m_particle.instanceOverride.speed->value.get ();
    DynamicValue* dragValue = op.drag->value.get ();
    DynamicValue* gravityValue = op.gravity->value.get ();

    return [dragValue, gravityValue, speedOverride](
        std::vector<ParticleInstance>& particles,
        uint32_t count,
        const std::vector<ControlPointData>&,
        float,
        float dt
    ) {
        float speed = speedOverride->getFloat ();
        float drag = dragValue->getFloat ();
        glm::vec3 gravity = gravityValue->getVec3 ();
        // Flip gravity Y for centered space
        gravity.y = -gravity.y;

        for (uint32_t i = 0; i < count; i++) {
            auto& p = particles [i];

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

OperatorFunc CParticle::createAngularMovementOperator (const AngularMovementOperator& op) {
    DynamicValue* dragValue = op.drag->value.get ();
    DynamicValue* forceValue = op.force->value.get ();

    return [dragValue, forceValue](
        std::vector<ParticleInstance>& particles,
        uint32_t count,
        const std::vector<ControlPointData>&,
        float,
        float dt
    ) {
        float drag = dragValue->getFloat ();
        glm::vec3 force = forceValue->getVec3 ();

        for (uint32_t i = 0; i < count; i++) {
            auto& p = particles [i];

            glm::vec3 dragForce = -drag * p.angularVelocity;
            glm::vec3 totalAccel = dragForce + force;

            p.angularVelocity += totalAccel * dt;
            p.rotation += p.angularVelocity * dt;

            // Wrap rotation to prevent floating-point precision issues
            const float pi = glm::pi<float>();
            const float two_pi = glm::two_pi<float>();
            for (int j = 0; j < 3; j++) {
                while (p.rotation[j] > pi) p.rotation[j] -= two_pi;
                while (p.rotation[j] < -pi) p.rotation[j] += two_pi;
            }
        }
    };
}

OperatorFunc CParticle::createAlphaFadeOperator (const AlphaFadeOperator& op) {
    DynamicValue* fadeInTimeValue = op.fadeInTime->value.get ();
    DynamicValue* fadeOutTimeValue = op.fadeOutTime->value.get ();

    return [fadeInTimeValue, fadeOutTimeValue](
        std::vector<ParticleInstance>& particles,
        uint32_t count,
        const std::vector<ControlPointData>&,
        float,
        float
    ) {
        float fadeInTime = fadeInTimeValue->getFloat ();
        float fadeOutTime = fadeOutTimeValue->getFloat ();

        for (uint32_t i = 0; i < count; i++) {
            auto& p = particles [i];

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

OperatorFunc CParticle::createSizeChangeOperator (const SizeChangeOperator& op) {
    DynamicValue* startTimeValue = op.startTime->value.get ();
    DynamicValue* endTimeValue = op.endTime->value.get ();
    DynamicValue* startValueValue = op.startValue->value.get ();
    DynamicValue* endValueValue = op.endValue->value.get ();

    return [startTimeValue, endTimeValue, startValueValue, endValueValue](
        std::vector<ParticleInstance>& particles,
        uint32_t count,
        const std::vector<ControlPointData>&,
        float,
        float
    ) {
        float startTime = startTimeValue->getFloat ();
        float endTime = endTimeValue->getFloat ();
        float startValue = startValueValue->getFloat ();
        float endValue = endValueValue->getFloat ();

        for (uint32_t i = 0; i < count; i++) {
            auto& p = particles [i];

            float life = p.getLifetimePos ();
            float multiplier = fadeValue (life, startTime, endTime, startValue, endValue);
            p.size = p.initial.size * multiplier;
        }
    };
}

OperatorFunc CParticle::createAlphaChangeOperator (const AlphaChangeOperator& op) {
    DynamicValue* startTimeValue = op.startTime->value.get ();
    DynamicValue* endTimeValue = op.endTime->value.get ();
    DynamicValue* startValueValue = op.startValue->value.get ();
    DynamicValue* endValueValue = op.endValue->value.get ();

    return [startTimeValue, endTimeValue, startValueValue, endValueValue](
        std::vector<ParticleInstance>& particles,
        uint32_t count,
        const std::vector<ControlPointData>&,
        float,
        float
    ) {
        float startTime = startTimeValue->getFloat ();
        float endTime = endTimeValue->getFloat ();
        float startValue = startValueValue->getFloat ();
        float endValue = endValueValue->getFloat ();

        for (uint32_t i = 0; i < count; i++) {
            auto& p = particles [i];

            float life = p.getLifetimePos ();
            float multiplier = fadeValue (life, startTime, endTime, startValue, endValue);
            p.alpha = p.initial.alpha * multiplier;
        }
    };
}

OperatorFunc CParticle::createColorChangeOperator (const ColorChangeOperator& op) {
    DynamicValue* startTimeValue = op.startTime->value.get ();
    DynamicValue* endTimeValue = op.endTime->value.get ();
    DynamicValue* startValueValue = op.startValue->value.get ();
    DynamicValue* endValueValue = op.endValue->value.get ();

    return [startTimeValue, endTimeValue, startValueValue, endValueValue](
        std::vector<ParticleInstance>& particles,
        uint32_t count,
        const std::vector<ControlPointData>&,
        float,
        float
    ) {
        float startTime = startTimeValue->getFloat ();
        float endTime = endTimeValue->getFloat ();
        glm::vec3 startValue = startValueValue->getVec3 ();
        glm::vec3 endValue = endValueValue->getVec3 ();

        for (uint32_t i = 0; i < count; i++) {
            auto& p = particles [i];

            float life = p.getLifetimePos ();

            glm::vec3 color;
            color.r = fadeValue (life, startTime, endTime, startValue.r, endValue.r);
            color.g = fadeValue (life, startTime, endTime, startValue.g, endValue.g);
            color.b = fadeValue (life, startTime, endTime, startValue.b, endValue.b);

            p.color = p.initial.color * color;
        }
    };
}

OperatorFunc CParticle::createTurbulenceOperator (const TurbulenceOperator& op) {
    DynamicValue* scaleValue = op.scale->value.get ();
    DynamicValue* speedMinValue = op.speedMin->value.get ();
    DynamicValue* speedMaxValue = op.speedMax->value.get ();
    DynamicValue* timeScaleValue = op.timeScale->value.get ();

    // Random phase and speed for this turbulence instance
    float phase = randomFloat (m_rng, 0.0f, 100.0f);
    float speed = randomFloat (m_rng, speedMinValue->getFloat (), speedMaxValue->getFloat ());

    return [this, scaleValue, timeScaleValue, phase, speed](
        std::vector<ParticleInstance>& particles,
        uint32_t count,
        const std::vector<ControlPointData>&,
        float currentTime,
        float dt
    ) {
        float scale = scaleValue->getFloat ();
        float timeScale = timeScaleValue->getFloat ();

        for (uint32_t i = 0; i < count; i++) {
            auto& p = particles [i];

            // Apply time-based phase shift to noise position
            glm::vec3 noisePos = p.position * scale * 2.0f;
            noisePos.x += phase + timeScale * currentTime;

            // Get curl noise acceleration
            glm::vec3 acceleration = curlNoise (noisePos);

            // Normalize and scale by speed
            if (glm::length (acceleration) > 0.0f) {
                acceleration = glm::normalize (acceleration) * speed;
            }

            // Apply acceleration (convert to velocity change over dt)
            p.velocity += acceleration * dt;
        }
    };
}

OperatorFunc CParticle::createVortexOperator (const VortexOperator& op) {
    int controlPoint = op.controlPoint;
    DynamicValue* axisValue = op.axis->value.get ();
    DynamicValue* offsetValue = op.offset->value.get ();
    DynamicValue* distanceInnerValue = op.distanceInner->value.get ();
    DynamicValue* distanceOuterValue = op.distanceOuter->value.get ();
    DynamicValue* speedInnerValue = op.speedInner->value.get ();
    DynamicValue* speedOuterValue = op.speedOuter->value.get ();

    return [this, controlPoint, axisValue, offsetValue, distanceInnerValue, distanceOuterValue, speedInnerValue, speedOuterValue](
        std::vector<ParticleInstance>& particles,
        uint32_t count,
        const std::vector<ControlPointData>& controlPoints,
        float,
        float dt
    ) {
        glm::vec3 axis = axisValue->getVec3 ();
        glm::vec3 offset = offsetValue->getVec3 ();
        float distanceInner = distanceInnerValue->getFloat ();
        float distanceOuter = distanceOuterValue->getFloat ();
        float speedInner = speedInnerValue->getFloat ();
        float speedOuter = speedOuterValue->getFloat ();

        // Get vortex center from control point
        glm::vec3 center = glm::vec3 (0.0f);
        if (controlPoint >= 0 && controlPoint < static_cast<int>(controlPoints.size ())) {
            center = controlPoints [controlPoint].position + offset;
        } else {
            center = offset;
        }

        // Normalize axis
        if (glm::length (axis) > 0.0f) {
            axis = glm::normalize (axis);
        } else {
            axis = glm::vec3 (0.0f, 0.0f, 1.0f); // Default to Z-axis
        }

        for (uint32_t i = 0; i < count; i++) {
            auto& p = particles [i];

            // Vector from center to particle
            glm::vec3 toParticle = p.position - center;
            float distance = glm::length (toParticle);

            // Skip if distance is zero to avoid division by zero
            if (distance < 0.001f) {
                continue;
            }

            // Compute spiral direction using cross product
            // Negative axis to match WE behavior (particles swirl around axis)
            glm::vec3 spiralDirection = glm::cross (-axis, toParticle);

            if (glm::length (spiralDirection) > 0.0f) {
                spiralDirection = glm::normalize (spiralDirection);
            } else {
                continue; // Particle is on the axis
            }

            // Determine speed based on distance
            float speed = 0.0f;
            if (distance < distanceInner) {
                // Inside inner radius - use inner speed
                speed = speedInner;
            } else if (distance < distanceOuter) {
                // Between inner and outer - interpolate
                float t = (distance - distanceInner) / (distanceOuter - distanceInner);
                speed = glm::mix (speedInner, speedOuter, t);
            }
            // Outside outer radius - no effect (speed stays 0)

            // Apply spiral acceleration
            p.velocity += spiralDirection * speed * dt;
        }
    };
}

OperatorFunc CParticle::createControlPointAttractOperator (const ControlPointAttractOperator& op) {
    int controlPoint = op.controlPoint;
    DynamicValue* originValue = op.origin->value.get ();
    DynamicValue* scaleValue = op.scale->value.get ();
    DynamicValue* thresholdValue = op.threshold->value.get ();

    return [this, controlPoint, originValue, scaleValue, thresholdValue]
           (std::vector<ParticleInstance>& particles, uint32_t count,
            const std::vector<ControlPointData>& controlPoints, float currentTime, float dt) {

        // Get dynamic values
        glm::vec3 origin = originValue->getVec3 ();
        float scale = scaleValue->getFloat ();
        float threshold = thresholdValue->getFloat ();

        // Get control point position
        if (controlPoint < 0 || controlPoint >= static_cast<int>(controlPoints.size())) {
            return;
        }

        glm::vec3 center = controlPoints[controlPoint].position + origin;

        // Apply attraction force to all particles within threshold
        for (uint32_t i = 0; i < count; i++) {
            auto& p = particles [i];
            if (!p.alive) continue;

            // Calculate distance and direction to control point
            glm::vec3 toCenter = center - p.position;
            float distance = glm::length (toCenter);

            // Only apply force if within threshold
            if (distance > 0.001f && distance < threshold) {
                // Normalize direction
                glm::vec3 direction = toCenter / distance;

                // Calculate force (inversely proportional to distance)
                // Scale can be negative for repulsion
                float force = scale / distance;

                // Apply force as velocity change
                p.velocity += direction * force * dt;
            }
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
        layout (location = 2) in vec3 aRotation;
        layout (location = 3) in float aSize;
        layout (location = 4) in vec4 aColor;
        layout (location = 5) in float aFrame;
        layout (location = 6) in vec3 aVelocity;

        out vec2 vTexCoord;
        out vec4 vColor;
        out float vFrame;

        uniform mat4 g_ModelViewProjectionMatrix;
        uniform int u_UseTrailRenderer;
        uniform float u_TrailLength;
        uniform float u_TrailMaxLength;
        uniform float u_TextureRatio;

        void main() {
            vec2 offset = aTexCoord - 0.5;
            vec3 billboardPos;

            if (u_UseTrailRenderer == 1) {
                // Trail rendering: stretch particle along velocity
                vec3 right, up;
                float speed = length(aVelocity);

                if (speed > 0.001) {
                    // Compute trail direction from velocity
                    up = normalize(aVelocity);

                    // Calculate trail length based on speed
                    float trailLen = max(0.0, min(speed * u_TrailLength, u_TrailMaxLength));

                    // Compute perpendicular right vector (billboard facing camera)
                    vec3 viewDir = vec3(0.0, 0.0, -1.0); // Simplified view direction
                    right = normalize(cross(viewDir, up));

                    // If cross product is zero, use alternate perpendicular
                    if (length(right) < 0.001) {
                        right = vec3(1.0, 0.0, 0.0);
                    }

                    // Scale vectors
                    up = up * trailLen;
                } else {
                    // Fallback to rotation if velocity is too small
                    right = vec3(1.0, 0.0, 0.0);
                    up = vec3(0.0, 1.0, 0.0);
                }

                // Apply billboard transformation with size scaling
                // textureRatio maintains texture aspect ratio (height/width)
                billboardPos = aPos +
                    aSize * right * offset.x -
                    aSize * up * offset.y * u_TextureRatio;
            } else {
                // Standard rotation-based rendering
                float cx = cos(aRotation.x);
                float sx = sin(aRotation.x);
                float cy = cos(aRotation.y);
                float sy = sin(aRotation.y);
                float cz = cos(aRotation.z);
                float sz = sin(aRotation.z);

                vec3 offset3d = vec3(offset, 0.0);

                // Rotate around X axis
                vec3 rotX = vec3(
                    offset3d.x,
                    offset3d.y * cx - offset3d.z * sx,
                    offset3d.y * sx + offset3d.z * cx
                );

                // Rotate around Y axis
                vec3 rotY = vec3(
                    rotX.x * cy + rotX.z * sy,
                    rotX.y,
                    -rotX.x * sy + rotX.z * cy
                );

                // Rotate around Z axis
                vec3 rotated = vec3(
                    rotY.x * cz - rotY.y * sz,
                    rotY.x * sz + rotY.y * cz,
                    rotY.z
                );

                billboardPos = aPos + rotated * aSize;
            }

            gl_Position = g_ModelViewProjectionMatrix * vec4(billboardPos, 1.0);
            vTexCoord = aTexCoord;
            vColor = aColor;
            vFrame = aFrame;
        }
    )";

    const char* fragmentShaderSource = R"(
        #version 330 core
        in vec2 vTexCoord;
        in vec4 vColor;
        in float vFrame;

        out vec4 FragColor;

        uniform sampler2D g_Texture0;
        uniform int u_HasTexture;
        uniform int u_TextureFormat; // 8 = RG88, 9 = R8
        uniform vec2 u_SpritesheetSize; // x=cols, y=rows
        uniform float u_Overbright; // Brightness multiplier for additive particles

        void main() {
            vec4 texColor;
            if (u_HasTexture == 1) {
                // Calculate UV coordinates for spritesheet frame
                vec2 uv = vTexCoord;
                if (u_SpritesheetSize.x > 0.0) {
                    // Spritesheet: adjust UVs to sample only the current frame
                    float cols = u_SpritesheetSize.x;
                    float rows = u_SpritesheetSize.y;
                    float frameIndex = floor(vFrame);

                    float frameX = mod(frameIndex, cols);
                    float frameY = floor(frameIndex / cols);

                    float frameWidth = 1.0 / cols;
                    float frameHeight = 1.0 / rows;

                    // Calculate UV coordinates for the current frame
                    uv = vec2(
                        frameX * frameWidth + vTexCoord.x * frameWidth,
                        frameY * frameHeight + vTexCoord.y * frameHeight
                    );
                }

                // Sample texture
                vec4 sample = texture(g_Texture0, uv);

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

            // Apply vertex color and texture
            vec4 finalColor = vColor * texColor;

            // Apply overbright multiplier to RGB channels (controls brightness for additive particles)
            finalColor.rgb *= u_Overbright;

            FragColor = finalColor;
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

    // Cache uniform locations
    m_uniformTexture = glGetUniformLocation (m_shaderProgram, "g_Texture0");
    m_uniformHasTexture = glGetUniformLocation (m_shaderProgram, "u_HasTexture");
    m_uniformTextureFormat = glGetUniformLocation (m_shaderProgram, "u_TextureFormat");
    m_uniformSpritesheetSize = glGetUniformLocation (m_shaderProgram, "u_SpritesheetSize");
    m_uniformOverbright = glGetUniformLocation (m_shaderProgram, "u_Overbright");
    m_uniformUseTrailRenderer = glGetUniformLocation (m_shaderProgram, "u_UseTrailRenderer");
    m_uniformTrailLength = glGetUniformLocation (m_shaderProgram, "u_TrailLength");
    m_uniformTrailMaxLength = glGetUniformLocation (m_shaderProgram, "u_TrailMaxLength");
    m_uniformTextureRatio = glGetUniformLocation (m_shaderProgram, "u_TextureRatio");

    glGenVertexArrays (1, &m_vao);
    glGenBuffers (1, &m_vbo);

    glBindVertexArray (m_vao);
    glBindBuffer (GL_ARRAY_BUFFER, m_vbo);

    // Vertex format: pos(3) + texcoord(2) + rotation(3) + size(1) + color(4) + frame(1) + velocity(3) = 17 floats
    const int stride = sizeof (float) * 17;

    // Position (location 0)
    glEnableVertexAttribArray (0);
    glVertexAttribPointer (0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);

    // Texture coordinates (location 1)
    glEnableVertexAttribArray (1);
    glVertexAttribPointer (1, 2, GL_FLOAT, GL_FALSE, stride, (void*)(sizeof (float) * 3));

    // Rotation (location 2)
    glEnableVertexAttribArray (2);
    glVertexAttribPointer (2, 3, GL_FLOAT, GL_FALSE, stride, (void*)(sizeof (float) * 5));

    // Size (location 3)
    glEnableVertexAttribArray (3);
    glVertexAttribPointer (3, 1, GL_FLOAT, GL_FALSE, stride, (void*)(sizeof (float) * 8));

    // Color (location 4) - includes alpha as 4th component
    glEnableVertexAttribArray (4);
    glVertexAttribPointer (4, 4, GL_FLOAT, GL_FALSE, stride, (void*)(sizeof (float) * 9));

    // Frame (location 5)
    glEnableVertexAttribArray (5);
    glVertexAttribPointer (5, 1, GL_FLOAT, GL_FALSE, stride, (void*)(sizeof (float) * 13));

    // Velocity (location 6)
    glEnableVertexAttribArray (6);
    glVertexAttribPointer (6, 3, GL_FLOAT, GL_FALSE, stride, (void*)(sizeof (float) * 14));

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

    // Debug: Log particle count and position periodically
    static int frameCounter = 0;
    if (++frameCounter % 60 == 0 && m_particle.name == "cherry blossoms on cursor") {
        sLog.out ("Cherry blossom particles: ", aliveCount, " alive out of ", m_particleCount, " total");
        if (aliveCount > 0) {
            // Log first alive particle's position for debugging
            for (uint32_t i = 0; i < m_particleCount; i++) {
                if (m_particles[i].alive) {
                    sLog.out ("  First particle: pos=(", m_particles[i].position.x, ",", m_particles[i].position.y, ",", m_particles[i].position.z,
                              ") size=", m_particles[i].size, " alpha=", m_particles[i].alpha);
                    break;
                }
            }
        }
    }

    if (aliveCount == 0)
        return;

    // Prepare vertex data - 6 vertices per particle (2 triangles forming a quad)
    // Vertex format: pos(3) + texcoord(2) + rotation(3) + size(1) + color(4) + frame(1) + velocity(3) = 17 floats per vertex
    std::vector<float> vertices;
    vertices.reserve (aliveCount * 6 * 17);

    for (uint32_t i = 0; i < m_particleCount; i++) {
        const auto& p = m_particles [i];
        if (!p.alive)
            continue;

        // Skip particles with invalid values (NaN, infinity, or extreme size)
        if (!std::isfinite(p.position.x) || !std::isfinite(p.position.y) || !std::isfinite(p.position.z) ||
            !std::isfinite(p.rotation.x) || !std::isfinite(p.rotation.y) || !std::isfinite(p.rotation.z) ||
            !std::isfinite(p.size) || p.size <= 0.0f || p.size > 10000.0f) {
            continue;
        }

        // Particle size is already scaled by instance override, don't apply object scale
        float size = p.size / 2.0f;

        // Create 6 vertices forming 2 triangles (GL_QUADS not available in core profile)

        auto addVertex = [&](float u, float v) {
            vertices.push_back (p.position.x);
            vertices.push_back (p.position.y);
            vertices.push_back (p.position.z);
            vertices.push_back (u);
            vertices.push_back (v);
            vertices.push_back (p.rotation.x);
            vertices.push_back (p.rotation.y);
            vertices.push_back (p.rotation.z);
            vertices.push_back (size);
            vertices.push_back (p.color.r);
            vertices.push_back (p.color.g);
            vertices.push_back (p.color.b);
            vertices.push_back (p.alpha);
            vertices.push_back (p.frame);
            vertices.push_back (p.velocity.x);
            vertices.push_back (p.velocity.y);
            vertices.push_back (p.velocity.z);
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

    if (m_shaderProgram == 0) {
        return;
    }

    // Clear any existing GL errors before we start
    while (glGetError () != GL_NO_ERROR);

    // Save current GL state before any modifications
    GLint prevProgram = 0;
    GLint prevVAO = 0;
    GLint prevTexture = 0;
    GLboolean prevBlendEnabled = glIsEnabled (GL_BLEND);
    GLboolean prevDepthMask = GL_TRUE;
    GLint prevBlendSrcRGB = GL_ONE;
    GLint prevBlendDstRGB = GL_ZERO;
    GLint prevBlendSrcAlpha = GL_ONE;
    GLint prevBlendDstAlpha = GL_ZERO;
    GLint prevActiveTexture = GL_TEXTURE0;
    GLint prevArrayBuffer = 0;
    glGetIntegerv (GL_CURRENT_PROGRAM, &prevProgram);
    glGetIntegerv (GL_VERTEX_ARRAY_BINDING, &prevVAO);
    glGetIntegerv (GL_TEXTURE_BINDING_2D, &prevTexture);
    glGetBooleanv (GL_DEPTH_WRITEMASK, &prevDepthMask);
    glGetIntegerv (GL_BLEND_SRC_RGB, &prevBlendSrcRGB);
    glGetIntegerv (GL_BLEND_DST_RGB, &prevBlendDstRGB);
    glGetIntegerv (GL_BLEND_SRC_ALPHA, &prevBlendSrcAlpha);
    glGetIntegerv (GL_BLEND_DST_ALPHA, &prevBlendDstAlpha);
    glGetIntegerv (GL_ACTIVE_TEXTURE, &prevActiveTexture);
    glGetIntegerv (GL_ARRAY_BUFFER_BINDING, &prevArrayBuffer);

    glBindBuffer (GL_ARRAY_BUFFER, m_vbo);
    glBufferData (GL_ARRAY_BUFFER, vertices.size () * sizeof (float), vertices.data (), GL_DYNAMIC_DRAW);

    // Use particle shader
    glUseProgram (m_shaderProgram);

    // Bind particle texture
    if (m_texture) {
        glActiveTexture (GL_TEXTURE0);
        glBindTexture (GL_TEXTURE_2D, m_texture->getTextureID (0));

        // Set texture wrapping mode
        glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        if (m_uniformTexture != -1) {
            glUniform1i (m_uniformTexture, 0);
        }
        if (m_uniformHasTexture != -1) {
            glUniform1i (m_uniformHasTexture, 1);
        }
        if (m_uniformTextureFormat != -1) {
            glUniform1i (m_uniformTextureFormat, static_cast<int> (m_textureFormat));
        }
        // Set spritesheet size (cols, rows)
        if (m_uniformSpritesheetSize != -1) {
            glUniform2f (m_uniformSpritesheetSize, static_cast<float>(m_spritesheetCols), static_cast<float>(m_spritesheetRows));
        }
        // Set texture aspect ratio (height / width)
        if (m_uniformTextureRatio != -1) {
            float width = static_cast<float>(m_texture->getRealWidth());
            float height = static_cast<float>(m_texture->getRealHeight());
            float textureRatio = (width > 0.0f) ? (height / width) : 1.0f;
            glUniform1f (m_uniformTextureRatio, textureRatio);
        }
    } else {
        if (m_uniformHasTexture != -1) {
            glUniform1i (m_uniformHasTexture, 0);
        }
        if (m_uniformSpritesheetSize != -1) {
            glUniform2f (m_uniformSpritesheetSize, 0.0f, 0.0f);
        }
        // Default texture ratio for no texture
        if (m_uniformTextureRatio != -1) {
            glUniform1f (m_uniformTextureRatio, 1.0f);
        }
    }

    // Set overbright multiplier (brightness control for additive particles)
    if (m_uniformOverbright != -1) {
        glUniform1f (m_uniformOverbright, m_overbright);
    }

    // Set trail renderer uniforms
    if (m_uniformUseTrailRenderer != -1) {
        glUniform1i (m_uniformUseTrailRenderer, m_useTrailRenderer ? 1 : 0);
    }
    if (m_uniformTrailLength != -1) {
        glUniform1f (m_uniformTrailLength, m_trailLength);
    }
    if (m_uniformTrailMaxLength != -1) {
        glUniform1f (m_uniformTrailMaxLength, m_trailMaxLength);
    }

    // Build model matrix from particle object transform
    glm::vec3 scale = m_particle.scale->value->getVec3 ();
    glm::vec3 angles = m_particle.angles->value->getVec3 ();

    glm::mat4 model = glm::mat4 (1.0f);
    model = glm::translate (model, m_transformedOrigin);
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
    glBlendFuncSeparate (prevBlendSrcRGB, prevBlendDstRGB, prevBlendSrcAlpha, prevBlendDstAlpha);
    if (prevBlendEnabled) {
        glEnable (GL_BLEND);
    } else {
        glDisable (GL_BLEND);
    }
    glUseProgram (prevProgram);
    glActiveTexture (prevActiveTexture);
    glBindTexture (GL_TEXTURE_2D, prevTexture);
    glBindBuffer (GL_ARRAY_BUFFER, prevArrayBuffer);
    glBindVertexArray (prevVAO);
}
