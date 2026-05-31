#include "ScriptEngine.h"

#include "ScriptableObject.h"
#include "WallpaperEngine/Audio/AudioContext.h"
#include "WallpaperEngine/Audio/Drivers/Recorders/PlaybackRecorder.h"
#include "WallpaperEngine/Data/Utils/ScopeGuard.h"
#include "WallpaperEngine/Logging/Log.h"
#include "WallpaperEngine/Render/CObject.h"
#include "WallpaperEngine/Render/Objects/CSound.h"
#include "WallpaperEngine/Render/Wallpapers/CScene.h"
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

// forward defs
JSValue property_get (JSContext* ctx, JSValueConst obj_val, JSAtom atom, JSValueConst receiver);
int property_set (
    JSContext* ctx, JSValueConst obj_val, JSAtom atom, JSValueConst val, JSValueConst receiver, int flags
);

static std::unique_ptr<ScriptEngine> sScriptEngine;
static const auto sStartTime = std::chrono::steady_clock::now ();
// TODO: SEPARATE ALL THE ENGINE CODE INTO SEPARATE FILES BASED ON ROLE AN RESPONSIBILITY
static JSClassID ScriptableObjectClassId;
// TODO: HAVE ONE FOR EACH ACTUAL TYPE
struct JSClassExoticMethods exotic_methods = {
    .get_property = property_get,
    .set_property = property_set,
};
static JSClassDef def {
    .class_name = "ScriptableObject",
};

static JSValue constructVectorObject (JSContext* ctx, const char* name, const std::vector<double>& values) {
    auto constructPlainObject = [&] () {
	JSValue obj = JS_NewObject (ctx);
	if (!values.empty ()) {
	    JS_SetPropertyStr (ctx, obj, "x", JS_NewFloat64 (ctx, values[0]));
	}
	if (values.size () > 1) {
	    JS_SetPropertyStr (ctx, obj, "y", JS_NewFloat64 (ctx, values[1]));
	}
	if (values.size () > 2) {
	    JS_SetPropertyStr (ctx, obj, "z", JS_NewFloat64 (ctx, values[2]));
	}
	if (values.size () > 3) {
	    JS_SetPropertyStr (ctx, obj, "w", JS_NewFloat64 (ctx, values[3]));
	}
	return obj;
    };

    JSValue global = JS_GetGlobalObject (ctx);
    JSValue ctor = JS_GetPropertyStr (ctx, global, name);
    JS_FreeValue (ctx, global);

    if (!JS_IsFunction (ctx, ctor)) {
	JS_FreeValue (ctx, ctor);
	return constructPlainObject ();
    }

    std::vector<JSValue> args;
    args.reserve (values.size ());
    for (const auto& v : values) {
	args.emplace_back (JS_NewFloat64 (ctx, v));
    }

    JSValue obj = JS_CallConstructor (ctx, ctor, args.size (), args.data ());
    for (auto& arg : args) {
	JS_FreeValue (ctx, arg);
    }
    JS_FreeValue (ctx, ctor);
    if (JS_IsException (obj)) {
	JSValue exception = JS_GetException (ctx);
	JS_FreeValue (ctx, exception);
	return constructPlainObject ();
    }
    return obj;
}

