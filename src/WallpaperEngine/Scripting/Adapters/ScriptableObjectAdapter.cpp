#include "ScriptableObjectAdapter.h"

#include <utility>

#include "WallpaperEngine/Data/Utils/ScopeGuard.h"
#include "WallpaperEngine/Logging/Log.h"
#include "WallpaperEngine/Scripting/ScriptEngine.h"
#include "WallpaperEngine/Scripting/ScriptableObject.h"

#include <string>

using namespace WallpaperEngine::Data::Model;
using namespace WallpaperEngine::Data::Utils;
using namespace WallpaperEngine::Scripting::Adapters;

#define SCRIPTABLE_OPAQUE_MAGIC 0xdeadbeef
#define ANIMATION_LAYER_OPAQUE_MAGIC 0x51a9e11

struct OpaqueScriptableObjectAdapter {
    unsigned int magic;
    ScriptableObjectAdapter& adapter;
    WallpaperEngine::Scripting::ScriptableObject& object;
};

namespace {
struct OpaqueAnimationLayer {
    unsigned int magic;
    WallpaperEngine::Scripting::ScriptEngine& engine;
    WallpaperEngine::Data::Model::DynamicValue& rate;
    WallpaperEngine::Data::Model::DynamicValue& visible;
};

OpaqueAnimationLayer* getAnimationLayer (JSContext* ctx, JSValueConst value) {
    JSClassID classId = 0;
    auto* handle = static_cast<OpaqueAnimationLayer*> (JS_GetAnyOpaque (value, &classId));
    if (handle == nullptr || handle->magic != ANIMATION_LAYER_OPAQUE_MAGIC) {
	JS_ThrowTypeError (ctx, "Invalid animation layer receiver");
	return nullptr;
    }
    return handle;
}

JSValue animation_layer_get_rate (JSContext* ctx, JSValueConst this_val) {
    auto* handle = getAnimationLayer (ctx, this_val);
    return handle == nullptr ? JS_EXCEPTION : handle->engine.dynamicToJs (handle->rate);
}

JSValue animation_layer_set_rate (JSContext* ctx, JSValueConst this_val, JSValueConst value) {
    auto* handle = getAnimationLayer (ctx, this_val);
    if (handle == nullptr) {
	return JS_EXCEPTION;
    }
    double rate = 0.0;
    if (JS_ToFloat64 (ctx, &rate, value) < 0) {
	return JS_EXCEPTION;
    }
    handle->rate.update (static_cast<float> (rate), WallpaperEngine::Data::Model::DynamicValue::UpdateSource::Script);
    return JS_UNDEFINED;
}

JSValue animation_layer_play (JSContext* ctx, JSValueConst this_val, int, JSValueConst*) {
    auto* handle = getAnimationLayer (ctx, this_val);
    if (handle == nullptr) {
	return JS_EXCEPTION;
    }
    handle->visible.update (true, WallpaperEngine::Data::Model::DynamicValue::UpdateSource::Script);
    return JS_UNDEFINED;
}

JSValue animation_layer_stop (JSContext* ctx, JSValueConst this_val, int, JSValueConst*) {
    auto* handle = getAnimationLayer (ctx, this_val);
    if (handle == nullptr) {
	return JS_EXCEPTION;
    }
    handle->visible.update (false, WallpaperEngine::Data::Model::DynamicValue::UpdateSource::Script);
    return JS_UNDEFINED;
}

void animation_layer_finalizer (JSRuntime*, JSValueConst value) {
    JSClassID classId = 0;
    auto* handle = static_cast<OpaqueAnimationLayer*> (JS_GetAnyOpaque (value, &classId));
    if (handle != nullptr && handle->magic == ANIMATION_LAYER_OPAQUE_MAGIC) {
	handle->magic = 0;
	delete handle;
    }
}

JSValue scriptableobject_get_animation_layer (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    JSClassID classId = 0;
    auto* container = static_cast<OpaqueScriptableObjectAdapter*> (JS_GetAnyOpaque (this_val, &classId));
    if (container == nullptr || container->magic != SCRIPTABLE_OPAQUE_MAGIC || argc < 1 || !JS_IsString (argv[0])) {
	return JS_ThrowTypeError (ctx, "getAnimationLayer requires an animation name");
    }

    const char* name = JS_ToCString (ctx, argv[0]);
    if (name == nullptr) {
	return JS_EXCEPTION;
    }
    ScopeGuard nameGuard ([ctx, name] () { JS_FreeCString (ctx, name); });

    const auto properties = container->object.findAnimationLayer (name);
    if (!properties.has_value () || properties->rate == nullptr || properties->visible == nullptr) {
	return JS_UNDEFINED;
    }

    return container->adapter.instantiateAnimationLayer (*properties->rate, *properties->visible);
}
} // namespace

