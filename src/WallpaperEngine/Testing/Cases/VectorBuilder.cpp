#include <catch2/catch_test_macros.hpp>

#include <glm/glm.hpp>

#include "WallpaperEngine/Data/Builders/ColorBuilder.h"
#include "WallpaperEngine/Data/Builders/VectorBuilder.h"

using WallpaperEngine::Data::Builders::ColorBuilder;
using WallpaperEngine::Data::Builders::VectorBuilder;

static glm::vec4 rgba (const WallpaperEngine::Data::Model::Color& color) {
    return { color.r, color.g, color.b, color.a };
}

// Vectors are usually stored as "x y z", but some workshop effects store them
// comma-separated: the "Raindrop on Glass" effect (workshop 3706822104) has
// "Drop Shadow Color" : "0.10,0.11,0.12", which used to make the whole image
// layer fail to load ("Invalid vector format ... too few values").

TEST_CASE ("Vectors parse with space separators", "[vector]") {
    CHECK (VectorBuilder::parse<glm::vec2> ("1 2") == glm::vec2 (1, 2));
    CHECK (VectorBuilder::parse<glm::vec3> ("0.5 1.5 -2") == glm::vec3 (0.5f, 1.5f, -2.0f));
    CHECK (VectorBuilder::parse<glm::vec4> ("1 2 3 4") == glm::vec4 (1, 2, 3, 4));
}

TEST_CASE ("Vectors parse with comma separators", "[vector]") {
    CHECK (VectorBuilder::parse<glm::vec3> ("0.10,0.11,0.12") == glm::vec3 (0.10f, 0.11f, 0.12f));
    CHECK (VectorBuilder::parse<glm::vec3> ("0.04,0.05,0.06") == glm::vec3 (0.04f, 0.05f, 0.06f));
    CHECK (VectorBuilder::parse<glm::vec2> ("1,2") == glm::vec2 (1, 2));
    CHECK (VectorBuilder::parse<glm::vec4> ("1,2,3,4") == glm::vec4 (1, 2, 3, 4));
    CHECK (VectorBuilder::parse<glm::ivec3> ("255,128,0") == glm::ivec3 (255, 128, 0));
}

TEST_CASE ("Vectors parse with a comma and a space between values", "[vector]") {
    CHECK (VectorBuilder::parse<glm::vec3> ("0.10, 0.11, 0.12") == glm::vec3 (0.10f, 0.11f, 0.12f));
    CHECK (VectorBuilder::parse<glm::vec2> ("1,  2") == glm::vec2 (1, 2));
}

TEST_CASE ("Vectors parse with repeated spaces or a space before a comma", "[vector]") {
    CHECK (VectorBuilder::parse<glm::vec2> ("1  2") == glm::vec2 (1, 2));
    CHECK (VectorBuilder::parse<glm::vec2> ("1 , 2") == glm::vec2 (1, 2));
    CHECK (VectorBuilder::parse<glm::vec3> (" 1 2 3 ") == glm::vec3 (1, 2, 3));
}

TEST_CASE ("Vectors with the wrong number of values still throw", "[vector]") {
    CHECK_THROWS (VectorBuilder::parse<glm::vec3> ("0.10,0.11"));
    CHECK_THROWS (VectorBuilder::parse<glm::vec2> ("1,2,3"));
    CHECK_THROWS (VectorBuilder::parse<glm::vec3> ("1,2,3,4"));
    CHECK_THROWS (VectorBuilder::parse<glm::vec4> ("1 2,3"));
    CHECK_THROWS (VectorBuilder::parse<glm::vec4> ("1,2,3,4,5"));
    CHECK_THROWS (VectorBuilder::parse<glm::vec4> ("1 2 3 4 5"));
}

TEST_CASE ("Vectors with an empty value throw", "[vector]") {
    CHECK_THROWS (VectorBuilder::parse<glm::vec3> ("1,,2"));
    CHECK_THROWS (VectorBuilder::parse<glm::vec3> ("1,2,"));
    CHECK_THROWS (VectorBuilder::parse<glm::vec3> (",1,2"));
    CHECK_THROWS (VectorBuilder::parse<glm::vec2> ("1, ,2"));
}

// preparseSize picks the vector size that DynamicValueParser and ColorBuilder then
// pass to parse, so it has to split values the same way.
TEST_CASE ("preparseSize counts values like parse does", "[vector]") {
    CHECK (VectorBuilder::preparseSize ("1 2") == 2);
    CHECK (VectorBuilder::preparseSize ("0.10,0.11,0.12") == 3);
    CHECK (VectorBuilder::preparseSize ("0.1, 0.2, 0.3") == 3);
    CHECK (VectorBuilder::preparseSize ("1,2 3") == 3);
    CHECK (VectorBuilder::preparseSize ("1 2 3 4") == 4);
}

TEST_CASE ("preparseSize returns 1 for scalars, text and malformed vectors", "[vector]") {
    CHECK (VectorBuilder::preparseSize ("0.5") == 1);
    CHECK (VectorBuilder::preparseSize ("Rain,Snow") == 1);
    CHECK (VectorBuilder::preparseSize ("Rain and snow") == 1);
    CHECK (VectorBuilder::preparseSize ("1,,2") == 1);
    CHECK (VectorBuilder::preparseSize ("1 2 3 4 5") == 1);
}

TEST_CASE ("Colors parse with a comma and a space between values", "[vector][color]") {
    // ColorBuilder used to turn ", " into two spaces, which counted as four values
    // and put green in both g and b, and blue in alpha
    const glm::vec4 color = rgba (ColorBuilder::parse ("0.1, 0.2, 0.3"));
    CHECK (color == glm::vec4 (0.1f, 0.2f, 0.3f, 1.0f));

    const glm::vec4 intColor = rgba (ColorBuilder::parse ("255, 0, 51"));
    CHECK (intColor == glm::vec4 (1.0f, 0.0f, 0.2f, 1.0f));

    CHECK_THROWS (ColorBuilder::parse ("0.1,,0.3"));
}
