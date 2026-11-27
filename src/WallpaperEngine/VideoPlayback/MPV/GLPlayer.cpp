#include "GLPlayer.h"

#include "WallpaperEngine/Logging/Log.h"

#include <algorithm>
#include <mpv/render_gl.h>
#include <mpv/stream_cb.h>

using namespace WallpaperEngine::VideoPlayback::MPV;

void* get_proc_address (void* ctx, const char* name) {
    return static_cast<GLPlayer*> (ctx)->getContext ().getDriver ().getProcAddress (name);
}

GLPlayer::GLPlayer (
    RenderContext& context, GLuint outputTexture, const std::filesystem::path& file, const int64_t baseWidth,
    const int64_t baseHeight, const GLuint fbo
) : ContextAware (context), m_outputTexture (outputTexture), m_width (baseWidth), m_height (baseHeight) {
    this->m_fbo = fbo;
    this->m_doWeOwnFramebuffer = this->m_fbo == GL_NONE;

    this->prepareGL ();
    this->setSource (file);
}

GLPlayer::GLPlayer (
    RenderContext& context, const GLuint outputTexture, MemoryStreamProtocolUniquePtr stream, const int64_t baseWidth,
    const int64_t baseHeight, const GLuint fbo
) : ContextAware (context), m_outputTexture (outputTexture), m_width (baseWidth), m_height (baseHeight) {
    this->m_fbo = fbo;
    this->m_doWeOwnFramebuffer = this->m_fbo == GL_NONE;

    this->prepareGL ();
    this->setSource (std::move (stream));
}

GLPlayer::~GLPlayer () {
    // clean up any kept resources
    this->stop ();

    // only clean up framebuffer if we own it
    if (this->m_doWeOwnFramebuffer) {
	// free gl resources too
	glDeleteFramebuffers (1, &this->m_fbo);
    }
}

void GLPlayer::incrementUsageCount () {
    this->m_usageCount++;

    if (this->m_usageCount == 1) {
	this->play ();
    }
}

void GLPlayer::decrementUsageCount () {
    if (this->m_usageCount == 0) {
	sLog.exception ("GLPlayer usage count would underflow");
    }

    this->m_usageCount--;

    if (this->m_usageCount == 0) {
	this->stop ();
    }
}

void GLPlayer::enableHDR () {
    if (this->m_handle) sLog.exception ("Cannot enable HDR after video playback has started");
    this->m_hdrEnabled = true;
}

void GLPlayer::setHDRPresentation (const bool enabled) const {
    const std::lock_guard lock (this->m_playbackMutex);
    this->m_hdrPresentation = enabled;
}

bool GLPlayer::hasHDRSource () const {
    const std::lock_guard lock (this->m_playbackMutex);
    return this->m_hdrEnabled && this->m_hdrSource && !this->m_hdrDisabled;
}

bool GLPlayer::isHDRFrame () const {
    const std::lock_guard lock (this->m_playbackMutex);
    return this->m_hdrFrame;
}

void GLPlayer::queueHDRConfiguration (const bool enabled) const {
    this->m_hdrConfigurationTarget = enabled;
    this->m_hdrConfigurationFailed = false;
    this->m_hdrConfigurationBegin = this->m_nextCommandId + 1;
    // vo_gpu emits linear RGB relative to its 203-nit reference white. A 10000-nit
    // target avoids tone mapping PQ; the owner converts this to Windows-scRGB/80.
    this->queueCommand ({ "set", "target-trc", enabled ? "linear" : "auto" });
    this->queueCommand ({ "set", "target-prim", enabled ? "bt.709" : "auto" });
    this->queueCommand ({ "set", "target-peak", enabled ? "10000" : "auto" });
    this->queueCommand ({ "set", "tone-mapping", enabled ? "clip" : "auto" });
    this->queueCommand ({ "set", "gamut-mapping-mode", enabled ? "clip" : "auto" });
    this->m_hdrConfigurationEnd = this->queueCommand (
        { "set", "fbo-format", enabled ? "rgba16f" : "rgba8" }
    );
}

void GLPlayer::setNonBlocking () {
    if (this->m_handle) {
	sLog.exception ("Cannot set nonblocking mode after playback has started");
    }

    this->m_nonBlocking = true;
}

