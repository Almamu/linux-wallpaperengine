#include "ScriptPropertiesObject.h"

#include "Adapters/ScriptableObjectAdapter.h"
#include "EngineObject.h"
#include "ScriptEngine.h"
#include "WallpaperEngine/Data/Utils/ScopeGuard.h"
#include "WallpaperEngine/Render/Wallpapers/CScene.h"

using namespace WallpaperEngine::Scripting;

static uint32_t ScriptPropertiesObjectInstanceId = 0;
std::map<uint32_t, ScriptPropertiesObject&> scriptPropertiesObjectInstances;

struct OpaqueScriptPropertiesInstance {
    ScriptPropertiesObject& object;
    DynamicValue& value;
};

struct OpaqueScriptProperties {
    ScriptPropertiesObject& object;
};

JSValue scriptproperties_property_get (JSContext* ctx, JSValueConst obj_val, JSAtom atom, JSValueConst receiver) {
    JSClassID classId = 0;

    auto* container = static_cast<OpaqueScriptPropertiesInstance*> (JS_GetAnyOpaque (obj_val, &classId));

    const char* name = JS_AtomToCString (ctx, atom);

    if (name == nullptr) {
	return JS_EXCEPTION;
    }

    ScopeGuard guard ([=] { JS_FreeCString (ctx, name); });

    try {
	auto& properties = container->value.getProperties ();
	const auto it = properties.find (name);

	if (it == properties.end ()) {
	    return JS_UNDEFINED;
	}

	return container->object.getEngine ().dynamicToJs (*it->second->value);
    } catch (const std::exception& e) {
	return JS_EXCEPTION;
    }
}

int scriptproperties_property_set (
    JSContext* ctx, JSValueConst obj_val, JSAtom atom, JSValueConst val, JSValueConst receiver, int flags
) {
    // do not support setting properties
    return -1;
}

JSValue scriptpropertiescreator_add (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    // no need to do anything, any add call should just return itself
    // we'll set them either way as what comes in the DynamicValue
    // TODO: PROPERLY IMPLEMENT THIS CHAIN AT SOME POINT
    return this_val;
}

JSValue scriptpropertiescreator_finish (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    JSClassID classId = 0;
    const auto container = static_cast<OpaqueScriptProperties*> (JS_GetAnyOpaque (this_val, &classId));

    // get all the properties and set the right values
    const auto* module = container->object.getEngine ().getRunningModule ();

    if (module == nullptr) {
	return JS_UNDEFINED;
    }

    // create a new object based off the properties in the dynamic value and call it a day
    JSValue result = JS_NewObjectClass (ctx, container->object.getPropertiesClassId ());
    JS_SetOpaque (
	result,
	new OpaqueScriptPropertiesInstance { .object = container->object,
					     .value = container->object.getEngine ().getRunningModule ()->value }
    );

    return result;
}

void scriptpropertiescreator_finalizer (JSRuntime* rt, JSValueConst val) {
    JSClassID classId = 0;

    delete static_cast<OpaqueScriptProperties*> (JS_GetAnyOpaque (val, &classId));
}

void scriptproperties_finalizer (JSRuntime* rt, JSValueConst val) {
    JSClassID classId = 0;

    delete static_cast<OpaqueScriptPropertiesInstance*> (JS_GetAnyOpaque (val, &classId));
}

JSValue
scriptpropertiescreator_create (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv, int magic) {
    const auto instance = scriptPropertiesObjectInstances.find (magic);

    if (instance == scriptPropertiesObjectInstances.end ()) {
	return JS_UNDEFINED;
    }

    JSValue creator = JS_NewObjectClass (ctx, instance->second.getCreatorClassId ());

    // setup the current script properties creator
    JS_SetOpaque (creator, new OpaqueScriptProperties { .object = instance->second });

    return creator;
}

