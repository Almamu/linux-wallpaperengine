#pragma once

#include <string>
#include <sstream>

#include "WallpaperEngine/Data/Model/Types.h"

namespace WallpaperEngine::Data::Dumpers {
using namespace WallpaperEngine::Data::Model;

class StringPrinter {
  public:
    explicit StringPrinter (std::string indentationCharacter = "\t");
    ~StringPrinter () = default;

    /**
     * @return The contents of the pretty printer buffer
     */
    std::string str ();

    /**
     * Prints the information of the given wallpaper
     *
     * @param wallpaper
     */
    void printWallpaper (const Wallpaper& wallpaper);

    /**
     * Prints the information of the given object
     *
     * @param object
     */
    void printObject (const Object& object);

    /**
     * Prints the information of the given image
     *
     * @param image
     */
    void printImage (const Image& image);

    /**
     * Prints the information of the given sound
     *
     * @param sound
     */
    void printSound (const Sound& sound);

    /**
     * Prints the information of the given model
     *
     * @param model
     */
    void printModel (const ModelStruct& model);

    /**
     * Prints the information of the given image effect
     *
     * @param imageEffect
     */
    void printImageEffect (const ImageEffect& imageEffect);

    /**
     * Prints the information of the given image effect pass
     *
     * @param imageEffectPass
     */
    void printImageEffectPassOverride (const ImageEffectPassOverride& imageEffectPass);

    /**
     * Prints the information of the given FBO
     *
     * @param fbo
     */
    void printFBO (const FBO& fbo);

    /**
     * Prints the information of the given material
     *
     * @param material
     */
    void printMaterial (const Material& material);

    /**
     * Prints the information of the given material pass
     *
     * @param materialPass
     */
    void printMaterialPass (const MaterialPass& materialPass);

    /**
     * Prints the information of the given effect
     *
     * @param effect
     */
    void printEffect (const Effect& effect);

    /**
     * Prints the information of the given effect
     *
     * @param effectPass
     */
    void printEffectPass (const EffectPass& effectPass);

  private:
    /**
     * Printss the current identation level
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

    int m_level = 0;
    std::stringbuf m_buffer = {};
    std::ostream m_out;
    std::string m_indentationCharacter;
};
} // namespace WallpaperEngine::Data::Dumpers
