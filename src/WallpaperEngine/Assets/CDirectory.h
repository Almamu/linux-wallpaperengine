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
    explicit CDirectory (std::filesystem::path basepath);

    /** @inheritdoc */
    [[nodiscard]] std::filesystem::path resolveRealFile (const std::string& filename) const override;
    /** @inheritdoc */
    [[nodiscard]] const void* readFile (const std::string& filename, uint32_t* length) const override;

  private:
    /** The basepath for the directory */
    std::filesystem::path m_basepath;
    /** File cache to simplify access to data */
    std::map<std::string, CFileEntry> m_cache;
};
} // namespace WallpaperEngine::Assets