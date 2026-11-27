#include <catch2/catch_test_macros.hpp>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <utility>

#include "WallpaperEngine/Data/Model/DynamicValue.h"
#include "WallpaperEngine/Data/Model/Project.h"
#include "WallpaperEngine/Data/Parsers/ObjectParser.h"
#include "WallpaperEngine/Render/Objects/CText.h"

using WallpaperEngine::Render::Objects::blitTextBitmap;
using WallpaperEngine::Render::Objects::computeTextAlignmentOffset;
using WallpaperEngine::Render::Objects::computeTextEffectLayout;
using WallpaperEngine::Render::Objects::layoutTextLines;
using WallpaperEngine::Render::Objects::nextUtf8Codepoint;
using WallpaperEngine::Render::Objects::TextGlyphMetrics;

TEST_CASE ("classic text coverage honors bitmap pitch and monochrome packing", "[text][bitmap]") {
    unsigned char gray[] = { 24, 128, 255, 0, 255, 77 };
    FT_Bitmap bitmap { .rows = 2, .width = 2, .pitch = -3, .buffer = gray, .pixel_mode = FT_PIXEL_MODE_GRAY };
    std::vector<uint8_t> coverage { 40, 30, 0, 64 };
    blitTextBitmap (bitmap, { 0, 0 }, { 2, 2 }, coverage);
    CHECK (coverage == std::vector<uint8_t> { 40, 255, 24, 160 });

    unsigned char mono[] = { 0x81, 0x80, 0xFF };
    bitmap = { .rows = 1, .width = 9, .pitch = 3, .buffer = mono, .pixel_mode = FT_PIXEL_MODE_MONO };
    coverage.assign (8, 32);
    blitTextBitmap (bitmap, { -1, 0 }, { 8, 1 }, coverage);
    CHECK (coverage == std::vector<uint8_t> { 32, 32, 32, 32, 32, 32, 255, 255 });
}

TEST_CASE ("text spacing defaults to zero and preserves signed fractional pixels", "[text][spacing]") {
    const WallpaperEngine::Data::Model::Project project {};
    for (const auto& [source, expected] : std::vector<std::pair<const char*, glm::vec2>> {
	     { R"({"id":1,"name":"text","text":"AA"})", glm::vec2 (0.0f) },
	     { R"({"id":1,"name":"text","text":"AA","spacing":"0 0"})", glm::vec2 (0.0f) },
	     { R"({"id":1,"name":"text","text":"AA","spacing":"1.25 -2.5"})", { 1.25f, -2.5f } },
	 }) {
	const auto data = WallpaperEngine::Data::JSON::JSON::parse (source);
	const auto object = WallpaperEngine::Data::Parsers::ObjectParser::parse (data, project);
	const auto* text = object->as<WallpaperEngine::Data::Model::Text> ();
	REQUIRE (text != nullptr);
	REQUIRE (text->spacing != nullptr);
	CHECK (glm::vec2 (text->spacing->evaluateVec3 (0.0f)) == expected);
    }
}

TEST_CASE ("text spacing retains scripts and samples both animation channels", "[text][spacing]") {
    const WallpaperEngine::Data::Model::Project project {};
    const auto object = WallpaperEngine::Data::Parsers::ObjectParser::parse (
	WallpaperEngine::Data::JSON::JSON::parse (R"({
	    "id":1,"name":"text","text":"AA",
	    "spacing": {
		"value":"0.25 -0.5",
		"script":"export function update(value) { return value; }",
		"animation": {
		    "c0":[{"frame":0,"value":0},{"frame":30,"value":2}],
		    "c1":[{"frame":0,"value":0},{"frame":30,"value":-4}],
		    "options":{"fps":30,"length":30,"mode":"single"},
		    "relative":true
		}
	    }
	})"), project
    );
    const auto* text = object->as<WallpaperEngine::Data::Model::Text> ();
    REQUIRE (text != nullptr);
    REQUIRE (text->spacing->value->getScriptSource ().has_value ());
    REQUIRE (text->spacing->animation != nullptr);
    CHECK (glm::vec2 (text->spacing->evaluateVec3 (0.0f)) == glm::vec2 (0.25f, -0.5f));
    CHECK (glm::vec2 (text->spacing->evaluateVec3 (0.5f)) == glm::vec2 (1.25f, -2.5f));
    CHECK (glm::vec2 (text->spacing->evaluateVec3 (1.0f)) == glm::vec2 (2.25f, -4.5f));
}

