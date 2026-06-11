#pragma once

#include "../Utils/BinaryReader.h"
#include "WallpaperEngine/Data/Assets/Types.h"
#include <iosfwd>

namespace WallpaperEngine::Data::Parsers {
using namespace WallpaperEngine::Data::Assets;
using namespace WallpaperEngine::Data::Utils;
class PackageParser {
public:
    static PackageUniquePtr parse (ReadStreamSharedPtr stream);

private:
    static FileEntryList parseFileList (const BinaryReader& stream);
};
}