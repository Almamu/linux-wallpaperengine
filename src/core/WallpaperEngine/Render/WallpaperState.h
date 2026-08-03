#pragma once

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/vec4.hpp>

#include "TextureProvider.h"

namespace WallpaperEngine::Render {
using namespace WallpaperEngine::Data::Assets;
/**
 * Represents current wallpaper state
 */
class WallpaperState {
public:
    // Scaling modes. Defines how UVs coordinates are calculated.
    enum class TextureUVsScaling : uint8_t {
	DefaultUVs,
	ZoomFitUVs,
	ZoomFillUVs,
	StretchUVs,
    };

    WallpaperState (const TextureUVsScaling& textureUVsMode, const uint32_t& clampMode);

    /**
     * Checks if any of the given values has changed
     * @param viewport
     * @param vflip
     * @param projectionWidth
     * @param projectionHeight
     * @return
     */
    [[nodiscard]] bool hasChanged (
	const glm::ivec4& viewport, const bool& vflip, const int& projectionWidth, const int& projectionHeight
    ) const;

    /**
     * Resets UVs to 0/1 values.
     */
    void resetUVs ();

    /**
     * Updates UVs coordinates for current viewport and projection
     *
     * @param projectionWidth
     * @param projectionHeight
     */
    void updateUs (const int& projectionWidth, const int& projectionHeight);

    /**
     * Updates Vs coordinates for current viewport and projection
     *
     * @param projectionWidth
     * @param projectionHeight
     */
    void updateVs (const int& projectionWidth, const int& projectionHeight);

    /**
     * @return Texture UV coordinates for current viewport and projection
     */
    [[nodiscard]] auto getTextureUVs () const { return m_UVs; };

    /**
     * Updates UVs coordinates for current viewport and projection
     */
    template <WallpaperState::TextureUVsScaling> void updateTextureUVs ();

    // Updates state with provided values
    void updateState (
	const glm::ivec4& viewport, const bool& vflip, const int& projectionWidth, const int& projectionHeight
    );

    /**
     * @return The texture scaling mode
     */
    [[nodiscard]] TextureUVsScaling getTextureUVsScaling () const;

    /**
     * @return The texture clamping mode.
     */
    [[nodiscard]] uint32_t getClampingMode () const;

    /**
     * Sets the texture scaling mode
     *
     * @param strategy
     */
    void setTextureUVsStrategy (TextureUVsScaling strategy);

    /**
     * @return The width of viewport
     */
    [[nodiscard]] int getViewportWidth () const;

    /**
     * @return The height of viewport
     */
    [[nodiscard]] int getViewportHeight () const;

    /**
     * @return The width of the projection
     */
    [[nodiscard]] int getProjectionWidth () const;

    /**
     * @return The height of the projection
     */
    [[nodiscard]] int getProjectionHeight () const;

private:
    // Cached UVs value for texture coordinates. No need to recalculate if viewport and projection haven't changed.
    struct {
	float ustart;
	float uend;
	float vstart;
	float vend;
    } m_UVs {};

    // Viewport for which UVs were calculated
    struct {
	int width;
	int height;
    } m_viewport {};

    // Wallpaper dimensions
    struct {
	int width;
	int height;
    } m_projection {};

    // Are Vs coordinates fliped
    bool m_vflip = false;

    // Texture scaling mode
    TextureUVsScaling m_textureUVsMode = TextureUVsScaling::DefaultUVs;
    uint32_t m_clampingMode = TextureFlags_NoFlags;
};
} // namespace WallpaperEngine::Render