void GLPlayer::clearNonBlocking () {
    if (this->m_handle) {
	sLog.exception ("Cannot set nonblocking mode after playback has started");
    }

    this->m_nonBlocking = false;
}

void GLPlayer::setMuted () {
    const std::lock_guard lock (this->m_playbackMutex);
    if (this->m_muted) {
	return;
    }
    this->m_muted = true;
    this->startAudioFade ();
}

void GLPlayer::clearMuted () {
    const std::lock_guard lock (this->m_playbackMutex);
    if (!this->m_muted) {
	return;
    }
    this->m_muted = false;
    if (this->m_handle) {
	this->queueCommand ({ "set", "mute", "no" });
    }
    this->startAudioFade ();
}

void GLPlayer::setVolume (double volume) {
    const std::lock_guard lock (this->m_playbackMutex);
    if (this->m_volume == volume) {
	return;
    }
    this->m_volume = volume;
    this->m_audioVolume = this->m_muted || this->m_paused ? 0.0 : volume;
    if (this->m_handle) {
	this->queueCommand ({ "set", "volume", std::to_string (this->m_audioVolume) });
	this->startAudioFade ();
    }
}

void GLPlayer::setPaused () {
    const std::lock_guard lock (this->m_playbackMutex);
    if (this->m_paused) {
	return;
    }
    this->m_paused = true;
    this->startAudioFade ();
}

void GLPlayer::clearPaused () {
    const std::lock_guard lock (this->m_playbackMutex);
    if (!this->m_paused) {
	return;
    }
    this->m_paused = false;
    this->m_pausedSeek.reset ();
    if (this->m_handle && !this->m_activitySuspended) {
	this->queueCommand ({ "set", "pause", "no" });
    }
    this->startAudioFade ();
}

void GLPlayer::startAudioFade () {
    if (!this->m_handle) return;
    const double target = this->m_muted || this->m_paused ? 0.0 : this->m_volume;
    if (this->m_audioVolume == target) {
	this->m_audioFadeNextTick.reset ();
	if (this->m_muted) this->queueCommand ({ "set", "mute", "yes" });
	if (this->m_paused) this->queueCommand ({ "set", "pause", "yes" });
	return;
    }
    if (!this->m_audioFadeNextTick) {
	this->m_audioFadeNextTick = std::chrono::steady_clock::now () + std::chrono::milliseconds (25);
    }
    mpv_wakeup (this->m_handle);
}

void GLPlayer::updateAudioFade (const std::chrono::steady_clock::time_point now) {
    if (!this->m_audioFadeNextTick || now < *this->m_audioFadeNextTick) return;
    // Native video timer 101 steps every 25 ms: normalized gain falls by
    // volume*.03+.02 and rises by volume*.01+.01. MPV uses percent units.
    const double target = this->m_muted || this->m_paused ? 0.0 : this->m_volume;
    const double step = this->m_audioVolume > target ? this->m_volume * 0.03 + 2.0 : this->m_volume * 0.01 + 1.0;
    this->m_audioVolume = this->m_audioVolume > target
	? std::max (target, this->m_audioVolume - step) : std::min (target, this->m_audioVolume + step);
    this->queueCommand ({ "set", "volume", std::to_string (this->m_audioVolume) });
    if (this->m_audioVolume == target) {
	this->startAudioFade ();
    } else {
	this->m_audioFadeNextTick = now + std::chrono::milliseconds (25);
    }
}

void GLPlayer::setSceneActive (const Render::Wallpapers::CScene& scene, const bool active) {
    const std::lock_guard lock (this->m_playbackMutex);
    this->m_sceneActivity.insert_or_assign (&scene, active);
    this->updateActivityPause ();
}

void GLPlayer::removeSceneActivity (const Render::Wallpapers::CScene& scene) {
    const std::lock_guard lock (this->m_playbackMutex);
    this->m_sceneActivity.erase (&scene);
    this->updateActivityPause ();
}

void GLPlayer::updateActivityPause () {
    const bool suspended = !this->m_sceneActivity.empty ()
	&& std::ranges::none_of (this->m_sceneActivity, [] (const auto& entry) { return entry.second; });
    if (suspended == this->m_activitySuspended) return;
    this->m_activitySuspended = suspended;
    if (this->m_handle && !this->m_paused) {
	this->queueCommand ({ "set", "pause", suspended ? "yes" : "no" });
    }
}

