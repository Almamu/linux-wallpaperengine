#include <catch2/catch_test_macros.hpp>

#include <array>
#include <string>

#include "WallpaperEngine/Data/Builders/ColorBuilder.h"
#include "WallpaperEngine/Data/Model/Property.h"
#include "WallpaperEngine/Data/Parsers/PropertyParser.h"

using WallpaperEngine::Data::Builders::ColorBuilder;
using WallpaperEngine::Data::JSON::JSON;
using WallpaperEngine::Data::Model::DynamicValue;
using WallpaperEngine::Data::Parsers::PropertyParser;

TEST_CASE ("Bool properties without a value default to false") {
    const JSON propertyData = {
	{ "type", "bool" },
	{ "text", "Enabled" },
    };

    const auto property = PropertyParser::parse (propertyData, "enabled");

    REQUIRE (property != nullptr);
    CHECK (property->getType () == DynamicValue::Boolean);
    CHECK_FALSE (property->getBool ());
}

TEST_CASE ("Directory properties are parsed as file-like properties") {
    const JSON propertyData = {
	{ "type", "directory" },
	{ "text", "Folder" },
    };

    const auto property = PropertyParser::parse (propertyData, "folder");

    REQUIRE (property != nullptr);
    CHECK (property->dump ().find ("folder - file") != std::string::npos);
}
TEST_CASE ("Hex colors parse in every CSS length") {
    const auto rgba = [] (const std::string& value) {
	const auto c = ColorBuilder::parse (value);
	return std::array<int, 4> { static_cast<int> (c.r * 255.0f + 0.5f), static_cast<int> (c.g * 255.0f + 0.5f),
				    static_cast<int> (c.b * 255.0f + 0.5f), static_cast<int> (c.a * 255.0f + 0.5f) };
    };

    CHECK (rgba ("#f00") == std::array<int, 4> { 255, 0, 0, 255 });
    CHECK (rgba ("#f008") == std::array<int, 4> { 255, 0, 0, 136 });
    // six digits used to be read as GGBBAA with red 0
    CHECK (rgba ("#ff8000") == std::array<int, 4> { 255, 128, 0, 255 });
    // eight digits with red above 7f used to overflow std::stoi
    CHECK (rgba ("#ff000080") == std::array<int, 4> { 255, 0, 0, 128 });
    CHECK (rgba ("#11223344") == std::array<int, 4> { 17, 34, 51, 68 });
}

TEST_CASE ("Malformed hex colors throw") {
    CHECK_THROWS (ColorBuilder::parse ("#12345"));
    CHECK_THROWS (ColorBuilder::parse ("#gg0000"));
    CHECK_THROWS (ColorBuilder::parse ("#ff00zz"));
}
