#include "ObjectParser.h"
#include "ModelParser.h"
#include "MaterialParser.h"
#include "EffectParser.h"

#include "ShaderConstantParser.h"
#include "WallpaperEngine/Data/Model/Object.h"
#include "WallpaperEngine/Data/Model/Project.h"
#include "WallpaperEngine/Logging/Log.h"

#include <glm/gtc/constants.hpp>
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
        sLog.error ("Unknown object type found: ", it.dump ());
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
                .instanceOverride = {
                    .enabled = std::make_unique<UserSetting> (UserSetting {.value = std::make_unique<DynamicValue> (false), .property = nullptr, .condition = std::nullopt}),
                    .alpha = std::make_unique<UserSetting> (UserSetting {.value = std::make_unique<DynamicValue> (1.0f), .property = nullptr, .condition = std::nullopt}),
                    .size = std::make_unique<UserSetting> (UserSetting {.value = std::make_unique<DynamicValue> (1.0f), .property = nullptr, .condition = std::nullopt}),
                    .lifetime = std::make_unique<UserSetting> (UserSetting {.value = std::make_unique<DynamicValue> (1.0f), .property = nullptr, .condition = std::nullopt}),
                    .rate = std::make_unique<UserSetting> (UserSetting {.value = std::make_unique<DynamicValue> (1.0f), .property = nullptr, .condition = std::nullopt}),
                    .speed = std::make_unique<UserSetting> (UserSetting {.value = std::make_unique<DynamicValue> (1.0f), .property = nullptr, .condition = std::nullopt}),
                    .count = std::make_unique<UserSetting> (UserSetting {.value = std::make_unique<DynamicValue> (1.0f), .property = nullptr, .condition = std::nullopt}),
                    .color = std::make_unique<UserSetting> (UserSetting {.value = std::make_unique<DynamicValue> (glm::vec3 (1.0f)), .property = nullptr, .condition = std::nullopt}),
                    .colorn = std::make_unique<UserSetting> (UserSetting {.value = std::make_unique<DynamicValue> (glm::vec3 (1.0f)), .property = nullptr, .condition = std::nullopt})
                },
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
    std::vector<ParticleInitializerUniquePtr> initializers;
    const auto initializersIt = particleJson.find ("initializer");
    if (initializersIt != particleJson.end () && initializersIt->is_array ()) {
        for (const auto& initializer : *initializersIt) {
            auto init = parseParticleInitializer (initializer, project.properties);
            if (init) {
                initializers.push_back (std::move (init));
            }
        }
    }

    // Parse operators (note: field is named "operator" not "operators")
    std::vector<ParticleOperatorUniquePtr> operators;
    const auto operatorsIt = particleJson.find ("operator");
    if (operatorsIt != particleJson.end () && operatorsIt->is_array ()) {
        for (const auto& op : *operatorsIt) {
            auto oper = parseParticleOperator (op, project.properties);
            if (oper) {
                operators.push_back (std::move (oper));
            }
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
            .minLength = 0.0f,
            .subdivision = 1.0f,
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
        .enabled = std::make_unique<UserSetting> (UserSetting {
            .value = std::make_unique<DynamicValue> (false),
            .property = nullptr,
            .condition = std::nullopt
        }),
        .alpha = std::make_unique<UserSetting> (UserSetting {
            .value = std::make_unique<DynamicValue> (1.0f),
            .property = nullptr,
            .condition = std::nullopt
        }),
        .size = std::make_unique<UserSetting> (UserSetting {
            .value = std::make_unique<DynamicValue> (1.0f),
            .property = nullptr,
            .condition = std::nullopt
        }),
        .lifetime = std::make_unique<UserSetting> (UserSetting {
            .value = std::make_unique<DynamicValue> (1.0f),
            .property = nullptr,
            .condition = std::nullopt
        }),
        .rate = std::make_unique<UserSetting> (UserSetting {
            .value = std::make_unique<DynamicValue> (1.0f),
            .property = nullptr,
            .condition = std::nullopt
        }),
        .speed = std::make_unique<UserSetting> (UserSetting {
            .value = std::make_unique<DynamicValue> (1.0f),
            .property = nullptr,
            .condition = std::nullopt
        }),
        .count = std::make_unique<UserSetting> (UserSetting {
            .value = std::make_unique<DynamicValue> (1.0f),
            .property = nullptr,
            .condition = std::nullopt
        }),
        .color = std::make_unique<UserSetting> (UserSetting {
            .value = std::make_unique<DynamicValue> (glm::vec3 (1.0f)),
            .property = nullptr,
            .condition = std::nullopt
        }),
        .colorn = std::make_unique<UserSetting> (UserSetting {
            .value = std::make_unique<DynamicValue> (glm::vec3 (1.0f)),
            .property = nullptr,
            .condition = std::nullopt
        })
    };
    const auto instanceOverrideIt = it.optional ("instanceoverride");
    if (instanceOverrideIt.has_value ()) {
        instanceOverride = parseParticleInstanceOverride (*instanceOverrideIt, project.properties);
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
            .instanceOverride = std::move (instanceOverride),
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

    // Helper lambda to parse vec3 fields that might be strings, arrays, single numbers, or missing
    auto parseVec3 = [&](const char* fieldName, const glm::vec3& defaultValue) -> glm::vec3 {
        const auto fieldIt = it.find (fieldName);
        if (fieldIt == it.end ()) {
            return defaultValue;
        }
        if (fieldIt->is_string ()) {
            return it.optional (fieldName, defaultValue);
        }
        if (fieldIt->is_number ()) {
            // Single number - use for all components (common for distancemax/distancemin)
            float val = fieldIt->get<float> ();
            return glm::vec3 (val, val, val);
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

    auto parseVec2 = [&](const char* fieldName, const glm::vec2& defaultValue) -> glm::vec2 {
        const auto fieldIt = it.find (fieldName);
        if (fieldIt == it.end ()) {
            return defaultValue;
        }
        if (fieldIt->is_string ()) {
            return it.optional (fieldName, defaultValue);
        }
        if (fieldIt->is_array () && fieldIt->size () >= 2) {
            return glm::vec2 (
                (*fieldIt)[0].get<float> (),
                (*fieldIt)[1].get<float> ()
            );
        }
        return defaultValue;
    };

    try {
        return ParticleEmitter {
            .id = it.optional ("id", -1),
            .name = name,
            .directions = parseVec3 ("directions", glm::vec3 (1.0f, 1.0f, 0.0f)),
            .distanceMin = parseVec3 ("distancemin", glm::vec3 (0.0f, 0.0f, 0.0f)),
            .distanceMax = parseVec3 ("distancemax", glm::vec3 (256.0f, 256.0f, 0.0f)),
            .origin = parseVec3 ("origin", glm::vec3 (0.0f)),
            .sign = parseIVec3 ("sign", glm::ivec3 (0)),
            .instantaneous = it.optional ("instantaneous", 0u),
            .speedMin = it.optional ("speedmin", 0.0f),
            .speedMax = it.optional ("speedmax", 0.0f),
            .rate = it.optional ("rate", 10.0f),
            .controlPoint = it.optional ("controlpoint", 0),
            .flags = it.optional ("flags", 0u),
            .cone = it.optional ("cone", 0.0f),
            .delay = it.optional ("delay", 0.0f),
            .duration = it.optional ("duration", 0.0f),
            .audioProcessingBounds = parseVec2 ("audioprocessingbounds", glm::vec2 (0.8f, 1.0f)),
            .audioProcessingExponent = it.optional ("audioprocessingexponent", 2),
            .audioProcessingFrequencyStart = it.optional ("audioprocessingfrequencystart", 0),
            .audioProcessingFrequencyEnd = it.optional ("audioprocessingfrequencyend", 1),
            .audioProcessingMode = it.optional ("audioprocessingmode", 0),
            .minPeriodicDelay = it.optional ("minperiodicdelay", 1.0f),
            .maxPeriodicDelay = it.optional ("maxperiodicdelay", 2.0f),
            .minPeriodicDuration = it.optional ("minperiodicduration", 2.0f),
            .maxPeriodicDuration = it.optional ("maxperiodicduration", 3.0f),
        };
    } catch (nlohmann::json::exception& e) {
        sLog.error ("Error parsing emitter: ", e.what ());
        sLog.error ("Emitter JSON: ", it.dump ());
        throw;
    }
}

ParticleInitializerUniquePtr ObjectParser::parseParticleInitializer (const JSON& it, const Properties& properties) {
    std::string name = it.optional<std::string> ("name", "");

    if (name == "colorrandom") {
        // Only normalize if there's no property connection or values are > 1.0
        auto minSetting = it.user ("min", properties, glm::vec3 (0.0f));
        auto maxSetting = it.user ("max", properties, glm::vec3 (255.0f));

        auto minVec = minSetting->value->getVec3 ();
        if (minSetting->property == nullptr &&
            (minVec.x > 1.0f || minVec.y > 1.0f || minVec.z > 1.0f)) {
            minSetting->value->update (minVec / 255.0f);
        }

        auto maxVec = maxSetting->value->getVec3 ();
        if (maxSetting->property == nullptr &&
            (maxVec.x > 1.0f || maxVec.y > 1.0f || maxVec.z > 1.0f)) {
            maxSetting->value->update (maxVec / 255.0f);
        }

        return std::make_unique<ColorRandomInitializer> (std::move (minSetting), std::move (maxSetting));
    } else if (name == "sizerandom") {
        return std::make_unique<SizeRandomInitializer> (
            it.user ("min", properties, 0.0f),
            it.user ("max", properties, 20.0f),
            it.user ("exponent", properties, 1.0f)
        );
    } else if (name == "alpharandom") {
        return std::make_unique<AlphaRandomInitializer> (
            it.user ("min", properties, 0.05f),
            it.user ("max", properties, 1.0f)
        );
    } else if (name == "lifetimerandom") {
        return std::make_unique<LifetimeRandomInitializer> (
            it.user ("min", properties, 0.0f),
            it.user ("max", properties, 1.0f)
        );
    } else if (name == "velocityrandom") {
        return std::make_unique<VelocityRandomInitializer> (
            it.user ("min", properties, glm::vec3 (-32.0f)),
            it.user ("max", properties, glm::vec3 (32.0f))
        );
    } else if (name == "rotationrandom") {
        return std::make_unique<RotationRandomInitializer> (
            it.user ("min", properties, glm::vec3 (0.0f)),
            it.user ("max", properties, glm::vec3 (0.0f, 0.0f, glm::two_pi<float>()))
        );
    } else if (name == "angularvelocityrandom") {
        return std::make_unique<AngularVelocityRandomInitializer> (
            it.user ("min", properties, glm::vec3 (0.0f, 0.0f, -5.0f)),
            it.user ("max", properties, glm::vec3 (0.0f, 0.0f, 5.0f)),
            it.user ("exponent", properties, 1.0f)
        );
    } else if (name == "turbulentvelocityrandom") {
        return std::make_unique<TurbulentVelocityRandomInitializer> (
            it.user ("speedmin", properties, 100.0f),
            it.user ("speedmax", properties, 250.0f),
            it.user ("scale", properties, 1.0f),
            it.user ("offset", properties, 0.0f),
            it.user ("forward", properties, glm::vec3 (0.0f, 1.0f, 0.0f)),
            it.user ("timescale", properties, 1.0f),
            it.user ("phasemin", properties, 0.0f),
            it.user ("phasemax", properties, 0.1f),
            it.user ("right", properties, glm::vec3 (0.0f, 0.0f, 1.0f))
        );
    } else if (name == "mapsequencearoundcontrolpoint") {
        return std::make_unique<MapSequenceAroundControlPointInitializer> (
            it.user ("controlpoint", properties, 0),
            it.user ("count", properties, 1),
            it.user ("speedmin", properties, glm::vec3 (0.0f)),
            it.user ("speedmax", properties, glm::vec3 (100.0f))
        );
    }

    return nullptr;
}

ParticleOperatorUniquePtr ObjectParser::parseParticleOperator (const JSON& it, const Properties& properties) {
    std::string name = it.optional<std::string> ("name", "");

    if (name == "movement") {
        return std::make_unique<MovementOperator> (
            it.user ("drag", properties, 0.0f),
            it.user ("gravity", properties, glm::vec3 (0.0f))
        );
    } else if (name == "angularmovement") {
        return std::make_unique<AngularMovementOperator> (
            it.user ("drag", properties, 0.0f),
            it.user ("force", properties, glm::vec3 (0.0f))
        );
    } else if (name == "alphafade") {
        return std::make_unique<AlphaFadeOperator> (
            it.user ("fadeintime", properties, 0.5f),
            it.user ("fadeouttime", properties, 0.5f)
        );
    } else if (name == "sizechange") {
        return std::make_unique<SizeChangeOperator> (
            it.user ("starttime", properties, 0.0f),
            it.user ("endtime", properties, 1.0f),
            it.user ("startvalue", properties, 1.0f),
            it.user ("endvalue", properties, 0.0f)
        );
    } else if (name == "alphachange") {
        return std::make_unique<AlphaChangeOperator> (
            it.user ("starttime", properties, 0.0f),
            it.user ("endtime", properties, 1.0f),
            it.user ("startvalue", properties, 1.0f),
            it.user ("endvalue", properties, 0.0f)
        );
    } else if (name == "colorchange") {
        return std::make_unique<ColorChangeOperator> (
            it.user ("starttime", properties, 0.0f),
            it.user ("endtime", properties, 1.0f),
            it.user ("startvalue", properties, glm::vec3 (1.0f)),
            it.user ("endvalue", properties, glm::vec3 (1.0f))
        );
    } else if (name == "turbulence") {
        return std::make_unique<TurbulenceOperator> (
            it.user ("scale", properties, 0.005f),
            it.user ("speedmin", properties, 500.0f),
            it.user ("speedmax", properties, 1000.0f),
            it.user ("timescale", properties, 0.01f),
            it.user ("mask", properties, glm::vec3 (1.0f, 1.0f, 0.0f)),
            it.user ("phasemin", properties, 0.0f),
            it.user ("phasemax", properties, 0.0f),
            it.user ("audioprocessingmode", properties, 0),
            it.user ("audioprocessingbounds", properties, glm::vec2 (0.0f, 1.0f)),
            it.user ("audioprocessingexponent", properties, 1.0f),
            it.user ("audioprocessingfrequencystart", properties, 0),
            it.user ("audioprocessingfrequencyend", properties, 15)
        );
    } else if (name == "vortex" || name == "vortex_v2") {
        return std::make_unique<VortexOperator> (
            it.optional ("controlpoint", 0),
            it.optional ("flags", 0),  // 1 = infinite axis, 2 = maintain distance, 4 = ring shape
            it.user ("axis", properties, glm::vec3 (0.0f, 0.0f, 1.0f)),
            it.user ("offset", properties, glm::vec3 (0.0f)),
            it.user ("distanceinner", properties, 500.0f),
            it.user ("distanceouter", properties, 650.0f),
            it.user ("speedinner", properties, 2500.0f),
            it.user ("speedouter", properties, 0.0f),
            it.user ("centerforce", properties, 1.0f),
            it.user ("ringradius", properties, 300.0f),
            it.user ("ringwidth", properties, 50.0f),
            it.user ("ringpulldistance", properties, 50.0f),
            it.user ("ringpullforce", properties, 10.0f),
            it.user ("audioprocessingmode", properties, 0),
            it.user ("audioprocessingbounds", properties, glm::vec2 (0.0f, 1.0f))
        );
    } else if (name == "controlpointattract") {
        return std::make_unique<ControlPointAttractOperator> (
            it.optional ("controlpoint", 0),
            it.user ("origin", properties, glm::vec3 (0.0f)),
            it.user ("scale", properties, 100.0f),
            it.user ("threshold", properties, 1000.0f)
        );
    } else if (name == "oscillatealpha") {
        return std::make_unique<OscillateAlphaOperator> (
            it.user ("frequencymin", properties, 0.0f),
            it.user ("frequencymax", properties, 10.0f),
            it.user ("scalemin", properties, 0.0f),
            it.user ("scalemax", properties, 1.0f),
            it.user ("phasemin", properties, 0.0f),
            it.user ("phasemax", properties, static_cast<float> (2.0 * M_PI))
        );
    } else if (name == "oscillatesize") {
        return std::make_unique<OscillateSizeOperator> (
            it.user ("frequencymin", properties, 0.0f),
            it.user ("frequencymax", properties, 10.0f),
            it.user ("scalemin", properties, 0.8f),
            it.user ("scalemax", properties, 1.2f),
            it.user ("phasemin", properties, 0.0f),
            it.user ("phasemax", properties, static_cast<float> (2.0 * M_PI))
        );
    } else if (name == "oscillateposition") {
        return std::make_unique<OscillatePositionOperator> (
            it.user ("frequencymin", properties, 0.0f),
            it.user ("frequencymax", properties, 5.0f),
            it.user ("scalemin", properties, 0.0f),
            it.user ("scalemax", properties, 10.0f),
            it.user ("phasemin", properties, 0.0f),
            it.user ("phasemax", properties, static_cast<float> (2.0 * M_PI)),
            it.user ("mask", properties, glm::vec3 (1.0f, 1.0f, 0.0f))
        );
    }

    return nullptr;
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
        .minLength = it.optional ("minlength", 0.0f),
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
        .lockToPointer = it.optional ("locktopointer", false),
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

    // Helper lambda to parse vec3 fields that might be strings, arrays, single numbers, or missing
    auto parseVec3 = [&](const char* fieldName, const glm::vec3& defaultValue) -> glm::vec3 {
        const auto fieldIt = it.find (fieldName);
        if (fieldIt == it.end ()) {
            return defaultValue;
        }
        if (fieldIt->is_string ()) {
            return it.optional (fieldName, defaultValue);
        }
        if (fieldIt->is_number ()) {
            // Single number - use for all components
            float val = fieldIt->get<float> ();
            return glm::vec3 (val, val, val);
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

ParticleInstanceOverride ObjectParser::parseParticleInstanceOverride (const JSON& it, const Properties& properties) {
    return ParticleInstanceOverride {
        .enabled = it.user ("enabled", properties, true),
        .alpha = it.user ("alpha", properties, 1.0f),
        .size = it.user ("size", properties, 1.0f),
        .lifetime = it.user ("lifetime", properties, 1.0f),
        .rate = it.user ("rate", properties, 1.0f),
        .speed = it.user ("speed", properties, 1.0f),
        .count = it.user ("count", properties, 1.0f),
        .color = it.user ("color", properties, glm::vec3 (1.0f)),
        .colorn = it.user ("colorn", properties, glm::vec3 (1.0f)),
    };
}