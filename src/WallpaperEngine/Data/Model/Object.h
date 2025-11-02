#pragma once

#include <utility>
#include <vector>
#include <string>

#include <glm/glm.hpp>

#include "Types.h"
#include "UserSetting.h"
#include "Material.h"
#include "Model.h"
#include "Effect.h"
#include "WallpaperEngine/Data/Utils/TypeCaster.h"
#include <memory>

namespace WallpaperEngine::Data::Model {
using namespace WallpaperEngine::Data::Utils;

struct ObjectData {
    int id;
    std::string name;
    std::vector <int> dependencies;
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
    explicit Object (ObjectData data) noexcept : TypeCaster (), ObjectData (std::move (data)) {};
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
    std::vector <ImageEffectPassOverrideUniquePtr> passOverrides;
    /** The effect definition */
    EffectUniquePtr effect;
};

/**
 * Animation layers for the puppet warp
 */
struct ImageAnimationLayer {
    int id;
    float rate;
    bool visible;
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
    std::vector <ImageEffectUniquePtr> effects;
    /** The animation layers used in the puppet warp */
    std::vector <ImageAnimationLayerUniquePtr> animationLayers;
};

class Image : public Object, public ImageData {
  public:
    explicit Image (ObjectData data, ImageData imageData) noexcept : Object (std::move(data)), ImageData (std::move (imageData)) {};
    ~Image () override = default;
};

struct SoundData {
    /** Playback mode, loop, */
    // TODO: WRITE AN ENUM FOR THIS
    std::optional <std::string> playbackmode;
    std::vector <std::string> sounds;
};

class Sound : public Object, public SoundData {
  public:
    explicit Sound (ObjectData data, SoundData soundData) noexcept : Object (std::move(data)), SoundData (std::move (soundData)) {};
    ~Sound () override = default;
};

/**
 * Particle control points for forces and positions
 */
struct ParticleControlPoint {
    int id;
    uint32_t flags;
    glm::vec3 offset;
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
    ColorRandomInitializer (glm::vec3 min, glm::vec3 max) : min (min), max (max) {}
    glm::vec3 min;
    glm::vec3 max;
};

class SizeRandomInitializer : public ParticleInitializerBase {
  public:
    SizeRandomInitializer (float min, float max) : min (min), max (max) {}
    float min;
    float max;
};

class AlphaRandomInitializer : public ParticleInitializerBase {
  public:
    AlphaRandomInitializer (float min, float max) : min (min), max (max) {}
    float min;
    float max;
};

class LifetimeRandomInitializer : public ParticleInitializerBase {
  public:
    LifetimeRandomInitializer (float min, float max) : min (min), max (max) {}
    float min;
    float max;
};

class VelocityRandomInitializer : public ParticleInitializerBase {
  public:
    VelocityRandomInitializer (glm::vec3 min, glm::vec3 max) : min (min), max (max) {}
    glm::vec3 min;
    glm::vec3 max;
};

class RotationRandomInitializer : public ParticleInitializerBase {
  public:
    RotationRandomInitializer (glm::vec3 min, glm::vec3 max) : min (min), max (max) {}
    glm::vec3 min;
    glm::vec3 max;
};

class AngularVelocityRandomInitializer : public ParticleInitializerBase {
  public:
    AngularVelocityRandomInitializer (glm::vec3 min, glm::vec3 max) : min (min), max (max) {}
    glm::vec3 min;
    glm::vec3 max;
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
    MovementOperator (float drag, glm::vec3 gravity) : drag (drag), gravity (gravity) {}
    float drag;
    glm::vec3 gravity;
};

class AngularMovementOperator : public ParticleOperatorBase {
  public:
    AngularMovementOperator (float drag, glm::vec3 force) : drag (drag), force (force) {}
    float drag;
    glm::vec3 force;
};

class AlphaFadeOperator : public ParticleOperatorBase {
  public:
    AlphaFadeOperator (float fadeInTime, float fadeOutTime) : fadeInTime (fadeInTime), fadeOutTime (fadeOutTime) {}
    float fadeInTime;
    float fadeOutTime;
};

class SizeChangeOperator : public ParticleOperatorBase {
  public:
    SizeChangeOperator (float startTime, float endTime, float startValue, float endValue)
        : startTime (startTime), endTime (endTime), startValue (startValue), endValue (endValue) {}
    float startTime;
    float endTime;
    float startValue;
    float endValue;
};

class AlphaChangeOperator : public ParticleOperatorBase {
  public:
    AlphaChangeOperator (float startTime, float endTime, float startValue, float endValue)
        : startTime (startTime), endTime (endTime), startValue (startValue), endValue (endValue) {}
    float startTime;
    float endTime;
    float startValue;
    float endValue;
};

class ColorChangeOperator : public ParticleOperatorBase {
  public:
    ColorChangeOperator (float startTime, float endTime, glm::vec3 startValue, glm::vec3 endValue)
        : startTime (startTime), endTime (endTime), startValue (startValue), endValue (endValue) {}
    float startTime;
    float endTime;
    glm::vec3 startValue;
    glm::vec3 endValue;
};

using ParticleOperatorUniquePtr = std::unique_ptr<ParticleOperatorBase>;

/**
 * Particle renderer configuration
 */
struct ParticleRenderer {
    std::string name;
    float length;
    float maxLength;
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
    bool enabled;
    float alpha;
    float size;
    float lifetime;
    float rate;
    float speed;
    float count;
    glm::vec3 color;
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
    explicit Particle (ObjectData data, ParticleData particleData) noexcept : Object (std::move(data)), ParticleData (std::move (particleData)) {};
    ~Particle () override = default;
};
} // namespace WallpaperEngine::Data::Model