static JSValue dynamicValueToJS (JSContext* ctx, const DynamicValue& value) {
    switch (value.getType ()) {
	case DynamicValue::String:
	    return JS_NewString (ctx, value.getString ().c_str ());
	case DynamicValue::Float:
	    return JS_NewFloat64 (ctx, value.getFloat ());
	case DynamicValue::Int:
	    return JS_NewInt32 (ctx, value.getInt ());
	case DynamicValue::Boolean:
	    return JS_NewBool (ctx, value.getBool ());
	case DynamicValue::Vec2:
	    return constructVectorObject (ctx, "Vec2", { value.getVec2 ().x, value.getVec2 ().y });
	case DynamicValue::Vec3:
	    return constructVectorObject (ctx, "Vec3", { value.getVec3 ().x, value.getVec3 ().y, value.getVec3 ().z });
	case DynamicValue::Vec4:
	    return constructVectorObject (
		ctx, "Vec4", { value.getVec4 ().x, value.getVec4 ().y, value.getVec4 ().z, value.getVec4 ().w }
	    );
	case DynamicValue::IVec2:
	    return constructVectorObject (ctx, "Vec2", { value.getIVec2 ().x, value.getIVec2 ().y });
	case DynamicValue::IVec3:
	    return constructVectorObject (
		ctx, "Vec3", { value.getIVec3 ().x, value.getIVec3 ().y, value.getIVec3 ().z }
	    );
	case DynamicValue::IVec4:
	    return constructVectorObject (
		ctx, "Vec4", { value.getIVec4 ().x, value.getIVec4 ().y, value.getIVec4 ().z, value.getIVec4 ().w }
	    );
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
	int xTag = JS_VALUE_GET_TAG (x);
	int yTag = JS_VALUE_GET_TAG (y);
	int zTag = JS_VALUE_GET_TAG (z);
	int wTag = JS_VALUE_GET_TAG (w);
	ScopeGuard guard ([=] {
	    JS_FreeValue (ctx, x);
	    JS_FreeValue (ctx, y);
	    JS_FreeValue (ctx, z);
	    JS_FreeValue (ctx, w);
	});

	if (xTag == JS_TAG_UNDEFINED || xTag == JS_TAG_UNINITIALIZED || xTag == JS_TAG_NULL || yTag == JS_TAG_UNDEFINED
	    || yTag == JS_TAG_UNINITIALIZED || yTag == JS_TAG_NULL) {
	    sLog.exception ("Vector's x and y components must have a value");
	}

	if (xTag == JS_TAG_FLOAT64 || yTag == JS_TAG_FLOAT64 || zTag == JS_TAG_FLOAT64 || wTag == JS_TAG_FLOAT64) {
	    float xVal = 0.0f, yVal = 0.0f, zVal = 0.0f, wVal = 0.0f;

	    // TODO: DO WE REALLY NEED ALL THIS CASTING? WOULD CHECKING TYPE AND ACCESSING INT/FLOAT VERSION BE ENOUGH?
	    xVal = xTag == JS_TAG_FLOAT64 ? JS_VALUE_GET_FLOAT64 (x) : JS_VALUE_GET_INT (x);
	    yVal = yTag == JS_TAG_FLOAT64 ? JS_VALUE_GET_FLOAT64 (y) : JS_VALUE_GET_INT (y);

	    if (zTag != JS_TAG_UNDEFINED && zTag != JS_TAG_UNINITIALIZED && zTag != JS_TAG_NULL) {
		zVal = zTag == JS_TAG_FLOAT64 ? JS_VALUE_GET_FLOAT64 (z) : JS_VALUE_GET_INT (z);
	    }

	    if (wTag != JS_TAG_UNDEFINED && wTag != JS_TAG_UNINITIALIZED && wTag != JS_TAG_NULL) {
		wVal = wTag == JS_TAG_FLOAT64 ? JS_VALUE_GET_FLOAT64 (w) : JS_VALUE_GET_INT (w);
	    }

	    // any float component means float vector
	    if (wTag != JS_TAG_UNDEFINED && wTag != JS_TAG_NULL) {
		source.update (glm::vec4 (xVal, yVal, zVal, wVal), DynamicValue::UpdateSource::Script);
		return;
	    }

	    if (zTag != JS_TAG_UNDEFINED && zTag != JS_TAG_NULL) {
		source.update (glm::vec3 (xVal, yVal, zVal), DynamicValue::UpdateSource::Script);
		return;
	    }

	    source.update (glm::vec2 (xVal, yVal), DynamicValue::UpdateSource::Script);
	} else {
	    int xVal = 0, yVal = 0, zVal = 0, wVal = 0;

	    xVal = JS_VALUE_GET_INT (x);
	    yVal = JS_VALUE_GET_INT (y);

	    if (zTag != JS_TAG_UNDEFINED && zTag != JS_TAG_UNINITIALIZED && zTag != JS_TAG_NULL) {
		zVal = JS_VALUE_GET_INT (z);
	    }

	    if (wTag != JS_TAG_UNDEFINED && wTag != JS_TAG_UNINITIALIZED && wTag != JS_TAG_NULL) {
		wVal = JS_VALUE_GET_INT (w);
	    }

	    // all integers is a integer vector
	    if (wTag != JS_TAG_UNDEFINED && wTag != JS_TAG_NULL) {
		source.update (glm::ivec4 (xVal, yVal, zVal, wVal), DynamicValue::UpdateSource::Script);
		return;
	    }

	    if (zTag != JS_TAG_UNDEFINED && zTag != JS_TAG_UNINITIALIZED && zTag != JS_TAG_NULL) {
		source.update (glm::ivec3 (xVal, yVal, zVal), DynamicValue::UpdateSource::Script);
		return;
	    }

	    source.update (glm::ivec2 (xVal, yVal), DynamicValue::UpdateSource::Script);
	}
    }
}

JSValue property_get (JSContext* ctx, JSValueConst obj_val, JSAtom atom, JSValueConst receiver) {
    auto* container = static_cast<ScriptableObject*> (JS_GetOpaque2 (ctx, obj_val, ScriptableObjectClassId));

    if (!container) {
	return JS_EXCEPTION;
    }

    const char* name = JS_AtomToCString (ctx, atom);

    if (name == nullptr) {
	return JS_EXCEPTION;
    }

    ScopeGuard guard ([=] { JS_FreeCString (ctx, name); });

    return dynamicValueToJS (ctx, container->getProperty (name));
}

