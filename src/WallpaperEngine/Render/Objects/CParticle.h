#pragma once

#include "CRenderable.h"
#include "WallpaperEngine/Data/Model/Object.h"
#include "WallpaperEngine/Data/Utils/ScopeGuard.h"
#include "WallpaperEngine/Render/Objects/Effects/CPass.h"
#include "WallpaperEngine/Render/Wallpapers/CScene.h"
#include "WallpaperEngine/Scripting/ScriptableObject.h"

#include <array>
#include <functional>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <memory>
#include <random>
#include <vector>

using namespace WallpaperEngine;
using namespace WallpaperEngine::Render;
using namespace WallpaperEngine::Data::Model;

namespace WallpaperEngine::Render::Objects {

class CImage;

struct ParticleLayerImageSample {
    int16_t x;
    int16_t y;
    std::array<uint8_t, 3> color;
};

[[nodiscard]] std::vector<ParticleLayerImageSample> buildParticleLayerImageSamples (
    const std::vector<uint8_t>& rgba, glm::uvec2 extent, glm::uvec2 sourceExtent
);

constexpr uint32_t DEFAULT_MAX_PARTICLES = 1000;

[[nodiscard]] float advanceParticleRenderTime (double currentTime, double& previousTime);
[[nodiscard]] float calculateParticleSimulationDelta (float elapsed, float rate);
[[nodiscard]] float calculateParticleEmissionRate (float emitterRate, float count);
[[nodiscard]] float calculateParticleLifetime (
    float minimum, float maximum, float exponent, float fraction, float multiplier
);
[[nodiscard]] glm::vec3 calculateParticleVelocityCap (
    const glm::vec3& velocity, float maxSpeed, float lifetimePosition, glm::vec4 blendTimes
);
[[nodiscard]] glm::vec3 calculateParticleMovementReduction (
    const glm::vec3& velocity, float distance, glm::vec2 distanceRange, glm::vec2 reductions,
    float deltaTime, float lifetimePosition, glm::vec4 blendTimes
);
[[nodiscard]] float calculateParticleAudioResponse (
    const float* left, const float* right, int mode, const glm::vec2& bounds,
    float exponent, int frequencyStart, int frequencyEnd
);
[[nodiscard]] glm::vec3 convertParticleRotationForRender (const glm::vec3& rotation, bool preserveZ = false);
[[nodiscard]] float calculateRopeTrailVisualValue (float currentValue, float trailPosition, bool fadeAlongTrail);
[[nodiscard]] glm::vec3 calculateControlPointAttraction (
    const glm::vec3& toCenter, float strength, float radius, float deltaTime,
    bool limitToDistance = true, float lifetimeWeight = 1.0f
);
[[nodiscard]] glm::vec3 calculateParticleLegacyVortex (
    const glm::vec3& displacement, const glm::vec3& axis, int flags,
    glm::vec2 distances, glm::vec2 speeds, float forceDt
);
[[nodiscard]] glm::vec3 calculateParticleVortexV2 (
    const glm::vec3& displacement, const glm::vec3& velocity, const glm::vec3& axis, int flags,
    glm::vec2 distances, glm::vec2 speeds, float centerForce, glm::vec4 ring,
    float rawDt, float forceDt, float lifetimeWeight
);
[[nodiscard]] glm::vec3 resolveParticleControlPoint (
    const glm::vec3& offset, const glm::mat4& worldToLocal, bool worldSpace
);
[[nodiscard]] glm::vec3 calculateFollowChildDisplacement (
    const std::optional<glm::vec3>& previous, const glm::vec3& next, bool worldSpace
);
[[nodiscard]] glm::mat3 calculateFixedParticleOrientation (
    const glm::vec3& axis, const glm::mat3& model, bool worldSpace
);
[[nodiscard]] glm::mat3 calculateUprightParticleOrientation (
    const glm::vec3& axis, const glm::mat3& model, const glm::vec3& cameraForward, bool worldSpace
);
[[nodiscard]] glm::vec3 calculateUprightRopeForward (
    const glm::vec3& axis, const glm::mat3& model, const glm::vec3& cameraForward, bool worldSpace
);
[[nodiscard]] glm::mat3 calculateBillboardParticleOrientation (
    const glm::mat4& modelInverse, const glm::mat4& cameraWorld, float roll
);

/**
 * Runtime particle instance state
 */
struct ParticleInstance {
    uint64_t serial = 0; // Stable across pool compaction; never reused by this emitter.
    struct TrailPoint {
	glm::vec3 position { 0.0f };
    };

