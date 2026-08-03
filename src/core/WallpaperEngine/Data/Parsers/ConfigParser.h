#pragma once
#include "../JSON.h"
#include "WallpaperEngine/Data/Model/Config.h"

namespace WallpaperEngine::Data::Parsers {
using JSON = WallpaperEngine::Data::JSON::JSON;
using namespace WallpaperEngine::Data::Model;

class ConfigParser {
public:
    static ConfigUniquePtr load (const std::filesystem::path& path);
    static ConfigUniquePtr parse (const JSON& data);

private:
    static PlaylistMap parsePlaylists (const JSON& it);
    static PlaylistUniquePtr parsePlaylist (const JSON& it);

    static std::vector<PlaylistItemUniquePtr> parsePlaylistItems (const JSON& it);
    static PlaylistTransition parseTransition (const std::string& transition);
    static PlaylistOrder parseOrder (const std::string& order);
    static PlaylistMode parseMode (const std::string& mode);
};
}