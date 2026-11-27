#include "InputObject.h"

#include "EngineObject.h"
#include "ScriptEngine.h"
#include "WallpaperEngine/Render/Wallpapers/CScene.h"

using namespace WallpaperEngine::Scripting;

JSValue get_cursor_world_position (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    JSClassID classId;
    auto* input = static_cast<InputObject*> (JS_GetAnyOpaque (this_val, &classId));
    const auto& scene = input->getScene ();
    const glm::vec3 position = scene.getCamera ().screenToWorld (*scene.getMousePositionNormalized ());

    JSValue result = scene.getScriptEngine ().getAdapters ().vec3->instantiate ();
    JS_SetPropertyStr (ctx, result, "x", JS_NewFloat64 (ctx, position.x));
    JS_SetPropertyStr (ctx, result, "y", JS_NewFloat64 (ctx, position.y));
    JS_SetPropertyStr (ctx, result, "z", JS_NewFloat64 (ctx, 0.0f));
    return result;
}

JSValue get_cursor_screen_position (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    JSClassID classId;
    auto* input = static_cast<InputObject*> (JS_GetAnyOpaque (this_val, &classId));
    const auto& scene = input->getScene ();
    const glm::dvec2 position = scene.getContext ().getInputContext ().getMouseInput ().screenPosition ();

    JSValue result = scene.getScriptEngine ().getAdapters ().vec2->instantiate ();

    JS_SetPropertyStr (ctx, result, "x", JS_NewFloat64 (ctx, position.x));
    JS_SetPropertyStr (ctx, result, "y", JS_NewFloat64 (ctx, position.y));

    return result;
}

JSValue get_cursor_left_down (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    JSClassID classId;
    auto* input = static_cast<InputObject*> (JS_GetAnyOpaque (this_val, &classId));
    if (input == nullptr) {
	return JS_ThrowTypeError (ctx, "invalid receiver for cursorLeftDown");
    }

    const auto status = input->getScene ().getContext ().getInputContext ().getMouseInput ().leftClick ();
    return JS_NewBool (ctx, status == WallpaperEngine::Input::MouseClickStatus::Clicked);
}

JSValue input_set_value (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    return JS_ThrowTypeError (ctx, "Cannot assign to read-only property");
}

InputObject::InputObject (ScriptEngine& engine, Render::Wallpapers::CScene& scene) :
    m_scene (scene), m_engine (engine), m_classId (0) {
    this->m_definition = { .class_name = "IInput" };
    JS_NewClassID (this->m_engine.getRuntime (), &this->m_classId);
    JS_NewClass (this->m_engine.getRuntime (), this->m_classId, &this->m_definition);
    this->m_instance = JS_NewObjectClass (this->m_engine.getContext (), this->m_classId);

    JS_DupValue (this->m_engine.getContext (), this->m_instance);

    // set properties
    JS_SetOpaque (this->m_instance, this);
    JS_DefinePropertyGetSet (
	this->m_engine.getContext (), this->m_instance,
	JS_NewAtom (this->m_engine.getContext (), "cursorWorldPosition"),
	JS_NewCFunction (this->m_engine.getContext (), get_cursor_world_position, "get", 0),
	JS_NewCFunction (this->m_engine.getContext (), input_set_value, "set", 1), JS_PROP_ENUMERABLE
    );
    JS_DefinePropertyGetSet (
	this->m_engine.getContext (), this->m_instance,
	JS_NewAtom (this->m_engine.getContext (), "cursorScreenPosition"),
	JS_NewCFunction (this->m_engine.getContext (), get_cursor_screen_position, "get", 0),
	JS_NewCFunction (this->m_engine.getContext (), input_set_value, "set", 1), JS_PROP_ENUMERABLE
    );
    JS_DefinePropertyGetSet (
	this->m_engine.getContext (), this->m_instance, JS_NewAtom (this->m_engine.getContext (), "cursorLeftDown"),
	JS_NewCFunction (this->m_engine.getContext (), get_cursor_left_down, "get", 0),
	JS_NewCFunction (this->m_engine.getContext (), input_set_value, "set", 1), JS_PROP_ENUMERABLE
    );
}
InputObject::~InputObject () { JS_FreeValue (this->m_engine.getContext (), this->m_instance); }