    // Position and movement
    glm::vec3 position { 0.0f };
    glm::vec3 previousPosition { 0.0f }; // Birth position, refreshed before each movement operator.
    glm::vec3 velocity { 0.0f };
    glm::vec3 acceleration { 0.0f };

    // Rotation
    glm::vec3 rotation { 0.0f };
    glm::vec3 angularVelocity { 0.0f };
    glm::vec3 angularAcceleration { 0.0f };

    // Visual properties
    glm::vec3 color { 1.0f };
    glm::vec3 emitterColor { 1.0f }; // Working RGB before birth initializers.
    float alpha { 1.0f };
    float size { 20.0f };

    // Lifetime
    float lifetime { 1.0f }; // Total lifetime in seconds
    float age { 0.0f }; // Current age in seconds

    // Native random-frame animation, oscillation and turbulence share one stable fraction.
    float oscillationRandom { -1.0f };

    // Initial values for resets/multipliers
    struct {
	glm::vec3 color { 1.0f };
	float alpha { 1.0f };
	float size { 20.0f };
	float lifetime { 1.0f };
    } initial;

    bool alive { false };
    uint32_t remapSlot { 0 };
    bool nativeSlotState { false };
    bool shouldSimulate () const { return alive || nativeSlotState; }

    // Rope trails follow one particle through time. Keeping the history on the
    // particle also makes order-preserving compaction move it with its owner.
    std::vector<TrailPoint> trailHistory;
    TrailPoint trailLastFrame;
    bool trailLastFrameValid { false };

    // Get normalized lifetime position (0.0 to 1.0)
    float getLifetimePos () const {
        // Retired native slots keep aging with a zero lifetime; operator envelopes
        // consume that division, rather than the live-view fallback endpoint.
        return nativeSlotState ? age / lifetime : lifetime > 0.0f ? age / lifetime : 1.0f;
    }

