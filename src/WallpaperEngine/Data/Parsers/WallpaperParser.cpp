#include "WallpaperParser.h"

#include "ObjectParser.h"
#include "WallpaperEngine/Data/Model/Project.h"
#include "WallpaperEngine/Data/Model/Wallpaper.h"
#include "WallpaperEngine/Logging/CLog.h"

using namespace WallpaperEngine::Data::Parsers;

WallpaperSharedPtr WallpaperParser::parse (const JSON& file, const ProjectWeakPtr& project) {
    switch (project.lock ()->type) {
        case Project::Type_Scene:
            return std::dynamic_pointer_cast <Wallpaper> (parseScene (file, project));
        case Project::Type_Video:
            return std::dynamic_pointer_cast <Wallpaper> (parseVideo (file, project));
        case Project::Type_Web:
            return std::dynamic_pointer_cast <Wallpaper> (parseWeb (file, project));
        default:
            sLog.exception ("Unexpected project type value found... This is likely a bug");
    }
}

SceneSharedPtr WallpaperParser::parseScene (const JSON& file, const ProjectWeakPtr& project) {
    const auto scene = JSON::parse (project.lock ()->container.lock ()->readFileAsString (file));
    const auto camera = scene.require ("camera", "Scenes must have a camera section");
    const auto general = scene.require ("general", "Scenes must have a general section");
    const auto projection = general.require ("orthogonalprojection", "General section must have orthogonal projection info");
    const auto objects = scene.require ("objects", "Scenes must have an objects section");
    const auto& properties = project.lock ()->properties;

    // TODO: FIND IF THESE DEFAULTS ARE SENSIBLE OR NOT AND PERFORM PROPER VALIDATION WHEN CAMERA PREVIEW AND CAMERA
    // PARALLAX ARE PRESENT

    return std::make_shared <Scene> (
        WallpaperData {
            .filename = "",
            .project = project
        }, SceneData {
            .colors = {
                .ambient  = general.optional ("ambientcolor", glm::vec3 (0.0f)),
                .skylight = general.optional ("skylightcolor", glm::vec3 (0.0f)),
                .clear = general.user ("clearcolor", properties, glm::vec3 (1.0f)),
            },
            .camera = {
                .fade = general.optional ("camerafade", false),
                .preview = general.optional ("camerapreview", false),
                .bloom = {
                    .enabled = general.user ("bloom", properties, false),
                    .strength = general.user ("bloomstrength", properties, 0.0f),
                    .threshold = general.user ("bloomthreshold", properties, 0.0f),
                },
                .parallax = {
                    .enabled = general.optional ("cameraparallax", false),
                    .amount = general.optional ("cameraparallaxamount", 1.0f),
                    .delay = general.optional ("cameraparallaxdelay", 0.0f),
                    .mouseInfluence = general.optional ("cameraparallaxmouseinfluence", 1.0f),
                },
                .shake = {
                    .enabled = general.optional ("camerashake", false),
                    .amplitude = general.optional ("camerashakeamplitude", 0.0f),
                    .roughness = general.optional ("camerashakeroughness", 0.0f),
                    .speed = general.optional ("camerashakespeed", 0.0f),
                },
                .configuration = {
                    .center = camera.require <glm::vec3> ("center", "Camera must have a center position"),
                    .eye = camera.require <glm::vec3> ("eye", "Camera must have an eye position"),
                    .up = camera.require <glm::vec3> ("up", "Camera must have an up position"),
                },
                .projection = {
                    .width = projection.require <int> ("width", "Projection must have a width"),
                    .height = projection.require <int> ("height", "Projection must have a height"),
                    .isAuto = projection.optional ("auto", false)
                }
            },
            .objects = parseObjects (objects, project)
        }
    );
}

VideoSharedPtr WallpaperParser::parseVideo (const JSON& file, const ProjectWeakPtr& project) {
    return std::make_unique <Video> (WallpaperData {
        .filename = file,
        .project = project
    });
}

WebSharedPtr  WallpaperParser::parseWeb (const JSON& file, const ProjectWeakPtr& project) {
    return std::make_unique <Web> (WallpaperData {
        .filename = file,
        .project = project,
    });
}

ObjectMap WallpaperParser::parseObjects (const JSON& objects, const ProjectWeakPtr& project) {
    ObjectMap result = {};

    for (const auto& cur : objects) {
        auto object = ObjectParser::parse (cur, project);

        //TODO: DO WE REALLY WANT TO DIRECTLY CONSTRUCT UNIQUE AND SHARED PTRS EVERYWHERE?
        // SHOULDN'T THAT BE HANDLED BY CALLING CODE (LIKE THIS) INSTEAD?
        result.emplace (object->id, std::move (object));
    }

    return result;
}