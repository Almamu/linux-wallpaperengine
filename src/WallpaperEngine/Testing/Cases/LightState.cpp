#include "WallpaperEngine/Data/Model/DynamicValue.h"
#include "WallpaperEngine/Data/Model/Object.h"
#include "WallpaperEngine/Data/Parsers/ObjectParser.h"
#include "WallpaperEngine/Render/Objects/CLight.h"

// CEF exposes its own CHECK macro through CLight's scene includes.
#ifdef CHECK
#undef CHECK
#endif

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>

using WallpaperEngine::Data::Model::LightData;
using WallpaperEngine::Render::Objects::CLight;
using WallpaperEngine::Render::Wallpapers::CScene;

TEST_CASE ("Spotlights retain authored cookie selection", "[lighting][cookie]") {
    using namespace WallpaperEngine::Data::Model;
    using WallpaperEngine::Data::Parsers::ObjectParser;
    using WallpaperEngine::Data::JSON::JSON;
    const Project project {};
    const auto defaults = ObjectParser::parse (JSON::parse (R"({"id":2,"name":"spot","light":"lspot"})"), project);
    CHECK_FALSE (defaults->as<Light> ()->useCookie);
    CHECK (defaults->as<Light> ()->cookie.empty ());
    const auto authored = ObjectParser::parse (JSON::parse (
        R"({"id":2,"name":"spot","light":"lspot","usecookie":true,"cookie":"_rt_imageLayerComposite_4_a"})"
    ), project);
    CHECK (authored->as<Light> ()->useCookie);
    CHECK (authored->as<Light> ()->cookie == "_rt_imageLayerComposite_4_a");
}

TEST_CASE ("Light volumes preserve native defaults and authored settings", "[lighting][volumetrics]") {
    using namespace WallpaperEngine::Data::Model;
    using WallpaperEngine::Data::Parsers::ObjectParser;
    using WallpaperEngine::Data::JSON::JSON;
    const Project project {};
    const auto defaults = ObjectParser::parse (JSON::parse (R"({"id":2,"light":"lspot"})"), project);
    const auto* light = defaults->as<Light> ();
    REQUIRE (light != nullptr);
    CHECK_FALSE (light->castVolumetrics->value->getBool ());
    // Native 14018ff60 initializes density at +0x2f8 to 2 and exponent at +0x2fc to 1.
    CHECK (light->density->evaluateFloat (0) == 2.0f);
    CHECK (light->volumetricsExponent->evaluateFloat (0) == 1.0f);

    const auto authored = ObjectParser::parse (JSON::parse (
        R"({"id":127,"light":"lspot","castvolumetrics":true,"density":0.5,"volumetricsexponent":3})"
    ), project);
    light = authored->as<Light> ();
    CHECK (light->castVolumetrics->value->getBool ());
    CHECK (light->density->evaluateFloat (0) == .5f);
    CHECK (light->volumetricsExponent->evaluateFloat (0) == 3.0f);
}