    bool isAlive () const {
        return alive && (nativeSlotState ? lifetime != 0.0f && !(lifetime < age) : age < lifetime);
    }
};

void applyParticleAttractionDeletion (
    ParticleInstance& particle, const glm::vec3& center, float radius
);

void applyParticlePlaneCollision (
    ParticleInstance& particle, const glm::vec3& plane, float distance, float bounceFactor,
    ParticleCollisionBehavior behavior, bool stopRotation
);

void applyParticleQuadCollision (
    ParticleInstance& particle, const glm::vec3& origin, const glm::vec3& plane,
    const glm::vec3& right, const glm::vec3& forward, const glm::vec2& halfSize, float bounceFactor,
    ParticleCollisionBehavior behavior, bool stopRotation
);

void applyParticleSphereCollision (
    ParticleInstance& particle, const glm::vec3& origin, float radius, float bounceFactor,
    ParticleCollisionBehavior behavior, bool stopRotation
);

[[nodiscard]] std::array<glm::vec4, 4> calculateParticleBoundsPlanes (
    const glm::ivec2& extent, const glm::mat4& worldToLocal
);

void applyParticleBoundsCollision (
    ParticleInstance& particle, const std::array<glm::vec4, 4>& planes, float bounceFactor,
    ParticleCollisionBehavior behavior
);

struct ParticleModelCapsule {
    glm::vec3 origin;
    glm::vec3 direction;
    float length;
    float radius;
};

[[nodiscard]] std::optional<int> resolveParticleModelCollisionDependency (
    const std::vector<ObjectDependency>& dependencies, uint32_t index
);
[[nodiscard]] glm::mat4 calculateParticleModelCollisionSpace (const glm::mat4& ownerWorld, bool worldSpace);
[[nodiscard]] ParticleModelCapsule calculateParticleModelCapsule (
    const glm::vec3& halfExtents, const glm::vec3& center, const glm::mat4& transform, bool boneHitBox = false
);
void applyParticleModelCollision (
    ParticleInstance& particle, const std::vector<ParticleModelCapsule>& capsules,
    float bounceFactor, ParticleCollisionBehavior behavior
);

void applyParticleAngularMovement (
    ParticleInstance& particle, const glm::vec3& force, float drag,
    float rawDelta, float forceDelta, float lifetimeWeight
);

[[nodiscard]] float calculateParticleOscillationMultiplier (
    const ParticleInstance& particle, glm::vec2 frequencyRange, glm::vec2 phaseRange,
    glm::vec2 scaleRange, glm::vec4 blendTimes
);
[[nodiscard]] glm::vec3 calculateParticlePositionOscillation (
    const ParticleInstance& particle, glm::vec2 frequencyRange, glm::vec2 phaseRange,
    glm::vec2 scaleRange, glm::vec3 mask, glm::vec4 blendTimes, float deltaTime
);

void initializeParticleBetweenControlPoints (
    ParticleInstance& particle, const MapSequenceBetweenControlPointsInitializer& initializer,
    const glm::vec3& start, const glm::vec3& end, float& phase, float& direction, float countMultiplier = 1.0f
);

/**
 * Control point runtime data
 */
struct ControlPointData {
    glm::vec3 position { 0.0f };
    glm::vec3 offset { 0.0f };
    glm::vec3 basePosition { 0.0f };
    glm::mat3 baseOrientation { 1.0f };
    glm::mat3 orientation { 1.0f };
    uint32_t flags { 0 };
    int parentControlPoint { 0 };
    glm::vec3 velocity { 0.0f };
    glm::vec3 displacement { 0.0f };
    glm::vec3 previousPosition { 0.0f };
    bool hasPreviousPosition { false };
    glm::vec3 simulationPosition { 0.0f };
    glm::vec3 birthPreviousPosition { 0.0f };

    void sampleVelocity (const glm::vec3& simulationPosition, float frameTime);
};

void initializeParticleEventColor (
    ParticleInstance& particle, const ParticleInstance* parent, bool copyOpacity
);

void applyParticleEventColor (
    ParticleInstance& particle, const ParticleInstance* parent, glm::vec4 blendTimes
);

void applyParticleDistanceConstraint (
    ParticleInstance& particle, const ControlPointData& point, const glm::mat3& inverseBasis,
    float distance, float variableStrength, float deltaTime, glm::vec4 blendTimes
);

void applyParticleBetweenControlPointConstraint (
    std::vector<ParticleInstance>& particles, uint32_t count, const ControlPointData& start,
    const ControlPointData& end, glm::vec4 blendTimes
);

void applyParticleControlPointRemap (
    ParticleInstance& particle, const ControlPointRemap& remap,
    const std::vector<ControlPointData>& controlPoints, bool initializer, bool angularVelocityAvailable = false,
    float runtime = 0.0f, float timeOfDay = 0.0f, glm::vec3 layerOrigin = glm::vec3 (0.0f),
    float initializerPhase = 0.0f, float systemTime = 0.0f, float layerTime = 0.0f, glm::vec3* pointOutput = nullptr
);

/**
 * Particle emitter function
 */
using EmitterFunc = std::function<void (std::vector<ParticleInstance>&, uint32_t&, float, uint32_t)>;

/**
 * Particle initializer function
 */
using InitializerFunc = std::function<void (ParticleInstance&)>;

/**
 * Particle operator function
 */
using OperatorFunc = std::function<
    void (std::vector<ParticleInstance>&, uint32_t, const std::vector<ControlPointData>&, float, float)>;

class CParticle final : public CRenderable, public Scripting::ScriptableObject {
    friend CObject;

public:
    CParticle (Wallpapers::CScene& scene, const Particle& particle, CParticle* parent = nullptr);
    ~CParticle ();

