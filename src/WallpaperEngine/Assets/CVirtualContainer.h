#pragma once

#include <map>
#include <string>

#include "CContainer.h"
#include "CFileEntry.h"

namespace WallpaperEngine::Assets {
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
    void add (const std::string& filename, const char* contents, uint32_t length);

    /**
     * Adds a new file to the virtual container
     *
     * @param filename
     * @param contents
     */
    void add (const std::string& filename, const std::string& contents);
    /** @inheritdoc */
    const void* readFile (const std::string& filename, uint32_t* length) const override;

  private:
    /** The recorded files in this virtual container */
    std::map<std::string, CFileEntry*> m_virtualFiles;
};
} // namespace WallpaperEngine::Assets