#include "SceneObject.h"

#include "Adapters/ScriptableObjectAdapter.h"
#include "ScriptEngine.h"
#include "ScriptableObject.h"
#include "WallpaperEngine/Data/Model/DynamicValue.h"
#include "WallpaperEngine/Data/Model/UserSetting.h"
#include "WallpaperEngine/Data/Utils/ScopeGuard.h"
#include "WallpaperEngine/Render/Camera.h"
#include "WallpaperEngine/Render/Wallpapers/CScene.h"

#include <cmath>
#include <cstdlib>

using namespace WallpaperEngine::Scripting;

SceneObject* get_opaque (JSValueConst this_val) {
    JSClassID classId;
    return static_cast<SceneObject*> (JS_GetAnyOpaque (this_val, &classId));
}

enum SceneAnimationCommand {
    SceneAnimation_Play, SceneAnimation_Pause, SceneAnimation_Stop, SceneAnimation_IsPlaying,
    SceneAnimation_GetFrame, SceneAnimation_SetFrame, SceneAnimation_GetRate, SceneAnimation_SetRate,
    SceneAnimation_GetFps, SceneAnimation_GetFrameCount, SceneAnimation_GetDuration
};

static JSValue scene_animation_command (
    JSContext* ctx, JSValueConst, int argc, JSValueConst* argv, int magic, JSValueConst* data
) {
    auto* owner = get_opaque (data[0]);
    const char* name = JS_ToCString (ctx, data[1]);
    if (name == nullptr) return JS_EXCEPTION;
    auto* animation = owner->findPropertyAnimation (name);
    JS_FreeCString (ctx, name);
    if (animation == nullptr) return JS_UNDEFINED;
    const double time = owner->getScene ().getElapsedTime ();
    double value = 0.0;
    switch (magic) {
        case SceneAnimation_Play: animation->play (time); break;
        case SceneAnimation_Pause: animation->pause (time); break;
        case SceneAnimation_Stop: animation->stop (time); break;
        case SceneAnimation_IsPlaying: return JS_NewBool (ctx, animation->isPlaying (time));
        case SceneAnimation_GetFrame: return JS_NewFloat64 (ctx, animation->frameAt (time));
        case SceneAnimation_GetRate: return JS_NewFloat64 (ctx, animation->rate);
        case SceneAnimation_GetFps: return JS_NewFloat64 (ctx, animation->fps);
        case SceneAnimation_GetFrameCount: return JS_NewFloat64 (ctx, animation->length);
        case SceneAnimation_GetDuration:
            return JS_NewFloat64 (ctx, animation->fps > 0.0f ? animation->length / animation->fps : 0.0f);
        case SceneAnimation_SetFrame:
        case SceneAnimation_SetRate:
            if (argc < 1 || JS_ToFloat64 (ctx, &value, argv[0]) < 0 || !std::isfinite (value)) {
                return JS_ThrowTypeError (ctx, "Animation frame/rate expects a finite number");
            }
            if (magic == SceneAnimation_SetFrame) animation->setFrame (static_cast<float> (value), time);
            else animation->setRate (static_cast<float> (value), time);
            break;
    }
    return JS_UNDEFINED;
}

static JSValue scene_property_get_animation (
    JSContext* ctx, JSValueConst, int argc, JSValueConst* argv, int, JSValueConst* data
) {
    JSValueConst nameValue = argc > 0 && !JS_IsUndefined (argv[0]) ? argv[0] : data[1];
    const char* name = JS_ToCString (ctx, nameValue);
    if (name == nullptr) return JS_EXCEPTION;
    const bool empty = name[0] == '\0';
    JS_FreeCString (ctx, name);
    if (empty) nameValue = data[1];
    name = JS_ToCString (ctx, nameValue);
    if (name == nullptr) return JS_EXCEPTION;
    const bool found = get_opaque (data[0])->findPropertyAnimation (name) != nullptr;
    JS_FreeCString (ctx, name);
    if (!found) return JS_UNDEFINED;
    JSValue animationData[] = { data[0], nameValue };
    JSValue result = JS_NewObject (ctx);
    const auto method = [&] (const char* key, int command, int length = 0) {
        JS_SetPropertyStr (ctx, result, key,
            JS_NewCFunctionData (ctx, scene_animation_command, length, command, 2, animationData));
    };
    method ("play", SceneAnimation_Play);
    method ("pause", SceneAnimation_Pause);
    method ("stop", SceneAnimation_Stop);
    method ("isPlaying", SceneAnimation_IsPlaying);
    method ("getFrame", SceneAnimation_GetFrame);
    method ("setFrame", SceneAnimation_SetFrame, 1);
    for (const auto& [field, command] : std::initializer_list<std::pair<const char*, int>> {
        { "rate", SceneAnimation_GetRate }, { "fps", SceneAnimation_GetFps },
        { "frameCount", SceneAnimation_GetFrameCount }, { "duration", SceneAnimation_GetDuration } }) {
        const JSAtom atom = JS_NewAtom (ctx, field);
        JS_DefinePropertyGetSet (ctx, result, atom,
            JS_NewCFunctionData (ctx, scene_animation_command, 0, command, 2, animationData),
            command == SceneAnimation_GetRate
                ? JS_NewCFunctionData (ctx, scene_animation_command, 1, SceneAnimation_SetRate, 2, animationData)
                : JS_UNDEFINED, JS_PROP_ENUMERABLE);
        JS_FreeAtom (ctx, atom);
    }
    return result;
}

void SceneObject::registerPropertyAnimation (const std::string& property, const UserSetting& setting) {
    if (setting.animation != nullptr) m_propertyAnimations.emplace_back (property, &setting);
}

PropertyAnimation* SceneObject::findPropertyAnimation (const std::string& name) const {
    for (const auto& [property, setting] : m_propertyAnimations) {
        if (property == name || setting->animation->name == name) return setting->animation.get ();
    }
    return nullptr;
}

JSValue SceneObject::makePropertyOwner (const std::string& property) {
    auto* ctx = m_engine.getContext ();
    JSValue data[] = { m_instance, JS_NewString (ctx, property.c_str ()) };
    JSValue result = JS_NewObject (ctx);
    JS_SetPropertyStr (ctx, result, "getAnimation",
        JS_NewCFunctionData (ctx, scene_property_get_animation, 1, 0, 2, data));
    JS_FreeValue (ctx, data[1]);
    return result;
}

JSValue get_bloom (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    auto* container = get_opaque (this_val);

    return JS_NewBool (ctx, container->getScene ().getScene ().camera.bloom.enabled->value->getBool ());
}

JSValue get_bloomstrength (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    auto* container = get_opaque (this_val);

    return JS_NewFloat64 (ctx, container->getScene ().getScene ().camera.bloom.strength->value->getFloat ());
}

JSValue get_bloomthreshold (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    auto* container = get_opaque (this_val);

    return JS_NewFloat64 (ctx, container->getScene ().getScene ().camera.bloom.threshold->value->getFloat ());
}

JSValue get_clearenabled (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    auto* container = get_opaque (this_val);

    return JS_NewBool (ctx, container->getScene ().getScene ().clearEnabled->value->getBool ());
}