    void setup () override;
    void render () override;
    void renderReflection (const glm::mat4& view, const glm::vec3& eye);
    void rebaseRenderTime (double offset) override;
    void update (float dt);
    void play ();
    void pause ();
    void stop ();
    /** Queue a burst from each authored emitter, including while paused or stopped. */
    void emitParticles (uint32_t count = 1);
    [[nodiscard]] bool isPlaying () const { return m_emitting; }

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
    void updateControlPoints (float frameTime = 0.0f, bool refreshOnly = false);
    void includeControlPoint (uint32_t index);
    [[nodiscard]] glm::mat4 particleWorldMatrix ();

    // Emitter creators
    [[nodiscard]] glm::vec3 resolveEmitterOrigin (const glm::vec3& origin) const;
    EmitterFunc createBoxEmitter (const ParticleEmitter& emitter);
    EmitterFunc createSphereEmitter (const ParticleEmitter& emitter);
    EmitterFunc createLayerImageEmitter (const ParticleEmitter& emitter, uint32_t imageIndex);
    struct LayerImageCache {
	const CImage* source = nullptr;
	int sourceId = -1;
	glm::uvec2 extent { 0 };
	bool ready = false;
	std::vector<ParticleLayerImageSample> samples;
    };
    bool updateLayerImageCache (LayerImageCache& cache);

    // Initializer creators
    [[nodiscard]] glm::vec3 remapLayerOrigin () const;
    [[nodiscard]] float remapLayerTime () const;
    void updateRemapClocks (float frameDelta, bool visible);
    void clearParticles (bool stopEmission);
    InitializerFunc createControlPointRemapInitializer (const ControlPointRemapInitializer& init);
    InitializerFunc createColorRandomInitializer (const ColorRandomInitializer& init);
    InitializerFunc createInheritInitialEventColorInitializer (const InheritInitialEventColorInitializer& init);
    InitializerFunc createSizeRandomInitializer (const SizeRandomInitializer& init);
    InitializerFunc createAlphaRandomInitializer (const AlphaRandomInitializer& init);
    InitializerFunc createLifetimeRandomInitializer (const LifetimeRandomInitializer& init);
    InitializerFunc createVelocityRandomInitializer (const VelocityRandomInitializer& init);
    InitializerFunc createInheritControlPointVelocityInitializer (const InheritControlPointVelocityInitializer& init);
    InitializerFunc createRotationRandomInitializer (const RotationRandomInitializer& init);
    InitializerFunc createAngularVelocityRandomInitializer (const AngularVelocityRandomInitializer& init);
    InitializerFunc createTurbulentVelocityRandomInitializer (const TurbulentVelocityRandomInitializer& init);
    InitializerFunc createPositionOffsetRandomInitializer (const PositionOffsetRandomInitializer& init);
    InitializerFunc
    createMapSequenceAroundControlPointInitializer (const MapSequenceAroundControlPointInitializer& init);
    InitializerFunc
    createMapSequenceBetweenControlPointsInitializer (const MapSequenceBetweenControlPointsInitializer& init);

