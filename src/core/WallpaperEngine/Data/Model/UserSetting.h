#pragma once

#include <memory>
#include <optional>

#include "DynamicValue.h"
#include "Types.h"

namespace WallpaperEngine::Data::Model {
struct UserSetting {
    /**
     * The value of this setting, can be a few different things:
     * - a value connected to the property
     * - a default value
     * - a static value
     */
    DynamicValueUniquePtr value;
    /** The property this setting takes the value from (if specified) */
    PropertySharedPtr property;
    /** Condition required for this setting, this should be possible to run in JS' V8 */
    std::optional<ConditionInfo> condition;
    /** TODO: Value might come from a script and not have conditions, implement this later */
};
} // namespace WallpaperEngine::Data::Model