JSValue get_clearcolor (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    auto* container = get_opaque (this_val);

    return container->getEngine ().getAdapters ().vec3->instantiate (
	*container->getScene ().getScene ().colors.clear->value
    );
}

JSValue get_ambientcolor (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    auto* container = get_opaque (this_val);

    return container->getEngine ().getAdapters ().vec3->instantiate (
	*container->getScene ().getScene ().colors.ambient->value
    );
}

JSValue get_skylightcolor (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    auto* container = get_opaque (this_val);

    return container->getEngine ().getAdapters ().vec3->instantiate (
	*container->getScene ().getScene ().colors.skylight->value
    );
}

JSValue get_gravitydirection (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    auto* container = get_opaque (this_val);

    return container->getEngine ().getAdapters ().vec3->instantiate (*container->getScene ().getScene ().physics.gravityDirection->value);
}

JSValue get_gravitystrength (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    auto* container = get_opaque (this_val);

    return JS_NewFloat64 (ctx, container->getScene ().getScene ().physics.gravityStrength->value->getFloat ());
}

JSValue get_windenabled (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    auto* container = get_opaque (this_val);

    return JS_NewBool (ctx, container->getScene ().getScene ().physics.windEnabled->value->getBool ());
}

JSValue get_winddirection (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    auto* container = get_opaque (this_val);

    return container->getEngine ().getAdapters ().vec3->instantiate (*container->getScene ().getScene ().physics.windDirection->value);
}

JSValue get_windstrength (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    auto* container = get_opaque (this_val);

    return JS_NewFloat64 (ctx, container->getScene ().getScene ().physics.windStrength->value->getFloat ());
}

JSValue get_fov (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    auto* container = get_opaque (this_val);

    return JS_NewFloat64 (ctx, container->getScene ().getScene ().camera.projection.fov->value->getFloat ());
}

JSValue get_nearz (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    auto* container = get_opaque (this_val);

    return JS_NewFloat64 (ctx, container->getScene ().getScene ().camera.projection.nearz->value->getFloat ());
}

JSValue get_farz (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    auto* container = get_opaque (this_val);

    return JS_NewFloat64 (ctx, container->getScene ().getScene ().camera.projection.farz->value->getFloat ());
}

JSValue get_camerafade (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    auto* container = get_opaque (this_val);

    return JS_NewBool (ctx, container->getScene ().getScene ().camera.fade->value->getBool ());
}

JSValue get_camerashake (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    auto* container = get_opaque (this_val);

    return JS_NewBool (ctx, container->getScene ().getScene ().camera.shake.enabled->value->getBool ());
}

JSValue get_camerashakespeed (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    auto* container = get_opaque (this_val);

    return JS_NewFloat64 (ctx, container->getScene ().getScene ().camera.shake.speed->value->getFloat ());
}

JSValue get_camerashakeamplitude (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    auto* container = get_opaque (this_val);

    return JS_NewFloat64 (ctx, container->getScene ().getScene ().camera.shake.amplitude->value->getFloat ());
}

JSValue get_camerashakeroughness (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    auto* container = get_opaque (this_val);

    return JS_NewFloat64 (ctx, container->getScene ().getScene ().camera.shake.roughness->value->getFloat ());
}

JSValue get_cameraparallax (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    auto* container = get_opaque (this_val);

    return JS_NewBool (ctx, container->getScene ().getScene ().camera.parallax.enabled->value->getBool ());
}

JSValue get_cameraparallaxamount (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    auto* container = get_opaque (this_val);

    return JS_NewFloat64 (ctx, container->getScene ().getScene ().camera.parallax.amount->value->getFloat ());
}

JSValue get_cameraparallaxdelay (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    auto* container = get_opaque (this_val);

    return JS_NewFloat64 (ctx, container->getScene ().getScene ().camera.parallax.delay->value->getFloat ());
}

JSValue get_cameraparallaxmouseinfluence (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    auto* container = get_opaque (this_val);

    return JS_NewFloat64 (ctx, container->getScene ().getScene ().camera.parallax.mouseInfluence->value->getFloat ());
}

JSValue get_layer (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    if (argc != 1) {
	return JS_ThrowTypeError (ctx, "getLayer() expects exactly one argument");
    }

    auto* container = get_opaque (this_val);

    JSValue layer = argv[0];

    if (JS_IsNumber (layer)) {
	int index = 0;

	JS_ToInt32 (ctx, &index, layer);

	if (index < 0) {
	    return JS_NULL;
	}

	for (auto* object : container->getScene ().getObjectsByCursorOrder ()) {
	    if (object == nullptr || !object->is<ScriptableObject> ()) {
		continue;
	    }
	    if (index-- == 0) {
		return container->getEngine ().getAdapters ().object->instantiate (*object->as<ScriptableObject> ());
	    }
	}

	return JS_NULL;
    } else if (!JS_IsNull (layer) && !JS_IsUndefined (layer)) {
	// Wallpaper Engine accepts string-coercible values here too (notably boxed
	// strings produced by some workshop script/transpiler combinations).
	const char* result = JS_ToCString (ctx, layer);

	if (result == nullptr) {
	    return JS_EXCEPTION;
	}

	ScopeGuard guard ([=] { JS_FreeCString (ctx, result); });

	for (auto object : container->getScene ().getObjectsByRenderOrder ()) {
	    if (object->getObject ().name != result) {
		continue;
	    }

	    if (!object->is<ScriptableObject> ()) {
		continue;
	    }

	    return container->getEngine ().getAdapters ().object->instantiate (*object->as<ScriptableObject> ());
	}
    }

    return JS_UNDEFINED;
}

// Instantiate a real engine Vec3 rather than a bare {x,y,z} bag: stock camera controllers chain
// vector math straight off getCameraTransforms() (`p2.subtract(p1).divide(len)`), so a plain
// object throws "not a function" on the first tick and kills the whole script.
static JSValue make_script_vec3 (JSContext* ctx, ScriptEngine& engine, const glm::vec3& value) {
    JSValue result = engine.getAdapters ().vec3->instantiate ();
    JS_SetPropertyStr (ctx, result, "x", JS_NewFloat64 (ctx, value.x));
    JS_SetPropertyStr (ctx, result, "y", JS_NewFloat64 (ctx, value.y));
    JS_SetPropertyStr (ctx, result, "z", JS_NewFloat64 (ctx, value.z));
    return result;
}

JSValue scene_get_camera_transforms (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    auto* container = get_opaque (this_val);
    const auto& transform = container->getScene ().getCamera ().getUnshakenTransform ();

    auto& engine = container->getEngine ();
    JSValue result = JS_NewObject (ctx);
    JS_SetPropertyStr (ctx, result, "eye", make_script_vec3 (ctx, engine, transform.eye));
    JS_SetPropertyStr (ctx, result, "center", make_script_vec3 (ctx, engine, transform.center));
    JS_SetPropertyStr (ctx, result, "up", make_script_vec3 (ctx, engine, transform.up));
    JS_SetPropertyStr (ctx, result, "zoom", JS_NewFloat64 (ctx, transform.zoom));
    return result;
}

