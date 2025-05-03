#pragma once

#include "CContainer.h"

#include <filesystem>
#include <stdexcept>
#include <vector>

namespace WallpaperEngine::Assets {
/**
 * A meta-container that allows backgrounds to have files spread across different containers
 */
class CCombinedContainer final : public CContainer {
  public:
    CCombinedContainer ();

    /**
     * Adds a container to the list
     *
     * @param container
     */
    void add (std::unique_ptr<CContainer> container);
    /**
     * Adds the given package to the list
     *
     * @param path
     */
    void addPkg (const std::filesystem::path& path);

    [[nodiscard]] std::filesystem::path resolveRealFile (const std::filesystem::path& filename) const override;
    [[nodiscard]] std::shared_ptr<const uint8_t[]> readFile (const std::filesystem::path& filename, uint32_t* length) const override;

  private:
    /** The list of containers to search files off from */
    std::vector<std::unique_ptr<CContainer>> m_containers {};
};
}; // namespace WallpaperEngine::Assets