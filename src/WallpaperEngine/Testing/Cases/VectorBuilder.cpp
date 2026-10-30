#include <catch2/catch_test_macros.hpp>

#include <glm/glm.hpp>

#include "WallpaperEngine/Data/Builders/VectorBuilder.h"

using WallpaperEngine::Data::Builders::VectorBuilder;

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

TEST_CASE ("Vectors with the wrong number of values still throw", "[vector]") {
    CHECK_THROWS (VectorBuilder::parse<glm::vec3> ("0.10,0.11"));
    CHECK_THROWS (VectorBuilder::parse<glm::vec2> ("1,2,3"));
    CHECK_THROWS (VectorBuilder::parse<glm::vec4> ("1 2,3"));
}
