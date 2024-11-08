#pragma once

// Matrices manipulation for OpenGL
#include <glm/glm.hpp>
#include <glm/ext.hpp>

#include <string>
#include <utility>
#include <vector>
#include <memory>
#include <algorithm>

#include "WallpaperEngine/Audio/CAudioStream.h"
#include "WallpaperEngine/Core/Wallpapers/CWeb.h"
#include "WallpaperEngine/Render/CWallpaper.h"
#include "common.h"

namespace WallpaperEngine::Render
{
    class CWeb : public CWallpaper
    {
        public:
            CWeb (Core::CWeb* scene, CRenderContext& context, CAudioContext& audioContext, WallpaperEngine::WebBrowser::CWebBrowserContext& browserContext, const CWallpaperState::TextureUVsScaling& scalingMode);
            ~CWeb() override;
            [[nodiscard]] int getWidth  () const override { return this->m_width; }

            [[nodiscard]] int getHeight () const override { return this->m_height; }

            void setSize (int width, int height);

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
                    explicit RenderHandler(CWeb* webdata);

                    //! \brief
                    ~RenderHandler() override = default;

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

                    int getWidth () const {
                        return this->m_webdata->getWidth();
                    };
                    int getHeight () const {
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
                    explicit BrowserClient(CefRefPtr<CefRenderHandler> ptr)
                        : m_renderHandler(std::move(ptr))
                    {}

                    CefRefPtr<CefRenderHandler> GetRenderHandler() override
                    {
                        return m_renderHandler;
                    }

                    CefRefPtr<CefRenderHandler> m_renderHandler;

                    IMPLEMENT_REFCOUNTING(BrowserClient);
            };

            WallpaperEngine::WebBrowser::CWebBrowserContext& m_browserContext;
            CefRefPtr<CefBrowser> m_browser;
            CefRefPtr<BrowserClient> m_client;
            RenderHandler* m_render_handler = nullptr;

            int m_width;
            int m_height;

            WallpaperEngine::Input::MouseClickStatus m_leftClick;
            WallpaperEngine::Input::MouseClickStatus m_rightClick;

            glm::vec2 m_mousePosition;
            glm::vec2 m_mousePositionLast;
    };
}