    // Operator creators
    OperatorFunc createControlPointRemapOperator (const ControlPointRemapOperator& op);
    OperatorFunc createMovementOperator (const MovementOperator& op);
    OperatorFunc createInheritEventColorOperator (const InheritEventColorOperator& op);
    OperatorFunc createReduceMovementNearControlPointOperator (const ReduceMovementNearControlPointOperator& op);
    OperatorFunc createMaintainDistanceToControlPointOperator (const MaintainDistanceToControlPointOperator& op);
    OperatorFunc createMaintainDistanceBetweenControlPointsOperator (const MaintainDistanceBetweenControlPointsOperator& op);
    OperatorFunc createCollisionPlaneOperator (const CollisionPlaneOperator& op);
    OperatorFunc createCollisionQuadOperator (const CollisionQuadOperator& op);
    OperatorFunc createCollisionSphereOperator (const CollisionSphereOperator& op);
    OperatorFunc createCollisionModelOperator (const CollisionModelOperator& op, uint32_t dependencyIndex);
    OperatorFunc createBoidsOperator (const BoidsOperator& op);
    OperatorFunc createCollisionBoundsOperator (const CollisionBoundsOperator& op);
    OperatorFunc createCapVelocityOperator (const CapVelocityOperator& op);
    OperatorFunc createAngularMovementOperator (const AngularMovementOperator& op);
    OperatorFunc createAlphaFadeOperator (const AlphaFadeOperator& op);
    OperatorFunc createSizeChangeOperator (const SizeChangeOperator& op);
    OperatorFunc createAlphaChangeOperator (const AlphaChangeOperator& op);
    OperatorFunc createColorChangeOperator (const ColorChangeOperator& op);
    OperatorFunc createTurbulenceOperator (const TurbulenceOperator& op);
    OperatorFunc createVortexV2Operator (const VortexOperator& op);
    OperatorFunc createVortexOperator (const VortexOperator& op);
    OperatorFunc createControlPointAttractOperator (const ControlPointAttractOperator& op);
    OperatorFunc createOscillateAlphaOperator (const OscillateAlphaOperator& op);
    OperatorFunc createOscillateSizeOperator (const OscillateSizeOperator& op);
    OperatorFunc createOscillatePositionOperator (const OscillatePositionOperator& op);

    // Rendering
    struct ReflectionView {
	const glm::mat4& view;
	const glm::vec3& eye;
    };
    void renderSprites (const ReflectionView* reflection = nullptr);
    void renderRope ();
    void renderRopeTrail ();
    void updateRopeTrailHistory (float dt);
    void setupPass ();
    void setupGeometryCallbacks ();
    void setupParticleUniforms ();
    void updateMatrices (const ReflectionView* reflection = nullptr);
    void applyParallaxToModelMatrix ();
    void updateParticleViewProjection ();
    void updateParticleRenderVars ();
    void updateInstanceParameters (bool allowColorOverride);
    void activateInstanceParameters ();
    void refreshInstanceParameters (bool suppressColorOverride = false);
    [[nodiscard]] glm::vec3 adjustInstanceColor (const glm::vec3& color) const;

private:
    bool updateForRender ();
    const Particle& m_particle;
    CParticle* m_particleParent = nullptr;
    std::array<bool, PARTICLE_CONTROL_POINT_COUNT> m_controlPointOverridesChanged {};
    bool m_controlPointAnglesChanged = false;
    bool m_controlPointTimelines = false;
    uint32_t m_controlPointCount = 0;
    std::vector<Data::Utils::ScopeGuard<std::function<void ()>>> m_controlPointSubscriptions;
    std::vector<Data::Utils::ScopeGuard<std::function<void ()>>> m_instanceParameterSubscriptions;
    float m_turbulenceRate = 1.0f;
    glm::vec3 m_instanceColor { 1.0f };
    glm::vec3 m_colorOverrideDelta { 0.0f };
    bool m_colorOverrideActive = false;
    bool m_propagateColorOverride = false;
    bool m_instanceParametersDirty = false;
    bool m_instanceParameterTimelines = false;
    glm::mat4 m_controlPointTransform { 1.0f };
    glm::mat4 m_commonControlPointTransform { 1.0f };
    struct ChildSystem {
	// The renderer holds references into the definition and must die first.
	ObjectUniquePtr definition;
	std::unique_ptr<CParticle> renderer;
    };
    std::vector<ChildSystem> m_children;
    struct FollowChildSystem {
	ObjectUniquePtr definition;
	const ParticleChild* settings = nullptr;
	struct Instance {
	    std::unique_ptr<CParticle> renderer;
	    uint64_t parentSerial = 0;
	    bool hasStarted = false;
	};
	std::vector<Instance> instances;
    };
    std::vector<FollowChildSystem> m_followChildren;
    std::optional<glm::vec3> m_followPosition;
    uint64_t m_eventParentSerial = 0;
    // Event-follow child of a world-space system: it lives in world units, not the parent layer's frame.
    bool m_followsWorldParticle = false;
    uint64_t m_nextParticleSerial = 0;
    size_t m_childSystemCount = 0; // Root-owned allocation budget for the entire tree.
    size_t m_childAllocationLimit = 64; // Absolute end of this branch's share of that budget.
    int m_controlPointStartIndex = 0;