TEST_CASE ("text wrapping retains fractional glyph advances", "[text][spacing]") {
    REQUIRE (
	layoutTextLines ("abc", { .width = 30.5f }, [] (uint32_t) { return TextGlyphMetrics { 10.25f, 10.25f }; })
	== std::vector<std::string> { "ab", "c" }
    );
    REQUIRE (
	layoutTextLines ("abc", { .width = 29.0f }, [] (uint32_t) { return TextGlyphMetrics { 9.75f, 9.75f }; })
	== std::vector<std::string> { "ab", "c" }
    );
}

TEST_CASE ("text width uses signed pen and right ink bounds", "[text][spacing]") {
    const auto inset = [] (uint32_t) { return TextGlyphMetrics { 10, 8 }; };
    REQUIRE (layoutTextLines ("AB", { .width = 21 }, inset, 3) == std::vector<std::string> { "AB" });
    REQUIRE (layoutTextLines ("AB", { .width = 20 }, inset, 3) == std::vector<std::string> { "A", "B" });
    REQUIRE (layoutTextLines ("AB", { .width = 14 }, inset, -4) == std::vector<std::string> { "AB" });
    REQUIRE (layoutTextLines ("AB  ", { .width = 18 }, inset) == std::vector<std::string> { "AB  " });
    REQUIRE (
	layoutTextLines ("AB", { .width = 20 }, [] (uint32_t code) {
	    return TextGlyphMetrics { 10, code == 'B' ? 14.0f : 8.0f };
	})
	== std::vector<std::string> { "A", "B" }
    );
}

TEST_CASE ("text ellipsis fitting retains fractional glyph advances", "[text][spacing]") {
    REQUIRE (
	layoutTextLines ("abc\ndef", { .width = 36.0f, .rows = 1, .ellipsis = true },
	    [] (uint32_t code) {
		const float advance = code == 0x2026 ? 7.5f : 10.5f;
		return TextGlyphMetrics { advance, advance };
	    })
	== std::vector<std::string> { "ab…" }
    );
}

TEST_CASE ("text width wraps words and narrow vertical dates without splitting UTF-8") {
    const auto advance = [] (uint32_t) { return TextGlyphMetrics { 10, 10 }; };
    REQUIRE (
	layoutTextLines ("Wed\n9/Sep/2026", { .width = 10 }, advance)
	== std::vector<std::string> { "W", "e", "d", "9", "/", "S", "e", "p", "/", "2", "0", "2", "6" }
    );
    REQUIRE (
	layoutTextLines ("one two three", { .width = 60 }, advance)
	== std::vector<std::string> { "one", "two", "three" }
    );
    REQUIRE (layoutTextLines ("é画😀", { .width = 5 }, advance) == std::vector<std::string> { "é", "画", "😀" });
    REQUIRE (
	layoutTextLines ("one  two\r\n\nthree\n", {}, advance)
	== std::vector<std::string> { "one  two", "", "three", "" }
    );
    REQUIRE (layoutTextLines ("", {}, advance) == std::vector<std::string> { "" });
}

TEST_CASE ("text row limits truncate with a width-fitting ellipsis only when needed") {
    const auto advance = [] (uint32_t code) {
	const float advance = code == 0x2026 ? 6 : code == '.' ? 2 : 10;
	return TextGlyphMetrics { advance, advance };
    };
    REQUIRE (
	layoutTextLines ("Code therapy w / R...", { .width = 120, .rows = 1, .ellipsis = true }, advance)
	== std::vector<std::string> { "Code therap…" }
    );
    REQUIRE (
	layoutTextLines ("é画😀\nmore", { .width = 30, .rows = 1, .ellipsis = true }, advance)
	== std::vector<std::string> { "é画…" }
    );
    REQUIRE (
	layoutTextLines ("é\nmore", { .width = 1, .rows = 1, .ellipsis = true }, advance)
	== std::vector<std::string> { "é" }
    );
    REQUIRE (
	layoutTextLines ("abc…\ndef", { .rows = 1, .ellipsis = true }, advance)
	== std::vector<std::string> { "abc…" }
    );
    REQUIRE (layoutTextLines ("abc\ndef", { .rows = 1 }, advance) == std::vector<std::string> { "abc" });
    REQUIRE (
	layoutTextLines ("abc", { .width = 30, .rows = 1, .ellipsis = true }, advance)
	== std::vector<std::string> { "abc" }
    );
}

