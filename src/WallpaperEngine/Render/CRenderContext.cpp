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

    void CRenderContext::render ()
    {
        bool firstFrame = true;
        bool renderFrame = true;

        for (const auto& cur : this->getOutput ().getViewports ())
        {
            cur.second->makeCurrent ();

#if !NDEBUG
            std::string str = "Rendering to output " + cur.first;

            glPushDebugGroup (GL_DEBUG_SOURCE_APPLICATION, 0, -1, str.c_str ());
#endif /* DEBUG */
            // search the background in the viewport selection
            auto ref = this->m_wallpapers.find (cur.first);

            // render the background
            if (ref != this->m_wallpapers.end ())
                ref->second->render (cur.second->viewport, this->getOutput ().renderVFlip (), renderFrame, firstFrame);
            else
                this->m_defaultWallpaper->render (
                    cur.second->viewport, this->getOutput ().renderVFlip (), renderFrame, firstFrame
                );
            // scenes need to render a new frame for each viewport as they produce different results
            // but videos should only be rendered once per group of viewports
            firstFrame = false;
#if !NDEBUG
            glPopDebugGroup ();
#endif /* DEBUG */

            cur.second->swapOutput ();
        }

        // read the full texture into the image
        if (this->getOutput ().haveImageBuffer ())
            glReadPixels (
                0, 0, this->getOutput ().getFullWidth (), this->getOutput ().getFullHeight (), GL_BGRA, GL_UNSIGNED_BYTE,
                this->getOutput ().getImageBuffer ()
            );

        // update the output with the given image
        this->getOutput ().updateRender ();
        // finally swap buffers
        this->m_driver.swapBuffers ();
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