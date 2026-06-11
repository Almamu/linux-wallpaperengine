#include <filesystem>
#include <fstream>
#include <memory>

#include "MediaCover.h"

#include "WallpaperEngine/Assets/AssetLoadException.h"
#include "WallpaperEngine/Context.h"
#include "WallpaperEngine/Logging/Log.h"

using namespace WallpaperEngine::FileSystem;
using namespace WallpaperEngine::FileSystem::Adapters;

ReadStreamSharedPtr MediaCoverAdapter::open (const std::filesystem::path& path) const {
    if (path != "$mediaThumbnail") {
	throw std::filesystem::filesystem_error (
	    "MediaCoverAdapter only supports $mediaThumbnail", path, std::error_code ()
	);
    }

    if (!project.getMediaInfo ().url.has_value ()) {
	throw std::filesystem::filesystem_error ("Media source does not have a valid URL", path, std::error_code ());
    }

    std::string album = *project.getMediaInfo ().url;

    if (album.starts_with ("file://")) {
	album = album.substr (7);
    } else {
	throw std::filesystem::filesystem_error (
	    "Only file:// URLs are supported for media covers", album, std::error_code ()
	);
    }

    std::filesystem::path file = std::filesystem::absolute (album);

    if (std::filesystem::exists (file) == false) {
	throw std::filesystem::filesystem_error ("Media file does not exist", file, std::error_code ());
    }

    if (std::filesystem::is_regular_file (file) == false) {
	throw std::filesystem::filesystem_error ("Media file is not a regular file", file, std::error_code ());
    }

    return std::make_shared<std::ifstream> (file);
}

bool MediaCoverAdapter::exists (const std::filesystem::path& path) const { return path == ""; }

std::filesystem::path MediaCoverAdapter::physicalPath (const std::filesystem::path& path) const {
    sLog.exception ("MediaCoverAdapter does not support realpath");
}

bool MediaCoverFactory::handlesMountpoint (const std::filesystem::path& path) const {
    return path == "$mediaThumbnail";
}

AdapterSharedPtr MediaCoverFactory::create (const std::filesystem::path& path) const {
    if (path != "$mediaThumbnail") {
	sLog.exception ("MediaCoveradapter only supports $mediaThumbnail");
    }

    return std::make_unique<MediaCoverAdapter> (project);
}