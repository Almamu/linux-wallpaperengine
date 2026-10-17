#include "WallpaperEngine/Render/Objects/CCameraObject.h"
#include "WallpaperEngine/Render/Objects/CImage.h"
#include "WallpaperEngine/Render/Objects/CModel.h"
#include "WallpaperEngine/Render/Objects/CParticle.h"
#include "WallpaperEngine/Render/Objects/CSound.h"
#include "WallpaperEngine/Render/Objects/CText.h"

#include "WallpaperEngine/Render/WallpaperState.h"

#include "CScene.h"
#include "WallpaperEngine/Logging/Log.h"

#include "WallpaperEngine/Data/Model/Wallpaper.h"
#include "WallpaperEngine/Data/Parsers/ObjectParser.h"

#include <algorithm>
#include <cctype>
#include <glm/gtc/matrix_transform.hpp>
#include <map>
#include <optional>
#include <ranges>
#include <set>
#include <string>
#include <utility>
#include <vector>

extern float g_Time;
extern float g_TimeLast;

using namespace WallpaperEngine;
using namespace WallpaperEngine::Render;
using namespace WallpaperEngine::Data::Model;
using namespace WallpaperEngine::Data::Parsers;
using namespace WallpaperEngine::Render::Wallpapers;

namespace {
std::vector<std::string> tokenizeObjectName (const std::string& name) {
    std::vector<std::string> tokens;
    std::string token;
    for (size_t index = 0; index < name.size (); index++) {
	const unsigned char value = static_cast<unsigned char> (name[index]);
	const unsigned char previous
	    = index == 0 ? 0 : static_cast<unsigned char> (name[index - 1]);
	const bool camelBoundary = std::isupper (value) && !token.empty ()
	    && (std::islower (previous) || std::isdigit (previous));
	const bool numericBoundary = !token.empty () && std::isalnum (value) && std::isalnum (previous)
	    && (std::isdigit (value) != std::isdigit (previous));
	if (!std::isalnum (value) || camelBoundary || numericBoundary) {
	    if (!token.empty ()) {
		tokens.push_back (std::move (token));
		token.clear ();
	    }
	}
	if (std::isalnum (value)) {
	    token.push_back (static_cast<char> (std::tolower (value)));
	}
    }
    if (!token.empty ()) {
	tokens.push_back (std::move (token));
    }
    return tokens;
}

std::optional<int> eyeLayerRole (const std::vector<std::string>& tokens) {
    const auto has
	= [&tokens] (const std::string& expected) { return std::ranges::find (tokens, expected) != tokens.end (); };
    const bool hasEye = has ("eye") || has ("eyeball");
    const bool hasSide = has ("left") || has ("right");
    const bool numericSuffix = tokens.size () == 2
	&& std::ranges::all_of (tokens.back (), [] (unsigned char value) { return std::isdigit (value); });
    if (has ("sclera") || has ("eyewhite") || has ("eyeballwhite") || (hasEye && has ("white"))) {
	return 0;
    }
    if (has ("iris") && (hasEye || hasSide || tokens.size () == 1 || numericSuffix)) {
	return 1;
    }
    if (has ("pupil") && (hasEye || hasSide || tokens.size () == 1 || numericSuffix)) {
	return 2;
    }
    return std::nullopt;
}

std::string eyeLayerIdentity (const std::vector<std::string>& tokens) {
    std::string identity;
    for (const auto& token : tokens) {
	if (token == "eye" || token == "eyeball" || token == "white" || token == "sclera"
	    || token == "eyewhite" || token == "eyeballwhite" || token == "iris" || token == "pupil") {
	    continue;
	}
	if (!identity.empty ()) {
	    identity += '/';
	}
	identity += token;
    }
    return identity;
}

struct EyeLayerSemantics {
    int role;
    std::string identity;
};

std::optional<EyeLayerSemantics> semanticEyeLayer (const std::string& name) {
    auto tokens = tokenizeObjectName (name);
    const auto role = eyeLayerRole (tokens);
    if (!role.has_value ()) {
	return std::nullopt;
    }
    return EyeLayerSemantics { *role, eyeLayerIdentity (tokens) };
}
}

