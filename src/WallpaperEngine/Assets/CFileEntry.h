#pragma once

#include <cstdint>

namespace WallpaperEngine::Assets
{
    /**
     * File cache entry to prevent hit the disk when loading the same file multiple times
     */
    class CFileEntry
    {
    public:
        CFileEntry (const void* address, uint32_t length) :
            address (address),
            length (length) { }

        /** File contents */
        const void* address;
        /** File length */
        uint32_t length;
    };
}
