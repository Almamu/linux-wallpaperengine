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
#include "WallpaperEngine/Data/JSON.h"

namespace WallpaperEngine::Data::Model {
using namespace WallpaperEngine::Data::Utils;
using JSON = WallpaperEngine::Data::JSON::JSON;

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
 * Particle initializer - sets initial particle properties
 */
struct ParticleInitializer {
    std::string name;
    JSON json; // Store full JSON for flexible parsing
};

/**
 * Particle operator - modifies particles over time
 */
struct ParticleOperator {
    std::string name;
    JSON json; // Store full JSON for flexible parsing
};

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
    std::vector<ParticleInitializer> initializers;
    std::vector<ParticleOperator> operators;
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