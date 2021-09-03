#pragma once

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include "WallpaperEngine/Core/CWallpaper.h"
#include "WallpaperEngine/Core/CScene.h"
#include "WallpaperEngine/Core/CVideo.h"

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
        virtual void render ();

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

        /**
         * @return The scene's framebuffer
         */
        GLuint getWallpaperFramebuffer () const;
        /**
         * @return The scene's texture
         */
        GLuint getWallpaperTexture () const;

    protected:
        void createFramebuffer (GLuint* framebuffer, GLuint* depthbuffer, GLuint* texture);

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
         * The framebuffer used for the scene output
         */
        GLuint m_sceneFramebuffer;
        /**
         * The depthbuffer used for the scene output
         */
        GLuint m_sceneDepthBuffer;
        /**
         * The texture used for the scene output
         */
        GLuint m_sceneTexture;
        GLuint m_texCoordBuffer;
        GLuint m_positionBuffer;
        GLuint m_shader;
        // shader variables
        GLint g_Texture0;
        GLint g_ModelViewProjectionMatrix;
        GLint a_Position;
        GLint a_TexCoord;

        /**
         * Setups OpenGL's framebuffers for ping-pong and scene rendering
         */
        void setupFramebuffers ();
        /**
         * Setups OpenGL's shaders for this wallpaper backbuffer
         */
        void setupShaders ();

    private:
        /**
         * The type of background this wallpaper is (used
         */
        std::string m_type;
    };
}
