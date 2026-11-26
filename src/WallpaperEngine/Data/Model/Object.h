#pragma once

#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <glm/glm.hpp>

#include "Effect.h"
#include "Material.h"
#include "MdlAnimation.h"
#include "MdlMesh.h"
#include "Model.h"
#include "Types.h"
#include "UserSetting.h"
#include "WallpaperEngine/Data/Utils/TypeCaster.h"
#include <memory>

namespace WallpaperEngine::Data::Model {
using namespace WallpaperEngine::Data::Utils;

struct ObjectDependency {
    int id;
    uint32_t index;
    std::string type;
    std::string mask;
};

struct ObjectData {
    int id;
    std::string name;
    std::vector<int> dependencies;
    std::optional<int> parent;
    /** Named puppet attachment on the parent model, when this object is bone-attached. */
    std::optional<std::string> attachment;
    /** The point of origin of the object */
    UserSettingUniquePtr origin;
    /** Whether the object's transform is locked in the editor UI (no effect on rendering) */
    bool locktransforms;
    /** Parallax depth as authored; the root-most layer controls its subtree and defaults to 1 */
    std::optional<glm::vec2> authoredParallaxDepth;
    /** Transform fields for generic scene/group objects. Typed objects keep their own transform fields. */
    UserSettingUniquePtr groupScale;
    UserSettingUniquePtr groupAngles;
    UserSettingUniquePtr groupVisible;
    /** Authored configuration returned by SceneScript getInitialLayerConfig(). */
    std::string initialConfiguration;
    /** Visible hit layers stop cursor events from reaching lower layers. */
    bool disablePropagation = false;
    /** Cameras retain a generic data object but never admit volumetric draws. */
    bool cameraLayer = false;
    /** Typed runtime bindings retain their authored dependency index. */
    std::vector<ObjectDependency> typedDependencies;
};

/**
 * Base class for all objects, represents a single object in the scene
 *
 * @see Image
 * @see Sound
 * @see Particle
 * @see Text
 * @see Light
 */
class Object : public TypeCaster, public ObjectData {
public:
    explicit Object (ObjectData data) noexcept : TypeCaster (), ObjectData (std::move (data)) { };
    ~Object () override = default;
};

/**
 * Overrides effect's passes configuration
 *
 * @see ImageEffect
 * @see EffectPass
 */
struct ImageEffectPassOverride {
    int id;
    ComboMap combos;
    ShaderConstantMap constants;
    TextureMap textures;
    TextureMap usertextures;
    std::optional<float> userTextureReferenceWidth;
    std::optional<float> userTextureReferenceHeight;
    std::optional<std::string> shaderOverride; // Overrides MaterialPass::shader when set
};

/**
 * Override information for an specific effect
 *
 * @see ImageEffect
 * @see Effect
 * @see EffectPass
 * @see ImageEffectPass
 */
struct ImageEffect {
    /** Not sure what it's used for */
    int id;
    /** Effect's name for the editor */
    std::string name;
    /** If this effect is visible or not */
    UserSettingUniquePtr visible;
    /** Pass overrides to apply to the effect's passes */
    std::vector<ImageEffectPassOverrideUniquePtr> passOverrides;
    /** The effect definition */
    EffectUniquePtr effect;
};

/**
 * Animation layers for the puppet warp
 */
struct ImageAnimationLayer {
    int id;
    std::string name;
    UserSettingUniquePtr rate;
    UserSettingUniquePtr visible;
    UserSettingUniquePtr blend;
    UserSettingUniquePtr animation;
    UserSettingUniquePtr additive;
    UserSettingUniquePtr blendIn;
    UserSettingUniquePtr blendOut;
    UserSettingUniquePtr blendTime;
};

struct ShapeData {
    UserSettingUniquePtr perspective;
    UserSettingUniquePtr parallaxDepth;
    UserSettingUniquePtr alpha;
    UserSettingUniquePtr color;
    UserSettingUniquePtr brightness;
    std::vector<ImageEffectUniquePtr> effects;
};

class Shape : public Object, public ShapeData {
public:
    Shape (ObjectData data, ShapeData shape) : Object (std::move (data)), ShapeData (std::move (shape)) { }
};

struct ImageData {
    /** The scale of the image */
    UserSettingUniquePtr scale;
    /** The rotation of the image */
    UserSettingUniquePtr angles;
    /** If the image is visible or not */
    UserSettingUniquePtr visible;
    /** Keep the image available as an offscreen LED source even when hidden. */
    UserSettingUniquePtr ledSource;
    /** The alpha of the image */
    UserSettingUniquePtr alpha;
    /** The color of the image */
    UserSettingUniquePtr color;
    // TODO: WRITE A COUPLE OF ENUMS FOR THIS
    /** The alignment of the image */
    std::string alignment;
    /** The size of the image in pixels */
    glm::vec2 size;
    /** An authored zero-sized helper must not be enlarged by the implicit-size fallback. */
    bool sizeSpecified = false;
    /** Parallax depth used for parallax scrolling */
    UserSettingUniquePtr parallaxDepth;
    /** The color blending mode for this image */
    UserSettingUniquePtr colorBlendMode;
    /** The brightness of the image */
    UserSettingUniquePtr brightness;
    /** Composition layers optionally seed their isolated child surface with the scene behind them. */
    bool copyBackground;
    /** Point filtering for this layer's automatic composition buffers. */
    UserSettingUniquePtr noInterpolation;
    /** Use the scene virtual perspective camera for this image. */
    UserSettingUniquePtr perspective;
    /** The material in use for this image */
    ModelUniquePtr model;
    /** The effects applied to this image after the material is rendered */
    std::vector<ImageEffectUniquePtr> effects;
    /** The animation layers used in the puppet warp */
    std::vector<ImageAnimationLayerUniquePtr> animationLayers;
};

class Image : public Object, public ImageData {
public:
    explicit Image (ObjectData data, ImageData imageData) noexcept :
	Object (std::move (data)), ImageData (std::move (imageData)) { };
    ~Image () override = default;
};

struct SoundData {
    /** Authored volume, including user properties, scripts and keyframes. */
    UserSettingUniquePtr volume;
    /** Position-dependent attenuation for mono sound sources. */
    UserSettingUniquePtr spatialization;
    UserSettingUniquePtr attenuation;
    UserSettingUniquePtr minDistance;
    /** Wait for a script or event to start this layer. */
    bool startSilent = false;
    /** Delay between randomly selected sounds, in seconds. */
    UserSettingUniquePtr minTime;
    UserSettingUniquePtr maxTime;
    /** Playback mode, loop, */
    // TODO: WRITE AN ENUM FOR THIS
    std::optional<std::string> playbackmode;
    std::vector<std::string> sounds;
};

class Sound : public Object, public SoundData {
public:
    explicit Sound (ObjectData data, SoundData soundData) noexcept :
	Object (std::move (data)), SoundData (std::move (soundData)) { };
    ~Sound () override = default;
};

/**
 * Particle control points for forces and positions
 */
/** Number of control points a particle system exposes (controlpoint0..7 in the assets) */
constexpr int PARTICLE_CONTROL_POINT_COUNT = 8;

struct ParticleControlPoint {
    int id;
    uint32_t flags;
    glm::vec3 offset;
    bool lockToPointer;
    int parentControlPoint = 0;
};

/**
 * Particle emitter configuration
 */
struct ParticleEmitter {
    int id;
    std::string name;
    glm::vec3 directions;
    glm::vec3 distanceMin;
    glm::vec3 distanceMax;
    glm::vec3 origin;
    glm::ivec3 sign;
    uint32_t instantaneous;
    float speedMin;
    float speedMax;
    float rate;
    int controlPoint;
    uint32_t flags;
    float cone;
    float delay;
    float duration;
    glm::vec2 audioProcessingBounds;
    float audioProcessingExponent;
    int audioProcessingFrequencyStart;
    int audioProcessingFrequencyEnd;
    int audioProcessingMode;
    float minPeriodicDelay;
    float maxPeriodicDelay;
    float minPeriodicDuration;
    float maxPeriodicDuration;
    uint32_t maxToEmitPerPeriod;
    // Omitted box fields resolve against the scene projection when instantiated.
    bool defaultDirections = false;
    bool defaultDistanceMax = false;
    // Missing image offset bounds resolve against the actual scene projection.
    std::optional<glm::vec3> offsetMin;
    std::optional<glm::vec3> offsetMax;
};

/**
 * Particle initializer base and implementations
 */
class ParticleInitializerBase : public TypeCaster {
public:
    virtual ~ParticleInitializerBase () = default;
};

// The measured two-control-point and lifetime-noise subsets of native remaps.
struct ControlPointRemap {
    enum class Input { BetweenPoints, DistanceToPoint, MaxLifetime, LifetimeFraction, Size, Opacity, Speed, Rotation, AngularSpeed, Runtime, TimeOfDay, ParticleSystemTime, Position, Velocity, Color, ControlPoint, DeltaToPoint, DirectionToPoint, LayerOrigin, LayerTime };
    enum class Output { BetweenPoints, DistanceToPoint, MaxLifetime, Size, Opacity, Speed, Velocity, Rotation, AngularSpeed, Position, Color, DeltaToPoint, DirectionToPoint, ControlPoint };
    enum class Transform { Identity, Sine, Square, Saw, Triangle, SimplexNoise, FbmNoise };
    enum class Operation { Remap, Multiply, Add, Subtract };
    enum class Component { All, X, Y, Z, Sum, Average, Max, Min };
    Input input;
    Output output;
    Transform transform;
    UserSettingUniquePtr inputRangeMin;
    UserSettingUniquePtr inputRangeMax;
    UserSettingUniquePtr outputRangeMin;
    UserSettingUniquePtr outputRangeMax;
    uint32_t inputControlPoint0;
    uint32_t inputControlPoint1;
    uint32_t outputControlPoint0;
    uint32_t outputControlPoint1;
    float transformInputScale { 2.0f };
    Operation operation { Operation::Remap };
    uint32_t flags { 1 };
    glm::vec4 blendTimes { 0.0f, 0.0f, 1.0f, 1.0f };
    Component inputComponent { Component::All };
    Component outputComponent { Component::All };
    uint32_t transformOctaves { 3 };
};

class ControlPointRemapInitializer : public ParticleInitializerBase {
public:
    explicit ControlPointRemapInitializer (ControlPointRemap remap) : remap (std::move (remap)) { }
    ControlPointRemap remap;
};

class InheritInitialEventColorInitializer : public ParticleInitializerBase {
public:
    explicit InheritInitialEventColorInitializer (bool copyOpacity) : copyOpacity (copyOpacity) { }
    bool copyOpacity;
};

class ColorRandomInitializer : public ParticleInitializerBase {
public:
    ColorRandomInitializer (UserSettingUniquePtr min, UserSettingUniquePtr max, float exponent = 1.0f) :
	min (std::move (min)), max (std::move (max)), exponent (exponent) { }
    UserSettingUniquePtr min;
    UserSettingUniquePtr max;
    float exponent;
};

class SizeRandomInitializer : public ParticleInitializerBase {
public:
    SizeRandomInitializer (UserSettingUniquePtr min, UserSettingUniquePtr max, UserSettingUniquePtr exponent) :
	min (std::move (min)), max (std::move (max)), exponent (std::move (exponent)) { }
    UserSettingUniquePtr min;
    UserSettingUniquePtr max;
    UserSettingUniquePtr exponent;
};

class AlphaRandomInitializer : public ParticleInitializerBase {
public:
    AlphaRandomInitializer (UserSettingUniquePtr min, UserSettingUniquePtr max) :
	min (std::move (min)), max (std::move (max)) { }
    UserSettingUniquePtr min;
    UserSettingUniquePtr max;
};

class LifetimeRandomInitializer : public ParticleInitializerBase {
public:
    LifetimeRandomInitializer (UserSettingUniquePtr min, UserSettingUniquePtr max, UserSettingUniquePtr exponent) :
	min (std::move (min)), max (std::move (max)), exponent (std::move (exponent)) { }
    UserSettingUniquePtr min;
    UserSettingUniquePtr max;
    UserSettingUniquePtr exponent;
};

class VelocityRandomInitializer : public ParticleInitializerBase {
public:
    VelocityRandomInitializer (UserSettingUniquePtr min, UserSettingUniquePtr max) :
	min (std::move (min)), max (std::move (max)) { }
    UserSettingUniquePtr min;
    UserSettingUniquePtr max;
};

class InheritControlPointVelocityInitializer : public ParticleInitializerBase {
public:
    InheritControlPointVelocityInitializer (UserSettingUniquePtr min, UserSettingUniquePtr max, int controlPoint) :
	min (std::move (min)), max (std::move (max)), controlPoint (controlPoint) { }
    UserSettingUniquePtr min;
    UserSettingUniquePtr max;
    int controlPoint;
};

class RotationRandomInitializer : public ParticleInitializerBase {
public:
    RotationRandomInitializer (UserSettingUniquePtr min, UserSettingUniquePtr max) :
	min (std::move (min)), max (std::move (max)) { }
    UserSettingUniquePtr min;
    UserSettingUniquePtr max;
};

class AngularVelocityRandomInitializer : public ParticleInitializerBase {
public:
    AngularVelocityRandomInitializer (
	UserSettingUniquePtr min, UserSettingUniquePtr max, UserSettingUniquePtr exponent
    ) : min (std::move (min)), max (std::move (max)), exponent (std::move (exponent)) { }
    UserSettingUniquePtr min;
    UserSettingUniquePtr max;
    UserSettingUniquePtr exponent;
};

class TurbulentVelocityRandomInitializer : public ParticleInitializerBase {
public:
    TurbulentVelocityRandomInitializer (
	UserSettingUniquePtr speedMin, UserSettingUniquePtr speedMax, UserSettingUniquePtr scale,
	UserSettingUniquePtr offset, UserSettingUniquePtr forward, UserSettingUniquePtr timeScale,
	UserSettingUniquePtr phaseMin, UserSettingUniquePtr phaseMax, UserSettingUniquePtr right
    ) :
	speedMin (std::move (speedMin)), speedMax (std::move (speedMax)), scale (std::move (scale)),
	offset (std::move (offset)), forward (std::move (forward)), timeScale (std::move (timeScale)),
	phaseMin (std::move (phaseMin)), phaseMax (std::move (phaseMax)), right (std::move (right)) { }
    UserSettingUniquePtr speedMin;
    UserSettingUniquePtr speedMax;
    UserSettingUniquePtr scale;
    UserSettingUniquePtr offset;
    UserSettingUniquePtr forward;
    UserSettingUniquePtr timeScale;
    UserSettingUniquePtr phaseMin;
    UserSettingUniquePtr phaseMax;
    UserSettingUniquePtr right;
};

class PositionOffsetRandomInitializer : public ParticleInitializerBase {
public:
    PositionOffsetRandomInitializer (
	UserSettingUniquePtr directions, UserSettingUniquePtr sign, UserSettingUniquePtr scale,
	UserSettingUniquePtr distance, UserSettingUniquePtr timeScale, int octaves
    ) :
	directions (std::move (directions)), sign (std::move (sign)), scale (std::move (scale)),
	distance (std::move (distance)), timeScale (std::move (timeScale)), octaves (octaves) { }
    // Omitted directions, scale and distance depend on the scene projection.
    UserSettingUniquePtr directions;
    UserSettingUniquePtr sign;
    UserSettingUniquePtr scale;
    UserSettingUniquePtr distance;
    UserSettingUniquePtr timeScale;
    int octaves;
};

class MapSequenceBetweenControlPointsInitializer : public ParticleInitializerBase {
public:
    MapSequenceBetweenControlPointsInitializer (
	UserSettingUniquePtr count, UserSettingUniquePtr bounds, std::string limitBehavior,
	int controlPointStart, int controlPointEnd, uint32_t flags, float arcAmount,
	glm::vec3 arcDirection, float sizeReductionAmount
    ) :
	count (std::move (count)), bounds (std::move (bounds)), limitBehavior (std::move (limitBehavior)),
	controlPointStart (controlPointStart), controlPointEnd (controlPointEnd), flags (flags),
	arcAmount (arcAmount), arcDirection (arcDirection), sizeReductionAmount (sizeReductionAmount) { }
    UserSettingUniquePtr count;
    UserSettingUniquePtr bounds;
    std::string limitBehavior;
    int controlPointStart;
    int controlPointEnd;
    uint32_t flags;
    float arcAmount;
    glm::vec3 arcDirection;
    float sizeReductionAmount;
};

class MapSequenceAroundControlPointInitializer : public ParticleInitializerBase {
public:
    MapSequenceAroundControlPointInitializer (
	UserSettingUniquePtr count, UserSettingUniquePtr bounds, UserSettingUniquePtr speedMin,
	UserSettingUniquePtr speedMax, UserSettingUniquePtr axis, std::string limitBehavior,
	UserSettingUniquePtr controlPoint, uint32_t flags
    ) :
	count (std::move (count)), bounds (std::move (bounds)), speedMin (std::move (speedMin)),
	speedMax (std::move (speedMax)), axis (std::move (axis)), limitBehavior (std::move (limitBehavior)),
	controlPoint (std::move (controlPoint)), flags (flags) { }
    UserSettingUniquePtr count;
    UserSettingUniquePtr bounds;
    UserSettingUniquePtr speedMin;
    UserSettingUniquePtr speedMax;
    UserSettingUniquePtr axis;
    std::string limitBehavior;
    UserSettingUniquePtr controlPoint;
    /** Bit1 opts sequence spacing into the instance count multiplier. */
    uint32_t flags;
};

using ParticleInitializerUniquePtr = std::unique_ptr<ParticleInitializerBase>;

/**
 * Particle operator base and implementations
 */
class ParticleOperatorBase : public TypeCaster {
public:
    virtual ~ParticleOperatorBase () = default;
};

class ControlPointRemapOperator : public ParticleOperatorBase {
public:
    explicit ControlPointRemapOperator (ControlPointRemap remap) : remap (std::move (remap)) { }
    ControlPointRemap remap;
};

// Bounded native input mode: setcoloropacity. Other event fields remain unsupported.
class InheritEventColorOperator : public ParticleOperatorBase {
public:
    explicit InheritEventColorOperator (glm::vec4 blendTimes) : blendTimes (blendTimes) { }
    glm::vec4 blendTimes;
};

class MovementOperator : public ParticleOperatorBase {
public:
    MovementOperator (UserSettingUniquePtr drag, UserSettingUniquePtr gravity) :
	drag (std::move (drag)), gravity (std::move (gravity)) { }
    UserSettingUniquePtr drag;
    UserSettingUniquePtr gravity;
};

enum class ParticleCollisionBehavior { Bounce, Slide, Stop, Delete };

class CollisionPlaneOperator : public ParticleOperatorBase {
public:
    CollisionPlaneOperator (
        glm::vec3 plane, UserSettingUniquePtr distance, UserSettingUniquePtr bounceFactor,
        ParticleCollisionBehavior behavior, uint32_t flags, uint32_t controlPoint
    ) : plane (plane), distance (std::move (distance)), bounceFactor (std::move (bounceFactor)),
        behavior (behavior), flags (flags), controlPoint (controlPoint) { }
    glm::vec3 plane;
    /** Omitted distance uses the scene's native 2D/3D default. */
    UserSettingUniquePtr distance;
    UserSettingUniquePtr bounceFactor;
    ParticleCollisionBehavior behavior;
    uint32_t flags;
    uint32_t controlPoint;
};

class CollisionQuadOperator : public ParticleOperatorBase {
public:
    CollisionQuadOperator (
        glm::vec3 plane, glm::vec3 right, glm::vec3 forward, UserSettingUniquePtr origin,
        UserSettingUniquePtr size, UserSettingUniquePtr bounceFactor,
        ParticleCollisionBehavior behavior, uint32_t flags, uint32_t controlPoint
    ) : plane (plane), right (right), forward (forward), origin (std::move (origin)), size (std::move (size)),
        bounceFactor (std::move (bounceFactor)), behavior (behavior), flags (flags), controlPoint (controlPoint) { }
    glm::vec3 plane;
    glm::vec3 right;
    glm::vec3 forward;
    /** Omitted origin and size use the scene's native 2D/3D defaults. */
    UserSettingUniquePtr origin;
    UserSettingUniquePtr size;
    UserSettingUniquePtr bounceFactor;
    ParticleCollisionBehavior behavior;
    uint32_t flags;
    uint32_t controlPoint;
};

class CollisionSphereOperator : public ParticleOperatorBase {
public:
    CollisionSphereOperator (
        UserSettingUniquePtr origin, UserSettingUniquePtr radius, UserSettingUniquePtr bounceFactor,
        ParticleCollisionBehavior behavior, uint32_t flags, uint32_t controlPoint
    ) : origin (std::move (origin)), radius (std::move (radius)), bounceFactor (std::move (bounceFactor)),
        behavior (behavior), flags (flags), controlPoint (controlPoint) { }
    /** Omitted origin and radius use the scene's native 2D/3D defaults. */
    UserSettingUniquePtr origin;
    UserSettingUniquePtr radius;
    UserSettingUniquePtr bounceFactor;
    ParticleCollisionBehavior behavior;
    uint32_t flags;
    uint32_t controlPoint;
};

class CollisionModelOperator : public ParticleOperatorBase {
public:
    CollisionModelOperator (float bounceFactor, ParticleCollisionBehavior behavior) :
        bounceFactor (bounceFactor), behavior (behavior) { }
    float bounceFactor;
    ParticleCollisionBehavior behavior;
};

class BoidsOperator : public ParticleOperatorBase {
public:
    BoidsOperator (
        std::optional<float> separationThreshold, std::optional<float> neighborThreshold,
        std::optional<float> maxSpeed, float separationFactor, float alignmentFactor,
        float cohesionFactor, uint32_t flags
    ) : separationThreshold (separationThreshold), neighborThreshold (neighborThreshold), maxSpeed (maxSpeed),
        separationFactor (separationFactor), alignmentFactor (alignmentFactor), cohesionFactor (cohesionFactor),
        flags (flags) { }
    std::optional<float> separationThreshold;
    std::optional<float> neighborThreshold;
    std::optional<float> maxSpeed;
    float separationFactor;
    float alignmentFactor;
    float cohesionFactor;
    uint32_t flags;
};

class CollisionBoundsOperator : public ParticleOperatorBase {
public:
    CollisionBoundsOperator (UserSettingUniquePtr bounceFactor, ParticleCollisionBehavior behavior) :
        bounceFactor (std::move (bounceFactor)), behavior (behavior) { }
    UserSettingUniquePtr bounceFactor;
    ParticleCollisionBehavior behavior;
};

class CapVelocityOperator : public ParticleOperatorBase {
public:
    CapVelocityOperator (UserSettingUniquePtr maxSpeed, glm::vec4 blendTimes) :
	maxSpeed (std::move (maxSpeed)), blendTimes (blendTimes) { }
    /** Omitted limits use the scene's native 2D/3D default. */
    UserSettingUniquePtr maxSpeed;
    glm::vec4 blendTimes;
};

class ReduceMovementNearControlPointOperator : public ParticleOperatorBase {
public:
    ReduceMovementNearControlPointOperator (
        uint32_t controlPoint, UserSettingUniquePtr distanceInner, UserSettingUniquePtr distanceOuter,
        UserSettingUniquePtr reductionInner, UserSettingUniquePtr reductionOuter, glm::vec4 blendTimes
    ) : controlPoint (controlPoint), distanceInner (std::move (distanceInner)),
        distanceOuter (std::move (distanceOuter)), reductionInner (std::move (reductionInner)),
        reductionOuter (std::move (reductionOuter)), blendTimes (blendTimes) { }
    uint32_t controlPoint;
    /** Omitted distances use the scene's native 2D/3D defaults. */
    UserSettingUniquePtr distanceInner;
    UserSettingUniquePtr distanceOuter;
    UserSettingUniquePtr reductionInner;
    UserSettingUniquePtr reductionOuter;
    glm::vec4 blendTimes;
};

class MaintainDistanceToControlPointOperator : public ParticleOperatorBase {
public:
    MaintainDistanceToControlPointOperator (
        uint32_t controlPoint, UserSettingUniquePtr distance, UserSettingUniquePtr variableStrength,
        glm::vec4 blendTimes
    ) : controlPoint (controlPoint), distance (std::move (distance)),
        variableStrength (std::move (variableStrength)), blendTimes (blendTimes) { }
    uint32_t controlPoint;
    /** Omitted distance uses the scene's native 2D/3D default. */
    UserSettingUniquePtr distance;
    UserSettingUniquePtr variableStrength;
    glm::vec4 blendTimes;
};

class MaintainDistanceBetweenControlPointsOperator : public ParticleOperatorBase {
public:
    MaintainDistanceBetweenControlPointsOperator (uint32_t start, uint32_t end, glm::vec4 blendTimes) :
        controlPointStart (start), controlPointEnd (end), blendTimes (blendTimes) { }
    uint32_t controlPointStart;
    uint32_t controlPointEnd;
    glm::vec4 blendTimes;
};

class AngularMovementOperator : public ParticleOperatorBase {
public:
    AngularMovementOperator (UserSettingUniquePtr drag, UserSettingUniquePtr force, glm::vec4 blendTimes) :
	drag (std::move (drag)), force (std::move (force)), blendTimes (blendTimes) { }
    UserSettingUniquePtr drag;
    UserSettingUniquePtr force;
    glm::vec4 blendTimes;
};

class AlphaFadeOperator : public ParticleOperatorBase {
public:
    AlphaFadeOperator (UserSettingUniquePtr fadeInTime, UserSettingUniquePtr fadeOutTime) :
	fadeInTime (std::move (fadeInTime)), fadeOutTime (std::move (fadeOutTime)) { }
    UserSettingUniquePtr fadeInTime;
    UserSettingUniquePtr fadeOutTime;
};

class SizeChangeOperator : public ParticleOperatorBase {
public:
    SizeChangeOperator (
	UserSettingUniquePtr startTime, UserSettingUniquePtr endTime, UserSettingUniquePtr startValue,
	UserSettingUniquePtr endValue
    ) :
	startTime (std::move (startTime)), endTime (std::move (endTime)), startValue (std::move (startValue)),
	endValue (std::move (endValue)) { }
    UserSettingUniquePtr startTime;
    UserSettingUniquePtr endTime;
    UserSettingUniquePtr startValue;
    UserSettingUniquePtr endValue;
};

class AlphaChangeOperator : public ParticleOperatorBase {
public:
    AlphaChangeOperator (
	UserSettingUniquePtr startTime, UserSettingUniquePtr endTime, UserSettingUniquePtr startValue,
	UserSettingUniquePtr endValue
    ) :
	startTime (std::move (startTime)), endTime (std::move (endTime)), startValue (std::move (startValue)),
	endValue (std::move (endValue)) { }
    UserSettingUniquePtr startTime;
    UserSettingUniquePtr endTime;
    UserSettingUniquePtr startValue;
    UserSettingUniquePtr endValue;
};

class ColorChangeOperator : public ParticleOperatorBase {
public:
    ColorChangeOperator (
	UserSettingUniquePtr startTime, UserSettingUniquePtr endTime, UserSettingUniquePtr startValue,
	UserSettingUniquePtr endValue
    ) :
	startTime (std::move (startTime)), endTime (std::move (endTime)), startValue (std::move (startValue)),
	endValue (std::move (endValue)) { }
    UserSettingUniquePtr startTime;
    UserSettingUniquePtr endTime;
    UserSettingUniquePtr startValue;
    UserSettingUniquePtr endValue;
};

class TurbulenceOperator : public ParticleOperatorBase {
public:
    TurbulenceOperator (
	UserSettingUniquePtr scale, UserSettingUniquePtr speedMin, UserSettingUniquePtr speedMax,
	UserSettingUniquePtr timeScale, UserSettingUniquePtr mask, UserSettingUniquePtr phaseMin,
	UserSettingUniquePtr phaseMax, UserSettingUniquePtr audioProcessingMode,
	UserSettingUniquePtr audioProcessingBounds, UserSettingUniquePtr audioProcessingExponent,
	UserSettingUniquePtr audioProcessingFrequencyStart, UserSettingUniquePtr audioProcessingFrequencyEnd,
	glm::vec4 blendTimes
    ) :
	scale (std::move (scale)), speedMin (std::move (speedMin)), speedMax (std::move (speedMax)),
	timeScale (std::move (timeScale)), mask (std::move (mask)), phaseMin (std::move (phaseMin)),
	phaseMax (std::move (phaseMax)), audioProcessingMode (std::move (audioProcessingMode)),
	audioProcessingBounds (std::move (audioProcessingBounds)),
	audioProcessingExponent (std::move (audioProcessingExponent)),
	audioProcessingFrequencyStart (std::move (audioProcessingFrequencyStart)),
	audioProcessingFrequencyEnd (std::move (audioProcessingFrequencyEnd)), blendTimes (blendTimes) { }
    UserSettingUniquePtr scale;
    UserSettingUniquePtr speedMin;
    UserSettingUniquePtr speedMax;
    UserSettingUniquePtr timeScale;
    UserSettingUniquePtr mask;
    UserSettingUniquePtr phaseMin;
    UserSettingUniquePtr phaseMax;
    UserSettingUniquePtr audioProcessingMode;
    UserSettingUniquePtr audioProcessingBounds;
    UserSettingUniquePtr audioProcessingExponent;
    UserSettingUniquePtr audioProcessingFrequencyStart;
    UserSettingUniquePtr audioProcessingFrequencyEnd;
    glm::vec4 blendTimes;
};

class VortexOperator : public ParticleOperatorBase {
public:
    VortexOperator (
	int controlPoint, int flags, UserSettingUniquePtr axis, UserSettingUniquePtr offset,
	UserSettingUniquePtr distanceInner, UserSettingUniquePtr distanceOuter, UserSettingUniquePtr speedInner,
	UserSettingUniquePtr speedOuter, UserSettingUniquePtr centerForce, UserSettingUniquePtr ringRadius,
	UserSettingUniquePtr ringWidth, UserSettingUniquePtr ringPullDistance, UserSettingUniquePtr ringPullForce,
	UserSettingUniquePtr audioProcessingMode, UserSettingUniquePtr audioProcessingBounds,
	bool nativeV2 = false, glm::vec4 blendTimes = { 0, 0, 1, 1 },
	UserSettingUniquePtr audioProcessingExponent = nullptr,
	UserSettingUniquePtr audioProcessingFrequencyStart = nullptr,
	UserSettingUniquePtr audioProcessingFrequencyEnd = nullptr
    ) :
	controlPoint (controlPoint), flags (flags), axis (std::move (axis)), offset (std::move (offset)),
	distanceInner (std::move (distanceInner)), distanceOuter (std::move (distanceOuter)),
	speedInner (std::move (speedInner)), speedOuter (std::move (speedOuter)), centerForce (std::move (centerForce)),
	ringRadius (std::move (ringRadius)), ringWidth (std::move (ringWidth)),
	ringPullDistance (std::move (ringPullDistance)), ringPullForce (std::move (ringPullForce)),
	audioProcessingMode (std::move (audioProcessingMode)),
	audioProcessingBounds (std::move (audioProcessingBounds)), nativeV2 (nativeV2), blendTimes (blendTimes),
	audioProcessingExponent (std::move (audioProcessingExponent)),
	audioProcessingFrequencyStart (std::move (audioProcessingFrequencyStart)),
	audioProcessingFrequencyEnd (std::move (audioProcessingFrequencyEnd)) { }
    int controlPoint;
    int flags; // 1 = infinite axis, 2 = maintain distance to center, 4 = ring shape
    UserSettingUniquePtr axis;
    UserSettingUniquePtr offset;
    UserSettingUniquePtr distanceInner; // Standard vortex inner radius
    UserSettingUniquePtr distanceOuter; // Standard vortex outer radius
    UserSettingUniquePtr speedInner;
    UserSettingUniquePtr speedOuter;
    UserSettingUniquePtr centerForce; // Strength to pull particles toward center
    UserSettingUniquePtr ringRadius; // Ring mode: radius of the ring
    UserSettingUniquePtr ringWidth; // Ring mode: width of the ring
    UserSettingUniquePtr ringPullDistance; // Ring mode: distance at which ring attracts particles
    UserSettingUniquePtr ringPullForce; // Ring mode: strength of ring attraction
    UserSettingUniquePtr audioProcessingMode;
    UserSettingUniquePtr audioProcessingBounds;
    bool nativeV2;
    glm::vec4 blendTimes;
    UserSettingUniquePtr audioProcessingExponent;
    UserSettingUniquePtr audioProcessingFrequencyStart;
    UserSettingUniquePtr audioProcessingFrequencyEnd;
};

class ControlPointAttractOperator : public ParticleOperatorBase {
public:
    ControlPointAttractOperator (
	int controlPoint, int flags, UserSettingUniquePtr scale, UserSettingUniquePtr threshold,
        UserSettingUniquePtr deleteThreshold, glm::vec4 blendTimes
    ) :
	controlPoint (controlPoint), flags (flags), scale (std::move (scale)),
	threshold (std::move (threshold)), deleteThreshold (std::move (deleteThreshold)), blendTimes (blendTimes) { }
    int controlPoint;
    int flags;
    // Missing values are resolved against the scene projection when instantiated.
    UserSettingUniquePtr scale;
    UserSettingUniquePtr threshold;
    UserSettingUniquePtr deleteThreshold;
    glm::vec4 blendTimes;
};

class OscillateAlphaOperator : public ParticleOperatorBase {
public:
    OscillateAlphaOperator (
	UserSettingUniquePtr frequencyMin, UserSettingUniquePtr frequencyMax, UserSettingUniquePtr scaleMin,
	UserSettingUniquePtr scaleMax, UserSettingUniquePtr phaseMin, UserSettingUniquePtr phaseMax,
	glm::vec4 blendTimes
    ) :
	frequencyMin (std::move (frequencyMin)), frequencyMax (std::move (frequencyMax)),
	scaleMin (std::move (scaleMin)), scaleMax (std::move (scaleMax)), phaseMin (std::move (phaseMin)),
	phaseMax (std::move (phaseMax)), blendTimes (blendTimes) { }
    UserSettingUniquePtr frequencyMin;
    UserSettingUniquePtr frequencyMax;
    UserSettingUniquePtr scaleMin;
    UserSettingUniquePtr scaleMax;
    UserSettingUniquePtr phaseMin;
    UserSettingUniquePtr phaseMax;
    glm::vec4 blendTimes;
};

class OscillateSizeOperator : public ParticleOperatorBase {
public:
    OscillateSizeOperator (
	UserSettingUniquePtr frequencyMin, UserSettingUniquePtr frequencyMax, UserSettingUniquePtr scaleMin,
	UserSettingUniquePtr scaleMax, UserSettingUniquePtr phaseMin, UserSettingUniquePtr phaseMax,
	glm::vec4 blendTimes
    ) :
	frequencyMin (std::move (frequencyMin)), frequencyMax (std::move (frequencyMax)),
	scaleMin (std::move (scaleMin)), scaleMax (std::move (scaleMax)), phaseMin (std::move (phaseMin)),
	phaseMax (std::move (phaseMax)), blendTimes (blendTimes) { }
    UserSettingUniquePtr frequencyMin;
    UserSettingUniquePtr frequencyMax;
    UserSettingUniquePtr scaleMin;
    UserSettingUniquePtr scaleMax;
    UserSettingUniquePtr phaseMin;
    UserSettingUniquePtr phaseMax;
    glm::vec4 blendTimes;
};

class OscillatePositionOperator : public ParticleOperatorBase {
public:
    OscillatePositionOperator (
	UserSettingUniquePtr frequencyMin, UserSettingUniquePtr frequencyMax, UserSettingUniquePtr scaleMin,
	UserSettingUniquePtr scaleMax, UserSettingUniquePtr phaseMin, UserSettingUniquePtr phaseMax,
	UserSettingUniquePtr mask, glm::vec4 blendTimes
    ) :
	frequencyMin (std::move (frequencyMin)), frequencyMax (std::move (frequencyMax)),
	scaleMin (std::move (scaleMin)), scaleMax (std::move (scaleMax)), phaseMin (std::move (phaseMin)),
	phaseMax (std::move (phaseMax)), mask (std::move (mask)), blendTimes (blendTimes) { }
    UserSettingUniquePtr frequencyMin;
    UserSettingUniquePtr frequencyMax;
    UserSettingUniquePtr scaleMin;
    UserSettingUniquePtr scaleMax;
    UserSettingUniquePtr phaseMin;
    UserSettingUniquePtr phaseMax;
    UserSettingUniquePtr mask;
    glm::vec4 blendTimes;
};

using ParticleOperatorUniquePtr = std::unique_ptr<ParticleOperatorBase>;

/**
 * Particle renderer configuration
 */
struct ParticleRenderer {
    std::string name;
    float length;
    float maxLength;
    float minLength;
    float subdivision;
    float segments; // ropetrail: number of history segments per particle
    bool segmentsExplicit;
    float uvScale;
    bool uvScrolling;
    bool uvSmoothing; // rope only: reduces flickering when lifetimes are identical
    bool fadeAlpha; // ropetrail: fade alpha along trail
    bool fadeSize; // ropetrail: fade size along trail
    std::string orientation { "screen" };
    glm::vec3 axis { 0.0f, 1.0f, 0.0f };
    uint32_t flags { 0 }; // bit 0: orientation ignores the layer transform
};

/**
 * Child particle system
 */
struct ParticleChild {
    std::string type;
    std::string name;
    int maxCount;
    int controlPointStartIndex;
    float probability;
    glm::vec3 angles;
    glm::vec3 origin;
    glm::vec3 scale;
    std::string particleFile;
};

/**
 * Instance override values
 */
struct ParticleInstanceOverride {
    UserSettingUniquePtr enabled;
    UserSettingUniquePtr alpha;
    UserSettingUniquePtr brightness;
    UserSettingUniquePtr size;
    UserSettingUniquePtr lifetime;
    UserSettingUniquePtr rate;
    UserSettingUniquePtr speed;
    UserSettingUniquePtr count;
    UserSettingUniquePtr color; // Neutral material tint; legacy color migrates into colorn
    UserSettingUniquePtr colorn; // Normalized instance override; negative X disables it
    /** Overrides retain each control point's local/world-space flags. */
    std::map<int, UserSettingUniquePtr> controlPointOffsets;
    std::map<int, UserSettingUniquePtr> controlPointAngles;
};

struct ParticleData {
    /** Position and transformation */
    UserSettingUniquePtr scale;
    UserSettingUniquePtr angles;
    UserSettingUniquePtr visible;

