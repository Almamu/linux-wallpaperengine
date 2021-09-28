#pragma once

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include "WallpaperEngine/Core/CWallpaper.h"
#include "WallpaperEngine/Core/CScene.h"
#include "WallpaperEngine/Core/CVideo.h"

#include "WallpaperEngine/Assets/CContainer.h"
#include "CFBO.h"

using namespace WallpaperEngine::Assets;

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
        void render (glm::vec4 viewport, bool newFrame = true);

        /**
         * @return The container to resolve files for this wallpaper
         */
        CContainer* getContainer () const;

        /**
         * @return The scene's framebuffer
         */
        GLuint getWallpaperFramebuffer () const;
        /**
         * @return The scene's texture
         */
        GLuint getWallpaperTexture () const;
        /**
         * Creates a new FBO for this wallpaper
         *
         * @param name The name of the FBO
         * @return
         */
        CFBO* createFBO (const std::string& name, ITexture::TextureFormat format, float scale, uint32_t realWidth, uint32_t realHeight, uint32_t textureWidth, uint32_t textureHeight);

        /**
         * @return The full FBO list to work with
         */
        const std::map<std::string, CFBO*>& getFBOs () const;
        /**
         * Searches the FBO list for the given FBO
         *
         * @param name
         * @return
         */
        CFBO* findFBO (const std::string& name) const;

        /**
         * @return The main FBO of this wallpaper
         */
        CFBO* getFBO () const;

    protected:
        /**
         * Renders a frame of the wallpaper
         */
        virtual void renderFrame () = 0;

        /**
         * Setups OpenGL's framebuffers for ping-pong and scene rendering
         */
        void setupFramebuffers ();

        CContainer* m_container;
        Core::CWallpaper* m_wallpaperData;

        Core::CWallpaper* getWallpaperData ();

        /**
         * The FBO used for scene output
         */
        CFBO* m_sceneFBO;

    private:
        /**
         * The texture used for the scene output
         */
        GLuint m_texCoordBuffer;
        GLuint m_positionBuffer;
        GLuint m_shader;
        // shader variables
        GLint g_Texture0;
        GLint a_Position;
        GLint a_TexCoord;
        /**
         * Setups OpenGL's shaders for this wallpaper backbuffer
         */
        void setupShaders ();
        /**
         * The type of background this wallpaper is (used
         */
        std::string m_type;

        /**
         * List of FBOs registered for this wallpaper
         */
        std::map<std::string, CFBO*> m_fbos;
    };
}
