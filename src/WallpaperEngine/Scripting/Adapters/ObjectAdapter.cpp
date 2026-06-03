#include "ObjectAdapter.h"

#include "WallpaperEngine/Scripting/ScriptEngine.h"

using namespace WallpaperEngine::Data::Model;
using namespace WallpaperEngine::Scripting::Adapters;

ObjectAdapter::ObjectAdapter (ScriptEngine& engine) :
    m_engine (engine), m_classId (JS_INVALID_CLASS_ID), m_definition () { }

JSValue ObjectAdapter::instantiate (ScriptableObject& object) {
    JSValue result = JS_NewObjectClass (this->m_engine.getContext (), this->m_classId);
    JS_SetOpaque (result, &object);

    return result;
}

JSValue ObjectAdapter::instantiate (DynamicValue& value) {
    JSValue result = JS_NewObjectClass (this->m_engine.getContext (), this->m_classId);
    JS_SetOpaque (result, &value);

    return result;
}

void ObjectAdapter::registerType (const JSClassDef& definition) {
    this->m_definition = definition;

    JS_NewClassID (this->m_engine.getRuntime (), &this->m_classId);
    JS_NewClass (this->m_engine.getRuntime (), this->m_classId, &definition);
}