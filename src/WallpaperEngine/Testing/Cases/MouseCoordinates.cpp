#include "WallpaperEngine/Render/Camera.h"
#include "WallpaperEngine/Render/Objects/CImage.h"
#include "WallpaperEngine/Render/Wallpapers/CScene.h"

// CEF exposes its own CHECK macro through CScene's renderer includes. Catch must own the
// test assertion macro in this translation unit.
#ifdef CHECK
#undef CHECK
#endif

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <limits>

using WallpaperEngine::Data::Model::CameraTransform;
using WallpaperEngine::Render::Camera;
using WallpaperEngine::Render::Objects::CImage;
using WallpaperEngine::Render::Wallpapers::CScene;

TEST_CASE ("perspective cursor hits tiny rotated image planes", "[mouse][regression]") {
    const glm::mat4 view = glm::lookAt (glm::vec3 (10, 0, 0), glm::vec3 (0), glm::vec3 (0, 1, 0));
    const glm::mat4 world = glm::translate (glm::mat4 (1), glm::vec3 (1, .1f, 0))
	* glm::rotate (glm::mat4 (1), glm::half_pi<float> (), glm::vec3 (0, 1, 0))
	* glm::scale (glm::mat4 (1), glm::vec3 (.001f));
    const glm::vec3 expected (2, -3, 0);
    for (const bool flipped : { false, true }) {
	glm::mat4 projection = glm::perspective (glm::radians (50.0f), 16.0f / 9, .1f, 100.0f);
	if (flipped) projection[1][1] *= -1.0f;
	const glm::vec4 clip = projection * view * world * glm::vec4 (expected, 1);
	glm::vec2 ndc = glm::vec2 (clip) / clip.w;
	if (flipped) ndc.y *= -1.0f;
	const auto hit = CImage::intersectCursorPlane (world, projection * view, (ndc + 1.0f) * .5f, flipped);
	REQUIRE (hit.has_value ());
	CHECK (hit->x == Catch::Approx (expected.x).margin (.002f));
	CHECK (hit->y == Catch::Approx (expected.y).margin (.002f));
	CHECK (hit->z == 0.0f);
    }
    const glm::mat4 projection = glm::perspective (glm::radians (50.0f), 1.0f, .1f, 100.0f);
    CHECK_FALSE (CImage::intersectCursorPlane (glm::mat4 (1), projection * view, {.5f, .5f}, false));
    CHECK_FALSE (CImage::intersectCursorPlane (glm::mat4 (0), projection * view, {.5f, .5f}, false));
    const glm::mat4 behind = glm::translate (glm::mat4 (1), glm::vec3 (20, 0, 0)) * world;
    CHECK_FALSE (CImage::intersectCursorPlane (behind, projection * view, {.5f, .5f}, false));
}

TEST_CASE ("cursor hit testing preserves a plane with zero Z scale", "[mouse][regression]") {
    const glm::mat4 view = glm::lookAt (glm::vec3 (0, 0, 10), glm::vec3 (0), glm::vec3 (0, 1, 0));
    const glm::mat4 projection = glm::perspective (glm::radians (50.0f), 1.0f, .1f, 100.0f);
    const glm::mat4 world = glm::scale (glm::mat4 (1), glm::vec3 (2, 3, 0));
    const glm::vec3 expected (.25f, -.5f, 0);
    const glm::vec4 clip = projection * view * world * glm::vec4 (expected, 1);
    const auto hit = CImage::intersectCursorPlane (
	world, projection * view, (glm::vec2 (clip) / clip.w + 1.0f) * .5f, false);
    REQUIRE (hit.has_value ());
    CHECK (hit->x == Catch::Approx (expected.x).margin (.001f));
    CHECK (hit->y == Catch::Approx (expected.y).margin (.001f));
    CHECK_FALSE (CImage::intersectCursorPlane (
	glm::scale (glm::mat4 (1), glm::vec3 (0, 3, 0)), projection * view, {.5f, .5f}, false));
}

/**
 * Test GLFW to OpenGL coordinate conversion
 * GLFW: Y=0 at top, Y=height at bottom
 * OpenGL: Y=0 at bottom, Y=height at top
 */
