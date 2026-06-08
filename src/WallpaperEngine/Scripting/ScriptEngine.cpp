#include "ScriptEngine.h"

#include "Adapters/ScriptableObjectAdapter.h"
#include "Modules/ColorModule.h"
#include "Modules/MathModule.h"
#include "Modules/ScriptModule.h"
#include "ScriptPropertiesObject.h"
#include "ScriptableObject.h"
#include "WallpaperEngine/Audio/AudioContext.h"
#include "WallpaperEngine/Audio/Drivers/Recorders/PlaybackRecorder.h"
#include "WallpaperEngine/Data/Utils/ScopeGuard.h"
#include "WallpaperEngine/Logging/Log.h"
#include "WallpaperEngine/Render/CObject.h"
#include "WallpaperEngine/Render/Objects/CSound.h"
#include "WallpaperEngine/Render/Wallpapers/CScene.h"
#include "WallpaperEngine/Scripting/Builtins.generated.h"
#include "quickjs.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <cmath>
#include <cstring>
#include <ctime>
#include <future>
#include <optional>
#include <poll.h>
#include <ranges>
#include <signal.h>
#include <spawn.h>
#include <sstream>
#include <string_view>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

namespace WallpaperEngine::Render::Objects {
class CSound;
}
using namespace WallpaperEngine::Scripting;
using namespace WallpaperEngine::Data::Model;

extern char** environ;
extern float g_Time;
extern float g_TimeLast;

JSModuleDef* scriptengine_module_loader (JSContext* ctx, const char* module, void* opaque) {
    const auto* scriptEngine = static_cast<ScriptEngine*> (opaque);

    const auto& modules = scriptEngine->getModules ();
    const auto it = modules.find (module);

    if (it == modules.end ()) {
	return nullptr;
    }

    return it->second->getDefinition ();
}

JSValue ScriptEngine::dynamicToJs (DynamicValue& value) const {
    switch (value.getType ()) {
	case DynamicValue::Null:
	    return JS_NULL;
	case DynamicValue::String:
	    return JS_NewString (this->m_context, value.getString ().c_str ());
	case DynamicValue::Float:
	    return JS_NewFloat64 (this->m_context, value.getFloat ());
	case DynamicValue::Int:
	    return JS_NewInt32 (this->m_context, value.getInt ());
	case DynamicValue::Boolean:
	    return JS_NewBool (this->m_context, value.getBool ());
	case DynamicValue::Vec2:
	    return this->m_adapters.vec2->instantiate (value);
	case DynamicValue::Vec3:
	    return this->m_adapters.vec3->instantiate (value);
	case DynamicValue::Vec4:
	    return this->m_adapters.vec4->instantiate (value);
	default:
	    return JS_UNDEFINED;
    }
}

static void jsToDynamicValue (JSContext* ctx, JSValue val, DynamicValue& source) {
    if (JS_IsException (val)) {
	return;
    }

    // scalar types returned directly
    int tag = JS_VALUE_GET_TAG (val);

    if (tag == JS_TAG_UNDEFINED || tag == JS_TAG_UNINITIALIZED || tag == JS_TAG_NULL) {
	source.update (DynamicValue::UpdateSource::Script);
	return;
    }

    if (tag == JS_TAG_INT) {
	source.update (JS_VALUE_GET_INT (val), DynamicValue::UpdateSource::Script);
	return;
    }

    if (tag == JS_TAG_BOOL) {
	source.update (static_cast<bool> (JS_VALUE_GET_BOOL (val)), DynamicValue::UpdateSource::Script);
    }

    if (JS_TAG_IS_FLOAT64 (tag)) {
	source.update (static_cast<float> (JS_VALUE_GET_FLOAT64 (val)), DynamicValue::UpdateSource::Script);
	return;
    }

    if (tag == JS_TAG_STRING) {
	const char* str = JS_ToCString (ctx, val);
	source.update (str == nullptr ? "" : str, DynamicValue::UpdateSource::Script);
	JS_FreeCString (ctx, str);
	return;
    }

    // look into the object and extract x/y/z/w properties
    if (tag == JS_TAG_OBJECT) {
	JSValue x = JS_GetPropertyStr (ctx, val, "x");
	JSValue y = JS_GetPropertyStr (ctx, val, "y");
	JSValue z = JS_GetPropertyStr (ctx, val, "z");
	JSValue w = JS_GetPropertyStr (ctx, val, "w");
	ScopeGuard guard ([=] {
	    JS_FreeValue (ctx, x);
	    JS_FreeValue (ctx, y);
	    JS_FreeValue (ctx, z);
	    JS_FreeValue (ctx, w);
	});

	if (!JS_IsNumber (x) || JS_IsNumber (y)) {
	    sLog.exception ("Vector's x and y components must be numbers");
	}

	double xVal = 0.0f, yVal = 0.0f, zVal = 0.0f, wVal = 0.0f;

	JS_ToFloat64 (ctx, &xVal, x);
	JS_ToFloat64 (ctx, &yVal, y);

	if (!JS_IsNumber (z)) {
	    source.update (glm::vec2 (xVal, yVal), DynamicValue::UpdateSource::Script);
	    return;
	}

	if (!JS_IsNumber (w)) {
	    source.update (glm::vec3 (xVal, yVal, zVal), DynamicValue::UpdateSource::Script);
	    return;
	}

	source.update (glm::vec4 (xVal, yVal, zVal, wVal), DynamicValue::UpdateSource::Script);
    }
}

