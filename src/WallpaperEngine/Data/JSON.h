#pragma once

#include <glm/detail/qualifier.hpp>
#include <glm/detail/type_vec1.hpp>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "WallpaperEngine/Data/Builders/UserSettingBuilder.h"
#include "WallpaperEngine/Data/Builders/VectorBuilder.h"
#include "WallpaperEngine/Data/Model/Types.h"
#include "WallpaperEngine/Data/Utils/JsonTelemetry.h"
#include "WallpaperEngine/Data/Utils/SFINAE.h"
#include "WallpaperEngine/Logging/Log.h"

namespace WallpaperEngine::Data::JSON {
using namespace WallpaperEngine::Data::Builders;
using namespace WallpaperEngine::Data::Model;
using namespace WallpaperEngine::Data::Utils::SFINAE;

class JsonExtensions;

using JSON = nlohmann::basic_json<
    std::map, std::vector, std::string, bool, std::int64_t, std::uint64_t, double, std::allocator,
    nlohmann::adl_serializer, std::vector<std::uint8_t>, JsonExtensions>;

/**
 * Parses Wallpaper Engine-authored JSON. The editor's reader accepts line and
 * block comments as well as trailing commas before array/object closes. Strict
 * JSON is attempted first, then those compatibility forms are sanitized while
 * preserving source positions for useful parse errors.
 */
[[nodiscard]] JSON parseCompatible (std::string_view contents, std::string_view source = {});

/**
 * Small extensions class that is used as base class of nlohmann's implementation.
 *
 * Provides shorthand methods to reduce the amount of handling required for specific situations
 * (mainly throwing readable exceptions when a value is missing, optional/default values, user settings...)
 */
class JsonExtensions {
public:
    using base_type = JSON;

    template <typename T, typename std::enable_if_t<is_glm_vec<T>::value, int> = 0> [[nodiscard]] T get () const {
	constexpr int length = GlmVecTraits<T>::length;
	constexpr glm::qualifier qualifier = GlmVecTraits<T>::qualifier;

	// call the specialized version of the function
	return get<length, typename GlmVecTraits<T>::type, qualifier> ();
    }
    template <int length, typename type, glm::qualifier qualifier>
    [[nodiscard]] glm::vec<length, type, qualifier> get () const {
	return VectorBuilder::parse<length, type, qualifier> (this->base ().get<std::string> ());
    }
    [[nodiscard]] Model::Color get () const;
    [[nodiscard]] base_type require (const std::string& key, const std::string& message) const {
	Utils::JsonTelemetry::recordAccess (key);
	auto base = this->base ();
	const auto it = base.find (key);

	if (it == base.end ()) {
	    sLog.exception (message, ". Contents: ", base.dump ());
	}

	return *it;
    }
    template <typename T> [[nodiscard]] T require (const std::string& key, const std::string& message) const {
	Utils::JsonTelemetry::recordAccess (key);
	auto base = this->base ();
	const auto it = base.find (key);

	if (it == base.end ()) {
	    sLog.exception (message, ". Contents: ", base.dump ());
	}

	return (*it);
    }
    [[nodiscard]] std::optional<base_type> optional (const std::string& key) const noexcept {
	Utils::JsonTelemetry::recordAccess (key);
	auto base = this->base ();
	const auto it = base.find (key);
	auto result = std::optional<base_type> {};

	if (it != base.end () && !it->is_null ()) {
	    result.emplace (*it);
	}

	return result;
    }
    template <typename T> [[nodiscard]] std::optional<T> optional (const std::string& key) const {
	Utils::JsonTelemetry::recordAccess (key);
	auto base = this->base ();
	const auto it = base.find (key);

	if (it == base.end () || it->is_null ()) {
	    return std::nullopt;
	}

	// Wallpaper Engine (JsonCpp-based) tolerates authored type drift — e.g. newer editors write a
	// text layer's "padding" as a "32.00000 32.00000" vector while older scenes store a number
	// (workshop 3758354038 authors exactly that on every text object). A mismatched optional has
	// to degrade to "not set" instead of crashing: these methods were marked noexcept while the
	// json conversion can throw, so the type_error became std::terminate and killed the engine.
	try {
	    return static_cast<T> (*it);
	} catch (const std::exception& e) {
	    sLog.error ("Ignoring optional value '", key, "' of mismatched type: ", e.what ());
	    return std::nullopt;
	}
    }
    template <typename T> [[nodiscard]] T optional (const std::string& key, T defaultValue) const {
	Utils::JsonTelemetry::recordAccess (key);
	auto base = this->base ();
	const auto it = base.find (key);

	if (it == base.end () || it->is_null ()) {
	    return defaultValue;
	}

	// see optional(key) above: tolerate authored type drift like Wallpaper Engine does
	try {
	    return static_cast<T> (*it);
	} catch (const std::exception& e) {
	    sLog.error ("Ignoring optional value '", key, "' of mismatched type: ", e.what ());
	    return defaultValue;
	}
    }
    [[nodiscard]] UserSettingUniquePtr user (const std::string& key, const Properties& properties) const;
    template <typename T>
    [[nodiscard]] UserSettingUniquePtr
    user (const std::string& key, const Properties& properties, T defaultValue) const {
	static_assert (std::is_same_v<T, Color> == false, "Use color() for color properties");
	const auto value = this->optional (key);

	if (!value.has_value ()) {
	    return UserSettingBuilder::fromValue<T> (defaultValue);
	}

	// performs a second lookup, but handles the actual call to UserSettingParser outside of this header
	// this resolving the include loop
	return this->user (key, properties);
    }
    [[nodiscard]] UserSettingUniquePtr color (const std::string& key, const Properties& properties) const;
    [[nodiscard]] UserSettingUniquePtr
    color (const std::string& key, const Properties& properties, Color defaultValue) const;

    template <int length, typename type, glm::qualifier qualifier> operator glm::vec<length, type, qualifier> () const {
	return get<length, type, qualifier> ();
    }
    template <typename T, typename std::enable_if_t<is_glm_vec<T>::value> = 0> operator T () const {
	constexpr int length = GlmVecTraits<T>::length;
	constexpr glm::qualifier qualifier = GlmVecTraits<T>::qualifier;

	// call the specialized version of the function
	return operator glm::vec<length, typename GlmVecTraits<T>::type, qualifier> ();
    }

private:
    /**
     * @return The base json object to be used by the extension methods
     */
    [[nodiscard]] const base_type& base () const { return *static_cast<const base_type*> (this); }
};

} // namespace WallpaperEngine::Data::JSON
