#include "WallpaperEngine/Render/CObject.h"
#include "WallpaperEngine/Testing/Harnesses/RenderHarness.h"

// CEF headers (included indirectly via CObject.h -> CScene.h -> CWallpaper.h) define
// a CHECK(condition) logging macro that conflicts with Catch2's CHECK(...) test assertion macro.
// Undefining CHECK here allows Catch2's assertion macro to take precedence within this unit test.
#ifdef CHECK
#undef CHECK
#endif

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

// #include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <map>
#include <memory>

using namespace WallpaperEngine;
using namespace WallpaperEngine::Data::Model;
using namespace WallpaperEngine::Render;
using namespace WallpaperEngine::Render::Wallpapers;
using Catch::Approx;

namespace {
UserSettingUniquePtr makeVec3Setting (const glm::vec3& val) {
    return std::make_unique<UserSetting> (UserSetting {
	.value = std::make_unique<DynamicValue> (val),
	.property = nullptr,
	.condition = std::nullopt,
    });
}

UserSettingUniquePtr makeFloatSetting (float val) {
    return std::make_unique<UserSetting> (UserSetting {
	.value = std::make_unique<DynamicValue> (val),
	.property = nullptr,
	.condition = std::nullopt,
    });
}

UserSettingUniquePtr makeBoolSetting (bool val) {
    return std::make_unique<UserSetting> (UserSetting {
	.value = std::make_unique<DynamicValue> (val),
	.property = nullptr,
	.condition = std::nullopt,
    });
}

ObjectData makeObjectData (
    int id, const glm::vec3& origin, const glm::vec3& scale, const glm::vec3& angles,
    std::optional<int> parent = std::nullopt
) {
    return ObjectData {
	.id = id,
	.name = "Object_" + std::to_string (id),
	.dependencies = {},
	.parent = parent,
	.origin = makeVec3Setting (origin),
	.groupScale = makeVec3Setting (scale),
	.groupAngles = makeVec3Setting (angles),
	.groupVisible = nullptr,
    };
}

std::unique_ptr<Image> makeImageObject (
    int id, const glm::vec3& origin, const glm::vec3& scale, const glm::vec3& angles,
    std::optional<int> parent = std::nullopt
) {
    ObjectData objData = makeObjectData (id, origin, scale, angles, parent);
    ImageData imgData {
	.scale = makeVec3Setting (scale),
	.angles = makeVec3Setting (angles),
	.visible = nullptr,
	.alpha = nullptr,
	.color = nullptr,
	.alignment = "center",
	.size = { 100.0f, 100.0f },
	.parallaxDepth = nullptr,
	.colorBlendMode = nullptr,
	.brightness = nullptr,
	.model = nullptr,
	.effects = {},
	.animationLayers = {},
    };
    return std::make_unique<Image> (std::move (objData), std::move (imgData));
}

std::unique_ptr<Text> makeTextObject (
    int id, const glm::vec3& origin, const glm::vec3& scale, const glm::vec3& angles,
    std::optional<int> parent = std::nullopt
) {
    ObjectData objData = makeObjectData (id, origin, scale, angles, parent);
    TextData txtData {
	.text = nullptr,
	.font = "",
	.pointSize = nullptr,
	.size = { 100.0f, 50.0f },
	.scale = makeVec3Setting (scale),
	.color = nullptr,
	.alpha = nullptr,
	.visible = nullptr,
	.alignment = "center",
	.verticalalign = "center",
	.padding = 0,
    };
    return std::make_unique<Text> (std::move (objData), std::move (txtData));
}
} // namespace

TEST_CASE ("Transform Resolution: Local transform extraction for generic objects") {
    const Object obj (makeObjectData (1, { 100.0f, 200.0f, 0.0f }, { 2.0f, 3.0f, 1.0f }, { 0.0f, 0.0f, 0.5f }));
    const glm::mat4 localMat = CObject::localModelMatrix (obj);

    CHECK (localMat[3][0] == Approx (100.0f));
    CHECK (localMat[3][1] == Approx (200.0f));
    CHECK (glm::length (glm::vec3 (localMat[0])) == Approx (2.0f));
    CHECK (glm::length (glm::vec3 (localMat[1])) == Approx (3.0f));
}

