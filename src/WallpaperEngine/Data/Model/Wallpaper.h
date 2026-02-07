#pragma once

#include <memory>

#include <glm/glm.hpp>
#include <utility>

#include "Types.h"
#include "Object.h"
#include "WallpaperEngine/Data/Utils/TypeCaster.h"

namespace WallpaperEngine::Data::Model {
using namespace WallpaperEngine::Data::Utils;

struct WallpaperData {
    std::string filename;
    Project& project;
};

class Wallpaper : public TypeCaster, public WallpaperData {
  public:
    explicit Wallpaper (WallpaperData data) noexcept : TypeCaster (), WallpaperData (std::move(data)) {};
    ~Wallpaper () override = default;
};

class Video final : public Wallpaper {
  public:
    explicit Video (WallpaperData data) noexcept : Wallpaper (std::move(data)) {}

    ~Video () override = default;
};


class Web final : public Wallpaper {
  public:
    explicit Web (WallpaperData data) noexcept : Wallpaper (std::move(data)) {}

    ~Web () override = default;
};

struct SceneData {
    struct {
        glm::vec3 ambient;
        glm::vec3 skylight;
        UserSettingUniquePtr clear;
    } colors;
    /**
     * Camera configuration
     */
    struct Camera {
        /** Enable fade effect */
        bool fade;
        /** Used by the software to allow the users to preview the background or not? */
        bool preview;

        /**
         * Bloom effect configuration
         */
        struct {
            /** If bloom is enabled or not */
            UserSettingUniquePtr enabled;
            /** Bloom's strength to pass onto the shader */
            UserSettingUniquePtr strength;
            /** Bloom's threshold to pass onto the shader */
            UserSettingUniquePtr threshold;
        } bloom;
        /**
         * Parallax effect configuration
         */
        struct {
            UserSettingUniquePtr enabled;
            UserSettingUniquePtr amount;
            UserSettingUniquePtr delay;
            UserSettingUniquePtr mouseInfluence;
        } parallax;

        /**
         * Shake effect configuration
         */
        struct {
            UserSettingUniquePtr enabled;
            UserSettingUniquePtr amplitude;
            UserSettingUniquePtr roughness;
            UserSettingUniquePtr speed;
        } shake;

        /**
         * Position configuration
         */
        struct {
            glm::vec3 center;
            glm::vec3 eye;
            glm::vec3 up;
        } configuration;

        /**
         * Projection information
         */
        struct {
            int width;
            int height;
            bool isAuto;
            float nearz;
            float farz;
            float fov;
        } projection;
    } camera;

    ObjectList objects;
};

class Scene final : public Wallpaper, public SceneData {
  public:
    explicit Scene (WallpaperData data, SceneData sceneData) noexcept : Wallpaper (std::move (data)), SceneData (std::move (sceneData)) {}

    ~Scene () override = default;
};
} // namespace WallpaperEngine::Data::Model