CScene::CScene (
    const Wallpaper& wallpaper, RenderContext& context, AudioContext& audioContext,
    const WallpaperState::TextureUVsScaling& scalingMode, const uint32_t& clampMode, const glm::vec2& uvOffset,
    const PostProcessSettings& postProcess
) : CWallpaper (wallpaper, context, audioContext, scalingMode, clampMode, uvOffset, postProcess) {
    this->m_startTime = g_Time;
    // caller should check this, if not a std::bad_cast is good to throw
    auto scene = wallpaper.as<Scene> ();

    // setup scripting engine
    this->m_scriptEngine = std::make_unique<Scripting::ScriptEngine> (*this, context.getMediaSource ());
    // setup the scene camera
    this->m_camera = std::make_unique<Camera> (*this, scene->camera);

    float width = scene->camera.projection.width;
    float height = scene->camera.projection.height;

    // Auto orthographic scenes and perspective scenes need canvas dimensions
    // inferred from authored content (falling back to the output size).
    if (scene->camera.projection.isAuto || scene->camera.projection.isPerspective) {
	glm::vec2 maxExtent = { 0.0f, 0.0f };

	for (const auto& object : scene->objects) {
	    if (!object->is<Image> ()) {
		continue;
	    }

	    const auto* image = object->as<Image> ();
	    if (!image->origin || !image->origin->value) {
		continue;
	    }

	    const glm::vec3 origin = image->origin->value->getVec3 ();
	    const glm::vec2 halfSize = image->size / 2.0f;

	    maxExtent.x = glm::max (maxExtent.x, glm::abs (origin.x) + halfSize.x);
	    maxExtent.y = glm::max (maxExtent.y, glm::abs (origin.y) + halfSize.y);
	}

	if (maxExtent.x > 0.0f && maxExtent.y > 0.0f) {
	    width = maxExtent.x * 2.0f;
	    height = maxExtent.y * 2.0f;
	} else {
	    width = this->getContext ().getOutput ().getFullWidth ();
	    height = this->getContext ().getOutput ().getFullHeight ();
	    sLog.debug ("Auto projection: falling back to screen resolution ", width, "x", height);
	}
    }

    this->m_parallaxDisplacement = { 0, 0 };

    // Perspective scenes are encoded by Wallpaper Engine as
    // general.orthogonalprojection = null.
    if (scene->camera.projection.isPerspective) {
	this->m_camera->setPerspectiveProjection (width, height);
    } else {
	this->m_camera->setOrthogonalProjection (width, height);
    }

    // setup framebuffers here as they're required for the scene setup
    this->setupFramebuffers ();

    const uint32_t sceneWidth = this->m_camera->getWidth ();
    const uint32_t sceneHeight = this->m_camera->getHeight ();

    this->_rt_shadowAtlas = this->create (
	"_rt_shadowAtlas", TextureFormat_ARGB8888, TextureFlags_ClampUVs, 1.0, { sceneWidth, sceneHeight },
	{ sceneWidth, sceneHeight }
    );
    this->alias ("_alias_lightCookie", "_rt_shadowAtlas");

    // set clear color
    const glm::vec3 clearColor = scene->colors.clear->value->getVec3 ();

    glClearColor (clearColor.r, clearColor.g, clearColor.b, 1.0f);

    // create all objects based off their dependencies
    for (const auto& object : scene->objects) {
	this->createObject (*object);
    }

    // copy over objects by render order
    for (const auto& object : scene->objects) {
	this->addObjectToRenderOrder (*object);
    }

    // Some scenes serialize eye-white images after their iris images. Correct only
    // within contiguous eye-layer runs, and keep distinct eyes separate by their
    // parent and semantic identity so unrelated authored layers never get crossed.
    size_t eyeRunStart = 0;
    while (eyeRunStart < this->m_objectsByRenderOrder.size ()) {
	const auto& firstObject = this->m_objectsByRenderOrder[eyeRunStart]->getObject ();
	const auto firstSemantics = firstObject.is<Image> () ? semanticEyeLayer (firstObject.name) : std::nullopt;
	if (!firstSemantics.has_value ()) {
	    eyeRunStart++;
	    continue;
	}

	std::map<std::pair<std::optional<int>, std::string>, std::vector<std::pair<size_t, int>>> eyeGroups;
	size_t eyeRunEnd = eyeRunStart;
	for (; eyeRunEnd < this->m_objectsByRenderOrder.size (); eyeRunEnd++) {
	    const auto& object = this->m_objectsByRenderOrder[eyeRunEnd]->getObject ();
	    if (!object.is<Image> ()) {
		break;
	    }
	    const auto semantics = semanticEyeLayer (object.name);
	    if (!semantics.has_value ()) {
		break;
	    }
	    eyeGroups[{ object.parent, semantics->identity }].emplace_back (eyeRunEnd, semantics->role);
	}

	for (const auto& [identity, positions] : eyeGroups) {
	    (void)identity;
	    const bool hasEyeBase
		= std::ranges::any_of (positions, [] (const auto& item) { return item.second == 0; });
	    const bool hasIris
		= std::ranges::any_of (positions, [] (const auto& item) { return item.second == 1; });
	    if (!hasEyeBase || !hasIris) {
		continue;
	    }

	    std::vector<std::pair<CObject*, int>> eyeLayers;
	    eyeLayers.reserve (positions.size ());
	    for (const auto& [index, role] : positions) {
		eyeLayers.emplace_back (this->m_objectsByRenderOrder[index], role);
	    }
	    std::ranges::stable_sort (eyeLayers, [] (const auto& left, const auto& right) {
		return left.second < right.second;
	    });
	    for (size_t index = 0; index < positions.size (); index++) {
		this->m_objectsByRenderOrder[positions[index].first] = eyeLayers[index].first;
	    }
	}

	eyeRunStart = eyeRunEnd;
    }

    // create extra framebuffers for the bloom effect
    this->_rt_4FrameBuffer = this->create (
	"_rt_4FrameBuffer", TextureFormat_ARGB8888, TextureFlags_ClampUVs, 1.0, { sceneWidth / 4, sceneHeight / 4 },
	{ sceneWidth / 4, sceneHeight / 4 }
    );
    this->_rt_8FrameBuffer = this->create (
	"_rt_8FrameBuffer", TextureFormat_ARGB8888, TextureFlags_ClampUVs, 1.0, { sceneWidth / 8, sceneHeight / 8 },
	{ sceneWidth / 8, sceneHeight / 8 }
    );
    this->_rt_Bloom = this->create (
	"_rt_Bloom", TextureFormat_ARGB8888, TextureFlags_ClampUVs, 1.0, { sceneWidth / 8, sceneHeight / 8 },
	{ sceneWidth / 8, sceneHeight / 8 }
    );

    //
    // Had to get a little creative with the effects to achieve the same bloom effect without any custom code
    // this custom image loads some effect files from the virtual container to achieve the same bloom effect
    // this approach requires of two extra draw calls due to the way the effect works in official WPE
    // (it renders directly to the screen, whereas here we never do that from a scene)
    //

    const auto bloomOrigin = glm::vec3 { sceneWidth / 2, sceneHeight / 2, 0.0f };
    const auto bloomSize = glm::vec2 { sceneWidth, sceneHeight };

    const JSON bloom
	= { { "image", "models/wpenginelinux.json" },
	    { "name", "bloomimagewpenginelinux" },
	    { "visible", true },
	    { "scale", "1.0 1.0 1.0" },
	    { "angles", "0.0 0.0 0.0" },
	    { "origin",
	      std::to_string (bloomOrigin.x) + " " + std::to_string (bloomOrigin.y) + " "
		  + std::to_string (bloomOrigin.z) },
	    { "size", std::to_string (bloomSize.x) + " " + std::to_string (bloomSize.y) },
	    { "id", -1 },
	    { "effects",
	      JSON::array (
		  { { { "file", "effects/wpenginelinux/bloomeffect.json" },
		      { "id", 15242000 },
		      { "name", "" },
		      { "passes",
			JSON::array (
			    { { { "constantshadervalues",
				  { { "bloomstrength", this->getScene ().camera.bloom.strength->value->getFloat () },
				    { "bloomthreshold",
				      this->getScene ().camera.bloom.threshold->value->getFloat () } } } },
			      { { "constantshadervalues",
				  { { "bloomstrength", this->getScene ().camera.bloom.strength->value->getFloat () },
				    { "bloomthreshold",
				      this->getScene ().camera.bloom.threshold->value->getFloat () } } } },
			      { { "constantshadervalues",
				  { { "bloomstrength", this->getScene ().camera.bloom.strength->value->getFloat () },
				    { "bloomthreshold",
				      this->getScene ().camera.bloom.threshold->value->getFloat () } } } } }
			) } } }
	      ) } };

    // create image for bloom passes
    if (scene->camera.bloom.enabled->value->getBool ()) {
	this->m_bloomObjectData = ObjectParser::parse (bloom, scene->project);
	this->m_bloomObject = this->createObject (*this->m_bloomObjectData);

	this->m_objectsByRenderOrder.push_back (this->m_bloomObject);
    }
}

