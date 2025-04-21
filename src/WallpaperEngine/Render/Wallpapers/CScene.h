#pragma once

#include "WallpaperEngine/Render/CCamera.h"

#include "WallpaperEngine/Core/Wallpapers/CScene.h"

#include "WallpaperEngine/Render/CWallpaper.h"

namespace WallpaperEngine::Render {
class CCamera;
class CObject;
}

namespace WallpaperEngine::Render::Wallpapers {
class CScene final : public CWallpaper {
  public:
    CScene (
        const Core::Wallpapers::CScene* scene, CRenderContext& context, CAudioContext& audioContext,
        const CWallpaperState::TextureUVsScaling& scalingMode,
        const WallpaperEngine::Assets::ITexture::TextureFlags& clampMode);

    CCamera* getCamera () const;

    const Core::Wallpapers::CScene* getScene () const;

    int getWidth () const override;
    int getHeight () const override;

    glm::vec2* getMousePosition ();
    glm::vec2* getMousePositionLast ();
    glm::vec2* getParallaxDisplacement ();

    const std::vector<CObject*>& getObjectsByRenderOrder () const;

  protected:
    void renderFrame (glm::ivec4 viewport) override;
    void updateMouse (glm::ivec4 viewport);

    friend class CWallpaper;

    static const std::string Type;

  private:
    Render::CObject* createObject (const Core::CObject* object);

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
} // namespace WallpaperEngine::Render::Wallpaper
