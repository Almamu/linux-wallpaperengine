#include "WallpaperApplication.h"

#include "Steam/FileSystem/FileSystem.h"
#include "WallpaperEngine/Application/ApplicationState.h"
#include "WallpaperEngine/Assets/AssetLoadException.h"
#include "WallpaperEngine/Audio/Drivers/Detectors/PulseAudioPlayingDetector.h"
#include "WallpaperEngine/Debug/RenderHealth.h"
#include "WallpaperEngine/FileSystem/Container.h"
#include "WallpaperEngine/Logging/Log.h"
#include "WallpaperEngine/Render/Drivers/VideoFactories.h"
#include "WallpaperEngine/Render/RenderContext.h"
#include "WallpaperEngine/Render/Wallpapers/CScene.h"
#include "WallpaperEngine/Render/Wallpapers/CVideo.h"
#include "WallpaperEngine/Scripting/JavaScriptCoreRuntime.h"
#include "WallpaperEngine/WebBrowser/WebBrowserContext.h"

#include "WallpaperEngine/Data/Dumpers/StringPrinter.h"
#include "WallpaperEngine/Data/Parsers/ProjectParser.h"
#include "WallpaperEngine/Data/Parsers/TextureParser.h"
#include "WallpaperEngine/Data/Model/Effect.h"
#include "WallpaperEngine/Data/Model/Material.h"
#include "WallpaperEngine/Data/Model/Model.h"
#include "WallpaperEngine/Data/Model/Object.h"
#include "WallpaperEngine/Data/Utils/BinaryReader.h"
#include "WallpaperEngine/Render/CTexture.h"
#include "WallpaperEngine/Render/CFBO.h"
#include "LoadingGLState.h"
#include "WallpaperEngine/Data/Utils/ScopeGuard.h"

#include "WallpaperEngine/Data/Model/Property.h"
#include "WallpaperEngine/Data/Model/Wallpaper.h"
#include "WallpaperEngine/Debugging/CallStack.h"
#include "WallpaperEngine/FileSystem/Adapters/MediaCover.h"
#include "WallpaperEngine/Media/DBusMediaSource.h"

#if DEMOMODE
#include "recording.h"
#endif /* DEMOMODE */

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <climits>
#include <cmath>
#include <condition_variable>
#include <deque>
#if defined(__GLIBC__)
#include <malloc.h>
#endif
#include <malloc.h>
#include <mutex>
#include <cstdlib>
#include <cstring>
#include <numeric>
#include <poll.h>
#include <sstream>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>
#include <thread>

#define FULLSCREEN_CHECK_WAIT_TIME 10000

float g_Time;
float g_TimeLast;
float g_Daytime;

using namespace WallpaperEngine::Assets;
using namespace WallpaperEngine::Application;
using namespace WallpaperEngine::Data::Model;
using namespace WallpaperEngine::FileSystem;

void CustomGLDebugCallback (
    GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam
) {
    if (severity != GL_DEBUG_SEVERITY_HIGH) {
	return;
    }

    sLog.error ("OpenGL error: ", message, ", type: ", type, ", id: ", id);

    std::vector<WallpaperEngine::Debugging::CallStack::CallInfo> callInfo;

    WallpaperEngine::Debugging::CallStack::GetCalls (callInfo);

    for (std::vector<WallpaperEngine::Debugging::CallStack::CallInfo>::size_type i = 0; i < callInfo.size (); ++i) {
	fprintf (
	    stderr, "[%3lu] %15lu: %s in %s\n", callInfo.size () - i, callInfo[i].offset, callInfo[i].function.c_str (),
	    callInfo[i].module.c_str ()
	);
    }
}

namespace {
/** Process start, as close as dynamic initialization gets to it; the baseline for startup.first_frame */
const auto sStartupBegin = std::chrono::steady_clock::now ();

/** Records how long a startup phase took. No-op unless WPE_HEALTH_REPORT is set */
void recordStartupPhase (const std::string& metric, const std::chrono::steady_clock::time_point start) {
    if (!WallpaperEngine::Debug::RenderHealth::enabled ()) {
	return;
    }

    const std::chrono::duration<double, std::milli> elapsed = std::chrono::steady_clock::now () - start;

    WallpaperEngine::Debug::RenderHealth::record (metric, std::to_string (elapsed.count ()) + " ms");
}
} // namespace

WallpaperApplication::WallpaperApplication (ApplicationContext& context) : m_context (context) {
    this->initializeSubsystems ();

    const auto projectLoadStart = std::chrono::steady_clock::now ();
    this->loadBackgrounds ();
    recordStartupPhase ("startup.project_load", projectLoadStart);

    this->setupProperties ();
    this->setupBrowser ();
    this->initializePlaylists ();
}

WallpaperApplication::~WallpaperApplication () {
    this->savePlaylists ();
    // A prepared result can own shared-context GL textures. Stop the producer and
    // destroy queued results while the render context is still current and alive.
    this->stopSwitchWorker ();
    this->makeAnyViewportCurrent ();
    this->releaseLockscreenFrames ();
    {
	std::lock_guard lock (this->m_switchMutex);
	this->m_switchRequests.clear ();
	this->m_switchResults.clear ();
    }

    this->cancelComparison ();

    // CEF-backed wallpapers must be destroyed while their browser and GL contexts still exist.
    if (this->m_renderContext) {
	this->m_renderContext.reset ();
    }
    Scripting::serviceJavaScriptCoreRuntime ();
    this->m_backgrounds.clear ();
    this->m_browserContext.reset ();
}

void WallpaperApplication::initializeSubsystems () {
    m_mediaSource = WallpaperEngine::Media::DBusMediaSource::createOrFallback (std::chrono::milliseconds (2000));
}

AssetLocatorUniquePtr WallpaperApplication::setupAssetLocator (const std::string& bg) const {
    auto container = std::make_unique<Container> ();

    const std::filesystem::path path = bg;

    container->registerAdapterFactory (std::make_unique<MediaCoverFactory> (*this->m_mediaSource));
    container->mount ("$mediaThumbnail", "$mediaThumbnail");
    container->mount (path, "/");

    // nearly every wallpaper packs its assets into "scene.pkg" or "gifscene.pkg", but older ones
    // name the package after the scene file instead, e.g. 869945315 ships "audiophile.pkg" and
    // could not resolve its own "/audiophile.json" at all
    std::error_code packageError;
    for (const auto& entry : std::filesystem::directory_iterator (path, packageError)) {
	if (entry.path ().extension () != ".pkg") {
	    continue;
	}

	try {
	    container->mount (entry.path (), "/");
	} catch (std::runtime_error&) { }
    }

    try {
	container->mount (this->m_context.settings.general.assets, "/");
    } catch (std::runtime_error&) {
	sLog.exception ("Cannot find a valid assets folder, resolved to ", this->m_context.settings.general.assets);
    }

    // Stock effects ship as self-contained containers: the assets root has no
    // materials/effects tree of its own, so `materials/effects/<name>.json` and
    // the effect's own shaders only resolve if each container is mounted too.
    // The directory name does not match the material name (watercaustics ->
    // caustics.json), so they cannot be resolved on demand from the path.
    std::error_code effectsError;
    std::vector<std::filesystem::path> textureFallbackRoots;
    for (const auto& effect : std::filesystem::directory_iterator (
	     this->m_context.settings.general.assets / "effects", effectsError
	 )) {
	if (effect.is_directory (effectsError) == false) {
	    continue;
	}

	try {
	    container->mount (effect.path (), "/");
	    textureFallbackRoots.emplace_back (
		std::filesystem::path ("effects") / effect.path ().filename () / "preview" / "materials"
	    );
	} catch (std::runtime_error&) { }
    }

    // mount the current directory as root
    try {
	container->mount (std::filesystem::current_path (), "/");
    } catch (std::runtime_error&) { }

    auto& vfs = container->getVFS ();

    //
    // Had to get a little creative with the effects to achieve the same bloom effect without any custom code
    // these virtual files are loaded by an image in the scene that takes current _rt_FullFrameBuffer and
    // applies the bloom effect to render it out to the screen
    //

    // add the effect file for screen bloom

    // add some model for the image element even if it's going to waste rendering cycles
    vfs.add (
	"effects/wpenginelinux/bloomeffect.json",
	{ { "name", "camerabloom_wpengine_linux" },
	  { "group", "wpengine_linux_camera" },
	  { "dependencies", JSON::array () },
	  {
	      "passes",
	      JSON::array (
		  { { { "material", "materials/util/downsample_quarter_bloom.json" },
		      { "target", "_rt_4FrameBuffer" },
		      { "bind", JSON::array ({ { { "name", "_rt_FullFrameBuffer" }, { "index", 0 } } }) } },
		    { { "material", "materials/util/downsample_eighth_blur_v.json" },
		      { "target", "_rt_8FrameBuffer" },
		      { "bind", JSON::array ({ { { "name", "_rt_4FrameBuffer" }, { "index", 0 } } }) } },
		    { { "material", "materials/util/blur_h_bloom.json" },
		      { "target", "_rt_Bloom" },
		      { "bind", JSON::array ({ { { "name", "_rt_8FrameBuffer" }, { "index", 0 } } }) } },
		    { { "material", "materials/util/combine.json" },
		      { "target", "_rt_FullFrameBuffer" },
		      { "bind",
			JSON::array (
			    { { { "name", "_rt_imageLayerComposite_-1_a" }, { "index", 0 } },
			      { { "name", "_rt_Bloom" }, { "index", 1 } } }
			) } } }
	      ),
	  } }
    );

    // Bloom combines the completed scene at output resolution, independently
    // of the authored canvas size, and must follow output resizes.
    vfs.add (
	"models/wpenginelinux.json", { { "material", "materials/wpenginelinux.json" }, { "fullscreen", true } }
    );

    vfs.add (
	"materials/wpenginelinux.json",
	{ { "passes",
	    JSON::array (
		{ { { "blending", "normal" },
		    { "cullmode", "nocull" },
		    { "depthtest", "disabled" },
		    { "depthwrite", "disabled" },
		    { "shader", "genericimage2" },
		    { "textures", JSON::array ({ "_rt_FullFrameBuffer" }) } } }
	    ) } }
    );

    vfs.add (
	"shaders/commands/copy.frag",
	"uniform sampler2D g_Texture0;\n"
	"in vec2 v_TexCoord;\n"
	"void main () {\n"
	"out_FragColor = texture (g_Texture0, v_TexCoord);\n"
	"}"
    );
    vfs.add (
	"shaders/commands/copy.vert",
	"in vec3 a_Position;\n"
	"in vec2 a_TexCoord;\n"
	"out vec2 v_TexCoord;\n"
	"void main () {\n"
	"gl_Position = vec4 (a_Position, 1.0);\n"
	"v_TexCoord = a_TexCoord;\n"
	"}"
    );

    std::ranges::sort (textureFallbackRoots);
    return std::make_unique<AssetLocator> (std::move (container), std::move (textureFallbackRoots));
}

void WallpaperApplication::loadBackgrounds () {
    if (this->m_context.settings.render.mode == ApplicationContext::NORMAL_WINDOW
	|| this->m_context.settings.render.mode == ApplicationContext::EXPLICIT_WINDOW) {
	auto path = this->m_context.settings.general.defaultBackground;

	if (this->m_context.settings.general.defaultPlaylist.has_value ()
	    && !this->m_context.settings.general.defaultPlaylist->items.empty ()) {
	    path = this->restorePlaylistPath (
		"default", *this->m_context.settings.general.defaultPlaylist,
		this->m_context.settings.general.defaultPlaylist->items.front ()
	    );
	}

	this->m_backgrounds["default"] = this->loadBackground (path);
	return;
    }

    for (const auto& [screen, path] : this->m_context.settings.general.screenBackgrounds) {
	// skip span group synthetic keys here, they're handled below
	if (screen.rfind ("span:", 0) == 0) {
	    continue;
	}
	// screens with no path should use the default
	auto background = path.empty () ? this->m_context.settings.general.defaultBackground : path;
	if (const auto playlist = this->m_context.settings.general.screenPlaylists.find (screen);
	    playlist != this->m_context.settings.general.screenPlaylists.end ()) {
	    background = this->restorePlaylistPath (screen, playlist->second, background);
	}
	this->m_backgrounds[screen] = this->loadBackground (background);
    }

    // Load one background per span group
    for (const auto& spanGroup : this->m_context.settings.general.spanGroups) {
	if (spanGroup.screens.empty ()) {
	    continue;
	}

	std::filesystem::path bgPath = spanGroup.background;
	if (bgPath.empty ()) {
	    bgPath = this->m_context.settings.general.defaultBackground;
	}

	// use the first screen's name as the group key for the loaded project
	const std::string groupKey = "span:" + spanGroup.screens.front ();
	this->m_backgrounds[groupKey] = this->loadBackground (bgPath);
    }
}

