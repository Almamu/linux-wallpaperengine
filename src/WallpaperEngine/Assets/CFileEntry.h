#pragma once

#include <cstdint>

namespace WallpaperEngine::Assets {
/**
 * File cache entry to prevent hit the disk when loading the same file multiple times
 */
class CFileEntry {
  public:
    CFileEntry (const uint8_t* address, uint32_t length) : address (address), length (length) {}

    ~CFileEntry () {
        delete [] address;
    }

    /** File contents */
    const uint8_t* address;
    /** File length */
    uint32_t length;
};
} // namespace WallpaperEngine::Assets
