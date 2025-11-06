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

constexpr uint32_t DEFAULT_MAX_PARTICLES = 1000;

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
    float frame {0.0f};         // Current animation frame

    // Lifetime
    float lifetime {1.0f};      // Total lifetime in seconds
    float age {0.0f};           // Current age in seconds

    // Turbulent velocity state
    glm::vec3 noisePos {0.0f};  // Position in noise field for turbulent velocity

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
    ~CParticle ();

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
    InitializerFunc createColorRandomInitializer (const ColorRandomInitializer& init);
    InitializerFunc createSizeRandomInitializer (const SizeRandomInitializer& init);
    InitializerFunc createAlphaRandomInitializer (const AlphaRandomInitializer& init);
    InitializerFunc createLifetimeRandomInitializer (const LifetimeRandomInitializer& init);
    InitializerFunc createVelocityRandomInitializer (const VelocityRandomInitializer& init);
    InitializerFunc createRotationRandomInitializer (const RotationRandomInitializer& init);
    InitializerFunc createAngularVelocityRandomInitializer (const AngularVelocityRandomInitializer& init);
    InitializerFunc createTurbulentVelocityRandomInitializer (const TurbulentVelocityRandomInitializer& init);

    // Operator creators
    OperatorFunc createMovementOperator (const MovementOperator& op);
    OperatorFunc createAngularMovementOperator (const AngularMovementOperator& op);
    OperatorFunc createAlphaFadeOperator (const AlphaFadeOperator& op);
    OperatorFunc createSizeChangeOperator (const SizeChangeOperator& op);
    OperatorFunc createAlphaChangeOperator (const AlphaChangeOperator& op);
    OperatorFunc createColorChangeOperator (const ColorChangeOperator& op);
    OperatorFunc createTurbulenceOperator (const TurbulenceOperator& op);
    OperatorFunc createVortexOperator (const VortexOperator& op);
    OperatorFunc createControlPointAttractOperator (const ControlPointAttractOperator& op);

    // Rendering
    void renderSprites ();
    void setupBuffers ();

  private:
    const Particle& m_particle;

    std::vector<ParticleInstance> m_particles;
    uint32_t m_particleCount {0};
    uint32_t m_maxParticles {DEFAULT_MAX_PARTICLES};

    std::vector<EmitterFunc> m_emitters;
    std::vector<InitializerFunc> m_initializers;
    std::vector<OperatorFunc> m_operators;

    std::vector<ControlPointData> m_controlPoints;

    double m_time {0.0};

    // OpenGL buffers
    GLuint m_vao {0};
    GLuint m_vbo {0};
    GLuint m_shaderProgram {0};

    // Cached uniform locations
    GLint m_uniformTexture {-1};
    GLint m_uniformHasTexture {-1};
    GLint m_uniformTextureFormat {-1};
    GLint m_uniformSpritesheetSize {-1};
    GLint m_uniformOverbright {-1};
    GLint m_uniformUseTrailRenderer {-1};
    GLint m_uniformTrailLength {-1};
    GLint m_uniformTrailMaxLength {-1};

    // Particle material texture
    std::shared_ptr<const TextureProvider> m_texture {nullptr};
    Data::Model::BlendingMode m_blendingMode {Data::Model::BlendingMode_Translucent};
    Data::Assets::TextureFormat m_textureFormat {Data::Assets::TextureFormat_ARGB8888};

    // Spritesheet animation data
    int m_spritesheetCols {0};
    int m_spritesheetRows {0};
    int m_spritesheetFrames {0};
    float m_spritesheetDuration {1.0f};

    // Material shader constants
    float m_overbright {1.0f};  // Brightness multiplier for additive particles

    // Renderer configuration
    bool m_useTrailRenderer {false};
    float m_trailLength {0.05f};
    float m_trailMaxLength {10.0f};

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
