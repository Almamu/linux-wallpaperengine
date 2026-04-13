#include "WallpaperApplication.h"

#include "../../../../include/frontends/project.h"
#include "WallpaperEngine/Data/Builders/VectorBuilder.h"

#include <glad/glad.h>
#include <linux-wallpaperengine/render.h>

#include "Steam/FileSystem/FileSystem.h"
#include "WallpaperEngine/Debugging/CallStack.h"
#include "WallpaperEngine/Desktop/Universal/Environment.h"
#include "WallpaperEngine/Desktop/Wayland/Environment.h"
#include "WallpaperEngine/Desktop/X11/Environment.h"
#include "WallpaperEngine/Logging/Log.h"

#include <glm/vec3.hpp>
#include <ranges>

#if DEMOMODE
#include "recording.h"
#endif /* DEMOMODE */

#include <algorithm>
#include <numeric>
#include <unistd.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>
#include <thread>

#define FULLSCREEN_CHECK_WAIT_TIME 250

float g_Time;
float g_TimeLast;
float g_Daytime;

using namespace WallpaperEngine::Application;

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

WallpaperApplication::WallpaperApplication (ApplicationContext& context) :
	m_context (context), m_desktopEnvironment (nullptr) {
	this->m_context.state.context = wp_context_create (this->m_context.getConfig ());
	this->setupEnvironment ();
	this->setupProperties ();
}

void WallpaperApplication::registerPlaylist (
	const std::string& screen, const ApplicationContext::PlaylistDefinition& playlist,
	const std::optional<std::string>& currentPath
) {
	const auto now = std::chrono::steady_clock::now ();

	if (playlist.items.empty ()) {
		return;
	}

	ActivePlaylist state;

	state.definition = playlist;
	state.order = this->buildPlaylistOrder (playlist);

	if (state.order.empty ()) {
		return;
	}

	if (currentPath.has_value ()) {
		state.orderIndex = 0;

		for (std::size_t i = 0; i < state.order.size (); i++) {
			if (playlist.items[state.order[i]] == currentPath.value ()) {
				state.orderIndex = i;
				break;
			}
		}
	}

	const uint32_t delayMinutes = std::max<uint32_t> (1, state.definition.settings.delayMinutes);
	state.nextSwitch = now + std::chrono::minutes (delayMinutes);
	state.lastUpdate = now;

	this->m_activePlaylists.insert_or_assign (screen, std::move (state));
}

void WallpaperApplication::deregisterPlaylist (const std::string& screen) { this->m_activePlaylists.erase (screen); }

void WallpaperApplication::setupEnvironment () {
	if (this->m_context.settings.render.mode == ApplicationContext::DESKTOP_BACKGROUND) {
		const char* XDG_SESSION_TYPE = getenv ("XDG_SESSION_TYPE");

		if (!XDG_SESSION_TYPE) {
			sLog.exception (
				"Cannot read environment variable XDG_SESSION_TYPE, window server detection failed. Please ensure "
				"proper "
				"values are set"
			);
		}

		sLog.debug ("Checking for window servers: wayland, x11, default");

		if (strncmp (XDG_SESSION_TYPE, "wayland", 7) == 0) {
			this->m_desktopEnvironment = new Desktop::Wayland::Environment (this->m_context, *this, *this);
		} else if (strncmp (XDG_SESSION_TYPE, "x11", 3) == 0) {
			this->m_desktopEnvironment = new Desktop::X11::Environment (this->m_context, *this, *this);
		} else {
			sLog.exception ("Unknown desktop type ", XDG_SESSION_TYPE);
		}
	} else {
		sLog.debug ("No desktop mode requested, using window output");
		this->m_desktopEnvironment = new Desktop::Universal::Environment (this->m_context, *this, *this);
	}

	// setup counter and gl_proc_address
	wp_context_set_gl_proc_address (this->m_context.state.context, &this->m_desktopEnvironment->gl_proc_address);
	wp_context_set_time_counter (this->m_context.state.context, &this->m_desktopEnvironment->counter);
}

wp_project* WallpaperApplication::loadBackground (const std::string& bg) const {
	return wp_project_load_id_str (this->m_context.state.context, nullptr, bg.c_str ())
		?: wp_project_load_folder (this->m_context.state.context, nullptr, bg.c_str ());
}