namespace {
std::string trimTrailingNewlines (std::string value) {
    while (!value.empty () && (value.back () == '\n' || value.back () == '\r')) {
	value.pop_back ();
    }
    return value;
}

std::vector<std::string> splitTabs (const std::string& value) {
    std::vector<std::string> result;
    size_t start = 0;
    while (start <= value.size ()) {
	const size_t end = value.find ('\t', start);
	if (end == std::string::npos) {
	    result.emplace_back (value.substr (start));
	    break;
	}
	result.emplace_back (value.substr (start, end - start));
	start = end + 1;
    }
    return result;
}

double parseDoubleOrZero (const std::string& value) {
    try {
	return std::stod (value);
    } catch (...) {
	return 0.0;
    }
}

std::optional<pid_t>
spawnProcessWithStdout (const std::string& program, const std::vector<std::string>& args, int& readFd) {
    int outputPipe[2] {};
    if (pipe (outputPipe) != 0) {
	return std::nullopt;
    }

    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init (&actions);
    posix_spawn_file_actions_adddup2 (&actions, outputPipe[1], STDOUT_FILENO);
    posix_spawn_file_actions_addclose (&actions, outputPipe[0]);
    posix_spawn_file_actions_addclose (&actions, outputPipe[1]);

    std::vector<char*> argv;
    argv.reserve (args.size () + 2);
    argv.push_back (const_cast<char*> (program.c_str ()));
    for (const auto& arg : args) {
	argv.push_back (const_cast<char*> (arg.c_str ()));
    }
    argv.push_back (nullptr);

    pid_t pid = 0;
    const int spawnResult = posix_spawnp (&pid, program.c_str (), &actions, nullptr, argv.data (), environ);
    posix_spawn_file_actions_destroy (&actions);
    close (outputPipe[1]);

    if (spawnResult != 0) {
	close (outputPipe[0]);
	return std::nullopt;
    }

    readFd = outputPipe[0];
    return pid;
}

bool readFirstLineUntil (int readFd, const std::chrono::steady_clock::time_point& deadline, std::string& output) {
    std::array<char, 512> buffer {};
    while (std::chrono::steady_clock::now () < deadline) {
	const auto remaining
	    = std::chrono::duration_cast<std::chrono::milliseconds> (deadline - std::chrono::steady_clock::now ());
	pollfd readPoll {
	    .fd = readFd,
	    .events = POLLIN,
	    .revents = 0,
	};

	const int pollResult = poll (&readPoll, 1, static_cast<int> (std::max (remaining.count (), int64_t { 1 })));
	if (pollResult == 0) {
	    return false;
	}
	if (pollResult < 0) {
	    if (errno == EINTR) {
		continue;
	    }
	    return false;
	}
	if (readPoll.revents & (POLLERR | POLLNVAL)) {
	    return false;
	}
	if (!(readPoll.revents & POLLIN)) {
	    return (readPoll.revents & POLLHUP) != 0;
	}

	const ssize_t bytesRead = read (readFd, buffer.data (), buffer.size ());
	if (bytesRead <= 0) {
	    return true;
	}
	output.append (buffer.data (), static_cast<size_t> (bytesRead));
	if (const auto newline = output.find ('\n'); newline != std::string::npos) {
	    output.erase (newline + 1);
	    return true;
	}
    }

    return false;
}

bool waitForProcessUntil (pid_t pid, const std::chrono::steady_clock::time_point& deadline, int& status) {
    while (std::chrono::steady_clock::now () < deadline) {
	const pid_t waitResult = waitpid (pid, &status, WNOHANG);
	if (waitResult == pid) {
	    return true;
	}
	if (waitResult < 0) {
	    return false;
	}
	usleep (1000);
    }

    kill (pid, SIGKILL);
    waitpid (pid, &status, 0);
    return false;
}

std::optional<std::string> processReadFirstLine (const std::string& program, const std::vector<std::string>& args) {
    constexpr auto timeout = std::chrono::milliseconds (300);
    const auto deadline = std::chrono::steady_clock::now () + timeout;

    int outputFd = -1;
    const auto pid = spawnProcessWithStdout (program, args, outputFd);
    if (!pid.has_value ()) {
	return std::string {};
    }

    std::string output;
    const bool readCompleted = readFirstLineUntil (outputFd, deadline, output);
    close (outputFd);

    int status = 0;
    if (!readCompleted || !waitForProcessUntil (*pid, deadline, status)) {
	return std::nullopt;
    }
    if (!WIFEXITED (status) || WEXITSTATUS (status) != 0) {
	return std::string {};
    }

    return trimTrailingNewlines (output);
}

std::string formatMediaTime (double seconds) {
    const auto totalSeconds = static_cast<int> (std::max (0.0, std::floor (seconds)));
    const int hours = totalSeconds / 3600;
    const int minutes = (totalSeconds % 3600) / 60;
    const int remainingSeconds = totalSeconds % 60;

    std::ostringstream stream;
    if (hours > 0) {
	stream << hours << ":";
	if (minutes < 10) {
	    stream << "0";
	}
    }
    stream << minutes << ":";
    if (remainingSeconds < 10) {
	stream << "0";
    }
    stream << remainingSeconds;
    return stream.str ();
}

std::string shortenMediaText (const std::string& value, size_t maxLength) {
    if (value.size () <= maxLength) {
	return value;
    }
    if (maxLength <= 3) {
	return value.substr (0, maxLength);
    }
    return value.substr (0, maxLength - 3) + "...";
}
}

