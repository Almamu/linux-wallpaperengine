#pragma once

#include "MemoryStreamProtocol.h"
#include "WallpaperEngine/Render/RenderContext.h"

#include <GL/glew.h>
#include <mpv/client.h>
#include <mpv/render.h>
#include <string>

namespace WallpaperEngine::VideoPlayback::MPV {
class GLPlayer : public Helpers::ContextAware {
    friend struct MemoryStreamProtocol;

public:
    GLPlayer (
	RenderContext& context, GLuint outputTexture, const std::filesystem::path& file, int64_t baseWidth,
	int64_t baseHeight, GLuint fbo = GL_NONE
    );
    GLPlayer (
	RenderContext& context, GLuint outputTexture, MemoryStreamProtocolUniquePtr stream, int64_t baseWidth,
	int64_t baseHeight, GLuint fbo = GL_NONE
    );
    ~GLPlayer () override;

    /**
     * Increments the usage count of the player
     *
     * Directly controls playback, only started when at least one thing is using it
     * Initializes mpv if needed and starts playback
     */
    void incrementUsageCount ();
    /**
     * Decrements the usage count of the player
     *
     * Directly controls playback, only stopped when nothing is using it
     * De-initializes mpv if needed
     */
    void decrementUsageCount ();

    void setUntimed ();
    void clearUntimed ();
    void setMuted ();
    void clearMuted ();
    void setVolume (double volume);
    void setPaused ();
    void clearPaused ();

    void render () const;

    int getWidth () const;
    int getHeight () const;

private:
    void prepareGL ();
    void init ();
    void play ();
    void setSource (MemoryStreamProtocolUniquePtr source);
    void setSource (const std::filesystem::path& file);
    void stop ();

protected:
    bool m_doWeOwnFramebuffer;
    GLuint m_outputTexture;
    GLuint m_fbo = GL_NONE;
    mutable int64_t m_width;
    mutable int64_t m_height;
    mpv_handle* m_handle = nullptr;
    mpv_render_context* m_renderContext = nullptr;
    double m_volume = 0.0f;
    bool m_muted = false;
    bool m_untimed = false;
    bool m_paused = false;
    std::optional<std::filesystem::path> m_file;
    std::optional<MemoryStreamProtocolUniquePtr> m_stream;
    uint32_t m_usageCount = 0;
};

using GLPlayerUniquePtr = std::unique_ptr<GLPlayer>;
using GLPlayerSharedPtr = std::shared_ptr<GLPlayer>;
};