ProjectUniquePtr WallpaperApplication::loadProject (const std::string& bg) const {
    auto container = this->setupAssetLocator (bg);
    auto json = WallpaperEngine::Data::JSON::parseCompatible (container->readString ("project.json"), "project.json");

    return WallpaperEngine::Data::Parsers::ProjectParser::parse (json, std::move (container));
}

void WallpaperApplication::resetScreenshotState () {
    // when a background is loaded, reset the screenshot variables
    // this allows taking screenshots after a background changes
    // useful for playlists
    if (!this->m_context.settings.screenshot.take) {
	return;
    }

    this->m_screenshotStartFrame = this->m_videoDriver ? this->m_videoDriver->getFrameCounter () : 0;
    this->m_screenshotReadyDeadline = {};
    this->m_screenShotTaken = false;
}

ProjectUniquePtr WallpaperApplication::loadBackground (const std::string& bg) {
    auto project = this->loadProject (bg);

    this->resetScreenshotState ();

    return project;
}

std::vector<std::size_t>
WallpaperApplication::buildPlaylistOrder (const ApplicationContext::PlaylistDefinition& definition) {
    std::vector<std::size_t> order (definition.items.size ());
    std::iota (order.begin (), order.end (), 0);

    if (definition.settings.order == "random") {
	std::shuffle (order.begin (), order.end (), this->m_playlistRng);
    }

    return order;
}

std::filesystem::path WallpaperApplication::restorePlaylistPath (
    const std::string& screen, const ApplicationContext::PlaylistDefinition& definition,
    const std::filesystem::path& fallback
) {
    try {
	if (auto state = PlaylistState::restore (screen, definition, this->m_playlistRng, std::chrono::system_clock::now ())) {
	    const auto path = definition.items[state->order[state->orderIndex]];
	    this->m_restoredPlaylists.insert_or_assign (screen, std::move (*state));
	    return path;
	}
    } catch (const std::exception& error) {
	sLog.error ("Cannot restore playlist for ", screen, ": ", error.what ());
    }
    return fallback;
}

void WallpaperApplication::savePlaylists () const {
    const auto now = std::chrono::steady_clock::now ();
    const auto wallNow = std::chrono::system_clock::now ();
    for (const auto& [screen, playlist] : this->m_activePlaylists) {
	if (playlist.definition.settings.mode != "timer" || playlist.definition.items.size () < 2) continue;
	const auto reference = this->m_isPaused && !playlist.definition.settings.updateOnPause ? this->m_pauseStart : now;
	const double delaySeconds = std::max<uint32_t> (1, playlist.definition.settings.delayMinutes) * 60.0;
	const float elapsed = static_cast<float> (std::max (0.0, delaySeconds - std::chrono::duration<double> (playlist.nextSwitch - reference).count ()));
	try {
	    const auto current = this->m_backgrounds.find (screen);
	    if (current == this->m_backgrounds.end ()) continue;
	    const auto loadedPath = current->second->assetLocator->physicalPath ("project.json").parent_path ();
	    std::size_t loadedIndex = playlist.order.size ();
	    for (std::size_t i = 0; i < playlist.order.size (); i++) {
		std::error_code error;
		if (std::filesystem::equivalent (playlist.definition.items[playlist.order[i]], loadedPath, error)) {
		    loadedIndex = i;
		    if (i == playlist.orderIndex) break;
		}
	    }
	    // A requested switch may still be loading; checkpoint the project actually shown.
	    if (loadedIndex == playlist.order.size ()) continue;
	    PlaylistState::save (screen, playlist.definition, { playlist.order, loadedIndex, elapsed }, wallNow);
	} catch (const std::exception& error) {
	    sLog.error ("Cannot save playlist for ", screen, ": ", error.what ());
	}
    }
}

void WallpaperApplication::initializePlaylists () {
    const bool hasDefaultPlaylist = this->m_context.settings.general.defaultPlaylist.has_value ();
    const bool hasScreenPlaylists = !this->m_context.settings.general.screenPlaylists.empty ();

    if (!hasDefaultPlaylist && !hasScreenPlaylists) {
	return;
    }

    const auto now = std::chrono::steady_clock::now ();

    auto registerPlaylist = [this, now] (
				const std::string& key, const ApplicationContext::PlaylistDefinition& playlist,
				std::optional<std::filesystem::path> currentPath
			    ) {
	if (playlist.items.empty ()) {
	    return;
	}

	ActivePlaylist state;

	state.definition = playlist;
	float restoredElapsed = 0;
	const auto restored = this->m_restoredPlaylists.find (key);
	if (restored != this->m_restoredPlaylists.end ()) {
	    state.order = std::move (restored->second.order);
	    state.orderIndex = restored->second.orderIndex;
	    restoredElapsed = restored->second.elapsedSeconds;
	} else {
	    state.order = this->buildPlaylistOrder (playlist);
	}

	if (state.order.empty ()) {
	    return;
	}

	if (restored == this->m_restoredPlaylists.end () && currentPath.has_value ()) {
	    state.orderIndex = 0;

	    for (std::size_t i = 0; i < state.order.size (); i++) {
		if (playlist.items[state.order[i]] == currentPath.value ()) {
		    state.orderIndex = i;
		    break;
		}
	    }
	}

	const uint32_t delayMinutes = std::max<uint32_t> (1, state.definition.settings.delayMinutes);
	state.nextSwitch = now + std::chrono::duration_cast<std::chrono::steady_clock::duration> (
	    std::chrono::duration<double> (delayMinutes * 60.0 - restoredElapsed)
	);
	state.lastUpdate = now;

	this->m_activePlaylists.insert_or_assign (key, std::move (state));
    };

    if (hasDefaultPlaylist
	&& (this->m_context.settings.render.mode == ApplicationContext::NORMAL_WINDOW
	    || this->m_context.settings.render.mode == ApplicationContext::EXPLICIT_WINDOW)) {
	const auto& playlist = this->m_context.settings.general.defaultPlaylist.value ();
	const auto currentPath = playlist.items.empty ()
	    ? std::optional<std::filesystem::path> { this->m_context.settings.general.defaultBackground }
	    : std::optional<std::filesystem::path> { playlist.items.front () };
	registerPlaylist ("default", playlist, currentPath);
    }

    for (const auto& [screen, playlist] : this->m_context.settings.general.screenPlaylists) {
	const auto current = this->m_context.settings.general.screenBackgrounds.find (screen);
	const auto currentPath = current != this->m_context.settings.general.screenBackgrounds.end ()
	    ? std::optional<std::filesystem::path> { current->second }
	    : std::nullopt;
	registerPlaylist (screen, playlist, currentPath);
    }
    this->m_restoredPlaylists.clear ();
}

void WallpaperApplication::ensureBrowserForProject (const Project& project) {
    if (!project.wallpaper->is<Web> ()) {
	return;
    }

    if (!this->m_browserContext) {
	this->m_browserContext = std::make_unique<WebBrowser::WebBrowserContext> (*this);
    }
}

bool WallpaperApplication::makeAnyViewportCurrent () const {
    if (!this->m_renderContext) {
	return false;
    }

    const auto& viewports = this->m_renderContext->getOutput ().getViewports ();

    if (viewports.empty ()) {
	return false;
    }

    viewports.begin ()->second->makeCurrent ();
    return true;
}

bool WallpaperApplication::preflightWallpaper (const std::string& path) {
    try {
	// avoid mutating state, just ensure project.json parses
	auto container = this->setupAssetLocator (path);
	const auto json
	    = WallpaperEngine::Data::JSON::parseCompatible (container->readString ("project.json"), "project.json");
	if (!json.contains ("type") || !json.contains ("file")) {
	    sLog.error ("Preflight failed for ", path, ": missing required fields");
	    return false;
	}
	return true;
    } catch (const std::exception& e) {
	sLog.error ("Preflight failed for ", path, ": ", e.what ());
	return false;
    }
}

bool WallpaperApplication::selectNextCandidate (ActivePlaylist& playlist, std::size_t& outOrderIndex) {
    if (playlist.order.empty ()) {
	return false;
    }

    std::size_t attempts = 0;
    std::size_t candidateOrderIndex = outOrderIndex;

    while (attempts < playlist.order.size ()) {
	const auto candidateIndex = playlist.order[candidateOrderIndex];

	if (!playlist.failedIndices.contains (candidateIndex)) {
	    outOrderIndex = candidateOrderIndex;
	    return true;
	}

	attempts++;
	candidateOrderIndex = (candidateOrderIndex + 1) % playlist.order.size ();
    }

    return false;
}

namespace {
/**
 * Maps a transition name from the control socket to its mode. "random" maps to
 * TransitionMode_None as a sentinel the caller resolves to a random pick.
 */
std::optional<WallpaperEngine::Render::TransitionMode> parseTransitionName (const std::string& name) {
    using namespace WallpaperEngine::Render;
    static const std::map<std::string, TransitionMode> NAMES = {
	{ "random", TransitionMode_None },	{ "fade", TransitionMode_Fade },
	{ "wipeleft", TransitionMode_WipeLeft }, { "wiperight", TransitionMode_WipeRight },
	{ "wipeup", TransitionMode_WipeUp },	{ "wipedown", TransitionMode_WipeDown },
	{ "disc", TransitionMode_Disc },	{ "stripes", TransitionMode_Stripes },
	{ "pixelate", TransitionMode_Pixelate }, { "honeycomb", TransitionMode_Honeycomb },
	{ "wipediag", TransitionMode_WipeDiag }, { "clock", TransitionMode_Clock },
	{ "iris", TransitionMode_Iris },	{ "checkerboard", TransitionMode_Checkerboard },
	{ "blinds", TransitionMode_Blinds },	{ "split", TransitionMode_Split },
	{ "voronoi", TransitionMode_Voronoi },	{ "noise", TransitionMode_Noise },
	{ "dots", TransitionMode_Dots },	{ "inksplash", TransitionMode_InkSplash },
    };

    const auto it = NAMES.find (name);
    return it != NAMES.end () ? std::optional (it->second) : std::nullopt;
}
} // namespace

void WallpaperApplication::setupControlSocket () {
    const char* socketOverride = getenv ("WPE_CONTROL_SOCKET");
    const char* runtimeDir = getenv ("XDG_RUNTIME_DIR");
    this->m_controlSocketPath = socketOverride != nullptr && socketOverride[0] != '\0'
	? socketOverride
	: std::string (runtimeDir != nullptr ? runtimeDir : "/tmp") + "/linux-wallpaperengine.sock";

    if (this->m_controlSocketPath.size () >= sizeof (sockaddr_un::sun_path)) {
	sLog.error ("Control socket path is too long: ", this->m_controlSocketPath);
	return;
    }

    this->m_controlSocket = socket (AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);

    if (this->m_controlSocket < 0) {
	sLog.error ("Cannot create control socket: ", strerror (errno));
	return;
    }

    // a previous instance (or a crashed one) may have left the socket file behind
    unlink (this->m_controlSocketPath.c_str ());

    sockaddr_un address {};
    address.sun_family = AF_UNIX;
    strncpy (address.sun_path, this->m_controlSocketPath.c_str (), sizeof (address.sun_path) - 1);

    if (bind (this->m_controlSocket, reinterpret_cast<sockaddr*> (&address), sizeof (address)) < 0
	|| listen (this->m_controlSocket, 4) < 0) {
	sLog.error ("Cannot bind control socket ", this->m_controlSocketPath, ": ", strerror (errno));
	close (this->m_controlSocket);
	this->m_controlSocket = -1;
	return;
    }

    // remember which filesystem entry we created; a newer instance may replace
    // it (unlink + rebind), in which case the exit path must leave it alone
    struct stat info {};
    if (stat (this->m_controlSocketPath.c_str (), &info) == 0) {
	this->m_controlSocketInode = info.st_ino;
    }

    sLog.out ("Control socket listening on ", this->m_controlSocketPath);
}

