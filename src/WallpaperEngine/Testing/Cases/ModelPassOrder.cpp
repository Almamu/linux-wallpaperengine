#include "WallpaperEngine/Render/Objects/CModel.h"
#include "WallpaperEngine/Render/Objects/ModelBounds.h"
#include "WallpaperEngine/Data/Parsers/UserSettingParser.h"
#include <glm/gtc/matrix_transform.hpp>
#include <bit>

// CEF exposes its own CHECK macro through CModel's renderer includes.
#ifdef CHECK
#undef CHECK
#endif

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

using WallpaperEngine::Data::Model::BlendingMode;
using WallpaperEngine::Render::Objects::CModel;

TEST_CASE ("Model pose weights sample the authored blend timeline", "[model-animation-blend]") {
    using namespace WallpaperEngine::Data;
    using Parsers::UserSettingParser;
    Model::ImageAnimationLayer settings {};
    // Rayman - Gemstone Temple, Look Left: the independently triggered blend curve.
    settings.blend = UserSettingParser::parse (JSON::JSON::parse (R"({"value":0,
        "animation":{"c0":[
            {"frame":0,"value":0,"front":{"enabled":true,"x":1,"y":0}},
            {"frame":10,"value":1,"back":{"enabled":true,"x":-1,"y":0}},
            {"frame":80,"value":1,"front":{"enabled":true,"x":1,"y":0}},
            {"frame":90,"value":0,"back":{"enabled":true,"x":-1,"y":0}}],
        "options":{"fps":30,"length":90,"mode":"single","name":"look","startpaused":true}}})"), {});
    CModel::AnimationLayer layer { .settings = &settings };
    auto& curve = *settings.blend->animation;
    CHECK (layer.evaluateBlend (8.0) == 0.0f);
    curve.play (8.0);
    CHECK (layer.evaluateBlend (8.0 + 5.0 / 30.0) == Catch::Approx (0.5f));
    CHECK (layer.evaluateBlend (9.0) == Catch::Approx (1.0f));
    layer.blendWrite = { 9.0, 0.0f }; // On this update, the attached blend script returned zero.
    CHECK (layer.evaluateBlend (9.0) == 0.0f);
    CHECK (layer.evaluateBlend (9.0 + 1.0 / 30.0) == Catch::Approx (1.0f));
    layer.setBlend (-0.25f, 9.0);
    CHECK (layer.evaluateBlend (9.0) == 0.0f);
    CHECK (layer.evaluateBlend (9.0 + 1.0 / 30.0) == Catch::Approx (1.0f));
    curve.pause (9.0);
    CHECK (layer.evaluateBlend (20.0) == Catch::Approx (1.0f));
    layer.setBlend (0.0f, 20.0);
    CHECK (layer.evaluateBlend (20.0) == 0.0f);
    CHECK (layer.evaluateBlend (20.0 + 1.0 / 30.0) == Catch::Approx (1.0f));
    curve.play (20.0);
    CHECK (layer.evaluateBlend (20.0 + 55.0 / 30.0) == Catch::Approx (0.5f));
    CHECK (layer.evaluateBlend (22.0) == 0.0f);
    CHECK (layer.evaluateBlend (24.0) == 0.0f);
    curve.play (24.0);
    CHECK (layer.evaluateBlend (25.0) == Catch::Approx (1.0f));
    curve.stop (25.0);
    CHECK (layer.evaluateBlend (26.0) == 0.0f);

    curve.mode = "loop";
    curve.play (26.0);
    CHECK (layer.evaluateBlend (29.0) == 0.0f);
    CHECK (layer.evaluateBlend (30.0) == Catch::Approx (1.0f));
}

TEST_CASE ("Model layers read live scalar blend for unkeyed and one-shot poses", "[model-animation-blend]") {
    using namespace WallpaperEngine::Data::Model;
    ImageAnimationLayer settings { .blend = std::make_unique<UserSetting> (UserSetting {
        .value = std::make_unique<DynamicValue> (0.25f) }) };
    CModel::AnimationLayer authored { .settings = &settings };
    CHECK (authored.evaluateBlend (1.0) == 0.25f);
    settings.blend->value->update (0.0f, DynamicValue::UpdateSource::Script);
    CHECK (authored.evaluateBlend (2.0) == 0.0f);
    authored.setBlend (1.5f, 3.0);
    CHECK (authored.evaluateBlend (4.0) == 1.0f);

    CModel::AnimationLayer oneShot { .blend = 0.5f, .single = true };
    CHECK (oneShot.evaluateBlend (2.0) == 0.5f);
    oneShot.setBlend (0.75f, 3.0);
    CHECK (oneShot.evaluateBlend (4.0) == 0.75f);
}

TEST_CASE ("Model passes render opaque before blended while preserving authored order") {
    const std::vector<BlendingMode> modes {
	WallpaperEngine::Data::Model::BlendingMode_Translucent,
	WallpaperEngine::Data::Model::BlendingMode_Normal,
	WallpaperEngine::Data::Model::BlendingMode_Additive,
	WallpaperEngine::Data::Model::BlendingMode_AlphaToCoverage,
	WallpaperEngine::Data::Model::BlendingMode_Translucent,
	WallpaperEngine::Data::Model::BlendingMode_Normal,
    };

    CHECK (CModel::calculatePassRenderPermutation (modes) == std::vector<size_t> { 1, 3, 5, 0, 2, 4 });
}

