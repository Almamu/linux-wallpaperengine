#include "WallpaperEngine/Render/Wallpapers/CScene.h"

// CEF exposes its own CHECK macro through CScene's renderer includes. Catch must
// own the test assertion macro in this translation unit.
#ifdef CHECK
#undef CHECK
#endif

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "WallpaperEngine/Data/JSON.h"
#include "WallpaperEngine/Data/Model/Project.h"
#include "WallpaperEngine/Data/Model/Property.h"
#include "WallpaperEngine/Data/Model/Wallpaper.h"
#include "WallpaperEngine/Data/Parsers/MaterialParser.h"
#include "WallpaperEngine/Data/Parsers/PropertyParser.h"
#include "WallpaperEngine/Data/Parsers/DynamicValueParser.h"
#include "WallpaperEngine/Data/Parsers/ObjectParser.h"
#include "WallpaperEngine/Data/Parsers/WallpaperParser.h"
#include "WallpaperEngine/FileSystem/Container.h"

using WallpaperEngine::Data::JSON::JSON;
using WallpaperEngine::Data::JSON::parseCompatible;
using namespace WallpaperEngine::Data::Model;
using WallpaperEngine::Data::Parsers::MaterialParser;
using WallpaperEngine::Data::Parsers::ObjectParser;
using WallpaperEngine::Data::Parsers::WallpaperParser;
using WallpaperEngine::FileSystem::Container;
using WallpaperEngine::Render::Wallpapers::CScene;

TEST_CASE ("explicit scene precache acquires material dependencies without an authored layer", "[asset-precache]") {
    auto filesystem = std::make_unique<Container> ();
    filesystem->getVFS ().add ("scene.json", R"({
	"camera":{"center":"0 0 -1","eye":"0 0 0","up":"0 1 0"},
	"general":{"orthogonalprojection":{"width":320,"height":180}},"objects":[],
	"precache":["materials/delayed.json","materials/delayed.json",null,42,"fonts/unused.ttf"]
    })");
    filesystem->getVFS ().add ("materials/delayed.json", R"({
	"passes":[{"shader":"copy","textures":["delayed"]}]
    })");
    Project project {};
    project.type = Project::Type_Scene;
    project.assetLocator = std::make_unique<WallpaperEngine::Assets::AssetLocator> (std::move (filesystem));
    const auto wallpaper = WallpaperParser::parse (JSON ("scene.json"), project);
    const auto* scene = wallpaper->as<Scene> ();
    REQUIRE (scene->objects.empty ());
    REQUIRE (scene->precachedMaterials.size () == 1);
    CHECK (scene->precachedMaterials.front ()->filename == "materials/delayed.json");
    REQUIRE (scene->precachedMaterials.front ()->passes.size () == 1);
    CHECK (scene->precachedMaterials.front ()->passes.front ()->textures.at (0) == "delayed");
}

TEST_CASE ("general precache acquires filtered material dependencies without an authored layer", "[asset-precache]") {
    auto filesystem = std::make_unique<Container> ();
    filesystem->getVFS ().add ("scene.json", R"({
	"camera":{"center":"0 0 -1","eye":"0 0 0","up":"0 1 0"},
	"general":{"orthogonalprojection":{"width":320,"height":180},
	  "precache":["materials/general.json","materials/general.json","materials_suffix.json",
	    null,42,"fonts/unused.ttf","images/unused.tex","models/unused.mdl"]},
	"objects":[]
    })");
    filesystem->getVFS ().add ("materials/general.json", R"({
	"passes":[{"shader":"copy","textures":["general"]}]
    })");
    filesystem->getVFS ().add ("materials_suffix.json", R"({
	"passes":[{"shader":"copy","textures":["prefix"]}]
    })");
    Project project {};
    project.type = Project::Type_Scene;
    project.assetLocator = std::make_unique<WallpaperEngine::Assets::AssetLocator> (std::move (filesystem));
    const auto wallpaper = WallpaperParser::parse (JSON ("scene.json"), project);
    const auto* scene = wallpaper->as<Scene> ();
    REQUIRE (scene->objects.empty ());
    REQUIRE (scene->precachedMaterials.size () == 2);
    CHECK (scene->precachedMaterials.front ()->filename == "materials/general.json");
    REQUIRE (scene->precachedMaterials.front ()->passes.size () == 1);
    CHECK (scene->precachedMaterials.front ()->passes.front ()->textures.at (0) == "general");
    CHECK (scene->precachedMaterials.at (1)->filename == "materials_suffix.json");
    REQUIRE (scene->precachedMaterials.at (1)->passes.size () == 1);
    CHECK (scene->precachedMaterials.at (1)->passes.front ()->textures.at (0) == "prefix");
}

