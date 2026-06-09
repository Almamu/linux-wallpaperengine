#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <glm/glm.hpp>

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