CScene::~CScene () {
    // bloom object is in the objects list, so no need to explicitly delete it
    this->m_bloomObject = nullptr;

    for (const auto& val : this->m_objects | std::views::values) {
	delete val;
    }

    this->m_objectsByRenderOrder.clear ();
    this->m_objects.clear ();
}

Render::CObject* CScene::createObject (const Object& object) {
    Render::CObject* renderObject = nullptr;

    // ensure the item is not loaded already
    if (const auto current = this->m_objects.find (object.id); current != this->m_objects.end ()) {
	return current->second;
    }

    // Parent/dependency cycles are malformed Workshop data; avoid infinite recursion.
    if (!this->m_objectsBeingResolved.insert (object.id).second) {
	sLog.error ("Object dependency/parent cycle while resolving id=", object.id);
	return nullptr;
    }
    struct ResolutionGuard {
	std::unordered_set<int>& ids;
	int id;
	~ResolutionGuard () { ids.erase (id); }
    } guard { this->m_objectsBeingResolved, object.id };

    // check dependencies too!
    for (const auto& cur : object.dependencies) {
	// self-dependency is a possibility...
	if (cur == object.id) {
	    continue;
	}

	const auto dep
	    = std::ranges::find_if (this->getScene ().objects, [&cur] (const auto& o) { return o->id == cur; });

	if (dep != this->getScene ().objects.end ()) {
	    this->createObject (**dep);
	}
    }

    // check if the item has any parent and also create it first
    if (object.parent.has_value ()) {
	int parentId = object.parent.value ();

	const auto dep = std::ranges::find_if (this->getScene ().objects, [&parentId] (const auto& o) {
	    return o->id == parentId;
	});

	if (dep == this->getScene ().objects.end ()) {
	    sLog.exception ("Cannot find parent ", parentId, " for object ", object.id);
	}

	this->createObject (**dep);
    }

    renderObject = this->dispatchObjectType (object);

    if (renderObject != nullptr) {
	this->m_objects.emplace (renderObject->getId (), renderObject);
    }

    return renderObject;
}

