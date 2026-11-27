#include "ObjectParser.h"
#include "EffectParser.h"
#include "MaterialParser.h"
#include "MdlAnimationParser.h"
#include "MdlParser.h"
#include "ModelParser.h"

#include "ShaderConstantParser.h"
#include "TextureParser.h"
#include "UserSettingParser.h"
#include "WallpaperEngine/Data/Builders/ColorBuilder.h"
#include "WallpaperEngine/Data/Builders/UserSettingBuilder.h"
#include "WallpaperEngine/Data/Model/DynamicValue.h"
#include "WallpaperEngine/Data/Model/Object.h"
#include "WallpaperEngine/Data/Model/Project.h"
#include "WallpaperEngine/Data/Model/Property.h"
#include "WallpaperEngine/Data/Utils/JsonTelemetry.h"
#include "WallpaperEngine/Logging/Log.h"
#include "WallpaperEngine/Maths.h"

#include <cstdlib>
#include <cstring>
#include <format>
#include <glm/gtc/constants.hpp>
#include <limits>
#include <sstream>
#include <unordered_map>

using namespace WallpaperEngine::Data::Parsers;
using namespace WallpaperEngine::Data::Model;

ObjectData ObjectParser::parseBase (const JSON& it, const Project& project) {
    // Parse base object data
    // Some particle objects have numeric 'name' fields, so handle type mismatches gracefully
    ObjectData basedata;
    try {
	basedata = ObjectData {
	    .id = it.require<int> ("id", "Object must have an id"),
	    .name = it.require<std::string> ("name", "Object must have a name"),
	    .dependencies = parseDependencies (it),
	    .parent = it.optional<int> ("parent"),
	    .attachment = it.optional<std::string> ("attachment"),
	    .origin = it.user ("origin", project.properties, glm::vec3 (0.0f)),
	    .locktransforms = it.optional ("locktransforms", false),
	    .authoredParallaxDepth = it.find ("parallaxDepth") != it.end ()
		? std::optional (it.user ("parallaxDepth", project.properties, glm::vec2 (0.0f))->value->getVec2 ())
		: std::nullopt,
	    .groupScale = it.user ("scale", project.properties, glm::vec3 (1.0f)),
	    .groupAngles = it.user ("angles", project.properties, glm::vec3 (0.0f)),
	    .groupVisible = it.user ("visible", project.properties, true),
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
	basedata = ObjectData {
	    .id = id,
	    .name = name,
	    .dependencies = parseDependencies (it),
	    .parent = it.optional<int> ("parent"),
	    .attachment = it.optional<std::string> ("attachment"),
	    .origin = it.user ("origin", project.properties, glm::vec3 (0.0f)),
	    .locktransforms = it.optional ("locktransforms", false),
	    .authoredParallaxDepth = it.find ("parallaxDepth") != it.end ()
		? std::optional (it.user ("parallaxDepth", project.properties, glm::vec2 (0.0f))->value->getVec2 ())
		: std::nullopt,
	    .groupScale = it.user ("scale", project.properties, glm::vec3 (1.0f)),
	    .groupAngles = it.user ("angles", project.properties, glm::vec3 (0.0f)),
	    .groupVisible = it.user ("visible", project.properties, true),
	};
    }

    basedata.typedDependencies = parseTypedDependencies (it);
    basedata.initialConfiguration = it.dump ();
    basedata.disablePropagation = it.optional ("disablepropagation", false);
    basedata.reflected = it.optional ("reflected", true);

    return basedata;
}

ObjectUniquePtr ObjectParser::parse (const JSON& it, const Project& project) {
    // scenes emit every layer-type key on every object and set the unused ones to JSON
    // null, so presence alone does not identify the type; image/sound/model/light are
    // already type-checked below, particle/text/shape must reject nulls the same way
    const auto imageIt = it.find ("image");
    const auto soundIt = it.find ("sound");
    const auto particleIt = it.find ("particle");
    const auto textIt = it.find ("text");
    const auto modelIt = it.find ("model");
    const auto lightIt = it.find ("light");
    const auto shapeIt = it.find ("shape");
    // the runtime camera of a 3D scene is an object too, but WallpaperParser consumes it
    // (origin/angles/fov/zoom plus its path/queuemode) before the object list is built
    const auto cameraIt = it.find ("camera");

    ObjectData basedata = parseBase (it, project);

    if (imageIt != it.end () && imageIt->is_string ()) {
	return parseImage (it, project, std::move (basedata), *imageIt);
    } else if (soundIt != it.end () && soundIt->is_array ()) {
	return parseSound (it, project, std::move (basedata));
    } else if (particleIt != it.end () && !particleIt->is_null ()) {
	return parseParticle (it, project, std::move (basedata));
    } else if (textIt != it.end () && !textIt->is_null ()) {
	return parseText (it, project, std::move (basedata));
    } else if (modelIt != it.end () && modelIt->is_string ()) {
	return parseModel3D (it, project, std::move (basedata), *modelIt);
    } else if (lightIt != it.end () && lightIt->is_string ()) {
	return parseLight (it, project, std::move (basedata), *lightIt);
    } else if (shapeIt != it.end () && !shapeIt->is_null ()) {
	if (shapeIt->is_string () && shapeIt->get<std::string> () == "quad") {
	    const auto effects = it.optional ("effects");
	    if (effects && effects->size () > 1) {
		sLog.error ("Shapes with multiple effects are unsupported");
		return std::make_unique<Object> (std::move (basedata));
	    }
	    auto parsedEffects = effects ? parseEffects (*effects, project) : std::vector<ImageEffectUniquePtr> {};
	    for (const auto& effect : parsedEffects) {
		bool direct = effect->effect->fbos.empty ();
		for (const auto& pass : effect->effect->passes) {
		    direct &= pass->material.has_value () && !pass->target && !pass->source && !pass->command;
		}
		if (!direct) {
		    sLog.error ("Shapes require direct material effects without private targets");
		    return std::make_unique<Object> (std::move (basedata));
		}
		for (const auto& pass : effect->passOverrides) pass->combos.insert_or_assign ("DIRECTDRAW", 1);
	    }
	    return std::make_unique<Shape> (std::move (basedata), ShapeData {
		.perspective = it.user ("perspective", project.properties, false),
		.parallaxDepth = it.user ("parallaxDepth", project.properties, glm::vec2 (1.0f)),
		.alpha = it.user ("alpha", project.properties, 1.0f),
		.color = it.color ("color", project.properties, Builders::ColorBuilder::White),
		.brightness = it.user ("brightness", project.properties, 1.0f),
		.effects = std::move (parsedEffects),
	    });
	}
	sLog.error ("Unsupported shape type: ", shapeIt->dump ());
    } else if (cameraIt != it.end () && cameraIt->is_string ()) {
	// Projection is handled by WallpaperParser; retain the native false draw gate.
	basedata.cameraLayer = true;
    }

    // Native file and script factories retain untyped objects as generic layers.
    return std::make_unique<Object> (std::move (basedata));
}

std::vector<int> ObjectParser::parseDependencies (const JSON& it) {
    const auto dependenciesIt = it.find ("dependencies");

    if (dependenciesIt == it.end () || !dependenciesIt->is_array ()) {
	return {};
    }

    std::vector<int> result = {};

    for (const auto& cur : *dependenciesIt) {
	if (cur.is_number ()) {
	    result.push_back (cur);
	} else if (cur.is_object ()) {
	    // newer scenes author entries as {"id": 104, "index": 0, "type": "collisionmodel"}
	    // instead of a bare object id. Retain IDs for creation/render ordering;
	    // typed bindings retain their type and index separately
	    if (const auto id = cur.optional<int> ("id"); id.has_value ()) {
		result.push_back (*id);
	    }
	}
    }

    return result;
}

std::vector<ObjectDependency> ObjectParser::parseTypedDependencies (const JSON& it) {
    const auto dependencies = it.find ("dependencies");
    std::vector<ObjectDependency> result;
    if (dependencies == it.end () || !dependencies->is_array ()) return result;
    for (const auto& dependency : *dependencies) {
	if (!dependency.is_object ()) continue;
	const auto type = dependency.optional<std::string> ("type");
	const auto id = dependency.optional<int> ("id");
	const auto index = dependency.optional<uint32_t> ("index");
	if (id && index && type) result.push_back ({ *id, *index, *type, dependency.optional ("mask", std::string ()) });
    }
    return result;
}

SoundUniquePtr ObjectParser::parseSound (const JSON& it, const Project& project, ObjectData base) {
    const auto soundIt = it.require ("sound", "Object must have a sound");
    std::vector<std::string> sounds = {};

    for (const auto& cur : soundIt) {
	sounds.push_back (cur);
    }

    return std::make_unique<Sound> (
	std::move (base),
	SoundData {
	    .volume = it.user ("volume", project.properties, 1.0f),
	    .spatialization = it.user ("spatialization", project.properties, false),
	    .attenuation = it.user ("attenuation", project.properties, 1.0f),
	    .minDistance = it.user ("mindistance", project.properties, 1.0f),
	    .startSilent = it.optional ("startsilent", false),
	    .minTime = it.user ("mintime", project.properties, 1.0f),
	    .maxTime = it.user ("maxtime", project.properties, 5.0f),
	    .playbackmode = it.optional<std::string> ("playbackmode"),
	    .sounds = sounds,
	}
    );
}

TextUniquePtr ObjectParser::parseText (const JSON& it, const Project& project, ObjectData base) {
    const auto& effects = it.optional ("effects");
    // Unlike an omitted property, an explicit null/invalid enum selects disabled.
    auto depthTest = it.find ("depthtest") != it.end ()
	? it.user ("depthtest", project.properties)
	: it.user ("depthtest", project.properties, std::string ("enabled"));
    // Evaluate conditional bindings before enum conversion. Rewriting a value
    // with an attached condition would convert the canonical string back to bool.
    std::shared_ptr<DynamicValue> conditionalSource;
    if (depthTest->property && depthTest->condition) {
	conditionalSource = std::move (depthTest->value);
	depthTest->value = std::make_unique<DynamicValue> ();
	if (const auto& script = conditionalSource->getScriptSource ()) {
	    depthTest->value->setScriptSource (*script);
	}
	depthTest->value->setProperties (std::move (conditionalSource->getProperties ()));
	depthTest->value->connect (conditionalSource.get ());
    }
    const auto normalizeDepthTest = [value = depthTest->value.get (), conditionalSource] (
	const DynamicValue&, const DynamicValue::UpdateSource source
    ) {
	if (value->getType () != DynamicValue::String
	    || (value->getString () != "enabled" && value->getString () != "disabled")) {
	    value->update (std::string ("disabled"), source);
	}
    };
    normalizeDepthTest (*depthTest->value, DynamicValue::Initialization);
    // Keep the condition source alive until the enum value disconnects from it.
    depthTest->value->listen (normalizeDepthTest);

    return std::make_unique<Text> (
	std::move (base),
	TextData {
	    .text = it.user ("text", project.properties),
	    .font = it.optional ("font", std::string ()),
	    .msdf = it.user ("msdf", project.properties, false),
	    .outline = it.user ("outline", project.properties, false),
	    .outlineWidth = it.user ("outlinethickness", project.properties, 4.0f),
	    .outlineColor = it.color ("outlinecolor", project.properties, Builders::ColorBuilder::Black),
	    .blur = it.user ("blur", project.properties, false),
	    .blurRadius = it.user ("blursize", project.properties, 6.0f),
	    .dropShadow = it.user ("dropshadow", project.properties, false),
	    .dropShadowRadius = it.user ("dropshadowsize", project.properties, 6.0f),
	    .dropShadowOpacity = it.user ("dropshadowopacity", project.properties, 1.0f),
	    .dropShadowOffset = it.user ("dropshadowoffset", project.properties, glm::vec2 (4.0f)),
	    .dropShadowColor = it.color ("dropshadowcolor", project.properties, Builders::ColorBuilder::Black),
	    .pointSize = it.user ("pointsize", project.properties, 32.0f),
	    .size = it.optional ("size", glm::vec2 (0.0f)),
	    .scale = it.user ("scale", project.properties, glm::vec3 (1.0f)),
	    .color = it.color ("color", project.properties, Builders::ColorBuilder::White),
	    .alpha = it.user ("alpha", project.properties, 1.0f),
	    .parallaxDepth = it.user ("parallaxDepth", project.properties, glm::vec2 (0.0f)),
	    .visible = it.user ("visible", project.properties, true),
	    .perspective = it.user ("perspective", project.properties, false),
	    .depthTest = std::move (depthTest),
	    .alignment = it.optional ("horizontalalign", it.optional ("alignment", std::string ("center"))),
	    .verticalalign = it.optional ("verticalalign", std::string ("center")),
	    .padding = it.user ("padding", project.properties, glm::vec2 (32.0f)),
	    .opaqueBackground = it.user ("opaquebackground", project.properties, false),
	    .backgroundColor = it.color ("backgroundcolor", project.properties, Builders::ColorBuilder::Black),
	    .backgroundBrightness = it.user ("backgroundbrightness", project.properties, 1.0f),
	    .spacing = it.user ("spacing", project.properties, glm::vec2 (0.0f)),
	    .brightness = it.user ("brightness", project.properties, 1.0f),
	    .effects = effects.has_value () ? parseEffects (*effects, project) : std::vector<ImageEffectUniquePtr> {},
	    .colorBlendMode = it.user ("colorBlendMode", project.properties, 0),
	    .ledSource = it.user ("ledsource", project.properties, false),
	    .limitWidth = it.user ("limitwidth", project.properties, false),
	    .maxWidth = it.user ("maxwidth", project.properties, 500.0f),
	    .limitRows = it.user ("limitrows", project.properties, false),
	    .maxRows = it.user ("maxrows", project.properties, 1),
	    .limitUseEllipsis = it.user ("limituseellipsis", project.properties, false),
	}
    );
}

struct ObjectParser::ModelCacheScope::Cache {
    struct Model {
	MdlMesh mesh;
	MdlAnimationData animationData;
    };
    std::unordered_map<std::string, Model> models;
};

namespace {
thread_local ObjectParser::ModelCacheScope::Cache* tModelCache = nullptr;

ObjectParser::ModelCacheScope::Cache::Model parseModelFile (const Project& project, const std::string& model) {
    // Geometry and animation live in the same MDL. Read it once in bulk;
    // the two load() helpers each reopened it and copied it byte by byte.
    const auto contents = project.assetLocator->readString (model);
    const std::vector<char> data (contents.begin (), contents.end ());
    ObjectParser::ModelCacheScope::Cache::Model result { .mesh = MdlParser::parse (data, model) };
    try {
	result.animationData = MdlAnimationParser::parse (data, model);
    } catch (const std::exception& e) {
	sLog.error ("Cannot load model animation data ", model, ": ", e.what ());
    }
    return result;
}

/** Scenes place the same MDL many times (238 objects over 44 files in one); parse each once. */
ObjectParser::ModelCacheScope::Cache::Model loadModel (const Project& project, const std::string& model) {
    if (tModelCache == nullptr) {
	return parseModelFile (project, model);
    }
    const auto key = project.assetLocator->identity () + '\0' + model;
    auto it = tModelCache->models.find (key);
    if (it == tModelCache->models.end ()) {
	it = tModelCache->models.emplace (key, parseModelFile (project, model)).first;
    }
    return it->second;
}
} // namespace

ObjectParser::ModelCacheScope::ModelCacheScope () : m_cache (std::make_unique<Cache> ()), m_previous (tModelCache) {
    tModelCache = m_cache.get ();
}

ObjectParser::ModelCacheScope::~ModelCacheScope () { tModelCache = m_previous; }

ObjectUniquePtr
ObjectParser::parseModel3D (const JSON& it, const Project& project, ObjectData base, const std::string& model) {
    try {
	auto [mesh, animationData] = loadModel (project, model);
	std::vector<MaterialUniquePtr> materials;

	for (const auto& submesh : mesh.submeshes) {
	    materials.push_back (MaterialParser::load (project, submesh.materialPath, true));
	}
	const auto& animationLayersIt = it.optional ("animationlayers");
	auto animationLayers = animationLayersIt.has_value () ? parseAnimationLayers (*animationLayersIt, project)
							      : std::vector<ImageAnimationLayerUniquePtr> {};

	return std::make_unique<Model3D> (
	    std::move (base),
	    Model3DData {
		.filename = model,
		.mesh = std::move (mesh),
		.animationData = std::move (animationData),
		.animationLayers = std::move (animationLayers),
		.materials = std::move (materials),
		.castShadow = it.user ("castshadow", project.properties, true),
		.perspective = it.user ("perspective", project.properties, false),
	    }
	);
    } catch (const std::exception& e) {
	sLog.error ("Cannot load model object ", base.id, " (", model, "): ", e.what ());
	return std::make_unique<Object> (std::move (base));
    }
}

LightUniquePtr
ObjectParser::parseLight (const JSON& it, const Project& project, ObjectData base, const std::string& light) {
    const auto& properties = project.properties;
    LightData::Type type = LightData::Type_Point;

    if (light == "lspot") {
	type = LightData::Type_Spot;
    } else if (light == "ltube") {
	type = LightData::Type_Tube;
    } else if (light == "ldirectional") {
	type = LightData::Type_Directional;
    } else if (light != "lpoint") {
	sLog.error ("Unsupported light type '", light, "' on object ", base.id, ", treating as point light");
    }

    return std::make_unique<Light> (
	std::move (base),
	LightData {
	    .type = type,
	    .color = it.user ("color", properties, glm::vec3 (1.0f)),
	    .intensity = it.user ("intensity", properties, 1.0f),
	    .radius = it.user ("radius", properties, 100.0f),
	    .sourceSize = it.user ("lightsourcesize", properties, 0.0f),
	    .exponent = it.user ("exponent", properties, 2.0f),
	    .innerCone = it.user ("innercone", properties, 20.0f),
	    .outerCone = it.user ("outercone", properties, 30.0f),
	    .useCookie = it.optional ("usecookie", false),
	    .cookie = it.optional ("cookie", std::string {}),
	    .controlPoint = it.user ("controlpoint", properties, glm::vec3 (2.0f, 0.0f, 0.0f)),
	    .cascadeDistances = glm::vec3 (
		it.optional ("cascadedistance0", 3.0f), it.optional ("cascadedistance1", 10.0f),
		it.optional ("cascadedistance2", 100.0f)
	    ),
	    .castShadow = it.optional ("castshadow", false),
	    .castVolumetrics = it.user ("castvolumetrics", properties, false),
	    .density = it.user ("density", properties, 2.0f),
	    .volumetricsExponent = it.user ("volumetricsexponent", properties, 1.0f),
	}
    );
}

ImageUniquePtr
ObjectParser::parseImage (const JSON& it, const Project& project, ObjectData base, const std::string& image) {
    const auto& properties = project.properties;
    const auto& effects = it.optional ("effects");
    const auto& animationLayers = it.optional ("animationlayers");

    auto result = std::make_unique<Image> (
	std::move (base),
	ImageData {
	    .scale = it.user ("scale", properties, glm::vec3 (1.0f)),
	    .angles = it.user ("angles", properties, glm::vec3 (0.0f)),
	    .visible = it.user ("visible", properties, true),
	    .ledSource = it.user ("ledsource", properties, false),
	    .alpha = it.user ("alpha", properties, 1.0f),
	    .color = it.color ("color", properties, Builders::ColorBuilder::White),
	    .alignment = it.optional ("horizontalalign", it.optional ("alignment", std::string ("center"))),
	    .size = it.user ("size", properties, glm::vec2 (0.0f))->value->getVec2 (),
	    .sizeSpecified = it.optional ("size").has_value (),
	    .parallaxDepth = it.user ("parallaxDepth", properties, glm::vec2 (1.0f)),
	    .colorBlendMode = it.user ("colorBlendMode", properties, 0),
	    .brightness = it.user ("brightness", properties, 1.0f),
	    .copyBackground = it.optional ("copybackground", true),
	    .noInterpolation = it.user ("nointerpolation", properties, false),
	    .perspective = it.user ("perspective", properties, false),
	    .model = ModelParser::load (project, image),
	    .effects = effects.has_value () ? parseEffects (*effects, project) : std::vector<ImageEffectUniquePtr> {},
	    .animationLayers = animationLayers.has_value () ? parseAnimationLayers (*animationLayers, project)
							    : std::vector<ImageAnimationLayerUniquePtr> {},
	}
    );

    const auto instance = it.optional ("instance");

    if (result->model->instanced && !result->model->material->passes.empty ()) {
	auto& firstPass = **result->model->material->passes.begin ();
	firstPass.userTextureReference = result->sizeSpecified ? result->size
	    : glm::vec2 (result->model->width.value_or (0), result->model->height.value_or (0));
    }

    if (result->model->instanced && instance.has_value () && instance->is_object ()
	&& !result->model->material->passes.empty ()) {
	auto& firstPass = **result->model->material->passes.begin ();
	const auto instanceTextures = instance->optional ("textures");

	if (instanceTextures.has_value ()) {
	    const auto parsed = TextureParser::parseTextureMap (*instanceTextures);
	    // An instance replaces populated slots too: the seaweed source in Deep Sea Dive
	    // overrides a solid layer's util/white texture with its authored foliage atlas.
	    for (const auto& [slot, texture] : parsed) {
		firstPass.textures.insert_or_assign (slot, texture);
	    }
	}

	const auto instanceUserTextures = instance->optional ("usertextures");

	if (instanceUserTextures.has_value ()) {
	    const auto parsed = TextureParser::parseTextureMap (*instanceUserTextures);
	    for (const auto& [slot, property] : parsed) {
		firstPass.usertextures.insert_or_assign (slot, property);
	    }
	}
    }

    return result;
}

std::vector<ImageEffectUniquePtr> ObjectParser::parseEffects (const JSON& it, const Project& project) {
    if (!it.is_array ()) {
	return {};
    }

    std::vector<ImageEffectUniquePtr> result = {};

    for (const auto& cur : it) {
	if (!cur.is_object ()) {
	    continue;
	}

	// Wallpaper Engine treats an effect as an optional part of the layer. Its
	// loader logs "Failed loading effect" and continues iterating the authored
	// effect list without appending the failed entry. Keep valid effects on
	// either side of a missing external workshop dependency.
	const auto filename = cur.optional<std::string> ("file");
	if (!filename.has_value ()) {
	    continue;
	}

	try {
	    result.push_back (parseEffect (cur, project));
	} catch (const std::exception& e) {
	    sLog.error ("Failed loading effect: ", *filename, " - ", e.what ());
	}
    }

    return result;
}

ImageEffectUniquePtr ObjectParser::parseEffect (const JSON& it, const Project& project) {
    const auto& passsOverrides = it.optional ("passes");
    return std::make_unique<ImageEffect> (ImageEffect {
	.id = it.optional<int> ("id", -1),
	.name = it.optional<std::string> ("name", "Effect without name"),
	.visible = it.user ("visible", project.properties, true),
	.passOverrides = passsOverrides.has_value () ? parseEffectPassOverrides (passsOverrides.value (), project)
						     : std::vector<ImageEffectPassOverrideUniquePtr> {},
	.effect = EffectParser::load (project, it.require ("file", "Image effect must have an effect"),
	    it.optional ("combos").value_or (JSON::object ())) });
}

std::vector<ImageEffectPassOverrideUniquePtr>
ObjectParser::parseEffectPassOverrides (const JSON& it, const Project& project) {
    if (!it.is_array ()) {
	return {};
    }

    std::vector<ImageEffectPassOverrideUniquePtr> result = {};

    for (const auto& cur : it) {
	result.push_back (parseEffectPass (cur, project));
    }

    return result;
}

ImageEffectPassOverrideUniquePtr ObjectParser::parseEffectPass (const JSON& it, const Project& project) {
    const auto& combos = it.optional ("combos");
    const auto& textures = it.optional ("textures");
    const auto& constants = it.optional ("constantshadervalues");
    const auto& usertextures = it.optional ("usertextures");
    const auto reference = it.optional ("usertexturereference");

    // TODO: PARSE CONSTANT SHADER VALUES AND FIND REFS?
    return std::make_unique<ImageEffectPassOverride> (ImageEffectPassOverride {
	.id = it.optional<int> ("id", -1),
	.combos = combos.has_value () ? parseComboMap (combos.value ()) : ComboMap {},
	.constants
	= constants.has_value () ? ShaderConstantParser::parse (constants.value (), project) : ShaderConstantMap {},
	.textures = textures.has_value () ? TextureParser::parseTextureMap (textures.value ()) : TextureMap {},
	.usertextures
	= usertextures.has_value () ? TextureParser::parseTextureMap (usertextures.value ()) : TextureMap {},
	.userTextureReferenceWidth = reference.has_value () ? reference->optional<float> ("width") : std::nullopt,
	.userTextureReferenceHeight = reference.has_value () ? reference->optional<float> ("height") : std::nullopt,
    });
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

std::vector<ImageAnimationLayerUniquePtr> ObjectParser::parseAnimationLayers (const JSON& it, const Project& project) {
    if (!it.is_array ()) {
	return {};
    }

    std::vector<ImageAnimationLayerUniquePtr> result = {};

    for (const auto& cur : it.items ()) {
	result.push_back (parseAnimationLayer (cur.value (), project));
    }

    return result;
}

ImageAnimationLayerUniquePtr ObjectParser::parseAnimationLayer (const JSON& it, const Project& project) {
    const auto& properties = project.properties;

    return std::make_unique<ImageAnimationLayer> (ImageAnimationLayer {
	.id = it.require<int> ("id", "Animation layer must have an id"),
	.name = it.optional<std::string> ("name").value_or (""),
	.rate = it.user ("rate", properties, 1.0f),
	.visible = it.user ("visible", properties, true),
	.blend = it.user ("blend", properties, 1.0f),
	.animation = it.user ("animation", properties, 0),
	.additive = it.user ("additive", properties, false),
	.blendIn = it.user ("blendin", properties, false),
	.blendOut = it.user ("blendout", properties, false),
	.blendTime = it.user ("blendtime", properties, 0.5f),
    });
}

namespace {
glm::vec3 particleColorFromString (const std::string& text) {
    glm::vec3 color (0.0f);
    const char* component = text.c_str ();
    for (int i = 0; i < 3; ++i) {
	color[i] = static_cast<float> (std::atof (component));
	component = std::strchr (component, ' ');
	if (component == nullptr) break;
	while (*component == ' ') ++component;
    }
    return color;
}

glm::vec3 roundParticleColor (const glm::vec3& color) {
    // Native serializes both generated references and migrated legacy overrides.
    return particleColorFromString (std::format ("{:.5f} {:.5f} {:.5f}", color.x, color.y, color.z));
}

std::pair<glm::vec3, bool> particleColorReference (const JSON& definition) {
    glm::vec3 reference (1.0f);
    bool hasColor = false;
    auto colorField = [] (const JSON& data, const char* key, const glm::vec3& defaultValue) {
	const auto value = data.optional (key);
	return value && value->is_string () ? particleColorFromString (value->get<std::string> ()) : defaultValue;
    };
    if (const auto initializers = definition.optional ("initializer"); initializers && initializers->is_array ()) {
	for (const auto& initializer : *initializers) {
	    const auto name = initializer.optional<std::string> ("name", "");
	    if (name == "colorrandom") {
		const auto min = colorField (initializer, "min", glm::vec3 (0.0f));
		const auto max = colorField (initializer, "max", glm::vec3 (255.0f));
		reference = (min + (max - min) * 0.5f) * (1.0f / 255.0f);
		hasColor = true;
	    } else if (name == "hsvcolorrandom") {
		auto scalar = [&] (const char* key, float defaultValue) {
		    const auto value = initializer.find (key);
		    if (value == initializer.end ()) return defaultValue;
		    if (value->is_boolean ()) return value->get<bool> () ? 1.0f : 0.0f;
		    return initializer.optional (key, 0.0f);
		};
		const glm::vec3 min (scalar ("huemin", 0.0f), scalar ("saturationmin", 0.5f), scalar ("valuemin", 0.5f));
		const glm::vec3 max (scalar ("huemax", 1.0f), scalar ("saturationmax", 1.0f), scalar ("valuemax", 1.0f));
		reference = WallpaperEngine::Maths::particleHsvToRgb (glm::clamp ((min + max) * 0.5f, 0.0f, 1.0f));
		hasColor = true;
	    } else if (name == "colorlist") {
		const auto colors = initializer.optional ("colors");
		if (!initializer.contains ("colors")) {
		    reference = glm::vec3 (1.0f);
		    hasColor = true;
		} else if (colors && colors->is_array ()) {
		    for (const auto& color : *colors) {
			if (!color.is_string ()) continue;
			const auto text = color.get<std::string> ();
			// Empty strings yield black; other entries need three space-delimited fields.
			const auto second = text.find_first_not_of (' ', text.find (' '));
			if (!text.empty () && (second == std::string::npos || text.find (' ', second) == std::string::npos)) {
			    continue;
			}
			reference = particleColorFromString (text);
			hasColor = true;
			break;
		    }
		}
	    }
	}
    }
    if (!hasColor) {
	if (const auto operators = definition.optional ("operator"); operators && operators->is_array ()) {
	    for (const auto& op : *operators) {
		if (op.optional<std::string> ("name", "") != "colorchange") continue;
		reference = colorField (op, "startvalue", glm::vec3 (1.0f));
		hasColor = true;
		break;
	    }
	}
    }
    return {roundParticleColor (reference), hasColor};
}
}

ParticleUniquePtr ObjectParser::parseParticle (const JSON& it, const Project& project, ObjectData base) {
    try {
	const auto& properties = project.properties;
	const auto particleIt = it.find ("particle");

	if (particleIt == it.end ()) {
	    sLog.error ("Particle object must have a particle definition");
	    return std::make_unique<Particle> (
		std::move (base),
		ParticleData {
		    .scale = it.user ("scale", properties, glm::vec3 (1.0f)),
		    .angles = it.user ("angles", properties, glm::vec3 (0.0f)),
		    .visible = it.user ("visible", properties, true),
		    .parallaxDepth = it.user ("parallaxDepth", properties, glm::vec2 (0.0f)),
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
		        .enabled = Builders::UserSettingBuilder::fromValue(false),
			.alpha = Builders::UserSettingBuilder::fromValue(1.0f),
			.brightness = Builders::UserSettingBuilder::fromValue(1.0f),
			.size = Builders::UserSettingBuilder::fromValue(1.0f),
			.lifetime = Builders::UserSettingBuilder::fromValue(1.0f),
			.rate = Builders::UserSettingBuilder::fromValue(1.0f),
			.speed = Builders::UserSettingBuilder::fromValue(1.0f),
			.count = Builders::UserSettingBuilder::fromValue(1.0f),
			.color = Builders::UserSettingBuilder::fromValue(glm::vec3 (1.0f)),
			.colorn = Builders::UserSettingBuilder::fromValue(glm::vec3 (-1.0f)),
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
		particleJson
		    = WallpaperEngine::Data::JSON::parseCompatible (
			project.assetLocator->readString (particleFile), particleFile
		    );
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
	    renderers.push_back (
		ParticleRenderer {
		    .name = "sprite",
		    .length = 0.05f,
		    .maxLength = 10.0f,
		    .minLength = 0.0f,
		    .subdivision = 1.0f,
		    .segments = 4.0f,
		    .uvScale = 1.0f,
		    .uvScrolling = false,
		    .uvSmoothing = true,
		    .fadeAlpha = false,
		    .fadeSize = false,
		}
	    );
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
	    .enabled = Builders::UserSettingBuilder::fromValue (false),
	    .alpha = Builders::UserSettingBuilder::fromValue (1.0f),
	    .brightness = Builders::UserSettingBuilder::fromValue (1.0f),
	    .size = Builders::UserSettingBuilder::fromValue (1.0f),
	    .lifetime = Builders::UserSettingBuilder::fromValue (1.0f),
	    .rate = Builders::UserSettingBuilder::fromValue (1.0f),
	    .speed = Builders::UserSettingBuilder::fromValue (1.0f),
	    .count = Builders::UserSettingBuilder::fromValue (1.0f),
	    .color = Builders::UserSettingBuilder::fromValue (glm::vec3 (1.0f)),
	    .colorn = Builders::UserSettingBuilder::fromValue (glm::vec3 (-1.0f)),
	};
	const auto instanceOverrideIt = it.optional ("instanceoverride");
	if (instanceOverrideIt.has_value ()) {
	    instanceOverride = parseParticleInstanceOverride (*instanceOverrideIt, project.properties);
	}
	for (int i = 0; i < PARTICLE_CONTROL_POINT_COUNT; ++i) {
	    if (!instanceOverride.controlPointOffsets.contains (i)) {
		instanceOverride.controlPointOffsets.emplace (i, Builders::UserSettingBuilder::fromValue (
		    glm::vec3 (std::numeric_limits<float>::max (), 0.0f, 0.0f)));
	    }
	    if (!instanceOverride.controlPointAngles.contains (i)) {
		instanceOverride.controlPointAngles.emplace (i, Builders::UserSettingBuilder::fromValue (
		    glm::vec3 (std::numeric_limits<float>::max (), 0.0f, 0.0f)));
	    }
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

		material = std::make_unique<ModelStruct> (ModelStruct {
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
		    .puppetMesh = std::nullopt,
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
	float startTime = 0.0f;
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
	    startTime = startTimeIt->get<float> ();
	}

	const auto flagsIt = particleJson.find ("flags");
	if (flagsIt != particleJson.end () && flagsIt->is_number ()) {
	    flags = flagsIt->get<uint32_t> ();
	}
	const auto [colorReference, hasColor] = particleColorReference (particleJson);

	// only particles loaded from their own file are a full document worth scanning;
	// inline definitions are part of the scene document and covered by its scan
	if (!particleFile.empty ()) {
	    WallpaperEngine::Data::Utils::JsonTelemetry::scan (particleJson, particleFile);
	}

	return std::make_unique<Particle> (
	    std::move (base),
	    ParticleData {
		.scale = it.user ("scale", properties, glm::vec3 (1.0f)),
		.angles = it.user ("angles", properties, glm::vec3 (0.0f)),
		.visible = it.user ("visible", properties, true),
		.parallaxDepth = it.user ("parallaxDepth", properties, glm::vec2 (0.0f)),
		.particleFile = particleFile,
		.animationMode = animationMode,
		.sequenceMultiplier = sequenceMultiplier,
		.maxCount = maxCount,
		.startTime = startTime,
		.flags = flags,
		.colorReference = colorReference,
		.hasColor = hasColor,
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
    std::string name;
    const auto nameIt = it.find ("name");
    if (nameIt != it.end () && nameIt->is_string ()) {
	name = nameIt->get<std::string> ();
    }

    // Helper lambda to parse vec3 fields that might be strings, arrays, single numbers, or missing
    auto parseVec3 = [&] (const char* fieldName, const glm::vec3& defaultValue) -> glm::vec3 {
	WallpaperEngine::Data::Utils::JsonTelemetry::recordAccess (fieldName);
	const auto fieldIt = it.find (fieldName);
	if (fieldIt == it.end ()) {
	    return defaultValue;
	}
	if (name == "boxrandom" && fieldIt->is_null ()) {
	    return glm::vec3 (0.0f);
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
	    return glm::vec3 ((*fieldIt)[0].get<float> (), (*fieldIt)[1].get<float> (), (*fieldIt)[2].get<float> ());
	}
	return defaultValue;
    };

    auto parseOffset = [&] (const char* fieldName) -> std::optional<glm::vec3> {
	WallpaperEngine::Data::Utils::JsonTelemetry::recordAccess (fieldName);
	const auto fieldIt = it.find (fieldName);
	if (name != "layerimage" || fieldIt == it.end ()) return std::nullopt;
	if (fieldIt->is_string () && fieldIt->get<std::string> ().empty ()) return glm::vec3 (0.0f);
	return parseVec3 (fieldName, glm::vec3 (0.0f));
    };

    auto parseIVec3 = [&] (const char* fieldName, const glm::ivec3& defaultValue) -> glm::ivec3 {
	WallpaperEngine::Data::Utils::JsonTelemetry::recordAccess (fieldName);
	const auto fieldIt = it.find (fieldName);
	if (fieldIt == it.end ()) {
	    return defaultValue;
	}
	if (fieldIt->is_array () && fieldIt->size () >= 3) {
	    return glm::ivec3 ((*fieldIt)[0].get<int> (), (*fieldIt)[1].get<int> (), (*fieldIt)[2].get<int> ());
	}
	return defaultValue;
    };

    auto parseVec2 = [&] (const char* fieldName, const glm::vec2& defaultValue) -> glm::vec2 {
	WallpaperEngine::Data::Utils::JsonTelemetry::recordAccess (fieldName);
	const auto fieldIt = it.find (fieldName);
	if (fieldIt == it.end ()) {
	    return defaultValue;
	}
	if (fieldIt->is_string ()) {
	    return it.optional (fieldName, defaultValue);
	}
	if (fieldIt->is_array () && fieldIt->size () >= 2) {
	    return glm::vec2 ((*fieldIt)[0].get<float> (), (*fieldIt)[1].get<float> ());
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
	    .controlPoint = static_cast<int> (std::min (it.optional ("controlpoint", 0u), 7u)),
	    .flags = it.contains ("flags") ? it.optional ("flags", 0u) : name == "layerimage" ? 65536u : 0u,
	    .cone = it.optional ("cone", 0.0f),
	    .delay = it.optional ("delay", 0.0f),
	    .duration = it.optional ("duration", 0.0f),
	    .audioProcessingBounds = parseVec2 ("audioprocessingbounds", glm::vec2 (0.8f, 1.0f)),
	    .audioProcessingExponent = it.optional ("audioprocessingexponent", 2.0f),
	    .audioProcessingFrequencyStart = it.optional ("audioprocessingfrequencystart", 0),
	    .audioProcessingFrequencyEnd = it.optional ("audioprocessingfrequencyend", 1),
	    .audioProcessingMode = it.optional ("audioprocessingmode", 0),
	    .minPeriodicDelay = it.optional ("minperiodicdelay", 1.0f),
	    .maxPeriodicDelay = it.optional ("maxperiodicdelay", 2.0f),
	    .minPeriodicDuration = it.optional ("minperiodicduration", 2.0f),
	    .maxPeriodicDuration = it.optional ("maxperiodicduration", 3.0f),
	    .maxToEmitPerPeriod = it.optional ("maxtoemitperperiod", 0u),
	    .defaultDirections = name == "boxrandom" && !it.contains ("directions"),
	    .defaultDistanceMax = name == "boxrandom" && !it.contains ("distancemax"),
	    .offsetMin = parseOffset ("offsetmin"),
	    .offsetMax = parseOffset ("offsetmax"),
	};
    } catch (nlohmann::json::exception& e) {
	sLog.error ("Error parsing emitter: ", e.what ());
	sLog.error ("Emitter JSON: ", it.dump ());
	throw;
    }
}

static std::optional<ControlPointRemap> parseControlPointRemap (
    const JSON& it, const Properties& properties, const bool initializer
) {
    const auto input = it.optional<std::string> ("input", initializer ? "maxlifetime" : "lifetimefraction");
    const auto output = it.optional<std::string> ("output", "size");
    const auto operation = it.optional<std::string> ("operation", "multiply");
    const auto transform = it.optional<std::string> ("transformfunction", "none");
    using Input = ControlPointRemap::Input;
    using Output = ControlPointRemap::Output;
    using Transform = ControlPointRemap::Transform;
    using Component = ControlPointRemap::Component;
    const std::optional<Input> parsedInput = input == "positionbetweentwocontrolpoints" ? Input::BetweenPoints
        : input == "distancetocontrolpoint" ? Input::DistanceToPoint
        : input == "maxlifetime" ? Input::MaxLifetime
        : input == "lifetimefraction" ? Input::LifetimeFraction
        : input == "size" ? Input::Size : input == "opacity" ? Input::Opacity
        : input == "speed" ? Input::Speed : input == "rotation" ? Input::Rotation
        : input == "angularspeed" ? Input::AngularSpeed : input == "runtime" ? Input::Runtime
        : input == "timeofday" ? Input::TimeOfDay
        : input == "particlesystemtime" ? Input::ParticleSystemTime : input == "layertime" ? Input::LayerTime
        : input == "controlpoint" ? Input::ControlPoint : input == "deltatocontrolpoint" ? Input::DeltaToPoint
        : input == "directiontocontrolpoint" ? Input::DirectionToPoint : input == "layerorigin" ? Input::LayerOrigin
        : input == "color" ? Input::Color : input == "position" ? Input::Position : input == "velocity" ? std::optional (Input::Velocity) : std::nullopt;
    const std::optional<Output> parsedOutput = output == "positionbetweentwocontrolpoints" ? Output::BetweenPoints
        : output == "controlpoint" ? Output::ControlPoint
        : output == "distancetocontrolpoint" ? Output::DistanceToPoint
        : output == "deltatocontrolpoint" ? Output::DeltaToPoint : output == "directiontocontrolpoint" ? Output::DirectionToPoint
        : output == "maxlifetime" ? Output::MaxLifetime : output == "size" ? Output::Size
        : output == "opacity" ? Output::Opacity : output == "speed" ? Output::Speed
        : output == "velocity" ? Output::Velocity : output == "rotation" ? Output::Rotation
        : output == "color" ? Output::Color : output == "angularspeed" ? Output::AngularSpeed : output == "position" ? std::optional (Output::Position) : std::nullopt;
    const std::optional<Transform> parsedTransform = transform == "none" ? Transform::Identity
        : transform == "sine" ? Transform::Sine : transform == "square" ? Transform::Square
        : transform == "saw" ? Transform::Saw : transform == "triangle" ? Transform::Triangle
        : transform == "simplexnoise" ? Transform::SimplexNoise
        : transform == "fbmnoise" ? std::optional (Transform::FbmNoise) : std::nullopt;
    // Initializer point inputs use the live mutation/birth owner in CParticle.
    // They are admitted only with the measured two-point position destination.
    const bool linearPair = (parsedInput == Input::BetweenPoints && parsedOutput.has_value ())
        || (parsedOutput == Output::BetweenPoints && parsedInput.has_value ());
    const bool periodic = parsedTransform == Transform::Sine || parsedTransform == Transform::Square
        || parsedTransform == Transform::Saw || parsedTransform == Transform::Triangle;
    const auto octaveValue = it.find ("transformoctaves");
    const int transformOctaves = parsedTransform != Transform::FbmNoise || octaveValue == it.end () ? 3
        : octaveValue->is_null () ? 0 : octaveValue->is_boolean () ? static_cast<int> (octaveValue->get<bool> ())
        : it.optional ("transformoctaves", -1);
    const bool linearTransform = parsedTransform == Transform::Identity || periodic
        || parsedTransform == Transform::SimplexNoise
        || (parsedTransform == Transform::FbmNoise && (initializer ? transformOctaves > 0 : transformOctaves >= 0));
    const bool speedOutput = parsedOutput == Output::Speed;
    const bool arithmetic = linearPair
        && (operation == "multiply" || operation == "add" || operation == "subtract");
    const bool noiseVelocity = !initializer && parsedInput == Input::LifetimeFraction
        && parsedOutput == Output::Velocity && parsedTransform == Transform::SimplexNoise;
    const bool noiseSpeed = !initializer && parsedInput == Input::LifetimeFraction
        && speedOutput && parsedTransform == Transform::FbmNoise && transformOctaves >= 0;
    const auto parseComponent = [] (const std::string& component) -> std::optional<Component> {
        if (component == "all") return Component::All;
        if (component == "x") return Component::X;
        if (component == "y") return Component::Y;
        if (component == "z") return Component::Z;
        if (component == "sum") return Component::Sum;
        if (component == "average") return Component::Average;
        if (component == "max") return Component::Max;
        if (component == "min") return Component::Min;
        return std::nullopt;
    };
    const auto inputComponent = parseComponent (it.optional<std::string> ("inputcomponent", "all"));
    const auto outputComponent = parseComponent (it.optional<std::string> ("outputcomponent", "all"));
    // Scalar selectors are ignored; vectors select/reduce input or select output.
    // The separate noise modes keep their existing all-component gate.
    const bool supportedInputComponent = inputComponent == Component::All || (linearPair && inputComponent.has_value ());
    const bool supportedOutputComponent = outputComponent == Component::All || (linearPair && outputComponent.has_value ());
    const auto flags = it.optional ("flags", 1u);
    const bool supportedFlags = noiseVelocity ? flags == 1u : noiseSpeed ? flags == 3u : flags <= 3u;
    const glm::vec4 blendTimes = initializer ? glm::vec4 (0.0f, 0.0f, 1.0f, 1.0f) : glm::vec4 (
        it.optional ("blendinstart", 0.0f), it.optional ("blendinend", 0.0f),
        it.optional ("blendoutstart", 1.0f), it.optional ("blendoutend", 1.0f));
    if ((!noiseVelocity && !noiseSpeed && (!linearPair || !linearTransform))
        || (operation != (noiseSpeed ? "multiply" : "remap") && !arithmetic)
        || !supportedInputComponent || !supportedOutputComponent
        || !supportedFlags) {
        // Keep unmeasured pairs, transforms, flags and octave counts explicit.
        sLog.error ("Unsupported particle ", initializer ? "remapinitialvalue" : "remapvalue",
                    " settings (input=", input, ", output=", output, ", operation=", operation,
                    ", transform=", transform, "); skipping");
        return std::nullopt;
    }
    const auto point = [&it] (const char* key, const int fallback) {
        return std::min (static_cast<uint32_t> (it.optional (key, fallback)), 7u);
    };
    return ControlPointRemap {
        .input = *parsedInput,
        .output = *parsedOutput,
        .transform = *parsedTransform,
        .inputRangeMin = it.user ("inputrangemin", properties, 0.0f),
        .inputRangeMax = it.user ("inputrangemax", properties, 1.0f),
        .outputRangeMin = it.user ("outputrangemin", properties, 0.0f),
        .outputRangeMax = it.user ("outputrangemax", properties, 1.0f),
        .inputControlPoint0 = point ("inputcontrolpoint0", 0),
        .inputControlPoint1 = point ("inputcontrolpoint1", 1),
        .outputControlPoint0 = point ("outputcontrolpoint0", 0),
        .outputControlPoint1 = point ("outputcontrolpoint1", 1),
        .transformInputScale = it.optional ("transforminputscale", 2.0f),
        .operation = operation == "multiply" ? ControlPointRemap::Operation::Multiply
                   : operation == "add" ? ControlPointRemap::Operation::Add
                   : operation == "subtract" ? ControlPointRemap::Operation::Subtract
                   : ControlPointRemap::Operation::Remap,
        .flags = flags,
        .blendTimes = blendTimes,
        .inputComponent = *inputComponent,
        .outputComponent = *outputComponent,
        .transformOctaves = static_cast<uint32_t> (transformOctaves),
    };
}

ParticleInitializerUniquePtr ObjectParser::parseParticleInitializer (const JSON& it, const Properties& properties) {
    std::string name = it.optional<std::string> ("name", "");

    if (name == "remapinitialvalue") {
        auto remap = parseControlPointRemap (it, properties, true);
        if (remap) return std::make_unique<ControlPointRemapInitializer> (std::move (*remap));
        return nullptr;
    } else if (name == "inheritinitialvaluefromevent") {
        const auto input = it.optional<std::string> ("input", "setcolor");
        if (input != "setcolor" && input != "setcoloropacity") return nullptr;
        return std::make_unique<InheritInitialEventColorInitializer> (input == "setcoloropacity");
    } else if (name == "colorrandom") {
	auto color = [&] (const char* key, const Color& defaultValue) {
	    const auto value = it.optional (key);
	    if (value && value->is_string ()) {
		return Builders::UserSettingBuilder::fromValue (
		    particleColorFromString (value->get<std::string> ()) * (1.0f / 255.0f));
	    }
	    return it.color (key, properties, defaultValue);
	};
	return std::make_unique<ColorRandomInitializer> (
	    color ("min", Builders::ColorBuilder::Black), color ("max", Builders::ColorBuilder::White),
	    it.contains ("exponent") ? it.optional ("exponent", 0.0f) : 1.0f
	);
    } else if (name == "sizerandom") {
	return std::make_unique<SizeRandomInitializer> (
	    it.user ("min", properties, 0.0f), it.user ("max", properties, 20.0f),
	    it.user ("exponent", properties, 1.0f)
	);
    } else if (name == "alpharandom") {
	return std::make_unique<AlphaRandomInitializer> (
	    it.user ("min", properties, 0.05f), it.user ("max", properties, 1.0f)
	);
    } else if (name == "lifetimerandom") {
	return std::make_unique<LifetimeRandomInitializer> (
	    it.user ("min", properties, 0.0f), it.user ("max", properties, 1.0f),
	    it.user ("exponent", properties, 1.0f)
	);
    } else if (name == "velocityrandom") {
	return std::make_unique<VelocityRandomInitializer> (
	    it.user ("min", properties, glm::vec3 (0.0f)), it.user ("max", properties, glm::vec3 (0.0f))
	);
    } else if (name == "inheritcontrolpointvelocity") {
	return std::make_unique<InheritControlPointVelocityInitializer> (
	    it.user ("min", properties, 0.1f), it.user ("max", properties, 0.2f), it.optional ("controlpoint", 0)
	);
    } else if (name == "rotationrandom") {
	return std::make_unique<RotationRandomInitializer> (
	    it.user ("min", properties, glm::vec3 (0.0f)),
	    it.user ("max", properties, glm::vec3 (0.0f, 0.0f, glm::two_pi<float> ()))
	);
    } else if (name == "angularvelocityrandom") {
	return std::make_unique<AngularVelocityRandomInitializer> (
	    it.user ("min", properties, glm::vec3 (0.0f, 0.0f, -5.0f)),
	    it.user ("max", properties, glm::vec3 (0.0f, 0.0f, 5.0f)), it.user ("exponent", properties, 1.0f)
	);
    } else if (name == "turbulentvelocityrandom") {
	return std::make_unique<TurbulentVelocityRandomInitializer> (
	    it.user ("speedmin", properties, 100.0f), it.user ("speedmax", properties, 250.0f),
	    it.user ("scale", properties, 1.0f), it.user ("offset", properties, 0.0f),
	    it.user ("forward", properties, glm::vec3 (0.0f, 1.0f, 0.0f)), it.user ("timescale", properties, 1.0f),
	    it.user ("phasemin", properties, 0.0f), it.user ("phasemax", properties, 0.1f),
	    it.user ("right", properties, glm::vec3 (0.0f, 0.0f, 1.0f))
	);
    } else if (name == "positionoffsetrandom") {
	return std::make_unique<PositionOffsetRandomInitializer> (
	    it.contains ("directions") && !it.at ("directions").is_null ()
		? it.user ("directions", properties, glm::vec3 (1.0f)) : nullptr,
	    it.user ("sign", properties, glm::vec3 (0.0f)),
	    it.contains ("scale") && !it.at ("scale").is_null () ? it.user ("scale", properties, 1.0f) : nullptr,
	    it.contains ("distance") && !it.at ("distance").is_null () ? it.user ("distance", properties, 0.1f) : nullptr,
	    it.user ("timescale", properties, 1.0f), static_cast<int> (std::clamp (it.optional ("octaves", 6u), 1u, 8u))
	);
    } else if (name == "mapsequencebetweencontrolpoints") {
	return std::make_unique<MapSequenceBetweenControlPointsInitializer> (
	    it.user ("count", properties, 32.0f), it.user ("bounds", properties, glm::vec2 (0.0f, 1.0f)),
	    it.optional<std::string> ("limitbehavior", "repeat"), it.optional ("controlpointstart", 0),
	    it.optional ("controlpointend", 1), it.optional ("flags", 0u), it.optional ("arcamount", 0.3f),
	    it.optional ("arcdirection", glm::vec3 (0.0f, 1.0f, 0.0f)), it.optional ("sizereductionamount", 0.9f)
	);
    } else if (name == "mapsequencearoundcontrolpoint") {
	return std::make_unique<MapSequenceAroundControlPointInitializer> (
	    it.user ("count", properties, 32.0f), it.user ("bounds", properties, glm::vec2 (0.0f, 1.0f)),
	    it.user ("speedmin", properties, glm::vec3 (0.0f)),
	    it.user ("speedmax", properties, glm::vec3 (0.0f)),
	    it.user ("axis", properties, glm::vec3 (0.0f, 0.0f, 1.0f)),
	    it.optional<std::string> ("limitbehavior", "repeat"), it.user ("controlpoint", properties, 0),
	    it.optional ("flags", 0u)
	);
    }

    return nullptr;
}

ParticleOperatorUniquePtr ObjectParser::parseParticleOperator (const JSON& it, const Properties& properties) {
    std::string name = it.optional<std::string> ("name", "");

    if (name == "remapvalue") {
        auto remap = parseControlPointRemap (it, properties, false);
        if (remap) return std::make_unique<ControlPointRemapOperator> (std::move (*remap));
        return nullptr;
    } else if (name == "inheritvaluefromevent") {
        if (it.optional<std::string> ("input", "setcoloropacity") != "setcoloropacity") return nullptr;
        return std::make_unique<InheritEventColorOperator> (
            glm::vec4 (it.optional ("blendinstart", 0.0f), it.optional ("blendinend", 0.0f),
                       it.optional ("blendoutstart", 1.0f), it.optional ("blendoutend", 1.0f))
        );
    } else if (name == "movement") {
	return std::make_unique<MovementOperator> (
	    it.user ("drag", properties, 0.0f), it.user ("gravity", properties, glm::vec3 (0.0f))
	);
    } else if (name == "collisionplane" || name == "collisionquad" || name == "collisionbounds") {
        const std::string behaviorName = it.optional<std::string> ("collisionbehavior", "bounce");
        const auto behavior = behaviorName == "slide" ? ParticleCollisionBehavior::Slide
            : behaviorName == "stop" ? ParticleCollisionBehavior::Stop
            : behaviorName == "delete" ? ParticleCollisionBehavior::Delete : ParticleCollisionBehavior::Bounce;
        if (name == "collisionbounds") return std::make_unique<CollisionBoundsOperator> (
            it.user ("bouncefactor", properties, it.contains ("bouncefactor") ? 0.0f : 0.5f), behavior
        );
        const glm::vec3 plane = it.user ("plane", properties, it.contains ("plane") ? glm::vec3 (0.0f) : glm::vec3 (0.0f, 1.0f, 0.0f))->value->getVec3 ();
        if (name == "collisionquad") {
            const glm::vec3 normal = glm::normalize (plane);
            const glm::vec3 forward = glm::normalize (it.user ("forward", properties,
                it.contains ("forward") ? glm::vec3 (0.0f) : glm::vec3 (0.0f, 0.0f, 1.0f))->value->getVec3 ());
            const glm::vec3 right = glm::normalize (glm::cross (normal, forward));
            return std::make_unique<CollisionQuadOperator> (
                normal, right, glm::normalize (glm::cross (right, normal)),
                it.contains ("origin") ? it.user ("origin", properties, glm::vec3 (0.0f)) : nullptr,
                it.contains ("size") ? it.user ("size", properties, glm::vec2 (0.0f)) : nullptr,
                it.user ("bouncefactor", properties, it.contains ("bouncefactor") ? 0.0f : 0.5f), behavior,
                it.optional ("flags", 0u), std::min (static_cast<uint32_t> (it.optional ("controlpoint", 0)), 7u)
            );
        }
        return std::make_unique<CollisionPlaneOperator> (
            glm::normalize (plane), it.contains ("distance") ? it.user ("distance", properties, 0.0f) : nullptr,
            it.user ("bouncefactor", properties, it.contains ("bouncefactor") ? 0.0f : 0.5f), behavior, it.optional ("flags", 0u),
            std::min (static_cast<uint32_t> (it.optional ("controlpoint", 0)), 7u)
        );
    } else if (name == "collisionsphere") {
        const std::string behaviorName = it.optional<std::string> ("collisionbehavior", "bounce");
        const auto behavior = behaviorName == "slide" ? ParticleCollisionBehavior::Slide
            : behaviorName == "stop" ? ParticleCollisionBehavior::Stop
            : behaviorName == "delete" ? ParticleCollisionBehavior::Delete : ParticleCollisionBehavior::Bounce;
        return std::make_unique<CollisionSphereOperator> (
            it.contains ("origin") ? it.user ("origin", properties, glm::vec3 (0.0f)) : nullptr,
            it.contains ("radius") ? it.user ("radius", properties, 0.0f) : nullptr,
            it.user ("bouncefactor", properties, it.contains ("bouncefactor") ? 0.0f : 0.5f), behavior,
            it.optional ("flags", 0u), std::min (static_cast<uint32_t> (it.optional ("controlpoint", 0)), 7u)
        );
    } else if (name == "collisionmodel") {
        const std::string behaviorName = it.optional<std::string> ("collisionbehavior", "bounce");
        const auto behavior = behaviorName == "slide" ? ParticleCollisionBehavior::Slide
            : behaviorName == "stop" ? ParticleCollisionBehavior::Stop
            : behaviorName == "delete" ? ParticleCollisionBehavior::Delete : ParticleCollisionBehavior::Bounce;
        return std::make_unique<CollisionModelOperator> (
            it.user ("bouncefactor", properties, it.contains ("bouncefactor") ? 0.0f : 0.5f)->value->getFloat (), behavior
        );
    } else if (name == "boids") {
        return std::make_unique<BoidsOperator> (
            it.contains ("separationthreshold") ? std::optional<float> (it.optional ("separationthreshold", 0.0f)) : std::nullopt,
            it.contains ("neighborthreshold") ? std::optional<float> (it.optional ("neighborthreshold", 0.0f)) : std::nullopt,
            it.contains ("maxspeed") ? std::optional<float> (it.optional ("maxspeed", 0.0f)) : std::nullopt,
            it.optional ("separationfactor", it.contains ("separationfactor") ? 0.0f : 15.0f),
            it.optional ("alignmentfactor", it.contains ("alignmentfactor") ? 0.0f : 1.0f),
            it.optional ("cohesionfactor", it.contains ("cohesionfactor") ? 0.0f : 2.0f),
            it.optional ("flags", it.contains ("flags") ? 0u : 1u)
        );
    } else if (name == "capvelocity") {
	return std::make_unique<CapVelocityOperator> (
	    it.optional ("maxspeed") ? it.user ("maxspeed", properties) : nullptr,
	    glm::vec4 (it.optional ("blendinstart", 0.0f), it.optional ("blendinend", 0.0f),
		       it.optional ("blendoutstart", 1.0f), it.optional ("blendoutend", 1.0f))
	);
    } else if (name == "reducemovementnearcontrolpoint") {
	return std::make_unique<ReduceMovementNearControlPointOperator> (
	    std::min (it.optional ("controlpoint", 0u), 7u),
	    it.optional ("distanceinner") ? it.user ("distanceinner", properties) : nullptr,
	    it.optional ("distanceouter") ? it.user ("distanceouter", properties) : nullptr,
	    it.user ("reductioninner", properties, 100.0f), it.user ("reductionouter", properties, 0.0f),
	    glm::vec4 (it.optional ("blendinstart", 0.0f), it.optional ("blendinend", 0.0f),
		       it.optional ("blendoutstart", 1.0f), it.optional ("blendoutend", 1.0f))
	);
    } else if (name == "maintaindistancetocontrolpoint") {
	return std::make_unique<MaintainDistanceToControlPointOperator> (
	    std::min (static_cast<uint32_t> (it.optional ("controlpoint", 0)), 7u),
	    it.contains ("distance") ? it.user ("distance", properties, 0.0f) : nullptr,
	    it.user ("variablestrength", properties, 0.0f),
	    glm::vec4 (it.optional ("blendinstart", 0.0f), it.optional ("blendinend", 0.0f),
		       it.optional ("blendoutstart", 1.0f), it.optional ("blendoutend", 1.0f))
	);
    } else if (name == "maintaindistancebetweencontrolpoints") {
	return std::make_unique<MaintainDistanceBetweenControlPointsOperator> (
	    std::min (it.optional ("controlpointstart", 0u), 7u),
	    std::min (it.optional ("controlpointend", it.contains ("controlpointend") ? 0u : 1u), 7u),
	    glm::vec4 (it.optional ("blendinstart", 0.0f), it.optional ("blendinend", 0.0f),
	               it.optional ("blendoutstart", 1.0f), it.optional ("blendoutend", 1.0f))
	);
    } else if (name == "angularmovement") {
	return std::make_unique<AngularMovementOperator> (
	    it.user ("drag", properties, 0.0f), it.user ("force", properties, glm::vec3 (0.0f)),
	    glm::vec4 (it.optional ("blendinstart", 0.0f), it.optional ("blendinend", 0.0f),
		       it.optional ("blendoutstart", 1.0f), it.optional ("blendoutend", 1.0f))
	);
    } else if (name == "alphafade") {
	return std::make_unique<AlphaFadeOperator> (
	    it.user ("fadeintime", properties, 0.5f), it.user ("fadeouttime", properties, 0.5f)
	);
    } else if (name == "sizechange") {
	return std::make_unique<SizeChangeOperator> (
	    it.user ("starttime", properties, 0.0f), it.user ("endtime", properties, 1.0f),
	    it.user ("startvalue", properties, 1.0f), it.user ("endvalue", properties, 0.0f)
	);
    } else if (name == "alphachange") {
	return std::make_unique<AlphaChangeOperator> (
	    it.user ("starttime", properties, 0.0f), it.user ("endtime", properties, 1.0f),
	    it.user ("startvalue", properties, 1.0f), it.user ("endvalue", properties, 0.0f)
	);
    } else if (name == "colorchange") {
	return std::make_unique<ColorChangeOperator> (
	    it.user ("starttime", properties, 0.0f), it.user ("endtime", properties, 1.0f),
	    it.user ("startvalue", properties, glm::vec3 (1.0f)), it.user ("endvalue", properties, glm::vec3 (1.0f))
	);
    } else if (name == "turbulence") {
	return std::make_unique<TurbulenceOperator> (
	    // Only absent keys use projection defaults; explicit null is native zero.
	    it.contains ("scale") ? it.user ("scale", properties, 0.0f) : nullptr,
	    it.contains ("speedmin") ? it.user ("speedmin", properties, 0.0f) : nullptr,
	    it.contains ("speedmax") ? it.user ("speedmax", properties, 0.0f) : nullptr,
	    it.contains ("timescale") ? it.user ("timescale", properties, 0.0f) : nullptr,
	    it.contains ("mask") ? it.user ("mask", properties, glm::vec3 (0.0f)) : nullptr,
	    it.user ("phasemin", properties, 0.0f),
	    it.user ("phasemax", properties, 0.0f), it.user ("audioprocessingmode", properties, 0),
	    it.user ("audioprocessingbounds", properties, it.contains ("audioprocessingbounds") ? glm::vec2 (0.0f) : glm::vec2 (.8f, 1.0f)),
	    it.user ("audioprocessingexponent", properties, it.contains ("audioprocessingexponent") ? 0.0f : 2.0f),
	    it.user ("audioprocessingfrequencystart", properties, 0),
	    it.user ("audioprocessingfrequencyend", properties, it.contains ("audioprocessingfrequencyend") ? 0 : 1),
	    glm::vec4 (it.optional ("blendinstart", 0.0f), it.optional ("blendinend", 0.0f),
		       it.optional ("blendoutstart", 1.0f), it.optional ("blendoutend", 1.0f))
	);
    } else if (name == "vortex" || name == "vortex_v2") {
	const bool v2 = name == "vortex_v2";
	return std::make_unique<VortexOperator> (
	    std::min (static_cast<uint32_t> (it.optional ("controlpoint", 0)), 7u),
	    it.optional ("flags", 0) & (v2 ? 255 : -1), // 1 = infinite axis, 2 = maintain distance, 4 = ring shape
	    it.user ("axis", properties, glm::vec3 (0.0f, 0.0f, 1.0f)),
	    it.user ("offset", properties, glm::vec3 (0.0f)), (v2 ? !it.optional ("distanceinner") : !it.contains ("distanceinner")) ? nullptr : it.user ("distanceinner", properties, v2 ? 500.0f : 0.0f),
	    (v2 ? !it.optional ("distanceouter") : !it.contains ("distanceouter")) ? nullptr : it.user ("distanceouter", properties, v2 ? 650.0f : 0.0f), (v2 ? !it.optional ("speedinner") : !it.contains ("speedinner")) ? nullptr : it.user ("speedinner", properties, v2 ? 2500.0f : 0.0f),
	    it.user ("speedouter", properties, 0.0f), it.user ("centerforce", properties, 1.0f),
	    v2 && !it.optional ("ringradius") ? nullptr : it.user ("ringradius", properties, 300.0f), v2 && !it.optional ("ringwidth") ? nullptr : it.user ("ringwidth", properties, 50.0f),
	    v2 && !it.optional ("ringpulldistance") ? nullptr : it.user ("ringpulldistance", properties, 50.0f), v2 && !it.optional ("ringpullforce") ? nullptr : it.user ("ringpullforce", properties, 10.0f),
	    it.user ("audioprocessingmode", properties, 0),
	    it.user ("audioprocessingbounds", properties, it.contains ("audioprocessingbounds") ? glm::vec2 (0.0f) : glm::vec2 (.8f, 1.0f)), v2,
	    glm::vec4 (it.optional ("blendinstart", 0.0f), it.optional ("blendinend", 0.0f),
		       it.optional ("blendoutstart", 1.0f), it.optional ("blendoutend", 1.0f)),
	    it.user ("audioprocessingexponent", properties, it.contains ("audioprocessingexponent") ? 0.0f : 2.0f),
	    it.user ("audioprocessingfrequencystart", properties, 0),
	    it.user ("audioprocessingfrequencyend", properties, it.contains ("audioprocessingfrequencyend") ? 0 : 1)
	);
    } else if (name == "controlpointattract") {
	return std::make_unique<ControlPointAttractOperator> (
	    std::min (static_cast<uint32_t> (it.optional ("controlpoint", 0)), 7u), it.optional ("flags", 2),
	    it.contains ("scale") ? it.user ("scale", properties, 0.0f) : nullptr,
	    it.contains ("threshold") ? it.user ("threshold", properties, 0.0f) : nullptr,
	    it.contains ("deletethreshold") ? it.user ("deletethreshold", properties, 0.0f) : nullptr,
	    glm::vec4 (it.optional ("blendinstart", 0.0f), it.optional ("blendinend", 0.0f),
		       it.optional ("blendoutstart", 1.0f), it.optional ("blendoutend", 1.0f))
	);
    } else if (name == "oscillatealpha") {
	return std::make_unique<OscillateAlphaOperator> (
	    it.user ("frequencymin", properties, 1.0f), it.user ("frequencymax", properties, 10.0f),
	    it.user ("scalemin", properties, 0.0f), it.user ("scalemax", properties, 1.0f),
	    it.user ("phasemin", properties, 0.0f), it.user ("phasemax", properties, glm::two_pi<float> ()),
	    glm::vec4 (it.optional ("blendinstart", 0.0f), it.optional ("blendinend", 0.0f),
		       it.optional ("blendoutstart", 1.0f), it.optional ("blendoutend", 1.0f))
	);
    } else if (name == "oscillatesize") {
	return std::make_unique<OscillateSizeOperator> (
	    it.user ("frequencymin", properties, 1.0f), it.user ("frequencymax", properties, 10.0f),
	    it.user ("scalemin", properties, 0.8f), it.user ("scalemax", properties, 1.2f),
	    it.user ("phasemin", properties, 0.0f), it.user ("phasemax", properties, glm::two_pi<float> ()),
	    glm::vec4 (it.optional ("blendinstart", 0.0f), it.optional ("blendinend", 0.0f),
		       it.optional ("blendoutstart", 1.0f), it.optional ("blendoutend", 1.0f))
	);
    } else if (name == "oscillateposition") {
	return std::make_unique<OscillatePositionOperator> (
	    it.user ("frequencymin", properties, 1.0f), it.user ("frequencymax", properties, 5.0f),
	    it.user ("scalemin", properties, 0.0f),
	    it.optional ("scalemax") ? it.user ("scalemax", properties) : nullptr,
	    it.user ("phasemin", properties, 0.0f), it.user ("phasemax", properties, glm::two_pi<float> ()),
	    it.user ("mask", properties, glm::vec3 (1.0f, 1.0f, 0.0f)),
	    glm::vec4 (it.optional ("blendinstart", 0.0f), it.optional ("blendinend", 0.0f),
		       it.optional ("blendoutstart", 1.0f), it.optional ("blendoutend", 1.0f))
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

    // Renderer-type-specific defaults
    float subdivisionDefault = (name == "rope") ? 4.0f : 1.0f;
    float lengthDefault = (name == "ropetrail") ? 1.0f : 0.05f;

    return ParticleRenderer {
	.name = name,
	.length = it.optional ("length", lengthDefault),
	.maxLength = it.optional ("maxlength", 10.0f),
	.minLength = it.optional ("minlength", 0.0f),
	.subdivision = it.optional ("subdivision", subdivisionDefault),
	.segments = it.optional ("segments", 4.0f),
	.segmentsExplicit = it.contains ("segments"),
	.uvScale = it.optional ("uvscale", 1.0f),
	.uvScrolling = it.optional ("uvscrolling", false),
	.uvSmoothing = it.optional ("uvsmoothing", true),
	.fadeAlpha = it.optional ("fadealpha", false),
	.fadeSize = it.optional ("fadesize", false),
	.orientation = it.optional ("orientation", std::string ("screen")),
	.axis = it.optional ("axis", glm::vec3 (0.0f, 1.0f, 0.0f)),
	.flags = it.optional ("flags", uint32_t (0)),
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
	.parentControlPoint = it.optional ("parentcontrolpoint", 0),
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
    // Child definitions use name for the asset path, unlike scene layers.
    if (particleFile.empty ()) {
	particleFile = name;
    }

    // Helper lambda to parse vec3 fields that might be strings, arrays, single numbers, or missing
    auto parseVec3 = [&] (const char* fieldName, const glm::vec3& defaultValue) -> glm::vec3 {
	WallpaperEngine::Data::Utils::JsonTelemetry::recordAccess (fieldName);
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
	    return glm::vec3 ((*fieldIt)[0].get<float> (), (*fieldIt)[1].get<float> (), (*fieldIt)[2].get<float> ());
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
    ParticleInstanceOverride override {
	.enabled = it.user ("enabled", properties, true),
	.alpha = it.user ("alpha", properties, 1.0f),
	.brightness = it.user ("brightness", properties, 1.0f),
	.size = it.user ("size", properties, 1.0f),
	.lifetime = it.user ("lifetime", properties, 1.0f),
	.rate = it.user ("rate", properties, 1.0f),
	.speed = it.user ("speed", properties, 1.0f),
	.count = it.user ("count", properties, 1.0f),
	.color = Builders::UserSettingBuilder::fromValue (glm::vec3 (1.0f)),
	.colorn = it.user ("colorn", properties, glm::vec3 (-1.0f)),
    };

    if (const auto legacyColor = it.optional ("color"); legacyColor && legacyColor->is_string ()) {
	// Migration takes precedence over colorn; color is not a second material tint.
	override.colorn = Builders::UserSettingBuilder::fromValue (roundParticleColor (
	    particleColorFromString (legacyColor->get<std::string> ()) * (1.0f / 255.0f)));
    }

    for (const auto& [prefix, values] : {
	    std::pair { "controlpoint", &override.controlPointOffsets },
	    std::pair { "controlpointangle", &override.controlPointAngles } }) {
	for (int i = 0; i < PARTICLE_CONTROL_POINT_COUNT; i++) {
	    // Native uses FLT_MAX in X to leave the control-point transform unchanged.
	    const std::string key = prefix + std::to_string (i);
	    auto value = it.user (key, properties,
		glm::vec3 (std::numeric_limits<float>::max (), 0.0f, 0.0f));
	    // Native control-point curves require all three vector channels.
	    if (value->animation != nullptr) {
		const auto& channels = value->animation->channels;
		if (!channels.contains (0) || !channels.contains (1) || !channels.contains (2)) {
		    value->animation.reset ();
		} else {
		    // For these curves native tests field presence, including explicit false.
		    value->animation->relative = it.at (key).at ("animation").contains ("relative");
		}
	    }
	    values->emplace (i, std::move (value));
	}
    }

    return override;
}
