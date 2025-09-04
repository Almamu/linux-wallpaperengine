#pragma once

#include <iostream>

namespace WallpaperEngine::Data::Utils {
struct MemoryStream : std::streambuf
{
    MemoryStream (char* p, size_t size)
    {
        this->setg(p, p, p + size); // set start end end pointers
    }
};
}