Render::CObject* CScene::dispatchObjectType (const Object& object) {
    Render::CObject* renderObject = nullptr;

    if (object.is<Light> ()) {
	return nullptr;
    } else if (object.is<CameraObject> ()) {
	renderObject = new Objects::CCameraObject (*this, *object.as<CameraObject> ());
    } else if (object.is<Image> ()) {
	renderObject = new Objects::CImage (*this, *object.as<Image> ());
    } else if (object.is<ModelObject> ()) {
	renderObject = new Objects::CModel (*this, *object.as<ModelObject> ());
    } else if (object.is<Sound> ()) {
	renderObject = new Objects::CSound (*this, *object.as<Sound> ());
    } else if (object.is<Text> ()) {
	renderObject = new Objects::CText (*this, *object.as<Text> ());
    } else if (object.is<Particle> ()) {
	const auto& particleData = *object.as<Particle> ();

	if (this->getContext ().getApp ().getContext ().settings.general.disableParticles == true) {
	    sLog.debug ("Ignoring particle system (disabled in settings): ", particleData.name);
	    return nullptr;
	}

	renderObject = new Objects::CParticle (*this, particleData);
    } else {
	sLog.error ("Unknown object type, creating placeholder, empty object: ", object.id);
	renderObject = new CObject (*this, object);
    }

    try {
	renderObject->setup ();
    } catch (const std::exception& e) {
	sLog.error ("Failed to setup object ", object.id, ": ", e.what ());
	delete renderObject;
	renderObject = nullptr;
    }

    return renderObject;
}

