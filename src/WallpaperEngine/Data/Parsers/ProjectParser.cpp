#include <algorithm>

#include "ProjectParser.h"
#include "WallpaperEngine/Logging/CLog.h"

#include "WallpaperParser.h"

#include "WallpaperEngine/Data/Model/Wallpaper.h"

using namespace WallpaperEngine::Data::Parsers;

static int backgroundId = 0;

ProjectUniquePtr ProjectParser::parse (const JSON& data, ContainerUniquePtr container) {
    const auto general = data.optional ("general");
    auto type = data.require <std::string> ("type", "Project type missing");

    // lowercase for consistency
    std::transform (type.begin (), type.end (), type.begin (), tolower);

    auto result = std::make_unique <Project> (Project {
        .title = data.require <std::string> ("title", "Project title missing"),
        .type = parseType (type),
        .workshopId = data.optional ("workshopid", std::to_string (--backgroundId)),
        .supportsAudioProcessing = general.has_value () && general.value ().optional ("supportsAudioProcessing", false),
        .properties = parseProperties (general),
        .container = std::move(container),
    });

    result->wallpaper = WallpaperParser::parse (data.require ("file", "Project's main file missing"), *result);

    return result;
}

Project::Type ProjectParser::parseType (const std::string& type) {
    if (type == "scene") {
        return Project::Type_Scene;
    } else if (type == "video") {
        return Project::Type_Video;
    } else if (type == "web") {
        return Project::Type_Web;
    }

    sLog.exception ("Unsupported project type ", type);
}

Properties ProjectParser::parseProperties (const std::optional <JSON>& data) {
    if (!data.has_value ()) {
        return {};
    }

    const auto properties = data.value ().optional ("properties");

    if (!properties.has_value ()) {
        return {};
    }

    Properties result = {};

    // TODO: CHANGE THIS ONCE THE PROPERTIES PARSING IS HANDLED IN THE NEW TYPES
    //  THESE ARE COMPLEX TYPES THAT INCLUDE SOME RUNTIME INFO THAT ISN'T EXPLICITLY DATA
    //  SO THIS WILL NEED A RETHINK IN THE FUTURE
    for (const auto& cur : properties.value ().items ()) {
        const auto& property = Property::fromJSON (cur.value (), cur.key ());

        result.emplace (property->getName (), property);
    }

    return result;
}