TEST_CASE ("GLFW to OpenGL coordinate conversion") {
    const int framebufferHeight = 1080;

    // Mouse at top of screen (GLFW: Y=0)
    double glfwY = 0.0;
    double openglY = static_cast<double> (framebufferHeight) - glfwY;
    CHECK (openglY == 1080.0); // Should be at top in OpenGL (Y=height)

    // Mouse at bottom of screen (GLFW: Y=height)
    glfwY = 1080.0;
    openglY = static_cast<double> (framebufferHeight) - glfwY;
    CHECK (openglY == 0.0); // Should be at bottom in OpenGL (Y=0)

    // Mouse at middle of screen
    glfwY = 540.0;
    openglY = static_cast<double> (framebufferHeight) - glfwY;
    CHECK (openglY == 540.0); // Should be at middle in both systems
}

/**
 * Test Wayland to OpenGL coordinate conversion
 * Wayland: Y=0 at top, Y=height at bottom
 * OpenGL: Y=0 at bottom, Y=height at top
 */
TEST_CASE ("Wayland to OpenGL coordinate conversion") {
    const double viewportHeight = 1080.0;

    // Mouse at top of screen (Wayland: Y=0)
    double waylandY = 0.0;
    double openglY = viewportHeight - waylandY;
    CHECK (openglY == 1080.0); // Should be at top in OpenGL

    // Mouse at bottom of screen (Wayland: Y=height)
    waylandY = 1080.0;
    openglY = viewportHeight - waylandY;
    CHECK (openglY == 0.0); // Should be at bottom in OpenGL

    // Mouse at middle of screen
    waylandY = 540.0;
    openglY = viewportHeight - waylandY;
    CHECK (openglY == 540.0); // Should be at middle
}

/**
 * Test OpenGL to normalized coordinate conversion
 * OpenGL: Y=0 at bottom, Y=height at top
 * Normalized: 0=bottom, 1=top (OpenGL convention)
 */
TEST_CASE ("OpenGL to normalized coordinate conversion") {
    const int viewportY = 0;
    const int viewportHeight = 1080;

    // Mouse at top in OpenGL (Y=height)
    double openglY = 1080.0;
    double normalizedY = glm::clamp ((openglY - viewportY) / static_cast<double> (viewportHeight), 0.0, 1.0);
    CHECK (normalizedY == 1.0); // Should be 1.0 (top)

    // Mouse at bottom in OpenGL (Y=0)
    openglY = 0.0;
    normalizedY = glm::clamp ((openglY - viewportY) / static_cast<double> (viewportHeight), 0.0, 1.0);
    CHECK (normalizedY == 0.0); // Should be 0.0 (bottom)

    // Mouse at middle
    openglY = 540.0;
    normalizedY = glm::clamp ((openglY - viewportY) / static_cast<double> (viewportHeight), 0.0, 1.0);
    CHECK (std::abs (normalizedY - 0.5) < 0.001); // Should be ~0.5 (middle)
}

/**
 * Test OpenGL to CEF coordinate conversion
 * OpenGL: Y=0 at bottom, Y=height at top
 * CEF: Y=0 at top, Y=height at bottom
 */
TEST_CASE ("OpenGL to CEF coordinate conversion") {
    const int viewportHeight = 1080;
    const int viewportY = 0;

    // Mouse at top in OpenGL (Y=height)
    double openglY = 1080.0;
    int clampedY = std::clamp (static_cast<int> (openglY - viewportY), 0, viewportHeight);
    int cefY = viewportHeight - clampedY;
    CHECK (cefY == 0); // Should be 0 (top in CEF)

    // Mouse at bottom in OpenGL (Y=0)
    openglY = 0.0;
    clampedY = std::clamp (static_cast<int> (openglY - viewportY), 0, viewportHeight);
    cefY = viewportHeight - clampedY;
    CHECK (cefY == 1080); // Should be height (bottom in CEF)

    // Mouse at middle
    openglY = 540.0;
    clampedY = std::clamp (static_cast<int> (openglY - viewportY), 0, viewportHeight);
    cefY = viewportHeight - clampedY;
    CHECK (cefY == 540); // Should be middle
}

/**
 * Test complete coordinate flow: GLFW → OpenGL → Normalized
 * Verifies the full pipeline works correctly
 */
TEST_CASE ("Complete coordinate flow: GLFW to normalized") {
    const int framebufferHeight = 1080;
    const int viewportY = 0;
    const int viewportHeight = 1080;

    // Mouse at top of screen
    double glfwY = 0.0;
    double openglY = static_cast<double> (framebufferHeight) - glfwY; // Convert to OpenGL
    double normalizedY = glm::clamp ((openglY - viewportY) / static_cast<double> (viewportHeight), 0.0, 1.0);
    CHECK (normalizedY == 1.0); // Top should normalize to 1.0

    // Mouse at bottom of screen
    glfwY = 1080.0;
    openglY = static_cast<double> (framebufferHeight) - glfwY;
    normalizedY = glm::clamp ((openglY - viewportY) / static_cast<double> (viewportHeight), 0.0, 1.0);
    CHECK (normalizedY == 0.0); // Bottom should normalize to 0.0
}

