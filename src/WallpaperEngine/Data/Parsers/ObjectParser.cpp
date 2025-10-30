#include "ObjectParser.h"
#include "ModelParser.h"
#include "MaterialParser.h"
#include "EffectParser.h"

#include "ShaderConstantParser.h"
#include "WallpaperEngine/Data/Model/Object.h"
#include "WallpaperEngine/Data/Model/Project.h"
#include "WallpaperEngine/Logging/Log.h"

#include <sstream>

using namespace WallpaperEngine::Data::Parsers;
using namespace WallpaperEngine::Data::Model;

ObjectUniquePtr ObjectParser::parse (const JSON& it, const Project& project) {
    const auto imageIt = it.find ("image");
    const auto soundIt = it.find ("sound");
    const auto particleIt = it.find ("particle");
    const auto textIt = it.find ("text");
    const auto lightIt = it.find ("light");

    // Parse base object data
    // Some particle objects have numeric 'name' fields, so handle type mismatches gracefully
    ObjectData basedata;
    try {
        basedata = ObjectData {
            .id = it.require <int> ("id", "Object must have an id"),
            .name = it.require <std::string> ("name", "Object must have a name"),
            .dependencies = parseDependencies (it),
        };
    } catch (const std::exception& e) {
        sLog.error ("Error parsing object base data: ", e.what ());
        const auto idIt = it.find ("id");
        const auto nameIt = it.find ("name");
        int id = (idIt != it.end () && idIt->is_number ()) ? idIt->get<int> () : -1;
        std::string name = "unknown";
        if (nameIt != it.end ()) {
            if (nameIt->is_string ()) {
                name = nameIt->get<std::string> ();
            } else if (nameIt->is_number ()) {
                name = std::to_string (nameIt->get<int> ());
            }
        }
        basedata = ObjectData { .id = id, .name = name, .dependencies = {} };
    }

    if (imageIt != it.end () && imageIt->is_string ()) {
        return parseImage (it, project, basedata, *imageIt);
    } else if (soundIt != it.end () && soundIt->is_array ()) {
        return parseSound (it, basedata);
    } else if (particleIt != it.end ()) {
        return parseParticle (it, project, basedata);
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
    const auto& animationLayers = it.optional ("animationlayers");

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
            .animationLayers = animationLayers.has_value () ? parseAnimationLayers (*animationLayers) : std::vector <ImageAnimationLayerUniquePtr> {},
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

std::vector <ImageAnimationLayerUniquePtr> ObjectParser::parseAnimationLayers (const JSON& it) {
    if (!it.is_array ()) {
        return {};
    }

    std::vector <ImageAnimationLayerUniquePtr> result = {};

    for (const auto& cur : it.items ()) {
        result.push_back (parseAnimationLayer (cur.value ()));
    }

    return result;
}

ImageAnimationLayerUniquePtr ObjectParser::parseAnimationLayer (const JSON& it) {
    return std::make_unique <ImageAnimationLayer> (ImageAnimationLayer {
        .id = it.require ("id", "Animation layer must have an id"),
        .rate = it.require ("rate", "Animation layer must have a rate"),
        .visible = it.require ("visible", "Animation layer must include visibility"),
        .blend = it.require ("blend", "Animation layer must include blend"),
        .animation = it.require ("animation", "Animation layer must include an animation"),
    });
}

ParticleUniquePtr ObjectParser::parseParticle (const JSON& it, const Project& project, ObjectData base) {
    try {
    const auto& properties = project.properties;
    const auto particleIt = it.find ("particle");

    if (particleIt == it.end ()) {
        sLog.error ("Particle object must have a particle definition");
        return std::make_unique <Particle> (
            std::move (base),
            ParticleData {
                .origin = it.user ("origin", properties, glm::vec3 (0.0f)),
                .scale = it.user ("scale", properties, glm::vec3 (1.0f)),
                .angles = it.user ("angles", properties, glm::vec3 (0.0f)),
                .visible = it.user ("visible", properties, true),
                .parallaxDepth = it.optional ("parallaxDepth", glm::vec2 (0.0f)),
                .particleFile = "",
                .animationMode = "sequence",
                .sequenceMultiplier = 1.0f,
                .maxCount = 100,
                .startTime = 0,
                .flags = 0,
                .material = nullptr,
                .emitters = {},
                .initializers = {},
                .operators = {},
                .renderers = {},
                .controlPoints = {},
                .children = {},
                .instanceOverride = {.enabled = false, .alpha = 1.0f, .size = 1.0f, .lifetime = 1.0f, .rate = 1.0f, .speed = 1.0f, .count = 1.0f, .color = glm::vec3 (1.0f)},
            }
        );
    }

    std::string particleFile;
    if (particleIt->is_string ()) {
        particleFile = particleIt->get<std::string> ();
    }

    // Load particle definition from file if it's a string reference
    JSON particleJson = JSON::object ();
    if (!particleFile.empty ()) {
        try {
            particleJson = WallpaperEngine::Data::JSON::JSON::parse (project.assetLocator->readString (particleFile));
        } catch (std::runtime_error& e) {
            sLog.error ("Cannot load particle file: ", particleFile, " - ", e.what ());
        }
    } else if (particleIt->is_object ()) {
        particleJson = *particleIt;
    }

    // Parse emitters (note: field is named "emitter" not "emitters")
    std::vector<ParticleEmitter> emitters;
    const auto emittersIt = particleJson.find ("emitter");
    if (emittersIt != particleJson.end () && emittersIt->is_array ()) {
        for (const auto& emitter : *emittersIt) {
            emitters.push_back (parseParticleEmitter (emitter));
        }
    }

    // Parse initializers (note: field is named "initializer" not "initializers")
    std::vector<ParticleInitializer> initializers;
    const auto initializersIt = particleJson.find ("initializer");
    if (initializersIt != particleJson.end () && initializersIt->is_array ()) {
        for (const auto& initializer : *initializersIt) {
            initializers.push_back (parseParticleInitializer (initializer));
        }
    }

    // Parse operators (note: field is named "operator" not "operators")
    std::vector<ParticleOperator> operators;
    const auto operatorsIt = particleJson.find ("operator");
    if (operatorsIt != particleJson.end () && operatorsIt->is_array ()) {
        for (const auto& op : *operatorsIt) {
            operators.push_back (parseParticleOperator (op));
        }
    }

    // Parse renderers (note: field is named "renderer" not "renderers")
    std::vector<ParticleRenderer> renderers;
    const auto renderersIt = particleJson.find ("renderer");
    if (renderersIt != particleJson.end () && renderersIt->is_array ()) {
        for (const auto& renderer : *renderersIt) {
            renderers.push_back (parseParticleRenderer (renderer));
        }
    }

    // Add default sprite renderer if none specified
    if (renderers.empty ()) {
        renderers.push_back (ParticleRenderer {
            .name = "sprite",
            .length = 0.05f,
            .maxLength = 10.0f,
            .subdivision = 3.0f,
        });
    }

    // Parse control points (note: field is named "controlpoint" not "controlpoints")
    std::vector<ParticleControlPoint> controlPoints;
    const auto controlPointsIt = particleJson.find ("controlpoint");
    if (controlPointsIt != particleJson.end () && controlPointsIt->is_array ()) {
        for (const auto& cp : *controlPointsIt) {
            controlPoints.push_back (parseParticleControlPoint (cp));
        }
    }

    // Parse children
    std::vector<ParticleChild> children;
    const auto childrenIt = particleJson.optional ("children");
    if (childrenIt.has_value () && childrenIt->is_array ()) {
        for (const auto& child : *childrenIt) {
            children.push_back (parseParticleChild (child, project));
        }
    }

    // Parse instance override
    ParticleInstanceOverride instanceOverride = {
        .enabled = false,
        .alpha = 1.0f,
        .size = 1.0f,
        .lifetime = 1.0f,
        .rate = 1.0f,
        .speed = 1.0f,
        .count = 1.0f,
        .color = glm::vec3 (1.0f),
    };
    const auto instanceOverrideIt = it.optional ("instanceoverride");
    if (instanceOverrideIt.has_value ()) {
        instanceOverride = parseParticleInstanceOverride (*instanceOverrideIt);
    }

    // Parse material - particles reference materials directly, not models
    ModelUniquePtr material = nullptr;
    const auto materialIt = particleJson.find ("material");
    if (materialIt != particleJson.end () && materialIt->is_string ()) {
        try {
            std::string materialPath = materialIt->get<std::string> ();

            // Particle materials are stored as just material definitions, not model files
            // So we need to wrap them in a model structure
            auto mat = MaterialParser::load (project, materialPath);

            material = std::make_unique <ModelStruct> (ModelStruct {
                .filename = materialPath,
                .material = std::move (mat),
                .solidlayer = false,
                .fullscreen = false,
                .passthrough = false,
                .autosize = false,
                .nopadding = false,
                .width = std::nullopt,
                .height = std::nullopt,
                .puppet = std::nullopt,
            });
        } catch (std::runtime_error& e) {
            sLog.error ("Cannot load particle material: ", materialIt->get<std::string> (), " - ", e.what ());
        }
    }

    // Parse string fields safely
    std::string animationMode = "sequence";
    const auto animModeIt = particleJson.find ("animationmode");
    if (animModeIt != particleJson.end () && animModeIt->is_string ()) {
        animationMode = animModeIt->get<std::string> ();
    }

    // Parse numeric fields safely
    float sequenceMultiplier = 1.0f;
    uint32_t maxCount = 100;
    uint32_t startTime = 0;
    uint32_t flags = 0;

    const auto seqMultIt = particleJson.find ("sequencemultiplier");
    if (seqMultIt != particleJson.end () && seqMultIt->is_number ()) {
        sequenceMultiplier = seqMultIt->get<float> ();
    }

    const auto maxCountIt = particleJson.find ("maxcount");
    if (maxCountIt != particleJson.end () && maxCountIt->is_number ()) {
        maxCount = maxCountIt->get<uint32_t> ();
    }

    const auto startTimeIt = particleJson.find ("starttime");
    if (startTimeIt != particleJson.end () && startTimeIt->is_number ()) {
        startTime = startTimeIt->get<uint32_t> ();
    }

    const auto flagsIt = particleJson.find ("flags");
    if (flagsIt != particleJson.end () && flagsIt->is_number ()) {
        flags = flagsIt->get<uint32_t> ();
    }

    return std::make_unique <Particle> (
        std::move (base),
        ParticleData {
            .origin = it.user ("origin", properties, glm::vec3 (0.0f)),
            .scale = it.user ("scale", properties, glm::vec3 (1.0f)),
            .angles = it.user ("angles", properties, glm::vec3 (0.0f)),
            .visible = it.user ("visible", properties, true),
            .parallaxDepth = it.optional ("parallaxDepth", glm::vec2 (0.0f)),
            .particleFile = particleFile,
            .animationMode = animationMode,
            .sequenceMultiplier = sequenceMultiplier,
            .maxCount = maxCount,
            .startTime = startTime,
            .flags = flags,
            .material = std::move (material),
            .emitters = std::move (emitters),
            .initializers = std::move (initializers),
            .operators = std::move (operators),
            .renderers = std::move (renderers),
            .controlPoints = std::move (controlPoints),
            .children = std::move (children),
            .instanceOverride = instanceOverride,
        }
    );
    } catch (nlohmann::json::exception& e) {
        sLog.error ("Error parsing particle '", base.name, "': ", e.what ());
        sLog.error ("Particle JSON: ", it.dump ());
        throw;
    }
}

ParticleEmitter ObjectParser::parseParticleEmitter (const JSON& it) {
    // Parse string name safely
    std::string name = "";
    const auto nameIt = it.find ("name");
    if (nameIt != it.end () && nameIt->is_string ()) {
        name = nameIt->get<std::string> ();
    }

    // Helper lambda to parse vec3 fields that might be strings, arrays, or missing
    auto parseVec3 = [&](const char* fieldName, const glm::vec3& defaultValue) -> glm::vec3 {
        const auto fieldIt = it.find (fieldName);
        if (fieldIt == it.end ()) {
            return defaultValue;
        }
        if (fieldIt->is_string ()) {
            return it.optional (fieldName, defaultValue);
        }
        if (fieldIt->is_array () && fieldIt->size () >= 3) {
            return glm::vec3 (
                (*fieldIt)[0].get<float> (),
                (*fieldIt)[1].get<float> (),
                (*fieldIt)[2].get<float> ()
            );
        }
        return defaultValue;
    };

    auto parseIVec3 = [&](const char* fieldName, const glm::ivec3& defaultValue) -> glm::ivec3 {
        const auto fieldIt = it.find (fieldName);
        if (fieldIt == it.end ()) {
            return defaultValue;
        }
        if (fieldIt->is_array () && fieldIt->size () >= 3) {
            return glm::ivec3 (
                (*fieldIt)[0].get<int> (),
                (*fieldIt)[1].get<int> (),
                (*fieldIt)[2].get<int> ()
            );
        }
        return defaultValue;
    };

    try {
        return ParticleEmitter {
            .id = it.optional ("id", -1),
            .name = name,
            .directions = parseVec3 ("directions", glm::vec3 (1.0f, 1.0f, 0.0f)),
            .distanceMin = parseVec3 ("distancemin", glm::vec3 (0.0f)),
            .distanceMax = parseVec3 ("distancemax", glm::vec3 (256.0f)),
            .origin = parseVec3 ("origin", glm::vec3 (0.0f)),
            .sign = parseIVec3 ("sign", glm::ivec3 (0)),
            .instantaneous = it.optional ("instantaneous", 0u),
            .speedMin = it.optional ("speedmin", 0.0f),
            .speedMax = it.optional ("speedmax", 0.0f),
            .rate = it.optional ("rate", 5.0f),
            .controlPoint = it.optional ("controlpoint", 0),
            .flags = it.optional ("flags", 0u),
        };
    } catch (nlohmann::json::exception& e) {
        sLog.error ("Error parsing emitter: ", e.what ());
        sLog.error ("Emitter JSON: ", it.dump ());
        throw;
    }
}

ParticleInitializer ObjectParser::parseParticleInitializer (const JSON& it) {
    std::string name = "";
    const auto nameIt = it.find ("name");
    if (nameIt != it.end () && nameIt->is_string ()) {
        name = nameIt->get<std::string> ();
    }

    return ParticleInitializer {
        .name = name,
        .json = it,
    };
}

ParticleOperator ObjectParser::parseParticleOperator (const JSON& it) {
    std::string name = "";
    const auto nameIt = it.find ("name");
    if (nameIt != it.end () && nameIt->is_string ()) {
        name = nameIt->get<std::string> ();
    }

    return ParticleOperator {
        .name = name,
        .json = it,
    };
}

ParticleRenderer ObjectParser::parseParticleRenderer (const JSON& it) {
    std::string name = "sprite";
    const auto nameIt = it.find ("name");
    if (nameIt != it.end () && nameIt->is_string ()) {
        name = nameIt->get<std::string> ();
    }

    return ParticleRenderer {
        .name = name,
        .length = it.optional ("length", 0.05f),
        .maxLength = it.optional ("maxlength", 10.0f),
        .subdivision = it.optional ("subdivision", 3.0f),
    };
}

ParticleControlPoint ObjectParser::parseParticleControlPoint (const JSON& it) {
    // Parse offset - can be string "x y z" or array [x,y,z]
    glm::vec3 offset (0.0f);
    const auto offsetIt = it.find ("offset");
    if (offsetIt != it.end ()) {
        if (offsetIt->is_string ()) {
            // Parse string format "x y z"
            std::string offsetStr = offsetIt->get<std::string> ();
            std::istringstream iss (offsetStr);
            iss >> offset.x >> offset.y >> offset.z;
        } else {
            // Try parsing as vec3 directly
            try {
                offset = it.optional ("offset", glm::vec3 (0.0f));
            } catch (...) {
                offset = glm::vec3 (0.0f);
            }
        }
    }

    return ParticleControlPoint {
        .id = it.optional ("id", -1),
        .flags = it.optional ("flags", 0u),
        .offset = offset,
    };
}

ParticleChild ObjectParser::parseParticleChild (const JSON& it, const Project& project) {
    std::string particleFile = "";
    const auto particleIt = it.find ("particle");
    if (particleIt != it.end () && particleIt->is_string ()) {
        particleFile = particleIt->get<std::string> ();
    }

    std::string type = "static";
    const auto typeIt = it.find ("type");
    if (typeIt != it.end () && typeIt->is_string ()) {
        type = typeIt->get<std::string> ();
    }

    std::string name = "";
    const auto nameIt = it.find ("name");
    if (nameIt != it.end () && nameIt->is_string ()) {
        name = nameIt->get<std::string> ();
    }

    // Helper lambda to parse vec3 fields
    auto parseVec3 = [&](const char* fieldName, const glm::vec3& defaultValue) -> glm::vec3 {
        const auto fieldIt = it.find (fieldName);
        if (fieldIt == it.end ()) {
            return defaultValue;
        }
        if (fieldIt->is_string ()) {
            return it.optional (fieldName, defaultValue);
        }
        if (fieldIt->is_array () && fieldIt->size () >= 3) {
            return glm::vec3 (
                (*fieldIt)[0].get<float> (),
                (*fieldIt)[1].get<float> (),
                (*fieldIt)[2].get<float> ()
            );
        }
        return defaultValue;
    };

    return ParticleChild {
        .type = type,
        .name = name,
        .maxCount = it.optional ("maxcount", 20),
        .controlPointStartIndex = it.optional ("controlpointstartindex", 0),
        .probability = it.optional ("probability", 1.0f),
        .angles = parseVec3 ("angles", glm::vec3 (0.0f)),
        .origin = parseVec3 ("origin", glm::vec3 (0.0f)),
        .scale = parseVec3 ("scale", glm::vec3 (1.0f)),
        .particleFile = particleFile,
    };
}

ParticleInstanceOverride ObjectParser::parseParticleInstanceOverride (const JSON& it) {
    // Helper lambda to parse vec3 fields
    auto parseVec3 = [&](const char* fieldName, const glm::vec3& defaultValue) -> glm::vec3 {
        const auto fieldIt = it.find (fieldName);
        if (fieldIt == it.end ()) {
            return defaultValue;
        }
        if (fieldIt->is_string ()) {
            return it.optional (fieldName, defaultValue);
        }
        if (fieldIt->is_array () && fieldIt->size () >= 3) {
            return glm::vec3 (
                (*fieldIt)[0].get<float> (),
                (*fieldIt)[1].get<float> (),
                (*fieldIt)[2].get<float> ()
            );
        }
        return defaultValue;
    };

    return ParticleInstanceOverride {
        .enabled = true,
        .alpha = it.optional ("alpha", 1.0f),
        .size = it.optional ("size", 1.0f),
        .lifetime = it.optional ("lifetime", 1.0f),
        .rate = it.optional ("rate", 1.0f),
        .speed = it.optional ("speed", 1.0f),
        .count = it.optional ("count", 1.0f),
        .color = parseVec3 ("color", glm::vec3 (1.0f)),
    };
}