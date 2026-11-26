#pragma once
#include "quickjs.h"
#include "WallpaperEngine/Data/Model/Types.h"
#include <string>
#include <utility>
#include <vector>

namespace WallpaperEngine::Render::Wallpapers {
class CScene;
}
namespace WallpaperEngine::Scripting {
class ScriptEngine;
class SceneObject {
public:
    SceneObject (ScriptEngine& engine, Render::Wallpapers::CScene& scene);
    ~SceneObject ();

    const Render::Wallpapers::CScene& getScene () const { return m_scene; }
    Render::Wallpapers::CScene& getScene () { return m_scene; }
    JSValue getInstance () const { return m_instance; }
    ScriptEngine& getEngine () const { return m_engine; }
    JSValue makeModelDataHandle (uint32_t token);
    void registerPropertyAnimation (const std::string& property, const Data::Model::UserSetting& setting);
    Data::Model::PropertyAnimation* findPropertyAnimation (const std::string& name) const;
    JSValue makePropertyOwner (const std::string& property);

private:
    Render::Wallpapers::CScene& m_scene;
    ScriptEngine& m_engine;

    JSClassID m_classId;
    JSClassID m_modelDataClassId = 0;
    JSClassDef m_definition;
    JSValue m_instance;
    std::vector<std::pair<std::string, const Data::Model::UserSetting*>> m_propertyAnimations;
};
}