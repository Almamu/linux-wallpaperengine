#pragma once

#include <chrono>
#include <random>

#include "WallpaperEngine/Application/ApplicationContext.h"
#include "WallpaperEngine/Application/LockscreenStream.h"
#include "WallpaperEngine/Application/PlaylistState.h"
#include "WallpaperEngine/Assets/AssetLocator.h"

#include "WallpaperEngine/Render/CWallpaper.h"
#include "WallpaperEngine/Render/Drivers/Detectors/FullScreenDetector.h"
#include "WallpaperEngine/Render/Drivers/GLFWOpenGLDriver.h"
#include "WallpaperEngine/Render/Drivers/Output/GLFWWindowOutput.h"
#include "WallpaperEngine/Render/RenderContext.h"

#include "WallpaperEngine/Audio/Drivers/SDLAudioDriver.h"

#include "WallpaperEngine/Input/InputContext.h"
#include "WallpaperEngine/WebBrowser/WebBrowserContext.h"

#include "WallpaperEngine/Data/Assets/Types.h"
#include "WallpaperEngine/Data/Model/Types.h"
#include "WallpaperEngine/Media/MediaSource.h"

#include <condition_variable>
#include <deque>
#include <mutex>
#include <set>
#include <thread>

namespace WallpaperEngine::Render {
struct TextureMipStream;
class CWallpaper;
}

namespace WallpaperEngine::Application {

using namespace WallpaperEngine::Assets;
using namespace WallpaperEngine::Data::Model;
/**
 * Small wrapper class over the actual wallpaper's main application skeleton
 */
class WallpaperApplication {
public:
    explicit WallpaperApplication (ApplicationContext& context);
    ~WallpaperApplication ();

    /**
     * Prepares the application for rendering.
     */
    void setup ();
    /**
     * Renders a frame of the application.
     */
    void render ();
    /**
     * Cleans up all the resources used by the application.
     */
    static void cleanup ();
    /**
     * Shows the application until it's closed
     */
    void show ();
    /**
     * @return Whether the render loop stopped abnormally (the video driver lost its output), so the
     *         process should exit non-zero and let a supervisor relaunch it
     */
    [[nodiscard]] bool abnormalTermination () const;
    /**
     * Handles a OS signal sent to this PID
     *
     * @param signal
     */
    void signal (int signal);
    /**
     * @return Maps screens to loaded backgrounds
     */
    [[nodiscard]] const std::map<std::string, ProjectUniquePtr>& getBackgrounds () const;
    /**
     * @return The current application context
     */
    [[nodiscard]] ApplicationContext& getContext () const;
    /**
     * Renders a frame
     */
    void update (Render::Drivers::Output::OutputViewport* viewport, bool present = true);
    /** Finish owned CPU mip records on the existing loader; never submits GL work. */
    void queueTextureStreams (std::vector<std::shared_ptr<Render::TextureMipStream>> streams);
    [[nodiscard]] int getLockscreenStreamFPS () const;
    [[nodiscard]] std::chrono::steady_clock::time_point getLockscreenFrameDeadline (const std::string& screen) const;
    /**
     * Gets the output
     */
    [[nodiscard]] const WallpaperEngine::Render::Drivers::Output::Output& getOutput () const;
    /**
     * Sets the destination framebuffer for rendering. If not called, the default framebuffer will be used.
     */
    void setDestinationFramebuffer (GLuint framebuffer);

    /**
     * Gets the currently set destination framebuffer for rendering. If not set, returns 0 (the default framebuffer).
     */
    [[nodiscard]] GLuint getDestinationFramebuffer () const;

private:
    /**
     * Sets up an asset locator for the given background
     *
     * @param bg
     */
    AssetLocatorUniquePtr setupAssetLocator (const std::string& bg) const;
    /**
     * Initializes subsystems required for application operation
     */
    void initializeSubsystems ();