TEST_CASE ("Legacy point lights retain positions and clear unused slots", "[lighting]") {
    CScene::SceneLights lights;
    lights.pointCount = 5;
    lights.pointOrigins = {
	{ 1, 2, 3, 99 }, { 4, 5, 6, 99 }, { 7, 8, 9, 99 }, { 10, 11, 12, 99 }, { 13, 14, 15, 99 }
    };
    lights.pointColors = {
	{ 0.2f, 0.4f, 0.6f, 2.5f }, { 1, 0, 0, 3 }, { 0, 1, 0, 4 }, { 0, 0, 1, 5 }, { 1, 1, 1, 6 }
    };
    lights.updateLegacyPointLights ();
    CHECK (lights.legacyPositions[0] == glm::vec3 (1, 2, 3));
    CHECK (lights.legacyColors[0] == glm::vec4 (0.2f, 0.4f, 0.6f, 2.5f));
    CHECK (lights.legacyPositions[3] == glm::vec3 (10, 11, 12));
    CHECK (lights.legacyColors[3] == glm::vec4 (0, 0, 1, 5));
    CHECK (lights.legacyPremultipliedColors[0] == glm::vec4 (1.25f, 2.5f, 3.75f, 0));
    CHECK (lights.legacyPremultipliedColors[1] == glm::vec4 (9, 0, 0, 0));
    CHECK (lights.legacyPremultipliedColors[2] == glm::vec4 (0, 16, 0, 25));

    lights.pointCount = 1;
    lights.updateLegacyPointLights ();
    CHECK (lights.legacyColors[0] == glm::vec4 (0.2f, 0.4f, 0.6f, 2.5f));
    CHECK (lights.legacyPremultipliedColors[0] == glm::vec4 (1.25f, 2.5f, 3.75f, 0));
    CHECK (lights.legacyPremultipliedColors[1] == glm::vec4 (0));
    CHECK (lights.legacyPremultipliedColors[2] == glm::vec4 (0));
    for (int index = 1; index < 4; ++index) {
	CHECK (lights.legacyPositions[index] == glm::vec3 (0));
	CHECK (lights.legacyColors[index] == glm::vec4 (0, 0, 0, 1));
    }
}

TEST_CASE ("Light type IDs match Wallpaper Engine") {
    CHECK (LightData::Type_Point == 0);
    CHECK (LightData::Type_Spot == 1);
    CHECK (LightData::Type_Tube == 2);
    CHECK (LightData::Type_Directional == 3);
}

TEST_CASE ("Spot cone angles are packed as cosine thresholds") {
    const glm::vec2 cones = CLight::calculateSpotConeCosines (20.0f, 35.0f);

    CHECK (cones.x == Catch::Approx (0.9396926f));
    CHECK (cones.y == Catch::Approx (0.8191520f));
    CHECK (cones.x > cones.y);
}

TEST_CASE ("Spot shadow projection covers the authored cone and radius") {
    const glm::vec3 origin (10.0f, 3.0f, -2.0f);
    const glm::vec3 direction = glm::normalize (glm::vec3 (-1.0f, -0.2f, 0.1f));
    const glm::mat4 projection = CLight::calculateSpotShadowViewProjection (origin, direction, 35.0f, 100.0f);

    const glm::vec4 centerClip = projection * glm::vec4 (origin + direction * 50.0f, 1.0f);
    const glm::vec3 centerNdc = glm::vec3 (centerClip) / centerClip.w;
    CHECK (std::isfinite (centerNdc.x));
    CHECK (std::isfinite (centerNdc.y));
    CHECK (std::isfinite (centerNdc.z));
    CHECK (std::abs (centerNdc.x) < 0.001f);
    CHECK (std::abs (centerNdc.y) < 0.001f);
    CHECK (centerNdc.z > -1.0f);
    CHECK (centerNdc.z < 1.0f);
}

TEST_CASE ("Point shadow projections cover all six native atlas faces") {
    const glm::vec3 origin (3.0f, -2.0f, 5.0f);
    const auto projections = CLight::calculatePointShadowViewProjections (origin, 20.0f);
    const std::array<glm::vec3, 6> directions = {
	glm::vec3 (1.0f, 0.0f, 0.0f), glm::vec3 (-1.0f, 0.0f, 0.0f),
	glm::vec3 (0.0f, 1.0f, 0.0f), glm::vec3 (0.0f, -1.0f, 0.0f),
	glm::vec3 (0.0f, 0.0f, 1.0f), glm::vec3 (0.0f, 0.0f, -1.0f),
    };

    for (size_t face = 0; face < projections.size (); face++) {
	const glm::vec4 clip = projections[face] * glm::vec4 (origin + directions[face] * 10.0f, 1.0f);
	const glm::vec3 ndc = glm::vec3 (clip) / clip.w;
	CHECK (std::isfinite (ndc.x));
	CHECK (std::isfinite (ndc.y));
	CHECK (std::isfinite (ndc.z));
	CHECK (ndc.x == Catch::Approx (0.0f).margin (0.000001f));
	CHECK (ndc.y == Catch::Approx (0.0f).margin (0.000001f));
	CHECK (ndc.z > -1.0f);
	CHECK (ndc.z < 1.0f);
    }
}