TEST_CASE ("authored general precache takes precedence over the root compatibility field", "[asset-precache]") {
    for (const JSON authored : { JSON::array ({ "materials/general.json" }), JSON::array (), JSON (nullptr), JSON (42) }) {
	auto data = JSON::parse (R"({
	    "camera":{"center":"0 0 -1","eye":"0 0 0","up":"0 1 0"},
	    "general":{"orthogonalprojection":{"width":320,"height":180}},"objects":[],
	    "precache":["materials/legacy.json"]
	})");
	data["general"]["precache"] = authored;
	auto filesystem = std::make_unique<Container> ();
	filesystem->getVFS ().add ("scene.json", data.dump ());
	for (const std::string name : { "general", "legacy" }) {
	    filesystem->getVFS ().add ("materials/" + name + ".json", R"({"passes":[{"shader":"copy"}]})");
	}
	Project project {};
	project.type = Project::Type_Scene;
	project.assetLocator = std::make_unique<WallpaperEngine::Assets::AssetLocator> (std::move (filesystem));
	const auto wallpaper = WallpaperParser::parse (JSON ("scene.json"), project);
	const auto* scene = wallpaper->as<Scene> ();
	REQUIRE (scene->objects.empty ());
	if (authored.is_array () && !authored.empty ()) {
	    REQUIRE (scene->precachedMaterials.size () == 1);
	    CHECK (scene->precachedMaterials.front ()->filename == "materials/general.json");
	} else {
	    CHECK (scene->precachedMaterials.empty ());
	}
    }
}

TEST_CASE ("dynamic strings preserve song titles and script layer names containing spaces") {
    const Properties properties;
    for (const std::string text : { "mxpheebz - The Beach", "Code therapy w / R...", "Song Title", "22:49 PM", "1 2oops" }) {
	const auto value = WallpaperEngine::Data::Parsers::DynamicValueParser::parse (JSON (text), properties, false);
	REQUIRE (value->getType () == DynamicValue::String);
	REQUIRE (value->getString () == text);
    }
    const auto vector = WallpaperEngine::Data::Parsers::DynamicValueParser::parse (JSON ("1.5 -2 3"), properties, false);
    REQUIRE (vector->getType () == DynamicValue::Vec3);
    REQUIRE (vector->getVec3 () == glm::vec3 (1.5f, -2.0f, 3.0f));
}

TEST_CASE ("text parses authored width and row limits") {
    const Project project {};
    const auto object = ObjectParser::parse (JSON::parse (R"({
        "id":353,"name":"Date","text":"Wed","limitwidth":true,"maxwidth":30,
        "limitrows":false,"maxrows":1,"limituseellipsis":false
    })"), project);
    const auto* text = object->as<Text> ();
    REQUIRE (text->limitWidth->value->getBool ());
    REQUIRE (text->maxWidth->value->getFloat () == 30.0f);
    REQUIRE_FALSE (text->limitRows->value->getBool ());
    REQUIRE (text->maxRows->value->getInt () == 1);
    REQUIRE_FALSE (text->limitUseEllipsis->value->getBool ());
}

TEST_CASE ("text depth defaults enabled and normalizes authored and live enum values") {
    const Project project {};
    auto data = JSON::parse (R"({"id":1,"name":"Depth text","text":"M"})");
    const auto omitted = ObjectParser::parse (data, project);
    CHECK (omitted->as<Text> ()->depthTest->value->getString () == "enabled");
    for (const JSON authored : { JSON ("enabled"), JSON ("disabled"), JSON ("bogus"), JSON (true), JSON (nullptr) }) {
	data["depthtest"] = authored;
	const auto object = ObjectParser::parse (data, project);
	auto& value = *object->as<Text> ()->depthTest->value;
	CHECK (value.getType () == DynamicValue::String);
	CHECK (value.getString () == (authored == "enabled" ? "enabled" : "disabled"));
	value.update (std::string ("enabled"), DynamicValue::Script);
	CHECK (value.getString () == "enabled");
	value.update (true, DynamicValue::Script);
	CHECK (value.getType () == DynamicValue::String);
	CHECK (value.getString () == "disabled");
    }
}

TEST_CASE ("text backgrounds retain native defaults and live padding values") {
    const Project project {};
    auto data = JSON::parse (R"({"id":1,"name":"Background text","text":"M"})");
    const auto defaults = ObjectParser::parse (data, project);
    const auto* text = defaults->as<Text> ();
    CHECK_FALSE (text->opaqueBackground->value->getBool ());
    CHECK (text->backgroundColor->value->getVec3 () == glm::vec3 (0.0f));
    CHECK (text->backgroundBrightness->value->getFloat () == 1.0f);
    CHECK (glm::vec2 (text->padding->value->getVec3 ()) == glm::vec2 (32.0f));
    for (const auto& [padding, expected] : std::vector<std::pair<JSON, glm::vec2>> {
	     { JSON (8), glm::vec2 (8) },
	     { JSON (32.75), glm::vec2 (32.75f) },
	     { JSON ("-12 -8"), glm::vec2 (-12, -8) },
	     { JSON (nullptr), glm::vec2 (32) },
	 }) {
	data["padding"] = padding;
	const auto object = ObjectParser::parse (data, project);
	auto& value = *object->as<Text> ()->padding->value;
	CHECK (glm::vec2 (value.getVec3 ()) == expected);
	value.update (glm::vec2 (24, 16), DynamicValue::Script);
	CHECK (glm::vec2 (value.getVec3 ()) == glm::vec2 (24, 16));
    }
}

