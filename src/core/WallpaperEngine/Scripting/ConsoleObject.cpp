#include "ConsoleObject.h"

#include "EngineObject.h"
#include "ScriptEngine.h"
#include "WallpaperEngine/Utils/ScopeGuard.h"
#include "WallpaperEngine/Render/Wallpapers/CScene.h"

using namespace WallpaperEngine::Scripting;

JSValue console_log (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    if (argc < 1) {
	return JS_UNDEFINED;
    }

    std::stringstream stream;

    for (int i = 0; i < argc; i++) {
	const char* str = JS_ToCString (ctx, argv[i]);
	ScopeGuard guard ([ctx, str] { JS_FreeCString (ctx, str); });

	stream << str;
    }

    sLog.out (stream.str ());

    return JS_UNDEFINED;
}

JSValue console_error (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    if (argc < 1) {
	return JS_UNDEFINED;
    }

    std::stringstream stream;

    for (int i = 0; i < argc; i++) {
	const char* str = JS_ToCString (ctx, argv[i]);
	ScopeGuard guard ([ctx, str] { JS_FreeCString (ctx, str); });

	stream << str;
    }

    sLog.error (stream.str ());

    return JS_UNDEFINED;
}

ConsoleObject::ConsoleObject (ScriptEngine& engine, Render::Wallpapers::CScene& scene) :
    m_scene (scene), m_engine (engine), m_classId (0) {
    this->m_definition = { .class_name = "IConsole" };
    JS_NewClassID (this->m_engine.getRuntime (), &this->m_classId);
    JS_NewClass (this->m_engine.getRuntime (), this->m_classId, &this->m_definition);
    this->m_instance = JS_NewObjectClass (this->m_engine.getContext (), this->m_classId);

    JS_DupValue (this->m_engine.getContext (), this->m_instance);

    // set properties
    JS_SetOpaque (this->m_instance, this);
    JS_DefinePropertyValueStr (
	this->m_engine.getContext (), this->m_instance, "log",
	JS_NewCFunction (this->m_engine.getContext (), console_log, "log", 1), JS_PROP_ENUMERABLE
    );
    JS_DefinePropertyValueStr (
	this->m_engine.getContext (), this->m_instance, "error",
	JS_NewCFunction (this->m_engine.getContext (), console_error, "error", 1), JS_PROP_ENUMERABLE
    );
}

ConsoleObject::~ConsoleObject () { JS_FreeValue (this->m_engine.getContext (), this->m_instance); }