#pragma once

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/vec4.hpp>

namespace WallpaperEngine::Render {
/**
 * Represents current wallpaper state
 */
class CWallpaperState {
  public:
    // Scaling modes. Defines how UVs coordinates are calculated.
    enum class TextureUVsScaling : uint8_t {
        DefaultUVs,
        ZoomFitUVs,
        ZoomFillUVs,
        StretchUVs,
    };

    CWallpaperState (const TextureUVsScaling& textureUVsMode) : m_textureUVsMode (textureUVsMode) {};

    // Compares saved state values with passed arguments
    bool hasChanged (const glm::ivec4& viewport, const bool& vflip, const int& projectionWidth,
                     const int& projectionHeight) const {
        return this->viewport.width != viewport.z || this->viewport.height != viewport.w ||
               this->projection.width != projectionWidth || this->projection.height != projectionHeight ||
               this->vflip != vflip;
    }

    // Reset UVs to 0/1 values
    void resetUVs ();

    // Update Us coordinates for current viewport and projection
    void updateUs (const int& projectionWidth, const int& projectionHeight);

    // Update Vs coordinates for current viewport and projection
    void updateVs (const int& projectionWidth, const int& projectionHeight);

    // Get texture UV coordinates
    auto getTextureUVs () const {
        return UVs;
    };

    // Set texture UV coordinates according to texture scaling mode
    template <CWallpaperState::TextureUVsScaling> void updateTextureUVs ();

    // Updates state with provided values
    void updateState (const glm::ivec4& viewport, const bool& vflip, const int& projectionWidth,
                      const int& projectionHeight);

    // @return The texture scaling mode
    TextureUVsScaling getTextureUVsScaling () const {
        return m_textureUVsMode;
    };

    // Set texture scaling mode
    void setTextureUVsStrategy (const TextureUVsScaling strategy) {
        m_textureUVsMode = strategy;
    };

    // @return The width of viewport
    int getViewportWidth () const {
        return viewport.width;
    };

    // @return The height of viewport
    int getViewportHeight () const {
        return viewport.height;
    };

    // @return The width of viewport
    int getProjectionWidth () const {
        return projection.width;
    };

    // @return The height of viewport
    int getProjectionHeight () const {
        return projection.height;
    };

  private:
    // Cached UVs value for texture coordinates. No need to recalculate if viewport and projection haven't changed.
    struct {
        float ustart;
        float uend;
        float vstart;
        float vend;
    } UVs;

    // Viewport for which UVs were calculated
    struct {
        int width;
        int height;
    } viewport {};

    // Wallpaper dimensions
    struct {
        int width;
        int height;
    } projection {};

    // Are Vs coordinates fliped
    bool vflip = false;

    // Texture scaling mode
    TextureUVsScaling m_textureUVsMode = TextureUVsScaling::DefaultUVs;
};
} // namespace WallpaperEngine::Render