#pragma once

#include "MemoryStreamProtocol.h"
#include "WallpaperEngine/Render/RenderContext.h"

#include <GL/glew.h>
#include <chrono>
#include <deque>
#include <map>
#include <mpv/client.h>
#include <mpv/render.h>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace WallpaperEngine::Render::Wallpapers {
class CScene;
}

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

    /** Retain real-time playback without blocking the renderer. */
    void setNonBlocking ();
    void clearNonBlocking ();
    void setMuted ();
    void clearMuted ();
    void setVolume (double volume);
    void setPaused ();
    void clearPaused ();
    /** Scene sampling demand suspends decoding without changing script playback state. */
    void setSceneActive (const Render::Wallpapers::CScene& scene, bool active);
    void removeSceneActivity (const Render::Wallpapers::CScene& scene);

    /** SceneScript playback controls retain the decoder when stopped. */
    void enableScriptControl ();
    void resumePlayback ();
    void stopPlayback ();
    [[nodiscard]] bool isPlaying () const;
    [[nodiscard]] double getDuration () const;
    [[nodiscard]] double getCurrentTime () const;
    void setCurrentTime (double time);
    [[nodiscard]] double getRate () const { return m_rate; }
    void setRate (double rate);
    [[nodiscard]] bool getLoop () const { return m_loop; }
    void setLoop (bool loop);

    /** Standalone video only; embedded textures retain the existing SDR contract. */
    void enableHDR ();
    void setHDRPresentation (bool enabled) const;
    [[nodiscard]] bool hasHDRSource () const;
    [[nodiscard]] bool isHDRFrame () const;
    void render (bool trackScreenshotFrame = false) const;
    [[nodiscard]] bool hasScreenshotFrame () const;

    int getWidth () const;
    int getHeight () const;

private:
    void prepareGL ();
    void queueHDRConfiguration (bool enabled) const;
    void clearOutputFrame () const;
    void init ();
    void play ();
    void setSource (MemoryStreamProtocolUniquePtr source);
    void setSource (const std::filesystem::path& file);
    void stop ();
    uint64_t queueCommand (std::vector<std::string> arguments, bool seek = false) const;
    void submitCommand () const;
    void processEvent (const mpv_event& event) const;
    void completeCommand (int error) const;
    void updateActivityPause ();
    void startAudioFade ();
    void updateAudioFade (std::chrono::steady_clock::time_point now);

protected:
    bool m_doWeOwnFramebuffer;
    GLuint m_outputTexture;
    GLuint m_fbo = GL_NONE;
    mutable int64_t m_width;
    mutable int64_t m_height;
    mpv_handle* m_handle = nullptr;
    mpv_render_context* m_renderContext = nullptr;
    double m_volume = 0.0f;
    double m_audioVolume = 0.0;
    std::optional<std::chrono::steady_clock::time_point> m_audioFadeNextTick;
    bool m_muted = false;
    bool m_nonBlocking = false;
    bool m_hdrEnabled = false;
    mutable bool m_hdrSource = false;
    mutable bool m_hdrPresentation = false;
    mutable bool m_hdrConfigured = false;
    mutable bool m_hdrFrame = false;
    mutable bool m_hasDecodedScreenshotFrame = false;
    mutable bool m_hasScreenshotFrame = false;
    mutable bool m_videoWidthReported = false;
    mutable bool m_videoHeightReported = false;
    mutable bool m_hdrDisabled = false;
    mutable bool m_hdrConfigurationBroken = false;
    mutable bool m_hdrConfigurationTarget = false;
    mutable bool m_hdrConfigurationFailed = false;
    mutable uint64_t m_hdrConfigurationBegin = 0;
    mutable uint64_t m_hdrConfigurationEnd = 0;
    bool m_paused = false;
    bool m_activitySuspended = false;
    std::map<const Render::Wallpapers::CScene*, bool> m_sceneActivity;
    double m_rate = 1.0;
    bool m_loop = true;
    bool m_scriptControlled = false;
    mutable bool m_loaded = false;
    mutable std::optional<double> m_pendingSeek;
    mutable double m_currentTime = 0.0;
    mutable double m_duration = 0.0;
    mutable bool m_ended = false;
    std::optional<double> m_pausedSeek;
    struct Command {
	uint64_t id;
	std::vector<std::string> arguments;
	bool seek;
    };
    mutable std::deque<Command> m_commands;
    mutable uint64_t m_nextCommandId = 0;
    mutable uint64_t m_pendingSeekId = 0;
    mutable bool m_commandInFlight = false;
    mutable bool m_dimensionsChanged = false;
    mutable std::recursive_mutex m_playbackMutex;
    std::jthread m_eventThread;
    std::optional<std::filesystem::path> m_file;
    std::optional<MemoryStreamProtocolUniquePtr> m_stream;
    uint32_t m_usageCount = 0;
};

using GLPlayerUniquePtr = std::unique_ptr<GLPlayer>;
using GLPlayerSharedPtr = std::shared_ptr<GLPlayer>;
};