    void setupChildren ();
    void prewarm (bool includePermanentChildren);
    void spawnEventChild (FollowChildSystem& child, const ParticleInstance& particle);
    void updateFollowChildren (uint32_t firstNewParticle);
    [[nodiscard]] bool hasLivingParticles () const;
    [[nodiscard]] bool hasPendingEmission () const;
    [[nodiscard]] const ParticleInstanceOverride& getInstanceOverride () const;

    std::vector<ParticleInstance> m_particles;
    uint32_t m_particleCount { 0 };
    // The public/render view stays dense. Only CP-output owners need inactive
    // native slots to retain and evolve state through the high-water extent.
    std::vector<ParticleInstance> m_remapSlots;
    uint32_t m_remapAllocationCursor { 0 };
    uint32_t m_remapNativeCount { 0 };
    bool m_hasControlPointOutputOperator { false };
    bool m_hasBoidsOperator { false };
    uint32_t m_boidsSlotExtent { 0 };
    bool m_hasModelCollisionOperator { false };
    bool usesRoundedParticleSlots () const { return m_hasBoidsOperator || m_hasModelCollisionOperator; }
    bool usesNativeParticleSlots () const { return m_hasControlPointOutputOperator || usesRoundedParticleSlots (); }
    void advanceRemapSlots (float dt);
    uint32_t acquireRemapSlot ();
    void synchronizeRemapSlots ();
    void publishRemapSlots ();
    uint32_t m_retiredParticleCount { 0 };
    bool m_hasPrewarmInTree = false;
    bool m_emitting = true;
    uint32_t m_pendingEmission = 0;
    std::vector<ControlPointData> m_pendingControlPoints;
    glm::mat4 m_pendingBirthTransform { 1.0f };
    float m_pendingSystemTime = 0.0f;
    float m_pendingLayerTime = 0.0f;
    uint32_t m_maxParticles { DEFAULT_MAX_PARTICLES };

    std::vector<EmitterFunc> m_emitters;
    size_t m_pendingEmitterCount = 0;
    std::vector<InitializerFunc> m_initializers;
    std::vector<OperatorFunc> m_operators;
    bool m_hasAngularVelocity { false };
    bool m_hasAlphaOperators { false };
    bool m_hasColorOperators { false };

    std::vector<ControlPointData> m_controlPoints;
    bool m_initializingManualEmission { false };
    bool m_hasMutatingRemapInitializer { false };
    void applyInitialControlPointRemap (ParticleInstance& particle, const ControlPointRemap& remap);
    void finalizeParticleBirths (uint32_t first, uint32_t firstManual);
    void finishManualParticleBirths (uint32_t firstNewParticle);

    std::vector<float> m_vertices;
    std::vector<uint32_t> m_indices;

    double m_time { 0.0 };
    float m_simulationTime { 0.0f };
    float m_frameSimulationDelta { 0.0f };
    std::optional<uint32_t> m_reflectionPreparedFrame;
    bool m_reflectionFrameVisible = false;
    float m_remapSystemTime = 0.0f;
    float m_remapLayerTime = 0.0f;
    float m_remapDelta = 0.0f;

