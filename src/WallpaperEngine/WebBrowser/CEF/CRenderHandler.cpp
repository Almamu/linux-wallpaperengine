#include "CRenderHandler.h"

using namespace WallpaperEngine::WebBrowser::CEF;

CRenderHandler::CRenderHandler (WallpaperEngine::Render::Wallpapers::CWeb* webdata) : m_webdata (webdata) {}

// Required by CEF
void CRenderHandler::GetViewRect (CefRefPtr<CefBrowser> browser, CefRect& rect) {
    rect = CefRect (0, 0, this->m_webdata->getWidth (), this->m_webdata->getHeight ());
}

// Will be executed in CEF message loop
void CRenderHandler::OnPaint (CefRefPtr<CefBrowser> browser, PaintElementType type, const RectList& dirtyRects,
                                   const void* buffer, int width, int height) {
    glActiveTexture (GL_TEXTURE0);
    glBindTexture (GL_TEXTURE_2D, this->texture ());
    glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_BGRA_EXT, GL_UNSIGNED_BYTE, (unsigned char*) buffer);
    glBindTexture (GL_TEXTURE_2D, 0);
}

int CRenderHandler::getWidth() const {
    return this->m_webdata->getWidth ();
}

int CRenderHandler::getHeight() const {
    return this->m_webdata->getHeight ();
}

GLuint CRenderHandler::texture () const {
    return this->m_webdata->getWallpaperFramebuffer();
}