void GLPlayer::enableScriptControl () {
    const std::lock_guard lock (this->m_playbackMutex);
    // Playback remains timed, including while scripts control rate and seeking.
    if (this->m_scriptControlled) {
	return;
    }
    this->m_scriptControlled = true;
    if (this->m_handle) {
	this->queueCommand ({ "set", "keep-open", "yes" });
    }
}

void GLPlayer::resumePlayback () {
    const std::lock_guard lock (this->m_playbackMutex);
    if (this->m_ended && !this->m_pendingSeek.has_value ()) {
	this->setCurrentTime (0.0);
    }
    // keep-open may pause mpv at EOF without a script pause request. An explicit
    // play must clear that internal pause even when our requested state is playing,
    // unless scene sampling has independently suspended the decoder.
    this->m_paused = false;
    this->m_pausedSeek.reset ();
    if (this->m_handle && !this->m_activitySuspended) {
	this->queueCommand ({ "set", "pause", "no" });
    }
    this->startAudioFade ();
}

void GLPlayer::stopPlayback () {
    const std::lock_guard lock (this->m_playbackMutex);
    this->setPaused ();
    this->setCurrentTime (0.0);
}

bool GLPlayer::isPlaying () const {
    const std::lock_guard lock (this->m_playbackMutex);
    return !this->m_paused && (this->m_pendingSeek.has_value () || !this->m_ended);
}

double GLPlayer::getDuration () const {
    const std::lock_guard lock (this->m_playbackMutex);
    return this->m_duration;
}

double GLPlayer::getCurrentTime () const {
    const std::lock_guard lock (this->m_playbackMutex);
    return this->m_pendingSeek.value_or (this->m_currentTime);
}

void GLPlayer::setCurrentTime (const double time) {
    const std::lock_guard lock (this->m_playbackMutex);
    // A stopped texture is often reset on every script tick. Do not enqueue the
    // same seek while paused, but allow repeated seeks during active playback.
    if (this->m_paused && this->m_pausedSeek == time) {
	return;
    }
    this->m_pausedSeek = this->m_paused ? std::optional<double> (time) : std::nullopt;
    this->m_pendingSeek = time;
    this->m_pendingSeekId = this->m_nextCommandId + 1;
    this->queueCommand ({ "seek", std::to_string (time), "absolute+exact" }, true);
}

void GLPlayer::setRate (const double rate) {
    const std::lock_guard lock (this->m_playbackMutex);
    if (this->m_rate == rate) {
	return;
    }
    this->m_rate = rate;
    if (this->m_handle) {
	this->queueCommand ({ "set", "speed", std::to_string (rate) });
    }
}

void GLPlayer::setLoop (const bool loop) {
    const std::lock_guard lock (this->m_playbackMutex);
    if (this->m_loop == loop) {
	return;
    }
    this->m_loop = loop;
    if (this->m_handle) {
	this->queueCommand ({ "set", "loop-file", loop ? "inf" : "no" });
    }
}

uint64_t GLPlayer::queueCommand (std::vector<std::string> arguments, const bool seek) const {
    const uint64_t id = ++this->m_nextCommandId;
    this->m_commands.push_back ({ id, std::move (arguments), seek });
    if (this->m_handle) {
	mpv_wakeup (this->m_handle);
    }
    return id;
}

void GLPlayer::submitCommand () const {
    // libmpv's synchronous client API can wait for this very render thread.
    // Only one async request may be outstanding: libmpv does not promise order
    // between concurrent requests (in particular pause -> seek -> play).
    while (!this->m_commandInFlight && !this->m_commands.empty ()) {
	const auto& command = this->m_commands.front ();
	if (command.seek && !this->m_loaded) {
	    return;
	}
	std::vector<const char*> arguments;
	for (const auto& argument : command.arguments) {
	    arguments.push_back (argument.c_str ());
	}
	arguments.push_back (nullptr);
	const int error = mpv_command_async (this->m_handle, command.id, arguments.data ());
	if (error >= 0) {
	    this->m_commandInFlight = true;
	} else {
	    this->completeCommand (error);
	}
    }
}

