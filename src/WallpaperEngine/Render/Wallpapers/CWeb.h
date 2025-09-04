#pragma once

// Matrices manipulation for OpenGL
#include <glm/glm.hpp>
#include <glm/ext.hpp>

#include <string>
#include <utility>
#include <vector>
#include <memory>
#include <algorithm>

#include "WallpaperEngine/Audio/CAudioStream.h"
#include "WallpaperEngine/Render/CWallpaper.h"
#include "WallpaperEngine/WebBrowser/CEF/CBrowserClient.h"
#include "WallpaperEngine/WebBrowser/CEF/CRenderHandler.h"

#include "WallpaperEngine/Data/Model/Wallpaper.h"

namespace WallpaperEngine::WebBrowser::CEF {
class CRenderHandler;
}

namespace WallpaperEngine::Render::Wallpapers {
class CWeb : public CWallpaper
{
    public:
        CWeb (
          const Wallpaper& wallpaper, CRenderContext& context, CAudioContext& audioContext,
          WallpaperEngine::WebBrowser::CWebBrowserContext& browserContext,
          const CWallpaperState::TextureUVsScaling& scalingMode,
          const uint32_t& clampMode);
        ~CWeb() override;
        [[nodiscard]] int getWidth  () const override { return this->m_width; }

        [[nodiscard]] int getHeight () const override { return this->m_height; }

        void setSize (int width, int height);

    protected:
        void renderFrame (glm::ivec4 viewport) override;
        void updateMouse (glm::ivec4 viewport);
        const Web& getWeb () const {
            return *this->getWallpaperData ().as<Web> ();
        }

        friend class CWallpaper;

    private:
        WallpaperEngine::WebBrowser::CWebBrowserContext& m_browserContext;
        CefRefPtr<CefBrowser> m_browser = nullptr;
        CefRefPtr<WallpaperEngine::WebBrowser::CEF::CBrowserClient> m_client = nullptr;
        WallpaperEngine::WebBrowser::CEF::CRenderHandler* m_renderHandler = nullptr;

        int m_width = 16;
        int m_height = 17;

        WallpaperEngine::Input::MouseClickStatus m_leftClick = Input::Released;
        WallpaperEngine::Input::MouseClickStatus m_rightClick = Input::Released;

        glm::vec2 m_mousePosition = {};
        glm::vec2 m_mousePositionLast = {};
};
}
