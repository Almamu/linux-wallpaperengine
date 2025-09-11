#include "Virtual.h"

#include "WallpaperEngine/Assets/AssetLoadException.h"

#include <cstring>

using namespace WallpaperEngine::FileSystem;
using namespace WallpaperEngine::FileSystem::Adapters;

ReadStreamSharedPtr VirtualAdapter::open (const std::filesystem::path& path) const {
    const auto file = this->files.find (path);

    if (file == this->files.end ()) {
        throw std::filesystem::filesystem_error ("Cannot find file", path, std::error_code ());
    }

    return file->second;
}

bool VirtualAdapter::exists (const std::filesystem::path& path) const {
    return this->files.contains (path);
}

std::filesystem::path VirtualAdapter::physicalPath (const std::filesystem::path& path) const {
    throw Render::AssetLoadException ("Virtual adapter does not support realpath", path);
}


void VirtualAdapter::add (const std::filesystem::path& path, const char* data) {
    size_t length = strlen (data);
    auto buffer = std::make_unique <char[]> (length);
    std::memcpy (buffer.get (), data, length);

    this->add (path, std::make_shared <MemoryStream> (std::move (buffer), length));
}

void VirtualAdapter::add (const std::filesystem::path& path, const JSON& data) {
    this->add (path, data.dump ());
}

void VirtualAdapter::add (const std::filesystem::path& path, const std::string& data) {
    auto buffer = std::make_unique <char[]> (data.size ());
    std::memcpy (buffer.get (), data.data (), data.size ());

    this->add (path, std::make_shared <MemoryStream> (std::move (buffer), data.size ()));
}

void VirtualAdapter::add (const std::filesystem::path& path, MemoryStreamSharedPtr stream) {
    this->files.insert_or_assign (path, stream);
}

bool VirtualFactory::handlesMountpoint (const std::filesystem::path& path) const {
    return path.string () == "virtual";
}

AdapterSharedPtr VirtualFactory::create (const std::filesystem::path& path) const {
    return std::make_unique<VirtualAdapter> ();
}