void GLPlayer::completeCommand (const int error) const {
    const auto& command = this->m_commands.front ();
    if (error < 0) {
	sLog.error ("Video command ", command.arguments.front (), ": ", mpv_error_string (error));
    }
    if (command.id == this->m_pendingSeekId) {
	if (error >= 0 && this->m_pendingSeek.has_value ()) {
	    this->m_currentTime = *this->m_pendingSeek;
	    this->m_ended = false;
	}
	this->m_pendingSeek.reset ();
    }
    const uint64_t id = command.id;
    this->m_commands.pop_front ();
    this->m_commandInFlight = false;
    if (this->m_hdrConfigurationEnd != 0 && id >= this->m_hdrConfigurationBegin
        && id <= this->m_hdrConfigurationEnd) {
        this->m_hdrConfigurationFailed |= error < 0;
        if (id == this->m_hdrConfigurationEnd) {
            this->m_hdrConfigurationEnd = 0;
            if (this->m_hdrConfigurationFailed) {
                this->m_hdrDisabled = true;
                if (this->m_hdrConfigurationTarget) this->queueHDRConfiguration (false);
                else this->m_hdrConfigurationBroken = true;
            } else {
                this->m_hdrConfigured = this->m_hdrConfigurationTarget;
                this->m_dimensionsChanged = true;
            }
        }
    }
}

void GLPlayer::processEvent (const mpv_event& event) const {
    if (event.event_id == MPV_EVENT_FILE_LOADED) {
	this->m_loaded = true;
    } else if (
	event.event_id == MPV_EVENT_COMMAND_REPLY && !this->m_commands.empty ()
	&& event.reply_userdata == this->m_commands.front ().id
    ) {
	this->completeCommand (event.error);
    } else if (event.event_id == MPV_EVENT_PROPERTY_CHANGE) {
	const auto* property = static_cast<mpv_event_property*> (event.data);
        if (this->m_hdrEnabled && std::string_view (property->name) == "video-params/gamma") {
            const char* gamma = property->format == MPV_FORMAT_STRING && property->data != nullptr
                ? *static_cast<char**> (property->data) : nullptr;
            this->m_hdrSource = gamma != nullptr
                && (std::string_view (gamma) == "pq" || std::string_view (gamma) == "hlg");
            return;
        }
	if (property->format == MPV_FORMAT_DOUBLE && property->data != nullptr) {
	    const double value = *static_cast<double*> (property->data);
	    if (std::string_view (property->name) == "time-pos") {
		this->m_currentTime = value;
	    }
	    if (std::string_view (property->name) == "duration") {
		this->m_duration = value;
	    }
	} else if (property->format == MPV_FORMAT_FLAG && property->data != nullptr) {
	    this->m_ended = *static_cast<int*> (property->data) != 0;
	} else if (property->format == MPV_FORMAT_INT64 && property->data != nullptr) {
	    const int64_t value = *static_cast<int64_t*> (property->data);
	    if (value > 0) {
		const bool width = std::string_view (property->name) == "dwidth";
		auto& dimension = width ? this->m_width : this->m_height;
		(width ? this->m_videoWidthReported : this->m_videoHeightReported) = true;
		this->m_dimensionsChanged |= dimension != value;
		dimension = value;
	    }
	}
    }
}

