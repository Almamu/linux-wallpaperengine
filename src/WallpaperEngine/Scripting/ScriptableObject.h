#pragma once
#include "WallpaperEngine/Data/Model/Types.h"
#include "WallpaperEngine/Render/CObject.h"

#include <map>
#include <memory>
#include <string>

namespace WallpaperEngine::Render::Wallpapers {
class CScene;
}

namespace WallpaperEngine::Scripting {
class ScriptableObject : virtual public CObject {
public:
    struct PropertyEntry {
	std::string key;
	DynamicValue& value;
    };

    ScriptableObject (Wallpapers::CScene& scene, const Object& object);
    virtual ~ScriptableObject () = default;

    DynamicValue& getProperty (const std::string& name);

    const std::map<std::string, PropertyEntry>& getProperties () const;

protected:
    /**
     * Exposes a property to scripts as `thisLayer.<name>`.
     *
     * @param runScript When true and the value carries a script, the script is loaded and
     *        driven by the ScriptEngine. Pass false when the owner runs the script itself.
     */
    void registerProperty (const std::string& name, DynamicValue& value, bool runScript = true);
    /** Exposes a fixed value (e.g. the layer size) to scripts; the object owns the storage */
    void registerConstant (const std::string& name, const DynamicValue& value);

private:
    std::map<std::string, PropertyEntry> m_properties;
    std::map<std::string, std::unique_ptr<DynamicValue>> m_constants;
};
}