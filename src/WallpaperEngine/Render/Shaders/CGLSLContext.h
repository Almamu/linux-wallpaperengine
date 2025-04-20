#pragma once

#include <memory>
#include <ostream>
#include <sstream>
#include <vector>
#include <string>

namespace WallpaperEngine::Render::Shaders {
class CGLSLContext {
  public:
    /**
     * Types of shaders
     */
    enum UnitType {
        UnitType_Vertex = 0,
        UnitType_Fragment = 1
    };

    CGLSLContext ();
    ~CGLSLContext ();

    [[nodiscard]] std::pair<std::string, std::string> toGlsl (const std::string& vertex, const std::string& fragment);

    [[nodiscard]] static CGLSLContext& get ();

  private:
    static std::shared_ptr<CGLSLContext> sInstance;
};
} // namespace WallpaperEngine::Render::Shaders