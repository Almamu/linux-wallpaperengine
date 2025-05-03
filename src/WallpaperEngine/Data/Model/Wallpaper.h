#pragma once

#include <memory>

#include <glm/glm.hpp>
#include <utility>

#include "Types.h"
#include "Object.h"
#include "WallpaperEngine/Data/Utils/TypeCaster.h"
#include "WallpaperEngine/Data/JSON.h"

namespace WallpaperEngine::Data::Model {
using json = WallpaperEngine::Data::JSON::JSON;
using TypeCaster = WallpaperEngine::Data::Utils::TypeCaster;

struct WallpaperData {
    std::string filename;
    ProjectWeakPtr project;
};

class Wallpaper : public TypeCaster, public WallpaperData {
  public:
    explicit Wallpaper (WallpaperData data) noexcept : WallpaperData (std::move(data)), TypeCaster () {};
    ~Wallpaper () override = default;
};

class Video : public Wallpaper {
  public:
    explicit Video (WallpaperData data) noexcept : Wallpaper (std::move(data)) {}

    ~Video () override = default;
};


class Web : public Wallpaper {
  public:
    explicit Web (WallpaperData data) noexcept : Wallpaper (std::move(data)) {}

    ~Web () override = default;
};

struct SceneData {
    struct {
        glm::vec3 ambient;
        glm::vec3 skylight;
        UserSettingSharedPtr clear;
    } colors;
    /**
     * Camera configuration
     */
    struct {
        /** Enable fade effect */
        bool fade;
        /** Used by the software to allow the users to preview the background or not? */
        bool preview;

        /**
         * Bloom effect configuration
         */
        struct {
            /** If bloom is enabled or not */
            UserSettingSharedPtr enabled;
            /** Bloom's strength to pass onto the shader */
            UserSettingSharedPtr strength;
            /** Bloom's threshold to pass onto the shader */
            UserSettingSharedPtr threshold;
        } bloom;
        /**
         * Parallax effect configuration
         */
        struct {
            bool enabled;
            float amount;
            float delay;
            float mouseInfluence;
        } parallax;

        /**
         * Shake effect configuration
         */
        struct {
            bool enabled;
            float amplitude;
            float roughness;
            float speed;
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
        } projection;
    } camera;
    ObjectMap objects;
    ProjectWeakPtr project;
};

class Scene : public Wallpaper, public SceneData {
  public:
    explicit Scene (WallpaperData data, SceneData sceneData) noexcept : SceneData (std::move(sceneData)), Wallpaper (data) {}

    ~Scene () override = default;

    // TODO: ADD OBJECTS HERE
};
} // namespace WallpaperEngine::Data::Model