TEST_CASE ("Point shadow projection info reconstructs the raster depth projection") {
    const glm::vec4 info = CLight::calculatePointShadowProjectionInfo (20.0f);
    const glm::mat4 expected = glm::perspective (glm::half_pi<float> (), 1.0f, 0.05f, 20.0f);

    CHECK (info.x == Catch::Approx (expected[2][2]));
    CHECK (info.y == Catch::Approx (expected[3][2]));
    CHECK (info.z == Catch::Approx (expected[2][3]));
    CHECK (info.w == Catch::Approx (expected[3][3]));
}

TEST_CASE ("Point shadow face bases survive large world-space translations", "[lighting][point-shadow]") {
    // At this position, float(origin + unitAxis) can round back to origin.
    const glm::vec3 origin (33554432.0f);
    const auto matrices = CLight::calculatePointShadowViewProjections (origin, 64.0f);
    const std::array<glm::vec3, 6> offsets = {
	glm::vec3 (16, 0, 0), glm::vec3 (-16, 0, 0),
	glm::vec3 (0, 16, 0), glm::vec3 (0, -16, 0),
	glm::vec3 (0, 0, 16), glm::vec3 (0, 0, -16),
    };
    for (size_t face = 0; face < matrices.size (); ++face) {
	const auto clip = matrices[face] * glm::vec4 (origin + offsets[face], 1.0f);
	CHECK (clip.x == Catch::Approx (0.0f).margin (1e-6));
	CHECK (clip.y == Catch::Approx (0.0f).margin (1e-6));
	CHECK (clip.w == Catch::Approx (16.0f));
	CHECK (std::isfinite (clip.z));
    }
}

TEST_CASE ("Point source size preserves native defaults and near clipping", "[lighting][source-size]") {
    using namespace WallpaperEngine::Data::Model;
    using WallpaperEngine::Data::Parsers::ObjectParser;
    using WallpaperEngine::Data::JSON::JSON;
    const Project project {};
    const auto defaults = ObjectParser::parse (JSON::parse (R"({"id":2,"light":"lpoint"})"), project);
    CHECK (defaults->as<Light> ()->sourceSize->evaluateFloat (0) == 0.0f);
    const auto authored = ObjectParser::parse (JSON::parse (
	R"({"id":2,"light":"lpoint","lightsourcesize":0.53})"
    ), project);
    CHECK (authored->as<Light> ()->sourceSize->evaluateFloat (0) == Catch::Approx (0.53f));

    for (const bool orthographic : { false, true }) {
	for (const float size : { -2.0f, 0.0f, 0.3f, 2.0f }) {
	    const float nearPlane = orthographic ? 1.0f : std::max (size, 0.05f);
	    const float farPlane = std::max (0.5f, nearPlane + 0.01f);
	    const auto matrices = CLight::calculatePointShadowViewProjections (glm::vec3 (0), 0.5f, size, orthographic);
	    const auto info = CLight::calculatePointShadowProjectionInfo (0.5f, size, orthographic);
	    for (const float distance : { nearPlane, farPlane }) {
		const auto clip = matrices[0] * glm::vec4 (distance, 0, 0, 1);
		const float reconstructed = (info.x * -distance + info.y) / (info.z * -distance + info.w);
		CHECK (clip.z / clip.w == Catch::Approx (distance == nearPlane ? -1.0f : 1.0f).margin (0.0001f));
		CHECK (clip.z / clip.w == Catch::Approx (reconstructed).margin (0.0001f));
	    }
	}
    }
}

