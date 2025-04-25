#pragma once

#include <filesystem>
#include <map>
#include <stdexcept>
#include <string>

#include "CContainer.h"
#include "CFileEntry.h"

namespace WallpaperEngine::Assets {
/**
 * Directory container implementation, provides access to background files under a specific directory
 */
class CDirectory final : public CContainer {
  public:
    explicit CDirectory (const std::filesystem::path& basepath);

    /** @inheritdoc */
    [[nodiscard]] std::filesystem::path resolveRealFile (const std::filesystem::path& filename) const override;
    /** @inheritdoc */
    [[nodiscard]] std::shared_ptr<const uint8_t[]> readFile (const std::filesystem::path& filename, uint32_t* length) const override;

  private:
    /** The basepath for the directory */
    std::filesystem::path m_basepath;
};
} // namespace WallpaperEngine::Assets