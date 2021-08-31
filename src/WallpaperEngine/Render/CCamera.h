#pragma once

#include <glm/vec3.hpp>
#include <glm/vec2.hpp>
#include <glm/mat4x4.hpp>
#include "CScene.h"

#include "WallpaperEngine/Core/Scenes/CCamera.h"

using namespace WallpaperEngine::Core::Types;

namespace WallpaperEngine::Render
{
    class CScene;

    class CCamera
    {
    public:
        CCamera (CScene* scene, const Core::Scenes::CCamera* camera);
        ~CCamera ();

        void setOrthogonalProjection (float width, float height);

        const glm::vec3& getCenter () const;
        const glm::vec3& getEye () const;
        const glm::vec3& getUp () const;
        const glm::mat4& getProjection () const;
        const glm::mat4& getLookAt () const;
        const bool isOrthogonal () const;

    private:
        bool m_isOrthogonal;
        glm::mat4 m_projection;
        glm::mat4 m_lookat;
        const Core::Scenes::CCamera* m_camera;
        CScene* m_scene;
    };
}