    /** Parallax depth */
    UserSettingUniquePtr parallaxDepth;

    /** Reference to particle definition file */
    std::string particleFile;

    /** Particle system configuration */
    std::string animationMode;
    float sequenceMultiplier;
    uint32_t maxCount;
    uint32_t startTime;
    uint32_t flags;

    /** Rounded definition color used as the reference for instance recoloring. */
    glm::vec3 colorReference {1.0f};
    bool hasColor = false;

    /** Material for rendering */
    ModelUniquePtr material;

    /** Emitters, initializers, operators, renderers */
    std::vector<ParticleEmitter> emitters;
    std::vector<ParticleInitializerUniquePtr> initializers;
    std::vector<ParticleOperatorUniquePtr> operators;
    std::vector<ParticleRenderer> renderers;
    std::vector<ParticleControlPoint> controlPoints;
    std::vector<ParticleChild> children;

    /** Instance override */
    ParticleInstanceOverride instanceOverride;
};

class Particle : public Object, public ParticleData {
public:
    explicit Particle (ObjectData data, ParticleData particleData) noexcept :
	Object (std::move (data)), ParticleData (std::move (particleData)) { };
    ~Particle () override = default;
};

/**
 * Text object data. Phase 1 of text support covers only static text;
 * dynamic (script-driven) text captures the script source for a future
 * pass but renders whatever initial value the scene provides.
 */
struct TextData {
    /** Initial text content to render (for scripted text, this is the `value` placeholder) */
    UserSettingUniquePtr text;
    /** Font reference from scene (e.g. "fonts/VCR_OSD_MONO.ttf" or "systemfont_arial") */
    std::string font;
    UserSettingUniquePtr msdf;
    UserSettingUniquePtr outline;
    UserSettingUniquePtr outlineWidth;
    UserSettingUniquePtr outlineColor;
    UserSettingUniquePtr blur;
    UserSettingUniquePtr blurRadius;
    UserSettingUniquePtr dropShadow;
    UserSettingUniquePtr dropShadowRadius;
    UserSettingUniquePtr dropShadowOpacity;
    UserSettingUniquePtr dropShadowOffset;
    UserSettingUniquePtr dropShadowColor;
    /** Font size in points, optionally bound to a user setting or script */
    UserSettingUniquePtr pointSize;
    /** Serialized/editor layer size; native glyph placement is driven by font metrics */
    glm::vec2 size;
    /** Scale (x, y, z) */
    UserSettingUniquePtr scale;
    /** Text color as linear-space RGB */
    UserSettingUniquePtr color;
    /** Alpha multiplier */
    UserSettingUniquePtr alpha;
    /** Parallax depth used for parallax scrolling */
    UserSettingUniquePtr parallaxDepth;
    /** Whether the text is visible */
    UserSettingUniquePtr visible;
    /** Use the scene virtual perspective camera for this text. */
    UserSettingUniquePtr perspective;
    /** Native string enum: enabled by default, ignored in orthographic scenes. */
    UserSettingUniquePtr depthTest;
    /** Horizontal alignment: "left", "center", "right" */
    std::string alignment;
    /** Vertical alignment: "top", "center", "bottom" */
    std::string verticalalign;
    /** Padding around the generated text surface, in pixels. Older scenes author a single
     *  number (uniform padding); newer editors write an "x y" vector. */
    UserSettingUniquePtr padding;
    UserSettingUniquePtr opaqueBackground;
    UserSettingUniquePtr backgroundColor;
    UserSettingUniquePtr backgroundBrightness;
    /** Additional glyph advance (X) and row height (Y), in font raster pixels. */
    UserSettingUniquePtr spacing;
    /** Layer brightness multiplier, applied to the text color at draw time */
    UserSettingUniquePtr brightness;
    /** Effects applied over the rendered text, same pipeline as image effects */
    std::vector<ImageEffectUniquePtr> effects;
    /** Blend the finished glyph/effect surface with the scene behind it. */
    UserSettingUniquePtr colorBlendMode;
    /** Native ledsource retains a padded text composite even without effects. */
    UserSettingUniquePtr ledSource;
    UserSettingUniquePtr limitWidth;
    UserSettingUniquePtr maxWidth;
    UserSettingUniquePtr limitRows;
    UserSettingUniquePtr maxRows;
    UserSettingUniquePtr limitUseEllipsis;
};

class Text : public Object, public TextData {
public:
    explicit Text (ObjectData data, TextData textData) noexcept :
	Object (std::move (data)), TextData (std::move (textData)) { };
    ~Text () override = default;
};

/**
 * Static 3D model object ("model" key in 3D scenes). Transform fields
 * (origin/scale/angles/visible) live in the base ObjectData group fields.
 */
struct Model3DData {
    /** Scene-local token for SceneScript geometry; zero denotes a file-backed model. */
    uint32_t modelDataToken = 0;
    /** The .mdl file the object references */
    std::string filename;
    /** Mesh data read from the MDLV container */
    MdlMesh mesh;
    /** Embedded MDLS skeleton, MDAT attachments, and MDLA animation clips. */
    MdlAnimationData animationData;
    /** Authored animation-layer controls for the embedded clips. */
    std::vector<ImageAnimationLayerUniquePtr> animationLayers;
    /** Materials referenced by the submesh headers, aligned with mesh.submeshes */
    std::vector<MaterialUniquePtr> materials;
    /** Whether this model participates in light-space shadow rendering. */
    UserSettingUniquePtr castShadow;
    /** Use the scene's virtual perspective camera on an orthographic canvas. */
    UserSettingUniquePtr perspective;
};

class Model3D : public Object, public Model3DData {
public:
    explicit Model3D (ObjectData data, Model3DData modelData) noexcept :
	Object (std::move (data)), Model3DData (std::move (modelData)) { };
    ~Model3D () override = default;
};

/**
 * Light object in 3D scenes. Origin/angles come from the base ObjectData
 * (both can be script-driven, e.g. a sun tracking script).
 */
struct LightData {
    enum Type {
	Type_Point = 0,
	Type_Spot = 1,
	Type_Tube = 2,
	Type_Directional = 3,
    };