static bool read_script_vec3 (JSContext* ctx, JSValueConst value, glm::vec3& result) {
    if (!JS_IsObject (value)) {
	return false;
    }

    JSValue fields[] = {
	JS_GetPropertyStr (ctx, value, "x"),
	JS_GetPropertyStr (ctx, value, "y"),
	JS_GetPropertyStr (ctx, value, "z"),
    };
    const bool valid = JS_IsNumber (fields[0]) && JS_IsNumber (fields[1]) && JS_IsNumber (fields[2]);
    double components[] = {0.0, 0.0, 0.0};
    if (valid) {
	JS_ToFloat64 (ctx, &components[0], fields[0]);
	JS_ToFloat64 (ctx, &components[1], fields[1]);
	JS_ToFloat64 (ctx, &components[2], fields[2]);
    }
    for (const auto& field : fields) {
	JS_FreeValue (ctx, field);
    }

    result = glm::vec3 (components[0], components[1], components[2]);
    return valid;
}

enum class SceneField {
    Bloom, BloomStrength, BloomThreshold, ClearEnabled, ClearColor, AmbientColor, SkylightColor, Fov, NearZ, FarZ, CameraFade, CameraShake, CameraShakeSpeed, CameraShakeAmplitude, CameraShakeRoughness, CameraParallax, CameraParallaxAmount, CameraParallaxDelay, CameraParallaxMouseInfluence
};

// Native IScene installs both property callbacks (scenescript64.dll
// 181631d00/181632030). These fields are writable in lib.sceneScript.d.ts.
// Update the existing setting so its renderer consumers and bindings see writes.
JSValue set_scene_field (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv, int magic) {
    if (argc != 1) {
        return JS_ThrowTypeError (ctx, "Scene property setter expects one value");
    }
    const auto& scene = get_opaque (this_val)->getScene ().getScene ();
    DynamicValue* target = nullptr;
    const auto field = static_cast<SceneField> (magic);
    switch (field) {
        case SceneField::Bloom: target = scene.camera.bloom.enabled->value.get (); break;
        case SceneField::BloomStrength: target = scene.camera.bloom.strength->value.get (); break;
        case SceneField::BloomThreshold: target = scene.camera.bloom.threshold->value.get (); break;
        case SceneField::ClearEnabled: target = scene.clearEnabled->value.get (); break;
        case SceneField::ClearColor: target = scene.colors.clear->value.get (); break;
        case SceneField::AmbientColor: target = scene.colors.ambient->value.get (); break;
        case SceneField::SkylightColor: target = scene.colors.skylight->value.get (); break;
        case SceneField::Fov: target = scene.camera.projection.fov->value.get (); break;
        case SceneField::NearZ: target = scene.camera.projection.nearz->value.get (); break;
        case SceneField::FarZ: target = scene.camera.projection.farz->value.get (); break;
        case SceneField::CameraFade: target = scene.camera.fade->value.get (); break;
        case SceneField::CameraShake: target = scene.camera.shake.enabled->value.get (); break;
        case SceneField::CameraShakeSpeed: target = scene.camera.shake.speed->value.get (); break;
        case SceneField::CameraShakeAmplitude: target = scene.camera.shake.amplitude->value.get (); break;
        case SceneField::CameraShakeRoughness: target = scene.camera.shake.roughness->value.get (); break;
        case SceneField::CameraParallax: target = scene.camera.parallax.enabled->value.get (); break;
        case SceneField::CameraParallaxAmount: target = scene.camera.parallax.amount->value.get (); break;
        case SceneField::CameraParallaxDelay: target = scene.camera.parallax.delay->value.get (); break;
        case SceneField::CameraParallaxMouseInfluence: target = scene.camera.parallax.mouseInfluence->value.get (); break;
    }
    if (target == nullptr) {
        return JS_UNDEFINED;
    }
    if (field == SceneField::ClearColor || field == SceneField::AmbientColor || field == SceneField::SkylightColor) {
        glm::vec3 value;
        if (!read_script_vec3 (ctx, argv[0], value)) {
            return JS_ThrowTypeError (ctx, "Scene color expects a Vec3");
        }
        target->update (value, DynamicValue::UpdateSource::Script);
    } else if (field == SceneField::Bloom || field == SceneField::ClearEnabled || field == SceneField::CameraFade
               || field == SceneField::CameraShake || field == SceneField::CameraParallax) {
        if (!JS_IsBool (argv[0])) {
            return JS_ThrowTypeError (ctx, "Scene toggle expects a boolean");
        }
        target->update (JS_ToBool (ctx, argv[0]) != 0, DynamicValue::UpdateSource::Script);
    } else {
        if (!JS_IsNumber (argv[0])) {
            return JS_ThrowTypeError (ctx, "Scene numeric property expects a number");
        }
        double value = 0.0;
        if (JS_ToFloat64 (ctx, &value, argv[0]) < 0) {
            return JS_EXCEPTION;
        }
        target->update (static_cast<float> (value), DynamicValue::UpdateSource::Script);
    }
    return JS_UNDEFINED;
}

// thisScene.setCameraTransforms({eye, center, up, zoom}) -> hands the camera to the script for
// this frame. This is how stock 3D scenes implement mouse-drag orbiting: a controller layer
// reads input.cursorScreenPosition/cursorLeftDown, integrates it, and pushes the result here.
// Fields are optional; anything omitted keeps the value getCameraTransforms would have returned.
JSValue scene_set_camera_transforms (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    if (argc < 1 || !JS_IsObject (argv[0])) {
	return JS_ThrowTypeError (ctx, "setCameraTransforms() expects a transform object");
    }

    auto* container = get_opaque (this_val);
    auto& scene = container->getScene ();
    CameraTransform transform = scene.getCamera ().getUnshakenTransform ();

    const auto readInto = [ctx, &argv] (const char* name, glm::vec3& target) {
	JSValue value = JS_GetPropertyStr (ctx, argv[0], name);
	glm::vec3 candidate;
	if (read_script_vec3 (ctx, value, candidate)) {
	    target = candidate;
	}
	JS_FreeValue (ctx, value);
    };
    readInto ("eye", transform.eye);
    readInto ("center", transform.center);
    readInto ("up", transform.up);

    JSValue zoom = JS_GetPropertyStr (ctx, argv[0], "zoom");
    if (JS_IsNumber (zoom)) {
	double value = 0.0;
	JS_ToFloat64 (ctx, &value, zoom);
	transform.zoom = static_cast<float> (value);
    }
    JS_FreeValue (ctx, zoom);

    // Drop a pose that is not finite instead of latching it. Controllers integrate from whatever
    // getCameraTransforms() hands back, so a single inf/NaN frame — these scripts divide by a vector
    // length that is zero until the scene settles — would otherwise feed itself forever. Ignoring
    // the call leaves the last good pose in place, which is what the script reads next tick.
    const auto finite = [] (const glm::vec3& value) {
	return std::isfinite (value.x) && std::isfinite (value.y) && std::isfinite (value.z);
    };

    if (!finite (transform.eye) || !finite (transform.center) || !finite (transform.up)
	|| !std::isfinite (transform.zoom)) {
	return JS_UNDEFINED;
    }

    scene.setScriptCameraTransform (transform);
    return JS_UNDEFINED;
}