TEST_CASE ("utf8 codepoint decoding") {
    // "aé画😀" — 1-, 2-, 3- and 4-byte sequences
    const std::string text = "a\xC3\xA9\xE7\x94\xBB\xF0\x9F\x98\x80";
    size_t offset = 0;

    REQUIRE (nextUtf8Codepoint (text, offset) == 0x61);
    REQUIRE (nextUtf8Codepoint (text, offset) == 0xE9);
    REQUIRE (nextUtf8Codepoint (text, offset) == 0x753B);
    REQUIRE (nextUtf8Codepoint (text, offset) == 0x1F600);
    REQUIRE (offset == text.size ());
}

TEST_CASE ("utf8 malformed input consumes one byte at a time") {
    // stray continuation byte, then a lead byte truncated by end-of-string
    const std::string bad = "\x80\xC3";
    size_t offset = 0;

    REQUIRE (nextUtf8Codepoint (bad, offset) == 0xFFFD);
    REQUIRE (offset == 1);
    REQUIRE (nextUtf8Codepoint (bad, offset) == 0xFFFD);
    REQUIRE (offset == bad.size ());

    // lead byte followed by a non-continuation byte resynchronizes on the next char
    const std::string resync = "\xE7g";
    offset = 0;
    REQUIRE (nextUtf8Codepoint (resync, offset) == 0xFFFD);
    REQUIRE (nextUtf8Codepoint (resync, offset) == 0x67);
    REQUIRE (offset == resync.size ());
}

TEST_CASE ("text alignment follows native glyph metrics instead of the serialized layer size") {
    // x: [-10, 90], y-up: [-20, 80], so the actual glyph-bounds center is (40, 30).
    const glm::vec4 bounds = { -10.0f, -20.0f, 90.0f, 80.0f };

    const glm::vec2 center = computeTextAlignmentOffset ("center", "center", bounds, 100.0f, -25.0f, 120.0f, 1);
    REQUIRE (center.x == 0.0f);
    REQUIRE (center.y == -20.0f); // bounds center - ascender/2

    const glm::vec2 top = computeTextAlignmentOffset ("left", "top", bounds, 100.0f, -25.0f, 120.0f, 1);
    REQUIRE (top.x == 50.0f); // move the glyph center so its left-aligned block starts at the origin
    REQUIRE (top.y == -70.0f); // bounds center - ascender

    const glm::vec2 bottom = computeTextAlignmentOffset ("right", "bottom", bounds, 100.0f, -25.0f, 120.0f, 1);
    REQUIRE (bottom.x == -50.0f);
    REQUIRE (bottom.y == 55.0f); // bounds center - descender
}

TEST_CASE ("native text vertical alignment accounts for following rows") {
    const glm::vec4 bounds = { 0.0f, -140.0f, 80.0f, 80.0f };

    const glm::vec2 center = computeTextAlignmentOffset ("center", "center", bounds, 100.0f, -20.0f, 120.0f, 2);
    REQUIRE (center.y == -20.0f); // -30 - ((100 - 120) / 2)

    const glm::vec2 bottom = computeTextAlignmentOffset ("center", "bottom", bounds, 100.0f, -20.0f, 120.0f, 2);
    REQUIRE (bottom.y == 110.0f); // -30 - (-20 - 120)
}