TEST_CASE ("conditional text depth bindings normalize once and release their subscription") {
    Project project {};
    const auto choice = std::make_shared<PropertyCombo> (
	PropertyData { .name = "choice", .text = "Choice" },
	ComboData { .values = { { "enabled", "On" }, { "disabled", "Off" } } }, "enabled"
    );
    project.properties.emplace ("choice", choice);
    {
	const auto object = ObjectParser::parse (JSON::parse (R"({
	    "id":1,"name":"Depth text","text":"M",
	    "depthtest":{"value":"enabled","user":{"name":"choice","condition":"enabled"},
	      "script":"export function update(value) { return value; }",
	      "scriptproperties":{"label":{"value":"kept"}}}
	})"), project);
	auto& value = *object->as<Text> ()->depthTest->value;
	REQUIRE (value.getType () == DynamicValue::String);
	CHECK (value.getString () == "disabled");
	CHECK (value.getScriptSource ().has_value ());
	CHECK (value.getProperties ().at ("label")->value->getString () == "kept");
	for (const std::string selected : { "disabled", "enabled" }) {
	    choice->update (selected, DynamicValue::User);
	    CHECK (value.getType () == DynamicValue::String);
	    CHECK (value.getString () == "disabled");
	    value.update (std::string ("enabled"), DynamicValue::Script);
	    CHECK (value.getString () == "enabled");
	    value.update (true, DynamicValue::Script);
	    CHECK (value.getType () == DynamicValue::String);
	    CHECK (value.getString () == "disabled");
	}
    }
    choice->update (std::string ("disabled"), DynamicValue::User);
    CHECK (choice->getString () == "disabled");
}

TEST_CASE ("image instances replace material texture defaults without changing sibling instances") {
    auto filesystem = std::make_unique<Container> ();
    filesystem->getVFS ().add (
	"models/instance.json", R"({"material":"materials/instance.json","instanced":true,"width":256,"height":256})"
    );
    filesystem->getVFS ().add (
	"materials/instance.json", R"({"passes":[{"shader":"genericimage4",
	    "textures":["util/white","normal"],"usertextures":["default_albedo","default_normal"]}]})"
    );
    Project project {};
    project.assetLocator = std::make_unique<WallpaperEngine::Assets::AssetLocator> (std::move (filesystem));
    const auto overridden = ObjectParser::parse (JSON::parse (R"({
	"id":613,"name":"seaweed source","image":"models/instance.json",
	"instance":{"textures":["seaweed",null,"extra"],"usertextures":["custom_albedo",null,"custom_extra"]}
    })"), project);
    const auto& pass = *overridden->as<Image> ()->model->material->passes.front ();
    CHECK (pass.userTextureReference == glm::vec2 (256.0f));
    CHECK (pass.textures.at (0) == "seaweed");
    CHECK (pass.textures.at (1) == "normal");
    CHECK (pass.textures.at (2) == "extra");
    CHECK (pass.usertextures.at (0) == "custom_albedo");
    CHECK (pass.usertextures.at (1) == "default_normal");
    CHECK (pass.usertextures.at (2) == "custom_extra");

    const auto sibling = ObjectParser::parse (
	JSON::parse (R"({"id":2,"name":"default source","image":"models/instance.json"})"), project
    );
    const auto& siblingPass = *sibling->as<Image> ()->model->material->passes.front ();
    CHECK (siblingPass.textures.at (0) == "util/white");
    CHECK (siblingPass.usertextures.at (0) == "default_albedo");
}

TEST_CASE ("optional tolerates authored type drift") {
    // workshop 3758354038 authors text "padding" as a vector string where older scenes
    // store a number; a mismatched optional must default, not std::terminate the engine
    const auto data = JSON::parse (R"({"padding": "32.00000 32.00000", "maxrows": 1})");

    REQUIRE (data.optional ("padding", 7) == 7);
    REQUIRE_FALSE (data.optional<int> ("padding").has_value ());
    REQUIRE (data.optional ("maxrows", 0) == 1);
    REQUIRE (data.optional ("missing", 3) == 3);
}

TEST_CASE ("dependencies tolerate the structured authoring form") {
    // workshop 3594400060, 2726424530 and 2787541254 author dependencies as
    // {"id": 104, "index": 0, "type": "collisionmodel"} instead of a bare id. the implicit
    // conversion to int threw type_error.302, and parseDependencies runs in both the parse
    // attempt and its fallback, so the wallpaper died outright
    const auto data = JSON::parse (
	R"({"id": 12, "name": "thing", "solid": true,
	    "dependencies": [7, {"id": 104, "index": 0, "type": "collisionmodel"}, {"index": 1}]})"
    );
    const Project project {};

    const auto object = ObjectParser::parse (data, project);
    REQUIRE (object->dependencies == std::vector {7, 104});
}