ScriptEngine::ScriptEngine (Wallpapers::CScene& scene) : m_scene (scene) {
    this->m_runtime = JS_NewRuntime ();

    if (!this->m_runtime) {
	sLog.exception ("ScriptEngine: Failed to create JS runtime");
    }

    // debug leaks on termination
    JS_SetDumpFlags (this->m_runtime, JS_DUMP_LEAKS);

    this->m_context = JS_NewContext (this->m_runtime);

    if (!this->m_context) {
	JS_FreeRuntime (this->m_runtime);
	sLog.exception ("ScriptEngine: Failed to create JS context");
    }

    this->m_globalThis = JS_GetGlobalObject (this->m_context);

    this->m_adapters = {
	.vec4 = std::unique_ptr<Adapters::VectorAdapter<4>> (new Adapters::VectorAdapter<4> (*this)),
	.vec3 = std::unique_ptr<Adapters::VectorAdapter<3>> (new Adapters::VectorAdapter<3> (*this)),
	.vec2 = std::unique_ptr<Adapters::VectorAdapter<2>> (new Adapters::VectorAdapter<2> (*this)),
	.object
	= std::unique_ptr<Adapters::ScriptableObjectAdapter> (new Adapters::ScriptableObjectAdapter (*this, "ILayer")),
    };

    this->m_engineObject = std::make_unique<EngineObject> (*this, scene);
    this->m_inputObject = std::make_unique<InputObject> (*this, scene);
    this->m_sceneObject = std::make_unique<SceneObject> (*this, scene);
    this->m_consoleObject = std::make_unique<ConsoleObject> (*this, scene);
    this->m_scriptPropertiesObject = std::make_unique<ScriptPropertiesObject> (*this, scene);

    auto wemath = std::make_unique<Modules::MathModule> (*this);
    auto wecolor = std::make_unique<Modules::ColorModule> (*this);

    this->m_modules.emplace (wemath->getName (), std::move (wemath));
    this->m_modules.emplace (wecolor->getName (), std::move (wecolor));

    JS_SetModuleLoaderFunc (this->m_runtime, nullptr, scriptengine_module_loader, this);
    // setup scene objects and other things
    this->installBuiltins ();
    // add engine to the global
    JS_DefinePropertyValueStr (
	this->m_context, this->m_globalThis, "engine", this->m_engineObject->getInstance (), JS_PROP_ENUMERABLE
    );
    JS_DefinePropertyValueStr (
	this->m_context, this->m_globalThis, "input", this->m_inputObject->getInstance (), JS_PROP_ENUMERABLE
    );
    JS_DefinePropertyValueStr (
	this->m_context, this->m_globalThis, "thisScene", this->m_sceneObject->getInstance (), JS_PROP_ENUMERABLE
    );
    JS_DefinePropertyValueStr (
	this->m_context, this->m_globalThis, "console", this->m_consoleObject->getInstance (), JS_PROP_ENUMERABLE
    );
    JS_DefinePropertyValueStr (
	this->m_context, this->m_globalThis, "shared", JS_NewObject (this->m_context), JS_PROP_ENUMERABLE
    );
}

ScriptEngine::~ScriptEngine () {
    if (this->m_mediaPollFuture.valid ()) {
	this->m_mediaPollFuture.wait ();
    }

    for (const auto& module : this->m_scriptModules | std::views::values) {
	JS_FreeValue (this->m_context, module.module);
    }

    JS_FreeValue (this->m_context, this->m_globalThis);

    this->m_adapters.vec4.reset ();
    this->m_adapters.vec3.reset ();
    this->m_adapters.vec2.reset ();
    this->m_adapters.object.reset ();

    this->m_consoleObject.reset ();
    this->m_engineObject.reset ();
    this->m_inputObject.reset ();
    this->m_sceneObject.reset ();
    this->m_scriptPropertiesObject.reset ();
    this->m_modules.clear ();
    this->m_scriptModules.clear ();

    if (this->m_context) {
	JS_FreeContext (this->m_context);
    }
    if (this->m_runtime) {
	JS_FreeRuntime (this->m_runtime);
    }
}

/// Helper to check for and log JS exceptions
static void logJSException (JSContext* ctx, const char* context) {
    JSValue exc = JS_GetException (ctx);
    if (!JS_IsNull (exc) && !JS_IsUndefined (exc)) {
	const char* str = JS_ToCString (ctx, exc);
	if (str) {
	    sLog.error ("ScriptEngine [", context, "]: ", str);
	    JS_FreeCString (ctx, str);
	}
    }
    JS_FreeValue (ctx, exc);
}