    /**
     * Loads projects based off the settings
     */
    void loadBackgrounds ();
    /**
     * Loads the given project
     *
     * @param bg
     * @return
     */
    [[nodiscard]] ProjectUniquePtr loadBackground (const std::string& bg);
    /**
     * Loads and parses the given project. Touches no GL or mutable application state so
     * the switch worker thread can run it off the render thread.
     *
     * @param bg
     * @return
     */
    [[nodiscard]] ProjectUniquePtr loadProject (const std::string& bg) const;
    /**
     * Re-arms the automatic screenshot so it is retaken after a background change
     */
    void resetScreenshotState ();
    /**
     * Prepares all background's values and updates their properties if required
     */
    void setupProperties ();
    /**
     * Updates the properties for the given background based on the current context
     *
     * @param project
     */
    void setupPropertiesForProject (const Project& project);
    /**
     * Prepares CEF browser to be used
     */
    void setupBrowser ();
    /**
     * Prepares desktop environment-related things (like render, window, fullscreen detector, etc)
     */
    void setupOutput ();
    /**
     * Prepares all audio-related things (like detector, output, etc)
     */
    void setupAudio ();
    /**
     * Prepares the render-context of all the backgrounds so they can be displayed on the screen
     */
    void prepareOutputs ();
    /**
     * Prepares output debugging for all opengl errors
     */
    void setupOpenGLDebugging ();
    /**
     * Takes an screenshot of the background and saves it to the specified path
     *
     * @param filename
     */
    void takeScreenshot (const std::filesystem::path& filename) const;
    void exportLockscreenFrame (Render::Drivers::Output::OutputViewport* viewport);
    void releaseLockscreenFrames ();

    struct LockscreenFrame {
	GLuint framebuffer = 0;
	GLuint texture = 0;
	int width = 0, height = 0;
	std::vector<uint8_t> pixels;
	std::chrono::steady_clock::time_point nextFrame {};
    };
    LockscreenStream m_lockscreenStream;
    std::map<std::string, LockscreenFrame> m_lockscreenFrames;

    struct ActivePlaylist {
	ApplicationContext::PlaylistDefinition definition;
	std::vector<std::size_t> order;
	std::size_t orderIndex = 0;
	std::chrono::steady_clock::time_point nextSwitch;
	std::chrono::steady_clock::time_point lastUpdate;
	std::set<std::size_t> failedIndices;
    };

    /**
     * A background switch staged by the loader thread. Project parsing, texture decode
     * and static texture uploads happen off the render thread. Media-backed textures and
     * the final scene install remain on the render thread.
     */
    struct PreparedSwitch {
	/** Monotonic id used to drop requests superseded by a newer one for the same screen */
	uint64_t id = 0;
	std::string screen {};
	std::string path {};
	Render::TransitionMode transition = Render::TransitionMode_Fade;
	/** Parsed project, set by the worker on success */
	ProjectUniquePtr project = nullptr;
	/** Static textures uploaded through the worker's shared GL context */
	std::vector<std::pair<std::string, std::shared_ptr<const Render::TextureProvider>>> readyTextures {};
	/** Media textures, or parsed textures left over when no shared build context is available */
	std::vector<std::pair<std::string, Data::Assets::TextureUniquePtr>> textures {};
	/** Failure description, empty on success */
	std::string error {};
	/** When the switch was requested, for the health report's switch.total */
	std::chrono::steady_clock::time_point requested {};
	/** Loader-thread phases in ms, for the load-time HUD */
	double projectMs = 0.0, textureMs = 0.0, uploadMs = 0.0;
	std::string comparisonToken {};
	/** Render-thread snapshot; the worker never reads mutable output geometry. */
	glm::ivec2 outputSize {};
	uint32_t textureReduction = 1;
    };

    struct ComparisonScreen {
	uint64_t id = 0;
	std::string path {};
	std::string state = "loading";
	std::string error {};
	ProjectUniquePtr project = nullptr;
	std::unique_ptr<Render::CWallpaper> wallpaper = nullptr;
    };

    /**
     * Queues a background switch for the given screen. The load happens on the loader
     * thread and the swap is applied by processPreparedSwitches once ready, so rendering
     * never blocks. Shared by playlists and the IPC control socket.
     */
    uint64_t requestBackgroundSwitch (
	const std::string& screen, const std::string& path,
	Render::TransitionMode transition = Render::TransitionMode_Fade,
	const std::string& comparisonToken = {}
    );
    std::string processComparisonCommand (const std::string& command);
    void cancelComparison ();
    /** Loader thread main loop */
    void switchWorkerMain ();
    /** Applies a background switch the loader thread finished preparing, if any */
    void processPreparedSwitches ();
    /**
     * Render-thread part of a background switch: media/fallback uploads, scene build and swap
     *
     * @return whether the switch succeeded
     */
    bool applyPreparedSwitch (PreparedSwitch& job);
    /** Marks a playlist item as failed so the next advancement skips it */
    void markPlaylistItemFailed (const std::string& screen, const std::string& path);
    /** Stops and joins the loader thread */
    void stopSwitchWorker ();
    /**
     * Creates the unix control socket used to switch wallpapers at runtime
     */
    void setupControlSocket ();
    /**
     * Accepts and executes pending commands on the control socket
     */
    void processControlSocket ();