void WallpaperApplication::processControlSocket () {
    if (this->m_controlSocket < 0) {
	return;
    }

    // handle every pending connection; each carries a single command line
    while (true) {
	const int client = accept4 (this->m_controlSocket, nullptr, nullptr, SOCK_CLOEXEC);

	if (client < 0) {
	    return;
	}

	// the client writes right after connecting, but give it a small grace period
	pollfd pfd { .fd = client, .events = POLLIN, .revents = 0 };
	std::string command;

	while (poll (&pfd, 1, 100) > 0 && (pfd.revents & POLLIN) != 0) {
	    char buffer [4096];
	    const ssize_t bytes = read (client, buffer, sizeof (buffer));

	    if (bytes <= 0) {
		break;
	    }

	    command.append (buffer, bytes);

	    if (command.find ('\n') != std::string::npos) {
		break;
	    }
	}

	if (const auto newline = command.find ('\n'); newline != std::string::npos) {
	    command.resize (newline);
	}

	std::string reply
	    = "err invalid command, expected: switch <screen> [transition] <path> | prop <screen> <key> <value> | language [locale] | comparison <prepare|status|commit|cancel> <token>\n";

	if (command.starts_with ("comparison ")) {
	    try {
		reply = this->processComparisonCommand (command.substr (11));
	    } catch (const std::exception& error) {
		reply = "err " + std::string (error.what ()) + "\n";
	    }
	}

	if (command == "lockscreen-stream" || command.starts_with ("lockscreen-stream ")) {
	    const char* runtime = std::getenv ("XDG_RUNTIME_DIR");
	    if (!runtime || runtime[0] == '\0') {
		reply = "err XDG_RUNTIME_DIR is required for lockscreen frames\n";
	    } else if (!this->m_lockscreenStream.renew (
		           command.size () > 18 ? command.substr (18) : "", std::chrono::steady_clock::now ())) {
		reply = "err expected: lockscreen-stream <fps: 0..60>\n";
	    } else {
		reply = "ok\n";
	    }
	}

	if (command == "language" || command.starts_with ("language ")) {
	    if (command.starts_with ("language ")) {
		this->m_context.setLanguage (command.substr (9));
		for (const auto& [screen, wallpaper] : this->m_renderContext->getWallpapers ()) {
		    if (auto* scene = dynamic_cast<Render::Wallpapers::CScene*> (wallpaper.get ()))
			scene->getScriptEngine ().dispatchGeneralSettings ();
		}
	    }
	    reply = "ok " + this->m_context.getLanguage () + "\n";
	}

	// prop <screen> <key> <value> updates scene values or built-in video color controls.
	// Scene values fire applyUserProperties. Properties that gate scene structure
	// (visibility conditions, effect toggles) need a rebuild, which this does NOT do yet.
	if (command.rfind ("prop ", 0) == 0) {
	    const auto body = command.substr (5);
	    const auto screenEnd = body.find (' ');
	    const auto keyEnd = body.find (' ', screenEnd + 1);

	    if (screenEnd == std::string::npos || keyEnd == std::string::npos) {
		reply = "err expected: prop <screen> <key> <value>\n";
	    } else {
		const auto screen = body.substr (0, screenEnd);
		const auto key = body.substr (screenEnd + 1, keyEnd - screenEnd - 1);
		const auto value = body.substr (keyEnd + 1);

		const auto& wallpapers
		    = this->m_renderContext != nullptr ? this->m_renderContext->getWallpapers ()
						       : std::map<std::string, std::shared_ptr<Render::CWallpaper>> {};
		const auto wpIt = wallpapers.find (screen);

		if (wpIt == wallpapers.end ()) {
		    reply = "err unknown screen " + screen + "\n";
		} else if (auto* video = dynamic_cast<Render::Wallpapers::CVideo*> (wpIt->second.get ())) {
		    reply = video->setColorProperty (key, value) ? "ok\n" : "err unknown property " + key + "\n";
		} else if (auto* scene = dynamic_cast<Render::Wallpapers::CScene*> (wpIt->second.get ());
			   scene == nullptr) {
		    reply = "err not a scene wallpaper on " + screen + "\n";
		} else {
		    // Scene::project is a reference member, so it stays mutable through the const getter
		    auto& project = scene->getScene ().project;
		    const auto propertyIt = project.properties.find (key);

		    if (propertyIt == project.properties.end ()) {
			reply = "err unknown property " + key + "\n";
		    } else {
			propertyIt->second->update (value, DynamicValue::UpdateSource::User);
			scene->getScriptEngine ().dispatchUserProperty (key, *propertyIt->second);
			reply = "ok\n";
		    }
		}
	    }
	}

	// switch <screen> [transition] <path> - path may contain spaces, so the optional
	// transition sits before it where it can be recognized unambiguously
	if (command.rfind ("switch ", 0) == 0) {
	    auto body = command.substr (7);

	    if (const auto separator = body.find (' '); separator != std::string::npos) {
		const auto screen = body.substr (0, separator);
		auto path = body.substr (separator + 1);
		auto transition = Render::TransitionMode_Fade;

		if (const auto transitionEnd = path.find (' '); transitionEnd != std::string::npos) {
		    if (const auto parsed = parseTransitionName (path.substr (0, transitionEnd));
			parsed.has_value ()) {
			transition = parsed.value ();
			path = path.substr (transitionEnd + 1);
		    }
		}

		if (transition == Render::TransitionMode_None) {
		    // "random" resolves here so the whole animation set stays in one place
		    transition = static_cast<Render::TransitionMode> (std::uniform_int_distribution<int> (
			Render::TransitionMode_Fade, Render::TransitionMode_Last
		    ) (this->m_playlistRng));
		}

		if (this->m_renderContext == nullptr
		    || !this->m_renderContext->getWallpapers ().contains (screen)) {
		    reply = "err unknown screen " + screen + "\n";
		} else {
		    // the load happens on the loader thread; failures only show up in the
		    // log once the prepared switch is applied
		    this->requestBackgroundSwitch (screen, path, transition);
		    reply = "ok\n";
		}
	    }
	}

	// memstats - glibc heap counters over the socket so leak hunts can run without ptrace
	if (command == "memstats") {
	    const auto info = mallinfo2 ();
	    const auto textures = this->m_renderContext->getTextureCacheStats ();
	    reply = "ok inuse=" + std::to_string (info.uordblks) + " free=" + std::to_string (info.fordblks)
		+ " arena=" + std::to_string (info.arena) + " mmap=" + std::to_string (info.hblkhd)
		+ " tex_entries=" + std::to_string (textures.entries)
		+ " tex_bytes=" + std::to_string (textures.approximateBytes)
		+ " tex_ref_entries=" + std::to_string (textures.referencedEntries)
		+ " tex_ref_bytes=" + std::to_string (textures.referencedBytes)
		+ " fbo_live=" + std::to_string (Render::CFBO::getLiveCount ())
		+ " fbo_bytes=" + std::to_string (Render::CFBO::getLiveGpuBytes ()) + "\n";
	}

	if (command == "fbostats") {
	    reply = "ok " + Render::CFBO::getLiveDebugSummary () + "\n";
	}

	if (send (client, reply.c_str (), reply.size (), MSG_NOSIGNAL | MSG_DONTWAIT) < 0) {
	    sLog.error ("Cannot write control socket reply: ", strerror (errno));
	}

	close (client);
    }
}

namespace {
/** Engine-generated or media-bound texture names the switch worker must not pre-parse */
bool isSpecialTexture (const std::string& name) {
    return name.empty () || name.starts_with ("_rt_") || name.starts_with ("$");
}

void collectTextureNames (const TextureMap& textures, std::set<std::string>& out) {
    for (const auto& [slot, name] : textures) {
	if (!isSpecialTexture (name)) {
	    out.insert (name);
	}
    }
}

void collectMaterialTextures (const Material& material, std::set<std::string>& out, const Properties* properties = nullptr) {
    for (const auto& pass : material.passes) {
	TextureMap required = pass->textures;
	if (properties) {
	    for (const auto& [index, entry] : pass->usertextures) {
		const auto property = properties->find (entry);
		const std::string selection = property == properties->end () ? entry
		    : property->second->getType () == DynamicValue::String ? property->second->getString () : "";
		if (std::filesystem::path (selection).is_absolute ()) required.erase (index);
	    }
	}
	collectTextureNames (required, out);
	collectTextureNames (pass->usertextures, out);
    }
}

void collectImageEffectTextures (const std::vector<ImageEffectUniquePtr>& effects, std::set<std::string>& out) {
    for (const auto& imageEffect : effects) {
	for (const auto& passOverride : imageEffect->passOverrides) {
	    collectTextureNames (passOverride->textures, out);
	    collectTextureNames (passOverride->usertextures, out);
	}

	if (imageEffect->effect == nullptr) {
	    continue;
	}

	for (const auto& pass : imageEffect->effect->passes) {
	    if (pass->material.has_value ()) {
		collectMaterialTextures (**pass->material, out);
	    }

	    collectTextureNames (pass->binds, out);
	}
    }
}

/**
 * Walks a scene's data model gathering every texture file its materials and effects
 * reference so the switch worker can parse them off the render thread. Non-scene
 * wallpapers (video, web) load no textures through the texture cache.
 */
std::set<std::string> collectProjectTextures (const Project& project) {
    std::set<std::string> result {};

    if (!project.wallpaper->is<Scene> ()) {
	return result;
    }

    for (const auto& material : project.wallpaper->as<Scene> ()->precachedMaterials) {
	if (material->passes.empty ()) continue;
	collectTextureNames (material->passes.front ()->textures, result);
	collectTextureNames (material->passes.front ()->usertextures, result);
    }

    for (const auto& object : project.wallpaper->as<Scene> ()->objects) {
	if (object->is<Image> ()) {
	    const auto* image = object->as<Image> ();

	    if (image->model != nullptr && image->model->material != nullptr) {
		collectMaterialTextures (*image->model->material, result, &project.properties);
	    }
	    if (image->model != nullptr && image->model->puppetMesh.has_value ()) {
		for (const auto& descriptor : image->model->puppetMesh->clippingDescriptors) {
		    result.insert (descriptor.maskAsset);
		}
	    }

	    collectImageEffectTextures (image->effects, result);
	} else if (object->is<Model3D> ()) {
	    for (const auto& material : object->as<Model3D> ()->materials) {
		if (material != nullptr) {
		    collectMaterialTextures (*material, result, &project.properties);
		}
	    }
	} else if (object->is<Particle> ()) {
	    const auto* particle = object->as<Particle> ();

	    if (particle->material != nullptr && particle->material->material != nullptr) {
		collectMaterialTextures (*particle->material->material, result, &project.properties);
	    }
	} else if (object->is<Text> ()) {
	    collectImageEffectTextures (object->as<Text> ()->effects, result);
	}
    }

    return result;
}

/**
 * Reads every texture a project references. Switches can decode only the required
 * startup mip of eligible textures; ordinary startup still prepares every mip.
 * Parse failures fall back to loading synchronously on the render thread.
 */
std::vector<std::pair<std::string, WallpaperEngine::Data::Assets::TextureUniquePtr>>
parseProjectTextures (const Project& project, const bool initialMipOnly = false) {
    std::vector<std::pair<std::string, WallpaperEngine::Data::Assets::TextureUniquePtr>> textures {};

    for (const auto& name : collectProjectTextures (project)) {
	try {
	    textures.emplace_back (
		name, WallpaperEngine::Data::Parsers::TextureParser::load (*project.assetLocator, name, initialMipOnly)
	    );
	} catch (const std::exception&) {
	    // ignored, the render thread falls back to loading it synchronously
	}
    }

    // Batch the required image-format decoding across the wallpaper. Optional switch
    // mips finish on this same loader worker after its temporary textures are published.
    std::vector<WallpaperEngine::Data::Assets::Texture*> decodable;
    decodable.reserve (textures.size ());

    for (const auto& [name, texture] : textures) {
	decodable.emplace_back (texture.get ());
    }

    WallpaperEngine::Data::Parsers::TextureParser::decodeMipmaps (decodable, initialMipOnly);

    return textures;
}

/** Finishes queued uploads and releases the worker's shared GL context on every exit path. */
class BuildContextGuard {
public:
    explicit BuildContextGuard (WallpaperEngine::Render::Drivers::VideoDriver& driver) : m_driver (driver) { }
    ~BuildContextGuard () {
	// The render context must never observe a partially uploaded shared texture.
	// Waiting here blocks only the loader thread, not frame dispatch.
	glFinish ();
	m_driver.releaseBuildContext ();
    }

