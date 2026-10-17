#pragma once
#include "WallpaperEngine/Data/Model/Types.h"
#include "WallpaperEngine/Render/CObject.h"

#include <optional>
#include <string>

namespace WallpaperEngine::Data::Model {
struct Material;
}

namespace WallpaperEngine::Render::Wallpapers {
class CScene;
}

namespace WallpaperEngine::Scripting {
class ScriptableObject : virtual public CObject {
public:
    struct AnimationLayerProperties {
	DynamicValue* rate;
	DynamicValue* visible;
    };

    struct PropertyEntry {
	std::string key;
	DynamicValue& value;
    };

    ScriptableObject (Wallpapers::CScene& scene, const Object& object);
    virtual ~ScriptableObject () = default;

    DynamicValue& getProperty (const std::string& name);

    /** Return the controls for a named puppet animation layer, when supported. */
    virtual std::optional<AnimationLayerProperties> findAnimationLayer (const std::string& name) const;

    const std::map<std::string, PropertyEntry>& getProperties () const;

protected:
    void registerProperty (const std::string& name, DynamicValue& value);
    void registerMaterialProperties (const std::string& prefix, const Data::Model::Material& material);

private:
    std::map<std::string, PropertyEntry> m_properties;
};
}
