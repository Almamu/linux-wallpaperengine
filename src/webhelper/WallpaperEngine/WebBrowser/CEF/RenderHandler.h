#pragma once

#include <cstddef>
#include <string>

#include "include/cef_render_handler.h"

namespace WallpaperEngine::WebBrowser::IPC {
class BrowserServer;
}

namespace WallpaperEngine::WebBrowser::CEF {

class RenderHandler : public CefRenderHandler {
public:
    RenderHandler (const std::string& uuid, WallpaperEngine::WebBrowser::IPC::BrowserServer* server);
    ~RenderHandler () override;

    // Open (or re-open on resize) the shared-memory region the core created.
    void configureShmem (const std::string& shmName, int width, int height);

    void GetViewRect (CefRefPtr<CefBrowser> browser, CefRect& rect) override;
    void OnPaint (
        CefRefPtr<CefBrowser> browser, PaintElementType type, const RectList& dirtyRects,
        const void* buffer, int width, int height
    ) override;

    IMPLEMENT_REFCOUNTING (RenderHandler);

private:
    void closeShmem ();

    std::string m_uuid;
    WallpaperEngine::WebBrowser::IPC::BrowserServer* m_server;

    int    m_shmFd   = -1;
    void*  m_shmPtr  = nullptr;  // MAP_FAILED sentinel handled in impl
    size_t m_shmSize = 0;
    int    m_width   = 0;
    int    m_height  = 0;
};

} // namespace WallpaperEngine::WebBrowser::CEF
