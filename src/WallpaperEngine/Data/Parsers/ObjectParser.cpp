#include "ObjectParser.h"
#include "ModelParser.h"
#include "EffectParser.h"

#include "WallpaperEngine/Logging/CLog.h"
#include "WallpaperEngine/Data/Model/Project.h"
#include "WallpaperEngine/Data/Model/Object.h"

using namespace WallpaperEngine::Data::Parsers;
using namespace WallpaperEngine::Data::Model;

ObjectUniquePtr ObjectParser::parse (const JSON& it, const ProjectWeakPtr& project) {
    const auto imageIt = it.find ("image");
    const auto soundIt = it.find ("sound");
    const auto particleIt = it.find ("particle");
    const auto textIt = it.find ("text");
    const auto lightIt = it.find ("light");
    const auto dependenciesIt = it.find ("dependencies");
    const auto basedata = ObjectData {
        .id = it.require <int> ("id", "Object must have an id"),
        .name = it.require <std::string> ("name", "Object must have a name"),
        .dependencies = parseDependencies (it),
    };

    if (imageIt != it.end ()) {
        return parseImage (it, project, basedata, *imageIt);
    } else if (soundIt != it.end ()) {
        return parseSound (it, project, basedata);
    } else if (particleIt != it.end ()) {

    } else if (textIt != it.end ()) {

    } else if (lightIt != it.end ()) {

    } else {
        // dump the object for now, might want to change later
        sLog.exception ("Unknown object type found: ", it.dump ());
    }

    return std::make_unique <Object> (basedata);
}

std::vector<int> ObjectParser::parseDependencies (const JSON& it) {
    const auto dependenciesIt = it.find ("dependencies");

    if (dependenciesIt == it.end () || !dependenciesIt->is_array ()) {
        return {};
    }

    std::vector<int> result = {};

    for (const auto& cur : *dependenciesIt) {
        result.push_back (cur);
    }

    return result;
}

SoundUniquePtr ObjectParser::parseSound (const JSON& it, const ProjectWeakPtr& project, ObjectData base) {
    const auto soundIt = it.require ("sound", "Object must have a sound");
    std::vector<std::string> sounds = {};

    for (const auto& cur : soundIt) {
        sounds.push_back (cur);
    }

    return std::make_unique <Sound> (
        std::move (base),
        SoundData {
            .playbackmode = it.optional <std::string> ("playbackmode"),
            .sounds = sounds,
        }
    );
}

ImageUniquePtr ObjectParser::parseImage (
    const JSON& it, const ProjectWeakPtr& project, ObjectData base, const std::string& image) {
    const auto& properties = project.lock ()->properties;
    const auto& effects = it.optional ("effects");

    return std::make_unique <Image> (
        std::move (base),
        ImageData {
            .origin = it.user ("origin", properties, glm::vec3 (0.0f)),
            .scale = it.user ("scale", properties, glm::vec3 (1.0f)),
            .angles = it.user ("angles", properties, glm::vec3 (0.0)),
            .visible = it.user ("visible", properties, true),
            .alpha = it.user ("alpha", properties, 1.0f),
            .color = it.user ("color", properties, glm::vec3 (1.0f)),
            .alignment = it.optional ("alignment", std::string ("center")),
            .size = it.optional ("size", glm::vec2 (0.0f)),
            .parallaxDepth = it.optional ("parallaxDepth", glm::vec2 (0.0f)),
            .colorBlendMode = it.optional ("colorBlendMode", 0),
            .brightness = it.optional ("brightness", 1.0f),
            .model = ModelParser::load (project, image),
            .effects = effects.has_value () ? parseEffects (*effects, project) : std::vector <ImageEffectUniquePtr> {},
        }
    );
}

std::vector <ImageEffectUniquePtr> ObjectParser::parseEffects (const JSON& it, const ProjectWeakPtr& project) {
    if (!it.is_array ()) {
        return {};
    }

    std::vector <ImageEffectUniquePtr> result = {};

    for (const auto& cur : it) {
       result.push_back (parseEffect (cur, project));
    }

    return result;
}

ImageEffectUniquePtr ObjectParser::parseEffect (const JSON& it, const ProjectWeakPtr& project) {
    const auto& passes = it.optional ("passes");
    return std::make_unique <ImageEffect> (ImageEffect {
        .id = it.require <int> ("id", "Image effect must have an id"),
        .name = it.require <std::string> ("name", "Image effect must have a name"),
        .visible = it.user ("visible", project.lock ()->properties, true),
        //TODO: ADD EFFECTS HERE
        .passes = passes.has_value () ? parseEffectPasses (passes.value (), project) : std::vector <ImageEffectPassUniquePtr> {},
        .effect = EffectParser::load (project, it.require ("file", "Image effect must have an effect"))
    });
}

std::vector <ImageEffectPassUniquePtr> ObjectParser::parseEffectPasses (const JSON& it, const ProjectWeakPtr& project) {
    if (!it.is_array ()) {
        return {};
    }

    std::vector <ImageEffectPassUniquePtr> result = {};

    for (const auto& cur : it) {
        result.push_back (parseEffectPass (cur, project));
    }

    return result;
}

ImageEffectPassUniquePtr ObjectParser::parseEffectPass (const JSON& it, const ProjectWeakPtr& project) {
    return std::make_unique <ImageEffectPass> (ImageEffectPass {
        .id = it.require <int> ("id", "Image effect pass must have an id"),
    });
}