TEST_CASE ("layer type keys authored as null do not claim the object") {
    // workshop 765030095 writes every layer-type key on its text layers and nulls the
    // unused ones. "particle": null routed the clock into parseParticle, which produced a
    // Particle with no material, and CParticle dereferenced it on construction
    const auto data = JSON::parse (
	R"({"id": 185, "name": "Clock", "image": null, "model": null, "particle": null,
	    "text": "12:00"})"
    );
    const Project project {};

    const auto object = ObjectParser::parse (data, project);
    REQUIRE (object != nullptr);
    REQUIRE (object->is<Text> ());
}

TEST_CASE ("orthographic camera layers retain their own animated projection settings") {
    auto filesystem = std::make_unique<Container> ();
    filesystem->getVFS ().add (
	"scene.json",
	R"({
	    "camera": {"center":"0 0 -1", "eye":"0 0 0", "up":"0 1 0"},
	    "general": {"orthogonalprojection":{"width":3840,"height":2160}},
	    "objects": [{
		"id":1640, "name":"opening camera", "camera":"default",
		"visible":{"value":true},
		"origin":{"value":"0 0 500"},
		"zoom":{"value":3,"animation":{
		    "c0":[{"frame":0,"value":3},{"frame":36,"value":1}],
		    "options":{"fps":12,"length":60,"mode":"single"}
		}}
	    }, {
		"id":309, "name":"disabled panorama camera", "camera":"default",
		"visible":{"value":false}, "zoom":0.75
	    }]
	})"
    );

    Project project {};
    project.type = Project::Type_Scene;
    project.assetLocator = std::make_unique<WallpaperEngine::Assets::AssetLocator> (std::move (filesystem));

    const auto wallpaper = WallpaperParser::parse (JSON ("scene.json"), project);
    REQUIRE (wallpaper->is<Scene> ());
    const auto* scene = wallpaper->as<Scene> ();
    REQUIRE (scene->camera.objectIds == std::vector { 1640, 309 });
    REQUIRE (scene->camera.projection.zoom->animation == nullptr);
    CHECK (scene->camera.projection.zoom->evaluateFloat (0.0f) == Catch::Approx (1.0f));
    REQUIRE (scene->camera.objectProjections.size () == 2);
    REQUIRE (scene->camera.objectProjections[0].zoom->animation != nullptr);
    CHECK (scene->camera.objectProjections[0].zoom->evaluateFloat (0.0f) == Catch::Approx (3.0f));
    CHECK (scene->camera.objectProjections[0].zoom->evaluateFloat (3.0f) == Catch::Approx (1.0f));
    REQUIRE (scene->camera.objectProjections[1].zoom->animation == nullptr);
    CHECK (scene->camera.objectProjections[1].zoom->evaluateFloat (0.0f) == Catch::Approx (0.75f));
}

TEST_CASE ("Scene zoom honors general settings in both 2D and 3D wallpapers") {
    const auto parsedZoom = [] (const std::string& general, const std::string& cameraFields) {
	auto filesystem = std::make_unique<Container> ();
	filesystem->getVFS ().add (
	    "scene.json",
	    "{\"camera\":{\"center\":\"0 0 -1\",\"eye\":\"0 0 0\",\"up\":\"0 1 0\"" + cameraFields
		+ "},\"general\":" + general + ",\"objects\":[]}"
	);
	Project project {};
	project.type = Project::Type_Scene;
	project.assetLocator = std::make_unique<WallpaperEngine::Assets::AssetLocator> (std::move (filesystem));
	const auto wallpaper = WallpaperParser::parse (JSON ("scene.json"), project);
	return wallpaper->as<Scene> ()->camera.projection.zoom->evaluateFloat (0.0f);
    };
    // Stratospheric Twilight / Dark Leaf author the overscan here, not in camera.
    CHECK (
	parsedZoom (R"({"orthogonalprojection":{"width":3840,"height":2160},"zoom":1.03})", "") == Catch::Approx (1.03f)
    );
    CHECK (
	parsedZoom (R"({"orthogonalprojection":{"width":3840,"height":2160},"zoom":1.05})", ",\"zoom\":2")
	== Catch::Approx (1.05f)
    );
    CHECK (
	parsedZoom (R"({"orthogonalprojection":{"width":1920,"height":1080}})", ",\"zoom\":1.2") == Catch::Approx (1.2f)
    );
    CHECK (parsedZoom (R"({"orthogonalprojection":{"width":1920,"height":1080}})", "") == Catch::Approx (1.0f));
    CHECK (parsedZoom (R"({"orthogonalprojection":null,"zoom":1.4})", ",\"zoom\":2") == Catch::Approx (1.4f));
}

