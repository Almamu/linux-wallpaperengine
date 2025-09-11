#pragma once

#include <exception>
#include <string>

// TODO: REWRITE THIS ONE TO MAKE MORE SENSE, IT REALLY MEANS "FILE-RELATED EXCEPTION"
namespace WallpaperEngine::Assets {
class CAssetLoadException final : public std::exception {
  public:
    explicit CAssetLoadException (const std::string& filename, const std::string& extrainfo = "");
    [[nodiscard]] const char* what () const noexcept override;

  private:
    std::string m_message {};
};
} // namespace WallpaperEngine::Assets