    BuildContextGuard (const BuildContextGuard&) = delete;
    BuildContextGuard& operator= (const BuildContextGuard&) = delete;

private:
    WallpaperEngine::Render::Drivers::VideoDriver& m_driver;
};
} // namespace

void WallpaperApplication::cancelComparison () {
    {
	std::lock_guard lock (this->m_switchMutex);
	for (const auto& [screen, candidate] : this->m_comparisonScreens) {
	    if (this->m_latestSwitchIds[screen] == candidate.id)
		this->m_latestSwitchIds[screen] = ++this->m_switchIdCounter;
	}
    }
    if (!this->m_comparisonScreens.empty ()) this->makeAnyViewportCurrent ();
    this->m_comparisonScreens.clear ();
    this->m_comparisonToken.clear ();
}

std::string WallpaperApplication::processComparisonCommand (const std::string& command) {
    std::istringstream input (command);
    std::string action, token;
    if (!(input >> action >> token)) return "err expected comparison action and token\n";

    if (action == "prepare") {
	std::string screen, path;
	input >> screen;
	std::getline (input >> std::ws, path);
	if (screen.empty () || path.empty ()) return "err expected comparison prepare <token> <screen> <path>\n";
	if (!this->m_renderContext || !this->m_renderContext->getWallpapers ().contains (screen))
	    return "err unknown screen " + screen + "\n";
	if (this->m_comparisonToken != token) {
	    this->cancelComparison ();
	    this->m_comparisonToken = token;
	}
	const auto id = this->requestBackgroundSwitch (screen, path, Render::TransitionMode_None, token);
	this->makeAnyViewportCurrent ();
	this->m_comparisonScreens.erase (screen);
	this->m_comparisonScreens.emplace (screen, ComparisonScreen { .id = id, .path = path });
	return "ok\n";
    }

    if (token != this->m_comparisonToken || this->m_comparisonScreens.empty ())
	return "err unknown or superseded comparison\n";

    if (action == "status") {
	JSON screens = JSON::object ();
	for (const auto& [screen, candidate] : this->m_comparisonScreens) {
	    screens[screen] = { { "id", candidate.id }, { "path", candidate.path },
		{ "state", candidate.state }, { "error", candidate.error } };
	}
	return "ok " + JSON ({ { "token", token }, { "screens", std::move (screens) } }).dump () + "\n";
    }
    if (action == "cancel") {
	this->cancelComparison ();
	return "ok\n";
    }
    if (action != "commit") return "err unknown comparison action\n";

    for (const auto& [screen, candidate] : this->m_comparisonScreens) {
	if (candidate.state != "ready" && candidate.state != "committed")
	    return "err comparison " + screen + " is " + candidate.state + ": " + candidate.error + "\n";
    }
    if (!this->makeAnyViewportCurrent ()) return "err no active viewport\n";
    for (auto& [screen, candidate] : this->m_comparisonScreens) {
	if (candidate.state == "committed") continue;
	auto* scene = static_cast<Render::Wallpapers::CScene*> (candidate.wallpaper.get ());
	scene->finishLoading ();
	if (this->m_isPaused) candidate.wallpaper->setPause (true);
	std::shared_ptr<void> previousProject;
	if (const auto it = this->m_backgrounds.find (screen); it != this->m_backgrounds.end ())
	    previousProject = std::shared_ptr<Project> (std::move (it->second));
	this->m_backgrounds[screen] = std::move (candidate.project);
	this->m_renderContext->setWallpaper (
	    screen, std::move (candidate.wallpaper), std::move (previousProject), Render::TransitionMode_None
	);
	this->m_context.settings.general.screenBackgrounds[screen] = candidate.path;
	candidate.state = "committed";
    }
    this->m_pendingMallocTrim = std::chrono::steady_clock::now () + std::chrono::seconds (5);
    return "ok\n";
}

uint64_t WallpaperApplication::requestBackgroundSwitch (
    const std::string& screen, const std::string& path, const Render::TransitionMode transition,
    const std::string& comparisonToken
) {
    if (comparisonToken.empty () && this->m_comparisonScreens.contains (screen)) this->cancelComparison ();
    uint64_t id;
    {
	std::lock_guard lock (this->m_switchMutex);

	if (!this->m_switchWorker.joinable ()) {
	    this->m_switchWorkerStop = false;
	    this->m_switchWorker = std::thread (&WallpaperApplication::switchWorkerMain, this);
	}

	PreparedSwitch job {};
	job.id = id = ++this->m_switchIdCounter;
	job.screen = screen;
	job.path = path;
	job.transition = transition;
	job.requested = std::chrono::steady_clock::now ();
	job.comparisonToken = comparisonToken;
	job.outputSize = this->m_renderContext->getTextureQualityOutputSize (screen);
	this->m_latestSwitchIds[screen] = job.id;
	this->m_switchRequests.emplace_back (std::move (job));
    }

    this->m_switchCv.notify_one ();
    return id;
}

void WallpaperApplication::queueTextureStreams (std::vector<std::shared_ptr<Render::TextureMipStream>> streams) {
    if (streams.empty ()) return;
    try {
	std::lock_guard lock (m_switchMutex);
	if (!m_switchWorker.joinable ()) {
	    m_switchWorkerStop = false;
	    m_switchWorker = std::thread (&WallpaperApplication::switchWorkerMain, this);
	}
	m_pendingTextureStreams.insert (m_pendingTextureStreams.end (), streams.begin (), streams.end ());
    } catch (const std::exception& error) {
	sLog.error ("Cannot queue material texture decoding: ", error.what ());
	Render::CTexture::completeMipStreams (streams);
	return;
    }
    m_switchCv.notify_one ();
}

void WallpaperApplication::switchWorkerMain () {
    while (true) {
	PreparedSwitch job {};
	std::vector<std::shared_ptr<Render::CTexture::MipStream>> mipStreams;

	{
	    std::unique_lock lock (this->m_switchMutex);
	    this->m_switchCv.wait (lock, [this] {
		return this->m_switchWorkerStop || !this->m_switchRequests.empty () || !m_pendingTextureStreams.empty ();
	    });

	    if (this->m_switchWorkerStop) {
		return;
	    }

	    if (m_switchRequests.empty ()) {
		mipStreams.swap (m_pendingTextureStreams);
		lock.unlock ();
		Render::CTexture::completeMipStreams (mipStreams);
		continue;
	    }
	    job = std::move (this->m_switchRequests.front ());
	    this->m_switchRequests.pop_front ();

	    // a newer request for the same screen already superseded this one
	    if (this->m_latestSwitchIds[job.screen] != job.id) {
		continue;
	    }
	}

	const auto prepareStart = std::chrono::steady_clock::now ();
	auto projectMs = std::chrono::duration<double, std::milli>::zero ();
	auto textureMs = projectMs;

	try {
	    job.project = this->loadProject (job.path);
	    job.textureReduction = this->m_renderContext->getTextureReduction (*job.project->wallpaper, job.outputSize);
	    if (!job.comparisonToken.empty ()) {
		if (!job.project->wallpaper->is<Scene> ())
		    throw std::runtime_error ("Synchronized comparison currently requires a scene wallpaper");
		for (auto& source : job.project->wallpaper->as<Scene> ()->camera.paths) source.queueMode = "sequence";
	    }
	    projectMs = std::chrono::steady_clock::now () - prepareStart;

	    // Read, decompress and decode textures away from the render loop first.
	    const auto textureStart = std::chrono::steady_clock::now ();
	    job.textures = parseProjectTextures (*job.project, true);
	    textureMs = std::chrono::steady_clock::now () - textureStart;

	    {
		std::lock_guard lock (this->m_switchMutex);
		// Avoid spending GPU time on a selection the user has already replaced.
		if (this->m_latestSwitchIds[job.screen] != job.id) {
		    continue;
		}
	    }

	    // Static GL textures are shareable between EGL contexts, so upload them here.
	    // Video textures initialize mpv and stay in the main-thread fallback list.
	    if (!job.textures.empty () && this->m_videoDriver != nullptr && this->m_renderContext != nullptr
		&& this->m_videoDriver->makeBuildContextCurrent ()) {
		BuildContextGuard buildContext (*this->m_videoDriver);
		std::vector<std::pair<std::string, Data::Assets::TextureUniquePtr>> fallbackTextures {};
		fallbackTextures.reserve (job.textures.size ());

		for (auto& [name, texture] : job.textures) {
		    const bool mediaBacked = texture->isVideoMp4 || (texture->flags & TextureFlags_Video);

		    if (mediaBacked) {
			fallbackTextures.emplace_back (std::move (name), std::move (texture));
			continue;
		    }

		    try {
			job.readyTextures.emplace_back (
			    std::move (name),
			    Render::CTexture::createStreaming (
				*this->m_renderContext, std::move (texture), job.textureReduction, mipStreams
			    )
			);
		    } catch (const std::exception& e) {
			// resolveTexture will load this asset normally during the scene build.
			sLog.error ("Async texture upload failed for ", name, ": ", e.what ());
		    }
		}

		job.textures = std::move (fallbackTextures);
	    } else {
		// Drivers without a shared build context publish CPU-ready textures.
		// Keep their remaining image decoding off the render thread too.
		std::vector<Data::Assets::Texture*> decodable;
		for (const auto& [name, texture] : job.textures) decodable.push_back (texture.get ());
		Data::Parsers::TextureParser::decodeMipmaps (decodable);
		textureMs = std::chrono::steady_clock::now () - textureStart;
	    }
	} catch (const std::exception& e) {
	    job.error = e.what ();
	}

	const std::chrono::duration<double, std::milli> totalMs = std::chrono::steady_clock::now () - prepareStart;
	job.projectMs = projectMs.count ();
	job.textureMs = textureMs.count ();
	job.uploadMs = (totalMs - projectMs - textureMs).count ();
	if (Debug::RenderHealth::enabled ()) {
	    std::ostringstream detail;
	    detail << totalMs.count () << " ms (project " << projectMs.count () << ", read+decode "
		   << textureMs.count () << ", upload " << (totalMs - projectMs - textureMs).count () << "), "
		   << (job.textures.size () + job.readyTextures.size ()) << " textures";
	    Debug::RenderHealth::record ("switch.prepare", detail.str ());
	}

	{
	    std::lock_guard lock (this->m_switchMutex);
	    this->m_switchResults.emplace_back (std::move (job));
	}
	// Optional mips remain CPU-only until the render thread observes the complete batch.
	Render::CTexture::completeMipStreams (mipStreams);
    }
}

