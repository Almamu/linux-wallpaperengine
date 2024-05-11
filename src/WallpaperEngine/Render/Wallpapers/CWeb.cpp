// This code is a modification of the original projects that can be found at
// https://github.com/if1live/cef-gl-example
// https://github.com/andmcgregor/cefgui
#include "CWeb.h"

using namespace WallpaperEngine::Render;
using namespace WallpaperEngine::WebBrowser;

CWeb::CWeb (Core::CWeb* web, CRenderContext& context, CAudioContext& audioContext, CWebBrowserContext& browserContext,
            const CWallpaperState::TextureUVsScaling& scalingMode) :
    CWallpaper (web, Type, context, audioContext, scalingMode),
    m_width (16),
    m_height (16),
    m_browserContext (browserContext),
    m_browser (),
    m_client () {
    this->m_browserContext.markAsUsed ();
    // setup framebuffers
    this->setupFramebuffers ();

    CefWindowInfo window_info;
    window_info.SetAsWindowless (0);

    this->m_render_handler = new RenderHandler (this);

    CefBrowserSettings browserSettings;
    // Documentaion says that 60 fps is maximum value
    browserSettings.windowless_frame_rate = std::max (60, context.getApp ().getContext ().settings.render.maximumFPS);

    m_client = new BrowserClient (m_render_handler);
    std::filesystem::path htmlpath =
        this->getWeb ()->getProject ().getContainer ()->resolveRealFile (this->getWeb ()->getFilename ());
    // To open local file in browser URL must be "file:///path/to/file.html"
    const std::string htmlURL = std::string ("file:///") + htmlpath.c_str ();
    m_browser =
        CefBrowserHost::CreateBrowserSync (window_info, m_client.get (), htmlURL, browserSettings, nullptr, nullptr);
}

void CWeb::setSize (int width, int height) {
    this->m_width = width > 0 ? width : this->m_width;
    this->m_height = height > 0 ? height : this->m_height;

    // do not refresh the texture if any of the sizes are invalid
    if (this->m_width <= 0 || this->m_height <= 0)
        return;

    // reconfigure the texture
    glBindTexture (GL_TEXTURE_2D, this->getWallpaperTexture ());
    glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA8, this->getWidth (), this->getHeight (), 0, GL_RGBA, GL_UNSIGNED_BYTE,
                  nullptr);

    // Notify cef that it was resized(maybe it's not even needed)
    m_browser->GetHost ()->WasResized ();
}

void CWeb::renderFrame (glm::ivec4 viewport) {
    // ensure the viewport matches the window size, and resize if needed
    if (viewport.z != this->getWidth () || viewport.w != this->getHeight ()) {
        this->setSize (viewport.z, viewport.w);
    }

    // ensure the virtual mouse position is up to date
    this->updateMouse (viewport);
    // use the scene's framebuffer by default
    glBindFramebuffer (GL_FRAMEBUFFER, this->getWallpaperFramebuffer ());
    // ensure we render over the whole framebuffer
    glViewport (0, 0, this->getWidth (), this->getHeight ());

    // Cef processes all messages, including OnPaint, which renders frame
    // If there is no OnPaint in message loop, we will not update(render) frame
    //  This means some frames will not have OnPaint call in cef messageLoop
    //  Because of that glClear will result in flickering on higher fps
    //  Do not use glClear until some method to control rendering with cef is supported
    // We might actually try to use cef to execute javascript, and not using off-screen rendering at all
    // But for now let it be like this
    //  glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    CefDoMessageLoopWork ();
}

void CWeb::updateMouse (glm::ivec4 viewport) {
    // update virtual mouse position first
    glm::dvec2 position = this->getContext ().getInputContext ().getMouseInput ().position ();

    CefMouseEvent evt;
    // Set mouse current position. Maybe clamps are not needed
    evt.x = std::clamp (int (position.x - viewport.x), 0, viewport.z);
    evt.y = std::clamp (int (position.y - viewport.y), 0, viewport.w);
    // Send mouse position to cef
    m_browser->GetHost ()->SendMouseMoveEvent (evt, false);
}

CWeb::~CWeb () {
    CefDoMessageLoopWork ();
    m_browser->GetHost ()->CloseBrowser (true);
}

CWeb::RenderHandler::RenderHandler (CWeb* webdata) : m_webdata (webdata) {}

// Required by CEF
void CWeb::RenderHandler::GetViewRect (CefRefPtr<CefBrowser> browser, CefRect& rect) {
    rect = CefRect (0, 0, this->m_webdata->getWidth (), this->m_webdata->getHeight ());
}

// Will be executed in CEF message loop
void CWeb::RenderHandler::OnPaint (CefRefPtr<CefBrowser> browser, PaintElementType type, const RectList& dirtyRects,
                                   const void* buffer, int width, int height) {
    // sLog.debug("BrowserView::RenderHandler::OnPaint");
    glActiveTexture (GL_TEXTURE0);
    glBindTexture (GL_TEXTURE_2D, this->texture ());
    glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_BGRA_EXT, GL_UNSIGNED_BYTE, (unsigned char*) buffer);
    glBindTexture (GL_TEXTURE_2D, 0);
}

const std::string CWeb::Type = "web";