JSValue scene_get_layer_count (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    auto* container = get_opaque (this_val);
    int count = 0;

    for (const auto* object : container->getScene ().getObjectsByCursorOrder ()) {
	if (object != nullptr && object->is<ScriptableObject> ()) {
	    ++count;
	}
    }

    return JS_NewInt32 (ctx, count);
}

// thisScene.enumerateLayers() -> array of every scriptable layer in authored stacking order.
JSValue scene_enumerate_layers (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    auto* container = get_opaque (this_val);
    JSValue arr = JS_NewArray (ctx);
    uint32_t index = 0;

    for (auto* object : container->getScene ().getObjectsByCursorOrder ()) {
	if (object == nullptr || !object->is<ScriptableObject> ()) {
	    continue;
	}
	JS_SetPropertyUint32 (
	    ctx, arr, index++, container->getEngine ().getAdapters ().object->instantiate (*object->as<ScriptableObject> ())
	);
    }

    return arr;
}

// thisScene.getLayerByID(id) -> the layer whose object id matches (id given as a string), or undefined.
JSValue scene_get_layer_by_id (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    if (argc != 1) {
	return JS_ThrowTypeError (ctx, "getLayerByID() expects exactly one argument");
    }

    auto* container = get_opaque (this_val);
    const char* str = JS_ToCString (ctx, argv[0]);

    if (str == nullptr) {
	return JS_UNDEFINED;
    }

    const long id = std::strtol (str, nullptr, 10);
    JS_FreeCString (ctx, str);

    for (auto* object : container->getScene ().getObjectsByRenderOrder ()) {
	if (object == nullptr || !object->is<ScriptableObject> ()) {
	    continue;
	}
	if (object->getId () == id) {
	    return container->getEngine ().getAdapters ().object->instantiate (*object->as<ScriptableObject> ());
	}
    }

    return JS_UNDEFINED;
}

// thisScene.getLayerIndex(layer) -> index of the layer in the authored scriptable order, or -1.
JSValue scene_get_layer_index (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    if (argc != 1) {
	return JS_ThrowTypeError (ctx, "getLayerIndex() expects one argument");
    }

    auto* container = get_opaque (this_val);
    auto* layer = WallpaperEngine::Scripting::Adapters::ScriptableObjectAdapter::fromJS (argv[0]);

    if (layer == nullptr) {
	return JS_NewInt32 (ctx, -1);
    }

    return JS_NewInt32 (ctx, container->getScene ().getScriptableLayerIndex (layer));
}

namespace {
struct ModelDataHandle {
    SceneObject* scene;
    uint32_t token;
};
void finalize_model_data (JSRuntime*, JSValue value) {
    JSClassID id;
    delete static_cast<ModelDataHandle*> (JS_GetAnyOpaque (value, &id));
}

bool model_bound (JSContext* ctx, JSValueConst value, glm::vec3& result) {
    constexpr const char* names[] = { "x", "y", "z" };
    for (int axis = 0; axis < 3; ++axis) {
	ModelDataInput::Value component (ctx, JS_GetPropertyStr (ctx, value, names[axis]));
	double number = 0;
	if (JS_IsException (component.value) || JS_ToFloat64 (ctx, &number, component.value) < 0) {
	    return false;
	}
	result[axis] = static_cast<float> (number);
    }
    return true;
}
}

JSValue scene_create_model_data (JSContext* ctx, JSValueConst self, int argc, JSValueConst* argv) {
    auto* container = get_opaque (self);
    if (container->getEngine ().getRegisteringAssets () != nullptr) {
	return JS_ThrowSyntaxError (ctx, "createModelData cannot be called from global scope.");
    }
    if (argc < 1) {
	return JS_UNDEFINED;
    }
    std::vector<ModelDataShape> shapes;
    ModelDataInput::Value shapeArray (ctx, JS_GetPropertyStr (ctx, argv[0], "shapes"));
    if (JS_IsException (shapeArray.value)) {
	return JS_EXCEPTION;
    }
    if (JS_IsArray (shapeArray.value)) {
	ModelDataInput::Value length (ctx, JS_GetPropertyStr (ctx, shapeArray.value, "length"));
	uint32_t count = 0;
	if (JS_IsException (length.value) || JS_ToUint32 (ctx, &count, length.value) < 0) {
	    return JS_EXCEPTION;
	}
	for (uint32_t index = 0; index < count; ++index) {
	    ModelDataInput::Value input (ctx, JS_GetPropertyUint32 (ctx, shapeArray.value, index));
	    ModelDataShape shape;
	    if (JS_IsException (input.value) || !ModelDataInput::readShape (ctx, input.value, shape)) {
		return JS_EXCEPTION;
	    }
	    shapes.push_back (std::move (shape));
	}
    } else {
	ModelDataShape shape;
	if (!ModelDataInput::readShape (ctx, argv[0], shape)) {
	    return JS_EXCEPTION;
	}
	shapes.push_back (std::move (shape));
    }
    std::optional<std::pair<glm::vec3, glm::vec3>> bounds;
    ModelDataInput::Value minimum (ctx, JS_GetPropertyStr (ctx, argv[0], "boundingBoxMins"));
    if (JS_IsException (minimum.value)) {
	return JS_EXCEPTION;
    }
    ModelDataInput::Value maximum (ctx, JS_GetPropertyStr (ctx, argv[0], "boundingBoxMaxs"));
    if (JS_IsException (maximum.value)) {
	return JS_EXCEPTION;
    }
    if (JS_IsObject (minimum.value) && JS_IsObject (maximum.value)) {
	bounds.emplace ();
	if (!model_bound (ctx, minimum.value, bounds->first) || !model_bound (ctx, maximum.value, bounds->second)) {
	    return JS_EXCEPTION;
	}
    }
    if (shapes.empty ()) {
        return JS_ThrowSyntaxError (ctx, "Shapes missing.");
    }
    try {
	return container->makeModelDataHandle (container->getScene ().createModelData (std::move (shapes), bounds));
    } catch (const std::exception& error) {
	return JS_ThrowSyntaxError (ctx, "%s", error.what ());
    }
}

JSValue scene_destroy_model_data (JSContext* ctx, JSValueConst self, int argc, JSValueConst* argv) {
    if (get_opaque (self)->getEngine ().getRegisteringAssets () != nullptr) {
	return JS_ThrowSyntaxError (ctx, "destroyModelData cannot be called from global scope.");
    }
    // Native checks IsUint32; documented handle objects, strings and fractions are ignored.
    if (argc == 0 || !JS_IsNumber (argv[0])) {
	return JS_UNDEFINED;
    }
    double token = 0;
    if (JS_ToFloat64 (ctx, &token, argv[0]) < 0) {
	return JS_EXCEPTION;
    }
    if (std::isfinite (token) && token >= 0 && token <= UINT32_MAX && std::floor (token) == token) {
	get_opaque (self)->getScene ().destroyModelData (static_cast<uint32_t> (token));
    }
    return JS_UNDEFINED;
}