void WallpaperApplication::processPreparedSwitches () {
#if defined(__GLIBC__)
    // a switch's peak allocations (decode staging, old+new project alive through the
    // crossfade) leave hundreds of MB of freed-but-retained chunks in the glibc arena;
    // once the fade is done and teardown has run, hand those pages back to the OS
    if (this->m_pendingMallocTrim != std::chrono::steady_clock::time_point {}
	&& std::chrono::steady_clock::now () >= this->m_pendingMallocTrim) {
	this->m_pendingMallocTrim = {};
	malloc_trim (0);
    }
#endif

    while (true) {
	PreparedSwitch job {};

	{
	    std::lock_guard lock (this->m_switchMutex);

	    if (this->m_switchResults.empty ()) {
		return;
	    }

	    job = std::move (this->m_switchResults.front ());
	    this->m_switchResults.pop_front ();

	    // a newer request for the same screen is already on the way, drop this one
	    if (this->m_latestSwitchIds[job.screen] != job.id) {
		// readyTextures owns GL objects. Ensure its destructor runs with a context.
		this->makeAnyViewportCurrent ();
		continue;
	    }
	}

	if (!job.error.empty ()) {
	    if (!job.readyTextures.empty ()) {
		this->makeAnyViewportCurrent ();
	    }
	    sLog.error ("Failed to load wallpaper ", job.path, " for ", job.screen, ": ", job.error);
	    if (!job.comparisonToken.empty () && job.comparisonToken == this->m_comparisonToken) {
		auto& candidate = this->m_comparisonScreens.at (job.screen);
		candidate.state = "error";
		candidate.error = job.error;
	    }
	    this->markPlaylistItemFailed (job.screen, job.path);
	    continue;
	}

	if (!this->applyPreparedSwitch (job)) {
	    this->markPlaylistItemFailed (job.screen, job.path);
	}

	// schedule an allocator trim for after the crossfade ends and the previous
	// project has been torn down (transitions run well under this)
	this->m_pendingMallocTrim = std::chrono::steady_clock::now () + std::chrono::seconds (5);
    }
}

bool WallpaperApplication::applyPreparedSwitch (PreparedSwitch& job) {
    // GL/QuickJS construction stays on the render thread, interleaved with ordinary frames.
    const auto applyStart = std::chrono::steady_clock::now ();
    const auto requested = job.requested;
    auto buildMs = std::chrono::duration<double, std::milli>::zero ();

    struct CancelledSwitch {};
    try {
	if (!this->makeAnyViewportCurrent ()) {
	    sLog.error ("Cannot switch wallpaper on ", job.screen, ": no active viewport");
	    throw std::runtime_error ("No viewport available");
	}

	this->m_buildingSwitch = true;
	float liveTime = g_Time, liveTimeLast = g_TimeLast;
	const float loadingTime = this->m_videoDriver->getRenderTime ();
	g_Time = g_TimeLast = loadingTime;
	// Uploads bind index buffers before objects have their own VAOs. Do not let
	// those binds overwrite a cached outgoing model/puppet VAO's element buffer.
	GLint previousVao = 0;
	glGetIntegerv (GL_VERTEX_ARRAY_BINDING, &previousVao);
	GLuint loadingVao;
	glGenVertexArrays (1, &loadingVao);
	glBindVertexArray (loadingVao);
	Data::Utils::ScopeGuard finishPreparation ([&] {
	    // GLFW also clears at the start of the outer frame after publication.
	    // Leave that clear pointing at the output, never at the warmed scene.
	    glBindFramebuffer (GL_FRAMEBUFFER, this->getDestinationFramebuffer ());
	    glBindVertexArray (glIsVertexArray (previousVao) ? previousVao : 0);
	    glDeleteVertexArrays (1, &loadingVao);
	    g_Time = liveTime;
	    g_TimeLast = liveTimeLast;
	    this->m_buildingSwitch = false;
	});
	using Milliseconds = std::chrono::duration<double, std::milli>;
	using SwitchPacing = ApplicationContext::SwitchPacing;
	const auto pacing = this->m_context.settings.render.switchPacing;
	const int fps = std::max (1, this->m_context.settings.render.maximumFPS);
	const Milliseconds framePeriod (1000.0 / fps);
	// Fixed pacing gives loading at most a quarter of a requested frame, capped at 2 ms.
	// High-refresh outputs must not inherit an 8 ms / 60 Hz loading slice. Adaptive pacing
	// starts there and grows into the part of each frame the outgoing wallpaper leaves idle:
	// at a fixed 2 ms, Pokemon's ~0.9 s build took ~440 frames (7 s at 60 FPS).
	const Milliseconds minimumBudget (std::min (2.0, 250.0 / fps));
	Milliseconds loadingBudget = minimumBudget;
	Milliseconds largestBudget = minimumBudget;
	auto lastYield = std::chrono::steady_clock::now ();
	auto lastCheckpoint = lastYield;
	double longestUnitMs = 0.0;
	size_t yields = 0;
	// Outgoing frames drawn between loading slices: what pacing adds to the load time.
	Milliseconds pacingMs = Milliseconds::zero ();
	const auto cancelled = [&] {
	    std::lock_guard lock (this->m_switchMutex);
	    return !this->m_context.state.general.keepRunning || this->m_latestSwitchIds[job.screen] != job.id;
	};
	const auto checkpoint = [&] (bool canCancel) {
	    const auto now = std::chrono::steady_clock::now ();
	    const Milliseconds elapsed = now - lastYield;
	    // Adaptive slices are long, so yield before a unit like the one just finished would
	    // overrun the slice instead of after it. Off never yields: the outgoing wallpaper freezes.
	    const Milliseconds unit = pacing == SwitchPacing::Adaptive ? now - lastCheckpoint : Milliseconds::zero ();
	    lastCheckpoint = now;
	    if (pacing != SwitchPacing::Off && elapsed + unit >= loadingBudget) {
		longestUnitMs = std::max (longestUnitMs, elapsed.count ());
		LoadingGLState state;
		state.prepareOutgoingFrame (this->getDestinationFramebuffer (), loadingVao);
		g_Time = liveTime;
		g_TimeLast = liveTimeLast;
		const auto framesBefore = this->m_videoDriver->getFrameCounter ();
		const auto renderStart = std::chrono::steady_clock::now ();
		this->render ();
		liveTime = g_Time;
		liveTimeLast = g_TimeLast;
		g_Time = g_TimeLast = loadingTime;
		// Event dispatch may select another surface. All viewport rendering shares
		// this context; resolve a current surface again without retaining a raw pointer.
		this->makeAnyViewportCurrent ();
		lastYield = lastCheckpoint = std::chrono::steady_clock::now ();
		pacingMs += lastYield - renderStart;
		++yields;
		if (pacing == SwitchPacing::Adaptive) {
		    // The outgoing frame was on time if the driver still had to wait for its deadline.
		    // The next slice may then take the rest of a frame: the period minus this frame's
		    // own work and a 2 ms margin. If the outgoing wallpaper already exceeds the
		    // requested period, reserve a quarter of its measured work for loading instead
		    // of forcing every loading slice back to 1-2 ms. This bounds the added CPU
		    // cost while allowing progress on heavy web/3D wallpapers below the FPS cap.
		    const Milliseconds wait = this->m_videoDriver->getLastFrameWait ();
		    const Milliseconds work = Milliseconds (lastYield - renderStart) - wait;
		    const bool rendered = this->m_videoDriver->getFrameCounter () != framesBefore;
		    const Milliseconds available = wait > Milliseconds::zero ()
			? framePeriod - work - Milliseconds (2.0) : work * 0.25;
		    loadingBudget = rendered
			? std::clamp (available, minimumBudget, framePeriod * 0.75)
			: minimumBudget;
		    largestBudget = std::max (largestBudget, loadingBudget);
		}
	    }
	    if (canCancel && cancelled ()) throw CancelledSwitch {};
	};

	this->setupPropertiesForProject (*job.project);
	this->ensureBrowserForProject (*job.project);

	const auto scalingIt = this->m_context.settings.general.screenScalings.find (job.screen);
	const auto clampIt = this->m_context.settings.general.screenClamps.find (job.screen);
	const auto scaling = scalingIt != this->m_context.settings.general.screenScalings.end ()
	    ? scalingIt->second
	    : this->m_context.settings.render.window.scalingMode;
	const auto clamp = clampIt != this->m_context.settings.general.screenClamps.end ()
	    ? clampIt->second
	    : this->m_context.settings.render.window.clamp;

	if (this->m_renderContext) {
	    // Keep the current wallpaper/project pair untouched until the replacement is
	    // fully constructed. A failed CScene constructor must not leave the still-rendered
	    // wallpaper referring to a Project that was moved out and destroyed while unwinding.
	    auto& nextProject = *job.project;
	    const auto& assetLocator = *nextProject.assetLocator;
	    // Keep the upload snapshot in its own quality namespace. If the output changed,
	    // CScene selects the current factor and resolves a different key during construction.
	    // Shared-context uploads are already complete; cache insertion is only a pointer move.
	    // These are deliberately not held past the store: the outgoing wallpaper's textures
	    // are still referenced through the crossfade, so keeping both sets pinned over the
	    // scene build exhausts memory on 4K wallpapers. trim() may evict some of these and
	    // the build reloads them (texture.sync_load counts it) — that is the cheaper failure.
	    std::set<std::string> conditionalTextures;
	    for (auto& [name, texture] : job.readyTextures) {
		if (const auto compiled = std::dynamic_pointer_cast<const Render::CTexture> (texture);
		    compiled && compiled->getHeader ().variantSource) conditionalTextures.insert (name);
		this->m_renderContext->storeTexture (name, assetLocator, std::move (texture), job.textureReduction);
	    }

	    // Media textures and non-Wayland fallbacks still need main-context construction.
	    for (auto& [name, texture] : job.textures) {
		if (texture->variantSource) conditionalTextures.insert (name);
		this->m_renderContext->storeTexture (
		    name, assetLocator,
		    std::make_shared<Render::CTexture> (*this->m_renderContext, std::move (texture), job.textureReduction),
		    job.textureReduction
		);
		checkpoint (true);
	    }

	    // The prefetched base is unbound. Prepare this project's selected patches
	    // before scene construction and retain that binding until its passes own it.
	    std::vector<std::shared_ptr<const Render::TextureProvider>> preparedVariants;
	    if (!conditionalTextures.empty ()) {
		const auto quality = m_renderContext->getTextureReduction (
		    *nextProject.wallpaper, m_renderContext->getTextureQualityOutputSize (job.screen)
		);
		preparedVariants = m_renderContext->prepareTextureFallbacks (
		    conditionalTextures, assetLocator, nextProject.properties, quality
		);
	    }
	    // Eager unsupported bases keep the switch cache's ordinary eviction policy.
	    std::erase_if (preparedVariants, [] (const auto& provider) {
		const auto texture = std::dynamic_pointer_cast<const Render::CTexture> (provider);
		return !texture || !texture->getHeader ().variantSelectionPrepared;
	    });
	    const auto buildStart = std::chrono::steady_clock::now ();
	    std::unique_ptr<Render::CWallpaper> renderWallpaper;
	    if (nextProject.wallpaper->is<Scene> ()) {
		auto scene = std::make_unique<Render::Wallpapers::CScene> (
		    *nextProject.wallpaper, *this->m_renderContext, *this->m_audioContext,
		    scaling, clamp, job.screen, checkpoint
		);
		scene->prepareFirstFrame ();
		checkpoint (true);
		if (job.comparisonToken.empty ()) scene->finishLoading ();
		else scene->holdLoading ();
		renderWallpaper = std::move (scene);
	    } else {
		renderWallpaper = Render::CWallpaper::fromWallpaper (
		    *nextProject.wallpaper, *this->m_renderContext, *this->m_audioContext,
		    this->m_browserContext.get (), scaling, clamp, job.screen
		);
	    }
	    if (cancelled ()) throw CancelledSwitch {};
	    buildMs = std::chrono::steady_clock::now () - buildStart;
	    if (!job.comparisonToken.empty ()) {
		auto& candidate = this->m_comparisonScreens.at (job.screen);
		candidate.project = std::move (job.project);
		candidate.wallpaper = std::move (renderWallpaper);
		candidate.state = "ready";
		return true;
	    }

	    // A replacement built while fullscreen-paused must inherit the pause state.
	    if (this->m_isPaused) {
		renderWallpaper->setPause (true);
	    }

	    // The replacement is valid. Transfer ownership atomically with the render swap;
	    // the outgoing CWallpaper references the previous Project through the transition.
	    std::shared_ptr<void> previousProject;
	    if (const auto it = this->m_backgrounds.find (job.screen); it != this->m_backgrounds.end ()) {
		previousProject = std::shared_ptr<Project> (std::move (it->second));
	    }
	    this->m_backgrounds[job.screen] = std::move (job.project);

	    this->m_renderContext->setWallpaper (
		job.screen, std::move (renderWallpaper), std::move (previousProject), job.transition
	    );

	    const auto ready = std::chrono::steady_clock::now ();
	    const double totalMs = Milliseconds (ready - requested).count ();
	    const double applyMs = Milliseconds (ready - applyStart).count ();
	    const auto seconds = [] (double ms) { return static_cast<float> (ms / 1000.0); };
	    // Waiting (queued, or prepared but not yet picked up) is whatever the phases leave.
	    this->m_renderContext->setLoadTimes (
		job.screen, { seconds (totalMs), seconds (job.projectMs), seconds (job.textureMs), seconds (job.uploadMs),
			      seconds (applyMs - pacingMs.count ()), seconds (pacingMs.count ()),
			      seconds (totalMs - job.projectMs - job.textureMs - job.uploadMs - applyMs) }
	    );
	}

	this->m_context.settings.general.screenBackgrounds[job.screen] = job.path;

	if (Debug::RenderHealth::enabled ()) {
	    const auto now = std::chrono::steady_clock::now ();
	    const std::chrono::duration<double, std::milli> applyMs = now - applyStart;
	    const std::chrono::duration<double, std::milli> totalMs = now - requested;
	    std::ostringstream detail;
	    detail << "elapsed " << applyMs.count () << " ms (scene build/warmup " << buildMs.count ()
		   << "), yields " << yields << ", longest loading unit " << longestUnitMs
		   << " ms, largest slice " << largestBudget.count () << " ms, request to ready " << totalMs.count ()
		   << " ms";
	    Debug::RenderHealth::record ("switch.apply", detail.str ());
	}

	return true;
    } catch (const CancelledSwitch&) {
	// A newer request (or shutdown) superseded this candidate; it is not a bad playlist item.
	return true;
    } catch (const std::exception& e) {
	sLog.error ("Failed to switch wallpaper on ", job.screen, ": ", e.what ());
	if (!job.comparisonToken.empty () && job.comparisonToken == this->m_comparisonToken) {
	    const auto candidate = this->m_comparisonScreens.find (job.screen);
	    if (candidate != this->m_comparisonScreens.end () && candidate->second.id == job.id) {
		candidate->second.state = "error";
		candidate->second.error = e.what ();
	    }
	}
	return false;
    }
}

