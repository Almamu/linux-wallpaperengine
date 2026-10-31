#include <catch2/catch_test_macros.hpp>

#include <glm/vec2.hpp>
#include <optional>
#include <string>

#include "WallpaperEngine/Data/JSON.h"

using WallpaperEngine::Data::JSON::JSON;

namespace {
glm::vec2 paddingOf (const JSON& value) {
    const JSON object = { { "padding", value } };
    return object.optional ("padding", glm::vec2 (0.0f));
}
} // namespace

TEST_CASE ("JSON optional with a default parses valid values") {
    CHECK (paddingOf ("8 9") == glm::vec2 (8.0f, 9.0f));

    const JSON object = { { "name", "clock" }, { "id", 7 } };
    CHECK (object.optional ("name", std::string ("x")) == "clock");
    CHECK (object.optional ("id", 0) == 7);
    CHECK (object.optional ("missing", 3) == 3);
    CHECK (JSON { { "id", nullptr } }.optional ("id", 3) == 3);
}

TEST_CASE ("JSON optional with a default falls back on values it can't convert") {
    // each of these used to call std::terminate (#712)
    CHECK (paddingOf ("32") == glm::vec2 (0.0f));
    CHECK (paddingOf ("8 9 10") == glm::vec2 (0.0f));
    CHECK (paddingOf ("") == glm::vec2 (0.0f));
    CHECK (paddingOf (true) == glm::vec2 (0.0f));
    CHECK (paddingOf (JSON::array ({ 1, 2 })) == glm::vec2 (0.0f));
    CHECK (paddingOf (JSON { { "user", "padding" }, { "value", "8 9" } }) == glm::vec2 (0.0f));

    const JSON object = { { "name", true }, { "id", "seven" } };
    CHECK (object.optional ("name", std::string ("x")) == "x");
    CHECK (object.optional ("id", 3) == 3);
}

TEST_CASE ("JSON optional without a default treats values it can't convert as missing") {
    const JSON object = { { "good", "8 9" }, { "bad", "8 9 10" }, { "flag", true } };

    CHECK (object.optional<glm::vec2> ("good") == std::optional (glm::vec2 (8.0f, 9.0f)));
    CHECK_FALSE (object.optional<glm::vec2> ("bad").has_value ());
    CHECK_FALSE (object.optional<std::string> ("flag").has_value ());
    CHECK_FALSE (object.optional<glm::vec2> ("missing").has_value ());
}