TEST_CASE ("Model bounds retain clip intersections and reject wholly outside geometry") {
    using WallpaperEngine::Render::Objects::ModelBounds;
    ModelBounds::Box box;
    box.include (glm::vec3 (-0.5f));
    box.include (glm::vec3 (0.5f));
    CHECK (box.intersects (glm::mat4 (1.0f)));
    CHECK (box.intersects (glm::translate (glm::mat4 (1.0f), glm::vec3 (1.5f, 0, 0))));
    CHECK_FALSE (box.intersects (glm::translate (glm::mat4 (1.0f), glm::vec3 (1.6f, 0, 0))));
    const auto perspective = glm::perspective (glm::radians (60.0f), 1.0f, 0.1f, 100.0f);
    CHECK (box.intersects (perspective * glm::translate (glm::mat4 (1.0f), glm::vec3 (0, 0, -2))));
    CHECK_FALSE (box.intersects (perspective * glm::translate (glm::mat4 (1.0f), glm::vec3 (0, 0, 2))));
    CHECK (box.intersects (perspective)); // Crossing the eye/near plane cannot be discarded.
    CHECK (ModelBounds::Box {}.intersects (glm::mat4 (1.0f))); // Unknown bounds stay visible.
}

TEST_CASE ("Skinned model bounds follow weighted bones instead of authored rest bounds") {
    using namespace WallpaperEngine::Data::Model;
    using WallpaperEngine::Render::Objects::ModelBounds;
    MdlMesh mesh;
    mesh.vertexTag = 1;
    mesh.strideBytes = 44;
    mesh.blendIndicesOffset = 12;
    mesh.blendWeightsOffset = 28;
    MdlSubmesh part;
    part.vertexTag = 1;
    part.vertices = { 0, 0, 0, std::bit_cast<float> (0u), std::bit_cast<float> (1u), 0, 0,
                      0.25f, 0.75f, 0, 0 };
    part.indices = { 0, 0, 0 };
    mesh.submeshes.push_back (part);
    ModelBounds bounds (mesh, 2);
    std::vector<glm::mat4x3> bones {
        glm::mat4x3 (glm::translate (glm::mat4 (1.0f), glm::vec3 (-4, 0, 0))),
        glm::mat4x3 (glm::translate (glm::mat4 (1.0f), glm::vec3 (4, 0, 0))) };
    const auto posed = bounds.evaluate (bones);
    CHECK (posed.minimum.x <= 2.0f);
    CHECK (posed.maximum.x >= 2.0f);
    CHECK (posed.intersects (glm::translate (glm::mat4 (1.0f), glm::vec3 (-2, 0, 0))));
    CHECK_FALSE (posed.intersects (glm::translate (glm::mat4 (1.0f), glm::vec3 (20, 0, 0))));
    mesh.submeshes[0].vertices[7] = -0.25f;
    CHECK (ModelBounds (mesh, 2).evaluate (bones).intersects (
        glm::translate (glm::mat4 (1.0f), glm::vec3 (20, 0, 0))));
}

TEST_CASE ("Skinned bounds contain GPU blended positions with nonunit weights and reflected bones") {
    using namespace WallpaperEngine::Data::Model;
    using WallpaperEngine::Render::Objects::ModelBounds;
    MdlMesh mesh;
    mesh.vertexTag = 1;
    mesh.strideBytes = 44;
    mesh.blendIndicesOffset = 12;
    mesh.blendWeightsOffset = 28;
    MdlSubmesh part;
    part.vertexTag = 1;
    for (int vertex = 0; vertex < 12; ++vertex) {
	part.vertices.insert (part.vertices.end (), {
	    float (vertex - 6), float (vertex % 3 - 1), float (vertex % 5 - 2),
	    std::bit_cast<float> (0u), std::bit_cast<float> (1u), std::bit_cast<float> (2u),
	    std::bit_cast<float> (3u), vertex == 0 ? 0.0f : 0.1f, float (vertex % 3) * 0.2f,
	    float (vertex % 4) * 0.3f, float (vertex % 2) * 0.5f });
	part.indices.push_back (vertex);
    }
    mesh.submeshes.push_back (part);
    ModelBounds bounds (mesh, 4);
    for (int frame = 0; frame < 40; ++frame) {
	std::vector<glm::mat4x3> bones;
	for (int bone = 0; bone < 4; ++bone) {
	    auto transform = glm::translate (glm::mat4 (1.0f), glm::vec3 (bone * 7 - 10, frame * 0.3f, -bone));
	    transform = glm::rotate (transform, frame * 0.17f + bone, glm::normalize (glm::vec3 (1, 2, 3)));
	    bones.emplace_back (glm::scale (transform, glm::vec3 (bone % 2 ? -2 : 1, 0.5f, 3)));
	}
	const auto box = bounds.evaluate (bones);
	REQUIRE (box.valid ());
	for (size_t vertex = 0; vertex < part.vertices.size (); vertex += 11) {
	    const glm::vec4 position (part.vertices[vertex], part.vertices[vertex + 1], part.vertices[vertex + 2], 1);
	    glm::mat4x3 skin (0.0f);
	    for (size_t bone = 0; bone < 4; ++bone) skin += bones[bone] * part.vertices[vertex + 7 + bone];
	    const auto posed = skin * position;
	    CHECK (glm::all (glm::greaterThanEqual (posed, box.minimum - glm::vec3 (0.0001f))));
	    CHECK (glm::all (glm::lessThanEqual (posed, box.maximum + glm::vec3 (0.0001f))));
	}
    }
}