void WallpaperApplication::markPlaylistItemFailed (const std::string& screen, const std::string& path) {
    const auto it = this->m_activePlaylists.find (screen);

    if (it == this->m_activePlaylists.end ()) {
	return;
    }

    auto& playlist = it->second;
    const auto& items = playlist.definition.items;

    for (std::size_t index = 0; index < items.size (); index++) {
	if (items[index] == path) {
	    playlist.failedIndices.insert (index);
	    sLog.error ("Failed to load wallpaper for ", screen, ", will retry on next cycle");
	    break;
	}
    }
}

void WallpaperApplication::stopSwitchWorker () {
    {
	std::lock_guard lock (this->m_switchMutex);

	if (!this->m_switchWorker.joinable ()) {
	    return;
	}

	this->m_switchWorkerStop = true;
    }

    this->m_switchCv.notify_all ();
    this->m_switchWorker.join ();
    // Cache-held providers may outlive their scene. Leave no pending record nonterminal.
    Render::CTexture::completeMipStreams (m_pendingTextureStreams);
    m_pendingTextureStreams.clear ();
}

void WallpaperApplication::advancePlaylist (
    const std::string& screen, ActivePlaylist& playlist, const std::chrono::steady_clock::time_point& now
) {
    if (playlist.order.empty ()) {
	return;
    }

    playlist.orderIndex = (playlist.orderIndex + 1) % playlist.order.size ();

    if (playlist.orderIndex == 0 && playlist.definition.settings.order == "random") {
	std::shuffle (playlist.order.begin (), playlist.order.end (), this->m_playlistRng);
    }

    std::size_t candidateOrderIndex = playlist.orderIndex;

    if (!this->selectNextCandidate (playlist, candidateOrderIndex)) {
	sLog.error ("All playlist items failed for ", screen, ", keeping current wallpaper");
	const uint32_t delayMinutes = std::max<uint32_t> (1, playlist.definition.settings.delayMinutes);
	playlist.nextSwitch = now + std::chrono::minutes (delayMinutes);
	return;
    }

    const auto candidateIndex = playlist.order[candidateOrderIndex];
    const auto& candidatePath = playlist.definition.items[candidateIndex];

    if (!this->preflightWallpaper (candidatePath.string ())) {
	playlist.failedIndices.insert (candidateIndex);

	if (!this->selectNextCandidate (playlist, candidateOrderIndex)) {
	    sLog.error ("All playlist items failed for ", screen, ", keeping current wallpaper");
	    const uint32_t delayMinutes = std::max<uint32_t> (1, playlist.definition.settings.delayMinutes);
	    playlist.nextSwitch = now + std::chrono::minutes (delayMinutes);
	    return;
	}
    }

    playlist.orderIndex = candidateOrderIndex;
    const auto& nextPath = playlist.definition.items[playlist.order[playlist.orderIndex]];

    // the load happens on the loader thread; failures are reported back through
    // markPlaylistItemFailed so the next advancement skips the item
    this->requestBackgroundSwitch (screen, nextPath.string ());

    const uint32_t delayMinutes = std::max<uint32_t> (1, playlist.definition.settings.delayMinutes);
    playlist.nextSwitch = now + std::chrono::minutes (delayMinutes);
}

void WallpaperApplication::updatePlaylists () {
    if (this->m_activePlaylists.empty ()) {
	return;
    }

    const auto now = std::chrono::steady_clock::now ();

    for (auto& [screen, playlist] : this->m_activePlaylists) {
	playlist.lastUpdate = now;

	if (playlist.definition.settings.mode != "timer") {
	    continue;
	}

	if (playlist.definition.items.size () <= 1) {
	    continue;
	}

	if (now < playlist.nextSwitch) {
	    continue;
	}

	this->advancePlaylist (screen, playlist, now);
    }
}

void WallpaperApplication::setupPropertiesForProject (const Project& project) {
    // show properties if required
    for (const auto& [key, cur] : project.properties) {
	// update the value of the property
	auto override = this->m_context.settings.general.properties.find (key);

	if (override != this->m_context.settings.general.properties.end ()) {
	    sLog.out ("Applying override value for ", key);

	    try {
		cur->update (override->second, DynamicValue::UpdateSource::User);
	    } catch (const std::exception& error) {
		sLog.error ("Ignoring override value '", override->second, "' for ", key, ": ", error.what ());
	    }
	}

	if (this->m_context.settings.general.onlyListProperties) {
	    sLog.out (cur->dump ());
	}
    }
}

void WallpaperApplication::setupProperties () {
    for (const auto& [background, info] : this->m_backgrounds) {
	this->setupPropertiesForProject (*info);
    }
}

void WallpaperApplication::setupBrowser () {
    // CEF only renders web wallpapers; starting it for a scene costs startup time for nothing.
    // A later switch to a web wallpaper creates it on demand (ensureBrowserForProject).
    if (this->m_browserContext) {
	return;
    }

    for (const auto& [background, info] : this->m_backgrounds) {
	this->ensureBrowserForProject (*info);
    }
}

void WallpaperApplication::takeScreenshot (const std::filesystem::path& filename) const {
    const int width = this->m_renderContext->getOutput ().getFullWidth ();
    const int height = this->m_renderContext->getOutput ().getFullHeight ();
    const bool vflip = this->m_renderContext->getOutput ().renderVFlip ();
    const auto& wallpapers = this->m_renderContext->getWallpapers ();

    struct ViewportCapture {
	uint8_t* buffer;
	int readWidth;
	int readHeight;
	int vpWidth;
	int vpHeight;
	int xoffset;
	float ustart, uend, vstart, vend;
    };

    std::vector<ViewportCapture> captures;
    int currentXOffset = 0;

    for (const auto& [screen, viewport] : this->m_renderContext->getOutput ().getViewports ()) {
	// activate opengl context so we can read from the framebuffer
	viewport->makeCurrent ();

	// find the wallpaper for this screen to read from its FBO
	const auto wallpaperIt = wallpapers.find (screen);
	if (wallpaperIt == wallpapers.end ()) {
	    sLog.error ("Cannot find wallpaper for screen ", screen);
	    continue;
	}

	const auto& wallpaper = wallpaperIt->second;
	const int vpWidth = viewport->viewport.z - viewport->viewport.x;
	const int vpHeight = viewport->viewport.w - viewport->viewport.y;

	// bind the wallpaper's FBO to read from it directly
	// this is more reliable than the default framebuffer on some drivers (NVIDIA/Wayland)
	glBindFramebuffer (GL_FRAMEBUFFER, wallpaper->getWallpaperFramebuffer ());

	// ensure rendering is complete before reading
	glFinish ();

	// make room for storing the pixel of this viewport
	// Scenes rasterize at output resolution; their authored canvas can be tiny.
	auto readSize = wallpaper->getFramebufferSize ();
	if (dynamic_cast<const Render::Wallpapers::CVideo*> (wallpaper.get ()) != nullptr) {
	    // The decode thread can publish a new size before the video attachment has been resized.
	    GLint texture = 0;
	    glGetIntegerv (GL_TEXTURE_BINDING_2D, &texture);
	    glBindTexture (GL_TEXTURE_2D, wallpaper->getWallpaperTexture ());
	    glGetTexLevelParameteriv (GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &readSize.x);
	    glGetTexLevelParameteriv (GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &readSize.y);
	    glBindTexture (GL_TEXTURE_2D, static_cast<GLuint> (texture));
	}
	const int readWidth = readSize.x;
	const int readHeight = readSize.y;
	const auto bufferSize = readWidth * readHeight * 3;
	auto* buffer = new uint8_t[bufferSize];

	// drain any errors accumulated during rendering: glGetError reports anything raised
	// since the last check, so a stale error from a draw call earlier in the frame would
	// otherwise be misattributed to the readback below and silently discard the capture
	for (int i = 0; i < 8; i++) {
	    const GLenum pending = glGetError ();
	    if (pending == GL_NO_ERROR) {
		break;
	    }
	    sLog.error ("Pending OpenGL error raised during rendering (not readback): ", pending);
	}

	// read the FBO data into the pixel buffer
	glPixelStorei (GL_PACK_ALIGNMENT, 1);
	if (GLEW_VERSION_4_5) {
	    glReadnPixels (0, 0, readWidth, readHeight, GL_RGB, GL_UNSIGNED_BYTE, bufferSize, buffer);
	} else {
	    glReadPixels (0, 0, readWidth, readHeight, GL_RGB, GL_UNSIGNED_BYTE, buffer);
	}

	// restore default framebuffer
	glBindFramebuffer (GL_FRAMEBUFFER, 0);

	if (const GLenum error = glGetError (); error != GL_NO_ERROR) {
	    sLog.error ("Cannot obtain pixel data for screen ", screen, ". OpenGL error: ", error);
	    delete[] buffer;
	    continue;
	}

	// Get the UV coordinates which define the visible portion based on scaling mode
	const auto [ustart, uend, vstart, vend] = wallpaper->getPresentationUVs ();

	captures.push_back (
	    { buffer, readWidth, readHeight, vpWidth, vpHeight, currentXOffset, ustart, uend, vstart, vend }
	);

	if (viewport->single) {
	    currentXOffset += vpWidth;
	}
    }

    const auto extension = filename.extension ();
    const std::string extStr = extension.string ();

    // Offload pixel processing and saving to a background thread to avoid hitches
    std::thread ([captures, width, height, vflip, extStr, filename] () {
	auto* bitmap = new uint8_t[width * height * 3] { 0 };

	for (const auto& capture : captures) {
	    // copy pixels to bitmap, sampling from the UV-defined region
	    for (int y = 0; y < capture.vpHeight; y++) {
		for (int x = 0; x < capture.vpWidth; x++) {
		    // Sample pixel centers. Sampling edges duplicates the first row when
		    // V is reversed and shifts the rest of an otherwise 1:1 capture.
		    const float u
			= capture.ustart + ((static_cast<float> (x) + 0.5f) / capture.vpWidth) * (capture.uend - capture.ustart);
		    const float v = capture.vstart
			+ ((static_cast<float> (y) + 0.5f) / capture.vpHeight) * (capture.vend - capture.vstart);

		    // convert UV to pixel coordinates in the source buffer
		    const int srcX = std::clamp (static_cast<int> (u * capture.readWidth), 0, capture.readWidth - 1);
		    const int srcY = std::clamp (static_cast<int> (v * capture.readHeight), 0, capture.readHeight - 1);
		    const int srcIdx = (srcY * capture.readWidth + srcX) * 3;

		    const int xfinal = x + capture.xoffset;
		    // FBO content is not flipped like default framebuffer, so invert vflip logic
		    const int yfinal = vflip ? y : (capture.vpHeight - y - 1);

		    if (yfinal >= 0 && yfinal < height && xfinal >= 0 && xfinal < width) {
			bitmap[yfinal * width * 3 + xfinal * 3] = capture.buffer[srcIdx];
			bitmap[yfinal * width * 3 + xfinal * 3 + 1] = capture.buffer[srcIdx + 1];
			bitmap[yfinal * width * 3 + xfinal * 3 + 2] = capture.buffer[srcIdx + 2];
		    }
		}
	    }
	    delete[] capture.buffer;
	}

	if (extStr == ".bmp") {
	    stbi_write_bmp (filename.c_str (), width, height, 3, bitmap);
	} else if (extStr == ".png") {
	    stbi_write_png (filename.c_str (), width, height, 3, bitmap, width * 3);
	} else if (extStr == ".jpg" || extStr == ".jpeg") {
	    stbi_write_jpg (filename.c_str (), width, height, 3, bitmap, 100);
	}

	delete[] bitmap;
    }).detach ();
}