int property_set (
    JSContext* ctx, JSValueConst obj_val, JSAtom atom, JSValueConst val, JSValueConst receiver, int flags
) {
    auto* container = static_cast<ScriptableObject*> (JS_GetOpaque2 (ctx, obj_val, ScriptableObjectClassId));

    if (!container) {
	return -1;
    }

    const char* name = JS_AtomToCString (ctx, atom);

    if (name == nullptr) {
	return -1;
    }

    ScopeGuard guard ([=] { JS_FreeCString (ctx, name); });

    jsToDynamicValue (ctx, val, container->getProperty (name));

    return 0;
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

ScriptEngine& ScriptEngine::instance () {
    if (!sScriptEngine) {
	sScriptEngine = std::unique_ptr<ScriptEngine> (new ScriptEngine ());
    }
    return *sScriptEngine;
}

ScriptEngine::ScriptEngine () {
    this->m_runtime = JS_NewRuntime ();

    if (!this->m_runtime) {
	sLog.error ("ScriptEngine: Failed to create JS runtime");
	return;
    }

    this->m_context = JS_NewContext (this->m_runtime);

    if (!this->m_context) {
	sLog.error ("ScriptEngine: Failed to create JS context");
	JS_FreeRuntime (this->m_runtime);
	this->m_runtime = nullptr;
	return;
    }

    JS_NewClassID (this->m_runtime, &ScriptableObjectClassId);
    JS_NewClass (this->m_runtime, ScriptableObjectClassId, &def);
}

ScriptEngine::~ScriptEngine () {
    if (this->m_mediaPollFuture.valid ()) {
	this->m_mediaPollFuture.wait ();
    }

    if (this->m_context) {
	for (const auto& module : this->m_modules | std::views::values) {
	    JS_FreeValue (this->m_context, module);
	}
    }
    this->m_modules.clear ();

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

static void updateAudioArray (JSContext* ctx, JSValue global, const char* name, const float* values, int count) {
    JSValue audio = JS_GetPropertyStr (ctx, global, name);
    if (JS_IsUndefined (audio)) {
	audio = JS_NewObject (ctx);
    }

    auto setChannel = [&] (const char* channel) {
	JSValue array = JS_NewArray (ctx);
	for (int i = 0; i < count; i++) {
	    JS_SetPropertyUint32 (ctx, array, i, JS_NewFloat64 (ctx, values[i]));
	}
	JS_SetPropertyStr (ctx, audio, channel, array);
    };

    setChannel ("average");
    // PlaybackRecorder currently exposes averaged samples only. Mirror that
    // data into WE's stereo fields until separate channels are available.
    setChannel ("left");
    setChannel ("right");

    JS_SetPropertyStr (ctx, global, name, audio);
}

void ScriptEngine::installBuiltins () {
    if (this->m_builtinsInstalled || !this->m_context) {
	return;
    }

    static constexpr const char* builtins = R"JS(
globalThis.__weNum = function(v, fallback) {
  const n = parseFloat(v);
  return Number.isFinite(n) ? n : fallback;
};
globalThis.__weParts = function(v) {
  if (typeof v === 'string') return v.trim().split(/\s+/).map(Number);
  if (typeof v === 'number') return [v, v, v, v];
  if (v && typeof v === 'object') return [v.x || 0, v.y || 0, v.z || 0, v.w || 0];
  return [0, 0, 0, 0];
};
globalThis.Vec2 = class Vec2 {
  constructor(x, y) { const p = __weParts(x); this.x = __weNum(x, p[0] || 0); this.y = __weNum(y, p[1] || 0); }
  copy() { return new Vec2(this.x, this.y); }
  add(v) { const p = __weParts(v); return new Vec2(this.x + p[0], this.y + p[1]); }
  subtract(v) { const p = __weParts(v); return new Vec2(this.x - p[0], this.y - p[1]); }
  multiply(v) { const p = __weParts(v); return new Vec2(this.x * p[0], this.y * p[1]); }
  divide(v) { const p = __weParts(v); return new Vec2(this.x / p[0], this.y / p[1]); }
  toString() { return this.x + ' ' + this.y; }
};
globalThis.Vec3 = class Vec3 {
  constructor(x, y, z) { const p = __weParts(x); this.x = __weNum(x, p[0] || 0); this.y = __weNum(y, p[1] || 0); this.z = __weNum(z, p[2] || 0); }
  copy() { return new Vec3(this.x, this.y, this.z); }
  equals(v) { return Math.abs(this.x - v.x) < 0.0001 && Math.abs(this.y - v.y) < 0.0001 && Math.abs(this.z - v.z) < 0.0001; }
  lengthSqr() { return this.x * this.x + this.y * this.y + this.z * this.z; }
  length() { return Math.sqrt(this.lengthSqr()); }
  normalize() { const l = this.length(); return l ? this.divide(l) : new Vec3(0, 0, 0); }
  add(v) { const p = __weParts(v); return new Vec3(this.x + p[0], this.y + p[1], this.z + p[2]); }
  subtract(v) { const p = __weParts(v); return new Vec3(this.x - p[0], this.y - p[1], this.z - p[2]); }
  multiply(v) { const p = __weParts(v); return new Vec3(this.x * p[0], this.y * p[1], this.z * p[2]); }
  divide(v) { const p = __weParts(v); return new Vec3(this.x / p[0], this.y / p[1], this.z / p[2]); }
  dot(v) { return this.x * v.x + this.y * v.y + this.z * v.z; }
  cross(v) { return new Vec3(this.y * v.z - this.z * v.y, this.z * v.x - this.x * v.z, this.x * v.y - this.y * v.x); }
  mix(v, a) { return new Vec3(this.x + (v.x - this.x) * a, this.y + (v.y - this.y) * a, this.z + (v.z - this.z) * a); }
  min(v) { return new Vec3(Math.min(this.x, v.x), Math.min(this.y, v.y), Math.min(this.z, v.z)); }
  max(v) { return new Vec3(Math.max(this.x, v.x), Math.max(this.y, v.y), Math.max(this.z, v.z)); }
  abs() { return new Vec3(Math.abs(this.x), Math.abs(this.y), Math.abs(this.z)); }
  sign() { return new Vec3(Math.sign(this.x), Math.sign(this.y), Math.sign(this.z)); }
  round() { return new Vec3(Math.round(this.x), Math.round(this.y), Math.round(this.z)); }
  floor() { return new Vec3(Math.floor(this.x), Math.floor(this.y), Math.floor(this.z)); }
  ceil() { return new Vec3(Math.ceil(this.x), Math.ceil(this.y), Math.ceil(this.z)); }
  toString() { return this.x + ' ' + this.y + ' ' + this.z; }
};
globalThis.Vec4 = class Vec4 {
  constructor(x, y, z, w) { const p = __weParts(x); this.x = __weNum(x, p[0] || 0); this.y = __weNum(y, p[1] || 0); this.z = __weNum(z, p[2] || 0); this.w = __weNum(w, p[3] || 0); }
  copy() { return new Vec4(this.x, this.y, this.z, this.w); }
  toString() { return this.x + ' ' + this.y + ' ' + this.z + ' ' + this.w; }
};
globalThis.WEColor = {
  rgb2hsv(c) {
    const r = c.x, g = c.y, b = c.z, max = Math.max(r, g, b), min = Math.min(r, g, b), d = max - min;
    let h = 0;
    if (d !== 0) h = max === r ? (((g - b) / d) % 6) : max === g ? ((b - r) / d + 2) : ((r - g) / d + 4);
    h = ((h / 6) + 1) % 1;
    return new Vec3(h, max === 0 ? 0 : d / max, max);
  },
  hsv2rgb(c) {
    const h = ((c.x % 1) + 1) % 1, s = c.y, v = c.z, i = Math.floor(h * 6), f = h * 6 - i;
    const p = v * (1 - s), q = v * (1 - f * s), t = v * (1 - (1 - f) * s);
    switch (i % 6) { case 0: return new Vec3(v, t, p); case 1: return new Vec3(q, v, p); case 2: return new Vec3(p, v, t); case 3: return new Vec3(p, q, v); case 4: return new Vec3(t, p, v); default: return new Vec3(v, p, q); }
  }
};
globalThis.WEMath = {
  smoothStep(edge0, edge1, x) {
    edge0 = Number(edge0); edge1 = Number(edge1); x = Number(x);
    if (!Number.isFinite(edge0) || !Number.isFinite(edge1) || !Number.isFinite(x)) return 0;
    if (edge0 === edge1) return x < edge0 ? 0 : 1;
    const t = Math.max(0, Math.min(1, (x - edge0) / (edge1 - edge0)));
    return t * t * (3 - 2 * t);
  },
  smootherStep(edge0, edge1, x) {
    edge0 = Number(edge0); edge1 = Number(edge1); x = Number(x);
    if (!Number.isFinite(edge0) || !Number.isFinite(edge1) || !Number.isFinite(x)) return 0;
    if (edge0 === edge1) return x < edge0 ? 0 : 1;
    const t = Math.max(0, Math.min(1, (x - edge0) / (edge1 - edge0)));
    return t * t * t * (t * (t * 6 - 15) + 10);
  },
  clamp(x, min, max) {
    x = Number(x); min = Number(min); max = Number(max);
    if (!Number.isFinite(x)) return 0;
    return Math.max(min, Math.min(max, x));
  },
  saturate(x) {
    x = Number(x);
    if (!Number.isFinite(x)) return 0;
    return Math.max(0, Math.min(1, x));
  },
  mix(a, b, t) {
    a = Number(a); b = Number(b); t = Number(t);
    if (!Number.isFinite(a) || !Number.isFinite(b) || !Number.isFinite(t)) return 0;
    return a + (b - a) * t;
  },
  lerp(a, b, t) {
    a = Number(a); b = Number(b); t = Number(t);
    if (!Number.isFinite(a) || !Number.isFinite(b) || !Number.isFinite(t)) return 0;
    return a + (b - a) * t;
  }
};
globalThis.__audio16 = { average: Array(16).fill(0), left: Array(16).fill(0), right: Array(16).fill(0) };
globalThis.__audio32 = { average: Array(32).fill(0), left: Array(32).fill(0), right: Array(32).fill(0) };
globalThis.__audio64 = { average: Array(64).fill(0), left: Array(64).fill(0), right: Array(64).fill(0) };
globalThis.__intervals = Object.create(null);
globalThis.__weIntervalBucket = function(bindingKey) {
  const key = String(bindingKey || globalThis.__currentBindingKey || 'default');
  if (!globalThis.__intervals[key]) globalThis.__intervals[key] = [];
  return globalThis.__intervals[key];
};
globalThis.shared = globalThis.shared || {};
globalThis.localStorage = globalThis.localStorage || {
  __data: Object.create(null),
  get(key) {
    key = String(key);
    return Object.prototype.hasOwnProperty.call(this.__data, key) ? this.__data[key] : null;
  },
  set(key, value) { this.__data[String(key)] = String(value); },
  remove(key) { delete this.__data[String(key)]; },
  clear() { this.__data = Object.create(null); }
};
globalThis.MediaPlaybackEvent = globalThis.MediaPlaybackEvent || {
  PLAYBACK_STOPPED: 0,
  PLAYBACK_PLAYING: 1,
  PLAYBACK_PAUSED: 2
};
globalThis.input = globalThis.input || {
  cursorPosition: new Vec2(0, 0),
  cursorWorldPosition: new Vec3(0, 0, 0)
};
globalThis.console = globalThis.console || {
  log() {},
  warn() {},
  error() {},
  info() {}
};
globalThis.__weRunIntervals = function(bindingKey) {
  for (const interval of globalThis.__weIntervalBucket(bindingKey)) {
    if (!interval.active || typeof interval.callback !== 'function') continue;
    if (engine.runtime < interval.next) continue;
    interval.next = engine.runtime + interval.delay;
    interval.callback();
  }
};
globalThis.__missingLayer = { origin: new Vec3(0, 0, 0), scale: new Vec3(1, 1, 1), angles: new Vec3(0, 0, 0), visible: false, alpha: 0, color: new Vec4(0, 0, 0, 0), parallaxDepth: new Vec2(0, 0) };
globalThis.engine = {
  runtime: 0,
  frametime: 0,
  AUDIO_RESOLUTION_16: 16,
  AUDIO_RESOLUTION_32: 32,
  AUDIO_RESOLUTION_64: 64,
  registerAudioBuffers(resolution) {
    if (resolution === 64) return globalThis.__audio64;
    if (resolution === 32) return globalThis.__audio32;
    return globalThis.__audio16;
  },
  setInterval(callback, delayMs) {
    const interval = {
      callback,
      delay: Math.max(0.001, Number(delayMs || 0) / 1000),
      next: this.runtime + Math.max(0.001, Number(delayMs || 0) / 1000),
      active: true
    };
    globalThis.__weIntervalBucket().push(interval);
    return function() { interval.active = false; };
  },
  openUserShortcut() {
    return undefined;
  }
};
globalThis.thisScene = {
  getLayer(name) {
    const key = String(name);
    return globalThis.__layers && globalThis.__layers[key] ? globalThis.__layers[key] : globalThis.__missingLayer;
  },
  enumerateLayers() {
    return globalThis.__layerList || [];
  }
};
globalThis.createScriptProperties = function() {
  const props = globalThis.__scriptProps || {};
  const builder = {};
  for (const name of ['addSlider', 'addCheckbox', 'addCombo', 'addColor', 'addText']) {
    builder[name] = function(opts) {
      if (!(opts.name in props)) props[opts.name] = opts.value;
      return builder;
    };
  }
  builder.finish = function() { return props; };
  return builder;
};
)JS";

    JSValue result
	= JS_Eval (this->m_context, builtins, strlen (builtins), "<scene-script-builtins>", JS_EVAL_TYPE_GLOBAL);
    if (JS_IsException (result)) {
	logJSException (this->m_context, "installBuiltins");
    }
    JS_FreeValue (this->m_context, result);
    this->m_builtinsInstalled = true;
}

JSValue ScriptEngine::ensureModule (const void* bindingKey, const std::string& scriptSource) {
    const auto existing = this->m_modules.find (bindingKey);
    if (existing != this->m_modules.end ()) {
	return existing->second;
    }

    std::string body = scriptSource;
    size_t pos;
    const auto removeAll = [&body] (const std::string& pattern) {
	size_t position;
	while ((position = body.find (pattern)) != std::string::npos) {
	    body.erase (position, pattern.length ());
	}
    };
    removeAll ("'use strict';");
    removeAll ("\"use strict\";");
    removeAll ("import * as WEColor from 'WEColor';");
    removeAll ("import * as WEColor from \"WEColor\";");
    removeAll ("import * as WEMath from 'WEMath';");
    removeAll ("import * as WEMath from \"WEMath\";");
    while ((pos = body.find ("export ")) != std::string::npos) {
	body.erase (pos, 7);
    }

    std::ostringstream wrapper;
    wrapper
	<< "(function() {\n"
	<< body << "\n"
	<< "return {\n"
	<< "  update: typeof update === 'function' ? update : null,\n"
	<< "  init: typeof init === 'function' ? init : null,\n"
	<< "  applyUserProperties: typeof applyUserProperties === 'function' ? applyUserProperties : null,\n"
	<< "  mediaPropertiesChanged: typeof mediaPropertiesChanged === 'function' ? mediaPropertiesChanged : null,\n"
	<< "  mediaPlaybackChanged: typeof mediaPlaybackChanged === 'function' ? mediaPlaybackChanged : null,\n"
	<< "  mediaTimelineChanged: typeof mediaTimelineChanged === 'function' ? mediaTimelineChanged : null,\n"
	<< "  mediaThumbnailChanged: typeof mediaThumbnailChanged === 'function' ? mediaThumbnailChanged : null,\n"
	<< "  setScriptProperties: function(props) { if (typeof scriptProperties !== 'undefined' && scriptProperties) "
	   "Object.assign(scriptProperties, props); }\n"
	<< "};\n"
	<< "})();\n";

    const std::string source = wrapper.str ();
    JSValue module
	= JS_Eval (this->m_context, source.c_str (), source.size (), "<scene-script-module>", JS_EVAL_TYPE_GLOBAL);
    if (JS_IsException (module)) {
	logJSException (this->m_context, "module");
	JS_FreeValue (this->m_context, module);
	module = JS_NewObject (this->m_context);
    }

    this->m_modules.emplace (bindingKey, module);
    return module;
}

void ScriptEngine::releaseBinding (const void* bindingKey) {
    if (this->m_context) {
	JSValue global = JS_GetGlobalObject (this->m_context);
	JSValue intervals = JS_GetPropertyStr (this->m_context, global, "__intervals");
	if (JS_IsObject (intervals)) {
	    const std::string key = std::to_string (reinterpret_cast<uintptr_t> (bindingKey));
	    JSAtom keyAtom = JS_NewAtom (this->m_context, key.c_str ());
	    JS_DeleteProperty (this->m_context, intervals, keyAtom, 0);
	    JS_FreeAtom (this->m_context, keyAtom);
	}
	JS_FreeValue (this->m_context, intervals);
	JS_FreeValue (this->m_context, global);

	if (const auto module = this->m_modules.find (bindingKey); module != this->m_modules.end ()) {
	    JS_FreeValue (this->m_context, module->second);
	    this->m_modules.erase (module);
	}
    } else if (const auto module = this->m_modules.find (bindingKey); module != this->m_modules.end ()) {
	if (this->m_runtime) {
	    JS_FreeValueRT (this->m_runtime, module->second);
	}
	this->m_modules.erase (module);
    }

    this->m_initializedModules.erase (bindingKey);
    this->m_lastMediaProperties.erase (bindingKey);
    this->m_lastMediaPlayback.erase (bindingKey);
    this->m_lastMediaTimeline.erase (bindingKey);
    this->m_lastMediaThumbnail.erase (bindingKey);
}

static JSValue buildScriptPropertiesObject (JSContext* ctx, const DynamicValue& value) {
    JSValue propsObj = JS_NewObject (ctx);

    for (const auto& [name, dynVal] : value.getProperties ()) {
	JS_SetPropertyStr (ctx, propsObj, name.c_str (), dynamicValueToJS (ctx, dynVal));
    }

    return propsObj;
}

static JSValue jsNoop (JSContext*, JSValueConst, int, JSValueConst*) { return JS_UNDEFINED; }
static JSValue jsZero (JSContext* ctx, JSValueConst, int, JSValueConst*) { return JS_NewInt32 (ctx, 0); }

static JSValue jsGetTextureAnimation (JSContext* ctx, JSValueConst, int, JSValueConst*) {
    JSValue animation = JS_NewObject (ctx);
    JS_SetPropertyStr (ctx, animation, "pause", JS_NewCFunction (ctx, jsNoop, "pause", 0));
    JS_SetPropertyStr (ctx, animation, "play", JS_NewCFunction (ctx, jsNoop, "play", 0));
    JS_SetPropertyStr (ctx, animation, "stop", JS_NewCFunction (ctx, jsNoop, "stop", 0));
    JS_SetPropertyStr (ctx, animation, "setFrame", JS_NewCFunction (ctx, jsNoop, "setFrame", 1));
    JS_SetPropertyStr (ctx, animation, "getFrame", JS_NewCFunction (ctx, jsZero, "getFrame", 0));
    return animation;
}

static void syncLayerObjectProperties (JSContext* ctx, JSValue layer, ScriptableObject* object) {
    JS_SetPropertyStr (ctx, layer, "id", JS_NewInt32 (ctx, object->getId ()));
    JS_SetPropertyStr (ctx, layer, "name", JS_NewString (ctx, object->getObject ().name.c_str ()));
    JS_SetPropertyStr (
	ctx, layer, "getTextureAnimation", JS_NewCFunction (ctx, jsGetTextureAnimation, "getTextureAnimation", 0)
    );
    JS_SetPropertyStr (ctx, layer, "getAnimation", JS_NewCFunction (ctx, jsGetTextureAnimation, "getAnimation", 0));

    if (object->is<Objects::CSound> ()) {
	JS_SetPropertyStr (ctx, layer, "volume", JS_NewFloat64 (ctx, 1.0));
	JS_SetPropertyStr (ctx, layer, "play", JS_NewCFunction (ctx, jsNoop, "play", 0));
	JS_SetPropertyStr (ctx, layer, "stop", JS_NewCFunction (ctx, jsNoop, "stop", 0));
    }
}

static JSValue buildLayerObject (JSContext* ctx, ScriptableObject* object) {
    JSValue layer = JS_NewObjectClass (ctx, ScriptableObjectClassId);
    JS_SetOpaque (layer, object);

    // adds the base properties that the exotic object does not handle
    syncLayerObjectProperties (ctx, layer, object);

    return layer;
}

static void installSceneLayers (
    JSContext* ctx, JSValue global, WallpaperEngine::Render::Wallpapers::CScene* scene,
    const ScriptContext* bindingContext
) {
    JSValue layers = JS_GetPropertyStr (ctx, global, "__layers");

    if (JS_IsException (layers) || !JS_IsObject (layers)) {
	JS_FreeValue (ctx, layers);
	layers = JS_NewObject (ctx);
    }

    JSValue layerList = JS_NewArray (ctx);
    JSValue ownerLayer = JS_UNDEFINED;

    if (scene) {
	uint32_t layerIndex = 0;
	for (const auto& object : scene->getObjectsByRenderOrder ()) {
	    if (!object->is<ScriptableObject> ()) {
		continue;
	    }

	    auto scriptableObject = object->as<ScriptableObject> ();
	    const std::string id = std::to_string (object->getId ());

	    JSValue layer = JS_GetPropertyStr (ctx, layers, id.c_str ());

	    if ((JS_IsUndefined (layer) || JS_IsException (layer)) && !object->getObject ().name.empty ()) {
		JS_FreeValue (ctx, layer);
		layer = JS_GetPropertyStr (ctx, layers, object->getObject ().name.c_str ());
	    }

	    if (JS_IsUndefined (layer) || JS_IsException (layer)) {
		JS_FreeValue (ctx, layer);
		layer = buildLayerObject (ctx, scriptableObject);
	    }

	    JS_SetPropertyStr (ctx, layers, id.c_str (), JS_DupValue (ctx, layer));
	    JS_SetPropertyUint32 (ctx, layerList, layerIndex++, JS_DupValue (ctx, layer));

	    if (!object->getObject ().name.empty ()) {
		JS_SetPropertyStr (ctx, layers, object->getObject ().name.c_str (), JS_DupValue (ctx, layer));
	    }

	    if (bindingContext
		&& (object->getId () == bindingContext->object.id
		    || object->getObject ().name == bindingContext->object.name)) {
		ownerLayer = JS_DupValue (ctx, layer);
	    }

	    JS_FreeValue (ctx, layer);
	}
    }

    if (JS_IsUndefined (ownerLayer)) {
	ownerLayer = JS_NewObject (ctx);
    }

    JS_SetPropertyStr (ctx, global, "__layers", JS_DupValue (ctx, layers));
    JS_FreeValue (ctx, layers);
    JS_SetPropertyStr (ctx, global, "__layerList", layerList);
    JS_SetPropertyStr (ctx, global, "thisObject", JS_DupValue (ctx, ownerLayer));
    JS_SetPropertyStr (ctx, global, "thisLayer", ownerLayer);
}

static void applyLayerUpdates (
    JSContext* ctx, ScriptEngine& engine, JSValue global, WallpaperEngine::Render::Wallpapers::CScene* scene
) {
    if (!scene) {
	return;
    }

    JSValue layers = JS_GetPropertyStr (ctx, global, "__layers");

    if (JS_IsUndefined (layers) || JS_IsException (layers)) {
	JS_FreeValue (ctx, layers);
	return;
    }

    for (const auto& object : scene->getObjectsByRenderOrder ()) {
	if (object->is<ScriptableObject> () == false) {
	    continue;
	}

	JSValue layer = JS_GetPropertyStr (ctx, layers, std::to_string (object->getId ()).c_str ());

	if (JS_IsUndefined (layer)) {
	    JS_FreeValue (ctx, layer);
	    if (!object->getObject ().name.empty ()) {
		layer = JS_GetPropertyStr (ctx, layers, object->getObject ().name.c_str ());
	    }
	}

	if (JS_IsUndefined (layer) || JS_IsException (layer)) {
	    JS_FreeValue (ctx, layer);
	    continue;
	}

	JS_FreeValue (ctx, layer);
    }

    JS_FreeValue (ctx, layers);
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

void ScriptEngine::updateRuntimeGlobals (JSContext* ctx, JSValue globalObj) const {
    const auto runtimeSeconds = std::chrono::duration<double> (std::chrono::steady_clock::now () - sStartTime).count ();
    JSValue engine = JS_GetPropertyStr (ctx, globalObj, "engine");
    JS_SetPropertyStr (ctx, engine, "runtime", JS_NewFloat64 (ctx, runtimeSeconds));
    JS_SetPropertyStr (ctx, engine, "frametime", JS_NewFloat64 (ctx, g_Time - g_TimeLast));
    std::time_t now = std::time (nullptr);
    std::tm localTime {};
    localtime_r (&now, &localTime);
    const auto secondsOfDay = localTime.tm_hour * 3600 + localTime.tm_min * 60 + localTime.tm_sec;
    JS_SetPropertyStr (ctx, engine, "timeOfDay", JS_NewFloat64 (ctx, static_cast<double> (secondsOfDay) / 86400.0));
    JS_FreeValue (ctx, engine);
}

void ScriptEngine::updateSceneInputGlobals (
    JSContext* ctx, JSValue globalObj, WallpaperEngine::Render::Wallpapers::CScene* scene
) {
    if (scene == nullptr) {
	return;
    }

    auto& recorder = scene->getAudioContext ().getRecorder ();
    recorder.update ();
    updateAudioArray (ctx, globalObj, "__audio16", recorder.audio16, 16);
    updateAudioArray (ctx, globalObj, "__audio32", recorder.audio32, 32);
    updateAudioArray (ctx, globalObj, "__audio64", recorder.audio64, 64);

    JSValue input = JS_GetPropertyStr (ctx, globalObj, "input");
    if (JS_IsUndefined (input) || JS_IsException (input)) {
	JS_FreeValue (ctx, input);
	input = JS_NewObject (ctx);
    }

    const glm::vec2 mouse = *scene->getMousePosition ();
    DynamicValue cursorPosition;
    cursorPosition.update (glm::vec2 (mouse.x, mouse.y), DynamicValue::UpdateSource::Script);
    DynamicValue cursorWorldPosition;
    cursorWorldPosition.update (
	glm::vec3 (mouse.x * scene->getWidth (), mouse.y * scene->getHeight (), 0.0f),
	DynamicValue::UpdateSource::Script
    );
    JS_SetPropertyStr (ctx, input, "cursorPosition", dynamicValueToJS (ctx, cursorPosition));
    JS_SetPropertyStr (ctx, input, "cursorWorldPosition", dynamicValueToJS (ctx, cursorWorldPosition));
    JS_SetPropertyStr (ctx, globalObj, "input", input);
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

void ScriptEngine::initializeModuleIfNeeded (
    JSContext* ctx, JSValue module, const void* bindingKey, const DynamicValue& currentValue, bool hasScene
) {
    if (!hasScene || this->m_initializedModules.contains (bindingKey)) {
	return;
    }

    JSValue init = JS_GetPropertyStr (ctx, module, "init");
    bool initOk = true;
    if (JS_IsFunction (ctx, init)) {
	JSValue current = dynamicValueToJS (ctx, currentValue);
	JSValue args[] = { current };
	JSValue initResult = JS_Call (ctx, init, JS_UNDEFINED, 1, args);
	if (JS_IsException (initResult)) {
	    logJSException (ctx, "init");
	    initOk = false;
	}
	JS_FreeValue (ctx, initResult);
	JS_FreeValue (ctx, current);
    }
    JS_FreeValue (ctx, init);
    if (initOk) {
	this->m_initializedModules.insert (bindingKey);
    }
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

void ScriptEngine::logTextEvaluationDebug (const ScriptContext* bindingContext) const {
    // TODO: REWRITE THIS TO PROPERLY HANDLE TEXT INSTEAD OF HAVING A SPECIFIC EXCEPTION
    /*
    if (
	std::getenv ("LWE_MEDIA_DEBUG") == nullptr || bindingContext == nullptr
	|| bindingContext->property != "text"
    ) {
	return;
    }*/

    sLog.out ("Media debug: evaluating text object=", bindingContext->object.id, " name=", bindingContext->object.name);
}

void ScriptEngine::logTextResultDebug (
    const ScriptContext* bindingContext, const DynamicValueUniquePtr& dynResult
) const {
    // TODO: REWRITE THIS TO PROPERLY HANDLE TEXT INSTEAD OF HAVING A SPECIFIC EXCEPTION
    /*
    if (
	std::getenv ("LWE_MEDIA_DEBUG") == nullptr || bindingContext == nullptr || !dynResult
	|| bindingContext->property != "text"
    ) {
	return;
    }
*/
    sLog.out (
	"Media debug: text result object=", bindingContext->object.id, " name=", bindingContext->object.name,
	" type=", static_cast<int> (dynResult->getType ()),
	" value=", dynResult->getType () == DynamicValue::String ? dynResult->getString () : "<non-string>"
    );
}

JSValue ScriptEngine::callUpdate (JSContext* ctx, JSValue module, const DynamicValue& currentValue) const {
    JSValue update = JS_GetPropertyStr (ctx, module, "update");
    JSValue current = dynamicValueToJS (ctx, currentValue);
    JSValue result = JS_UNDEFINED;
    if (JS_IsFunction (ctx, update)) {
	JSValue args[] = { current };
	result = JS_Call (ctx, update, JS_UNDEFINED, 1, args);
    }
    JS_FreeValue (ctx, update);
    JS_FreeValue (ctx, current);
    return result;
}

void ScriptEngine::resolveEvaluationResult (JSContext* ctx, JSValue result, DynamicValue& currentValue) const {
    ScopeGuard guard ([&] () { JS_FreeValue (ctx, result); });

    if (JS_IsException (result)) {
	return;
    }

    if (JS_IsUndefined (result)) {
	currentValue.update (DynamicValue::Script);
	return;
    }

    jsToDynamicValue (ctx, result, currentValue);
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
    const std::string& scriptSource, const std::map<std::string, DynamicValue>& initialScriptProps,
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

    for (const auto& [name, dynVal] : initialScriptProps) {
	JS_SetPropertyStr (ctx, seedProps, name.c_str (), dynamicValueToJS (ctx, dynVal));
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

void ScriptEngine::evaluate (
    const void* bindingKey, const std::string& scriptSource, DynamicValue& currentValue, Wallpapers::CScene* scene,
    const ScriptContext* bindingContext
) {
    if (!this->m_context) {
	sLog.exception ("ScriptEngine: No JS context available");
    }

    this->installBuiltins ();

    JSContext* ctx = this->m_context;
    JSValue globalObj = JS_GetGlobalObject (ctx);
    const std::string bindingKeyString = std::to_string (reinterpret_cast<uintptr_t> (bindingKey));
    JS_SetPropertyStr (ctx, globalObj, "__currentBindingKey", JS_NewString (ctx, bindingKeyString.c_str ()));

    this->updateRuntimeGlobals (ctx, globalObj);
    this->updateSceneInputGlobals (ctx, globalObj, scene);

    JSValue propsObj = buildScriptPropertiesObject (ctx, currentValue);
    JS_SetPropertyStr (ctx, globalObj, "__scriptProps", JS_DupValue (ctx, propsObj));
    installSceneLayers (ctx, globalObj, scene, bindingContext);

    JSValue module = this->ensureModule (bindingKey, scriptSource);
    this->logTextEvaluationDebug (bindingContext);

    this->callModuleWithProps (ctx, module, "setScriptProperties", propsObj);
    this->initializeModuleIfNeeded (ctx, module, bindingKey, currentValue, scene != nullptr);
    this->callModuleWithProps (ctx, module, "applyUserProperties", propsObj);

    this->dispatchMediaEvents (module, bindingKey);
    this->runIntervals (ctx, globalObj, bindingKeyString);

    JSValue result = this->callUpdate (ctx, module, currentValue);

    if (!JS_IsException (result)) {
	applyLayerUpdates (ctx, *this, globalObj, scene);
    }

    JS_SetPropertyStr (ctx, globalObj, "__scriptProps", JS_UNDEFINED);
    JS_FreeValue (ctx, globalObj);
    JS_FreeValue (ctx, propsObj);

    this->resolveEvaluationResult (ctx, result, currentValue);
}