void CScene::addObjectToRenderOrder (const Object& object) {
    const auto obj = this->m_objects.find (object.id);

    // ignores not created objects like particle systems
    if (obj == this->m_objects.end ()) {
	return;
    }

    // take into account any dependency first
    for (const auto& dep : object.dependencies) {
	// self-dependency is possible
	if (dep == object.id) {
	    continue;
	}

	// add the dependency to the list if it's created
	auto depIt = std::ranges::find_if (this->getScene ().objects, [&dep] (const auto& o) { return o->id == dep; });

	if (depIt != this->getScene ().objects.end ()) {
	    this->addObjectToRenderOrder (**depIt);
	} else {
	    sLog.error ("Cannot find dependency ", dep, " for object ", object.id);
	}
    }

    // ensure we're added only once to the render list
    const auto renderIt = std::ranges::find_if (this->m_objectsByRenderOrder, [&object] (const auto& o) {
	return o->getId () == object.id;
    });

    if (renderIt == this->m_objectsByRenderOrder.end ()) {
	this->m_objectsByRenderOrder.emplace_back (obj->second);
    }
}

ScriptEngine& CScene::getScriptEngine () const { return *this->m_scriptEngine; }
Camera& CScene::getCamera () const { return *this->m_camera; }

void CScene::renderFrame (const glm::ivec4& viewport) {
    if (!this->m_hasStarted) {
	this->m_startTime = g_Time;
	this->m_hasStarted = true;
    }

    // ensure the virtual mouse position is up to date
    this->updateMouse (viewport);

    // update the parallax position if required
    if (this->getScene ().camera.parallax.enabled->value->getBool ()
	&& this->getContext ().getApp ().getContext ().settings.mouse.enabled
	&& !this->getContext ().getApp ().getContext ().settings.mouse.disableparallax) {
	const float influence = this->getScene ().camera.parallax.mouseInfluence->value->getFloat ();
	const float amount = this->getScene ().camera.parallax.amount->value->getFloat ();
	const float delay = glm::clamp (
	    this->getScene ().camera.parallax.delay->value->getFloat () * (g_Time - g_TimeLast), 0.0f, 1.0f
	);

	const glm::vec2 centeredMouse = this->m_mousePosition - glm::vec2 (0.5f, 0.5f);
	this->m_parallaxDisplacement
	    = glm::mix (this->m_parallaxDisplacement, (centeredMouse * amount) * influence, delay);
    }

    // run a tick in the javascript logic
    this->getScriptEngine ().tick ();
    this->updateActiveCamera ();

    // update main textures for images
    for (const auto& cur : this->m_objectsByRenderOrder) {
	if (!cur->is<Objects::CImage> ()) {
	    continue;
	}

	const Objects::CImage* image = cur->as<Objects::CImage> ();

#if !NDEBUG
	const std::string message = "Updating texture " + image->getImage ().model->filename;

	glPushDebugGroup (GL_DEBUG_SOURCE_APPLICATION, 0, -1, message.c_str ());
#endif

	image->getTexture ()->update ();

#if !NDEBUG
	glPopDebugGroup ();
#endif
    }

    // bind the vertex array
    glBindVertexArray (this->m_vaoBuffer);
    // use the scene's framebuffer by default
    glBindFramebuffer (GL_FRAMEBUFFER, this->getWallpaperFramebuffer ());
    // ensure we render over the whole framebuffer
    glViewport (0, 0, this->m_sceneFBO->getRealWidth (), this->m_sceneFBO->getRealHeight ());

    glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    const auto& debug = this->getContext ().getApp ().getContext ().settings.render.debug;

    const auto isVisible = [] (const Object& object) {
	const UserSettingUniquePtr* setting = &object.groupVisible;
	if (object.is<Image> ()) {
	    setting = &object.as<Image> ()->visible;
	} else if (object.is<Particle> ()) {
	    setting = &object.as<Particle> ()->visible;
	} else if (object.is<Text> ()) {
	    setting = &object.as<Text> ()->visible;
	} else if (object.is<ModelObject> ()) {
	    setting = &object.as<ModelObject> ()->visible;
	}
	return *setting != nullptr && (*setting)->value != nullptr && (*setting)->value->getBool ();
    };

    const auto visibleInHierarchy = [this, &isVisible] (const CObject* object) {
	const Object* current = &object->getObject ();
	for (int depth = 0; current != nullptr && depth < 32; depth++) {
	    if (!isVisible (*current)) {
		return false;
	    }
	    if (!current->parent.has_value ()) {
		return true;
	    }
	    const auto parent = this->m_objects.find (*current->parent);
	    if (parent == this->m_objects.end ()) {
		return true;
	    }
	    current = &parent->second->getObject ();
	}
	return false;
    };

    const auto enabledByDebug = [&debug, &visibleInHierarchy] (const CObject* object) {
	if (!visibleInHierarchy (object)) {
	    return false;
	}
	if (debug.objectFilter.has_value () && object->getId () != debug.objectFilter.value ()) {
	    return false;
	}
	return std::ranges::find (debug.skipObjects, object->getId ()) == debug.skipObjects.end ();
    };

    const auto findAuthored = [this] (const int id) -> const Object* {
	const auto it = std::ranges::find_if (this->getScene ().objects, [id] (const auto& object) {
	    return object != nullptr && object->id == id;
	});
	return it == this->getScene ().objects.end () ? nullptr : it->get ();
    };

    // Return the nearest authored composition-layer ancestor of a render object.
    const auto compositionAncestor = [this, &findAuthored] (const CObject* object) -> Objects::CImage* {
	const Object* current = findAuthored (object->getId ());
	for (int depth = 0; current != nullptr && current->parent.has_value () && depth < 32; depth++) {
	    const auto parentIt = this->m_objects.find (*current->parent);
	    if (parentIt == this->m_objects.end ()) {
		break;
	    }
	    if (auto* image = dynamic_cast<Objects::CImage*> (parentIt->second);
		image != nullptr && image->isCompositionLayer () && image->getCompositionFBO () != nullptr) {
		return image;
	    }
	    current = findAuthored (parentIt->second->getId ());
	}
	return nullptr;
    };

    std::set<int> compositionSubmitted;
    const auto renderCompositionImpl = [&] (Objects::CImage* composition, const auto& self) -> void {
	if (composition == nullptr || !compositionSubmitted.insert (composition->getId ()).second) {
	    return;
	}

	const auto target = composition->getCompositionFBO ();
	if (target == nullptr) {
	    if (enabledByDebug (composition)) {
		composition->render ();
	    }
	    return;
	}

	const auto previousTarget = this->m_compositionRenderTarget;
	const auto source = this->getActiveRenderTarget ();

	GLfloat previousClearColor[4] = {};
	glGetFloatv (GL_COLOR_CLEAR_VALUE, previousClearColor);
	glBindFramebuffer (GL_FRAMEBUFFER, target->getFramebuffer ());
	glViewport (0, 0, target->getRealWidth (), target->getRealHeight ());
	glColorMask (true, true, true, true);
	glDepthMask (true);
	glClearColor (0.0f, 0.0f, 0.0f, 0.0f);
	glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glClearColor (previousClearColor[0], previousClearColor[1], previousClearColor[2], previousClearColor[3]);

	if (composition->copiesCompositionBackground () && source != nullptr && source != target) {
	    glBindFramebuffer (GL_READ_FRAMEBUFFER, source->getFramebuffer ());
	    glBindFramebuffer (GL_DRAW_FRAMEBUFFER, target->getFramebuffer ());
	    glBlitFramebuffer (
		0, 0, source->getRealWidth (), source->getRealHeight (), 0, 0, target->getRealWidth (),
		target->getRealHeight (), GL_COLOR_BUFFER_BIT, GL_LINEAR
	    );
	}

	this->m_compositionRenderTarget = target;
	for (CObject* child : this->m_objectsByRenderOrder) {
	    if (compositionSubmitted.contains (child->getId ()) || compositionAncestor (child) != composition) {
		continue;
	    }
	    if (auto* nested = dynamic_cast<Objects::CImage*> (child);
		nested != nullptr && nested->isCompositionLayer () && nested->getCompositionFBO () != nullptr) {
		self (nested, self);
	    } else {
		compositionSubmitted.insert (child->getId ());
		if (enabledByDebug (child)) {
		    child->render ();
		}
	    }
	}

	this->m_compositionRenderTarget = previousTarget;
	if (enabledByDebug (composition)) {
	    composition->render ();
	}
    };

    const auto renderComposition
	= [&] (Objects::CImage* composition) { renderCompositionImpl (composition, renderCompositionImpl); };

    for (const auto& cur : this->m_objectsByRenderOrder) {
	if (compositionSubmitted.contains (cur->getId ())) {
	    continue;
	}

	if (auto* ancestor = compositionAncestor (cur); ancestor != nullptr) {
	    renderComposition (ancestor);
	    continue;
	}

	if (auto* image = dynamic_cast<Objects::CImage*> (cur);
	    image != nullptr && image->isCompositionLayer () && image->getCompositionFBO () != nullptr) {
	    renderComposition (image);
	    continue;
	}

	if (enabledByDebug (cur)) {
	    cur->render ();
	}
    }
}