TEST_CASE ("Transform Resolution: Nested non-uniform scale plus rotation full matrix validation") {
    const glm::vec3 parentOrigin = { 0.0f, 0.0f, 0.0f };
    const glm::vec3 parentScale = { 2.0f, 1.0f, 1.0f };
    const glm::vec3 parentAngles = { 0.0f, 0.0f, 0.0f };

    const glm::vec3 childOrigin = { 10.0f, 5.0f, 0.0f };
    const glm::vec3 childScale = { 1.0f, 1.0f, 1.0f };
    const glm::vec3 childAngles = { 0.0f, 0.0f, glm::radians (90.0f) };

    const Object parentObj (makeObjectData (1, parentOrigin, parentScale, parentAngles));
    const Object childObj (makeObjectData (2, childOrigin, childScale, childAngles, 1));

    std::map<int, const Object*> objects = { { 1, &parentObj }, { 2, &childObj } };
    auto lookup = [&objects] (int id) -> const Object* {
	auto it = objects.find (id);
	return it != objects.end () ? it->second : nullptr;
    };

    // Independently constructed expected affine matrix from raw fixture inputs
    const glm::mat4 expectedParent
	= glm::translate (glm::mat4 (1.0f), parentOrigin) * glm::scale (glm::mat4 (1.0f), parentScale);
    const glm::mat4 expectedChild = glm::translate (glm::mat4 (1.0f), childOrigin)
	* glm::rotate (glm::mat4 (1.0f), childAngles.z, glm::vec3 (0.0f, 0.0f, 1.0f))
	* glm::scale (glm::mat4 (1.0f), childScale);
    const glm::mat4 expectedWorld = expectedParent * expectedChild;

    // Obtain actual resulting matrix from production transform API
    const glm::mat4 actualWorld = CObject::resolveModelMatrix (childObj, lookup);

    // Compare all 16 matrix elements
    for (int col = 0; col < 4; ++col) {
	for (int row = 0; row < 4; ++row) {
	    CHECK (actualWorld[col][row] == Approx (expectedWorld[col][row]).margin (1e-4));
	}
    }
}

TEST_CASE ("Transform Resolution: Real Image model path hierarchy matrix resolution") {
    const glm::vec3 parentOrigin = { 100.0f, 200.0f, 0.0f };
    const glm::vec3 parentScale = { 2.0f, 1.0f, 1.0f };
    const glm::vec3 parentAngles = { 0.0f, 0.0f, 0.0f };

    const glm::vec3 childOrigin = { 50.0f, 30.0f, 0.0f };
    const glm::vec3 childScale = { 0.5f, 0.5f, 1.0f };
    const glm::vec3 childAngles = { 0.0f, 0.0f, glm::radians (45.0f) };

    const auto parentImg = makeImageObject (1, parentOrigin, parentScale, parentAngles);
    const auto childImg = makeImageObject (2, childOrigin, childScale, childAngles, 1);

    std::map<int, const Object*> objects = { { 1, parentImg.get () }, { 2, childImg.get () } };
    auto lookup = [&objects] (int id) -> const Object* {
	auto it = objects.find (id);
	return it != objects.end () ? it->second : nullptr;
    };

    const glm::mat4 expectedParent
	= glm::translate (glm::mat4 (1.0f), parentOrigin) * glm::scale (glm::mat4 (1.0f), parentScale);
    const glm::mat4 expectedChild = glm::translate (glm::mat4 (1.0f), childOrigin)
	* glm::rotate (glm::mat4 (1.0f), childAngles.z, glm::vec3 (0.0f, 0.0f, 1.0f))
	* glm::scale (glm::mat4 (1.0f), childScale);
    const glm::mat4 expectedWorld = expectedParent * expectedChild;

    const glm::mat4 actualWorld = CObject::resolveModelMatrix (*childImg, lookup);

    for (int col = 0; col < 4; ++col) {
	for (int row = 0; row < 4; ++row) {
	    CHECK (actualWorld[col][row] == Approx (expectedWorld[col][row]).margin (1e-4));
	}
    }
}

TEST_CASE ("Transform Resolution: Real Text model path hierarchy matrix resolution") {
    const glm::vec3 parentOrigin = { 500.0f, 300.0f, 0.0f };
    const glm::vec3 parentScale = { 2.0f, 1.0f, 1.0f };
    const glm::vec3 parentAngles = { 0.0f, 0.0f, 0.0f };

    const glm::vec3 childOrigin = { 80.0f, 40.0f, 0.0f };
    const glm::vec3 childScale = { 4.0f, 4.0f, 1.0f };
    const glm::vec3 childAngles = { 0.0f, 0.0f, glm::radians (45.0f) };

    const Object parent (makeObjectData (1, parentOrigin, parentScale, parentAngles));
    const auto childText = makeTextObject (2, childOrigin, childScale, childAngles, 1);

    std::map<int, const Object*> objects = { { 1, &parent }, { 2, childText.get () } };
    auto lookup = [&objects] (int id) -> const Object* {
	auto it = objects.find (id);
	return it != objects.end () ? it->second : nullptr;
    };

    const glm::mat4 expectedParent
	= glm::translate (glm::mat4 (1.0f), parentOrigin) * glm::scale (glm::mat4 (1.0f), parentScale);
    const glm::mat4 expectedChild = glm::translate (glm::mat4 (1.0f), childOrigin)
	* glm::rotate (glm::mat4 (1.0f), childAngles.z, glm::vec3 (0.0f, 0.0f, 1.0f))
	* glm::scale (glm::mat4 (1.0f), childScale);
    const glm::mat4 expectedWorld = expectedParent * expectedChild;

    const glm::mat4 actualWorld = CObject::resolveModelMatrix (*childText, lookup);

    for (int col = 0; col < 4; ++col) {
	for (int row = 0; row < 4; ++row) {
	    CHECK (actualWorld[col][row] == Approx (expectedWorld[col][row]).margin (1e-4));
	}
    }
}

