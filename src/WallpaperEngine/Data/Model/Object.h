#pragma once

#include <utility>
#include <vector>
#include <string>

#include <glm/glm.hpp>

#include "Types.h"
#include "Effect.h"
#include "Material.h"
#include "Model.h"
#include "WallpaperEngine/Data/Utils/TypeCaster.h"

namespace WallpaperEngine::Data::Model {
using TypeCaster = WallpaperEngine::Data::Utils::TypeCaster;

struct ObjectData {
    int id;
    std::string name;
    std::vector <int> dependencies;
};

//TODO: CHECK IF THE SEMANTICS FOR MEMORY ARE RIGHT HERE

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
    explicit Object (ObjectData data) noexcept : ObjectData (std::move (data)), TypeCaster () {};
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
    std::map <int, std::string> textures;
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
    UserSettingSharedPtr visible;
    /** Pass overrides to apply to the effect's passes */
    std::vector <ImageEffectPassOverrideUniquePtr> passOverrides;
    /** The effect definition */
    EffectUniquePtr effect;
};

struct ImageData {
    /** The point of origin of the image */
    UserSettingSharedPtr origin;
    /** The scale of the image */
    UserSettingSharedPtr scale;
    /** The rotation of the image */
    UserSettingSharedPtr angles;
    /** If the image is visible or not */
    UserSettingSharedPtr visible;
    /** The alpha of the image */
    UserSettingSharedPtr alpha;
    /** The color of the image */
    UserSettingSharedPtr color;
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
};

class Image : public Object, public ImageData {
  public:
    explicit Image (ObjectData data, ImageData imageData) noexcept : Object (std::move(data)), ImageData (std::move (imageData)) {};
    ~Image () override = default;
};

struct SoundData {
    /** Playback mode, loop, */
    std::optional <std::string> playbackmode;
    std::vector <std::string> sounds;
};

class Sound : public Object, public SoundData {
  public:
    explicit Sound (ObjectData data, SoundData soundData) noexcept : Object (std::move(data)), SoundData (std::move (soundData)) {};
    ~Sound () override = default;
};
} // namespace WallpaperEngine::Data::Model