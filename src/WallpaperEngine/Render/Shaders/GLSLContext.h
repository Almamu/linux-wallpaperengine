#pragma once

#include <filesystem>
#include <memory>
#include <mutex>
#include <ostream>
#include <sstream>
#include <string>
#include <unordered_map>
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
    static std::unique_ptr<GLSLContext> sInstance;

    std::filesystem::path m_cacheDir;
    std::unordered_map<std::string, std::pair<std::string, std::string>> m_memoryCache;
    std::mutex m_cacheMutex;
};
} // namespace WallpaperEngine::Render::Shaders