    void initializePlaylists ();
    std::filesystem::path restorePlaylistPath (
	const std::string& screen, const ApplicationContext::PlaylistDefinition& definition,
	const std::filesystem::path& fallback
    );
    void savePlaylists () const;
    void updatePlaylists ();
    void advancePlaylist (
	const std::string& screen, ActivePlaylist& playlist, const std::chrono::steady_clock::time_point& now
    );
    bool selectNextCandidate (ActivePlaylist& playlist, std::size_t& outOrderIndex);
    bool preflightWallpaper (const std::string& path);
    std::vector<std::size_t> buildPlaylistOrder (const ApplicationContext::PlaylistDefinition& definition);
    void ensureBrowserForProject (const Project& project);
    bool makeAnyViewportCurrent () const;

    /** The application context that contains the current app settings */
    ApplicationContext& m_context;
    /** Listening unix socket for runtime wallpaper switching, -1 when unavailable */
    int m_controlSocket = -1;
    /** Path of the bound control socket so it can be unlinked on exit */
    std::string m_controlSocketPath {};
    /** Inode of the socket file this instance bound; exit only unlinks the path while it still matches */
    ino_t m_controlSocketInode = 0;
    /** Maps screens to backgrounds */
    std::map<std::string, ProjectUniquePtr> m_backgrounds {};
    std::map<std::string, ActivePlaylist> m_activePlaylists {};
    std::map<std::string, PlaylistState::State> m_restoredPlaylists {};

    std::unique_ptr<WallpaperEngine::Audio::Drivers::Detectors::AudioPlayingDetector> m_audioDetector = nullptr;
    std::unique_ptr<WallpaperEngine::Audio::AudioContext> m_audioContext = nullptr;
    std::unique_ptr<WallpaperEngine::Audio::Drivers::SDLAudioDriver> m_audioDriver = nullptr;
    std::unique_ptr<WallpaperEngine::Audio::Drivers::Recorders::PlaybackRecorder> m_audioRecorder = nullptr;
    std::unique_ptr<WallpaperEngine::Render::RenderContext> m_renderContext = nullptr;
    std::unique_ptr<WallpaperEngine::Render::Drivers::VideoDriver> m_videoDriver = nullptr;
    std::unique_ptr<WallpaperEngine::Render::Drivers::Detectors::FullScreenDetector> m_fullScreenDetector = nullptr;
    std::unique_ptr<WallpaperEngine::WebBrowser::WebBrowserContext> m_browserContext = nullptr;
    std::unique_ptr<WallpaperEngine::Media::MediaSource> m_mediaSource = nullptr;
    std::mt19937 m_playlistRng { std::random_device {}() };

    /** Loader thread that prepares background switches off the render thread */
    std::thread m_switchWorker {};
    /** Guards the switch queues, ids and the stop flag */
    std::mutex m_switchMutex {};
    /** Wakes the loader thread when work arrives or on shutdown */
    std::condition_variable m_switchCv {};
    /** Switches waiting to be loaded by the worker */
    std::deque<PreparedSwitch> m_switchRequests {};
    std::vector<std::shared_ptr<Render::TextureMipStream>> m_pendingTextureStreams {};
    /** Switches loaded by the worker, waiting to be applied on the render thread */
    std::deque<PreparedSwitch> m_switchResults {};
    /** Latest switch request id per screen, used to drop superseded work */
    std::map<std::string, uint64_t> m_latestSwitchIds {};
    /** Monotonic counter feeding PreparedSwitch::id */
    uint64_t m_switchIdCounter = 0;
    /** Set under m_switchMutex to make the loader thread exit */
    bool m_switchWorkerStop = false;
    bool m_buildingSwitch = false;
    std::string m_comparisonToken {};
    std::map<std::string, ComparisonScreen> m_comparisonScreens {};
    /** When set, malloc_trim runs at this point to return post-switch allocator slack to the OS */
    std::chrono::steady_clock::time_point m_pendingMallocTrim {};

    bool m_isPaused = false;
    bool m_screenShotTaken = false;
    uint32_t m_nextFrameScreenshot = 0;
    std::chrono::steady_clock::time_point m_pauseStart {};
    GLuint m_destinationFramebuffer = 0;
};
} // namespace WallpaperEngine::Application