    Type type;
    UserSettingUniquePtr color;
    UserSettingUniquePtr intensity;
    UserSettingUniquePtr radius;
    /** Point-shadow source exclusion distance; independent of attenuation radius. */
    UserSettingUniquePtr sourceSize;
    /** Falloff exponent for local (point, spot, and tube) lights */
    UserSettingUniquePtr exponent;
    /** Spot cone angles in degrees, packed as cosines for the shader */
    UserSettingUniquePtr innerCone;
    UserSettingUniquePtr outerCone;
    bool useCookie;
    std::string cookie;
    /** Tube light's second endpoint in the light's local coordinate system */
    UserSettingUniquePtr controlPoint;
    /** Far distances of the three nested directional-light shadow cascades. */
    glm::vec3 cascadeDistances;
    /** Whether this light renders and samples a scene shadow map. */
    bool castShadow;
    /** Authored participation and falloff in the point/spot light-volume pass. */
    UserSettingUniquePtr castVolumetrics;
    UserSettingUniquePtr density;
    UserSettingUniquePtr volumetricsExponent;
};

class Light : public Object, public LightData {
public:
    explicit Light (ObjectData data, LightData lightData) noexcept :
	Object (std::move (data)), LightData (std::move (lightData)) { };
    ~Light () override = default;
};
} // namespace WallpaperEngine::Data::Model
