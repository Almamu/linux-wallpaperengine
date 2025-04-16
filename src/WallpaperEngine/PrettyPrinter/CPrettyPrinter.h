#pragma once

#include <sstream>

#include "WallpaperEngine/Render/Objects/CImage.h"

using namespace WallpaperEngine::Render;
using namespace WallpaperEngine::Render::Objects;
using namespace WallpaperEngine::Render::Objects::Effects;

namespace WallpaperEngine::PrettyPrinter {
/**
 * Provides insights on the actual data that is being used to render the wallpaper
 * to the screens
 */
class CPrettyPrinter {
  public:
    CPrettyPrinter ();
    ~CPrettyPrinter () = default;

    /**
     * Prints the information of the given wallpaper
     *
     * @param wallpaper
     */
    void printWallpaper (const CWallpaper& wallpaper);

    /**
     * Prints the information of the given image
     * @param image
     */
    void printImage (const CImage& image);

    /**
     * Prints the information of the given effect
     *
     * @param effect
     * @param effectId The effect ID to show on the output if required
     */
    void printEffect (const CEffect& effect, int effectId = 0);

    /**
     * Prints the information of the given FBO
     *
     * @param fbo
     */
    void printFBO (const CFBO& fbo);

    /**
     * Prints the information of the given material
     *
     * @param material
     */
    void printMaterial (const CMaterial& material);

    /**
     * Prints the information of the given material's pass
     *
     * @param pass
     */
    void printPass (const CPass& pass, int passId = 0);

    /**
     * @return The contents of the pretty printer buffer
     */
    std::string str ();

  private:
    /**
     * Prints the current indentation level in tabs
     */
    void indentation ();
    /**
     * Prints an end of line and indentates the next line to the aproppiate level
     */
    void lineEnd ();
    /**
     * Increases the indentation level and prints a new line up to the specified level
     */
    void increaseIndentation ();
    /**
     * Decreases the indentation level and prints a new line up to the specified level
     */
    void decreaseIndentation ();
    /**
     * Prints a texture's format as string in-place
     *
     * @param format
     */
    void printTextureFormat (ITexture::TextureFormat format);
    /**
     * Prints a texture's flags as string in-place
     *
     * @param flags
     */
    void printTextureFlags (ITexture::TextureFlags flags);
    /**
     * Prints information about a given ITexture in-place
     *
     * @param texture
     */
    void printTextureInfo (const ITexture& texture);

    int m_level;
    std::stringbuf m_buffer;
    std::ostream m_out;
};
}