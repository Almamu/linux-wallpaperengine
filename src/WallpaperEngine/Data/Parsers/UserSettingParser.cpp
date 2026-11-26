#include "UserSettingParser.h"
#include "DynamicValueParser.h"

#include "WallpaperEngine/Data/Model/DynamicValue.h"
#include "WallpaperEngine/Data/Model/Property.h"
#include "WallpaperEngine/Data/Model/PropertyAnimation.h"
#include "WallpaperEngine/Data/Model/UserSetting.h"

#include <algorithm>

using namespace WallpaperEngine::Data::Parsers;
using namespace WallpaperEngine::Data::Builders;

namespace {
PropertyKeyframeHandle parseAnimationHandle (const json& keyframe, const std::string& name) {
    const auto handle = keyframe.optional (name);
    if (!handle.has_value () || !handle->is_object ()) {
	return {};
    }

    return PropertyKeyframeHandle {
	.enabled = handle->optional ("enabled", false),
	.offset = glm::vec2 (handle->optional ("x", 0.0f), handle->optional ("y", 0.0f)),
    };
}

PropertyAnimationUniquePtr parseAnimation (const json& data) {
    const auto animationIt = data.optional ("animation");

    if (!animationIt.has_value () || !animationIt->is_object ()) {
        return nullptr;
    }

    auto animation = std::make_unique<PropertyAnimation> ();
    const auto& it = *animationIt;
    const auto options = it.optional ("options");

    animation->fps = options.has_value () ? options->optional ("fps", 1.0f) : 1.0f;
    animation->length = options.has_value () ? options->optional ("length", 0.0f) : 0.0f;
    animation->mode = options.has_value () ? options->optional<std::string> ("mode", "loop") : "loop";
    animation->relative = it.optional ("relative", false);
    if (options.has_value ()) {
	animation->name = options->optional<std::string> ("name", "");
	animation->playing = !options->optional ("startpaused", false);
	if (const auto parent = options->optional ("parent"); parent.has_value () && parent->is_object ()) {
	    animation->parentKey = parent->optional<std::string> ("key", "");
	}
	if (const auto events = options->optional ("events"); events.has_value () && events->is_array ()) {
	    for (const auto& event : *events) {
		animation->events.push_back ({ event.optional ("frame", 0.0f), event.optional<std::string> ("name", "") });
	    }
	}
    }
    const bool wrapLoop = options.has_value () && options->optional ("wraploop", false);

    // channels are stored as c0/c1/c2... keys mapping to keyframe arrays
    for (const auto& [key, channelData] : it.items ()) {
        if (key.size () < 2 || key [0] != 'c' || !channelData.is_array ()) {
            continue;
        }

        int channel;
        try {
            channel = std::stoi (key.substr (1));
        } catch (const std::exception&) {
            continue;
        }

        auto& keyframes = animation->channels [channel];

	for (const auto& keyframeData : channelData) {
	    keyframes.push_back (PropertyKeyframe {
		.frame = keyframeData.optional ("frame", 0.0f),
		.value = keyframeData.optional ("value", 0.0f),
		.incoming = parseAnimationHandle (keyframeData, "back"),
		.outgoing = parseAnimationHandle (keyframeData, "front"),
	    });
	}
	std::ranges::sort (keyframes, {}, &PropertyKeyframe::frame);
	// The native loader closes wraploop curves before playback. Holding the
	// last authored key until fmod wraps causes a pause followed by a hard cut.
	if (wrapLoop && animation->length > 0.0f) {
	    while (keyframes.size () > 1 && keyframes.back ().frame > animation->length) {
		keyframes.pop_back ();
	    }
	    if (keyframes.size () >= 2) {
		const auto first = keyframes.front ();
		if (keyframes.back ().frame != animation->length) {
		    keyframes.push_back (PropertyKeyframe { .frame = animation->length, .value = first.value });
		}
		auto& closing = keyframes.back ();
		closing.value = first.value;
		closing.incoming = first.outgoing.enabled
		    ? PropertyKeyframeHandle { .enabled = true, .offset = -first.outgoing.offset }
		    : PropertyKeyframeHandle {};
	    }
	}
    }

    return animation;
}
} // namespace

UserSettingUniquePtr UserSettingParser::parse (const json& data, const Properties& properties, bool expectColor) {
    auto value = DynamicValueParser::parse (data, properties, expectColor);
    PropertySharedPtr property;
    std::optional<ConditionInfo> condition = std::nullopt;
    PropertyAnimationUniquePtr animation = data.is_object () ? parseAnimation (data) : nullptr;

    if (data.is_object ()) {
	const auto user = data.optional ("user");

	if (user.has_value () && !user->is_null ()) {
	    std::string source;

	    if (const auto& it = *user; it.is_string ()) {
		source = it;
	    } else {
		condition = ConditionInfo {
		    .name = it.require<std::string> ("name", "Name for conditional setting must be present"),
		    .condition
		    = it.require<std::string> ("condition", "Condition for conditional setting must be present"),
		};

		source = condition.value ().name;
	    }

	    if (const auto propertyIt = properties.find (source); propertyIt != properties.end ()) {
		property = propertyIt->second;
	    }
	}
    }

    // TODO: This might need to be removed if it causes issues with default values
    // Connect to property if one is specified (this allows property overrides to propagate)
    if (property != nullptr) {
	if (condition.has_value ()) {
	    value->attachCondition (condition.value ());
	}

	value->connect (property.get ());
    }

    return std::make_unique<UserSetting> (UserSetting {
	.value = std::move (value),
	.property = property,
	.condition = condition,
	.animation = std::move (animation),
    });
}
