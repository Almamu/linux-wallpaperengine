#pragma once

#include <map>
#include <optional>
#include <string>

#include "DynamicValue.h"
#include "Types.h"

namespace WallpaperEngine::Render::Wallpapers {
class CScene;
}

namespace WallpaperEngine::Data::Model {

struct ScriptBindingContext {
    int objectId = -1;
    std::string objectName;
    std::string propertyName;
};

/**
 * A DynamicValue whose value is computed by evaluating a WallpaperEngine script.
 *
 * Holds the script source, a map of script property names to their DynamicValue
 * pointers (connected to user Properties), and the base value from the JSON.
 * When any connected scriptProperty changes, re-evaluates the script.
 */
class ScriptedDynamicValue : public DynamicValue {
public:
    ScriptedDynamicValue (
	std::string scriptSource,
	std::map<std::string, DynamicValueUniquePtr> scriptProps,
	DynamicValue baseValue
    );

    ~ScriptedDynamicValue () override;

    void setBindingContext (ScriptBindingContext context);
    [[nodiscard]] const std::optional<ScriptBindingContext>& getBindingContext () const;
    void reevaluate (WallpaperEngine::Render::Wallpapers::CScene* scene);

private:
    void reevaluate ();

    std::string m_scriptSource;
    std::map<std::string, DynamicValueUniquePtr> m_scriptProps;
    DynamicValue m_baseValue;
    std::optional<ScriptBindingContext> m_bindingContext = std::nullopt;
    bool m_evaluating = false;
};
} // namespace WallpaperEngine::Data::Model
