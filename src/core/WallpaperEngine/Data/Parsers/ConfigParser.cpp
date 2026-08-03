#include <filesystem>
#include <fstream>

#include "ConfigParser.h"

#include "WallpaperEngine/Data/Model/Config.h"
#include "linux-wallpaperengine/configuration.h"

using namespace WallpaperEngine::Data::Parsers;
using namespace WallpaperEngine::Data::Model;

ConfigUniquePtr ConfigParser::load (const std::filesystem::path& path) {
    if (!std::filesystem::exists (path)) {
	return nullptr;
    }

    return parse (JSON::parse (std::ifstream (path)));
}

ConfigUniquePtr ConfigParser::parse (const JSON& data) {
    const auto playlistsJson
	= data.optional ("steamuser").value_or (JSON ()).optional ("general").value_or (JSON ()).optional ("playlists");

    return std::make_unique<Config> (Config {
	.playlists = playlistsJson.has_value () ? parsePlaylists (*playlistsJson) : PlaylistMap {},
    });
}

PlaylistMap ConfigParser::parsePlaylists (const JSON& it) {
    PlaylistMap result = {};

    if (!it.is_array ()) {
	return result;
    }

    for (const auto& cur : it) {
	auto playlist = parsePlaylist (cur);

	result.emplace (playlist->name, std::move (playlist));
    }

    return result;
}

PlaylistUniquePtr ConfigParser::parsePlaylist (const JSON& it) {
    const auto settings = it.require ("settings", "Playlist must have settings");
    const auto transition = settings.require ("transition", "Playlist must have a transition");
    const auto order = settings.require ("order", "Playlist must have a order");

    return std::make_unique<Playlist> (Playlist {
	.name = it.require ("name", "Playlist must have a name"),
	.mode = parseMode (settings.require ("mode", "Playlist must have a mode")),
	.order = parseOrder (settings.require ("order", "Playlist must have a order")),
	.delay = settings.require ("delay", "Playlist must have a delay"),
	.transition = parseTransition (settings.require ("transition", "Playlist must have a transition")),
	.transitiontime = settings.require ("transitiontime", "Playlist must have a transitiontime"),
	.items = parsePlaylistItems (it.require ("items", "Playlist must have items")),
    });
}

std::vector<PlaylistItemUniquePtr> ConfigParser::parsePlaylistItems (const JSON& it) {
    std::vector<PlaylistItemUniquePtr> result = {};

    if (!it.is_array ()) {
	return result;
    }

    for (const auto& cur : it) {
	float daytimeend = 0.0f;
	std::string fullpath;

	if (cur.is_object ()) {
	    daytimeend = cur.optional ("daytimeend", 0.0f);
	    fullpath = cur.require ("file", "Playlist item must have a path").get<std::string> ();
	} else {
	    fullpath = cur.get<std::string> ();
	}

	if (fullpath.empty ()) {
	    continue;
	}

	// windows-specific code that cleans up the filepaths
	if (fullpath.starts_with (R"(\\?\)") || fullpath.starts_with ("//?/")) {
	    fullpath = fullpath.substr (4);
	}

	if (fullpath.at (1) == ':') {
	    fullpath = fullpath.substr (2);
	}

	std::filesystem::path path = fullpath;

	if (std::filesystem::is_regular_file (path) == false) {
	    continue;
	}

	path = path.parent_path ();

	result.push_back (std::make_unique<PlaylistItem> (PlaylistItem { .daytimeend = daytimeend, .path = path }));
    }

    return result;
}

std::map<std::string, PlaylistTransition> valueToTransition = { { "none", PlaylistTransition_None },
								{ "random", PlaylistTransition_Random },
								{ "-2", PlaylistTransition_NoneReducedFlicker },
								{ "0", PlaylistTransition_Fade },
								{ "1", PlaylistTransition_Mosaic },
								{ "2", PlaylistTransition_Diffuse },
								{ "3", PlaylistTransition_HorizontalSlide },
								{ "4", PlaylistTransition_VerticalSlide },
								{ "5", PlaylistTransition_HorizontalFade },
								{ "6", PlaylistTransition_VerticalFade },
								{ "7", PlaylistTransition_Clouds },
								{ "8", PlaylistTransition_BurnPaper },
								{ "9", PlaylistTransition_Circular },
								{ "10", PlaylistTransition_Zipper },
								{ "11", PlaylistTransition_Door },
								{ "12", PlaylistTransition_Lines },
								{ "13", PlaylistTransition_Zoom },
								{ "14", PlaylistTransition_Drip },
								{ "15", PlaylistTransition_Pixelate },
								{ "16", PlaylistTransition_Bricks },
								{ "17", PlaylistTransition_Paint },
								{ "18", PlaylistTransition_FadeToBlack },
								{ "19", PlaylistTransition_Twister },
								{ "20", PlaylistTransition_BlackHole },
								{ "21", PlaylistTransition_CRT },
								{ "22", PlaylistTransition_RadialWipe },
								{ "23", PlaylistTransition_GlassShatter },
								{ "24", PlaylistTransition_Bullets },
								{ "25", PlaylistTransition_Ice },
								{ "26", PlaylistTransition_Boilover } };

PlaylistTransition ConfigParser::parseTransition (const std::string& transition) {
    if (valueToTransition.contains (transition)) {
	return valueToTransition.at (transition);
    }

    return PlaylistTransition_Unknown;
}

PlaylistOrder ConfigParser::parseOrder (const std::string& order) {
    if (order == "random") {
	return PlaylistOrder_Random;
    }

    if (order == "sorted") {
	return PlaylistOrder_Sorted;
    }

    return PlaylistOrder_Unknown;
}

std::map<std::string, PlaylistMode> valueToMode = { { "timer", PlaylistMode_Timer },
						    { "logon", PlaylistMode_Logon },
						    { "daytime", PlaylistMode_Daytime },
						    { "dayofweek", PlaylistMode_Weekday },
						    { "never", PlaylsitMode_Never } };

PlaylistMode ConfigParser::parseMode (const std::string& mode) {
    if (valueToMode.contains (mode)) {
	return valueToMode.at (mode);
    }

    return PlaylistMode_Unknown;
}