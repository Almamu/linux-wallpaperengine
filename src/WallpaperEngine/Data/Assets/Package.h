#pragma once

#include "Types.h"
#include "WallpaperEngine/Data/Utils/BinaryReader.h"

#include <cstdint>
#include <string>

namespace WallpaperEngine::Data::Assets {
using namespace WallpaperEngine::Data::Utils;

struct FileEntry {
    std::string filename;
    uint32_t offset;
    uint32_t length;
};

struct Package {
    BinaryReaderUniquePtr file;
    FileEntryList files;
    uint32_t baseOffset;
};
}