#include "ObjectParser.h"
#include "ModelParser.h"
#include "EffectParser.h"

#include "ShaderConstantParser.h"
#include "WallpaperEngine/Data/Model/Object.h"
#include "WallpaperEngine/Data/Model/Project.h"
#include "WallpaperEngine/Logging/Log.h"

using namespace WallpaperEngine::Data::Parsers;
using namespace WallpaperEngine::Data::Model;

ObjectUniquePtr ObjectParser::parse (const JSON& it, const Project& project) {
    const auto imageIt = it.find ("image");
    const auto soundIt = it.find ("sound");
    const auto particleIt = it.find ("particle");
    const auto textIt = it.find ("text");
    const auto lightIt = it.find ("light");
    const auto basedata = ObjectData {
        .id = it.require <int> ("id", "Object must have an id"),
        .name = it.require <std::string> ("name", "Object must have a name"),
        .dependencies = parseDependencies (it),
    };

    if (imageIt != it.end () && imageIt->is_string ()) {
        return parseImage (it, project, basedata, *imageIt);
    } else if (soundIt != it.end () && soundIt->is_array ()) {
        return parseSound (it, basedata);
    } else if (particleIt != it.end ()) {
        sLog.error ("Particle objects are not supported yet");
    } else if (textIt != it.end ()) {
        sLog.error ("Text objects are not supported yet");
    } else if (lightIt != it.end ()) {
        sLog.error ("Light objects are not supported yet");
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

SoundUniquePtr ObjectParser::parseSound (const JSON& it, ObjectData base) {
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

ImageUniquePtr ObjectParser::parseImage (const JSON& it, const Project& project, ObjectData base, const std::string& image) {
    const auto& properties = project.properties;
    const auto& effects = it.optional ("effects");

    auto result = std::make_unique <Image> (
        std::move (base),
        ImageData {
            .origin = it.user ("origin", properties, glm::vec3 (0.0f)),
            .scale = it.user ("scale", properties, glm::vec3 (1.0f)),
            .angles = it.user ("angles", properties, glm::vec3 (0.0)),
            .visible = it.user ("visible", properties, true),
            .alpha = it.user ("alpha", properties, 1.0f),
            .color = it.user ("color", properties, glm::vec4 (1.0f)),
            .alignment = it.optional ("alignment", std::string ("center")),
            .size = it.optional ("size", glm::vec2 (0.0f)),
            .parallaxDepth = it.optional ("parallaxDepth", glm::vec2 (0.0f)),
            .colorBlendMode = it.optional ("colorBlendMode", 0),
            .brightness = it.optional ("brightness", 1.0f),
            .model = ModelParser::load (project, image),
            .effects = effects.has_value () ? parseEffects (*effects, project) : std::vector <ImageEffectUniquePtr> {},
        }
    );

    // color should be a vec4 for alpha, but it's read as vec3
    if (result->color->value->getType () == DynamicValue::UnderlyingType::Vec3) {
        result->color->value->update (glm::vec4 (result->color->value->getVec3 (), 1.0f));
    } else if (result->color->value->getType () == DynamicValue::UnderlyingType::IVec3) {
        result->color->value->update (glm::vec4 (result->color->value->getIVec3 (), 255));
    }

    return result;
}

std::vector <ImageEffectUniquePtr> ObjectParser::parseEffects (const JSON& it, const Project& project) {
    if (!it.is_array ()) {
        return {};
    }

    std::vector <ImageEffectUniquePtr> result = {};

    for (const auto& cur : it) {
       result.push_back (parseEffect (cur, project));
    }

    return result;
}

ImageEffectUniquePtr ObjectParser::parseEffect (const JSON& it, const Project& project) {
    const auto& passsOverrides = it.optional ("passes");
    return std::make_unique <ImageEffect> (ImageEffect {
        .id = it.optional <int> ("id", -1),
        .name = it.optional <std::string> ("name", "Effect without name"),
        .visible = it.user ("visible", project.properties, true),
        .passOverrides = passsOverrides.has_value () ? parseEffectPassOverrides (passsOverrides.value (), project) : std::vector <ImageEffectPassOverrideUniquePtr> {},
        .effect = EffectParser::load (project, it.require ("file", "Image effect must have an effect"))
    });
}

std::vector <ImageEffectPassOverrideUniquePtr> ObjectParser::parseEffectPassOverrides (const JSON& it, const Project& project) {
    if (!it.is_array ()) {
        return {};
    }

    std::vector <ImageEffectPassOverrideUniquePtr> result = {};

    for (const auto& cur : it) {
        result.push_back (parseEffectPass (cur, project));
    }

    return result;
}

ImageEffectPassOverrideUniquePtr ObjectParser::parseEffectPass (const JSON& it, const Project& project) {
    const auto& combos = it.optional ("combos");
    const auto& textures = it.optional ("textures");
    const auto& constants = it.optional ("constantshadervalues");

    // TODO: PARSE CONSTANT SHADER VALUES AND FIND REFS?
    return std::make_unique <ImageEffectPassOverride> (ImageEffectPassOverride {
        .id = it.optional <int> ("id", -1),
        .combos = combos.has_value () ? parseComboMap (combos.value ()) : ComboMap {},
        .constants = constants.has_value () ? ShaderConstantParser::parse (constants.value (), project) : ShaderConstantMap {},
        .textures = textures.has_value () ? parseTextureMap (textures.value ()) : TextureMap {},
    });
}

TextureMap ObjectParser::parseTextureMap (const JSON& it) {
    if (!it.is_array ()) {
        return {};
    }

    TextureMap result = {};
    int textureIndex = -1;

    for (const auto& cur : it) {
        textureIndex ++;

        if (cur.is_null ()) {
            result.emplace (textureIndex, "");
        } else {
            result.emplace (textureIndex, cur);
        }
    }

    return result;
}

ComboMap ObjectParser::parseComboMap (const JSON& it) {
    if (!it.is_object ()) {
        return {};
    }

    ComboMap result = {};

    for (const auto& cur : it.items ()) {
        result.emplace (cur.key (), cur.value ());
    }

    return result;
}