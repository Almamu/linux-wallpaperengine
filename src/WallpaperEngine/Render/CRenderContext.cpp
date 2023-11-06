#include "common.h"
#include <iostream>

#include <GL/glew.h>

#include "CRenderContext.h"
#include "CVideo.h"
#include "CWallpaper.h"

namespace WallpaperEngine::Render
{
    CRenderContext::CRenderContext (
        Drivers::CVideoDriver& driver, Input::CInputContext& input,
        CWallpaperApplication& app
    ) :
        m_defaultWallpaper (nullptr),
        m_driver (driver),
        m_app (app),
        m_input (input),
        m_textureCache (new CTextureCache (*this))
    {
    }

    void CRenderContext::render (Drivers::Output::COutputViewport* viewport)
    {
        viewport->makeCurrent ();

#if !NDEBUG
        std::string str = "Rendering to output " + viewport->name;

        glPushDebugGroup (GL_DEBUG_SOURCE_APPLICATION, 0, -1, str.c_str ());
#endif /* DEBUG */

        // search the background in the viewport selection
        auto ref = this->m_wallpapers.find (viewport->name);

        // render the background
        if (ref != this->m_wallpapers.end ())
            ref->second->render (viewport->viewport, this->getOutput ().renderVFlip (), this->getApp().getContext().settings.render.window.scaleToFit);
        else
            this->m_defaultWallpaper->render (viewport->viewport, this->getOutput ().renderVFlip (), this->getApp().getContext().settings.render.window.scaleToFit);

#if !NDEBUG
        glPopDebugGroup ();
#endif /* DEBUG */

        viewport->swapOutput ();
    }

    void CRenderContext::setDefaultWallpaper (CWallpaper* wallpaper)
    {
        this->m_defaultWallpaper = wallpaper;
    }

    void CRenderContext::setWallpaper (const std::string& display, CWallpaper* wallpaper)
    {
        this->m_wallpapers.insert_or_assign (display, wallpaper);
    }

    Input::CInputContext& CRenderContext::getInputContext () const
    {
        return this->m_input;
    }

    const CWallpaperApplication& CRenderContext::getApp () const
    {
        return this->m_app;
    }

    const Drivers::CVideoDriver& CRenderContext::getDriver () const
    {
        return this->m_driver;
    }

    const Drivers::Output::COutput& CRenderContext::getOutput () const
    {
        return this->m_driver.getOutput ();
    }

    const ITexture* CRenderContext::resolveTexture (const std::string& name)
    {
        return this->m_textureCache->resolve (name);
    }
}