#include <map>
#include <memory>

#include "Container.h"

#include "Adapters/Directory.h"
#include "Adapters/Package.h"
#include "Adapters/Types.h"
#include "Adapters/Virtual.h"
#include "WallpaperEngine/Assets/AssetLoadException.h"
#include "WallpaperEngine/Logging/Log.h"

using namespace WallpaperEngine::FileSystem;
using namespace WallpaperEngine::FileSystem::Adapters;

/**
 * Normalizes a file path to get rid of relative stuff as most as possible
 * This is not a security measure but helps keep adapters that do not really have an actual filesystem
 * behind to trust the input data without much validation
 * @see https://en.cppreference.com/w/cpp/filesystem/path/lexically_normal
 */
std::filesystem::path normalize_path(const std::filesystem::path& input_path) {
    return input_path.lexically_normal ();
}

Container::Container () {
    // register all available factories
    this->m_factories.push_back (std::make_unique<VirtualFactory> ());
    this->m_factories.push_back (std::make_unique<PackageFactory> ());
    this->m_factories.push_back (std::make_unique<DirectoryFactory> ());

    this->m_vfs = std::make_shared<VirtualAdapter> ();
    this->m_mountpoints.emplace_back ("/", this->m_vfs);
    // mount the current directory as root
    this->mount (std::filesystem::current_path (), "/");
}

ReadStreamSharedPtr Container::read (const std::filesystem::path& path) const {
    const auto normalized = normalize_path (path);

    return this->resolveAdapterForFile (path).open (normalized);
}

std::string Container::readString (const std::filesystem::path& path) const {
    const auto stream = this->read (path);
    std::stringstream buffer;
    buffer << stream->rdbuf ();
    return buffer.str ();
}

std::filesystem::path Container::physicalPath (const std::filesystem::path& path) const {
    const auto normalized = normalize_path (path);

    return this->resolveAdapterForFile (path).physicalPath (normalized);
}

AdapterSharedPtr Container::mount (const std::filesystem::path& path, const std::filesystem::path& mountPoint) {
    // check if any adapter can handle the path
    for (const auto& factory : this->m_factories) {
        if (factory->handlesMountpoint (path) == false) {
            continue;
        }

        return this->m_mountpoints.emplace_back (mountPoint, factory->create (path)).second;
    }

    throw std::filesystem::filesystem_error (
        "The specified mount cannot be handled by any of the filesystem adapters", path, std::error_code ());
}

VirtualAdapter& Container::getVFS () const {
    return *this->m_vfs;
}

Adapter& Container::resolveAdapterForFile (const std::filesystem::path& path) const {
    const auto normalized = normalize_path (path);

    for (const auto& [root, adapter] : this->m_mountpoints) {
        if (normalized.string().starts_with (root.string()) == false) {
            continue;
        }

        if (const auto relative = normalized.string ().substr (root.string ().length ());
            adapter->exists (relative) == false) {
            continue;
        }

        return *adapter;
    }

    if (normalized.string().starts_with ("/") == false) {
        // try resolving as absolute, just in case it's relative to the root
        return this->resolveAdapterForFile ("/" + normalized.string());
    }

    throw std::filesystem::filesystem_error ("Cannot find requested file in any of the mountpoints", path, std::error_code ());
}
