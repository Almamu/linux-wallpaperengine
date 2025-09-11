#include <memory>
#include <fstream>

#include "Package.h"

#include "WallpaperEngine/Assets/AssetLoadException.h"
#include "WallpaperEngine/Data/Parsers/PackageParser.h"
#include "WallpaperEngine/Data/Utils/BinaryReader.h"
#include "WallpaperEngine/Data/Utils/MemoryStream.h"

#include <algorithm>

using namespace WallpaperEngine::FileSystem;
using namespace WallpaperEngine::FileSystem::Adapters;

ReadStreamSharedPtr PackageAdapter::open (const std::filesystem::path& path) const {
    // find the file entry
    const auto it = std::ranges::find_if (this->package->files, [&path] (const auto& file) {
        return file->filename == path.string ();
    });

    if (it == this->package->files.end ()) {
        throw std::filesystem::filesystem_error ("Cannot find file", path, std::error_code ());
    }

    // read file into memory
    auto buffer = std::make_unique <char[]> (it->get ()->length);

    // go to the file's position and read into the buffer
    this->package->file->base ().seekg (it->get ()->offset + this->package->baseOffset, std::ios::beg);
    this->package->file->next (buffer.get (), it->get ()->length);

    // create a memory stream and return that
    return std::make_shared <MemoryStream> (std::move (buffer), it->get ()->length);
}

bool PackageAdapter::exists (const std::filesystem::path& path) const {
    for (const auto& file : this->package->files) {
        if (file->filename == path.string ()) {
            return true;
        }
    }

    return false;
}

std::filesystem::path PackageAdapter::physicalPath (const std::filesystem::path& path) const {
    throw Render::AssetLoadException ("Package adapter does not support realpath", path);
}

bool PackageFactory::handlesMountpoint (const std::filesystem::path& path) const {
    auto finalpath = std::filesystem::canonical (path);
    auto status = std::filesystem::status (finalpath);

    return std::filesystem::exists (finalpath) && std::filesystem::is_regular_file (status) && finalpath.extension () == ".pkg";
}

AdapterSharedPtr PackageFactory::create (const std::filesystem::path& path) const {
    auto stream = std::make_shared <std::ifstream> (path, std::ios::binary);
    auto package = Data::Parsers::PackageParser::parse (stream);

    return std::make_unique <PackageAdapter> (std::move (package));
}

