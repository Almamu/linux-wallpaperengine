#include <iostream>

#include <GL/glew.h>

#include "CWallpaper.h"
#include "RenderContext.h"

#include "WallpaperEngine/Data/Model/Project.h"

namespace WallpaperEngine::Render {
RenderContext::RenderContext (Drivers::VideoDriver& driver, WallpaperApplication& app) :
    m_driver (driver), m_app (app), m_textureCache (new TextureCache (*this)) { }

void RenderContext::render (Drivers::Output::OutputViewport* viewport) {
    viewport->makeCurrent ();

#if !NDEBUG
    const std::string str = "Rendering to output " + viewport->name;

    glPushDebugGroup (GL_DEBUG_SOURCE_APPLICATION, 0, -1, str.c_str ());
#endif /* DEBUG */

    // search the background in the viewport selection

    // render the background
    if (const auto ref = this->m_wallpapers.find (viewport->name); ref != this->m_wallpapers.end ()) {
	ref->second->render (viewport->viewport, this->getOutput ().renderVFlip ());
    }

#if !NDEBUG
    glPopDebugGroup ();
#endif /* DEBUG */

    viewport->swapOutput ();
}

void RenderContext::setWallpaper (const std::string& display, std::shared_ptr<CWallpaper> wallpaper) {
    this->m_wallpapers.insert_or_assign (display, wallpaper);
}

void RenderContext::setPause (const bool newState) const {
    for (const auto& wallpaper : this->m_wallpapers | std::views::values) {
	wallpaper->setPause (newState);
    }
}

Input::InputContext& RenderContext::getInputContext () const { return this->m_driver.getInputContext (); }

const WallpaperApplication& RenderContext::getApp () const { return this->m_app; }

const Drivers::VideoDriver& RenderContext::getDriver () const { return this->m_driver; }

const Drivers::Output::Output& RenderContext::getOutput () const { return this->m_driver.getOutput (); }

std::shared_ptr<const TextureProvider> RenderContext::resolveTexture (const std::string& name) const {
    return this->m_textureCache->resolve (name);
}

const std::map<std::string, std::shared_ptr<CWallpaper>>& RenderContext::getWallpapers () const {
    return this->m_wallpapers;
}
} // namespace WallpaperEngine::Render
