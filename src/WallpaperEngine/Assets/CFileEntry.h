//
// Created by almamu on 8/8/21.
//

#pragma once

namespace WallpaperEngine::Assets
{
    class CFileEntry
    {
    public:
        CFileEntry (const void* address, uint32_t length) :
            address (address),
            length (length) { }

        const void* address;
        uint32_t length;
    };
}