void ScriptEngine::installBuiltins () {
    if (this->m_builtinsInstalled || !this->m_context) {
	return;
    }

    JSValue result = JS_Eval (
	this->m_context, SCENE_SCRIPT_BUILTINS, strlen (SCENE_SCRIPT_BUILTINS), "<scene-script-builtins>",
	JS_EVAL_TYPE_GLOBAL
    );
    if (JS_IsException (result)) {
	logJSException (this->m_context, "installBuiltins");
    }
    JS_FreeValue (this->m_context, result);
    this->m_builtinsInstalled = true;
}

void ScriptEngine::refreshMediaState () {
    auto buildMediaState = [] () -> std::optional<MediaState> {
	const auto line = processReadFirstLine (
	    "playerctl",
	    { "metadata", "--format",
	      "{{status}}\t{{title}}\t{{artist}}\t{{mpris:length}}\t{{position}}\t{{mpris:artUrl}}" }
	);

	if (!line.has_value ()) {
	    return std::nullopt;
	}

	MediaState next;
	if (line->empty ()) {
	    next.available = false;
	    next.playbackState = 0;
	    next.status = "Stopped";
	    return next;
	}

	const auto parts = splitTabs (*line);
	next.available = true;
	next.status = parts.size () > 0 ? parts[0] : "";
	next.title = parts.size () > 1 ? parts[1] : "";
	next.artist = parts.size () > 2 ? parts[2] : "";
	next.duration = (parts.size () > 3 ? parseDoubleOrZero (parts[3]) : 0.0) / 1000000.0;
	next.position = (parts.size () > 4 ? parseDoubleOrZero (parts[4]) : 0.0) / 1000000.0;
	next.artUrl = parts.size () > 5 ? parts[5] : "";

	if (next.status == "Playing") {
	    next.playbackState = 1;
	} else if (next.status == "Paused") {
	    next.playbackState = 2;
	} else {
	    next.playbackState = 0;
	}

	return next;
    };

    if (this->m_mediaPollFuture.valid ()
	&& this->m_mediaPollFuture.wait_for (std::chrono::milliseconds (0)) == std::future_status::ready) {
	try {
	    auto next = this->m_mediaPollFuture.get ();
	    if (next.has_value ()) {
		this->m_mediaState = std::move (*next);
		if (std::getenv ("LWE_MEDIA_DEBUG") != nullptr) {
		    if (!this->m_mediaState.available) {
			sLog.out ("Media debug: no playerctl media state");
		    } else {
			sLog.out (
			    "Media debug: status=", this->m_mediaState.status, " title=", this->m_mediaState.title,
			    " artist=", this->m_mediaState.artist, " duration=", this->m_mediaState.duration,
			    " position=", this->m_mediaState.position
			);
		    }
		}
	    }
	} catch (const std::exception& e) {
	    if (std::getenv ("LWE_MEDIA_DEBUG") != nullptr) {
		sLog.out ("Media debug: playerctl media poll failed: ", e.what ());
	    }
	}
    }

    if (this->m_mediaPollFuture.valid ()) {
	return;
    }

    const auto now = std::chrono::steady_clock::now ();
    if (this->m_lastMediaPoll.time_since_epoch ().count () != 0
	&& now - this->m_lastMediaPoll < std::chrono::milliseconds (750)) {
	return;
    }
    this->m_lastMediaPoll = now;
    this->m_mediaPollFuture = std::async (std::launch::async, buildMediaState);
}

static JSValue makeVec3Object (JSContext* ctx, float x, float y, float z) {
    JSValue result = JS_NewObject (ctx);
    JS_SetPropertyStr (ctx, result, "x", JS_NewFloat64 (ctx, x));
    JS_SetPropertyStr (ctx, result, "y", JS_NewFloat64 (ctx, y));
    JS_SetPropertyStr (ctx, result, "z", JS_NewFloat64 (ctx, z));
    return result;
}

static bool
callModuleFunction (JSContext* ctx, JSValue module, const char* name, JSValue event, const char* logContext) {
    JSValue function = JS_GetPropertyStr (ctx, module, name);
    if (JS_IsFunction (ctx, function)) {
	JSValue args[] = { event };
	JSValue result = JS_Call (ctx, function, JS_UNDEFINED, 1, args);
	if (JS_IsException (result)) {
	    logJSException (ctx, logContext);
	}
	JS_FreeValue (ctx, result);
	JS_FreeValue (ctx, function);
	return true;
    }
    JS_FreeValue (ctx, function);
    return false;
}