void CScene::updateActiveCamera () {
    Objects::CCameraObject* active = nullptr;
    for (const auto& object : this->getScene ().objects) {
	if (!object->is<CameraObject> () || !object->groupVisible->value->getBool ()) {
	    continue;
	}
	const auto* data = object->as<CameraObject> ();
	if (data->camera != "default") {
	    continue;
	}
	const auto it = this->m_objects.find (object->id);
	if (it != this->m_objects.end ()) {
	    if (auto* camera = dynamic_cast<Objects::CCameraObject*> (it->second)) {
		active = camera; // Wallpaper Engine selects the last visible camera asset.
	    }
	}
    }
    if (active == nullptr) {
	this->m_camera->clearScriptedView ();
	return;
    }

    std::vector<Scripting::ScriptableObject*> hierarchy;
    Render::CObject* current = active;
    for (int depth = 0; current != nullptr && depth < 32; depth++) {
	if (auto* layer = dynamic_cast<Scripting::ScriptableObject*> (current)) {
	    hierarchy.push_back (layer);
	}
	const auto parent = current->getObject ().parent;
	if (!parent.has_value ()) {
	    break;
	}
	const auto it = this->m_objects.find (*parent);
	if (it == this->m_objects.end ()) {
	    break;
	}
	current = it->second;
    }

    glm::mat4 world (1.0f);
    for (auto it = hierarchy.rbegin (); it != hierarchy.rend (); ++it) {
	const glm::vec3 origin = (*it)->getProperty ("origin").getVec3 ();
	const glm::vec3 angles = (*it)->getProperty ("angles").getVec3 ();
	const glm::vec3 scale = (*it)->getProperty ("scale").getVec3 ();
	glm::mat4 local = glm::translate (glm::mat4 (1.0f), origin);
	local = glm::rotate (local, angles.z, glm::vec3 (0, 0, 1));
	local = glm::rotate (local, angles.y, glm::vec3 (0, 1, 0));
	local = glm::rotate (local, angles.x, glm::vec3 (1, 0, 0));
	local = glm::scale (local, scale);
	world *= local;
    }

    const glm::vec3 eye (world[3]);
    const glm::mat3 rotation (world);
    const glm::vec3 forward = glm::normalize (rotation * glm::vec3 (0, 0, -1));
    const glm::vec3 up = glm::normalize (rotation * glm::vec3 (0, 1, 0));
    const float fov = active->getCameraObject ().fov->value->getFloat ();
    this->m_camera->setScriptedView (eye, eye + forward, up, fov);
}

