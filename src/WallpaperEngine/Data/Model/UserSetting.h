#pragma once

#include <memory>
#include <optional>
#include <type_traits>

#include "ConditionInfo.h"
#include "PropertyAnimation.h"
#include "Types.h"

namespace WallpaperEngine::Data::Model {
struct UserSetting {
    // Define deletion where DynamicValue is complete so UserSetting can stay an aggregate.
    struct ValueDeleter : std::default_delete<DynamicValue> {
	ValueDeleter () = default;
	template <typename T> requires std::is_convertible_v<T*, DynamicValue*>
	ValueDeleter (const std::default_delete<T>&) noexcept { }
	void operator() (DynamicValue* value) const noexcept;
    };

    /**
     * The value of this setting, can be a few different things:
     * - a value connected to the property
     * - a default value
     * - a static value
     */
    std::unique_ptr<DynamicValue, ValueDeleter> value;
    /** The property this setting takes the value from (if specified) */
    PropertySharedPtr property;
    /** Condition required for this setting, this should be possible to run in JS' V8 */
    std::optional<ConditionInfo> condition;
    /** Keyframed animation for this setting (e.g. an origin moving across the scene), if any */
    PropertyAnimationUniquePtr animation;
    /** TODO: Value might come from a script and not have conditions, implement this later */

    /**
     * Resolve the current scalar value, including an authored keyframe animation.
     *
     * Wallpaper Engine attaches animations to the property itself, so consumers
     * must sample the setting instead of reading DynamicValue directly.
     */
    [[nodiscard]] float evaluateFloat (double time) const;

    /**
     * Resolve the current vector value, including absolute or relative keyframes.
     */
    [[nodiscard]] glm::vec3 evaluateVec3 (double time) const;
};
} // namespace WallpaperEngine::Data::Model