void ScriptEngine::dispatchMediaEvents (JSValue module, const void* bindingKey) {
    this->refreshMediaState ();
    JSContext* ctx = this->m_context;

    const std::string propertiesSignature = this->m_mediaState.title + "\n" + this->m_mediaState.artist;
    if (this->m_lastMediaProperties.find (bindingKey) == this->m_lastMediaProperties.end ()
	|| this->m_lastMediaProperties[bindingKey] != propertiesSignature) {
	this->m_lastMediaProperties[bindingKey] = propertiesSignature;
	JSValue propertiesEvent = JS_NewObject (ctx);
	JS_SetPropertyStr (ctx, propertiesEvent, "title", JS_NewString (ctx, this->m_mediaState.title.c_str ()));
	JS_SetPropertyStr (ctx, propertiesEvent, "artist", JS_NewString (ctx, this->m_mediaState.artist.c_str ()));
	JS_SetPropertyStr (ctx, propertiesEvent, "albumTitle", JS_NewString (ctx, ""));
	const bool calledProperties
	    = callModuleFunction (ctx, module, "mediaPropertiesChanged", propertiesEvent, "mediaPropertiesChanged");
	if (calledProperties && std::getenv ("LWE_MEDIA_DEBUG") != nullptr) {
	    sLog.out (
		"Media debug: dispatched properties key=", reinterpret_cast<uintptr_t> (bindingKey),
		" title=", this->m_mediaState.title, " artist=", this->m_mediaState.artist
	    );
	}
	JS_FreeValue (ctx, propertiesEvent);
    }

    const std::string playbackSignature = std::to_string (this->m_mediaState.playbackState);
    if (this->m_lastMediaPlayback[bindingKey] != playbackSignature) {
	this->m_lastMediaPlayback[bindingKey] = playbackSignature;
	JSValue event = JS_NewObject (ctx);
	JS_SetPropertyStr (ctx, event, "state", JS_NewInt32 (ctx, this->m_mediaState.playbackState));
	callModuleFunction (ctx, module, "mediaPlaybackChanged", event, "mediaPlaybackChanged");
	JS_FreeValue (ctx, event);
    }

    const int roundedPosition = static_cast<int> (std::max (0.0, this->m_mediaState.position));
    const std::string timelineSignature
	= std::to_string (roundedPosition) + "\n" + std::to_string (this->m_mediaState.duration);
    if (this->m_lastMediaTimeline.find (bindingKey) == this->m_lastMediaTimeline.end ()
	|| this->m_lastMediaTimeline[bindingKey] != timelineSignature) {
	this->m_lastMediaTimeline[bindingKey] = timelineSignature;
	JSValue timelineEvent = JS_NewObject (ctx);
	JS_SetPropertyStr (ctx, timelineEvent, "position", JS_NewFloat64 (ctx, this->m_mediaState.position));
	JS_SetPropertyStr (ctx, timelineEvent, "duration", JS_NewFloat64 (ctx, this->m_mediaState.duration));
	callModuleFunction (ctx, module, "mediaTimelineChanged", timelineEvent, "mediaTimelineChanged");
	JS_FreeValue (ctx, timelineEvent);
    }

    if (this->m_lastMediaThumbnail[bindingKey] != this->m_mediaState.artUrl) {
	this->m_lastMediaThumbnail[bindingKey] = this->m_mediaState.artUrl;
	JSValue event = JS_NewObject (ctx);
	JS_SetPropertyStr (ctx, event, "url", JS_NewString (ctx, this->m_mediaState.artUrl.c_str ()));
	JS_SetPropertyStr (ctx, event, "primaryColor", makeVec3Object (ctx, 0.12f, 0.12f, 0.12f));
	JS_SetPropertyStr (ctx, event, "secondaryColor", makeVec3Object (ctx, 0.0f, 0.0f, 0.0f));
	JS_SetPropertyStr (ctx, event, "tertiaryColor", makeVec3Object (ctx, 0.25f, 0.25f, 0.25f));
	JS_SetPropertyStr (ctx, event, "highContrastColor", makeVec3Object (ctx, 1.0f, 1.0f, 1.0f));
	callModuleFunction (ctx, module, "mediaThumbnailChanged", event, "mediaThumbnailChanged");
	JS_FreeValue (ctx, event);
    }
}

void ScriptEngine::callModuleWithProps (JSContext* ctx, JSValue module, const char* name, JSValue propsObj) const {
    JSValue function = JS_GetPropertyStr (ctx, module, name);
    if (JS_IsFunction (ctx, function)) {
	JSValue args[] = { propsObj };
	JSValue result = JS_Call (ctx, function, JS_UNDEFINED, 1, args);
	if (JS_IsException (result)) {
	    logJSException (ctx, name);
	}
	JS_FreeValue (ctx, result);
    }
    JS_FreeValue (ctx, function);
}

void ScriptEngine::runIntervals (JSContext* ctx, JSValue globalObj, const std::string& bindingKeyString) const {
    JSValue runIntervals = JS_GetPropertyStr (ctx, globalObj, "__weRunIntervals");
    if (JS_IsFunction (ctx, runIntervals)) {
	JSValue args[] = { JS_NewString (ctx, bindingKeyString.c_str ()) };
	JSValue intervalResult = JS_Call (ctx, runIntervals, JS_UNDEFINED, 1, args);
	if (JS_IsException (intervalResult)) {
	    logJSException (ctx, "setInterval");
	}
	JS_FreeValue (ctx, args[0]);
	JS_FreeValue (ctx, intervalResult);
    }
    JS_FreeValue (ctx, runIntervals);
}

