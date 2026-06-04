#include "ScriptPropertiesObject.h"

#include "EngineObject.h"
#include "ScriptEngine.h"
#include "WallpaperEngine/Data/Utils/ScopeGuard.h"
#include "WallpaperEngine/Render/Wallpapers/CScene.h"

using namespace WallpaperEngine::Scripting;

JSValue scriptproperties_add (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    // no need to do anything, any add call should just return itself
    // we'll set them either way as what comes in the DynamicValue
    // TODO: PROPERLY IMPLEMENT THIS CHAIN AT SOME POINT
    return this_val;
}

JSValue scriptproperties_finish (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    return JS_UNDEFINED;
}

ScriptPropertiesObject::ScriptPropertiesObject (ScriptEngine& engine, Render::Wallpapers::CScene& scene) :
    m_scene (scene), m_engine (engine) {
    this->m_creatorDefinition = { .class_name = "IScriptPropertiesCreator" };
    JS_NewClassID (this->m_engine.getRuntime (), &this->m_creatorClassId);
    JS_NewClass (this->m_engine.getRuntime (), this->m_creatorClassId, &this->m_creatorDefinition);
    this->m_creatorInstance = JS_NewObjectClass (this->m_engine.getContext (), this->m_creatorClassId);

    this->m_propertiesDefinition = { .class_name = "IScriptProperties" };
    JS_NewClassID (this->m_engine.getRuntime (), &this->m_propertiesClassId);
    JS_NewClass (this->m_engine.getRuntime (), this->m_propertiesClassId, &this->m_propertiesDefinition);
    this->m_propertiesInstance = JS_NewObjectClass (this->m_engine.getContext (), this->m_propertiesClassId);

    JS_DupValue (this->m_engine.getContext (), this->m_creatorInstance);
    JS_DupValue (this->m_engine.getContext (), this->m_propertiesInstance);

    // set properties
    JS_SetOpaque (this->m_creatorInstance, this);
    JS_DefinePropertyValueStr (
	this->m_engine.getContext (), this->m_creatorInstance, "addSlider",
	JS_NewCFunction (this->m_engine.getContext (), scriptproperties_add, "addSlider", 0), JS_PROP_ENUMERABLE
    );
    JS_DefinePropertyValueStr (
	this->m_engine.getContext (), this->m_creatorInstance, "addCheckbox",
	JS_NewCFunction (this->m_engine.getContext (), scriptproperties_add, "addCheckbox", 0), JS_PROP_ENUMERABLE
    );
    JS_DefinePropertyValueStr (
	this->m_engine.getContext (), this->m_creatorInstance, "addText",
	JS_NewCFunction (this->m_engine.getContext (), scriptproperties_add, "addText", 0), JS_PROP_ENUMERABLE
    );
    JS_DefinePropertyValueStr (
	this->m_engine.getContext (), this->m_creatorInstance, "addCombo",
	JS_NewCFunction (this->m_engine.getContext (), scriptproperties_add, "addCombo", 0), JS_PROP_ENUMERABLE
    );
    JS_DefinePropertyValueStr (
	this->m_engine.getContext (), this->m_creatorInstance, "addColor",
	JS_NewCFunction (this->m_engine.getContext (), scriptproperties_add, "addColor", 0), JS_PROP_ENUMERABLE
    );
    JS_DefinePropertyValueStr (
	this->m_engine.getContext (), this->m_creatorInstance, "finish",
	JS_NewCFunction (this->m_engine.getContext (), scriptproperties_finish, "finish", 0), JS_PROP_ENUMERABLE
    );
}

ScriptPropertiesObject::~ScriptPropertiesObject () {
    JS_FreeValue (this->m_engine.getContext (), this->m_creatorInstance);
    JS_FreeValue (this->m_engine.getContext (), this->m_propertiesInstance);
}