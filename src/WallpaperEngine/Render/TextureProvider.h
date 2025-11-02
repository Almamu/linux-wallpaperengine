#pragma once

#include <vector>
#include <memory>

#include <GL/glew.h>
#include <glm/vec4.hpp>

#include "WallpaperEngine/Data/Assets/Texture.h"
#include "WallpaperEngine/Data/Model/Types.h"

namespace WallpaperEngine::Render {
using namespace WallpaperEngine::Data::Assets;
/**
 * Base interface that describes the minimum information required for a texture
 * to be displayed by the engine
 */
class TextureProvider {
  public:
    virtual ~TextureProvider () = default;

    /**
     * @param imageIndex For animated textures, the frame to get the ID of
     * @return The OpenGL texture to use when rendering
     */
    [[nodiscard]] virtual GLuint getTextureID (uint32_t imageIndex) const = 0;
    /**
     * @param imageIndex For animated textures, the frame to get the ID of
     * @return The texture's width
     */
    [[nodiscard]] virtual uint32_t getTextureWidth (uint32_t imageIndex) const = 0;
    /**
     * @param imageIndex For animated textures, the frame to get the ID of
     * @return The texture's height
     */
    [[nodiscard]] virtual uint32_t getTextureHeight (uint32_t imageIndex) const = 0;
    /**
     * @return The textures real width
     */
    [[nodiscard]] virtual uint32_t getRealWidth () const = 0;
    /**
     * @return The textures real height
     */
    [[nodiscard]] virtual uint32_t getRealHeight () const = 0;
    /**
     * @return The texture's memory format
     */
    [[nodiscard]] virtual TextureFormat getFormat () const = 0;
    /**
     * @return The texture's settings
     */
    [[nodiscard]] virtual uint32_t getFlags () const = 0;
    /**
     * @return The list of frames this texture has
     */
    [[nodiscard]] virtual const std::vector<FrameSharedPtr>& getFrames () const = 0;
    /**
     * @return The texture's resolution vector
     */
    [[nodiscard]] virtual const glm::vec4* getResolution () const = 0;
    /**
     * @return If the texture is animated or not
     */
    [[nodiscard]] virtual bool isAnimated () const = 0;
    /**
     * @return Number of columns in spritesheet grid (0 if not a spritesheet)
     */
    [[nodiscard]] virtual uint32_t getSpritesheetCols () const = 0;
    /**
     * @return Number of rows in spritesheet grid (0 if not a spritesheet)
     */
    [[nodiscard]] virtual uint32_t getSpritesheetRows () const = 0;
    /**
     * @return Total number of frames in spritesheet (0 if not a spritesheet)
     */
    [[nodiscard]] virtual uint32_t getSpritesheetFrames () const = 0;
    /**
     * @return Duration of spritesheet animation in seconds
     */
    [[nodiscard]] virtual float getSpritesheetDuration () const = 0;
};
} // namespace WallpaperEngine::Render