namespace {
bool updateDynamicValue (
    JSContext* ctx, JSValueConst value, DynamicValue& target, const bool degreesToRadians = false
) {
    const int tag = JS_VALUE_GET_TAG (value);

    if (tag == JS_TAG_UNDEFINED || tag == JS_TAG_UNINITIALIZED || tag == JS_TAG_NULL) {
	target.update (DynamicValue::UpdateSource::Script);
	return true;
    }
    if (tag == JS_TAG_INT) {
	target.update (JS_VALUE_GET_INT (value), DynamicValue::UpdateSource::Script);
	return true;
    }
    if (tag == JS_TAG_BOOL) {
	target.update (JS_VALUE_GET_BOOL (value) != 0, DynamicValue::UpdateSource::Script);
	return true;
    }
    if (JS_TAG_IS_FLOAT64 (tag)) {
	target.update (static_cast<float> (JS_VALUE_GET_FLOAT64 (value)), DynamicValue::UpdateSource::Script);
	return true;
    }
    if (tag == JS_TAG_STRING) {
	const char* text = JS_ToCString (ctx, value);
	if (text == nullptr) {
	    return false;
	}
	target.update (std::string (text), DynamicValue::UpdateSource::Script);
	JS_FreeCString (ctx, text);
	return true;
    }
    if (tag != JS_TAG_OBJECT) {
	return false;
    }

    JSValue x = JS_GetPropertyStr (ctx, value, "x");
    JSValue y = JS_GetPropertyStr (ctx, value, "y");
    JSValue z = JS_GetPropertyStr (ctx, value, "z");
    JSValue w = JS_GetPropertyStr (ctx, value, "w");
    ScopeGuard guard ([=] {
	JS_FreeValue (ctx, x);
	JS_FreeValue (ctx, y);
	JS_FreeValue (ctx, z);
	JS_FreeValue (ctx, w);
    });

    if (!JS_IsNumber (x) || !JS_IsNumber (y)) {
	JS_ThrowTypeError (ctx, "Layer vector properties require numeric x and y components");
	return false;
    }

    double xValue = 0.0;
    double yValue = 0.0;
    double zValue = 0.0;
    double wValue = 0.0;
    if (JS_ToFloat64 (ctx, &xValue, x) < 0 || JS_ToFloat64 (ctx, &yValue, y) < 0) {
	return false;
    }
    if (JS_IsNumber (z) && JS_ToFloat64 (ctx, &zValue, z) < 0) {
	return false;
    }
    if (JS_IsNumber (w) && JS_ToFloat64 (ctx, &wValue, w) < 0) {
	return false;
    }

    switch (target.getType ()) {
	case DynamicValue::Vec2:
	    target.update (glm::vec2 (xValue, yValue), DynamicValue::UpdateSource::Script);
	    return true;
	case DynamicValue::Vec3:
	    if (!JS_IsNumber (z)) {
		JS_ThrowTypeError (ctx, "Layer Vec3 properties require a numeric z component");
		return false;
	    }
	    target.update (
		degreesToRadians ? glm::vec3 (xValue, yValue, zValue) * 0.017453292519943295769f
				 : glm::vec3 (xValue, yValue, zValue),
		DynamicValue::UpdateSource::Script
	    );
	    return true;
	case DynamicValue::Vec4:
	    if (!JS_IsNumber (z) || !JS_IsNumber (w)) {
		JS_ThrowTypeError (ctx, "Layer Vec4 properties require numeric z and w components");
		return false;
	    }
	    target.update (glm::vec4 (xValue, yValue, zValue, wValue), DynamicValue::UpdateSource::Script);
	    return true;
	default:
	    JS_ThrowTypeError (ctx, "Layer property type cannot be assigned a vector");
	    return false;
    }
}
} // namespace

JSValue scriptableobject_property_get (JSContext* ctx, JSValueConst obj_val, JSAtom atom, JSValueConst receiver) {
    JSClassID classId = 0;

    auto* container = static_cast<OpaqueScriptableObjectAdapter*> (JS_GetAnyOpaque (obj_val, &classId));

    if (!container || container->magic != SCRIPTABLE_OPAQUE_MAGIC) {
	return JS_ThrowTypeError (ctx, "Invalid scriptable layer receiver while reading a property");
    }

    const char* name = JS_AtomToCString (ctx, atom);

    if (name == nullptr) {
	return JS_EXCEPTION;
    }

    ScopeGuard guard ([=] { JS_FreeCString (ctx, name); });

    if (std::strcmp (name, "getAnimationLayer") == 0) {
	return JS_NewCFunction (ctx, scriptableobject_get_animation_layer, "getAnimationLayer", 1);
    }

    try {
	// find the property inside, otherwise return undefined
	auto& property = container->object.getProperty (name);
	const auto& adapters = container->adapter.getEngine ().getAdapters ();
	JSValue result = JS_UNDEFINED;
	switch (property.getType ()) {
	    case DynamicValue::Vec2:
		result = adapters.vec2->instantiate (property, true);
		break;
	    case DynamicValue::Vec3:
		result = std::strcmp (name, "angles") == 0 ? adapters.vec3->instantiateAngles (property, true)
								     : adapters.vec3->instantiate (property, true);
		break;
	    case DynamicValue::Vec4:
		result = adapters.vec4->instantiate (property, true);
		break;
	    default:
		result = std::strcmp (name, "angles") == 0 ? container->adapter.getEngine ().anglesToJs (property)
								   : container->adapter.getEngine ().dynamicToJs (property);
		break;
	}
	return result;
    } catch (const std::exception& e) {
	return JS_UNDEFINED;
    }
}

