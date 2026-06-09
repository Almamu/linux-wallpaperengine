#include "ScriptableObjectAdapter.h"

#include <utility>

#include "WallpaperEngine/Data/Utils/ScopeGuard.h"
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

JSValue scriptableobject_property_get (JSContext* ctx, JSValueConst obj_val, JSAtom atom, JSValueConst receiver) {
    JSClassID classId = 0;

    auto* container = static_cast<OpaqueScriptableObjectAdapter*> (JS_GetAnyOpaque (obj_val, &classId));

    if (!container || container->magic != SCRIPTABLE_OPAQUE_MAGIC) {
	return JS_EXCEPTION;
    }

    const char* name = JS_AtomToCString (ctx, atom);

    if (name == nullptr) {
	return JS_EXCEPTION;
    }

    ScopeGuard guard ([=] { JS_FreeCString (ctx, name); });

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
    JSClassID classId = 0;

    auto* container = static_cast<OpaqueScriptableObjectAdapter*> (JS_GetAnyOpaque (obj_val, &classId));

    if (!container || container->magic != SCRIPTABLE_OPAQUE_MAGIC) {
	return -1;
    }

    const char* name = JS_AtomToCString (ctx, atom);

    if (name == nullptr) {
	return -1;
    }

    return 0;
}

ScriptableObjectAdapter::ScriptableObjectAdapter (ScriptEngine& engine, std::string name) :
    ObjectAdapter (engine), m_exoticMethods (), m_name (std::move (name)) {
    this->registerType (
	{
	    .class_name = m_name.c_str (),
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