ScriptPropertiesObject::ScriptPropertiesObject (ScriptEngine& engine, Render::Wallpapers::CScene& scene) :
    m_scene (scene), m_engine (engine), m_instanceId (++ScriptPropertiesObjectInstanceId), m_creatorClassId (0),
    m_propertiesClassId (0) {
    scriptPropertiesObjectInstances.emplace (this->m_instanceId, *this);

    this->m_exoticMethods = {
	.get_property = scriptproperties_property_get,
	.set_property = scriptproperties_property_set,
    };
    this->m_creatorDefinition = {
	.class_name = "IScriptPropertiesCreator",
	.finalizer = scriptpropertiescreator_finalizer,
    };
    JS_NewClassID (this->m_engine.getRuntime (), &this->m_creatorClassId);
    JS_NewClass (this->m_engine.getRuntime (), this->m_creatorClassId, &this->m_creatorDefinition);
    this->m_creatorPrototype = JS_NewObject (this->m_engine.getContext ());

    this->m_propertiesDefinition = {
	.class_name = "IScriptProperties",
	.finalizer = scriptproperties_finalizer,
	.exotic = &this->m_exoticMethods,
    };
    JS_NewClassID (this->m_engine.getRuntime (), &this->m_propertiesClassId);
    JS_NewClass (this->m_engine.getRuntime (), this->m_propertiesClassId, &this->m_propertiesDefinition);
    this->m_propertiesPrototype = JS_NewObject (this->m_engine.getContext ());

    JS_DupValue (this->m_engine.getContext (), this->m_propertiesPrototype);
    JS_DupValue (this->m_engine.getContext (), this->m_creatorPrototype);

    // set properties
    JS_DefinePropertyValueStr (
	this->m_engine.getContext (), this->m_creatorPrototype, "addSlider",
	JS_NewCFunction (this->m_engine.getContext (), scriptpropertiescreator_add, "addSlider", 0), JS_PROP_ENUMERABLE
    );
    JS_DefinePropertyValueStr (
	this->m_engine.getContext (), this->m_creatorPrototype, "addCheckbox",
	JS_NewCFunction (this->m_engine.getContext (), scriptpropertiescreator_add, "addCheckbox", 0),
	JS_PROP_ENUMERABLE
    );
    JS_DefinePropertyValueStr (
	this->m_engine.getContext (), this->m_creatorPrototype, "addText",
	JS_NewCFunction (this->m_engine.getContext (), scriptpropertiescreator_add, "addText", 0), JS_PROP_ENUMERABLE
    );
    JS_DefinePropertyValueStr (
	this->m_engine.getContext (), this->m_creatorPrototype, "addCombo",
	JS_NewCFunction (this->m_engine.getContext (), scriptpropertiescreator_add, "addCombo", 0), JS_PROP_ENUMERABLE
    );
    JS_DefinePropertyValueStr (
	this->m_engine.getContext (), this->m_creatorPrototype, "addColor",
	JS_NewCFunction (this->m_engine.getContext (), scriptpropertiescreator_add, "addColor", 0), JS_PROP_ENUMERABLE
    );
    JS_DefinePropertyValueStr (
	this->m_engine.getContext (), this->m_creatorPrototype, "finish",
	JS_NewCFunction (this->m_engine.getContext (), scriptpropertiescreator_finish, "finish", 0), JS_PROP_ENUMERABLE
    );
    JS_DefinePropertyValueStr (
	this->m_engine.getContext (), this->m_engine.getGlobalThis (), "createScriptProperties",
	JS_NewCFunctionMagic (
	    this->m_engine.getContext (), scriptpropertiescreator_create, "createScriptProperties", 0, JS_CFUNC_generic,
	    m_instanceId
	),
	JS_PROP_ENUMERABLE
    );

    JS_SetClassProto (this->m_engine.getContext (), this->m_propertiesClassId, this->m_propertiesPrototype);
    JS_SetClassProto (this->m_engine.getContext (), this->m_creatorClassId, this->m_creatorPrototype);
}

ScriptPropertiesObject::~ScriptPropertiesObject () {
    JS_FreeValue (this->m_engine.getContext (), this->m_creatorPrototype);
    JS_FreeValue (this->m_engine.getContext (), this->m_propertiesPrototype);
}