void WallpaperApplication::setupOutput () {
    const char* XDG_SESSION_TYPE = getenv ("XDG_SESSION_TYPE");

    if (!XDG_SESSION_TYPE) {
	sLog.exception (
	    "Cannot read environment variable XDG_SESSION_TYPE, window server detection failed. Please ensure proper "
	    "values are set"
	);
    }

    sLog.debug ("Checking for window servers: ");

    for (const auto& windowServer : sVideoFactories.getRegisteredDrivers ()) {
	sLog.debug ("\t", windowServer);
    }

    this->m_videoDriver = sVideoFactories.createVideoDriver (
	this->m_context.settings.render.mode, XDG_SESSION_TYPE, this->m_context, *this
    );
    this->m_fullScreenDetector
	= sVideoFactories.createFullscreenDetector (XDG_SESSION_TYPE, this->m_context, *this->m_videoDriver);
}

void WallpaperApplication::setupAudio () {
    // ensure audioprocessing is required by any background, and we have it enabled
    const bool audioProcessingRequired = std::ranges::any_of (
	this->m_backgrounds, [] (const std::pair<const std::string, ProjectUniquePtr>& pair) -> bool {
	    return pair.second->supportsAudioProcessing;
	}
    );

    if (audioProcessingRequired && this->m_context.settings.audio.audioprocessing) {
	this->m_audioRecorder
	    = std::make_unique<WallpaperEngine::Audio::Drivers::Recorders::PulseAudioPlaybackRecorder> ();
    } else {
	this->m_audioRecorder = std::make_unique<WallpaperEngine::Audio::Drivers::Recorders::PlaybackRecorder> ();
    }

    if (this->m_context.settings.audio.automute) {
	m_audioDetector = std::make_unique<WallpaperEngine::Audio::Drivers::Detectors::PulseAudioPlayingDetector> (
	    this->m_context, *this->m_fullScreenDetector
	);
    } else {
	m_audioDetector = std::make_unique<WallpaperEngine::Audio::Drivers::Detectors::AudioPlayingDetector> (
	    this->m_context, *this->m_fullScreenDetector
	);
    }

    // initialize sdl audio driver
    m_audioDriver = std::make_unique<WallpaperEngine::Audio::Drivers::SDLAudioDriver> (
	this->m_context, *this->m_audioDetector, *this->m_audioRecorder
    );
    // initialize audio context
    m_audioContext = std::make_unique<WallpaperEngine::Audio::AudioContext> (*m_audioDriver);
}

void WallpaperApplication::prepareOutputs () {
    // initialize render context
    m_renderContext
	= std::make_unique<WallpaperEngine::Render::RenderContext> (*m_videoDriver, *this, *this->m_mediaSource);

    // Stage every texture before the scene build asks for them. Without this the first
    // wallpaper of a session decodes lazily, one texture at a time, inside CTexture on this
    // thread; batching them hands the whole set to the decode pool at once (6x on heavy ones)
    // Keep prepared providers alive until their initial scenes have bound them.
    std::vector<std::shared_ptr<Render::CTexture>> stagedTextures;
    std::vector<std::shared_ptr<const Render::TextureProvider>> stagedVariants;
    if (this->makeAnyViewportCurrent ()) {
	const auto prefetchStart = std::chrono::steady_clock::now ();
	auto readMs = std::chrono::duration<double, std::milli>::zero ();
	size_t staged = 0;

	for (const auto& [background, info] : this->m_backgrounds) {
	    const auto readStart = std::chrono::steady_clock::now ();
	    auto textures = parseProjectTextures (*info, true);
	    const uint32_t textureReduction = m_renderContext->getTextureReduction (
		*info->wallpaper, m_renderContext->getTextureQualityOutputSize (background)
	    );
	    readMs += std::chrono::steady_clock::now () - readStart;

	    std::vector<std::shared_ptr<Render::CTexture::MipStream>> mipStreams;
	    std::set<std::string> conditionalTextures;
	    for (auto& [name, texture] : textures) {
		const bool conditional = bool (texture->variantSource);
		try {
		    auto prepared = Render::CTexture::createStreaming (
			*m_renderContext, std::move (texture), textureReduction, mipStreams
		    );
		    stagedTextures.push_back (prepared);
		    m_renderContext->storeTexture (
			name, *info->assetLocator, std::move (prepared), textureReduction
		    );
		    if (conditional) conditionalTextures.insert (name);
		    staged++;
		} catch (const std::exception& e) {
		    // resolveTexture will load this asset normally during the scene build
		    sLog.error ("Texture prefetch failed for ", name, ": ", e.what ());
		}
	    }
	    queueTextureStreams (std::move (mipStreams));
	    if (!conditionalTextures.empty ()) {
		auto variants = m_renderContext->prepareTextureFallbacks (
		    conditionalTextures, *info->assetLocator, info->properties, textureReduction
		);
		stagedVariants.insert (stagedVariants.end (), variants.begin (), variants.end ());
	    }
	}

	if (Debug::RenderHealth::enabled ()) {
	    const std::chrono::duration<double, std::milli> totalMs = std::chrono::steady_clock::now () - prefetchStart;
	    std::ostringstream detail;
	    detail << staged << " textures, read+decode " << readMs.count () << " ms, upload "
		   << (totalMs - readMs).count () << " ms";
	    Debug::RenderHealth::record ("startup.texture_prefetch", detail.str ());
	}
    }

    // create a new background for each screen
    const auto sceneBuildStart = std::chrono::steady_clock::now ();

    // set all the specific wallpapers required (skip span group synthetic keys)
    for (const auto& [background, info] : this->m_backgrounds) {
	if (background.rfind ("span:", 0) == 0) {
	    continue;
	}
	const auto scalingIt = this->m_context.settings.general.screenScalings.find (background);
	const auto clampIt = this->m_context.settings.general.screenClamps.find (background);
	const auto scaling = scalingIt != this->m_context.settings.general.screenScalings.end ()
	    ? scalingIt->second
	    : this->m_context.settings.render.window.scalingMode;
	const auto clamp = clampIt != this->m_context.settings.general.screenClamps.end ()
	    ? clampIt->second
	    : this->m_context.settings.render.window.clamp;

	m_renderContext->setWallpaper (
	    background,
	    WallpaperEngine::Render::CWallpaper::fromWallpaper (
		*info->wallpaper, *m_renderContext, *m_audioContext, m_browserContext.get (), scaling, clamp, background
	    )
	);
    }

    // Set up span groups: one shared wallpaper per group, registered for each viewport
    for (const auto& spanGroup : this->m_context.settings.general.spanGroups) {
	if (spanGroup.screens.empty ()) {
	    continue;
	}

	const std::string groupKey = "span:" + spanGroup.screens.front ();
	const auto bgIt = this->m_backgrounds.find (groupKey);
	if (bgIt == this->m_backgrounds.end ()) {
	    continue;
	}

	const auto geometry = m_renderContext->getSpanGeometry (spanGroup.screens);
	if (!geometry) {
	    sLog.error ("No viewports found for span group, skipping");
	    continue;
	}
	WallpaperEngine::Render::CWallpaper::SpanInfo spanInfo;
	spanInfo.totalBounds = geometry->bounds;

	// Create one shared wallpaper with the span group's scaling mode
	auto sharedWallpaper = WallpaperEngine::Render::CWallpaper::fromWallpaper (
	    *bgIt->second->wallpaper, *m_renderContext, *m_audioContext, m_browserContext.get (), spanGroup.scaling,
	    spanGroup.clamp, groupKey
	);

	// Convert to shared_ptr so it can be registered for multiple viewports
	std::shared_ptr<WallpaperEngine::Render::CWallpaper> shared (std::move (sharedWallpaper));
	shared->setSpanInfo (spanInfo);

	// Register the same wallpaper for each screen in the span group
	for (const auto& screenName : spanGroup.screens) {
	    m_renderContext->setWallpaper (screenName, shared);
	}
    }

    recordStartupPhase ("startup.scene_build", sceneBuildStart);
}

void WallpaperApplication::setupOpenGLDebugging () {
#if !NDEBUG
    glDebugMessageCallback (CustomGLDebugCallback, nullptr);
    // GL_DEBUG_OUTPUT defaults to disabled in non-debug contexts (and our EGL context is not
    // created with the debug flag), so the callback never fires without enabling it explicitly
    glEnable (GL_DEBUG_OUTPUT);
    glEnable (GL_DEBUG_OUTPUT_SYNCHRONOUS);
#endif
}

void WallpaperApplication::setup () {
    this->setupOutput ();
    this->setupAudio ();
    this->prepareOutputs ();
    this->setupOpenGLDebugging ();
    this->setupControlSocket ();

    if (this->m_context.settings.general.dumpStructure) {
	auto prettyPrinter = Data::Dumpers::StringPrinter ();

	for (const auto& [background, info] : this->m_renderContext->getWallpapers ()) {
	    prettyPrinter.printWallpaper (info->getWallpaperData ());
	}

	std::cout << prettyPrinter.str () << std::endl;
    }

#if DEMOMODE
    // ensure only one background is running so everything can be properly caught
    if (this->m_renderContext->getWallpapers ().size () > 1) {
	sLog.exception ("Demo mode only supports one background");
    }

    int width = this->m_renderContext->getWallpapers ().begin ()->second->getWidth ();
    int height = this->m_renderContext->getWallpapers ().begin ()->second->getHeight ();
    std::vector<uint8_t> pixels (width * height * 3);
    bool initialized = false;
    int frame = 0;
#endif /* DEMOMODE */
}

