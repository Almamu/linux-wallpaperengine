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
 *
 * Normalization is handled by going up a level on ".." when there's a previous level
 * or keeping it when there's none, this will most likely not be a valid path, but
 * the filesystem might resolve it into a file that's inside it's root, so this has to be taken into account
 *
 * @param input_path The path to normalize
 * @return The normalized path
 */
std::filesystem::path normalize_path(const std::filesystem::path& input_path) {
    std::filesystem::path result;

    for (const auto& part : input_path) {
        std::string part_str = part.string();
        if (part_str == ".") {
            // Skip
        } else if (part_str == "..") {
            // Go up one level if possible
            if (!result.empty() && *--result.end() != "..") {
                result = result.parent_path();
            } else {
                // If we're already at root or have ".." segments,
                // keep the ".." (for relative paths)
                result /= part;
            }
        } else {
            // Add regular segment
            result /= part;
        }
    }

    return result;
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
