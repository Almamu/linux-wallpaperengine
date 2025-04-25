#include <iostream>

#include <GL/glew.h>

#include "CRenderContext.h"
#include "CWallpaper.h"

namespace WallpaperEngine::Render {
CRenderContext::CRenderContext (Drivers::CVideoDriver& driver, Input::CInputContext& input,
                                CWallpaperApplication& app) :
    m_driver (driver),
    m_input (input),
    m_app (app),
    m_textureCache (new CTextureCache (*this)) {}

void CRenderContext::render (Drivers::Output::COutputViewport* viewport) {
    viewport->makeCurrent ();

#if !NDEBUG
    const std::string str = "Rendering to output " + viewport->name;

    glPushDebugGroup (GL_DEBUG_SOURCE_APPLICATION, 0, -1, str.c_str ());
#endif /* DEBUG */

    // search the background in the viewport selection
    const auto ref = this->m_wallpapers.find (viewport->name);

    // render the background
    if (ref != this->m_wallpapers.end ())
        ref->second->render (viewport->viewport, this->getOutput ().renderVFlip ());

#if !NDEBUG
    glPopDebugGroup ();
#endif /* DEBUG */

    viewport->swapOutput ();
}

void CRenderContext::setWallpaper (const std::string& display, CWallpaper* wallpaper) {
    this->m_wallpapers.insert_or_assign (display, wallpaper);
}

void CRenderContext::setPause (bool newState) {
    for (auto&& wallpaper : this->m_wallpapers)
        wallpaper.second->setPause (newState);
}

Input::CInputContext& CRenderContext::getInputContext () const {
    return this->m_input;
}

const CWallpaperApplication& CRenderContext::getApp () const {
    return this->m_app;
}

const Drivers::CVideoDriver& CRenderContext::getDriver () const {
    return this->m_driver;
}

const Drivers::Output::COutput& CRenderContext::getOutput () const {
    return this->m_driver.getOutput ();
}

std::shared_ptr<const ITexture> CRenderContext::resolveTexture (const std::string& name) {
    return this->m_textureCache->resolve (name);
}

const std::map<std::string, CWallpaper*>& CRenderContext::getWallpapers () const {
    return this->m_wallpapers;
}
} // namespace WallpaperEngine::Render
