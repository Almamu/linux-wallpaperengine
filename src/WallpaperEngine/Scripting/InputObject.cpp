#include "InputObject.h"

#include "EngineObject.h"
#include "ScriptEngine.h"
#include "WallpaperEngine/Render/Wallpapers/CScene.h"

using namespace WallpaperEngine::Scripting;

JSValue get_cursor_world_position (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    JSClassID classId;
    auto* input = static_cast<InputObject*> (JS_GetAnyOpaque (this_val, &classId));
    const auto& scene = input->getScene ();
    const auto& camera = scene.getCamera ();
    JSValue result = scene.getScriptEngine ().getAdapters ().vec3->instantiate ();

    if (!camera.isOrthogonal ()) {
	return result;
    }

    // CScene stores the cursor in visible wallpaper UVs, including any crop.
    // Convert the output's texture orientation to the scene's top-left coordinates.
    const auto* position = scene.getMousePosition ();
    const double x = position->x * camera.getWidth ();
    const bool flipY = scene.getContext ().getOutput ().renderVFlip ();
    const double y = (flipY ? 1.0 - position->y : position->y) * camera.getHeight ();
    JS_SetPropertyStr (ctx, result, "x", JS_NewFloat64 (ctx, x));
    JS_SetPropertyStr (ctx, result, "y", JS_NewFloat64 (ctx, y));
    JS_SetPropertyStr (ctx, result, "z", JS_NewFloat64 (ctx, 0.0));

    return result;
}

JSValue get_cursor_screen_position (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    JSClassID classId;
    auto* input = static_cast<InputObject*> (JS_GetAnyOpaque (this_val, &classId));
    const auto& scene = input->getScene ();
    const auto position = scene.getContext ().getInputContext ().getMouseInput ().position ();
    const auto& output = scene.getContext ().getOutput ();
    const double x = position.x;
    const double y = output.getFullHeight () - position.y;

    JSValue result = input->getScene ().getScriptEngine ().getAdapters ().vec2->instantiate ();

    JS_SetPropertyStr (ctx, result, "x", JS_NewFloat64 (ctx, x));
    JS_SetPropertyStr (ctx, result, "y", JS_NewFloat64 (ctx, y));

    return result;
}

JSValue get_cursor_left_down (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    // TODO: IMPLEMENT THIS
    return JS_NewBool (ctx, false);
}

JSValue input_set_value (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) { return JS_EXCEPTION; }

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
