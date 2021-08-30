#pragma once

#include <irrlicht/irrlicht.h>
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include "WallpaperEngine/Core/CWallpaper.h"
#include "WallpaperEngine/Core/CScene.h"
#include "WallpaperEngine/Core/CVideo.h"

#include "WallpaperEngine/Irrlicht/CContext.h"
#include "WallpaperEngine/Assets/CContainer.h"

using namespace WallpaperEngine::Assets;

namespace WallpaperEngine::Irrlicht
{
    class CContext;
};

namespace WallpaperEngine::Render
{
    class CWallpaper
    {
    public:
        template<class T> const T* as () const { assert (is<T> ()); return (const T*) this; }
        template<class T> T* as () { assert (is<T> ()); return (T*) this; }

        template<class T> bool is () { return this->m_type == T::Type; }

        CWallpaper (Core::CWallpaper* wallpaperData, std::string type, CContainer* container);
        ~CWallpaper ();

        /**
         * Performs a render pass of the wallpaper
         */
        virtual void render () = 0;

        /**
         * @return The container to resolve files for this wallpaper
         */
        CContainer* getContainer () const;

        /**
         * Performs a ping-pong on the available framebuffers to be able to continue rendering things to them
         *
         * @param drawTo The framebuffer to use
         * @param asInput The last texture used as output (if needed)
         */
        void pinpongFramebuffer (GLuint* drawTo, GLuint* inputTexture);

    protected:
        CContainer* m_container;
        Core::CWallpaper* m_wallpaperData;

        Core::CWallpaper* getWallpaperData ();

        /**
         * The main framebuffer
         */
        GLuint m_mainFramebuffer;
        /**
         * The sub framebuffer
         */
        GLuint m_subFramebuffer;

        /**
         * The main depth render buffer
         */
        GLuint m_mainDepthBuffer;
        /**
         * The sub depth render buffer
         */
        GLuint m_subDepthBuffer;

        /**
         * The main texture used on the framebuffer
         */
        GLuint m_mainTexture;
        /**
         * The sub texture used on the framebuffer
         */
        GLuint m_subTexture;

        /**
         * Setups OpenGL's framebuffers for ping-pong
         */
        void setupFramebuffers ();

    private:
        /**
         * The type of background this wallpaper is (used
         */
        std::string m_type;
    };
}