DynamicValueUniquePtr ScriptEngine::fallbackTextValue (const std::string& scriptSource) const {
    auto fallback = std::make_unique<DynamicValue> ();
    if (scriptSource.find ("event.title") != std::string::npos) {
	fallback->update (shortenMediaText (this->m_mediaState.title, 32), DynamicValue::UpdateSource::Script);
    } else if (scriptSource.find ("event.artist") != std::string::npos) {
	fallback->update (shortenMediaText (this->m_mediaState.artist, 28), DynamicValue::UpdateSource::Script);
    } else if (
	scriptSource.find ("event.position") != std::string::npos
	|| scriptSource.find ("displayPosition") != std::string::npos
    ) {
	fallback->update (formatMediaTime (this->m_mediaState.position), DynamicValue::UpdateSource::Script);
    } else if (scriptSource.find ("event.duration") != std::string::npos) {
	fallback->update (formatMediaTime (this->m_mediaState.duration), DynamicValue::UpdateSource::Script);
    }
    return fallback;
}

void ScriptEngine::applyTextFallback (DynamicValue& value, const std::string& scriptSource) const {
    auto fallback = this->fallbackTextValue (scriptSource);
    if (fallback->getType () != DynamicValue::Null) {
	value.update (*fallback, DynamicValue::UpdateSource::Script);
    }
}

// ---------------------------------------------------------------------------
// Layer-script API (Phase 2)
// ---------------------------------------------------------------------------

void ScriptEngine::ensureLayerRegistry () {
    if (this->m_layerRegistryReady || !this->m_context) {
	return;
    }
    JSContext* ctx = this->m_context;
    JSValue globalObj = JS_GetGlobalObject (ctx);
    JS_SetPropertyStr (ctx, globalObj, "__textLayers", JS_NewObject (ctx));
    JS_FreeValue (ctx, globalObj);
    this->m_layerRegistryReady = true;
}

ScriptLayerHandle ScriptEngine::createLayerScript (
    const std::string& scriptSource, std::map<std::string, UserSettingUniquePtr>& initialScriptProps,
    const std::string& initialText
) {
    if (!this->m_context) {
	sLog.error ("ScriptEngine: No JS context available");
	return kInvalidLayerHandle;
    }

    this->ensureLayerRegistry ();

    JSContext* ctx = this->m_context;
    JSValue globalObj = JS_GetGlobalObject (ctx);

    // Seed initial scriptProperties and text as temporary globals the IIFE reads.
    JSValue seedProps = JS_NewObject (ctx);

    for (auto& [name, dynVal] : initialScriptProps) {
	JS_SetPropertyStr (ctx, seedProps, name.c_str (), this->dynamicToJs (*dynVal->value));
    }

    JS_SetPropertyStr (ctx, globalObj, "__layerSeedProps", seedProps);
    JS_SetPropertyStr (ctx, globalObj, "__layerSeedText", JS_NewString (ctx, initialText.c_str ()));

    const ScriptLayerHandle id = this->m_nextLayerId++;

    // Same stripping logic as evaluate(): WE scripts come as ES6 modules but
    // QuickJS is easier to drive as plain script evaluation.
    std::string body = scriptSource;
    size_t pos;
    while ((pos = body.find ("'use strict';")) != std::string::npos) {
	body.erase (pos, 13);
    }
    while ((pos = body.find ("\"use strict\";")) != std::string::npos) {
	body.erase (pos, 13);
    }
    while ((pos = body.find ("export ")) != std::string::npos) {
	body.erase (pos, 7);
    }

    // The IIFE gives every layer its own closure for top-level vars and
    // functions, so two layers that both define `function update()` or a
    // top-level `var scriptProperties` don't clobber each other. Lifecycle
    // hooks are captured into globalThis.__textLayers[id] so tick/destroy can
    // reach them later. `typeof init === 'function'` is safe even when
    // `init` was never declared — bare-identifier `typeof` never throws.
    std::ostringstream wrapper;
    wrapper
	<< "(function() {\n"
	<< "  var __id = " << id << ";\n"
	<< "  var __props = Object.assign({}, globalThis.__layerSeedProps || {});\n"
	<< "  var thisLayer = { text: String(globalThis.__layerSeedText || '') };\n"
	<< "  var thisScene = {\n"
	<< "    get time()        { var c = globalThis.__sceneCtx; return c ? c.time : 0; },\n"
	<< "    get currentTime() { var c = globalThis.__sceneCtx; return c ? c.time : 0; },\n"
	<< "    get dt()          { var c = globalThis.__sceneCtx; return c ? c.dt   : 0; },\n"
	<< "    get fps()         { var c = globalThis.__sceneCtx; return c ? c.fps  : 60; },\n"
	<< "  };\n"
	// Minimal WE `engine` shim. Real Wallpaper Engine exposes a broad API
	// (media events, audio buffer, user input); we provide just enough for
	// the common built-in text scripts to run without ReferenceError.
	// `frametime` is the per-frame delta in seconds (what InsertFPS reads).
	<< "  var engine = {\n"
	<< "    get frametime() { var c = globalThis.__sceneCtx; return c ? c.dt : 0; },\n"
	<< "    get time()      { var c = globalThis.__sceneCtx; return c ? c.time : 0; },\n"
	<< "  };\n"
	<< "  function createScriptProperties() {\n"
	<< "    var builder = {\n"
	<< "      addSlider:   function(o){ if (!(o.name in __props)) __props[o.name] = o.value; return builder; },\n"
	<< "      addCheckbox: function(o){ if (!(o.name in __props)) __props[o.name] = o.value; return builder; },\n"
	<< "      addCombo:    function(o){ if (!(o.name in __props)) __props[o.name] = o.value; return builder; },\n"
	<< "      addColor:    function(o){ if (!(o.name in __props)) __props[o.name] = o.value; return builder; },\n"
	<< "      addText:     function(o){ if (!(o.name in __props)) __props[o.name] = o.value; return builder; },\n"
	<< "      finish:      function(){ return __props; }\n"
	<< "    };\n"
	<< "    return builder;\n"
	<< "  }\n"
	<< body
	<< "\n"
	// `_tick` wraps the user's `update()` so both WE text conventions work:
	//   A) `export function update() { thisLayer.text = …; }` (mutates in place)
	//   B) `export function update(value) { …; return value; }` (returns new text)
	// We pass the current text in, and if the return value is a string we
	// adopt it as the new `thisLayer.text`. Non-string / undefined return
	// leaves `thisLayer.text` as whatever the function assigned itself.
	<< "  globalThis.__textLayers[__id] = {\n"
	<< "    thisLayer: thisLayer,\n"
	<< "    thisScene: thisScene,\n"
	<< "    _init:    (typeof init    === 'function') ? init    : null,\n"
	<< "    _destroy: (typeof destroy === 'function') ? destroy : null,\n"
	<< "    _tick:    (typeof update  === 'function')\n"
	<< "              ? function() {\n"
	<< "                  var r = update(thisLayer.text);\n"
	<< "                  if (typeof r === 'string') thisLayer.text = r;\n"
	<< "                }\n"
	<< "              : null,\n"
	<< "    _scriptProperties: (typeof scriptProperties !== 'undefined') ? scriptProperties : __props\n"
	<< "  };\n"
	<< "})();\n";

    const std::string evalScript = wrapper.str ();
    JSValue result = JS_Eval (ctx, evalScript.c_str (), evalScript.size (), "<layer-script>", JS_EVAL_TYPE_GLOBAL);

    // Unset seeds so they don't leak into the next createLayerScript call.
    JS_SetPropertyStr (ctx, globalObj, "__layerSeedProps", JS_UNDEFINED);
    JS_SetPropertyStr (ctx, globalObj, "__layerSeedText", JS_UNDEFINED);
    JS_FreeValue (ctx, globalObj);

    if (JS_IsException (result)) {
	logJSException (ctx, "createLayerScript");
	JS_FreeValue (ctx, result);
	return kInvalidLayerHandle;
    }
    JS_FreeValue (ctx, result);

    this->m_layerInitialized[id] = false;
    return id;
}

