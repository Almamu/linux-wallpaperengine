#pragma once

#include "WallpaperEngine/Render/CObject.h"
#include "WallpaperEngine/Render/Wallpapers/CScene.h"
#include "WallpaperEngine/Data/Model/Object.h"

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <vector>
#include <random>
#include <functional>

using namespace WallpaperEngine;
using namespace WallpaperEngine::Render;
using namespace WallpaperEngine::Data::Model;

namespace WallpaperEngine::Render::Objects {

constexpr uint32_t MAX_PARTICLES = 10000;

/**
 * Runtime particle instance state
 */
struct ParticleInstance {
    // Position and movement
    glm::vec3 position {0.0f};
    glm::vec3 velocity {0.0f};
    glm::vec3 acceleration {0.0f};

    // Rotation
    glm::vec3 rotation {0.0f};
    glm::vec3 angularVelocity {0.0f};
    glm::vec3 angularAcceleration {0.0f};

    // Visual properties
    glm::vec3 color {1.0f};
    float alpha {1.0f};
    float size {20.0f};

    // Lifetime
    float lifetime {1.0f};      // Total lifetime in seconds
    float age {0.0f};           // Current age in seconds

    // Initial values for resets/multipliers
    struct {
        glm::vec3 color {1.0f};
        float alpha {1.0f};
        float size {20.0f};
        float lifetime {1.0f};
    } initial;

    bool alive {false};

    // Get normalized lifetime position (0.0 to 1.0)
    float getLifetimePos () const {
        return lifetime > 0.0f ? (age / lifetime) : 1.0f;
    }

    bool isAlive () const {
        return alive && age < lifetime;
    }
};

/**
 * Control point runtime data
 */
struct ControlPointData {
    glm::vec3 position {0.0f};
    glm::vec3 offset {0.0f};
    bool linkMouse {false};
    bool worldSpace {false};
};

/**
 * Particle emitter function
 */
using EmitterFunc = std::function<void(std::vector<ParticleInstance>&, uint32_t&, float)>;

/**
 * Particle initializer function
 */
using InitializerFunc = std::function<void(ParticleInstance&)>;

/**
 * Particle operator function
 */
using OperatorFunc = std::function<void(std::vector<ParticleInstance>&, uint32_t, const std::vector<ControlPointData>&, float, float)>;

class CParticle final : public CObject {
    friend CObject;

  public:
    CParticle (Wallpapers::CScene& scene, const Particle& particle);

    void setup ();
    void render () override;
    void update (float dt);

    [[nodiscard]] const Particle& getParticle () const;

  protected:
    void setupEmitters ();
    void setupInitializers ();
    void setupOperators ();

    // Emitter creators
    EmitterFunc createBoxEmitter (const ParticleEmitter& emitter);
    EmitterFunc createSphereEmitter (const ParticleEmitter& emitter);

    // Initializer creators
    InitializerFunc createColorRandomInitializer (const JSON& json);
    InitializerFunc createSizeRandomInitializer (const JSON& json);
    InitializerFunc createAlphaRandomInitializer (const JSON& json);
    InitializerFunc createLifetimeRandomInitializer (const JSON& json);
    InitializerFunc createVelocityRandomInitializer (const JSON& json);
    InitializerFunc createRotationRandomInitializer (const JSON& json);
    InitializerFunc createAngularVelocityRandomInitializer (const JSON& json);

    // Operator creators
    OperatorFunc createMovementOperator (const JSON& json);
    OperatorFunc createAngularMovementOperator (const JSON& json);
    OperatorFunc createAlphaFadeOperator (const JSON& json);
    OperatorFunc createSizeChangeOperator (const JSON& json);
    OperatorFunc createAlphaChangeOperator (const JSON& json);
    OperatorFunc createColorChangeOperator (const JSON& json);

    // Rendering
    void renderSprites ();
    void setupBuffers ();

  private:
    const Particle& m_particle;

    std::vector<ParticleInstance> m_particles;
    uint32_t m_particleCount {0};

    std::vector<EmitterFunc> m_emitters;
    std::vector<InitializerFunc> m_initializers;
    std::vector<OperatorFunc> m_operators;

    std::vector<ControlPointData> m_controlPoints;

    // Emission timing
    float m_emissionTimer {0.0f};
    double m_time {0.0f};

    // OpenGL buffers
    GLuint m_vao {0};
    GLuint m_vbo {0};
    GLuint m_shaderProgram {0};

    // Particle material texture
    std::shared_ptr<const TextureProvider> m_texture {nullptr};
    Data::Model::BlendingMode m_blendingMode {Data::Model::BlendingMode_Translucent};
    Data::Assets::TextureFormat m_textureFormat {Data::Assets::TextureFormat_ARGB8888};

    // Transformed origin (screen space to centered space conversion)
    glm::vec3 m_transformedOrigin {0.0f};

    // Random number generator
    std::mt19937 m_rng;

    bool m_initialized {false};

    // Helper methods
    GLuint compileShader (GLenum type, const char* source);
    GLuint createShaderProgram ();
};
} // namespace WallpaperEngine::Render::Objects