int scriptableobject_property_set (
    JSContext* ctx, JSValueConst obj_val, JSAtom atom, JSValueConst val, JSValueConst receiver, int flags
) {
    JSClassID classId = 0;

    auto* container = static_cast<OpaqueScriptableObjectAdapter*> (JS_GetAnyOpaque (obj_val, &classId));

    if (!container || container->magic != SCRIPTABLE_OPAQUE_MAGIC) {
	JS_ThrowTypeError (ctx, "Invalid scriptable layer receiver while setting a property");
	return -1;
    }

    const char* name = JS_AtomToCString (ctx, atom);

    if (name == nullptr) {
	return -1;
    }

    ScopeGuard guard ([=] { JS_FreeCString (ctx, name); });
    try {
	auto& property = container->object.getProperty (name);
	if (updateDynamicValue (ctx, val, property, std::strcmp (name, "angles") == 0)) {
	    return 1;
	}
	if (!JS_HasException (ctx)) {
	    JS_ThrowTypeError (ctx, "Unsupported value for scriptable layer property '%s'", name);
	}
	return -1;
    } catch (const std::exception&) {
	JS_ThrowTypeError (ctx, "Unknown scriptable layer property '%s'", name);
	return -1;
    }
}

ScriptableObjectAdapter::ScriptableObjectAdapter (ScriptEngine& engine, std::string name) :
    ObjectAdapter (engine), m_exoticMethods (), m_animationLayerClassId (JS_INVALID_CLASS_ID),
    m_name (std::move (name)) {
    this->m_exoticMethods = {
	.get_property = scriptableobject_property_get,
	.set_property = scriptableobject_property_set,
    };
    this->registerType (
	{
	    .class_name = m_name.c_str (),
	    .exotic = &m_exoticMethods,
	}
    );

    JS_NewClassID (engine.getRuntime (), &this->m_animationLayerClassId);
    JSClassDef animationLayerClass { .class_name = "WallpaperEngineAnimationLayer",
				     .finalizer = animation_layer_finalizer };
    JS_NewClass (engine.getRuntime (), this->m_animationLayerClassId, &animationLayerClass);
}

JSValue ScriptableObjectAdapter::instantiate (ScriptableObject& object) {
    JSValue result = this->ObjectAdapter::instantiate (object);
    JS_SetOpaque (
	result,
	new OpaqueScriptableObjectAdapter { .magic = SCRIPTABLE_OPAQUE_MAGIC, .adapter = *this, .object = object }
    );

    return result;
}

JSValue ScriptableObjectAdapter::instantiate (DynamicValue& value) {
    throw std::runtime_error ("Cannot create a ScriptableObject instance from a DynamicValue");
}

JSValue ScriptableObjectAdapter::instantiateAnimationLayer (DynamicValue& rate, DynamicValue& visible) {
    JSContext* ctx = this->m_engine.getContext ();
    JSValue result = JS_NewObjectClass (ctx, this->m_animationLayerClassId);
    auto* handle = new OpaqueAnimationLayer {
	.magic = ANIMATION_LAYER_OPAQUE_MAGIC, .engine = this->m_engine, .rate = rate, .visible = visible
    };
    JS_SetOpaque (result, handle);

    JSValue rateGetter = JS_NewCFunction2 (
	ctx, reinterpret_cast<JSCFunction*> (animation_layer_get_rate), "get rate", 0, JS_CFUNC_getter, 0
    );
    JSValue rateSetter = JS_NewCFunction2 (
	ctx, reinterpret_cast<JSCFunction*> (animation_layer_set_rate), "set rate", 1, JS_CFUNC_setter, 0
    );
    const JSAtom rateAtom = JS_NewAtom (ctx, "rate");
    JS_DefinePropertyGetSet (ctx, result, rateAtom, rateGetter, rateSetter, JS_PROP_ENUMERABLE);
    JS_FreeAtom (ctx, rateAtom);
    JS_DefinePropertyValueStr (
	ctx, result, "play", JS_NewCFunction (ctx, animation_layer_play, "play", 0), JS_PROP_ENUMERABLE
    );
    JS_DefinePropertyValueStr (
	ctx, result, "stop", JS_NewCFunction (ctx, animation_layer_stop, "stop", 0), JS_PROP_ENUMERABLE
    );
    return result;
}