std::vector<std::size_t>
WallpaperApplication::buildPlaylistOrder (const ApplicationContext::PlaylistDefinition& definition) {
	std::vector<std::size_t> order (definition.items.size ());
	std::iota (order.begin (), order.end (), 0);

	if (definition.settings.order == wp_playlist_order_Random) {
		std::ranges::shuffle (order, this->m_playlistRng);
	}

	return order;
}

bool WallpaperApplication::selectNextCandidate (const ActivePlaylist& playlist, std::size_t& outOrderIndex) {
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

void WallpaperApplication::advancePlaylist (
	const std::string& screen, ActivePlaylist& playlist, const std::chrono::steady_clock::time_point& now
) {
	if (playlist.order.empty ()) {
		return;
	}

	playlist.orderIndex = (playlist.orderIndex + 1) % playlist.order.size ();

	if (playlist.orderIndex == 0 && playlist.definition.settings.order == wp_playlist_order_Random) {
		std::ranges::shuffle (playlist.order, this->m_playlistRng);
	}

	std::size_t candidateOrderIndex = playlist.orderIndex;

	if (!selectNextCandidate (playlist, candidateOrderIndex)) {
		sLog.error ("All playlist items failed for ", screen, ", keeping current wallpaper");
		const uint32_t delayMinutes = std::max<uint32_t> (1, playlist.definition.settings.delayMinutes);
		playlist.nextSwitch = now + std::chrono::minutes (delayMinutes);
		return;
	}

	// TODO: CHECK HOW THIS BEHAVES WITH MULTIPLE FAILURES, KEEP CURRENT LOGIC FOR NOW
	auto nextIndex = playlist.order[candidateOrderIndex];
	auto& nextPath = playlist.definition.items[nextIndex];
	auto project = this->loadBackground (nextPath);

	if (project == nullptr) {
		playlist.failedIndices.insert (nextIndex);

		if (!selectNextCandidate (playlist, candidateOrderIndex)) {
			sLog.error ("All playlist items failed for ", screen, ", keeping current wallpaper");
			const uint32_t delayMinutes = std::max<uint32_t> (1, playlist.definition.settings.delayMinutes);
			playlist.nextSwitch = now + std::chrono::minutes (delayMinutes);
			return;
		}

		nextIndex = playlist.order[candidateOrderIndex];
		nextPath = playlist.definition.items[nextIndex];
	}

	playlist.orderIndex = candidateOrderIndex;

	try {
		if (project == nullptr) {
			project = this->loadBackground (nextPath);
		}

		auto previousBgIt = this->m_backgrounds.find (screen);

		if (previousBgIt != this->m_backgrounds.end ()) {
			wp_project_destroy (previousBgIt->second);
		}

		this->setupPropertiesForProject (project);
		this->m_backgrounds[screen] = project;

		const auto scalingIt = this->m_context.settings.general.screenScalings.find (screen);
		const auto clampIt = this->m_context.settings.general.screenClamps.find (screen);
		const auto scaling = scalingIt != this->m_context.settings.general.screenScalings.end ()
			? scalingIt->second
			: this->m_context.settings.render.window.scalingMode;
		const auto clamp = clampIt != this->m_context.settings.general.screenClamps.end ()
			? clampIt->second
			: this->m_context.settings.render.window.clamp;

		const auto activeScreenIt = this->m_activeOutputs.find (screen);

		if (activeScreenIt != this->m_activeOutputs.end ()) {
			activeScreenIt->second->setWallpaper (project);
		}
		// TODO: STORE SCALE, CLAMP, ETC TO BE USED
	} catch (const std::exception& e) {
		sLog.error ("Failed to advance playlist on ", screen, ": ", e.what ());
	}

	if (project == nullptr) {
		playlist.failedIndices.insert (nextIndex);
		// keep current position; next timer tick will retry advancement
		sLog.error ("Failed to load wallpaper for ", screen, ", will retry on next cycle");
	}

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

		if (playlist.definition.settings.mode != wp_playlist_mode_Timer) {
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

void WallpaperApplication::setupPropertiesForProject (wp_project* project) {
	wp_project_property_list_reset (project);
	wp_project_property* property = nullptr;

	while ((property = wp_project_property_list_next (project)) != nullptr) {
		auto it = this->m_context.settings.general.properties.find (property->name);

		// no need to do anything if the value is not set by the user
		if (it == this->m_context.settings.general.properties.end ()) {
			continue;
		}

		const auto combo = WP_PROPERTY_AS_COMBO (property);
		const auto boolean = WP_PROPERTY_AS_BOOLEAN (property);
		const auto color = WP_PROPERTY_AS_COLOR (property);
		const auto file = WP_PROPERTY_AS_FILE (property);
		const auto scene_texture = WP_PROPERTY_AS_SCENE_TEXTURE (property);
		const auto slider = WP_PROPERTY_AS_SLIDER (property);
		const auto text = WP_PROPERTY_AS_TEXT (property);

		if (combo != nullptr) {
			wp_project_property_set_combo (project, combo, it->second.c_str ());
		} else if (boolean != nullptr) {
			wp_project_property_set_boolean (project, boolean, it->second == "true" || it->second == "1");
		} else if (color != nullptr) {
			// parse color
			int vectorSize = Data::Builders::VectorBuilder::preparseSize (it->second);

			// TODO: PROPERLY PARSE THIS VALUE AS THE VECTORBUILDER CLASS ONLY VALIDATES NUMBER OF COMPONENTS, BUT NOT
			// RANGES
			// TODO: MAYBE IMPLEMENT A COLOR TYPE OR SOMETHING?
			if (vectorSize < 3) {
				sLog.error ("Invalid color value for property ", property->name, ": ", it->second, ". Ignoring...");
				continue;
			}

			if (vectorSize == 3) {
				const auto value = Data::Builders::VectorBuilder::parse<glm::vec3> (it->second);

				wp_project_property_set_color (project, color, value.r, value.g, value.b, 1.0f);
			} else {
				const auto value = Data::Builders::VectorBuilder::parse<glm::vec4> (it->second);

				wp_project_property_set_color (project, color, value.r, value.g, value.b, value.a);
			}
		} else if (file != nullptr) {
			wp_project_property_set_file (project, file, it->second.c_str ());
		} else if (scene_texture != nullptr) {
			wp_project_property_set_scene_texture (project, scene_texture, it->second.c_str ());
		} else if (slider != nullptr) {
			wp_project_property_set_slider (
				project, slider, Data::Builders::VectorBuilder::convert<float> (it->second)
			);
		} else if (text != nullptr) {
			wp_project_property_set_text (project, text, it->second.c_str ());
		} else {
			sLog.error ("Unknown property type (", property->type, ") for property ", property->name, ". Ignoring...");
		}
	}
}

void WallpaperApplication::setupProperties () {
	for (const auto& [background, info] : this->m_backgrounds) {
		this->setupPropertiesForProject (info);
	}
}

void WallpaperApplication::takeScreenshot (const std::filesystem::path& filename) const {
	struct ViewportCapture {
		uint8_t* buffer;
		int width;
		int height;
	};

	std::vector<ViewportCapture> captures;
	int currentXOffset = 0;

	for (const auto& [screen, output] : this->m_activeOutputs) {
		const auto viewport = output->getViewport ();
		const int vpWidth = viewport.z - viewport.x;
		const int vpHeight = viewport.w - viewport.y;

		// bind the wallpaper's FBO to read from it directly
		// this is more reliable than the default framebuffer on some drivers (NVIDIA/Wayland)
		glBindFramebuffer (GL_FRAMEBUFFER, output->getFramebuffer ());

		// ensure rendering is complete before reading
		glFinish ();

		// make room for storing the pixel of this viewport
		const auto bufferSize = vpWidth * vpHeight * 3;
		auto* buffer = new uint8_t[bufferSize];

		// read the FBO data into the pixel buffer
		glPixelStorei (GL_PACK_ALIGNMENT, 1);

		if (GLAD_GL_VERSION_4_5) {
			glReadnPixels (0, 0, vpWidth, vpHeight, GL_RGB, GL_UNSIGNED_BYTE, bufferSize, buffer);
		} else {
			glReadPixels (0, 0, vpWidth, vpHeight, GL_RGB, GL_UNSIGNED_BYTE, buffer);
		}

		// restore default framebuffer
		glBindFramebuffer (GL_FRAMEBUFFER, 0);

		if (const GLenum error = glGetError (); error != GL_NO_ERROR) {
			sLog.error ("Cannot obtain pixel data for screen ", screen, ". OpenGL error: ", error);
			delete[] buffer;
			continue;
		}

		captures.push_back ({ .buffer = buffer, .width = vpWidth, .height = vpHeight });

		currentXOffset += vpWidth;
	}

	const auto width = std::accumulate (captures.begin (), captures.end (), 0, [] (int acc, const auto& capture) {
		return acc + capture.width;
	});
	const auto height = std::accumulate (captures.begin (), captures.end (), 0, [] (int acc, const auto& capture) {
		if (capture.height > acc) {
			return capture.height;
		}

		return acc;
	});

	constexpr auto vflip = false;
	const auto extension = filename.extension ();
	const std::string extStr = extension.string ();

	// Offload pixel processing and saving to a background thread to avoid hitches
	std::thread ([captures, width, height, extStr, filename] () {
		auto* bitmap = new uint8_t[width * height * 3] { 0 };
		int offsetX = 0;

		for (const auto& capture : captures) {
			// copy pixels to bitmap, sampling from the UV-defined region
			for (int y = 0; y < capture.height; y++) {
				for (int x = 0; x < capture.width; x++) {
					const int srcIdx = (y * capture.width + x) * 3;

					const int xfinal = x + offsetX;
					// FBO content is not flipped like default framebuffer, so invert vflip logic
					const int yfinal = vflip ? y : (capture.height - y - 1);

					if (yfinal >= 0 && yfinal < height && xfinal >= 0 && xfinal < width) {
						bitmap[yfinal * width * 3 + xfinal * 3] = capture.buffer[srcIdx];
						bitmap[yfinal * width * 3 + xfinal * 3 + 1] = capture.buffer[srcIdx + 1];
						bitmap[yfinal * width * 3 + xfinal * 3 + 2] = capture.buffer[srcIdx + 2];
					}
				}
			}

			delete[] capture.buffer;
			offsetX += capture.width;
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

void WallpaperApplication::setupAudio () {
	this->m_playbackRecorder = std::make_unique<Audio::Pulseaudio> ();
	this->m_playbackRecorder->desktopEnvironment = this->m_desktopEnvironment;

	// setup audio processing
	if (this->m_context.settings.audio.audioprocessing) {
		wp_context_set_audio_input_mix (this->m_context.state.context, &this->m_playbackRecorder->input_mix);
	}

	if (this->m_context.settings.audio.automute) {
		wp_config_set_mute_check (this->m_context.getConfig (), &this->m_playbackRecorder->mute_check);
	}
}

void WallpaperApplication::setupOpenGLDebugging () {
#if !NDEBUG
	glDebugMessageCallback (CustomGLDebugCallback, nullptr);
	glEnable (GL_DEBUG_OUTPUT_SYNCHRONOUS);
#endif
}

void WallpaperApplication::setup () {
	this->setupAudio ();
	setupOpenGLDebugging ();

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
	if (this->m_desktopEnvironment->isCloseRequested ()) {
		this->m_context.state.keepRunning = false;
		return;
	}

	wp_render_update_time (this->m_context.state.context);

	// update fullscreen detection status
	this->m_desktopEnvironment->detectFullscreen ();

	// do not render anything if there's anything fullscreen
	if (this->m_desktopEnvironment->anything_fullscreen) {
		// give the cpu some time to breathe
		usleep (FULLSCREEN_CHECK_WAIT_TIME);

		// store the time the pause started
		if (!this->m_pauseStart.has_value ()) {
			this->m_pauseStart = std::chrono::steady_clock::now ();
		}

		return;
	}

	// background was paused, but it's not anymore
	// advance playlists
	if (this->m_pauseStart.has_value ()) {
		const auto pausedDuration = std::chrono::steady_clock::now () - *this->m_pauseStart;

		for (auto& playlist : this->m_activePlaylists | std::views::values) {
			playlist.nextSwitch += pausedDuration;
			playlist.lastUpdate += pausedDuration;
		}
	}

	this->m_desktopEnvironment->render ();

	// update playlists status
	this->updatePlaylists ();

	if (!this->m_context.settings.screenshot.take || this->m_screenShotTaken == true) {
		return;
	}

	if (this->m_desktopEnvironment->getCurrentFrame () < this->m_nextFrameScreenshot) {
		return;
	}

	this->takeScreenshot (this->m_context.settings.screenshot.path);
	this->m_screenShotTaken = true;

#if DEMOMODE
	// TODO: UPDATE THE VIDEO RECORDING CODE
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
}

void WallpaperApplication::cleanup () {
	sLog.out ("Stopping");

#if DEMOMODE
	close_encoder ();
#endif /* DEMOMODE */
}

void WallpaperApplication::show () {
	this->setup ();
	while (this->m_context.state.keepRunning) {
		this->render ();
	}
	this->cleanup ();
}

void WallpaperApplication::signal (int signal) {
	sLog.out ("Stop requested by signal ", signal);
	this->m_context.state.keepRunning = false;
}

const std::map<std::string, wp_project*>& WallpaperApplication::getBackgrounds () const { return this->m_backgrounds; }

ApplicationContext& WallpaperApplication::getContext () const { return this->m_context; }

void WallpaperApplication::onScreenAvailable (const std::string& screen, Desktop::Output* output) {
	// only act on screens that don't have a background set
	if (this->m_backgrounds.contains (screen)) {
		return;
	}

	const auto& playlists = this->m_context.getPlaylists ();
	std::optional<ApplicationContext::PlaylistDefinition> defaultPlaylist = std::nullopt;
	std::optional<std::string> defaultBackground = std::nullopt;
	bool defaultBackgroundFromPlaylist = false;

	if (playlists.contains (screen)) {
		defaultPlaylist = playlists.at (screen);
	}

	// fallback to DEFAULT_SCREEN_NAME as default background
	if (this->m_context.settings.general.backgrounds.contains (DEFAULT_SCREEN_NAME)) {
		defaultBackground = this->m_context.settings.general.backgrounds[DEFAULT_SCREEN_NAME];
	}

	// default playlist overrides the default background
	if (this->m_context.settings.general.playlists.contains (DEFAULT_SCREEN_NAME)) {
		const auto it = playlists.find (this->m_context.settings.general.playlists[DEFAULT_SCREEN_NAME]);

		if (it == playlists.end ()) {
			sLog.exception ("Playlist ", this->m_context.settings.general.playlists[DEFAULT_SCREEN_NAME], "not found");
		}

		if (it->second.items.empty ()) {
			sLog.exception ("Playlist ", this->m_context.settings.general.playlists[DEFAULT_SCREEN_NAME], "is empty");
		}

		// playlists take precedence over default background
		defaultBackground = it->second.items.front ();
		defaultPlaylist = it->second;
		defaultBackgroundFromPlaylist = true;
	}

	std::string path;

	if (this->m_context.settings.general.backgrounds.contains (screen)) {
		path = this->m_context.settings.general.backgrounds[screen];
	}

	auto currentDefaultBackground = path.empty () ? defaultBackground : path;
	auto currentDefaultPlaylist = defaultBackgroundFromPlaylist ? defaultPlaylist : std::nullopt;
	const auto it = this->m_context.settings.general.playlists.find (screen);

	if (it != this->m_context.settings.general.playlists.end ()) {
		const auto playlistsIt = playlists.find (it->second);

		if (playlistsIt != playlists.end () && !playlistsIt->second.items.empty ()) {
			currentDefaultBackground = playlistsIt->second.items.front ();
			currentDefaultPlaylist = playlistsIt->second;
		}
	}

	if (currentDefaultPlaylist.has_value ()) {
		this->registerPlaylist (screen, *currentDefaultPlaylist, *currentDefaultBackground);
	}

	if (currentDefaultBackground.has_value () == false) {
		return;
	}

	this->m_backgrounds[screen] = this->loadBackground (*currentDefaultBackground);
	this->m_activeOutputs[screen] = output;

	output->setWallpaper (this->m_backgrounds[screen]);
}

void WallpaperApplication::onScreenUnavailable (const std::string& name, Desktop::Output* output) {
	// de-register background and playlist
	this->deregisterPlaylist (name);
	auto it = this->m_backgrounds.find (name);

	if (it != this->m_backgrounds.end ()) {
		// free memory
		wp_project_destroy (it->second);
	}

	this->m_backgrounds.erase (name);
}