TEST_CASE ("text effects use the native padded line box and split placement") {
    // Summer85 at 30pt/300DPI for "12:34": ink bounds are -4,-8..283,94,
    // while native expands the generated line box to -4,-31..283,114.
    const glm::vec4 glyphBounds = { -4.0f, -8.0f, 283.0f, 94.0f };
    const auto layout
	= computeTextEffectLayout ("center", "center", glyphBounds, 114.0f, -27.0f, 145.0f, 1, { 32, 32 });

    REQUIRE (layout.surfaceSize == glm::vec2 (351.0f, 209.0f));
    REQUIRE (layout.rasterOffset == glm::vec2 (0.0f, -1.5f));
    REQUIRE (layout.compositeOffset == glm::vec2 (0.0f, 15.5f));
    REQUIRE (layout.rasterOffset + layout.compositeOffset == glm::vec2 (0.0f, 14.0f));

    const auto cropped = computeTextEffectLayout (
	"center", "center", glyphBounds, 114.0f, -27.0f, 145.0f, 1, { -12, -8 }
    );
    REQUIRE (cropped.surfaceSize == glm::vec2 (263.0f, 129.0f));
    REQUIRE (cropped.compositeOffset == layout.compositeOffset);
    const auto capped = computeTextEffectLayout (
	"center", "center", glyphBounds, 114.0f, -27.0f, 145.0f, 1, { 1024, 1024 }
    );
    REQUIRE (capped.surfaceSize == glm::vec2 (1311.0f, 1169.0f));
}

TEST_CASE ("text horizontal alignment includes the pen origin before positive bearings") {
    const glm::vec4 bounds { 6.0f, -10.0f, 52.0f, 49.0f };
    CHECK (computeTextAlignmentOffset ("center", "center", bounds, 63, -16, 78, 1).x == 3.0f);
    CHECK (computeTextAlignmentOffset ("left", "center", bounds, 63, -16, 78, 1).x == 29.0f);
    CHECK (computeTextAlignmentOffset ("right", "center", bounds, 63, -16, 78, 1).x == -23.0f);
}

TEST_CASE ("MSDF text rows preserve the fractional center inferred from measured ink", "[text][msdf][alignment]") {
    using WallpaperEngine::Render::Objects::measureMsdfTextRowWidth;
    using WallpaperEngine::Render::Objects::TextShapedGlyph;

    const auto row = [] (float advance, float left, float right) {
        return std::vector<TextShapedGlyph> { { .advance = advance, .right = right, .inkLeft = left } };
    };
    const auto time = row (143.0f, 0.0f, 143.0f);
    const auto date = row (169.0f, -1.0f, 169.0f);
    const auto weekday = row (138.0f, 0.0f, 138.0f);
    CHECK (measureMsdfTextRowWidth (time, 0.0f) == 143.0f);
    CHECK (measureMsdfTextRowWidth (date, 0.0f) == 170.0f);
    CHECK (measureMsdfTextRowWidth (weekday, 0.0f) == 138.0f);
    CHECK ((measureMsdfTextRowWidth (date, 0.0f) - measureMsdfTextRowWidth (time, 0.0f)) * 0.5f == 13.5f);
}

TEST_CASE ("MSDF row width includes the origin and blank glyph pens", "[text][msdf][alignment]") {
    using WallpaperEngine::Render::Objects::computeTextRowOffset;
    using WallpaperEngine::Render::Objects::measureMsdfTextRowWidth;
    using WallpaperEngine::Render::Objects::TextShapedGlyph;

    const std::vector<TextShapedGlyph> leadingBearing {
        { .advance = 21.0f, .right = 23.0f, .inkLeft = 2.0f },
        { .advance = 21.0f, .right = 20.0f, .inkLeft = 0.0f },
    };
    const TextShapedGlyph space { .advance = 7.0f, .right = 0.0f, .inkLeft = 0.0f };
    const std::vector<TextShapedGlyph> trailingSpaces {
        { .advance = 22.0f, .right = 22.0f, .inkLeft = -1.0f }, space, space, space,
    };
    REQUIRE (measureMsdfTextRowWidth (leadingBearing, 0.0f) == 41.0f);
    const float width = measureMsdfTextRowWidth (trailingSpaces, 0.0f);
    REQUIRE (width == 37.0f);
    CHECK (computeTextRowOffset ("left", width, 127.0f, false) == 0.0f);
    CHECK (computeTextRowOffset ("center", width, 127.0f, false) == 45.0f);
    CHECK (computeTextRowOffset ("right", width, 127.0f, false) == 90.0f);
    CHECK (computeTextRowOffset ("center", 36.0f, 127.0f, false) == 45.5f);
    CHECK (computeTextRowOffset ("center", 36.0f, 127.0f, true) == 46.0f);
}