TEST_CASE ("perspective scenes retain depth precision when the near plane is omitted", "[scene-defaults]") {
    const auto parsedNear = [] (const std::string& general) {
	auto filesystem = std::make_unique<Container> ();
	filesystem->getVFS ().add (
	    "scene.json", "{\"camera\":{\"center\":\"0 0 -1\",\"eye\":\"0 0 0\",\"up\":\"0 1 0\"},"
		"\"general\":" + general + ",\"objects\":[]}"
	);
	Project project {};
	project.type = Project::Type_Scene;
	project.assetLocator = std::make_unique<WallpaperEngine::Assets::AssetLocator> (std::move (filesystem));
	const auto wallpaper = WallpaperParser::parse (JSON ("scene.json"), project);
	return wallpaper->as<Scene> ()->camera.projection.nearz->evaluateFloat (0.0f);
    };
    // A zero default was clamped to 0.0001 by Camera, collapsing depth precision
    // on older scenes that omit projection parameters entirely.
    CHECK (parsedNear (R"({})") == Catch::Approx (0.1f));
    CHECK (parsedNear (R"({"orthogonalprojection":null})") == Catch::Approx (0.1f));
    CHECK (parsedNear (R"({"nearz":0.025})") == Catch::Approx (0.025f));
    CHECK (parsedNear (R"({"orthogonalprojection":{"width":1920,"height":1080}})") == 0.0f);
}

TEST_CASE ("2D scenes read projection planes and fov from general", "[scene-defaults]") {
    const auto parsed = [] (const std::string& general, const std::string& cameraFields) {
	auto filesystem = std::make_unique<Container> ();
	filesystem->getVFS ().add (
	    "scene.json",
	    "{\"camera\":{\"center\":\"0 0 -1\",\"eye\":\"0 0 0\",\"up\":\"0 1 0\"" + cameraFields
		+ "},\"general\":" + general + ",\"objects\":[]}"
	);
	Project project {};
	project.type = Project::Type_Scene;
	project.assetLocator = std::make_unique<WallpaperEngine::Assets::AssetLocator> (std::move (filesystem));
	const auto wallpaper = WallpaperParser::parse (JSON ("scene.json"), project);
	const auto& projection = wallpaper->as<Scene> ()->camera.projection;
	return glm::vec4 (
	    projection.nearz->evaluateFloat (0.0f), projection.farz->evaluateFloat (0.0f),
	    projection.fov->evaluateFloat (0.0f), projection.perspectiveOverrideFov->evaluateFloat (0.0f)
	);
    };
    // Native applies the general table (nearz/farz/fov) to every scene. Space Balls
    // (3594400060) lost its perspective particles to the old camera-block defaults:
    // a zero near plane collapses every particle onto the far plane.
    const glm::vec4 authored = parsed (
	R"({"orthogonalprojection":{"width":3840,"height":2160},"nearz":0.01,"farz":10000,"fov":40,"perspectiveoverridefov":30})", ""
    );
    CHECK (authored.x == Catch::Approx (0.01f));
    CHECK (authored.y == Catch::Approx (10000.0f));
    CHECK (authored.z == Catch::Approx (40.0f));
    CHECK (authored.w == Catch::Approx (30.0f));
    // Camera-block values remain a fallback for scenes that only author them there.
    const glm::vec4 legacy = parsed (
	R"({"orthogonalprojection":{"width":1920,"height":1080}})", R"(,"nearz":0.5,"farz":2000,"fov":60)"
    );
    CHECK (legacy.x == Catch::Approx (0.5f));
    CHECK (legacy.y == Catch::Approx (2000.0f));
    CHECK (legacy.z == Catch::Approx (60.0f));
    CHECK (legacy.w == Catch::Approx (95.0f));
    CHECK (parsed (R"({"perspectiveoverridefov":{"value":120}})", "").w == Catch::Approx (120.0f));
}

TEST_CASE ("scene bloom defaults match native LDR settings without replacing authored values", "[scene-defaults][bloom]") {
    struct BloomCase {
	const char* general;
	bool enabled;
	float strength;
	float threshold;
	glm::vec3 tint;
    };
    const BloomCase cases[] = {
	{ R"({})", false, 2.0f, 0.65f, glm::vec3 (1.0f) },
	{ R"({"bloom":true})", true, 2.0f, 0.65f, glm::vec3 (1.0f) },
	{ R"({"bloom":false})", false, 2.0f, 0.65f, glm::vec3 (1.0f) },
	{ R"({"bloom":true,"bloomstrength":0,"bloomthreshold":0})", true, 0.0f, 0.0f, glm::vec3 (1.0f) },
	{ R"({"bloom":true,"bloomstrength":1.25,"bloomthreshold":0.4,"bloomtint":"0.25 0.5 0.75"})",
	    true, 1.25f, 0.4f, { 0.25f, 0.5f, 0.75f } },
	{ R"({"bloom":false,"bloomstrength":3.5,"bloomthreshold":0.8,"bloomtint":"0 0 0"})",
	    false, 3.5f, 0.8f, glm::vec3 (0.0f) },
    };
    for (const auto& [general, enabled, strength, threshold, tint] : cases) {
	CAPTURE (general);
	auto filesystem = std::make_unique<Container> ();
	filesystem->getVFS ().add (
	    "scene.json", std::string ("{\"camera\":{\"center\":\"0 0 -1\",\"eye\":\"0 0 0\",\"up\":\"0 1 0\"},"
		"\"general\":") + general + ",\"objects\":[]}"
	);
	Project project {};
	project.type = Project::Type_Scene;
	project.assetLocator = std::make_unique<WallpaperEngine::Assets::AssetLocator> (std::move (filesystem));
	const auto wallpaper = WallpaperParser::parse (JSON ("scene.json"), project);
	REQUIRE (wallpaper->is<Scene> ());
	const auto& bloom = wallpaper->as<Scene> ()->camera.bloom;
	CHECK (bloom.enabled->value->getBool () == enabled);
	CHECK (bloom.strength->evaluateFloat (0.0f) == Catch::Approx (strength));
	CHECK (bloom.threshold->evaluateFloat (0.0f) == Catch::Approx (threshold));
	CHECK (bloom.tint->value->getVec3 () == tint);
    }
}