TEST_CASE ("Transform Resolution: Safe fallback on cyclic parent dependencies") {
    // Object A has parent B (ID 2), Object B has parent A (ID 1)
    const Object objA (makeObjectData (1, { 10.0f, 20.0f, 0.0f }, { 1.0f, 1.0f, 1.0f }, { 0.0f, 0.0f, 0.0f }, 2));
    const Object objB (makeObjectData (2, { 30.0f, 40.0f, 0.0f }, { 1.0f, 1.0f, 1.0f }, { 0.0f, 0.0f, 0.0f }, 1));

    std::map<int, const Object*> objects = { { 1, &objA }, { 2, &objB } };
    auto lookup = [&objects] (int id) -> const Object* {
	auto it = objects.find (id);
	return it != objects.end () ? it->second : nullptr;
    };

    // Must safely detect cycle and return objA's local transform
    const glm::mat4 resolved = CObject::resolveModelMatrix (objA, lookup);
    CHECK (resolved[3][0] == Approx (10.0f));
    CHECK (resolved[3][1] == Approx (20.0f));
}

TEST_CASE ("Scene Construction: Cyclic parent dependency in CScene object creation terminates safely") {
    auto harness = std::unique_ptr<WallpaperEngine::Testing::Harnesses::RenderHarness> (
	WallpaperEngine::Testing::Harnesses::RenderHarness::build ("")
    );

    Project project;
    project.assetLocator = std::make_unique<AssetLocator> (
	std::make_unique<WallpaperEngine::FileSystem::Container> ()
    );
    WallpaperData wpData { .filename = "scene.json", .project = project };
    SceneData sceneData {};
    sceneData.camera.configuration.center = { 0.0f, 0.0f, 0.0f };
    sceneData.camera.configuration.eye = { 0.0f, 0.0f, 1.0f };
    sceneData.camera.configuration.up = { 0.0f, 1.0f, 0.0f };
    sceneData.camera.projection.width = 1920;
    sceneData.camera.projection.height = 1080;
    sceneData.camera.projection.isAuto = false;
    sceneData.camera.projection.fov = makeFloatSetting (45.0f);
    sceneData.camera.projection.nearz = makeFloatSetting (0.1f);
    sceneData.camera.projection.farz = makeFloatSetting (1000.0f);
    sceneData.camera.bloom.enabled = makeBoolSetting (false);
    sceneData.camera.bloom.strength = makeFloatSetting (1.0f);
    sceneData.camera.bloom.threshold = makeFloatSetting (1.0f);
    sceneData.camera.shake.enabled = makeBoolSetting (false);
    sceneData.colors.clear = makeVec3Setting ({ 0.0f, 0.0f, 0.0f });
    sceneData.colors.ambient = makeVec3Setting ({ 1.0f, 1.0f, 1.0f });
    sceneData.colors.skylight = makeVec3Setting ({ 1.0f, 1.0f, 1.0f });

    // Cyclic parent references: Object 1 (parent 2) and Object 2 (parent 1)
    sceneData.objects.push_back (
	std::make_unique<Object> (
	    makeObjectData (1, { 10.0f, 20.0f, 0.0f }, { 1.0f, 1.0f, 1.0f }, { 0.0f, 0.0f, 0.0f }, 2)
	)
    );
    sceneData.objects.push_back (
	std::make_unique<Object> (
	    makeObjectData (2, { 30.0f, 40.0f, 0.0f }, { 1.0f, 1.0f, 1.0f }, { 0.0f, 0.0f, 0.0f }, 1)
	)
    );

    const Scene sceneModel (std::move (wpData), std::move (sceneData));

    // Invokes the real public CScene constructor and executes the real createObject production path
    CScene cscene (
    sceneModel, harness->getRenderContext (), harness->getAudioContext (),
    WallpaperEngine::Render::WallpaperState::TextureUVsScaling::DefaultUVs, 0
    );

    // Assert that real scene construction returned normally and both objects were safely instantiated
    CHECK (cscene.getObject (1) != nullptr);
    CHECK (cscene.getObject (2) != nullptr);
    CHECK (cscene.getObjectsByRenderOrder ().size () == 2);
}