TEST_CASE ("Vertical spot shadow projections remain finite") {
    const glm::mat4 projection = CLight::calculateSpotShadowViewProjection (
	glm::vec3 (0.0f), glm::vec3 (0.0f, 1.0f, 0.0f), 30.0f, 10.0f
    );

    for (int column = 0; column < 4; column++) {
	for (int row = 0; row < 4; row++) {
	    CHECK (std::isfinite (projection[column][row]));
	}
    }
}

TEST_CASE ("Directional shadow cascades enclose their camera frustum") {
    const glm::vec3 eye (0.0f, 0.0f, 5.0f);
    const glm::vec3 center (0.0f);
    const float nearDistance = 0.1f;
    const float farDistance = 10.0f;
    const float fieldOfView = 60.0f;
    const float aspect = 16.0f / 9.0f;
    const glm::mat4 projection = CLight::calculateDirectionalShadowViewProjection (
	eye, center, glm::vec3 (0.0f, 1.0f, 0.0f), fieldOfView, aspect, 1.0f, nearDistance, farDistance,
	glm::normalize (glm::vec3 (-1.0f, -1.0f, -1.0f)), 512
    );

    for (const float distance : { nearDistance, farDistance }) {
	const float halfHeight = std::tan (glm::radians (fieldOfView) * 0.5f) * distance;
	const float halfWidth = halfHeight * aspect;
	for (const float y : { -1.0f, 1.0f }) {
	    for (const float x : { -1.0f, 1.0f }) {
		const glm::vec3 point = eye + glm::vec3 (x * halfWidth, y * halfHeight, -distance);
		const glm::vec4 clip = projection * glm::vec4 (point, 1.0f);
		const glm::vec3 ndc = glm::vec3 (clip) / clip.w;
		CHECK (std::isfinite (ndc.x));
		CHECK (std::isfinite (ndc.y));
		CHECK (std::isfinite (ndc.z));
		CHECK (std::abs (ndc.x) <= 1.001f);
		CHECK (std::abs (ndc.y) <= 1.001f);
		CHECK (std::abs (ndc.z) <= 1.001f);
	    }
	}
    }
}

TEST_CASE ("Directional shadow cascades remain finite for vertical light directions") {
    const glm::mat4 projection = CLight::calculateDirectionalShadowViewProjection (
	glm::vec3 (0.0f, 0.0f, 5.0f), glm::vec3 (0.0f), glm::vec3 (0.0f, 1.0f, 0.0f), 60.0f,
	16.0f / 9.0f, 1.0f, 0.1f, 10.0f, glm::vec3 (0.0f, 1.0f, 0.0f), 512
    );

    for (int column = 0; column < 4; column++) {
	for (int row = 0; row < 4; row++) {
	    CHECK (std::isfinite (projection[column][row]));
	}
    }
}

TEST_CASE ("Directional shadow cascades suppress sub-texel camera movement") {
    const glm::vec3 lightDirection = glm::normalize (glm::vec3 (-1.0f, -1.0f, -1.0f));
    const glm::mat4 original = CLight::calculateDirectionalShadowViewProjection (
	glm::vec3 (0.0f, 0.0f, 5.0f), glm::vec3 (0.0f), glm::vec3 (0.0f, 1.0f, 0.0f), 60.0f,
	16.0f / 9.0f, 1.0f, 0.1f, 10.0f, lightDirection, 512
    );
    const glm::vec3 movement (0.00001f, 0.0f, 0.0f);
    const glm::mat4 moved = CLight::calculateDirectionalShadowViewProjection (
	glm::vec3 (0.0f, 0.0f, 5.0f) + movement, glm::vec3 (0.0f) + movement,
	glm::vec3 (0.0f, 1.0f, 0.0f), 60.0f, 16.0f / 9.0f, 1.0f, 0.1f, 10.0f, lightDirection, 512
    );

    for (int column = 0; column < 4; column++) {
	for (int row = 0; row < 4; row++) {
	    CHECK (moved[column][row] == Catch::Approx (original[column][row]).margin (0.000001f));
	}
    }
}