void GLPlayer::render (const bool trackScreenshotFrame) const {
    // rendering should only happen if the texture is in use
    if (this->m_handle == nullptr) {
	return;
    }

    int64_t width;
    int64_t height;
    bool dimensionsChanged;
    bool hdr;
    bool configuring;
    {
	const std::lock_guard lock (this->m_playbackMutex);
        const bool desiredHDR = this->m_hdrEnabled && this->m_hdrSource
            && this->m_hdrPresentation && !this->m_hdrDisabled;
        if (this->m_hdrConfigurationEnd == 0 && !this->m_hdrConfigurationBroken
            && desiredHDR != this->m_hdrConfigured) this->queueHDRConfiguration (desiredHDR);
        configuring = this->m_hdrConfigurationEnd != 0 || this->m_hdrConfigurationBroken;
        hdr = this->m_hdrConfigured;
	width = this->m_width;
	height = this->m_height;
	dimensionsChanged = this->m_dimensionsChanged;
	this->m_dimensionsChanged = false;
    }
    const GLint format = hdr ? GL_RGBA16F : GL_RGBA8;
    bool decodedFrame = false;
    if (trackScreenshotFrame && !this->m_hasDecodedScreenshotFrame) {
	mpv_render_frame_info frame {};
	constexpr auto frameFlags = MPV_RENDER_FRAME_INFO_PRESENT | MPV_RENDER_FRAME_INFO_REDRAW;
	decodedFrame = mpv_render_context_get_info (
	    this->m_renderContext, { MPV_RENDER_PARAM_NEXT_FRAME_INFO, &frame }
	) >= 0 && (frame.flags & frameFlags) == MPV_RENDER_FRAME_INFO_PRESENT;
    }
    if (configuring) {
        // A partially applied set of target options has no publishable encoding.
        // Service libmpv without drawing it; black is valid in either destination.
        int skip = 1;
        int block = 0;
        mpv_render_param pending[] = { { MPV_RENDER_PARAM_SKIP_RENDERING, &skip },
            { MPV_RENDER_PARAM_BLOCK_FOR_TARGET_TIME, &block }, { MPV_RENDER_PARAM_INVALID, nullptr } };
        const int error = mpv_render_context_render (this->m_renderContext, pending);
        // A skipped frame can be redrawn once configuration finishes.
        this->m_hasDecodedScreenshotFrame |= error >= 0 && decodedFrame;
        this->m_hasScreenshotFrame = false;
        this->clearOutputFrame ();
        const std::lock_guard lock (this->m_playbackMutex);
        this->m_hdrFrame = false;
        this->m_dimensionsChanged |= dimensionsChanged;
        return;
    }
    if (dimensionsChanged) {
	glBindTexture (GL_TEXTURE_2D, this->m_outputTexture);
	glTexImage2D (GL_TEXTURE_2D, 0, format, width, height, 0, GL_RGBA, hdr ? GL_FLOAT : GL_UNSIGNED_BYTE, nullptr);
    }

    // render the next
    glViewport (0, 0, width, height);

    mpv_opengl_fbo fbo { static_cast<int> (this->m_fbo), static_cast<int> (width), static_cast<int> (height),
			 format };

    // no need to flip as it'll be handled by the wallpaper rendering code
    int flip_y = 0;
    int block = this->m_nonBlocking || this->m_scriptControlled ? 0 : 1;

    mpv_render_param params[] = { { MPV_RENDER_PARAM_OPENGL_FBO, &fbo },
				  { MPV_RENDER_PARAM_FLIP_Y, &flip_y },
				  { MPV_RENDER_PARAM_BLOCK_FOR_TARGET_TIME, &block },
				  { MPV_RENDER_PARAM_INVALID, nullptr } };

    const int error = mpv_render_context_render (this->m_renderContext, params);
    if (error < 0 && this->m_hdrEnabled) {
        this->m_hasScreenshotFrame = false;
        this->clearOutputFrame ();
        const std::lock_guard lock (this->m_playbackMutex);
        this->m_hdrFrame = false;
        if (hdr) {
            this->m_hdrDisabled = true;
            this->queueHDRConfiguration (false);
        }
        return;
    }
    const std::lock_guard lock (this->m_playbackMutex);
    if (error >= 0 && trackScreenshotFrame) {
	this->m_hasDecodedScreenshotFrame |= decodedFrame;
	// Capture only once the decoded size has reached the actual GL draw.
	this->m_hasScreenshotFrame = this->m_hasDecodedScreenshotFrame && this->m_videoWidthReported
	    && this->m_videoHeightReported && !this->m_dimensionsChanged;
    }
    this->m_hdrFrame = hdr;
}

bool GLPlayer::hasScreenshotFrame () const {
    const std::lock_guard lock (this->m_playbackMutex);
    return this->m_hasScreenshotFrame && !this->m_dimensionsChanged;
}