TEST_CASE ("SceneScript cursor world position uses orthographic canvas coordinates") {
    const glm::mat4 identity (1.0f);

    const glm::vec3 bottomLeft
	= Camera::projectScreenPosition ({ 0.0f, 0.0f }, true, 343.0f, 193.0f, identity, identity, false);
    const glm::vec3 center
	= Camera::projectScreenPosition ({ 0.5f, 0.5f }, true, 343.0f, 193.0f, identity, identity, false);
    const glm::vec3 topRight
	= Camera::projectScreenPosition ({ 1.0f, 1.0f }, true, 343.0f, 193.0f, identity, identity, false);

    CHECK (bottomLeft == glm::vec3 (0.0f, 0.0f, 0.0f));
    CHECK (center == glm::vec3 (171.5f, 96.5f, 0.0f));
    CHECK (topRight == glm::vec3 (343.0f, 193.0f, 0.0f));
}

TEST_CASE ("SceneScript cursor world position intersects the perspective authoring plane") {
    constexpr float fov = 50.0f;
    constexpr float aspect = 16.0f / 9.0f;
    constexpr float cameraDistance = 2.3f;
    const glm::mat4 view
	= glm::lookAt (glm::vec3 (0.0f, 0.0f, cameraDistance), glm::vec3 (0.0f), glm::vec3 (0.0f, 1.0f, 0.0f));
    glm::mat4 projection = glm::perspective (glm::radians (fov), aspect, 0.01f, 100.0f);

    const glm::vec3 center
	= Camera::projectScreenPosition ({ 0.5f, 0.5f }, false, 1920.0f, 1080.0f, projection, view, false);
    const glm::vec3 right
	= Camera::projectScreenPosition ({ 1.0f, 0.5f }, false, 1920.0f, 1080.0f, projection, view, false);
    const glm::vec3 top
	= Camera::projectScreenPosition ({ 0.5f, 1.0f }, false, 1920.0f, 1080.0f, projection, view, false);

    const float expectedY = cameraDistance * std::tan (glm::radians (fov) * 0.5f);
    CHECK (center.x == Catch::Approx (0.0f).margin (0.0001f));
    CHECK (center.y == Catch::Approx (0.0f).margin (0.0001f));
    CHECK (right.x == Catch::Approx (expectedY * aspect).margin (0.0001f));
    CHECK (top.y == Catch::Approx (expectedY).margin (0.0001f));

    projection[1][1] *= -1.0f;
    const glm::vec3 flippedTop
	= Camera::projectScreenPosition ({ 0.5f, 1.0f }, false, 1920.0f, 1080.0f, projection, view, true);
    CHECK (flippedTop.y == Catch::Approx (expectedY).margin (0.0001f));
}

/**
 * Test coordinate conversion with different viewport sizes
 * Ensures conversion works with non-standard viewport dimensions
 */
TEST_CASE ("Coordinate conversion with different viewport sizes") {
    // Test with 1920x1080 viewport
    {
	const int height = 1080;
	double glfwY = 0.0;
	double openglY = static_cast<double> (height) - glfwY;
	CHECK (openglY == 1080.0);
    }

    // Test with 2560x1440 viewport
    {
	const int height = 1440;
	double glfwY = 0.0;
	double openglY = static_cast<double> (height) - glfwY;
	CHECK (openglY == 1440.0);
    }

    // Test with 800x600 viewport
    {
	const int height = 600;
	double glfwY = 0.0;
	double openglY = static_cast<double> (height) - glfwY;
	CHECK (openglY == 600.0);
    }
}

TEST_CASE ("Wallpaper Engine camera parallax delay response") {
    CHECK (CScene::calculateParallaxSmoothingAlpha (0.0f, 1.0f / 60.0f) == 1.0f);
    CHECK (CScene::calculateParallaxSmoothingAlpha (1.0f, 1.0f / 60.0f) == Catch::Approx (1.0f / 9.0f));
    CHECK (CScene::calculateParallaxSmoothingAlpha (2.0f, 1.0f / 60.0f) == Catch::Approx (1.0f / 18.0f));
    CHECK (CScene::calculateParallaxSmoothingAlpha (3.0f, 1.0f / 60.0f) == 0.0f);
}

