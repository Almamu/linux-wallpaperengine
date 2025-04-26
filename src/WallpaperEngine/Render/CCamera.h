#pragma once

#include "WallpaperEngine/Render/Wallpapers/CScene.h"
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "WallpaperEngine/Core/Scenes/CCamera.h"

namespace WallpaperEngine::Render::Wallpapers {
class CScene;
}

namespace WallpaperEngine::Render {
class CCamera {
  public:
    CCamera (Wallpapers::CScene* scene, const Core::Scenes::CCamera* camera);
    ~CCamera ();

    void setOrthogonalProjection (float width, float height);

    [[nodiscard]] const glm::vec3& getCenter () const;
    [[nodiscard]] const glm::vec3& getEye () const;
    [[nodiscard]] const glm::vec3& getUp () const;
    [[nodiscard]] const glm::mat4& getProjection () const;
    [[nodiscard]] const glm::mat4& getLookAt () const;
    [[nodiscard]] Wallpapers::CScene* getScene () const;
    [[nodiscard]] bool isOrthogonal () const;

  private:
    bool m_isOrthogonal = false;
    glm::mat4 m_projection = {};
    glm::mat4 m_lookat = {};
    const Core::Scenes::CCamera* m_camera;
    Wallpapers::CScene* m_scene = nullptr;
};
} // namespace WallpaperEngine::Render