void GLPlayer::clearOutputFrame () const {
    glBindFramebuffer (GL_FRAMEBUFFER, this->m_fbo);
    GLboolean mask[4];
    glGetBooleanv (GL_COLOR_WRITEMASK, mask);
    const bool scissor = glIsEnabled (GL_SCISSOR_TEST);
    glDisable (GL_SCISSOR_TEST);
    glColorMask (GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    constexpr GLfloat black[] = { 0.0f, 0.0f, 0.0f, 1.0f };
    glClearBufferfv (GL_COLOR, 0, black);
    glColorMask (mask[0], mask[1], mask[2], mask[3]);
    if (scissor) glEnable (GL_SCISSOR_TEST);
}

int GLPlayer::getWidth () const {
    const std::lock_guard lock (this->m_playbackMutex);
    return this->m_width;
}
int GLPlayer::getHeight () const {
    const std::lock_guard lock (this->m_playbackMutex);
    return this->m_height;
}

void GLPlayer::prepareGL () {
    if (!this->m_doWeOwnFramebuffer || this->m_fbo != GL_NONE) {
	return;
    }

    glGenFramebuffers (1, &this->m_fbo);
    glBindFramebuffer (GL_FRAMEBUFFER, this->m_fbo);
    glBindTexture (GL_TEXTURE_2D, this->m_outputTexture);
    // reset texture's contents
    glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA8, this->m_width, this->m_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    constexpr GLenum drawBuffers[1] = { GL_COLOR_ATTACHMENT0 };
    glFramebufferTexture2D (GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, this->m_outputTexture, 0);
    glDrawBuffers (1, drawBuffers);

    // ensure first framebuffer is okay
    if (glCheckFramebufferStatus (GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
	sLog.exception ("Framebuffers are not properly set");
    }

    // clear the framebuffer
    glClear (GL_COLOR_BUFFER_BIT);
}

void GLPlayer::init () {
    this->m_handle = mpv_create ();

    if (this->m_handle == nullptr) {
	sLog.exception ("Cannot create mpv context for video texture");
    }

    // setup mpv options for playback
    mpv_set_option_string (this->m_handle, "terminal", "yes");
#if NDEBUG
    mpv_set_option_string (this->m_handle, "msg-level", "all=status,statusline=no");
#else
    mpv_set_option_string (this->m_handle, "msg-level", "all=v");
#endif
    mpv_set_option_string (this->m_handle, "input-cursor", "no");
    mpv_set_option_string (this->m_handle, "cursor-autohide", "no");
    mpv_set_option_string (this->m_handle, "config", "no");
    mpv_set_option_string (this->m_handle, "fbo-format", "rgba8");
    mpv_set_option_string (this->m_handle, "vo", "libmpv");
    mpv_set_option_string (this->m_handle, "profile", "fast");
    // Untimed playback consumes frames at the scene render rate when a clip has
    // no audio clock. Keep mpv's real-time scheduler even for nonblocking textures.
    mpv_set_option_string (this->m_handle, "untimed", "no");
    if (this->m_nonBlocking) {
	// With BLOCK_FOR_TARGET_TIME=0, ask mpv to deliver frames at their target
	// time instead of its default early render lead, retaining the real-time
	// scheduler and audio/video synchronization without blocking the renderer.
	mpv_set_option_string (this->m_handle, "video-timing-offset", "0");
    }

    if (mpv_initialize (this->m_handle) < 0) {
	sLog.exception ("Could not initialize mpv context");
    }

    // ensure video is muted and plays in a loop
    mpv_set_property_string (this->m_handle, "hwdec", "auto");
    mpv_set_property_string (this->m_handle, "loop-file", this->m_loop ? "inf" : "no");
    mpv_set_property (this->m_handle, "speed", MPV_FORMAT_DOUBLE, &this->m_rate);
    if (this->m_scriptControlled) mpv_set_property_string (this->m_handle, "keep-open", "yes");
    this->m_audioFadeNextTick.reset ();
    this->m_audioVolume = this->m_muted || this->m_paused ? 0.0 : this->m_volume;
    mpv_set_property (this->m_handle, "volume", MPV_FORMAT_DOUBLE, &this->m_audioVolume);

    mpv_observe_property (this->m_handle, 0, "time-pos", MPV_FORMAT_DOUBLE);
    mpv_observe_property (this->m_handle, 0, "duration", MPV_FORMAT_DOUBLE);
    mpv_observe_property (this->m_handle, 0, "eof-reached", MPV_FORMAT_FLAG);
    mpv_observe_property (this->m_handle, 0, "dwidth", MPV_FORMAT_INT64);
    mpv_observe_property (this->m_handle, 0, "dheight", MPV_FORMAT_INT64);
    if (this->m_hdrEnabled)
        mpv_observe_property (this->m_handle, 0, "video-params/gamma", MPV_FORMAT_STRING);

    // initialize gl context for mpv
    mpv_opengl_init_params gl_init_params { get_proc_address, this };
    mpv_render_param params[] { { MPV_RENDER_PARAM_API_TYPE, const_cast<char*> (MPV_RENDER_API_TYPE_OPENGL) },
				{ MPV_RENDER_PARAM_OPENGL_INIT_PARAMS, &gl_init_params },
				{ MPV_RENDER_PARAM_INVALID, nullptr } };

    if (mpv_render_context_create (&this->m_renderContext, this->m_handle, params) < 0) {
	sLog.exception ("Failed to initialize MPV's GL context");
    }

    // mute the video if required
    mpv_set_property_string (this->m_handle, "mute", this->m_muted ? "yes" : "no");
    // ensure play/pause status is respected too
    mpv_set_property_string (this->m_handle, "pause", this->m_paused || this->m_activitySuspended ? "yes" : "no");
}

void GLPlayer::setSource (const std::filesystem::path& file) { this->m_file = file; }

void GLPlayer::setSource (MemoryStreamProtocolUniquePtr source) { this->m_stream = std::move (source); }

void GLPlayer::play () {
    if (this->m_handle != nullptr) {
	sLog.exception ("Cannot play the same GLPlayer twice");
    }

    if (!this->m_file.has_value () && !this->m_stream.has_value ()) {
	sLog.exception ("Cannot play a GLPlayer without a source");
    }

    this->init ();

    if (this->m_file.has_value ()) {
	// build the path to the video file
	const char* command[] = { "loadfile", this->m_file.value ().c_str (), nullptr };

	if (mpv_command (this->m_handle, command) < 0) {
	    sLog.exception ("Cannot load video to play");
	}
    } else if (this->m_stream) {
	this->m_stream.value ()->registerReadCallback (this->m_handle);

	// start playing the video
	const char* command[] = { "loadfile", "buffer://", nullptr };

	if (mpv_command (this->m_handle, command) < 0) {
	    sLog.exception ("Cannot load video texture to play");
	}
    }

    // Service acknowledgements independently of scene FPS. Otherwise each
    // ordered control waits an entire render frame before the next can start.
    // The worker never calls OpenGL or waits for the render thread under a lock.
    this->m_eventThread = std::jthread ([this] (const std::stop_token stop) {
	while (!stop.stop_requested ()) {
	    double timeout = -1;
	    {
		const std::lock_guard lock (this->m_playbackMutex);
		if (this->m_audioFadeNextTick) {
		    timeout = std::max (0.0, std::chrono::duration<double> (
			*this->m_audioFadeNextTick - std::chrono::steady_clock::now ()).count ());
		}
	    }
	    const mpv_event* event = mpv_wait_event (this->m_handle, timeout);
	    const std::lock_guard lock (this->m_playbackMutex);
	    if (event != nullptr) {
		this->processEvent (*event);
	    }
	    this->updateAudioFade (std::chrono::steady_clock::now ());
	    this->submitCommand ();
	}
    });
}

void GLPlayer::stop () {
    if (this->m_eventThread.joinable ()) {
	this->m_eventThread.request_stop ();
	mpv_wakeup (this->m_handle);
	this->m_eventThread.join ();
    }
    this->m_loaded = false;
    this->m_hdrSource = false;
    this->m_hdrConfigured = false;
    this->m_hdrFrame = false;
    this->m_hasDecodedScreenshotFrame = false;
    this->m_hasScreenshotFrame = false;
    this->m_videoWidthReported = false;
    this->m_videoHeightReported = false;
    this->m_hdrConfigurationEnd = 0;
    this->m_hdrConfigurationBroken = false;
    this->m_audioFadeNextTick.reset ();
    this->m_commands.clear ();
    this->m_commandInFlight = false;
    this->m_pendingSeek.reset ();
    this->m_pausedSeek.reset ();
    this->m_currentTime = 0.0;
    this->m_duration = 0.0;
    this->m_ended = false;
    // clean up mpv and get it ready to start again at some point
    if (this->m_renderContext) {
	mpv_render_context_free (this->m_renderContext);
	this->m_renderContext = nullptr;
    }

    if (this->m_handle) {
	mpv_terminate_destroy (this->m_handle);
	this->m_handle = nullptr;
    }
}