TEST_CASE ("Off-center parallax layers use their root origin relative to the canvas cursor") {
    using WallpaperEngine::Render::CObject;
    // 3100265648's left background has only 14.89 authored pixels of left
    // overscan. A cursor-only translation added 52.8 px and exposed gray;
    // the native root-origin term produces 5.06 px and preserves coverage.
    const glm::vec2 canvas (1920.0f, 1080.0f);
    const glm::vec2 backgroundOrigin (525.98431f, 541.46173f);
    const glm::vec2 offset
	= CObject::calculateParallaxOffset (backgroundOrigin, glm::vec2 (1.0f), canvas, { -0.5f, 0.0f }, 0.11f);
    CHECK (offset.x == Catch::Approx (5.058274f));
    CHECK (offset.y == Catch::Approx (-0.160791f));
    CHECK (backgroundOrigin.x - 1010.0f * 1.07103f * 0.5f + offset.x < 0.0f);

    const glm::vec2 centered
	= CObject::calculateParallaxOffset (canvas * 0.5f, { 0.5f, 0.5f }, canvas, { -0.5f, 0.5f }, 0.1f);
    CHECK (centered.x == Catch::Approx (24.0f));
    CHECK (centered.y == Catch::Approx (13.5f));
    CHECK (
	CObject::calculateParallaxOffset (backgroundOrigin, glm::vec2 (0), canvas, { -0.5f, 0.5f }, 0.11f)
	== glm::vec2 (0)
    );
    CHECK (
	CObject::calculateParallaxOffset (backgroundOrigin, glm::vec2 (1), canvas, { -0.5f, 0.5f }, 0.0f)
	== glm::vec2 (0)
    );
    // Signed per-axis depths remain supported; origin and cursor share canvas units.
    const glm::vec2 opposite = CObject::calculateParallaxOffset ({ 480, 810 }, { -1, 2 }, canvas, { 0, 0 }, 0.1f);
    CHECK (opposite == glm::vec2 (48, -54));
}

TEST_CASE ("Scene timing includes global frames skipped by an asynchronous output") {
    // A new wallpaper starts at zero even after a long load or an hours-old engine process.
    CHECK (CScene::calculateSceneDeltaTime (2.0f, std::nullopt) == 0.0f);
    CHECK (CScene::calculateSceneDeltaTime (7200.0f, std::nullopt) == 0.0f);

    // A 60 Hz output can render after several 165 Hz application iterations. Its scene clock
    // must advance by the full time since that scene last rendered, not one 165 Hz iteration.
    CHECK (CScene::calculateSceneDeltaTime (2.0f + 1.0f / 60.0f, 2.0f) == Catch::Approx (1.0f / 60.0f));

    // Duplicate renders of a shared/span scene at one application timestamp do not advance twice.
    CHECK (CScene::calculateSceneDeltaTime (2.0f, 2.0f) == 0.0f);
    CHECK (CScene::calculateSceneDeltaTime (1.0f, 2.0f) == 0.0f);
}

TEST_CASE ("Scene timing retains frame intervals after five days", "[scene-time]") {
    // Native rolls its exposed scene clock after five days. Process timestamps must
    // retain frame precision before subtraction even when a scene loaded recently.
    const double previous = 432000.0;
    const double interval = 1.0 / 60.0;
    CHECK (CScene::calculateSceneDeltaTime (previous + interval, previous) == Catch::Approx (interval));
    CHECK (CScene::calculateSceneDeltaTime (previous, previous) == 0.0f);
}

TEST_CASE ("Lit perspective images use the authored world transform") {
    glm::mat4 world = glm::translate (glm::mat4 (1.0f), glm::vec3 (-4.31196f, 1.66488f, 2.36022f));
    world = glm::rotate (world, 1.57080f, glm::vec3 (0.0f, 0.0f, 1.0f));
    world = glm::rotate (world, 1.41049f, glm::vec3 (0.0f, 1.0f, 0.0f));
    world = glm::rotate (world, 1.57080f, glm::vec3 (1.0f, 0.0f, 0.0f));
    world = glm::scale (world, glm::vec3 (0.002f));

    const glm::mat4 view = glm::lookAt (
	glm::vec3 (-1.21766f, 1.04065f, 2.12913f), glm::vec3 (-2.16943f, 0.87410f, 2.38680f),
	glm::vec3 (0.0f, 1.0f, 0.0f)
    );
    const glm::mat4 projection = glm::perspective (glm::radians (50.0f), 16.0f / 9.0f, 0.1f, 10000.0f);
    const glm::mat4 viewProjection = projection * view;
    const CImage::PerspectiveSceneMatrices matrices = CImage::calculatePerspectiveSceneMatrices (world, viewProjection);

    const glm::vec4 localCorner (108.0f, 155.0f, 0.0f, 1.0f);
    const glm::vec4 litShaderPosition = matrices.viewProjection * matrices.model * localCorner;
    const glm::vec4 regularShaderPosition = matrices.modelViewProjection * localCorner;

    for (int component = 0; component < 4; component++) {
	CHECK (litShaderPosition[component] == Catch::Approx (regularShaderPosition[component]));
    }
    CHECK (glm::length (glm::vec3 (matrices.model * localCorner) - glm::vec3 (world * localCorner)) < 1e-6f);
}