void CScene::updateMouse (const glm::ivec4& viewport) {
    // update virtual mouse position first
    const glm::dvec2 position = this->getContext ().getInputContext ().getMouseInput ().position ();

    // rollover the position to the last
    this->m_mousePositionLast = this->m_mousePosition;

    // calculate the current position of the mouse in viewport space [0, 1]
    double mouseX = glm::clamp ((position.x - viewport.x) / viewport.z, 0.0, 1.0);
    // Normalize Y coordinate (OpenGL convention: 0=bottom, 1=top)
    // Particle code expects this convention: 0=bottom results in negative Y (down), 1=top results in positive Y (up)
    double normalizedMouseY = glm::clamp ((position.y - viewport.y) / viewport.w, 0.0, 1.0);

    // Account for UV cropping when using fill/fit scaling modes
    // The scene may be rendered larger than viewport and cropped via UVs
    const auto uvs = this->getState ().getTextureUVs ();

    // Map mouse position from viewport space to scene UV space
    // UVs define what portion of the scene texture is visible
    this->m_mousePositionNormalized.x = uvs.ustart + mouseX * (uvs.uend - uvs.ustart);
    this->m_mousePositionNormalized.y = uvs.vstart + normalizedMouseY * (uvs.vend - uvs.vstart);

    // Invert previous normalization of Y to match what the shader expects
    double mouseY = 1.0 - normalizedMouseY;

    this->m_mousePosition.x = this->m_mousePositionNormalized.x;
    this->m_mousePosition.y = uvs.vstart + mouseY * (uvs.vend - uvs.vstart);
}