void ScriptEngine::tickLayer (ScriptLayerHandle handle, double time, double deltaTime, double fps) {
    if (!this->m_context || handle == kInvalidLayerHandle) {
	return;
    }
    JSContext* ctx = this->m_context;
    JSValue globalObj = JS_GetGlobalObject (ctx);

    JSValue sceneCtx = JS_NewObject (ctx);
    JS_SetPropertyStr (ctx, sceneCtx, "time", JS_NewFloat64 (ctx, time));
    JS_SetPropertyStr (ctx, sceneCtx, "dt", JS_NewFloat64 (ctx, deltaTime));
    JS_SetPropertyStr (ctx, sceneCtx, "fps", JS_NewFloat64 (ctx, fps));
    JS_SetPropertyStr (ctx, globalObj, "__sceneCtx", sceneCtx);

    JSValue layers = JS_GetPropertyStr (ctx, globalObj, "__textLayers");
    JSValue layerObj = JS_GetPropertyUint32 (ctx, layers, static_cast<uint32_t> (handle));
    JS_FreeValue (ctx, layers);

    if (JS_IsUndefined (layerObj) || JS_IsNull (layerObj)) {
	JS_FreeValue (ctx, layerObj);
	JS_FreeValue (ctx, globalObj);
	return;
    }

    auto callHook = [&] (const char* prop, const char* tag) {
	JSValue fn = JS_GetPropertyStr (ctx, layerObj, prop);
	if (JS_IsFunction (ctx, fn)) {
	    JSValue ret = JS_Call (ctx, fn, layerObj, 0, nullptr);
	    if (JS_IsException (ret)) {
		logJSException (ctx, tag);
	    }
	    JS_FreeValue (ctx, ret);
	}
	JS_FreeValue (ctx, fn);
    };

    auto it = this->m_layerInitialized.find (handle);
    if (it != this->m_layerInitialized.end () && !it->second) {
	callHook ("_init", "layer.init");
	it->second = true;
    }
    callHook ("_tick", "layer.update");

    JS_FreeValue (ctx, layerObj);
    JS_FreeValue (ctx, globalObj);
}