TEST_CASE ("Normal model bases preserve sheared model columns", "[normal-model-matrix]") {
    // Native binds individually normalized model columns, rather than the
    // inverse-transpose orientation. Values below use its one-step estimate.
    glm::mat4 world (1.0f);
    world[0] = glm::vec4 (2.0f, 0.0f, 0.0f, 0.0f);
    world[1] = glm::vec4 (1.0f, 3.0f, 0.0f, 0.0f);
    world[2] = glm::vec4 (0.0f, 0.0f, -4.0f, 0.0f);
    world[3] = glm::vec4 (17.0f, -23.0f, 8.0f, 1.0f);
    const glm::mat3 expected {
        { 0.9983081222f, 0.0f, 0.0f },
        { 0.3156852722f, 0.9470558167f, 0.0f },
        { 0.0f, 0.0f, -0.9983081222f }
    };
    const auto matrices = CImage::calculatePerspectiveSceneMatrices (world, glm::mat4 (1.0f));
    for (int column = 0; column < 3; ++column) {
        for (int component = 0; component < 3; ++component) {
            CHECK (matrices.normalModel[column][component]
                == Catch::Approx (expected[column][component]).margin (0.000002f));
        }
    }
    world[2] = glm::vec4 (0.0f);
    const auto flattened = CImage::calculatePerspectiveSceneMatrices (world, glm::mat4 (1.0f));
    for (int column = 0; column < 3; ++column) {
        for (int component = 0; component < 3; ++component) {
            CHECK (flattened.normalModel[column][component]
                == Catch::Approx (column == 2 ? 0.0f : expected[column][component]).margin (0.000002f));
        }
    }
}

TEST_CASE ("Wallpaper Engine fog authoring values map to shader parameters") {
    const glm::vec4 distance = CScene::calculateFogParams (1.0f, 24.65f, 0.54f, 0.97f);
    CHECK (distance.x == 1.0f);
    CHECK (distance.y == Catch::Approx (23.65f));
    CHECK (distance.z == 0.54f);
    CHECK (distance.w == Catch::Approx (0.43f));

    const glm::vec4 height = CScene::calculateFogParams (-8.35f, 75.0f, 0.0f, 0.6f);
    CHECK (height.x == -8.35f);
    CHECK (height.y == Catch::Approx (83.35f));
    CHECK (height.z == 0.0f);
    CHECK (height.w == 0.6f);
}

TEST_CASE ("Wallpaper Engine camera path startup fade envelope") {
    CHECK (CScene::calculateCameraFadeAlpha (0.0f, 10.0f) == 1.0f);
    CHECK (CScene::calculateCameraFadeAlpha (0.25f, 10.0f) == Catch::Approx (0.5f));
    CHECK (CScene::calculateCameraFadeAlpha (0.5f, 10.0f) == 0.0f);
    CHECK (CScene::calculateCameraFadeAlpha (9.5f, 10.0f) == 0.0f);
    CHECK (CScene::calculateCameraFadeAlpha (9.75f, 10.0f) == Catch::Approx (0.5f));
    CHECK (CScene::calculateCameraFadeAlpha (10.0f, 10.0f) == 1.0f);
    CHECK (CScene::calculateCameraFadeAlpha (1.0f, 0.0f) == 0.0f);
}

