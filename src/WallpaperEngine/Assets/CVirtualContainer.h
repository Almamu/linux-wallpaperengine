#pragma once

#include <map>
#include <string>
#include <filesystem>

#include "CContainer.h"
#include "CFileEntry.h"
#include <nlohmann/json.hpp>


namespace WallpaperEngine::Assets {
using json = nlohmann::json;
/**
 * Virtual container implementation, provides virtual files for the backgrounds to use
 */
class CVirtualContainer final : public CContainer {
  public:
    /**
     * Adds a new file to the virtual container
     *
     * @param filename
     * @param contents
     * @param length
     */
    void add (const std::filesystem::path& filename, const std::shared_ptr<const uint8_t[]>& contents, uint32_t length);

    /**
     * Adds a new file to the virtual container
     *
     * @param filename
     * @param contents
     */
    void add (const std::filesystem::path& filename, const std::string& contents);

    /**
     * Adds a new file to the virtual container
     *
     * @param filename
     * @param contents
     */
    void add (const std::filesystem::path& filename, const char* contents);
    /**
     * Adds a new file to the virtual container from a json object
     * @param filename
     * @param contents
     */
    void add (const std::filesystem::path& filename, const json& contents);

    /** @inheritdoc */
    std::shared_ptr<const uint8_t[]> readFile (const std::filesystem::path& filename, uint32_t* length) const override;

  private:
    /** The recorded files in this virtual container */
    std::map<std::string, std::unique_ptr<CFileEntry>> m_virtualFiles;
};
} // namespace WallpaperEngine::Assets