#pragma once

#include <memory>
#include <ostream>
#include <sstream>
#include <string>
#include <vector>

namespace WallpaperEngine::Render::Shaders {
class GLSLContext {
public:
    /**
     * Types of shaders
     */
    enum UnitType { UnitType_Vertex = 0, UnitType_Fragment = 1 };

    GLSLContext ();
    ~GLSLContext ();

    [[nodiscard]] std::pair<std::string, std::string> toGlsl (const std::string& vertex, const std::string& fragment);

    [[nodiscard]] static GLSLContext& get ();

private:
    /**
     * Attempts to fix HLSL-to-GLSL vector type mismatches by parsing glslang error messages
     * and adding appropriate swizzle operators to truncate larger vectors.
     *
     * @param source The shader source code
     * @param errorLog The glslang error log
     * @return true if a fix was applied, false otherwise
     */
    static bool fixVectorTypeMismatch (std::string& source, const std::string& errorLog);

    static std::unique_ptr<GLSLContext> sInstance;
};
} // namespace WallpaperEngine::Render::Shaders