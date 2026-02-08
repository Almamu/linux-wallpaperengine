#pragma once

#include <string>
#include <utility>
#include <vector>

#include <glm/glm.hpp>

#include "Effect.h"
#include "Material.h"
#include "Model.h"
#include "Types.h"
#include "UserSetting.h"
#include "WallpaperEngine/Data/Utils/TypeCaster.h"
#include <memory>

namespace WallpaperEngine::Data::Model {
using namespace WallpaperEngine::Data::Utils;

struct ObjectData {
    int id;
    std::string name;
    std::vector<int> dependencies;
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
    float rate;
    UserSettingUniquePtr visible;
    float blend;
    int animation;
};

struct ImageData {
    /** The point of origin of the image */
    UserSettingUniquePtr origin;
    /** The scale of the image */
    UserSettingUniquePtr scale;
    /** The rotation of the image */
    UserSettingUniquePtr angles;
    /** If the image is visible or not */
    UserSettingUniquePtr visible;
    /** The alpha of the image */
    UserSettingUniquePtr alpha;
    /** The color of the image */
    UserSettingUniquePtr color;
    // TODO: WRITE A COUPLE OF ENUMS FOR THIS
    /** The alignment of the image */
    std::string alignment;
    /** The size of the image in pixels */
    glm::vec2 size;
    /** Parallax depth used for parallax scrolling */
    glm::vec2 parallaxDepth;
    /** The color blending mode for this image */
    int colorBlendMode;
    /** The brightness of the image */
    float brightness;
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
struct ParticleControlPoint {
    int id;
    uint32_t flags;
    glm::vec3 offset;
    bool lockToPointer;
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
    int audioProcessingExponent;
    int audioProcessingFrequencyStart;
    int audioProcessingFrequencyEnd;
    int audioProcessingMode;
    float minPeriodicDelay;
    float maxPeriodicDelay;
    float minPeriodicDuration;
    float maxPeriodicDuration;
};

/**
 * Particle initializer base and implementations
 */
class ParticleInitializerBase : public TypeCaster {
public:
    virtual ~ParticleInitializerBase () = default;
};

class ColorRandomInitializer : public ParticleInitializerBase {
public:
    ColorRandomInitializer (UserSettingUniquePtr min, UserSettingUniquePtr max) :
	min (std::move (min)), max (std::move (max)) { }
    UserSettingUniquePtr min;
    UserSettingUniquePtr max;
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
    LifetimeRandomInitializer (UserSettingUniquePtr min, UserSettingUniquePtr max) :
	min (std::move (min)), max (std::move (max)) { }
    UserSettingUniquePtr min;
    UserSettingUniquePtr max;
};

class VelocityRandomInitializer : public ParticleInitializerBase {
public:
    VelocityRandomInitializer (UserSettingUniquePtr min, UserSettingUniquePtr max) :
	min (std::move (min)), max (std::move (max)) { }
    UserSettingUniquePtr min;
    UserSettingUniquePtr max;
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

class MapSequenceAroundControlPointInitializer : public ParticleInitializerBase {
public:
    MapSequenceAroundControlPointInitializer (
	UserSettingUniquePtr controlPoint, UserSettingUniquePtr count, UserSettingUniquePtr speedMin,
	UserSettingUniquePtr speedMax
    ) :
	controlPoint (std::move (controlPoint)), count (std::move (count)), speedMin (std::move (speedMin)),
	speedMax (std::move (speedMax)) { }
    UserSettingUniquePtr controlPoint;
    UserSettingUniquePtr count;
    UserSettingUniquePtr speedMin;
    UserSettingUniquePtr speedMax;
};

using ParticleInitializerUniquePtr = std::unique_ptr<ParticleInitializerBase>;

/**
 * Particle operator base and implementations
 */
class ParticleOperatorBase : public TypeCaster {
public:
    virtual ~ParticleOperatorBase () = default;
};

class MovementOperator : public ParticleOperatorBase {
public:
    MovementOperator (UserSettingUniquePtr drag, UserSettingUniquePtr gravity) :
	drag (std::move (drag)), gravity (std::move (gravity)) { }
    UserSettingUniquePtr drag;
    UserSettingUniquePtr gravity;
};

class AngularMovementOperator : public ParticleOperatorBase {
public:
    AngularMovementOperator (UserSettingUniquePtr drag, UserSettingUniquePtr force) :
	drag (std::move (drag)), force (std::move (force)) { }
    UserSettingUniquePtr drag;
    UserSettingUniquePtr force;
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
	UserSettingUniquePtr audioProcessingFrequencyStart, UserSettingUniquePtr audioProcessingFrequencyEnd
    ) :
	scale (std::move (scale)), speedMin (std::move (speedMin)), speedMax (std::move (speedMax)),
	timeScale (std::move (timeScale)), mask (std::move (mask)), phaseMin (std::move (phaseMin)),
	phaseMax (std::move (phaseMax)), audioProcessingMode (std::move (audioProcessingMode)),
	audioProcessingBounds (std::move (audioProcessingBounds)),
	audioProcessingExponent (std::move (audioProcessingExponent)),
	audioProcessingFrequencyStart (std::move (audioProcessingFrequencyStart)),
	audioProcessingFrequencyEnd (std::move (audioProcessingFrequencyEnd)) { }
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
};

class VortexOperator : public ParticleOperatorBase {
public:
    VortexOperator (
	int controlPoint, int flags, UserSettingUniquePtr axis, UserSettingUniquePtr offset,
	UserSettingUniquePtr distanceInner, UserSettingUniquePtr distanceOuter, UserSettingUniquePtr speedInner,
	UserSettingUniquePtr speedOuter, UserSettingUniquePtr centerForce, UserSettingUniquePtr ringRadius,
	UserSettingUniquePtr ringWidth, UserSettingUniquePtr ringPullDistance, UserSettingUniquePtr ringPullForce,
	UserSettingUniquePtr audioProcessingMode, UserSettingUniquePtr audioProcessingBounds
    ) :
	controlPoint (controlPoint), flags (flags), axis (std::move (axis)), offset (std::move (offset)),
	distanceInner (std::move (distanceInner)), distanceOuter (std::move (distanceOuter)),
	speedInner (std::move (speedInner)), speedOuter (std::move (speedOuter)), centerForce (std::move (centerForce)),
	ringRadius (std::move (ringRadius)), ringWidth (std::move (ringWidth)),
	ringPullDistance (std::move (ringPullDistance)), ringPullForce (std::move (ringPullForce)),
	audioProcessingMode (std::move (audioProcessingMode)),
	audioProcessingBounds (std::move (audioProcessingBounds)) { }
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
};

class ControlPointAttractOperator : public ParticleOperatorBase {
public:
    ControlPointAttractOperator (
	int controlPoint, UserSettingUniquePtr origin, UserSettingUniquePtr scale, UserSettingUniquePtr threshold
    ) :
	controlPoint (controlPoint), origin (std::move (origin)), scale (std::move (scale)),
	threshold (std::move (threshold)) { }
    int controlPoint;
    UserSettingUniquePtr origin;
    UserSettingUniquePtr scale;
    UserSettingUniquePtr threshold;
};

class OscillateAlphaOperator : public ParticleOperatorBase {
public:
    OscillateAlphaOperator (
	UserSettingUniquePtr frequencyMin, UserSettingUniquePtr frequencyMax, UserSettingUniquePtr scaleMin,
	UserSettingUniquePtr scaleMax, UserSettingUniquePtr phaseMin, UserSettingUniquePtr phaseMax
    ) :
	frequencyMin (std::move (frequencyMin)), frequencyMax (std::move (frequencyMax)),
	scaleMin (std::move (scaleMin)), scaleMax (std::move (scaleMax)), phaseMin (std::move (phaseMin)),
	phaseMax (std::move (phaseMax)) { }
    UserSettingUniquePtr frequencyMin;
    UserSettingUniquePtr frequencyMax;
    UserSettingUniquePtr scaleMin;
    UserSettingUniquePtr scaleMax;
    UserSettingUniquePtr phaseMin;
    UserSettingUniquePtr phaseMax;
};

class OscillateSizeOperator : public ParticleOperatorBase {
public:
    OscillateSizeOperator (
	UserSettingUniquePtr frequencyMin, UserSettingUniquePtr frequencyMax, UserSettingUniquePtr scaleMin,
	UserSettingUniquePtr scaleMax, UserSettingUniquePtr phaseMin, UserSettingUniquePtr phaseMax
    ) :
	frequencyMin (std::move (frequencyMin)), frequencyMax (std::move (frequencyMax)),
	scaleMin (std::move (scaleMin)), scaleMax (std::move (scaleMax)), phaseMin (std::move (phaseMin)),
	phaseMax (std::move (phaseMax)) { }
    UserSettingUniquePtr frequencyMin;
    UserSettingUniquePtr frequencyMax;
    UserSettingUniquePtr scaleMin;
    UserSettingUniquePtr scaleMax;
    UserSettingUniquePtr phaseMin;
    UserSettingUniquePtr phaseMax;
};

class OscillatePositionOperator : public ParticleOperatorBase {
public:
    OscillatePositionOperator (
	UserSettingUniquePtr frequencyMin, UserSettingUniquePtr frequencyMax, UserSettingUniquePtr scaleMin,
	UserSettingUniquePtr scaleMax, UserSettingUniquePtr phaseMin, UserSettingUniquePtr phaseMax,
	UserSettingUniquePtr mask
    ) :
	frequencyMin (std::move (frequencyMin)), frequencyMax (std::move (frequencyMax)),
	scaleMin (std::move (scaleMin)), scaleMax (std::move (scaleMax)), phaseMin (std::move (phaseMin)),
	phaseMax (std::move (phaseMax)), mask (std::move (mask)) { }
    UserSettingUniquePtr frequencyMin;
    UserSettingUniquePtr frequencyMax;
    UserSettingUniquePtr scaleMin;
    UserSettingUniquePtr scaleMax;
    UserSettingUniquePtr phaseMin;
    UserSettingUniquePtr phaseMax;
    UserSettingUniquePtr mask;
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
    UserSettingUniquePtr size;
    UserSettingUniquePtr lifetime;
    UserSettingUniquePtr rate;
    UserSettingUniquePtr speed;
    UserSettingUniquePtr count;
    UserSettingUniquePtr color; // Replaces particle color
    UserSettingUniquePtr colorn; // Multiplies particle color
};

struct ParticleData {
    /** Position and transformation */
    UserSettingUniquePtr origin;
    UserSettingUniquePtr scale;
    UserSettingUniquePtr angles;
    UserSettingUniquePtr visible;

    /** Parallax depth */
    glm::vec2 parallaxDepth;

    /** Reference to particle definition file */
    std::string particleFile;

    /** Particle system configuration */
    std::string animationMode;
    float sequenceMultiplier;
    uint32_t maxCount;
    uint32_t startTime;
    uint32_t flags;

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
} // namespace WallpaperEngine::Data::Model