    // CPass-based rendering
    Effects::CPass* m_pass { nullptr };
    std::unique_ptr<ImageEffectPassOverride> m_passOverride;
    std::shared_ptr<FBOProvider> m_passFBOProvider;
    TextureMap m_passBinds;
    GLsizei m_activeIndexCount { 0 };

    // REFRACT support: copy of scene FBO to avoid read-write conflict
    bool m_hasRefract { false };
    std::shared_ptr<CFBO> m_refractFBO;

    /**
     * Create and configure the VAO/VBO/EBO on first render. VAOs are container objects
     * and are NOT shared between GL contexts, so an async-built wallpaper must create
     * them on the render thread instead of the worker's build context.
     */
    void setupVao ();
    void uploadGeometryBuffers (GLsizeiptr vertexBytes, GLsizeiptr indexBytes);

    // OpenGL buffers
    GLuint m_vao { 0 };
    uint64_t m_vaoShaderRevision { 0 };
    GLuint m_vbo { 0 };
    GLuint m_ebo { 0 };
    GLint m_prevVAO { 0 };

    // Particle-specific uniform data (stored here, pointed to by CPass)
    glm::mat4 m_modelMatrix { 1.0f };
    glm::mat4 m_worldModelMatrix { 1.0f }; // Scene projection/parallax without emitter transforms.
    glm::mat4 m_modelMatrixInverse { 1.0f };
    glm::mat4 m_mvpMatrix { 1.0f };
    glm::mat4 m_mvpMatrixInverse { 1.0f };
    glm::mat4 m_viewProjectionMatrix { 1.0f };
    glm::vec3 m_orientationUp { 0.0f, 1.0f, 0.0f };
    glm::vec3 m_orientationRight { 1.0f, 0.0f, 0.0f };
    glm::vec3 m_orientationForward { 0.0f, 0.0f, 1.0f };
    glm::vec3 m_viewUp { 0.0f, 1.0f, 0.0f };
    glm::vec3 m_viewRight { 1.0f, 0.0f, 0.0f };
    glm::vec3 m_eyePosition { 0.0f, 0.0f, 1000.0f };
    glm::vec4 m_renderVar0 { 0.0f };
    glm::vec4 m_renderVar1 { 0.0f };

    // Spritesheet animation data
    int m_spritesheetCols { 0 };
    int m_spritesheetRows { 0 };
    int m_spritesheetFrames { 0 };

    // Material shader constants
    float m_overbright { 1.0f };
    float m_refractAmount { 0.05f }; // Default from shader annotation

    // Renderer configuration
    bool m_useTrailRenderer { false };
    float m_trailLength { 0.05f };
    float m_trailMaxLength { 10.0f };
    float m_trailMinLength { 0.0f };
    // Rope renderer (rope + ropetrail both use genericropeparticle shader)
    bool m_useRopeRenderer { false };
    bool m_useRopeTrailRenderer { false };
    int m_ropeSubdivision { 4 }; // Catmull-Rom subdivisions between points (smoothing)
    int m_ropeSegments { 8 }; // ropetrail: historical position snapshots per particle
    float m_ropeUVScale { 1.0f };
    float m_ropeMeanLifetime { 0.0f };
    float m_ropeEmitterRate { 0.0f };
    bool m_ropeUVScrolling { false };
    bool m_ropeUVSmoothing { true }; // rope only
    bool m_ropeFadeAlpha { false };
    bool m_ropeFadeSize { false };
    float m_ropeHistoryTimer { 0.0f };

    // Per-vertex float counts for different renderer types
    static constexpr int SPRITE_FLOATS_PER_VERTEX = 17;
    static constexpr int ROPE_FLOATS_PER_VERTEX = 26;

    // Random number generator
    std::mt19937 m_rng;

    bool m_initialized { false };
};
} // namespace WallpaperEngine::Render::Objects
