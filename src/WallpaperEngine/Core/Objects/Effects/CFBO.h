#pragma once

#include "WallpaperEngine/Core/Core.h"

#include <nlohmann/json.hpp>
#include <string>

namespace WallpaperEngine::Core::Objects::Effects {
using json = nlohmann::json;

/**
 * FBO = Frame Buffer Object
 *
 * Represents a framebuffer object used in objects with multiple effects or render passes
 */
class CFBO {
  public:
    static const CFBO* fromJSON (const json& data);

    /**
     * @return The FBO name used to identify it in the background's files
     */
    [[nodiscard]] const std::string& getName () const;
    /**
     * @return The scale factor of the FBO
     */
    [[nodiscard]] const float& getScale () const;
    /**
     * @return The FBO's format for the render
     */
    [[nodiscard]] const std::string& getFormat () const;

  protected:
    CFBO (std::string name, float scale, std::string format);

  private:
    /** The name of the FBO */
    const std::string m_name;
    /** The scale factor of the FBO */
    const float m_scale;
    /** The FBO's format for the render */
    const std::string m_format;
};
} // namespace WallpaperEngine::Core::Objects::Effects