TEST_CASE ("HDR bloom keeps native defaults and authored zero values", "[scene-defaults][hdr]") {
    for (const bool authored : { false, true }) {
        auto filesystem = std::make_unique<Container> ();
        const std::string general = authored
            ? R"({"hdr":true,"bloom":true,"bloomhdrstrength":0,"bloomhdrthreshold":0,"bloomhdrfeather":0,"bloomhdrscatter":0,"bloomhdriterations":1,"bloomtint":"0 0 0"})"
            : "{}";
        filesystem->getVFS ().add ("scene.json",
            R"({"camera":{"center":"0 0 -1","eye":"0 0 0","up":"0 1 0"},"general":)"
            + general + R"(,"objects":[]})");
        Project project {};
        project.type = Project::Type_Scene;
        project.assetLocator = std::make_unique<WallpaperEngine::Assets::AssetLocator> (std::move (filesystem));
        const auto wallpaper = WallpaperParser::parse (JSON ("scene.json"), project);
        const auto& bloom = wallpaper->as<Scene> ()->camera.bloom.hdr;
        CHECK (bloom.strength->evaluateFloat (0) == Catch::Approx (authored ? 0 : 2));
        CHECK (bloom.threshold->evaluateFloat (0) == Catch::Approx (authored ? 0 : 1));
        CHECK (bloom.feather->evaluateFloat (0) == Catch::Approx (authored ? 0 : 0.1f));
        CHECK (bloom.scatter->evaluateFloat (0) == Catch::Approx (authored ? 0 : 1.619f));
        CHECK (bloom.iterations->evaluateFloat (0) == Catch::Approx (authored ? 1 : 8));
        CHECK (wallpaper->as<Scene> ()->camera.bloom.tint->evaluateVec3 (0) == glm::vec3 (authored ? 0 : 1));
    }
}

TEST_CASE ("missing image effects are skipped without discarding neighboring effects") {
    auto filesystem = std::make_unique<Container> ();
    filesystem->getVFS ().add ("effects/before.json", R"({"name":"before","passes":[]})");
    filesystem->getVFS ().add ("effects/after.json", R"({"name":"after","passes":[]})");

    Project project {};
    project.assetLocator = std::make_unique<WallpaperEngine::Assets::AssetLocator> (std::move (filesystem));

    const auto data = JSON::parse (
	R"({
	    "id": 42,
	    "name": "text with optional effects",
	    "text": "still visible",
	    "effects": [
		{"id": 1, "file": "effects/before.json"},
		{"id": 2, "file": "effects/missing-workshop-dependency.json"},
		{"id": 3},
		{"id": 4, "file": "effects/after.json"}
	    ]
	})"
    );

    const auto object = ObjectParser::parse (data, project);
    REQUIRE (object->is<Text> ());
    const auto* text = object->as<Text> ();
    REQUIRE (text->effects.size () == 2);
    CHECK (text->effects[0]->effect->name == "before");
    CHECK (text->effects[1]->effect->name == "after");
}

TEST_CASE ("image composition layers retain copybackground") {
    const auto parseLayer = [] (const std::string& objectJson) {
	auto filesystem = std::make_unique<Container> ();
	filesystem->getVFS ().add (
	    "models/util/composelayer.json",
	    R"({"material":"materials/util/composelayer.json","passthrough":true})"
	);
	filesystem->getVFS ().add (
	    "materials/util/composelayer.json",
	    R"({"passes":[{"shader":"composelayer","textures":["_rt_FullFrameBuffer"]}]})"
	);
	Project project {};
	project.assetLocator
	    = std::make_unique<WallpaperEngine::Assets::AssetLocator> (std::move (filesystem));
	return ObjectParser::parse (JSON::parse (objectJson), project);
    };

    const auto isolated = parseLayer (
	R"({"id":1,"name":"isolated","image":"models/util/composelayer.json","copybackground":false})"
    );
    const auto copied = parseLayer (
	R"({"id":2,"name":"copied","image":"models/util/composelayer.json","copybackground":true})"
    );
    const auto omitted
	= parseLayer (R"({"id":3,"name":"omitted","image":"models/util/composelayer.json"})");

    REQUIRE (isolated->is<Image> ());
    REQUIRE_FALSE (isolated->as<Image> ()->copyBackground);
    REQUIRE (copied->is<Image> ());
    REQUIRE (copied->as<Image> ()->copyBackground);
    REQUIRE (omitted->is<Image> ());
    REQUIRE (omitted->as<Image> ()->copyBackground);
}

