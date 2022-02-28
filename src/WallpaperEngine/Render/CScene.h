#pragma once

#include "CCamera.h"

#include "WallpaperEngine/Core/CScene.h"

#include "WallpaperEngine/Render/CWallpaper.h"
#include "WallpaperEngine/Render/CObject.h"

namespace WallpaperEngine::Render
{
    class CCamera;
    class CObject;

    class CScene : public CWallpaper
    {
    public:
        CScene (Core::CScene* scene, CContainer* container, CContext* context);

        CCamera* getCamera () const;

        Core::CScene* getScene ();

        glm::vec2* getMousePosition ();

    protected:
        void renderFrame (glm::ivec4 viewport) override;
        void updateMouse (glm::ivec4 viewport);

        friend class CWallpaper;

        static const std::string Type;

    private:
        CCamera* m_camera;
        std::vector<CObject*> m_objects;
        glm::vec2 m_mousePosition;
    };
}