static JSValue model_change_data (JSContext* ctx, JSValueConst self, int argc, JSValueConst* argv, int classId, bool replace) {
    auto* handle = static_cast<ModelDataHandle*> (JS_GetOpaque (self, static_cast<JSClassID> (classId)));
    if (handle == nullptr) {
	return JS_ThrowTypeError (ctx, "Invalid IModelData receiver");
    }
    if (replace && handle->scene->getEngine ().isInUpdate ()) {
	return JS_ThrowSyntaxError (ctx, "IModelData.replace cannot be called in update.");
    }
    if (argc < 1) {
	return JS_UNDEFINED;
    }
    struct Capacity { size_t vertexBytes; size_t indexBytes; uint8_t indexWidth; uint32_t vertexFormat; };
    std::vector<Capacity> capacities;
    if (auto resource = handle->scene->getScene ().findModelData (handle->token); resource) {
	for (const auto& shape : resource->shapes) {
	    capacities.push_back ({ !replace || shape.input.vertexDynamic ? shape.input.vertices.size () * sizeof (float) : 0,
		!replace || shape.input.indexDynamic ? shape.input.indices.size () : 0, shape.input.indexWidth, shape.input.vertexFormat });
	}
    }
    // Do not keep a resource lease across property getters invoked by parsing.
    // A getter may release the script's last reference before the native lookup.
    std::vector<ModelDataShape> updates;
    ModelDataInput::Value shapeArray (ctx, JS_GetPropertyStr (ctx, argv[0], "shapes"));
    if (JS_IsException (shapeArray.value)) {
	return JS_EXCEPTION;
    }
    if (JS_IsArray (shapeArray.value)) {
	ModelDataInput::Value length (ctx, JS_GetPropertyStr (ctx, shapeArray.value, "length"));
	uint32_t count = 0;
	if (JS_IsException (length.value) || JS_ToUint32 (ctx, &count, length.value) < 0) {
	    return JS_EXCEPTION;
	}
	for (uint32_t index = 0; index < count; ++index) {
	    ModelDataInput::Value input (ctx, JS_GetPropertyUint32 (ctx, shapeArray.value, index));
	    ModelDataShape update;
	    if (JS_IsException (input.value) || !ModelDataInput::readShape (ctx, input.value, update, false,
		    index < capacities.size () ? capacities[index].vertexBytes : 0,
		    index < capacities.size () ? capacities[index].indexBytes : 0, replace, index < capacities.size () ? capacities[index].indexWidth : 0,
		    index < capacities.size () ? capacities[index].vertexFormat : 0)) {
		return JS_EXCEPTION;
	    }
	    updates.push_back (std::move (update));
	}
    } else {
	ModelDataShape update;
	if (!ModelDataInput::readShape (ctx, argv[0], update, false,
		capacities.empty () ? 0 : capacities[0].vertexBytes, capacities.empty () ? 0 : capacities[0].indexBytes, replace, capacities.empty () ? 0 : capacities[0].indexWidth,
		capacities.empty () ? 0 : capacities[0].vertexFormat)) {
	    return JS_EXCEPTION;
	}
	updates.push_back (std::move (update));
    }
    if (updates.empty ()) {
	return JS_ThrowSyntaxError (ctx, "Shapes missing.");
    }
    auto resource = handle->scene->getScene ().findModelData (handle->token);
    if (!resource) {
	return JS_ThrowSyntaxError (ctx, "Invalid model data token");
    }
    if (const char* error = replace ? resource->replaceData (std::move (updates)) : resource->applyData (updates)) {
	return JS_ThrowSyntaxError (ctx, "%s", error);
    }
    return JS_UNDEFINED;
}

static JSValue model_apply_data (JSContext* ctx, JSValueConst self, int argc, JSValueConst* argv, int classId) {
    return model_change_data (ctx, self, argc, argv, classId, false);
}

static JSValue model_replace_data (JSContext* ctx, JSValueConst self, int argc, JSValueConst* argv, int classId) {
    return model_change_data (ctx, self, argc, argv, classId, true);
}

JSValue SceneObject::makeModelDataHandle (uint32_t token) {
    auto* ctx = m_engine.getContext ();
    JSValue result = JS_NewObjectClass (ctx, m_modelDataClassId);
    JS_SetOpaque (result, new ModelDataHandle { this, token });
    JS_DefinePropertyValueStr (ctx, result, "__modelDataToken", JS_NewUint32 (ctx, token), 0);
    return result;
}

static JSValue layer_config_replacer (
    JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv, int magic, JSValue* data
) {
    if (argc < 2) return JS_UNDEFINED;
    if (JS_IsObject (argv[1])) {
	ModelDataInput::Value convert (ctx, JS_GetPropertyStr (ctx, argv[1], "toConfigString"));
	if (JS_IsException (convert.value)) {
	    return JS_EXCEPTION;
	}
	if (JS_IsFunction (ctx, convert.value)) {
	    return JS_Call (ctx, convert.value, argv[1], 0, nullptr);
	}
    }
    const auto& adapters = get_opaque (data[0])->getEngine ().getAdapters ();
    const int components = adapters.vec2->isInstance (argv[1]) ? 2
	: adapters.vec3->isInstance (argv[1]) ? 3 : adapters.vec4->isInstance (argv[1]) ? 4 : 0;
    if (components == 0) return JS_DupValue (ctx, argv[1]);

    constexpr const char* names[] = { "x", "y", "z", "w" };
    std::string text;
    for (int i = 0; i < components; ++i) {
	JSValue component = JS_GetPropertyStr (ctx, argv[1], names[i]);
	if (JS_IsException (component)) return component;
	const char* value = JS_ToCString (ctx, component);
	JS_FreeValue (ctx, component);
	if (value == nullptr) return JS_EXCEPTION;
	if (i != 0) text += ' ';
	text += value;
	JS_FreeCString (ctx, value);
    }
    return JS_NewString (ctx, text.c_str ());
}