TEST_CASE ("Tube control point follows the full world transform") {
    glm::mat4 world = glm::translate (glm::mat4 (1.0f), glm::vec3 (10.0f, -2.0f, 3.0f));
    world = glm::rotate (world, glm::half_pi<float> (), glm::vec3 (0.0f, 0.0f, 1.0f));
    world = glm::scale (world, glm::vec3 (2.0f));

    const glm::vec3 endpoint = CLight::calculateTubeEndPosition (world, glm::vec3 (3.0f, 0.0f, 0.0f));
    CHECK (endpoint.x == Catch::Approx (10.0f));
    CHECK (endpoint.y == Catch::Approx (4.0f));
    CHECK (endpoint.z == Catch::Approx (3.0f));
}

TEST_CASE ("Reverse point shadows retain unbiased receivers and native caster clipping", "[lighting][reverse-shadow]") {
    const glm::vec3 origin (3, -2, 5);
    const auto matrices = CLight::calculatePointShadowViewProjections (origin, 10000, 1, false, true);
    const auto info = CLight::calculatePointShadowProjectionInfo (10000, 1, false, true);
    CHECK (info.x == Catch::Approx (0.0001000100019f));
    CHECK (info.y == Catch::Approx (1.0001000166f));
    CHECK (info.z == -1.0f);
    CHECK (info.w == 0.0f);
    const std::array<glm::vec3, 6> directions = {
        glm::vec3 (1, 0, 0), glm::vec3 (-1, 0, 0), glm::vec3 (0, 1, 0),
        glm::vec3 (0, -1, 0), glm::vec3 (0, 0, 1), glm::vec3 (0, 0, -1)
    };
    for (size_t face = 0; face < matrices.size (); ++face) {
        for (float distance : { 100.0f, 9000.0f, 9990.0f }) {
            const auto clip = matrices[face] * glm::vec4 (origin + directions[face] * distance, 1);
            const float receiver = (-distance * info.x + info.y) / distance;
            CHECK (clip.x / clip.w == Catch::Approx (0).margin (1e-6));
            CHECK (clip.y / clip.w == Catch::Approx (0).margin (1e-6));
            CHECK (clip.z / clip.w == Catch::Approx (receiver - .00333f / distance).margin (1e-9));
            CHECK ((clip.z > 0) == (distance < 9966.7f));
        }
    }
    // Match the quality-2 helper's widened face; compact Z stays independent of FOV.
    const auto offAxis = matrices[4] * glm::vec4 (origin + glm::vec3 (9000, 0, 9000), 1);
    CHECK (std::abs (offAxis.x / offAxis.w) == Catch::Approx (1.0f / std::tan (glm::radians (47.0f))));
}

TEST_CASE ("Reverse spot projection matches native fast basis and legal far caster", "[lighting][reverse-shadow]") {
    const glm::mat4 world = glm::rotate (glm::mat4 (1), -glm::half_pi<float> (), glm::vec3 (0, 1, 0));
    const auto sampling = CLight::calculateSpotProjection (world, 45, 10000, false, true);
    const auto two = sampling * glm::vec4 (0, 0, 2, 1);
    // Native04 numeric readback .0250374094, with transport quantization 2.4e-7.
    CHECK (two.z / two.w == Catch::Approx (.0250374094f).margin (2.4e-7));
    auto caster = sampling;
    caster[3][2] -= .0005f;
    for (float distance : { 100.0f, 9000.0f, 9990.0f }) {
        const auto receiver = sampling * glm::vec4 (0, 0, distance, 1);
        const auto clip = caster * glm::vec4 (0, 0, distance, 1);
        CHECK (receiver.z > 0);
        CHECK ((clip.z > 0) == (distance < 9916.7f));
        CHECK ((receiver.z - clip.z) == Catch::Approx (.0005f).margin (1e-8));
    }
    // Authored roll and non-uniform scale still preserve the forward axis.
    const glm::mat4 rolled = glm::rotate (world, .7f, glm::vec3 (1, 0, 0))
        * glm::scale (glm::mat4 (1), glm::vec3 (2, 3, 4));
    const auto projected = CLight::calculateSpotProjection (rolled, 45, 10000, false, true)
        * glm::vec4 (0, 0, 2, 1);
    CHECK (std::abs (projected.x / projected.w) < 1e-5f);
    CHECK (std::abs (projected.y / projected.w) < 1e-5f);
}

