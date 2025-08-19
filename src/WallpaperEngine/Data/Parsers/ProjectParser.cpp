#include <algorithm>

#include "ProjectParser.h"
#include "WallpaperEngine/Logging/CLog.h"

#include "WallpaperParser.h"

#include "WallpaperEngine/Data/Model/Wallpaper.h"
#include "WallpaperEngine/Core/Projects/CProperty.h"

using namespace WallpaperEngine::Data::Parsers;

static int backgroundId = 0;

ProjectUniquePtr ProjectParser::parse (const JSON& data, ContainerUniquePtr container) {
    const auto general = data.optional ("general");
    const auto workshopId = data.optional ("workshopid");
    auto actualWorkshopId = std::to_string (--backgroundId);
    auto type = data.require <std::string> ("type", "Project type missing");

    if (workshopId.has_value ()) {
        if (workshopId->is_number ()) {
            actualWorkshopId = std::to_string (workshopId->get <int> ());
        } else if (workshopId->is_string ()) {
            actualWorkshopId = workshopId->get <std::string> ();
        } else {
            sLog.error ("Invalid workshop id: ", workshopId->dump ());
        }
    }

    // lowercase for consistency
    std::transform (type.begin (), type.end (), type.begin (), tolower);

    auto result = std::make_unique <Project> (Project {
        .title = data.require <std::string> ("title", "Project title missing"),
        .type = parseType (type),
        .workshopId = actualWorkshopId,
        .supportsAudioProcessing = general.has_value () && general.value ().optional ("supportsaudioprocessing", false),
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

        // ignore properties that failed, these are generally groups
        if (property == nullptr) {
            continue;
        }

        result.emplace (property->getName (), property);
    }

    return result;
}