// Assets and full layer configurations share the native object parser.
JSValue scene_create_layer (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    if (argc < 1) {
	return JS_UNDEFINED;
    }

    auto* container = get_opaque (this_val);
    // registerAsset returns a path string. Also accept an object with a file path
    // or a complete layer configuration.
    JSValue source = JS_DupValue (ctx, argv[0]);

    if (JS_IsObject (source)) {
	JSValue file = JS_GetPropertyStr (ctx, source, "file");
	if (!JS_IsString (file)) {
	    JS_FreeValue (ctx, file);
	    // Native vectors expose components through exotic accessors, so JSON's
	    // ordinary own-property enumeration loses them. Convert just this layer
	    // configuration, including nested property settings, without mutating it.
	    JSValue functionData[] = { this_val };
	    JSValue replacer = JS_NewCFunctionData (ctx, layer_config_replacer, 2, 0, 1, functionData);
	    JSValue json = JS_JSONStringify (ctx, source, replacer, JS_UNDEFINED);
	    JS_FreeValue (ctx, replacer);
	    JS_FreeValue (ctx, source);
	    if (JS_IsException (json)) return json;
	    const char* data = JS_ToCString (ctx, json);
	    JS_FreeValue (ctx, json);
	    if (data == nullptr) return JS_EXCEPTION;
	    ScopeGuard guard ([=] { JS_FreeCString (ctx, data); });
	    try {
		auto config = WallpaperEngine::Data::JSON::JSON::parse (data);
		if (config.is_number ()) {
		    config = WallpaperEngine::Data::JSON::JSON { { "model", config } };
		}
		auto* layer = container->getScene ().createLayer (
		    std::move (config), container->getEngine ().getRunningModuleWorkshopId ());
		return layer != nullptr && layer->is<ScriptableObject> ()
		    ? container->getEngine ().getAdapters ().object->instantiate (*layer->as<ScriptableObject> ())
		    : JS_UNDEFINED;
	    } catch (const std::exception& e) {
		return JS_ThrowTypeError (ctx, "Invalid layer configuration: %s", e.what ());
	    }
	}
	JS_FreeValue (ctx, source);
	source = file;
    }

    if (!JS_IsString (source)) {
	JS_FreeValue (ctx, source);
	return JS_UNDEFINED;
    }

    const char* path = JS_ToCString (ctx, source);
    JS_FreeValue (ctx, source);

    if (path == nullptr) {
	return JS_UNDEFINED;
    }

    ScopeGuard guard ([=] { JS_FreeCString (ctx, path); });

    const std::string workshopId = container->getEngine ().getRunningModuleWorkshopId ();
    auto* layer = container->getScene ().createLayer (std::string (path), workshopId);

    if (layer == nullptr || !layer->is<ScriptableObject> ()) {
	return JS_UNDEFINED;
    }

    return container->getEngine ().getAdapters ().object->instantiate (*layer->as<ScriptableObject> ());
}

static ScriptableObject* resolve_layer_argument (
    JSContext* ctx, JSValueConst scene, JSValueConst argument
) {
    if (auto* layer = WallpaperEngine::Scripting::Adapters::ScriptableObjectAdapter::fromJS (argument)) return layer;
    JSValue value = get_layer (ctx, scene, 1, &argument);
    auto* layer = WallpaperEngine::Scripting::Adapters::ScriptableObjectAdapter::fromJS (value);
    JS_FreeValue (ctx, value);
    return layer;
}

JSValue scene_get_initial_layer_config (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    auto* layer = argc > 0 ? resolve_layer_argument (ctx, this_val, argv[0]) : nullptr;
    if (layer == nullptr) return JS_UNDEFINED;
    const auto& config = layer->getObject ().initialConfiguration;
    return JS_ParseJSON (ctx, config.data (), config.size (), "<initial-layer-config>");
}

JSValue scene_destroy_layer (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    auto* layer = argc > 0 ? resolve_layer_argument (ctx, this_val, argv[0]) : nullptr;
    return JS_NewBool (ctx, get_opaque (this_val)->getScene ().destroyLayer (layer));
}

// thisScene.sortLayer(layer, index) -> move a layer to a z-position in the render order.
JSValue scene_sort_layer (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    if (argc < 2) {
	return JS_UNDEFINED;
    }

    auto* container = get_opaque (this_val);
    auto* layer = WallpaperEngine::Scripting::Adapters::ScriptableObjectAdapter::fromJS (argv[0]);

    if (layer == nullptr) {
	return JS_UNDEFINED;
    }

    int index = 0;
    JS_ToInt32 (ctx, &index, argv[1]);
    container->getScene ().moveLayerToScriptableIndex (layer, index);

    return JS_UNDEFINED;
}

