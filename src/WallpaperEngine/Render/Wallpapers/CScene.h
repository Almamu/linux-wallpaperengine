#pragma once

#include "WallpaperEngine/Render/CCamera.h"

#include "WallpaperEngine/Core/Wallpapers/CScene.h"

#include "WallpaperEngine/Render/CWallpaper.h"

namespace WallpaperEngine::Render {
class CCamera;
class CObject;

class CScene final : public CWallpaper {
  public:
    CScene (Core::CScene* scene, CRenderContext& context, CAudioContext& audioContext,
            const CWallpaperState::TextureUVsScaling& scalingMode);

    CCamera* getCamera () const;

    Core::CScene* getScene () const;

    int getWidth () const override;
    int getHeight () const override;

    glm::vec2* getMousePosition ();
    glm::vec2* getMousePositionLast ();
    glm::vec2* getParallaxDisplacement ();

  protected:
    void renderFrame (glm::ivec4 viewport) override;
    void updateMouse (glm::ivec4 viewport);

    friend class CWallpaper;

    static const std::string Type;

  private:
    Render::CObject* createObject (Core::CObject* object);

    CCamera* m_camera;
    CObject* m_bloomObject;
    std::map<int, CObject*> m_objects;
    std::vector<CObject*> m_objectsByRenderOrder;
    glm::vec2 m_mousePosition;
    glm::vec2 m_mousePositionLast;
    glm::vec2 m_parallaxDisplacement;
    CFBO* _rt_4FrameBuffer;
    CFBO* _rt_8FrameBuffer;
    CFBO* _rt_Bloom;
};
} // namespace WallpaperEngine::Render
