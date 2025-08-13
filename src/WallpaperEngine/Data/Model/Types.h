#pragma once

#include <memory>

#include "WallpaperEngine/Assets/CContainer.h"
#include "WallpaperEngine/Core/DynamicValues/CDynamicValue.h"
#include "WallpaperEngine/Core/Objects/Effects/Constants/CShaderConstant.h"
#include "WallpaperEngine/Core/Objects/Effects/Constants/CShaderConstantProperty.h"
#include "WallpaperEngine/Core/Objects/Effects/Constants/CShaderConstantFloat.h"
#include "WallpaperEngine/Core/Objects/Effects/Constants/CShaderConstantInteger.h"
#include "WallpaperEngine/Core/Objects/Effects/Constants/CShaderConstantVector2.h"
#include "WallpaperEngine/Core/Objects/Effects/Constants/CShaderConstantVector3.h"
#include "WallpaperEngine/Core/Objects/Effects/Constants/CShaderConstantVector4.h"
#include "WallpaperEngine/Core/Projects/CProperty.h"

namespace WallpaperEngine::Data::Model {
struct Project;
class Wallpaper;
class Scene;
class Web;
class Video;
class UserSetting;
class Object;
class Sound;
class Image;
struct ImageEffect;
struct ImageEffectPassOverride;
class Particle;
struct Material;
struct MaterialPass;
struct FBO;
struct Effect;
struct EffectPass;
struct ModelStruct;

// TODO: REMOVE ONCE THESE ARE RENAMED AND MOVED
using Container = WallpaperEngine::Assets::CContainer;
using ShaderConstant = WallpaperEngine::Core::Objects::Effects::Constants::CShaderConstant;
using ShaderConstantProperty = WallpaperEngine::Core::Objects::Effects::Constants::CShaderConstantProperty;
using ShaderConstantFloat = WallpaperEngine::Core::Objects::Effects::Constants::CShaderConstantFloat;
using ShaderConstantInteger = WallpaperEngine::Core::Objects::Effects::Constants::CShaderConstantInteger;
using ShaderConstantVector2 = WallpaperEngine::Core::Objects::Effects::Constants::CShaderConstantVector2;
using ShaderConstantVector3 = WallpaperEngine::Core::Objects::Effects::Constants::CShaderConstantVector3;
using ShaderConstantVector4 = WallpaperEngine::Core::Objects::Effects::Constants::CShaderConstantVector4;

using Property = WallpaperEngine::Core::Projects::CProperty;
using PropertySharedPtr = std::shared_ptr <Property>;
using PropertyWeakPtr = std::weak_ptr <Property>;
using Properties = std::map <std::string, PropertySharedPtr>;
using DynamicValue = WallpaperEngine::Core::DynamicValues::CDynamicValue;
using DynamicValueUniquePtr = std::unique_ptr <DynamicValue>;
using DynamicValueSharedPtr = std::shared_ptr <DynamicValue>;
using DynamicValueWeakPtr = std::weak_ptr <DynamicValue>;
using UserSettingSharedPtr = std::shared_ptr <UserSetting>;
using UserSettingWeakPtr = std::weak_ptr <UserSetting>;
using ShaderConstantUniquePtr = std::unique_ptr <ShaderConstant>;
// TODO: UP TO THIS POINT

using ShaderConstantMap = std::map <std::string, ShaderConstantUniquePtr>;

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