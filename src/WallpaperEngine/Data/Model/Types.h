#pragma once

#include <map>
#include <vector>
#include <string>
#include <optional>
#include <memory>

#include "WallpaperEngine/Assets/CContainer.h"

namespace WallpaperEngine::Assets {
class CContainer;
}


namespace WallpaperEngine::Data::Model {
struct Project;
class Wallpaper;
class Scene;
class Web;
class Video;
struct UserSetting;
class Object;
class Sound;
class Image;
struct ImageEffect;
struct ImageEffectPassOverride;
class Particle;
class DynamicValue;
class Property;
class PropertySlider;
class PropertyBoolean;
class PropertyCombo;
class PropertyText;
class PropertyColor;
struct Material;
struct MaterialPass;
struct FBO;
struct Effect;
struct EffectPass;
struct ModelStruct;

// TODO: REMOVE ONCE THESE ARE RENAMED AND MOVED
using Container = WallpaperEngine::Assets::CContainer;

using PropertySharedPtr = std::shared_ptr <Property>;
using Properties = std::map <std::string, PropertySharedPtr>;
using DynamicValueUniquePtr = std::unique_ptr <DynamicValue>;
using UserSettingUniquePtr = std::unique_ptr <UserSetting>;
// TODO: UP TO THIS POINT

using ShaderConstantMap = std::map <std::string, UserSettingUniquePtr>;

using ProjectUniquePtr = std::unique_ptr <Project>;
using WallpaperUniquePtr = std::unique_ptr <Wallpaper>;
using ContainerUniquePtr = std::unique_ptr <Container>;
using SceneUniquePtr = std::unique_ptr <Scene>;
using WebUniquePtr = std::unique_ptr <Web>;
using VideoUniquePtr = std::unique_ptr <Video>;
using ObjectUniquePtr = std::unique_ptr <Object>;
using SoundUniquePtr = std::unique_ptr <Sound>;
using ImageUniquePtr = std::unique_ptr <Image>;
using ParticleUniquePtr = std::unique_ptr <Particle>;
using MaterialUniquePtr = std::unique_ptr <Material>;
using MaterialPassUniquePtr = std::unique_ptr <MaterialPass>;
using EffectUniquePtr = std::unique_ptr <Effect>;
using ImageEffectUniquePtr = std::unique_ptr <ImageEffect>;
using ImageEffectPassOverrideUniquePtr = std::unique_ptr <ImageEffectPassOverride>;
using EffectPassUniquePtr = std::unique_ptr <EffectPass>;
using FBOUniquePtr = std::unique_ptr <FBO>;
using ModelUniquePtr = std::unique_ptr <ModelStruct>;

using ObjectList = std::vector<ObjectUniquePtr>;
using ComboMap = std::map<std::string, int>;
using TextureMap = std::map<int, std::string>;
}