TEST_CASE ("Camera path queues exhaust random pools with native bounded draws", "[camera][random-queue]") {
    std::vector<size_t> remaining;
    size_t consumed = 0;
    const auto unusedRandom = [&consumed] { ++consumed; return 1U; };
    SECTION ("Sequential and empty queues do not consume random values") {
	CHECK (CScene::chooseCameraPathIndex (std::nullopt, 0, "random", remaining, unusedRandom) == 0);
	CHECK (CScene::chooseCameraPathIndex (std::nullopt, 3, "sequence", remaining, unusedRandom) == 0);
	CHECK (CScene::chooseCameraPathIndex (0, 3, "sequence", remaining, unusedRandom) == 1);
	CHECK (CScene::chooseCameraPathIndex (2, 3, "sequential", remaining, unusedRandom) == 0);
	CHECK (consumed == 0);
    }
    SECTION ("Each pool plays every shot before refill without an immediate repeat") {
	const std::vector<uint32_t> words { 1, 1, 1, 0xffffffffU, 0xffffffffU, 0xffffffffU };
	const auto nextRandom = [&] { REQUIRE (consumed < words.size ()); return words[consumed++]; };
	std::optional<size_t> current;
	for (const size_t expected : { 0U, 1U, 2U, 0U, 2U, 1U }) {
	    current = CScene::chooseCameraPathIndex (current, 3, "random", remaining, nextRandom);
	    CHECK (*current == expected);
	}
	CHECK (consumed == words.size ());
    }
    SECTION ("Multiply-high selection retains the order of remaining shots") {
	const std::vector<uint32_t> words { 0x40000000U, 0xffffffffU, 0x80000000U, 1 };
	const auto nextRandom = [&] { REQUIRE (consumed < words.size ()); return words[consumed++]; };
	std::optional<size_t> current;
	for (const size_t expected : { 1U, 3U, 2U, 0U }) {
	    current = CScene::chooseCameraPathIndex (current, 4, "random", remaining, nextRandom);
	    CHECK (*current == expected);
	}
	CHECK (consumed == words.size ());
    }
    SECTION ("Rejected low products retry without selecting a shot") {
	const std::vector<uint32_t> words { 0, 1, 0xffffffffU, 1 };
	const auto nextRandom = [&] { REQUIRE (consumed < words.size ()); return words[consumed++]; };
	std::optional<size_t> current;
	for (const size_t expected : { 0U, 2U, 1U }) {
	    current = CScene::chooseCameraPathIndex (current, 3, "random", remaining, nextRandom);
	    CHECK (*current == expected);
	}
	CHECK (consumed == words.size ());
    }
    SECTION ("A single shot repeats and still consumes a draw") {
	CHECK (CScene::chooseCameraPathIndex (std::nullopt, 1, "random", remaining, unusedRandom) == 0);
	CHECK (CScene::chooseCameraPathIndex (0, 1, "random", remaining, unusedRandom) == 0);
	CHECK (consumed == 2);
    }
}

TEST_CASE ("Camera object transform follows its world-space parent rig") {
    glm::mat4 world = glm::translate (glm::mat4 (1.0f), glm::vec3 (2.0f, 3.0f, -5.0f));
    world = glm::rotate (world, glm::pi<float> (), glm::vec3 (0.0f, 1.0f, 0.0f));

    const CameraTransform camera = Camera::objectTransform (world, 60.0f, 1.25f);
    CHECK (camera.eye.x == Catch::Approx (2.0f));
    CHECK (camera.eye.y == Catch::Approx (3.0f));
    CHECK (camera.eye.z == Catch::Approx (-5.0f));
    CHECK (camera.center.x == Catch::Approx (2.0f));
    CHECK (camera.center.y == Catch::Approx (3.0f));
    CHECK (camera.center.z == Catch::Approx (-4.0f));
    CHECK (camera.up.y == Catch::Approx (1.0f));
    CHECK (camera.fov == Catch::Approx (60.0f));
    CHECK (camera.zoom == Catch::Approx (1.25f));
}

TEST_CASE ("3D automatic sorting draws the stable first phase before depth-sorted deferred objects") {
    const std::vector<CScene::TransparentSortKey> keys = {
	{ .sortable = true, .cameraDepth = -2.0f },
	{ .sortable = false, .cameraDepth = -100.0f },
	{ .sortable = true, .cameraDepth = -20.0f },
	{ .sortable = true, .cameraDepth = -10.0f },
	{ .sortable = false, .cameraDepth = -9.0f },
	{ .sortable = true, .cameraDepth = -10.0f },
	{ .sortable = true, .sortLast = true },
	{ .sortable = false, .cameraDepth = 0.0f },
    };

    const std::vector<size_t> permutation = CScene::calculateTransparentSortPermutation (keys);
    REQUIRE (permutation.size () == keys.size ());
    CHECK (permutation == std::vector<size_t> { 1, 4, 7, 2, 3, 5, 0, 6 });
}

