#pragma once
#include "WallpaperEngine/Data/Model/Types.h"
#include "WallpaperEngine/Render/CObject.h"

namespace WallpaperEngine::Render::Wallpapers {
class CScene;
}

namespace WallpaperEngine::Scripting {
class CScriptableObject : virtual public CObject {
public:
    CScriptableObject (Wallpapers::CScene& scene, const Object& object);
    virtual ~CScriptableObject () = default;
    virtual void reevaluate ();

protected:
    void registerProperty (const std::string& name, DynamicValue& value);

private:
    bool m_evaluating = false;
    ScriptContext m_context;
    std::map<std::string, DynamicValue&> m_properties;
};
}