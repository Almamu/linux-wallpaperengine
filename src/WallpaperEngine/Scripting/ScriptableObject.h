#pragma once
#include "WallpaperEngine/Data/Model/DynamicValue.h"
#include "WallpaperEngine/Data/Model/Types.h"
#include "WallpaperEngine/Render/CObject.h"

namespace WallpaperEngine::Render::Wallpapers {
class CScene;
}

namespace WallpaperEngine::Scripting {
class ScriptableObject : virtual public CObject {
public:
    ScriptableObject (Wallpapers::CScene& scene, const Object& object);
    virtual ~ScriptableObject () = default;
    virtual void reevaluate ();

    DynamicValue& getProperty (const std::string& name);

    const std::map<std::string, DynamicValue&>& getProperties () const;

protected:
    void registerProperty (const std::string& name, DynamicValue& value);

private:
    bool m_evaluating = false;
    ScriptContext m_context;
    std::map<std::string, DynamicValue&> m_properties;
};
}