TEST_CASE ("Deferred objects retain their prior frame order when moving depths become equal") {
    const std::vector<CScene::TransparentSortKey> keys = {
	{ .sortable = true, .cameraDepth = -10.0f, .previousOrder = 2 },
	{ .sortable = false, .cameraDepth = -50.0f },
	{ .sortable = true, .cameraDepth = -10.0f, .previousOrder = 0 },
	{ .sortable = true, .cameraDepth = -10.0f, .previousOrder = 1 },
	{ .sortable = true, .cameraDepth = -10.0f }, // A newly inserted deferred layer.
    };
    CHECK (CScene::calculateTransparentSortPermutation (keys) == std::vector<size_t> { 1, 2, 3, 0, 4 });
}

TEST_CASE ("Fullscreen deferred images sort last even with nonfinite layer depths") {
    const std::vector<CScene::TransparentSortKey> keys = {
	{ .sortable = true, .sortLast = true, .previousOrder = 0 },
	{ .sortable = true, .cameraDepth = std::numeric_limits<float>::infinity (), .previousOrder = 1 },
	{ .sortable = true, .cameraDepth = 2.0f, .previousOrder = 2 },
	{ .sortable = true, .cameraDepth = -std::numeric_limits<float>::infinity (), .previousOrder = 3 },
	{ .sortable = true, .cameraDepth = std::numeric_limits<float>::quiet_NaN (), .previousOrder = 4 },
	{ .sortable = true, .sortLast = true, .previousOrder = 5 },
    };
    CHECK (CScene::calculateTransparentSortPermutation (keys) == std::vector<size_t> { 1, 3, 4, 2, 0, 5 });
}

TEST_CASE ("Shader parallax position is independent of camera translation amount") {
    CHECK (CScene::calculateShaderParallaxPosition ({ 0.0f, 0.0f }) == glm::vec2 (0.5f));
    CHECK (CScene::calculateShaderParallaxPosition ({ -1.0f, 1.0f }) == glm::vec2 (0.0f, 1.0f));
    CHECK (CScene::calculateShaderParallaxPosition ({ -0.15f, 0.15f }) == glm::vec2 (0.425f, 0.575f));
}

TEST_CASE ("attachment texture projection retains native pixel bias and reflected basis", "[attachment][projection]") {
    const glm::mat4 attachment = glm::translate (glm::mat4 (1), {185, 110, 12})
        * glm::rotate (glm::mat4 (1), glm::radians (30.0f), {0, 0, 1})
        * glm::scale (glm::mat4 (1), {1.5f, .75f, 1});
    const glm::mat4 receiver = glm::translate (glm::mat4 (1), {160, 90, 0});
    const glm::mat4 projection = glm::ortho (0.0f, 320.0f, 0.0f, 180.0f, -1000.0f, 1000.0f);
    const glm::mat3 matrix = CImage::calculateOrthographicAttachmentTextureMatrix (
        attachment, receiver, projection, projection, 100, {100, 60});
    CHECK (matrix[2].x == Catch::Approx (.745f).margin (.00001f));
    CHECK (matrix[2].y == Catch::Approx (.175f).margin (.00001f));
    CHECK (matrix[0].x == Catch::Approx (.72057669f).margin (.00001f));
    CHECK (matrix[0].y == Catch::Approx (-.69337525f).margin (.00001f));
    CHECK (matrix[1].x == matrix[0].y);
    CHECK (matrix[1].y == -matrix[0].x);
    CHECK (glm::determinant (matrix) == Catch::Approx (-1));

    // Native 24-bit pixel readback confirmed this oblique receiver too.
    const glm::mat4 tiltedReceiver = glm::translate (glm::mat4 (1), {130, 75, 10})
        * glm::rotate (glm::mat4 (1), .3f, {0, 0, 1})
        * glm::rotate (glm::mat4 (1), .2f, {0, 1, 0})
        * glm::rotate (glm::mat4 (1), .1f, {1, 0, 0})
        * glm::scale (glm::mat4 (1), {1.2f, .8f, 1});
    const glm::mat3 tilted = CImage::calculateOrthographicAttachmentTextureMatrix (
        attachment, tiltedReceiver, projection, projection, 100, {100, 60});
    CHECK (tilted[2].x == Catch::Approx (1.026802263f).margin (.00003f));
    CHECK (tilted[2].y == Catch::Approx (.148553312f).margin (.00003f));
    CHECK (tilted[0].x == Catch::Approx (.871568771f).margin (.00003f));
    CHECK (tilted[0].y == Catch::Approx (-.490273268f).margin (.00003f));
    CHECK (glm::determinant (tilted) == Catch::Approx (-1));
}

