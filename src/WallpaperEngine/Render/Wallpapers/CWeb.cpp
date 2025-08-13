// This code is a modification of the original projects that can be found at
// https://github.com/if1live/cef-gl-example
// https://github.com/andmcgregor/cefgui
#include "CWeb.h"
#include "WallpaperEngine/WebBrowser/CEF/CWPSchemeHandlerFactory.h"

#include "WallpaperEngine/Data/Model/Wallpaper.h"
#include "WallpaperEngine/Data/Model/Project.h"

using namespace WallpaperEngine::Render;
using namespace WallpaperEngine::Render::Wallpapers;

using namespace WallpaperEngine::WebBrowser;
using namespace WallpaperEngine::WebBrowser::CEF;

CWeb::CWeb (
    const Wallpaper& wallpaper, CRenderContext& context, CAudioContext& audioContext,
    CWebBrowserContext& browserContext, const CWallpaperState::TextureUVsScaling& scalingMode,
    const WallpaperEngine::Assets::ITexture::TextureFlags& clampMode
) :
    CWallpaper (wallpaper, context, audioContext, scalingMode, clampMode),
    m_browserContext (browserContext) {
    // setup framebuffers
    this->setupFramebuffers ();

    CefWindowInfo window_info;
    window_info.SetAsWindowless (0);

    this->m_renderHandler = new WebBrowser::CEF::CRenderHandler (this);

    CefBrowserSettings browserSettings;
    // Documentaion says that 60 fps is maximum value
    browserSettings.windowless_frame_rate = std::max (60, context.getApp ().getContext ().settings.render.maximumFPS);

    this->m_client = new WebBrowser::CEF::CBrowserClient (m_renderHandler);
    // use the custom scheme for the wallpaper's files
    const std::string htmlURL =
        CWPSchemeHandlerFactory::generateSchemeName(this->getWeb ().project.workshopId) +
        "://root/" +
        this->getWeb().filename;
    this->m_browser =
        CefBrowserHost::CreateBrowserSync (window_info, this->m_client, htmlURL, browserSettings, nullptr, nullptr);
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
    this->m_browser->GetHost ()->WasResized ();
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
    auto& input = this->getContext ().getInputContext ().getMouseInput ();

    glm::dvec2 position = input.position ();
    WallpaperEngine::Input::MouseClickStatus leftClick = input.leftClick();
    WallpaperEngine::Input::MouseClickStatus rightClick = input.rightClick();

    CefMouseEvent evt;
    // Set mouse current position. Maybe clamps are not needed
    evt.x = std::clamp (int (position.x - viewport.x), 0, viewport.z);
    evt.y = std::clamp (int (position.y - viewport.y), 0, viewport.w);
    // Send mouse position to cef
    this->m_browser->GetHost ()->SendMouseMoveEvent (evt, false);

    // TODO: ANY OTHER MOUSE EVENTS TO SEND?
    if (leftClick != this->m_leftClick) {
        this->m_browser->GetHost ()->SendMouseClickEvent (evt, CefBrowserHost::MouseButtonType::MBT_LEFT, leftClick == WallpaperEngine::Input::MouseClickStatus::Released, 1);
    }

    if (rightClick != this->m_rightClick) {
        this->m_browser->GetHost ()->SendMouseClickEvent (evt, CefBrowserHost::MouseButtonType::MBT_RIGHT, rightClick == WallpaperEngine::Input::MouseClickStatus::Released, 1);
    }

    this->m_leftClick = leftClick;
    this->m_rightClick = rightClick;
}

CWeb::~CWeb () {
    CefDoMessageLoopWork ();
    this->m_browser->GetHost ()->CloseBrowser (true);

    delete this->m_renderHandler;
}