void WallpaperApplication::render () {
    // IPC must remain serviceable while fullscreen-paused or waiting for an output
    // to resume frame callbacks. A queued switch is application work, not a frame.
    this->processControlSocket ();
    const bool lockscreenActive = this->getLockscreenStreamFPS () > 0;
    if (!lockscreenActive && !this->m_lockscreenFrames.empty ()) this->releaseLockscreenFrames ();
    if (!this->m_buildingSwitch) this->processPreparedSwitches ();

    static time_t seconds;
    static struct tm* timeinfo;

    if (this->m_isPaused) {
	usleep (FULLSCREEN_CHECK_WAIT_TIME);
	if (!lockscreenActive && this->m_fullScreenDetector->anythingFullscreen () && this->m_context.state.general.keepRunning) {
	    return;
	}
	m_renderContext->setPause (false);

	// account for paused duration in playlist timers
	const auto pausedNow = std::chrono::steady_clock::now ();
	const auto pausedDuration = pausedNow - this->m_pauseStart;

	for (auto& [_, playlist] : this->m_activePlaylists) {
	    if (!playlist.definition.settings.updateOnPause) {
		playlist.nextSwitch += pausedDuration;
		playlist.lastUpdate += pausedDuration;
	    }
	}

	this->m_isPaused = false;
    } else {
	// update g_Daytime
	time (&seconds);
	timeinfo = localtime (&seconds);
	g_Daytime = static_cast<float> ((timeinfo->tm_hour * 60) + timeinfo->tm_min) / (24.0f * 60.0f);

	// keep track of the previous frame's time
	g_TimeLast = g_Time;
	// calculate the current time value
	g_Time = m_videoDriver->getRenderTime ();
	// update audio recorder
	m_audioDriver->update ();
	// update the media source
	m_mediaSource->update ();
	// update input information
	m_videoDriver->getInputContext ().update ();
	// process driver events
	m_videoDriver->dispatchEventQueue ();
	// Keep CEF alive while a scene wallpaper is active so browser shutdown and
	// late callbacks from a previous web wallpaper can complete safely.
	if (this->m_browserContext && this->makeAnyViewportCurrent ()) {
	    this->m_browserContext->doMessageLoopWork ();
	}

	if (m_videoDriver->closeRequested ()) {
	    sLog.out ("Stop requested by driver");
	    this->m_context.state.general.keepRunning = false;
	}

#if DEMOMODE
	// wait for a full render cycle before actually starting
	// this gives some extra time for video and web decoders to set themselves up
	// because of size changes
	if (m_videoDriver->getFrameCounter () > (uint32_t)this->m_context.settings.render.maximumFPS) {
	    if (!initialized) {
		width = this->m_renderContext->getWallpapers ().begin ()->second->getWidth ();
		height = this->m_renderContext->getWallpapers ().begin ()->second->getHeight ();
		pixels.reserve (width * height * 3);
		init_encoder ("output.webm", width, height);
		initialized = true;
	    }

	    glBindFramebuffer (
		GL_FRAMEBUFFER, this->m_renderContext->getWallpapers ().begin ()->second->getWallpaperFramebuffer ()
	    );

	    glPixelStorei (GL_PACK_ALIGNMENT, 1);
	    glReadPixels (0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels.data ());
	    write_video_frame (pixels.data ());
	    frame++;

	    // stop after the given framecount
	    if (frame >= FRAME_COUNT) {
		this->m_context.state.general.keepRunning = false;
	    }
	}
#endif /* DEMOMODE */
	// check for fullscreen windows and wait until there's none fullscreen
	if (!lockscreenActive && this->m_fullScreenDetector->anythingFullscreen () && this->m_context.state.general.keepRunning) {
	    this->m_isPaused = true;
	    this->m_pauseStart = std::chrono::steady_clock::now ();

	    m_renderContext->setPause (true);
	    return;
	}

	// one active (non-paused) loop iteration == one frame for the health report;
	// paused iterations are excluded so worst_frame_ms reflects real render gaps
	Debug::RenderHealth::frame ();

	// everything above finished at least once, so this is the whole load cost
	static bool firstFrame = true;
	if (firstFrame) {
	    firstFrame = false;
	    recordStartupPhase ("startup.first_frame", sStartupBegin);
	}
    }

    this->updatePlaylists ();

    if (!this->m_context.settings.screenshot.take || this->m_screenShotTaken == true) {
	return;
    }

    const uint32_t elapsedFrames = this->m_videoDriver->getFrameCounter () - this->m_screenshotStartFrame;
    if (elapsedFrames < this->m_context.settings.screenshot.delay) {
	return;
    }

    const auto& wallpapers = this->m_renderContext->getWallpapers ();
    const auto& viewports = this->m_renderContext->getOutput ().getViewports ();
    if (viewports.empty ()) {
	return;
    }

    const bool waitingForVideo = std::ranges::any_of (viewports, [&wallpapers] (const auto& entry) {
	const auto wallpaper = wallpapers.find (entry.first);
	if (wallpaper == wallpapers.end ()) {
	    return false;
	}

	const auto* video = dynamic_cast<const Render::Wallpapers::CVideo*> (wallpaper->second.get ());
	return video && !video->isReadyForScreenshot ();
    });
    if (waitingForVideo) {
	const auto now = std::chrono::steady_clock::now ();
	if (this->m_screenshotReadyDeadline == std::chrono::steady_clock::time_point {}) {
	    this->m_screenshotReadyDeadline = now + std::chrono::seconds (3);
	}
	if (now < this->m_screenshotReadyDeadline) {
	    return;
	}
    }

    this->takeScreenshot (this->m_context.settings.screenshot.path);
    this->m_screenShotTaken = true;
}

void WallpaperApplication::cleanup () {
    sLog.out ("Stopping");

#if DEMOMODE
    close_encoder ();
#endif /* DEMOMODE */

    SDL_Quit ();
}

void WallpaperApplication::show () {
    setup ();
    while (this->m_context.state.general.keepRunning) {
	render ();
	// render() can also be called while a replacement scene is under construction.
	Scripting::serviceJavaScriptCoreRuntime ();
    }

    if (this->m_controlSocket >= 0) {
	close (this->m_controlSocket);

	// only remove the socket file if it is still the one this instance
	// bound - a newer engine may own the path by now
	struct stat info {};
	if (stat (this->m_controlSocketPath.c_str (), &info) == 0 && info.st_ino == this->m_controlSocketInode) {
	    unlink (this->m_controlSocketPath.c_str ());
	}
    }

    this->stopSwitchWorker ();

    // Tear down scenes, sound streams and their reader threads while SDL is
    // still alive. Leaving m_renderContext until ~WallpaperApplication made
    // cleanup() call SDL_Quit first, which can hang or invalidate SDL state
    // underneath AudioStream teardown.
    this->cancelComparison ();
    this->releaseLockscreenFrames ();
    this->m_renderContext.reset ();
    Scripting::serviceJavaScriptCoreRuntime ();

    cleanup ();
}

bool WallpaperApplication::abnormalTermination () const {
    return this->m_videoDriver->abnormalTermination ();
}

void WallpaperApplication::update (Render::Drivers::Output::OutputViewport* viewport, bool present) {
    // render the scene
    if (present) m_renderContext->render (viewport);
    if (std::chrono::steady_clock::now () >= this->getLockscreenFrameDeadline (viewport->name))
	this->exportLockscreenFrame (viewport);
}

int WallpaperApplication::getLockscreenStreamFPS () const {
    return this->m_lockscreenStream.fps (std::chrono::steady_clock::now ());
}

std::chrono::steady_clock::time_point WallpaperApplication::getLockscreenFrameDeadline (const std::string& screen) const {
    if (this->getLockscreenStreamFPS () == 0) return std::chrono::steady_clock::time_point::max ();
    const auto frame = this->m_lockscreenFrames.find (screen);
    return frame == this->m_lockscreenFrames.end () ? std::chrono::steady_clock::time_point {} : frame->second.nextFrame;
}

void WallpaperApplication::exportLockscreenFrame (Render::Drivers::Output::OutputViewport* viewport) {
    using Clock = std::chrono::steady_clock;
    const int fps = this->getLockscreenStreamFPS ();
    if (fps == 0) return;
    auto& frame = this->m_lockscreenFrames[viewport->name];
    frame.nextFrame = Clock::now () + std::chrono::duration_cast<Clock::duration> (std::chrono::duration<double> (1.0 / fps));
    const auto found = this->m_renderContext->getWallpapers ().find (viewport->name);
    if (found == this->m_renderContext->getWallpapers ().end ()) return;
    const auto [width, height] = LockscreenStream::frameSize (viewport->viewport.z, viewport->viewport.w);
    if (width == 0 || height == 0) return;
    viewport->makeCurrent ();
    LoadingGLState glState;
    glState.prepareOutgoingFrame (0, 0);
    if (frame.framebuffer == 0) {
	glGenFramebuffers (1, &frame.framebuffer);
	glGenTextures (1, &frame.texture);
    }
    glBindFramebuffer (GL_FRAMEBUFFER, frame.framebuffer);
    if (frame.width != width || frame.height != height) {
	glBindTexture (GL_TEXTURE_2D, frame.texture);
	glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_BGRA, GL_UNSIGNED_BYTE, nullptr);
	glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glFramebufferTexture2D (GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, frame.texture, 0);
	if (glCheckFramebufferStatus (GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
	    sLog.error ("Cannot create lockscreen framebuffer for ", viewport->name);
	    return;
	}
	frame.width = width;
	frame.height = height;
	frame.pixels.resize (size_t (width) * height * 4);
    }
    const auto& wallpaper = found->second;
    const auto destination = wallpaper->m_destFramebuffer;
    const auto overlay = wallpaper->m_fps;
    const auto loadTimes = wallpaper->m_loadTimes;
    const auto loadDetails = wallpaper->m_loadDetails;
    const auto transition = wallpaper->m_transitionMode;
    Data::Utils::ScopeGuard restoreWallpaper ([&] {
	wallpaper->setDestinationFramebuffer (destination);
	wallpaper->setFpsOverlay (overlay);
	wallpaper->setLoadOverlay (loadTimes, loadDetails);
	wallpaper->m_transitionMode = transition;
    });
    wallpaper->setDestinationFramebuffer (frame.framebuffer);
    wallpaper->setFpsOverlay (-1.0f);
    wallpaper->setLoadOverlay ({ -1.0f }, false);
    wallpaper->m_transitionMode = Render::TransitionMode_None;
    const bool bottomUp = this->getOutput ().renderVFlip ();
    wallpaper->render (viewport->viewport, bottomUp, viewport->globalPosition,
		       viewport->logicalSize, { width, height });
    glBindFramebuffer (GL_READ_FRAMEBUFFER, frame.framebuffer);
    glBindBuffer (GL_PIXEL_PACK_BUFFER, 0);
    glPixelStorei (GL_PACK_ALIGNMENT, 4);
    glPixelStorei (GL_PACK_ROW_LENGTH, 0);
    glPixelStorei (GL_PACK_SKIP_PIXELS, 0);
    glPixelStorei (GL_PACK_SKIP_ROWS, 0);
    glReadPixels (0, 0, width, height, GL_BGRA, GL_UNSIGNED_BYTE, frame.pixels.data ());
    const char* runtime = std::getenv ("XDG_RUNTIME_DIR");
    // X11 root presentation sends readback rows directly to a top-down XImage.
    if (runtime && !LockscreenStream::writeFrame (std::string (runtime) + "/linux-wallpaperengine-lock",
						 viewport->name, width, height, frame.pixels, bottomUp))
	sLog.error ("Cannot write lockscreen frame for ", viewport->name);
}

void WallpaperApplication::releaseLockscreenFrames () {
    if (this->m_lockscreenFrames.empty ()) return;
    if (this->makeAnyViewportCurrent ()) {
	for (const auto& [screen, frame] : this->m_lockscreenFrames) {
	    glDeleteFramebuffers (1, &frame.framebuffer);
	    glDeleteTextures (1, &frame.texture);
	}
    }
    this->m_lockscreenFrames.clear ();
}

void WallpaperApplication::signal (int) {
    // Logging allocates and converts timestamps, which can deadlock if the
    // signal interrupted either operation. Only publish the stop request here.
    this->m_context.state.general.keepRunning.store (false, std::memory_order_relaxed);
}

const std::map<std::string, ProjectUniquePtr>& WallpaperApplication::getBackgrounds () const {
    return this->m_backgrounds;
}

ApplicationContext& WallpaperApplication::getContext () const { return this->m_context; }

const WallpaperEngine::Render::Drivers::Output::Output& WallpaperApplication::getOutput () const {
    return this->m_renderContext->getOutput ();
}

void WallpaperApplication::setDestinationFramebuffer (GLuint framebuffer) {
    this->m_destinationFramebuffer = framebuffer;
    // Update all wallpapers with the new destination framebuffer
    for (const auto& [screen, wallpaper] : this->m_renderContext->getWallpapers ()) {
	wallpaper->setDestinationFramebuffer (framebuffer);
    };
}

GLuint WallpaperApplication::getDestinationFramebuffer () const { return this->m_destinationFramebuffer; }