std::string ScriptEngine::layerText (ScriptLayerHandle handle) {
    if (!this->m_context || handle == kInvalidLayerHandle) {
	return {};
    }
    JSContext* ctx = this->m_context;
    JSValue globalObj = JS_GetGlobalObject (ctx);
    JSValue layers = JS_GetPropertyStr (ctx, globalObj, "__textLayers");
    JSValue layerObj = JS_GetPropertyUint32 (ctx, layers, static_cast<uint32_t> (handle));

    std::string result;
    if (!JS_IsUndefined (layerObj) && !JS_IsNull (layerObj)) {
	JSValue thisLayer = JS_GetPropertyStr (ctx, layerObj, "thisLayer");
	JSValue textVal = JS_GetPropertyStr (ctx, thisLayer, "text");
	if (!JS_IsUndefined (textVal) && !JS_IsNull (textVal)) {
	    const char* cstr = JS_ToCString (ctx, textVal);
	    if (cstr) {
		result.assign (cstr);
		JS_FreeCString (ctx, cstr);
	    }
	}
	JS_FreeValue (ctx, textVal);
	JS_FreeValue (ctx, thisLayer);
    }

    JS_FreeValue (ctx, layerObj);
    JS_FreeValue (ctx, layers);
    JS_FreeValue (ctx, globalObj);
    return result;
}

void ScriptEngine::destroyLayer (ScriptLayerHandle handle) {
    if (!this->m_context || handle == kInvalidLayerHandle) {
	return;
    }
    JSContext* ctx = this->m_context;
    JSValue globalObj = JS_GetGlobalObject (ctx);
    JSValue layers = JS_GetPropertyStr (ctx, globalObj, "__textLayers");
    JSValue layerObj = JS_GetPropertyUint32 (ctx, layers, static_cast<uint32_t> (handle));

    if (!JS_IsUndefined (layerObj) && !JS_IsNull (layerObj)) {
	JSValue fn = JS_GetPropertyStr (ctx, layerObj, "_destroy");
	if (JS_IsFunction (ctx, fn)) {
	    JSValue ret = JS_Call (ctx, fn, layerObj, 0, nullptr);
	    if (JS_IsException (ret)) {
		logJSException (ctx, "layer.destroy");
	    }
	    JS_FreeValue (ctx, ret);
	}
	JS_FreeValue (ctx, fn);
    }
    JS_FreeValue (ctx, layerObj);
    JS_FreeValue (ctx, layers);
    JS_FreeValue (ctx, globalObj);

    // Remove the entry from globalThis.__textLayers so GC can reclaim its closures.
    const std::string delScript = "delete globalThis.__textLayers[" + std::to_string (handle) + "];";
    JSValue delResult = JS_Eval (ctx, delScript.c_str (), delScript.size (), "<layer-destroy>", JS_EVAL_TYPE_GLOBAL);
    if (JS_IsException (delResult)) {
	logJSException (ctx, "layer.destroy.delete");
    }
    JS_FreeValue (ctx, delResult);

    this->m_layerInitialized.erase (handle);
}

JSValue ScriptEngine::call (JSValue module, int argc, JSValue argv[], const char* name) {
    // check if there's an update method and run it
    JSValue function = JS_GetPropertyStr (this->m_context, module, name);
    ScopeGuard guard ([&] () { JS_FreeValue (this->m_context, function); });

    if (!JS_IsFunction (this->m_context, function)) {
	return JS_UNDEFINED;
    }

    return JS_Call (this->m_context, function, module, argc, argv);
}

void ScriptEngine::queueScript (const std::string& key, DynamicValue& currentValue, ScriptableObject& object) {
    const auto source = currentValue.getScriptSource ();

    if (!source.has_value ()) {
	return;
    }

    auto it = this->m_scriptModules.find (key);

    if (it != this->m_scriptModules.end ()) {
	return;
    }

    // load the script and store it
    JSValue module = JS_Eval (this->m_context, source->c_str (), source->size (), key.c_str (), JS_EVAL_TYPE_MODULE);

    auto inserted = this->m_scriptModules.emplace (
	key,
	LoadedModule {
	    .value = currentValue,
	    .module = module,
	}
    );

    if (!inserted.second) {
	return;
    }

    JS_SetPropertyStr (this->m_context, this->m_globalThis, "thisLayer", this->m_adapters.object->instantiate (object));

    // script properties do not need update as they're connected directly to the source data
    this->m_runningModule = &inserted.first->second;

    // check if there's an update method and run it
    JSValue args[] = { this->dynamicToJs (currentValue) };
    JSValue result = this->call (module, 1, args, "update");

    ScopeGuard guard2 ([this, args, result] () {
	JS_FreeValue (this->m_context, result);
	JS_FreeValue (this->m_context, args[0]);
    });

    if (JS_IsException (result)) {
	return;
    }

    jsToDynamicValue (this->m_context, result, currentValue);
}

void ScriptEngine::tick () {
    // run intervals
    this->m_engineObject->tick ();

    // run any pending notifications

    // run all update methods
    for (auto& module : this->m_scriptModules | std::views::values) {
	this->m_runningModule = &module;

	JSValue args[] = { this->dynamicToJs (module.value) };
	JSValue result = this->call (module.module, 1, args, "update");
	ScopeGuard guard ([result, args, this] () {
	    JS_FreeValue (this->m_context, result);
	    JS_FreeValue (this->m_context, args[0]);
	});

	if (JS_IsException (result)) {
	    continue;
	}

	jsToDynamicValue (this->m_context, result, module.value);
    }
}