TEST_CASE ("attachment projection extrapolates tilted receiver planes and discards depth", "[attachment][projection]") {
    const glm::mat4 projection = glm::ortho (-100.0f, 100.0f, -100.0f, 100.0f, -10.0f, 10.0f);
    // The receiving plane is beyond the view's far clip, with a source well
    // outside its texture rectangle. Native still projects the infinite plane.
    const glm::mat4 receiver = glm::translate (glm::mat4 (1), {0, 0, 50})
        * glm::rotate (glm::mat4 (1), glm::radians (60.0f), {0, 1, 0});
    const auto calculate = [&] (float sourceZ) {
        return CImage::calculateOrthographicAttachmentTextureMatrix (
            glm::translate (glm::mat4 (1), {20, 10, sourceZ}), receiver,
            projection, projection, 100, {10, 10});
    };
    const glm::mat3 first = calculate (-500), second = calculate (900);
    CHECK (first[2].x == Catch::Approx (4.45f).margin (.0001f));
    CHECK (first[2].y == Catch::Approx (-.45f).margin (.0001f));
    CHECK (second[2].x == Catch::Approx (first[2].x));
    CHECK (second[2].y == Catch::Approx (first[2].y));
    CHECK (first[0].x == Catch::Approx (1));
    CHECK (first[0].y == Catch::Approx (0).margin (.00001f));

    glm::mat4 edgeOn (1);
    edgeOn[0] = {0, 0, 1, 0};
    edgeOn[2] = {1, 0, 0, 0};
    const glm::mat3 parallel = CImage::calculateOrthographicAttachmentTextureMatrix (
        glm::mat4 (1), edgeOn, projection, projection, 100, {100, 50});
    CHECK (parallel[0] == glm::vec3 (0));
    CHECK (parallel[1] == glm::vec3 (0));
    CHECK (parallel[2].x == Catch::Approx (.495f));
    CHECK (parallel[2].y == Catch::Approx (.51f));
}

TEST_CASE ("attachment projection uses distinct source and receiver render offsets", "[attachment][projection]") {
    const glm::mat4 projection = glm::ortho (-100.0f, 100.0f, -100.0f, 100.0f, -10.0f, 10.0f);
    const glm::mat3 matrix = CImage::calculateOrthographicAttachmentTextureMatrix (
        glm::mat4 (1), glm::mat4 (1),
        glm::translate (projection, {10, -20, 0}),
        glm::translate (projection, {-5, 4, 0}), 100, {100, 50});
    CHECK (matrix[2].x == Catch::Approx (.645f));
    CHECK (matrix[2].y == Catch::Approx (.99f));
}

TEST_CASE ("virtual model attachment projects through its source camera", "[attachment][projection]") {
    const glm::mat4 attachment = glm::translate (glm::mat4 (1), {185, 110, 12})
        * glm::rotate (glm::mat4 (1), glm::radians (30.0f), {0, 0, 1})
        * glm::scale (glm::mat4 (1), {1.5f, .75f, 1});
    const glm::mat4 sceneToRender = glm::translate (glm::mat4 (1), {-160, 90, 0})
        * glm::scale (glm::mat4 (1), {1, -1, 1});
    const glm::mat4 orthographic = glm::ortho (-160.0f, 160.0f, -90.0f, 90.0f, -1000.0f, 1000.0f);
    const float fov = glm::radians (95.0f);
    const float distance = 1.0f / (std::tan (fov * .5f) * orthographic[1][1]);
    const glm::mat4 sourceProjection = glm::perspective (fov, 320.0f / 180.0f, 5.0f, 15000.0f)
        * glm::translate (glm::mat4 (1), {0, 0, -distance}) * sceneToRender;
    const glm::mat4 receiver = glm::translate (glm::mat4 (1), {160, 90, 0});
    const glm::mat3 matrix = CImage::calculateOrthographicAttachmentTextureMatrix (
        attachment, receiver, sourceProjection, orthographic * sceneToRender, 100, {100, 60});
    // Native05 source2 -> ordinary receiver11, measured to +/- 1/131072.
    CHECK (matrix[2].x == Catch::Approx (.787571424f).margin (.00003f));
    CHECK (matrix[2].y == Catch::Approx (.118238101f).margin (.00003f));
    CHECK (matrix[0].x == Catch::Approx (.720576692f).margin (.00003f));
    CHECK (matrix[0].y == Catch::Approx (-.693375245f).margin (.00003f));
    CHECK (glm::determinant (matrix) == Catch::Approx (-1));
}
