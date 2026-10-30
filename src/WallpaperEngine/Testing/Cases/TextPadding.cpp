#include <catch2/catch_test_macros.hpp>

#include "WallpaperEngine/Data/Model/Object.h"
#include "WallpaperEngine/Data/Model/Project.h"
#include "WallpaperEngine/Data/Model/Wallpaper.h"
#include "WallpaperEngine/Data/Parsers/ObjectParser.h"

// Regression tests for text object padding (#652, #703, #710).
// Originally shared by ziggityzack on PR #652:
// https://github.com/Almamu/linux-wallpaperengine/pull/652#issuecomment-6434468457

using WallpaperEngine::Data::JSON::JSON;
using WallpaperEngine::Data::Model::Project;
using WallpaperEngine::Data::Model::Text;
using WallpaperEngine::Data::Parsers::ObjectParser;

namespace {
// A text object shaped like the "Date Sans Serif" object from the scene in #703.
JSON textObject () {
    return {
	{ "id", 395 },
	{ "name", "Date Sans Serif" },
	{ "text", { { "value", "" } } },
	{ "font", "systemfont_sansserif" },
	{ "horizontalalign", "center" },
	{ "origin", "2560.00000 1354.19055 0.00000" },
	{ "pointsize", 18.0 },
    };
}

// Parses a whole text object through ObjectParser::parse and returns its padding.
glm::vec2 parsePaddingOf (const JSON& object) {
    const Project project {};
    const auto parsed = ObjectParser::parse (object, project);

    REQUIRE (parsed != nullptr);
    REQUIRE (parsed->is<Text> ());

    // glm::vec2 (...) also accepts the scalar padding used before #652, so this file
    // builds against the pre-fix code to show the original failure.
    return glm::vec2 (parsed->as<Text> ()->padding);
}

glm::vec2 parsePadding (const JSON& padding) {
    auto object = textObject ();
    object["padding"] = padding;
    return parsePaddingOf (object);
}
} // namespace

// Formats seen in real Workshop scenes.

TEST_CASE ("Text padding: two-component strings", "[text][padding]") {
    // Somewhere In Nowhere [4K] (3819637149) and DJgun雨夜 (3804689861) both aborted on load before #652.
    CHECK (parsePadding ("32.00000 32.00000") == glm::vec2 (32.0f, 32.0f));
    CHECK (parsePadding ("12.00000 12.00000") == glm::vec2 (12.0f, 12.0f));

    const auto unequal = parsePadding ("12.5 4");
    CHECK (unequal.x == 12.5f);
    CHECK (unequal.y == 4.0f);
}

TEST_CASE ("Text padding: integer scalar applies to both axes", "[text][padding]") {
    // Control Resonant (3627075303) uses an integer scalar.
    CHECK (parsePadding (55) == glm::vec2 (55.0f, 55.0f));
    CHECK (parsePadding (0) == glm::vec2 (0.0f, 0.0f));
}

TEST_CASE ("Text padding: fractional scalar applies to both axes", "[text][padding]") {
    const auto fractional = parsePadding (32.75);
    CHECK (fractional.x == 32.75f);
    CHECK (fractional.y == 32.75f);
}

TEST_CASE ("Text padding: missing defaults to 0", "[text][padding]") {
    CHECK (parsePaddingOf (textObject ()) == glm::vec2 (0.0f));
}

TEST_CASE ("Text padding: null defaults to 0", "[text][padding]") {
    CHECK (parsePadding (nullptr) == glm::vec2 (0.0f));
}

// Malformed input (not seen in real scenes).
//
// These are hidden ("[.]"), so the default test run and CI skip them. They also leave
// out the [text] and [padding] tags, because Catch2 runs hidden tests that match a tag
// filter, so `tests "[padding]"` would otherwise hit them. Today each one
// ends the process with std::terminate: JsonExtensions::optional (key, default) is
// noexcept, so the exception thrown by VectorBuilder::parse or get<std::string> can't
// leave it, and CHECK_NOTHROW can't observe it. Removing noexcept alone would not be
// enough: the try/catch in ObjectParser::parse only covers the base object data, and
// parseText runs after it, so nothing would catch the exception further up either.
//
// The expectations below describe the proposed behaviour (fall back to the default
// padding) and are tracked in #712. Two cases are still open there: "32" could mean
// (32, 32) like the scalar 32 does, and a user-bound object could use its value.
// Run them one at a time, since each aborts today:
//   tests "Text padding hardening: boolean"

TEST_CASE ("Text padding hardening: single-component string", "[.][hardening]") {
    CHECK (parsePadding ("32") == glm::vec2 (0.0f));
}

TEST_CASE ("Text padding hardening: three-component string", "[.][hardening]") {
    CHECK (parsePadding ("8 9 10") == glm::vec2 (0.0f));
}

TEST_CASE ("Text padding hardening: empty string", "[.][hardening]") { CHECK (parsePadding ("") == glm::vec2 (0.0f)); }

TEST_CASE ("Text padding hardening: boolean", "[.][hardening]") { CHECK (parsePadding (true) == glm::vec2 (0.0f)); }

TEST_CASE ("Text padding hardening: array", "[.][hardening]") {
    CHECK (parsePadding (JSON::array ({ 1, 2 })) == glm::vec2 (0.0f));
}

TEST_CASE ("Text padding hardening: user-bound object", "[.][hardening]") {
    CHECK (parsePadding (JSON { { "user", "padding" }, { "value", 4 } }) == glm::vec2 (0.0f));
}
