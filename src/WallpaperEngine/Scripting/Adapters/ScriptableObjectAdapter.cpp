#include "ScriptableObjectAdapter.h"

#include <cstring>
#include <utility>

#include "WallpaperEngine/Data/Utils/ScopeGuard.h"
#include "WallpaperEngine/Render/Objects/CSound.h"
#include "WallpaperEngine/Scripting/ScriptEngine.h"
#include "WallpaperEngine/Scripting/ScriptableObject.h"

using namespace WallpaperEngine::Data::Model;
using namespace WallpaperEngine::Data::Utils;
using namespace WallpaperEngine::Scripting::Adapters;

#define SCRIPTABLE_OPAQUE_MAGIC 0xdeadbeef

struct OpaqueScriptableObjectAdapter {
    unsigned int magic;
    ScriptableObjectAdapter& adapter;
    WallpaperEngine::Scripting::ScriptableObject& object;
};

static OpaqueScriptableObjectAdapter* get_scriptable_object (JSValueConst value) {
    JSClassID classId = 0;
    auto* container = static_cast<OpaqueScriptableObjectAdapter*> (JS_GetAnyOpaque (value, &classId));
    return container != nullptr && container->magic == SCRIPTABLE_OPAQUE_MAGIC ? container : nullptr;
}

static WallpaperEngine::Render::Objects::CSound* get_sound (JSValueConst value) {
    auto* container = get_scriptable_object (value);
    if (container == nullptr || !container->object.is<WallpaperEngine::Render::Objects::CSound> ()) {
		return nullptr;
    }

    return container->object.as<WallpaperEngine::Render::Objects::CSound> ();
}

JSValue scriptableobject_get_children (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    auto* container = get_scriptable_object (this_val);
    if (container == nullptr) {
		return JS_EXCEPTION;
    }

    JSValue result = JS_NewArray (ctx);
    uint32_t index = 0;
    for (auto* child : container->object.getChildren ()) {
		JS_SetPropertyUint32 (ctx, result, index++, container->adapter.instantiate (*child));
    }

    return result;
}

JSValue scriptableobject_sound_is_playing (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    auto* sound = get_sound (this_val);
    return sound == nullptr ? JS_UNDEFINED : JS_NewBool (ctx, sound->isPlaying ());
}

JSValue scriptableobject_sound_play (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    auto* sound = get_sound (this_val);
    if (sound != nullptr) {
		sound->play ();
    }
    return JS_UNDEFINED;
}

JSValue scriptableobject_sound_pause (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    auto* sound = get_sound (this_val);
    if (sound != nullptr) {
		sound->pause ();
    }
    return JS_UNDEFINED;
}

JSValue scriptableobject_sound_stop (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    auto* sound = get_sound (this_val);
    if (sound != nullptr) {
		sound->stop ();
    }
    return JS_UNDEFINED;
}

void scriptableobject_finalizer (JSRuntime* rt, JSValueConst val) {
    auto* container = get_scriptable_object (val);
    delete container;
}

JSValue scriptableobject_property_get (JSContext* ctx, JSValueConst obj_val, JSAtom atom, JSValueConst receiver) {
	    auto* container = get_scriptable_object (obj_val);

	    if (container == nullptr) {
		return JS_EXCEPTION;
	    }

    const char* name = JS_AtomToCString (ctx, atom);

    if (name == nullptr) {
	return JS_EXCEPTION;
    }

    ScopeGuard guard ([=] { JS_FreeCString (ctx, name); });

	    if (strcmp (name, "name") == 0) {
		return JS_NewString (ctx, container->object.getObject ().name.c_str ());
	    }
	    if (strcmp (name, "getChildren") == 0) {
		return JS_NewCFunction (ctx, scriptableobject_get_children, "getChildren", 0);
	    }

	    if (container->object.is<WallpaperEngine::Render::Objects::CSound> ()) {
		auto* sound = container->object.as<WallpaperEngine::Render::Objects::CSound> ();
		if (strcmp (name, "volume") == 0) {
		    return JS_NewFloat64 (ctx, sound->getVolume ());
		}
		if (strcmp (name, "isPlaying") == 0) {
		    return JS_NewCFunction (ctx, scriptableobject_sound_is_playing, "isPlaying", 0);
		}
		if (strcmp (name, "play") == 0) {
		    return JS_NewCFunction (ctx, scriptableobject_sound_play, "play", 0);
		}
		if (strcmp (name, "pause") == 0) {
		    return JS_NewCFunction (ctx, scriptableobject_sound_pause, "pause", 0);
		}
		if (strcmp (name, "stop") == 0) {
		    return JS_NewCFunction (ctx, scriptableobject_sound_stop, "stop", 0);
		}
	    }

    try {
	// find the property inside, otherwise return undefined
	auto& property = container->object.getProperty (name);

	return container->adapter.getEngine ().dynamicToJs (property);
    } catch (const std::exception& e) {
	return JS_UNDEFINED;
    }
}

int scriptableobject_property_set (
    JSContext* ctx, JSValueConst obj_val, JSAtom atom, JSValueConst val, JSValueConst receiver, int flags
) {
	    auto* container = get_scriptable_object (obj_val);

	    if (container == nullptr) {
		return -1;
	    }

    const char* name = JS_AtomToCString (ctx, atom);

	    if (name == nullptr) {
		return -1;
	    }

	    ScopeGuard guard ([=] { JS_FreeCString (ctx, name); });

	    if (strcmp (name, "volume") == 0 && container->object.is<WallpaperEngine::Render::Objects::CSound> ()) {
		if (!JS_IsNumber (val)) {
		    return -1;
		}

		double volume = 0.0;
		if (JS_ToFloat64 (ctx, &volume, val) < 0) {
		    return -1;
		}

		container->object.as<WallpaperEngine::Render::Objects::CSound> ()->setVolume (static_cast<float> (volume));
	    }

	    return 0;
}

ScriptableObjectAdapter::ScriptableObjectAdapter (ScriptEngine& engine, std::string name) :
    ObjectAdapter (engine), m_exoticMethods (), m_name (std::move (name)) {
    this->registerType (
		{
		    .class_name = m_name.c_str (),
		    .finalizer = scriptableobject_finalizer,
		    .exotic = &m_exoticMethods,
		}
    );
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