TEST_CASE ("composition scope distinguishes child groups from full-frame stack effects") {
    Project project {};
    SceneData scene {};
    scene.objects.emplace_back (
	ObjectParser::parse (JSON::parse (R"({"id":1,"name":"group","solid":true})"), project)
    );
    scene.objects.emplace_back (ObjectParser::parse (
	JSON::parse (R"({"id":2,"name":"child","parent":1,"solid":true})"), project
    ));
    scene.objects.emplace_back (
	ObjectParser::parse (JSON::parse (R"({"id":3,"name":"flat","solid":true})"), project)
    );

    CHECK (CScene::hasAuthoredChildren (scene, 1));
    CHECK_FALSE (CScene::hasAuthoredChildren (scene, 2));
    CHECK_FALSE (CScene::hasAuthoredChildren (scene, 3));
}

TEST_CASE ("alpha-to-coverage material blending is preserved") {
    REQUIRE (MaterialParser::parseBlendMode ("alphatocoverage") == BlendingMode_AlphaToCoverage);
}

TEST_CASE ("omitted depth state uses 3D model defaults only in model context") {
    const auto material = JSON::parse (R"({"passes":[{"shader":"generic4"}]})");
    const auto explicitDisabled = JSON::parse (
	R"({"passes":[{"shader":"generic4","depthtest":"disabled","depthwrite":"disabled"}]})"
    );
    const Project project {};

    const auto imageMaterial = MaterialParser::parse (material, "image.json", project);
    REQUIRE (imageMaterial->passes.front ()->depthtest == DepthtestMode_Disabled);
    REQUIRE (imageMaterial->passes.front ()->depthwrite == DepthwriteMode_Disabled);

    const auto modelMaterial = MaterialParser::parse (material, "model.json", project, true);
    REQUIRE (modelMaterial->passes.front ()->depthtest == DepthtestMode_Enabled);
    REQUIRE (modelMaterial->passes.front ()->depthwrite == DepthwriteMode_Enabled);

    const auto overrideMaterial = MaterialParser::parse (explicitDisabled, "model.json", project, true);
    REQUIRE (overrideMaterial->passes.front ()->depthtest == DepthtestMode_Disabled);
    REQUIRE (overrideMaterial->passes.front ()->depthwrite == DepthwriteMode_Disabled);
}

TEST_CASE ("omitted culling rejects model backfaces without changing overlay materials", "[scene-defaults]") {
    const auto omitted = JSON::parse (R"({"passes":[{"shader":"generic"}]})");
    const auto explicitDisabled = JSON::parse (R"({"passes":[{"shader":"generic","cullmode":"nocull"}]})");
    const Project project {};
    const auto model = MaterialParser::parse (omitted, "model.json", project, true);
    const auto image = MaterialParser::parse (omitted, "image.json", project);
    const auto doubleSided = MaterialParser::parse (explicitDisabled, "model.json", project, true);
    CHECK (model->passes.front ()->cullmode == CullingMode_Normal);
    CHECK (image->passes.front ()->cullmode == CullingMode_Disable);
    CHECK (doubleSided->passes.front ()->cullmode == CullingMode_Disable);
}

TEST_CASE ("Wallpaper Engine JSON comments and trailing commas are accepted narrowly") {
    const auto data = parseCompatible (
	R"json({
	    // Comments may appear between values.
	    "array": [1, /* inline */ 2,],
	    "lineCommentBeforeClose": [1, // still a trailing comma
	    ],
	    "object": {"value": 3, /* and between a trailing comma and its close */},
	    "literal": ",] // not a comment /* either */",
	})json",
	"test.json"
    );

    REQUIRE (data["array"].size () == 2);
    REQUIRE (data["lineCommentBeforeClose"].size () == 1);
    REQUIRE (data["object"]["value"] == 3);
    REQUIRE (data["literal"] == ",] // not a comment /* either */");
    REQUIRE_THROWS_AS (parseCompatible (R"({"still":"broken",oops})"), JSON::parse_error);
    REQUIRE_THROWS_AS (parseCompatible (R"({"unterminated": true /* comment})"), JSON::parse_error);
}

TEST_CASE ("legacy material user shader values bind live project properties", "[material-users]") {
    Project project {};
    project.properties.emplace ("scheme", WallpaperEngine::Data::Parsers::PropertyParser::parse (
        JSON::parse (R"({"type":"color","value":"0 0.4 0.7"})"), "scheme"));
    const auto material = MaterialParser::parse (JSON::parse (R"({"passes":[{
        "shader":"generic", "constantshadervalues":{"tint":"1 1 1","opacity":0.75},
        "usershadervalues":{"scheme":"tint","missing":"opacity"}
    }]})"), "legacy.json", project);
    const auto& values = material->passes.front ()->constants;
    REQUIRE (values.at ("tint")->value->getVec3 () == glm::vec3 (0, 0.4f, 0.7f));
    REQUIRE (values.at ("opacity")->value->getFloat () == 0.75f);
    project.properties.at ("scheme")->update (glm::vec3 (0.2f, 0.6f, 0.1f), DynamicValue::UpdateSource::Script);
    REQUIRE (values.at ("tint")->value->getVec3 () == glm::vec3 (0.2f, 0.6f, 0.1f));
}

