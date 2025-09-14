#pragma once

#include <memory>
#include <ostream>
#include <sstream>
#include <vector>
#include <string>

namespace WallpaperEngine::Render::Shaders {
class GLSLContext {
  public:
    /**
     * Types of shaders
     */
    enum UnitType {
        UnitType_Vertex = 0,
        UnitType_Fragment = 1
    };

    GLSLContext ();
    ~GLSLContext ();

    [[nodiscard]] std::pair<std::string, std::string> toGlsl (const std::string& vertex, const std::string& fragment);

    [[nodiscard]] static GLSLContext& get ();

  private:
    static std::unique_ptr <GLSLContext> sInstance;
};
} // namespace WallpaperEngine::Render::Shaders