SceneObject::SceneObject (ScriptEngine& engine, Render::Wallpapers::CScene& scene) :
    m_scene (scene), m_engine (engine), m_classId (0) {
    this->m_definition = { .class_name = "IScene" };
    JS_NewClassID (this->m_engine.getRuntime (), &this->m_classId);
    JS_NewClass (this->m_engine.getRuntime (), this->m_classId, &this->m_definition);
    this->m_instance = JS_NewObjectClass (this->m_engine.getContext (), this->m_classId);

    JS_DupValue (this->m_engine.getContext (), this->m_instance);

    // IModelData's script object carries a token; it does not own the geometry lease.
    auto* ctx = m_engine.getContext ();
    const JSClassDef modelDataDefinition { .class_name = "IModelData", .finalizer = finalize_model_data };
    JS_NewClassID (m_engine.getRuntime (), &m_modelDataClassId);
    JS_NewClass (m_engine.getRuntime (), m_modelDataClassId, &modelDataDefinition);
    constexpr char modelDataClass[] = R"JS((class IModelData {
        static POSITION = 'position'; static NORMAL = 'normal'; static TANGENT_SIGNED = 'tangentSigned';
        static UV = 'uv'; static COLOR = 'color';
        toConfigString() { return this.__modelDataToken; }
    }))JS";
    JSValue constructor
	= JS_Eval (ctx, modelDataClass, sizeof (modelDataClass) - 1, "<IModelData>", JS_EVAL_TYPE_GLOBAL);
    JSValue modelPrototype = JS_GetPropertyStr (ctx, constructor, "prototype");
    JS_SetPropertyStr (
	ctx, modelPrototype, "applyData", JS_NewCFunctionMagic (ctx, model_apply_data, "applyData", 1,
	    JS_CFUNC_generic_magic, static_cast<int> (m_modelDataClassId))
    );
    JS_SetPropertyStr (
	ctx, modelPrototype, "replaceData", JS_NewCFunctionMagic (ctx, model_replace_data, "replaceData", 1,
	    JS_CFUNC_generic_magic, static_cast<int> (m_modelDataClassId))
    );
    JS_SetClassProto (ctx, m_modelDataClassId, modelPrototype);
    JSValue global = JS_GetGlobalObject (ctx);
    JS_SetPropertyStr (ctx, global, "IModelData", constructor);
    JS_FreeValue (ctx, global);

    // set properties
    JS_SetOpaque (this->m_instance, this);
    JS_DefinePropertyGetSet (
	this->m_engine.getContext (), this->m_instance, JS_NewAtom (this->m_engine.getContext (), "gravitydirection"),
	JS_NewCFunction (this->m_engine.getContext (), get_gravitydirection, "get", 0), JS_UNDEFINED, JS_PROP_ENUMERABLE
    );
    JS_DefinePropertyGetSet (
	this->m_engine.getContext (), this->m_instance, JS_NewAtom (this->m_engine.getContext (), "gravitystrength"),
	JS_NewCFunction (this->m_engine.getContext (), get_gravitystrength, "get", 0), JS_UNDEFINED, JS_PROP_ENUMERABLE
    );
    JS_DefinePropertyGetSet (
	this->m_engine.getContext (), this->m_instance, JS_NewAtom (this->m_engine.getContext (), "windenabled"),
	JS_NewCFunction (this->m_engine.getContext (), get_windenabled, "get", 0), JS_UNDEFINED, JS_PROP_ENUMERABLE
    );
    JS_DefinePropertyGetSet (
	this->m_engine.getContext (), this->m_instance, JS_NewAtom (this->m_engine.getContext (), "winddirection"),
	JS_NewCFunction (this->m_engine.getContext (), get_winddirection, "get", 0), JS_UNDEFINED, JS_PROP_ENUMERABLE
    );
    JS_DefinePropertyGetSet (
	this->m_engine.getContext (), this->m_instance, JS_NewAtom (this->m_engine.getContext (), "windstrength"),
	JS_NewCFunction (this->m_engine.getContext (), get_windstrength, "get", 0), JS_UNDEFINED, JS_PROP_ENUMERABLE
    );
    JS_DefinePropertyGetSet (
	this->m_engine.getContext (), this->m_instance, JS_NewAtom (this->m_engine.getContext (), "bloom"),
	JS_NewCFunction (this->m_engine.getContext (), get_bloom, "get", 0),
	JS_NewCFunctionMagic (this->m_engine.getContext (), set_scene_field, "set", 1, JS_CFUNC_generic_magic,
	    static_cast<int> (SceneField::Bloom)), JS_PROP_ENUMERABLE
    );
    JS_DefinePropertyGetSet (
	this->m_engine.getContext (), this->m_instance, JS_NewAtom (this->m_engine.getContext (), "bloomstrength"),
	JS_NewCFunction (this->m_engine.getContext (), get_bloomstrength, "get", 0),
	JS_NewCFunctionMagic (this->m_engine.getContext (), set_scene_field, "set", 1, JS_CFUNC_generic_magic,
	    static_cast<int> (SceneField::BloomStrength)), JS_PROP_ENUMERABLE
    );
    JS_DefinePropertyGetSet (
	this->m_engine.getContext (), this->m_instance, JS_NewAtom (this->m_engine.getContext (), "bloomthreshold"),
	JS_NewCFunction (this->m_engine.getContext (), get_bloomthreshold, "get", 0),
	JS_NewCFunctionMagic (this->m_engine.getContext (), set_scene_field, "set", 1, JS_CFUNC_generic_magic,
	    static_cast<int> (SceneField::BloomThreshold)), JS_PROP_ENUMERABLE
    );
    JS_DefinePropertyGetSet (
	this->m_engine.getContext (), this->m_instance, JS_NewAtom (this->m_engine.getContext (), "clearenabled"),
	JS_NewCFunction (this->m_engine.getContext (), get_clearenabled, "get", 0),
	JS_NewCFunctionMagic (this->m_engine.getContext (), set_scene_field, "set", 1, JS_CFUNC_generic_magic,
	    static_cast<int> (SceneField::ClearEnabled)), JS_PROP_ENUMERABLE
    );
    JS_DefinePropertyGetSet (
	this->m_engine.getContext (), this->m_instance, JS_NewAtom (this->m_engine.getContext (), "clearcolor"),
	JS_NewCFunction (this->m_engine.getContext (), get_clearcolor, "get", 0),
	JS_NewCFunctionMagic (this->m_engine.getContext (), set_scene_field, "set", 1, JS_CFUNC_generic_magic,
	    static_cast<int> (SceneField::ClearColor)), JS_PROP_ENUMERABLE
    );
    JS_DefinePropertyGetSet (
	this->m_engine.getContext (), this->m_instance, JS_NewAtom (this->m_engine.getContext (), "ambientcolor"),
	JS_NewCFunction (this->m_engine.getContext (), get_ambientcolor, "get", 0),
	JS_NewCFunctionMagic (this->m_engine.getContext (), set_scene_field, "set", 1, JS_CFUNC_generic_magic,
	    static_cast<int> (SceneField::AmbientColor)), JS_PROP_ENUMERABLE
    );
    JS_DefinePropertyGetSet (
	this->m_engine.getContext (), this->m_instance, JS_NewAtom (this->m_engine.getContext (), "skylightcolor"),
	JS_NewCFunction (this->m_engine.getContext (), get_skylightcolor, "get", 0),
	JS_NewCFunctionMagic (this->m_engine.getContext (), set_scene_field, "set", 1, JS_CFUNC_generic_magic,
	    static_cast<int> (SceneField::SkylightColor)), JS_PROP_ENUMERABLE
    );
    JS_DefinePropertyGetSet (
	this->m_engine.getContext (), this->m_instance, JS_NewAtom (this->m_engine.getContext (), "fov"),
	JS_NewCFunction (this->m_engine.getContext (), get_fov, "get", 0),
	JS_NewCFunctionMagic (this->m_engine.getContext (), set_scene_field, "set", 1, JS_CFUNC_generic_magic,
	    static_cast<int> (SceneField::Fov)), JS_PROP_ENUMERABLE
    );
    JS_DefinePropertyGetSet (
	this->m_engine.getContext (), this->m_instance, JS_NewAtom (this->m_engine.getContext (), "nearz"),
	JS_NewCFunction (this->m_engine.getContext (), get_nearz, "get", 0),
	JS_NewCFunctionMagic (this->m_engine.getContext (), set_scene_field, "set", 1, JS_CFUNC_generic_magic,
	    static_cast<int> (SceneField::NearZ)), JS_PROP_ENUMERABLE
    );
    JS_DefinePropertyGetSet (
	this->m_engine.getContext (), this->m_instance, JS_NewAtom (this->m_engine.getContext (), "farz"),
	JS_NewCFunction (this->m_engine.getContext (), get_farz, "get", 0),
	JS_NewCFunctionMagic (this->m_engine.getContext (), set_scene_field, "set", 1, JS_CFUNC_generic_magic,
	    static_cast<int> (SceneField::FarZ)), JS_PROP_ENUMERABLE
    );
    JS_DefinePropertyGetSet (
	this->m_engine.getContext (), this->m_instance, JS_NewAtom (this->m_engine.getContext (), "camerafade"),
	JS_NewCFunction (this->m_engine.getContext (), get_camerafade, "get", 0),
	JS_NewCFunctionMagic (this->m_engine.getContext (), set_scene_field, "set", 1, JS_CFUNC_generic_magic,
	    static_cast<int> (SceneField::CameraFade)), JS_PROP_ENUMERABLE
    );
    JS_DefinePropertyGetSet (
	this->m_engine.getContext (), this->m_instance, JS_NewAtom (this->m_engine.getContext (), "camerashake"),
	JS_NewCFunction (this->m_engine.getContext (), get_camerashake, "get", 0),
	JS_NewCFunctionMagic (this->m_engine.getContext (), set_scene_field, "set", 1, JS_CFUNC_generic_magic,
	    static_cast<int> (SceneField::CameraShake)), JS_PROP_ENUMERABLE
    );
    JS_DefinePropertyGetSet (
	this->m_engine.getContext (), this->m_instance, JS_NewAtom (this->m_engine.getContext (), "camerashakespeed"),
	JS_NewCFunction (this->m_engine.getContext (), get_camerashakespeed, "get", 0),
	JS_NewCFunctionMagic (this->m_engine.getContext (), set_scene_field, "set", 1, JS_CFUNC_generic_magic,
	    static_cast<int> (SceneField::CameraShakeSpeed)), JS_PROP_ENUMERABLE
    );
    JS_DefinePropertyGetSet (
	this->m_engine.getContext (), this->m_instance,
	JS_NewAtom (this->m_engine.getContext (), "camerashakeamplitude"),
	JS_NewCFunction (this->m_engine.getContext (), get_camerashakeamplitude, "get", 0),
	JS_NewCFunctionMagic (this->m_engine.getContext (), set_scene_field, "set", 1, JS_CFUNC_generic_magic,
	    static_cast<int> (SceneField::CameraShakeAmplitude)), JS_PROP_ENUMERABLE
    );
    JS_DefinePropertyGetSet (
	this->m_engine.getContext (), this->m_instance,
	JS_NewAtom (this->m_engine.getContext (), "camerashakeroughness"),
	JS_NewCFunction (this->m_engine.getContext (), get_camerashakeroughness, "get", 0),
	JS_NewCFunctionMagic (this->m_engine.getContext (), set_scene_field, "set", 1, JS_CFUNC_generic_magic,
	    static_cast<int> (SceneField::CameraShakeRoughness)), JS_PROP_ENUMERABLE
    );
    JS_DefinePropertyGetSet (
	this->m_engine.getContext (), this->m_instance, JS_NewAtom (this->m_engine.getContext (), "cameraparallax"),
	JS_NewCFunction (this->m_engine.getContext (), get_cameraparallax, "get", 0),
	JS_NewCFunctionMagic (this->m_engine.getContext (), set_scene_field, "set", 1, JS_CFUNC_generic_magic,
	    static_cast<int> (SceneField::CameraParallax)), JS_PROP_ENUMERABLE
    );
    JS_DefinePropertyGetSet (
	this->m_engine.getContext (), this->m_instance,
	JS_NewAtom (this->m_engine.getContext (), "cameraparallaxamount"),
	JS_NewCFunction (this->m_engine.getContext (), get_cameraparallaxamount, "get", 0),
	JS_NewCFunctionMagic (this->m_engine.getContext (), set_scene_field, "set", 1, JS_CFUNC_generic_magic,
	    static_cast<int> (SceneField::CameraParallaxAmount)), JS_PROP_ENUMERABLE
    );
    JS_DefinePropertyGetSet (
	this->m_engine.getContext (), this->m_instance,
	JS_NewAtom (this->m_engine.getContext (), "cameraparallaxdelay"),
	JS_NewCFunction (this->m_engine.getContext (), get_cameraparallaxdelay, "get", 0),
	JS_NewCFunctionMagic (this->m_engine.getContext (), set_scene_field, "set", 1, JS_CFUNC_generic_magic,
	    static_cast<int> (SceneField::CameraParallaxDelay)), JS_PROP_ENUMERABLE
    );
    JS_DefinePropertyGetSet (
	this->m_engine.getContext (), this->m_instance,
	JS_NewAtom (this->m_engine.getContext (), "cameraparallaxmouseinfluence"),
	JS_NewCFunction (this->m_engine.getContext (), get_cameraparallaxmouseinfluence, "get", 0),
	JS_NewCFunctionMagic (this->m_engine.getContext (), set_scene_field, "set", 1, JS_CFUNC_generic_magic,
	    static_cast<int> (SceneField::CameraParallaxMouseInfluence)), JS_PROP_ENUMERABLE
    );
    JS_DefinePropertyValueStr (
	this->m_engine.getContext (), this->m_instance, "getLayer",
	JS_NewCFunction (this->m_engine.getContext (), get_layer, "getLayer", 1), JS_PROP_ENUMERABLE
    );
    JS_DefinePropertyValueStr (
	this->m_engine.getContext (), this->m_instance, "getLayerCount",
	JS_NewCFunction (this->m_engine.getContext (), scene_get_layer_count, "getLayerCount", 0), JS_PROP_ENUMERABLE
    );
    JS_DefinePropertyValueStr (
	this->m_engine.getContext (), this->m_instance, "enumerateLayers",
	JS_NewCFunction (this->m_engine.getContext (), scene_enumerate_layers, "enumerateLayers", 0), JS_PROP_ENUMERABLE
    );
    JS_DefinePropertyValueStr (
	this->m_engine.getContext (), this->m_instance, "getLayerByID",
	JS_NewCFunction (this->m_engine.getContext (), scene_get_layer_by_id, "getLayerByID", 1), JS_PROP_ENUMERABLE
    );
    JS_DefinePropertyValueStr (
	this->m_engine.getContext (), this->m_instance, "getCameraTransforms",
	JS_NewCFunction (this->m_engine.getContext (), scene_get_camera_transforms, "getCameraTransforms", 0),
	JS_PROP_ENUMERABLE
    );
    JS_DefinePropertyValueStr (
	this->m_engine.getContext (), this->m_instance, "setCameraTransforms",
	JS_NewCFunction (this->m_engine.getContext (), scene_set_camera_transforms, "setCameraTransforms", 1),
	JS_PROP_ENUMERABLE
    );
    JS_DefinePropertyValueStr (
	this->m_engine.getContext (), this->m_instance, "getLayerIndex",
	JS_NewCFunction (this->m_engine.getContext (), scene_get_layer_index, "getLayerIndex", 1), JS_PROP_ENUMERABLE
    );
    JS_DefinePropertyValueStr (
	this->m_engine.getContext (), this->m_instance, "createLayer",
	JS_NewCFunction (this->m_engine.getContext (), scene_create_layer, "createLayer", 1), JS_PROP_ENUMERABLE
    );
    JS_DefinePropertyValueStr (
	this->m_engine.getContext (), this->m_instance, "getInitialLayerConfig",
	JS_NewCFunction (this->m_engine.getContext (), scene_get_initial_layer_config, "getInitialLayerConfig", 1),
	JS_PROP_ENUMERABLE
    );
    JS_DefinePropertyValueStr (
	this->m_engine.getContext (), this->m_instance, "destroyLayer",
	JS_NewCFunction (this->m_engine.getContext (), scene_destroy_layer, "destroyLayer", 1), JS_PROP_ENUMERABLE
    );
    JS_DefinePropertyValueStr (
	this->m_engine.getContext (), this->m_instance, "sortLayer",
	JS_NewCFunction (this->m_engine.getContext (), scene_sort_layer, "sortLayer", 2), JS_PROP_ENUMERABLE
    );
    JS_DefinePropertyValueStr (
	ctx, m_instance, "createModelData", JS_NewCFunction (ctx, scene_create_model_data, "createModelData", 1),
	JS_PROP_ENUMERABLE
    );
    JS_DefinePropertyValueStr (
	ctx, m_instance, "destroyModelData", JS_NewCFunction (ctx, scene_destroy_model_data, "destroyModelData", 1),
	JS_PROP_ENUMERABLE
    );
    // TODO: ADD REST OF THE METHODS
}

SceneObject::~SceneObject () { JS_FreeValue (this->m_engine.getContext (), this->m_instance); }