TEST_CASE ("shader comma vectors preserve text and wrapped property bindings", "[material-vectors]") {
    Project project {};
    project.properties.emplace ("scheme", WallpaperEngine::Data::Parsers::PropertyParser::parse (
        JSON::parse (R"({"type":"color","value":"0 0.4 0.7"})"), "scheme"));
    const auto material = MaterialParser::parse (JSON::parse (R"({"passes":[{
        "shader":"generic", "constantshadervalues":{
            "angles":"0.0, 360.0", "offset":{"value":"1, -2, 3"}, "rect":"0,0,0.4,1",
            "label":"Song, Title", "invalid":"0.0, 360oops", "scalar":"0.4",
            "incompleteExponent":"1, 2, 3e", "overflowExponent":"1, 2, 1e9999",
            "tint":{"value":"1, 0, 0","user":"scheme","script":"export function update(value) { return value; }"}
        }
    }]})"), "comma.json", project);
    const auto& values = material->passes.front ()->constants;
    REQUIRE (values.at ("angles")->value->getVec2 () == glm::vec2 (0, 360));
    REQUIRE (values.at ("offset")->value->getVec3 () == glm::vec3 (1, -2, 3));
    REQUIRE (values.at ("rect")->value->getVec4 () == glm::vec4 (0, 0, 0.4f, 1));
    REQUIRE (values.at ("label")->value->getString () == "Song, Title");
    REQUIRE (values.at ("invalid")->value->getString () == "0.0, 360oops");
    CHECK (values.at ("incompleteExponent")->value->getString () == "1, 2, 3e");
    CHECK (values.at ("overflowExponent")->value->getString () == "1, 2, 1e9999");
    REQUIRE (values.at ("scalar")->value->getFloat () == 0.4f);
    REQUIRE (values.at ("tint")->value->getVec3 () == glm::vec3 (0, 0.4f, 0.7f));
    REQUIRE (values.at ("tint")->value->getScriptSource () == "export function update(value) { return value; }");
    project.properties.at ("scheme")->update (glm::vec3 (0.2f, 0.6f, 0.1f), DynamicValue::UpdateSource::Script);
    REQUIRE (values.at ("tint")->value->getVec3 () == glm::vec3 (0.2f, 0.6f, 0.1f));
}

TEST_CASE ("generic layers preserve unrecognized configuration and live visibility", "[volume-admission]") {
    const Project project {};
    const auto data = JSON::parse (R"({
        "id":91,"name":"Generic barrier","visible":false,"parent":90,
        "customMetadata":{"kept":true}
    })");
    const auto object = ObjectParser::parse (data, project);
    CHECK (typeid (*object) == typeid (Object));
    CHECK_FALSE (object->cameraLayer);
    CHECK (object->parent == 90);
    CHECK_FALSE (object->groupVisible->value->getBool ());
    CHECK (JSON::parse (object->initialConfiguration) == data);
    object->groupVisible->value->update (true, DynamicValue::Script);
    CHECK (object->groupVisible->value->getBool ());
}

TEST_CASE ("image LED sources retain native defaults and live boolean state", "[volume-admission]") {
    auto filesystem = std::make_unique<Container> ();
    filesystem->getVFS ().add ("models/source.json", R"({"material":"materials/source.json"})");
    filesystem->getVFS ().add ("materials/source.json", R"({"passes":[{"shader":"copy"}]})");
    Project project {};
    project.assetLocator = std::make_unique<WallpaperEngine::Assets::AssetLocator> (std::move (filesystem));
    auto data = JSON::parse (R"({"id":92,"name":"LED source","image":"models/source.json"})");
    const auto defaults = ObjectParser::parse (data, project);
    CHECK_FALSE (defaults->as<Image> ()->ledSource->value->getBool ());
    data["ledsource"] = true;
    const auto enabled = ObjectParser::parse (data, project);
    auto& source = *enabled->as<Image> ()->ledSource->value;
    CHECK (source.getBool ());
    source.update (false, DynamicValue::Script);
    CHECK_FALSE (source.getBool ());
}

TEST_CASE ("camera admission remains false for standalone layer parsing", "[volume-admission]") {
    const Project project {};
    auto data = JSON::parse (R"({"id":93,"name":"Runtime camera","visible":true})");
    for (const JSON camera : { JSON ("default"), JSON (""), JSON (nullptr), JSON (true), JSON (7),
                               JSON::object (), JSON::array () }) {
        data["camera"] = camera;
        const auto object = ObjectParser::parse (data, project);
        CHECK (object->cameraLayer == camera.is_string ());
        CHECK (object->groupVisible->value->getBool ());
    }
}
