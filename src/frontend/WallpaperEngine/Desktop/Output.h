#pragma once

#include "WallpaperEngine/Application/ApplicationContext.h"

#include <glad/glad.h>

#include <glm/vec2.hpp>
#include <glm/vec4.hpp>
#include <linux-wallpaperengine/project.h>

namespace WallpaperEngine::Desktop {
class Output {
public:
    Output (wp_project* wallpaper, glm::vec4 viewport, bool vflip = false);
    virtual ~Output ();

    virtual void render ();

    virtual void setWallpaper (wp_project* wallpaper);
    virtual void setViewport (glm::vec4 viewport);
    virtual void setFramebuffer (GLuint framebuffer);
    virtual void setClamping (Application::ApplicationContext::CLAMP_MODE mode);
    virtual void setScaling (Application::ApplicationContext::SCALING_MODE mode);

    [[nodiscard]] glm::vec4 getViewport () const;
    [[nodiscard]] GLuint getFramebuffer () const;
    [[nodiscard]] wp_project* getWallpaper () const;

    [[nodiscard]] glm::ivec2 getGlobalPosition () const;
    [[nodiscard]] glm::ivec2 getLogicalSize () const;
    [[nodiscard]] glm::ivec4 getFramebufferBounds () const;

    void setGlobalPosition (glm::ivec2 position);
    void setLogicalSize (glm::ivec2 size);
    void setFramebufferBounds (glm::ivec4 bounds);

private:
    void setupFramebuffer ();
    void clearFramebuffer ();

    /** Framebuffer used by the desktop environment and uses the viewport specified */
    GLuint m_outputFramebuffer;
    /** Framebuffer created by the output, used for rendering the wallpaper */
    GLuint m_framebuffer = GL_NONE;
    wp_project* m_wallpaper;
    glm::vec4 m_viewport;

    Application::ApplicationContext::CLAMP_MODE m_clamp = Application::ApplicationContext::CLAMP_MODE_UVS;
    Application::ApplicationContext::SCALING_MODE m_scaling = Application::ApplicationContext::SCALING_MODE_DEFAULT;
    bool m_vflip;
    int m_width = 0;
    int m_height = 0;
    GLuint m_shader = GL_NONE;
    GLuint m_texture = GL_NONE;
    GLuint m_positionBuffer = GL_NONE;
    GLuint m_texCoordBuffer = GL_NONE;
    GLuint m_vaoBuffer = GL_NONE;
    GLint m_texture0 = GL_NONE;
    GLint m_position = GL_NONE;
    GLint m_texCoord = GL_NONE;

    glm::ivec2 m_globalPosition;
    glm::ivec2 m_logicalSize;
    glm::ivec4 m_framebufferBounds;
};
}
