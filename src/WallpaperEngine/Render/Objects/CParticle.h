#pragma once

#include "CRenderable.h"
#include "WallpaperEngine/Data/Model/Object.h"
#include "WallpaperEngine/Render/Wallpapers/CScene.h"

#include <functional>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <random>
#include <vector>

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
    glm::vec3 position { 0.0f };
    glm::vec3 velocity { 0.0f };
    glm::vec3 acceleration { 0.0f };

    // Rotation
    glm::vec3 rotation { 0.0f };
    glm::vec3 angularVelocity { 0.0f };
    glm::vec3 angularAcceleration { 0.0f };

    // Visual properties
    glm::vec3 color { 1.0f };
    float alpha { 1.0f };
    float size { 20.0f };
    float frame { 0.0f }; // Current animation frame

    // Lifetime
    float lifetime { 1.0f }; // Total lifetime in seconds
    float age { 0.0f }; // Current age in seconds

    // Oscillator state (per-particle random values)
    // base is updated by alphafade/sizechange operators so oscillation combines properly
    struct {
	float frequency { 0.0f };
	float scale { 1.0f };
	float phase { 0.0f };
	float base { 1.0f };
	bool initialized { false };
    } oscillateAlpha, oscillateSize;

    struct {
	glm::vec3 frequency { 0.0f };
	glm::vec3 scale { 1.0f };
	glm::vec3 phase { 0.0f };
	bool initialized { false };
    } oscillatePosition;

    // Initial values for resets/multipliers
    struct {
	glm::vec3 color { 1.0f };
	float alpha { 1.0f };
	float size { 20.0f };
	float lifetime { 1.0f };
    } initial;

    bool alive { false };

    // Get normalized lifetime position (0.0 to 1.0)
    float getLifetimePos () const { return lifetime > 0.0f ? (age / lifetime) : 1.0f; }

    bool isAlive () const { return alive && age < lifetime; }
};

/**
 * Control point runtime data
 */
struct ControlPointData {
    glm::vec3 position { 0.0f };
    glm::vec3 offset { 0.0f };
    bool linkMouse { false };
    bool worldSpace { false };
};

/**
 * Particle emitter function
 */
using EmitterFunc = std::function<void (std::vector<ParticleInstance>&, uint32_t&, float)>;

/**
 * Particle initializer function
 */
using InitializerFunc = std::function<void (ParticleInstance&)>;

/**
 * Particle operator function
 */
using OperatorFunc = std::function<
    void (std::vector<ParticleInstance>&, uint32_t, const std::vector<ControlPointData>&, float, float)>;

class CParticle final : public CRenderable {
    friend CObject;

public:
    CParticle (Wallpapers::CScene& scene, const Particle& particle);
    ~CParticle ();

    void setup () override;
    void render () override;
    void update (float dt);

    [[nodiscard]] const Particle& getParticle () const;

    [[nodiscard]] const float& getBrightness () const override;
    [[nodiscard]] const float& getUserAlpha () const override;
    [[nodiscard]] const float& getAlpha () const override;
    [[nodiscard]] const glm::vec3& getColor () const override;
    [[nodiscard]] const glm::vec4& getColor4 () const override;
    [[nodiscard]] const glm::vec3& getCompositeColor () const override;

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
    InitializerFunc
    createMapSequenceAroundControlPointInitializer (const MapSequenceAroundControlPointInitializer& init);

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
    OperatorFunc createOscillateAlphaOperator (const OscillateAlphaOperator& op);
    OperatorFunc createOscillateSizeOperator (const OscillateSizeOperator& op);
    OperatorFunc createOscillatePositionOperator (const OscillatePositionOperator& op);

    // Rendering
    void renderSprites ();
    void setupBuffers ();

private:
    const Particle& m_particle;

    std::vector<ParticleInstance> m_particles;
    uint32_t m_particleCount { 0 };
    uint32_t m_maxParticles { DEFAULT_MAX_PARTICLES };

    std::vector<EmitterFunc> m_emitters;
    std::vector<InitializerFunc> m_initializers;
    std::vector<OperatorFunc> m_operators;

    std::vector<ControlPointData> m_controlPoints;

    std::vector<float> m_vertices;
    std::vector<uint32_t> m_indices;

    double m_time { 0.0 };

    // OpenGL buffers
    GLuint m_vao { 0 };
    GLuint m_vbo { 0 };
    GLuint m_ebo { 0 }; // Element Buffer Object for indexed rendering
    GLuint m_shaderProgram { 0 };

    // Cached uniform locations
    GLint m_uniformTexture { -1 };
    GLint m_uniformHasTexture { -1 };
    GLint m_uniformTextureFormat { -1 };
    GLint m_uniformSpritesheetSize { -1 };
    GLint m_uniformOverbright { -1 };
    GLint m_uniformUseTrailRenderer { -1 };
    GLint m_uniformPerspective { -1 };
    GLint m_uniformTrailLength { -1 };
    GLint m_uniformTrailMaxLength { -1 };
    GLint m_uniformTrailMinLength { -1 };
    GLint m_uniformTextureRatio { -1 };
    GLint m_uniformCameraPos { -1 };
    GLint m_uniformVelocityRotation { -1 };

    // Particle material properties
    Data::Model::BlendingMode m_blendingMode { Data::Model::BlendingMode_Translucent };
    Data::Assets::TextureFormat m_textureFormat { Data::Assets::TextureFormat_ARGB8888 };

    // Spritesheet animation data
    int m_spritesheetCols { 0 };
    int m_spritesheetRows { 0 };
    int m_spritesheetFrames { 0 };
    float m_spritesheetDuration { 1.0f };

    // Material shader constants
    float m_overbright { 1.0f }; // Brightness multiplier for additive particles

    // Renderer configuration
    bool m_useTrailRenderer { false };
    float m_trailLength { 0.05f };
    float m_trailMaxLength { 10.0f };
    float m_trailMinLength { 0.0f };
    int m_trailSubdivision { 3 }; // Number of segments per trail

    // Transformed origin (screen space to centered space conversion)
    glm::vec3 m_transformedOrigin { 0.0f };

    // Last known resolution for detecting changes
    float m_lastScreenWidth { 0.0f };
    float m_lastScreenHeight { 0.0f };

    // Random number generator
    std::mt19937 m_rng;

    bool m_initialized { false };

    // Helper methods
    GLuint compileShader (GLenum type, const char* source);
    GLuint createShaderProgram ();
};
} // namespace WallpaperEngine::Render::Objects
