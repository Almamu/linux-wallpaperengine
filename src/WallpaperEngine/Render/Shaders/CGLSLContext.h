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
    enum ShaderType {
        ShaderType_Vertex = 0,
        ShaderType_Pixel = 1,
        ShaderType_Include = 2
    };

    CGLSLContext ();
    ~CGLSLContext ();

    [[nodiscard]] std::string toGlsl (const std::string& content, ShaderType type);

    [[nodiscard]] static CGLSLContext& get ();

  private:
    static std::shared_ptr<CGLSLContext> sInstance;
};
} // namespace WallpaperEngine::Render::Shaders