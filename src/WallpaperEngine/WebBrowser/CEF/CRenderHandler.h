#pragma once

#include "WallpaperEngine/Render/Wallpapers/CWeb.h"
#include "include/cef_browser.h"
#include "common.h"

namespace WallpaperEngine::Render::Wallpapers {
class CWeb;
}

namespace WallpaperEngine::WebBrowser::CEF {
// *************************************************************************
//! \brief Private implementation to handle CEF events to draw the web page.
// *************************************************************************
class CRenderHandler : public CefRenderHandler {
  public:
    explicit CRenderHandler (WallpaperEngine::Render::Wallpapers::CWeb* webdata);

    //! \brief
    ~CRenderHandler () override = default;

    //! \brief CefRenderHandler interface
    void GetViewRect (CefRefPtr<CefBrowser> browser, CefRect& rect) override;

    //! \brief CefRenderHandler interface
    //! Update the OpenGL texture.
    void OnPaint (CefRefPtr<CefBrowser> browser, PaintElementType type, const RectList& dirtyRects, const void* buffer,
                  int width, int height) override;

    //! \brief CefBase interface
    IMPLEMENT_REFCOUNTING (CRenderHandler);

  private:
    WallpaperEngine::Render::Wallpapers::CWeb* m_webdata;

    [[nodiscard]] int getWidth () const;
    [[nodiscard]] int getHeight () const;

    //! \brief Return the OpenGL texture handle
    [[nodiscard]] GLuint texture () const;
};
} // namespace WallpaperEngine::WebBrowser::CEF