TEST_CASE ("Reverse directional cascades preserve XY fitting and reverse finite depth", "[lighting][reverse-shadow]") {
    const auto projection = [] (bool reverse) {
        return CLight::calculateDirectionalShadowViewProjection (
            glm::vec3 (0, 0, 5), glm::vec3 (0), glm::vec3 (0, 1, 0), 60,
            16.0f / 9.0f, 1, .1f, 10000, glm::normalize (glm::vec3 (-1, -1, -1)), 512, reverse);
    };
    const auto ordinary = projection (false), reversed = projection (true);
    for (const auto point : { glm::vec3 (0), glm::vec3 (50, -20, -100), glm::vec3 (0, 0, -9990) }) {
        const auto a = ordinary * glm::vec4 (point, 1), b = reversed * glm::vec4 (point, 1);
        CHECK (a.x == b.x);
        CHECK (a.y == b.y);
        CHECK (a.w == b.w);
        CHECK (b.z == Catch::Approx ((a.w - a.z) * .5f).margin (1e-6));
        CHECK (b.z > 0);
        CHECK (b.z < b.w);
    }
}

TEST_CASE ("Directional shadow fitting retains native widths and authored basis snapping", "[lighting][directional-fit]") {
    const glm::vec3 eye (10, .375f, -.625f);
    const glm::mat4 cameraView = glm::lookAt (eye, eye + glm::vec3 (-1, 0, 0), glm::vec3 (0, 1, 0));
    const struct { glm::mat4 world; glm::vec3 snapped; glm::vec4 expected; } cases[] = {
        { glm::mat4 (1), { 9, 0, -.5f }, { .4991540611f, .4991540611f, .4376057386f, 1 } },
        { glm::mat4 (glm::vec4 (2, 0, 0, 0), glm::vec4 (0, 0, 3, 0), glm::vec4 (0, -4, 0, 0),
                     glm::vec4 (123, 456, 789, 1)),
          { 9, .375f, .5f }, { -.4991540611f, .4994291663f, .4376057386f, 1 } }
    };
    for (const auto& value : cases) {
        const glm::mat4 projection = CLight::calculateDirectionalShadowProjection (
            eye, cameraView, value.world, { 4, 4, 10 }, 0, 8);
        const glm::vec4 clip = projection * glm::vec4 (value.snapped + glm::vec3 (1), 1);
        const glm::vec4 center = projection * glm::vec4 (value.snapped, 1);
        for (int component = 0; component < 4; ++component) {
            // Literal oracle supplied independently from native equations, not this implementation.
            CHECK (clip[component] == Catch::Approx (value.expected[component]).margin (0.000001f));
            CHECK (center[component] == Catch::Approx (glm::vec4 (0, 0, .5f, 1)[component]).margin (0.000001f));
        }
    }
    const float scales[] = { .4991540611f, .9983081222f, .1996616244f };
    const float depthScales[] = { -.1247885153f, -.1247885153f, -.0665538748f };
    for (int cascade = 0; cascade < 3; ++cascade) {
        const auto projection = CLight::calculateDirectionalShadowProjection (
            eye, cameraView, glm::mat4 (1), { 4, 2, 10 }, cascade, 8);
        CHECK (projection[2][0] == Catch::Approx (scales[cascade]).margin (0.000001f));
        CHECK (projection[0][2] == Catch::Approx (depthScales[cascade]).margin (0.000001f));
    }
}
