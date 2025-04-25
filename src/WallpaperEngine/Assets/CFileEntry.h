#pragma once

#include <cstdint>

namespace WallpaperEngine::Assets {
/**
 * File cache entry to prevent hit the disk when loading the same file multiple times
 */
class CFileEntry {
  public:
    CFileEntry (std::shared_ptr<const uint8_t[]> content, uint32_t length) :
        content (content),
        length (length) {}
    ~CFileEntry() = default;

    /** File contents */
    std::shared_ptr<const uint8_t[]> content;
    /** File length */
    uint32_t length;
};
} // namespace WallpaperEngine::Assets