const Scene& CScene::getScene () const { return *this->getWallpaperData ().as<Scene> (); }

std::shared_ptr<const CFBO> CScene::getActiveRenderTarget () const {
    return this->m_compositionRenderTarget != nullptr ? this->m_compositionRenderTarget : this->getFBO ();
}

std::shared_ptr<const CFBO> CScene::resolveRenderTarget (const std::shared_ptr<const CFBO>& requested) const {
    if (requested == this->getFBO () && this->m_compositionRenderTarget != nullptr) {
	return this->m_compositionRenderTarget;
    }
    return requested;
}

bool CScene::isRenderingToComposition () const { return this->m_compositionRenderTarget != nullptr; }

bool CScene::hasAuthoredChildren (int parentId) const {
    return std::ranges::any_of (this->getScene ().objects, [parentId] (const auto& object) {
	return object != nullptr && object->parent.has_value () && *object->parent == parentId;
    });
}

int CScene::getWidth () const { return this->m_camera->getWidth (); }

int CScene::getHeight () const { return this->m_camera->getHeight (); }

float CScene::getTime () const { return g_Time - this->m_startTime; }

float CScene::getDeltaTime () const { return g_Time - g_TimeLast; }

float CScene::getFps () const {
    const float dt = g_Time - g_TimeLast;
    // Guard against the first frame (where g_TimeLast is 0 so dt == g_Time)
    // and division by zero on the very first call.
    if (dt <= 1e-6f) {
	return 60.0f;
    }
    return 1.0f / dt;
}

const glm::vec2* CScene::getMousePosition () const { return &this->m_mousePosition; }

const glm::vec2* CScene::getMousePositionLast () const { return &this->m_mousePositionLast; }

const glm::vec2* CScene::getMousePositionNormalized () const { return &this->m_mousePositionNormalized; }

const glm::vec2* CScene::getParallaxDisplacement () const { return &this->m_parallaxDisplacement; }

const std::vector<CObject*>& CScene::getObjectsByRenderOrder () const { return this->m_objectsByRenderOrder; }

const CObject* CScene::getObject (int id) const {
    const auto object = this->m_objects.find (id);
    return object == this->m_objects.end () ? nullptr : object->second;
}
