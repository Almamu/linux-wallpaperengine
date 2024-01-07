#pragma once

// Matrices manipulation for OpenGL
#include <glm/glm.hpp>
#include <glm/ext.hpp>

#include <string>
#include <vector>
#include <memory>
#include <algorithm>

#include "common.h"
#include "WallpaperEngine/Core/CWeb.h"
#include "WallpaperEngine/Render/CWallpaper.h"
#include "WallpaperEngine/Audio/CAudioStream.h"

namespace WallpaperEngine::Render
{
    class CWeb : public CWallpaper
    {
        public:
            CWeb (Core::CWeb* scene, CRenderContext& context, CAudioContext& audioContext, const CWallpaperState::TextureUVsScaling& scalingMode);
            ~CWeb();
            uint32_t getWidth  () const override { return this->m_width; }

            uint32_t getHeight () const override { return this->m_height; }

            void setSize (int64_t width, int64_t height);

        protected:
            void renderFrame (glm::ivec4 viewport) override;
            void updateMouse (glm::ivec4 viewport);
            Core::CWeb* getWeb ()
            {
                return this->getWallpaperData ()->as<Core::CWeb> ();
            }

            friend class CWallpaper;

            static const std::string Type;

        private:
            // *************************************************************************
            //! \brief Private implementation to handle CEF events to draw the web page.
            // *************************************************************************
            class RenderHandler: public CefRenderHandler
            {
                public:
                    RenderHandler(CWeb* webdata);

                    //! \brief
                    ~RenderHandler();

                    //! \brief Compile OpenGL shaders and create OpenGL objects (VAO,
                    //! VBO, texture, locations ...)
                    bool init();

                    //! \brief CefRenderHandler interface
                    void GetViewRect(CefRefPtr<CefBrowser> browser, CefRect &rect) override;

                    //! \brief CefRenderHandler interface
                    //! Update the OpenGL texture.
                    void OnPaint(CefRefPtr<CefBrowser> browser, PaintElementType type,
                                        const RectList &dirtyRects, const void *buffer,
                                        int width, int height) override;

                    //! \brief CefBase interface
                    IMPLEMENT_REFCOUNTING(RenderHandler);

                private:
                    CWeb* m_webdata;

                    uint32_t getWidth () const {
                        return this->m_webdata->getWidth();
                    };
                    uint32_t getHeight () const {
                        return this->m_webdata->getHeight();
                    };
                    //! \brief Return the OpenGL texture handle
                    GLuint texture() const
                    {
                        return this->m_webdata->getWallpaperFramebuffer();
                    }
            };

            // *************************************************************************
            //! \brief Provide access to browser-instance-specific callbacks. A single
            //! CefClient instance can be shared among any number of browsers.
            // *************************************************************************
            class BrowserClient: public CefClient
            {
                public:
                    BrowserClient(CefRefPtr<CefRenderHandler> ptr)
                        : m_renderHandler(ptr)
                    {}

                    CefRefPtr<CefRenderHandler> GetRenderHandler() override
                    {
                        return m_renderHandler;
                    }

                    CefRefPtr<CefRenderHandler> m_renderHandler;

                    IMPLEMENT_REFCOUNTING(BrowserClient);
            };
            
            CefRefPtr<CefBrowser> m_browser;
            CefRefPtr<BrowserClient> m_client;
            RenderHandler* m_render_handler = nullptr;

            int64_t m_width;
            int64_t m_height;

            glm::vec2 m_mousePosition;
            glm::vec2 m_mousePositionLast;
    };
}
