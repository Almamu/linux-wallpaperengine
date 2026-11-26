#include "WallpaperEngine/Render/Objects/CImage.h"
#include "WallpaperEngine/Render/Objects/CLight.h"
#include "WallpaperEngine/Render/Objects/CModel.h"
#include "WallpaperEngine/Render/Objects/ModelInputSignature.h"
#include "WallpaperEngine/Render/Objects/CParticle.h"
#include "WallpaperEngine/Render/Objects/CSound.h"
#include "WallpaperEngine/Render/Objects/CText.h"
#include "WallpaperEngine/Render/Objects/CShape.h"

#include "WallpaperEngine/Render/WallpaperState.h"

#include "CScene.h"
#include "WallpaperEngine/Render/CTexture.h"
#include "WallpaperEngine/Render/Shaders/Shader.h"
#include "WallpaperEngine/Render/Shaders/GLSLContext.h"
#include "WallpaperEngine/Data/Parsers/TextureParser.h"
#include "HdrBloom.h"
#include "VolumetricLights.h"
#include "WallpaperEngine/Render/Depth.h"
#include "WallpaperEngine/Logging/Log.h"

#include "WallpaperEngine/Data/Model/Wallpaper.h"
#include "WallpaperEngine/Data/Parsers/MaterialParser.h"
#include "WallpaperEngine/Data/Parsers/ObjectParser.h"
#include "WallpaperEngine/Data/Utils/ScopeGuard.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <fstream>
#include <limits>
#include <numeric>
#include <ranges>

extern float g_Time;

using namespace WallpaperEngine;
using namespace WallpaperEngine::Render;
using namespace WallpaperEngine::Data::Model;
using namespace WallpaperEngine::Data::Parsers;
using namespace WallpaperEngine::Render::Wallpapers;

CScene::CScene (
    const Wallpaper& wallpaper, RenderContext& context, AudioContext& audioContext,
    const WallpaperState::TextureUVsScaling& scalingMode, const uint32_t& clampMode, const std::string& screenName,
    std::function<void (bool)> loadingCheckpoint
) : CWallpaper (wallpaper, context, audioContext, scalingMode, clampMode),
    m_loadingCheckpoint (std::move (loadingCheckpoint)), m_loadingRenderTime (g_Time),
    m_loadingTimerTime (std::chrono::steady_clock::now ()) {
    // Optional supersampling is independent from scene MSAA and post-processing quality.
    this->setRenderScale (context.getApp ().getContext ().settings.render.renderScale);

    // caller should check this, if not a std::bad_cast is good to throw
    auto scene = wallpaper.as<Scene> ();
    this->m_hdrEnabled = context.getApp ().getContext ().settings.render.postProcessing
        == Application::ApplicationContext::PostProcessing::Ultra
        && scene->hdr && scene->camera.bloom.enabled->value->getBool ();

    // CScene still owns render objects as raw pointers. Its destructor is not called
    // when construction throws, so clean any objects created before the failure here.
    // This guard runs before member teardown, while ScriptEngine is still usable by
    // object destructors such as CText's script-layer cleanup.
    WallpaperEngine::Data::Utils::ScopeGuard constructionGuard ([this] { this->destroyObjects (); });

    // setup scripting engine
    this->m_scriptEngine = std::make_unique<Scripting::ScriptEngine> (*this, context.getMediaSource (), screenName);
    // setup the scene camera
    this->m_camera = std::make_unique<Camera> (*this, scene->camera);

    float width = scene->camera.projection.width;
    float height = scene->camera.projection.height;
    const bool isPerspective = scene->camera.projection.isPerspective;

    // Raster resolution belongs to the output. Orthographic canvas dimensions
    // remain authored units, while perspective cameras use the output aspect.
    this->m_outputSize = context.getSceneOutputSize (screenName);
    this->m_textureReduction = context.getTextureReduction (wallpaper, context.getTextureQualityOutputSize (screenName));
    if (isPerspective) {
	width = this->m_outputSize.x;
	height = this->m_outputSize.y;
    }

    const auto autoBoundsSize = prepareStaticAutoCollisionBounds (*scene);
    if (autoBoundsSize) m_autoCollisionBoundsExtent = glm::ivec2 (*autoBoundsSize);
    if (autoBoundsSize && autoBoundsSize->x > 0.0f && autoBoundsSize->y > 0.0f) {
        width = autoBoundsSize->x;
        height = autoBoundsSize->y;
    // Unsupported AUTO scenes and the image-less display keep their existing fallback.
    } else if (!isPerspective && scene->camera.projection.isAuto) {
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
	    // Use the first-captured output size, not the live one: an in-process rebuild (live property
	    // change, control-socket bg swap) must size the scene exactly like the original build or the
	    // effect-composite chain samples misaligned buffers and effects stop showing.
	    const auto fallback = this->getContext ().getStableOutputSize ();
	    width = fallback.x;
	    height = fallback.y;
	    sLog.debug ("Auto projection: falling back to screen resolution ", width, "x", height);
	}
    }

    this->m_parallaxDisplacement = { 0, 0 };

    if (isPerspective) {
	this->m_camera->setPerspectiveProjection (
	    width, height, this->getContext ().getOutput ().renderVFlip ()
	);
    } else {
	this->m_camera->setOrthogonalProjection (width, height);
    }
    this->updateUVs ({ 0, 0, this->m_outputSize.x, this->m_outputSize.y }, context.getOutput ().renderVFlip ());
    const auto framing = this->getState ().getTextureUVs ();
    this->m_camera->setViewportScale ({ 1.0f / (framing.uend - framing.ustart),
	1.0f / std::abs (framing.vend - framing.vstart) });

    {
	// Orthographic scenes can contain lit images and models too. Compile every
	// scene's LightingV1 modules with its authored lights before creating passes.
	for (const auto& object : scene->objects) {
	    if (!object->is<Data::Model::Light> ()) {
		continue;
	    }

	    const auto* light = object->as<Data::Model::Light> ();
	    switch (light->type) {
		case LightData::Type_Directional:
		    // Directional cascades currently use a perspective camera frustum.
		    if (light->castShadow && isPerspective && this->m_lights.directionalCount < 32) {
			this->m_lights.directionalShadowCount++;
		    }
		    this->m_lights.directionalCount++;
		    break;
		case LightData::Type_Spot:
		    if (light->castShadow && this->m_lights.spotCount < 32) {
			this->m_lights.spotShadowCount++;
		    }
		    this->m_lights.spotCount++;
		    break;
		case LightData::Type_Tube: this->m_lights.tubeCount++; break;
		case LightData::Type_Point:
		    if (light->castShadow && this->m_lights.pointCount < 32) {
			this->m_lights.pointShadowCount++;
		    }
		    this->m_lights.pointCount++;
		    break;
	    }
	}

	this->m_lights.shadowFeatureCount
	    = this->m_lights.directionalShadowCount * 3 + this->m_lights.spotShadowCount;
	this->m_lights.shadowViewCount
	    = this->m_lights.shadowFeatureCount + this->m_lights.pointShadowCount * 6;
	this->m_lights.directionalDirections.resize (this->m_lights.directionalCount, glm::vec4 (0.0f));
	this->m_lights.directionalColors.resize (this->m_lights.directionalCount, glm::vec4 (0.0f));
	this->m_lights.directionalShadowFeatures.resize (this->m_lights.directionalCount, glm::ivec3 (-1));
	this->m_lights.directionalShadowEnabled.resize (this->m_lights.directionalCount, 0.0f);
	this->m_lights.pointOrigins.resize (this->m_lights.pointCount, glm::vec4 (0.0f));
	this->m_lights.pointColors.resize (this->m_lights.pointCount, glm::vec4 (0.0f));
	this->m_lights.pointShadowProjections.resize (this->m_lights.pointCount, glm::vec4 (0.0f));
	this->m_lights.pointShadowTransforms.resize (this->m_lights.pointCount, glm::vec4 (0.0f));
	this->m_lights.pointShadowEnabled.resize (this->m_lights.pointCount, 0.0f);
	this->m_lights.pointShadowMatrices.resize (this->m_lights.pointCount);
	this->m_lights.pointShadowViewports.resize (this->m_lights.pointCount);
	this->m_lights.spotOrigins.resize (this->m_lights.spotCount, glm::vec4 (0.0f));
	this->m_lights.spotDirections.resize (this->m_lights.spotCount, glm::vec4 (0.0f));
	this->m_lights.spotColors.resize (this->m_lights.spotCount, glm::vec4 (0.0f));
	this->m_lights.spotExponents.resize (this->m_lights.spotCount, glm::vec4 (0.0f));
	this->m_lights.spotCookieMatrices.resize (this->m_lights.spotCount, glm::mat4 (1.0f));
	this->m_lights.spotShadowFeatures.resize (this->m_lights.spotCount, -1);
	this->m_lights.spotShadowEnabled.resize (this->m_lights.spotCount, 0.0f);
	this->m_lights.shadowMatrices.resize (this->m_lights.shadowFeatureCount, glm::mat4 (1.0f));
	this->m_lights.shadowTransforms.resize (this->m_lights.shadowFeatureCount, glm::vec4 (0.0f));
	this->m_lights.shadowFeatureEnabled.resize (this->m_lights.shadowFeatureCount, 0.0f);
	this->m_lights.shadowViewports.resize (this->m_lights.shadowFeatureCount, glm::ivec4 (0));
	this->m_lights.tubeOriginsA.resize (this->m_lights.tubeCount, glm::vec4 (0.0f));
	this->m_lights.tubeOriginsB.resize (this->m_lights.tubeCount, glm::vec4 (0.0f));
	this->m_lights.tubeColors.resize (this->m_lights.tubeCount, glm::vec4 (0.0f));

	int directional = 0;
	int point = 0;
	int spot = 0;
	int tube = 0;
	int directionalFeature = 0;
	int spotFeature = this->m_lights.directionalShadowCount * 3;
	for (const auto& object : scene->objects) {
	    if (!object->is<Data::Model::Light> ()) {
		continue;
	    }

	    const auto* light = object->as<Data::Model::Light> ();
	    if (light->type == LightData::Type_Directional) {
		this->m_authoredLightSlots.emplace (light->id, directional);
		if (light->castShadow && isPerspective && directional < 32) {
		    this->m_lights.directionalShadowFeatures[directional]
			= glm::ivec3 (directionalFeature, directionalFeature + 1, directionalFeature + 2);
		    directionalFeature += 3;
		    this->m_lights.directionalShadowMask |= uint32_t (1) << directional;
		}
		directional++;
	    } else if (light->type == LightData::Type_Point) {
		this->m_authoredLightSlots.emplace (light->id, point);
		if (light->castShadow && point < 32) {
		    this->m_lights.pointShadowMask |= uint32_t (1) << point;
		}
		point++;
	    } else if (light->type == LightData::Type_Spot) {
		this->m_authoredLightSlots.emplace (light->id, spot);
		if (light->useCookie && spot < 32) this->m_lights.spotCookieMask |= uint32_t (1) << spot;
		if (light->castShadow && spot < 32) {
		    this->m_lights.spotShadowFeatures[spot] = spotFeature++;
		    this->m_lights.spotShadowMask |= uint32_t (1) << spot;
		}
		spot++;
	    } else if (light->type == LightData::Type_Tube) {
		this->m_authoredLightSlots.emplace (light->id, tube++);
	    }
	}
    }

    // setup framebuffers here as they're required for the scene setup;
    // models depth-test their submeshes against each other, so any scene rendering one needs a
    // depth buffer. Orthographic scenes can carry MDL layers too (workshop 3101147701 places
    // Pokemon models on a 2D canvas); without the attachment their materials' authored
    // depthtest/depthwrite are silent no-ops and submeshes simply paint in draw order, hiding
    // the eye/face material behind the body and floating far geometry over near geometry.
    bool hasModels = false;
    for (const auto& object : scene->objects) {
	if (object->is<Data::Model::Model3D> ()) {
	    hasModels = true;
	    break;
	}
    }
    m_camera->setReverseDepth (isPerspective && ClipDepthScope::supported ());
    const ComboMap noCombos;
    const TextureMap noTextures;
    std::set<std::string> precachedTextures;
    std::map<std::string, std::pair<int, int>> precachedTextureMetadata;
    for (const auto& material : scene->precachedMaterials) {
	for (const auto& pass : material->passes | std::views::take (1)) {
	    TextureMap textures = pass->textures;
	    ComboMap combos = pass->combos;
	    this->applyLightCombos (combos);
	    std::set<int> formatRequests;
	    bool shaderMetadataReady = false;
	    bool shaderFogEnabled = false;
	    try {
		// Discover file dependencies before selecting the load-time shader variant.
		const Shaders::Shader shader (
		    this->getAssetLocator (), pass->shader, combos, noCombos, pass->textures, noTextures, pass->constants
		);
		TextureMap defaults = shader.getVertex ().getTextures ();
		for (const auto& [slot, name] : shader.getFragment ().getTextures ()) defaults[slot] = name;
		for (const auto& [slot, name] : defaults) textures.try_emplace (slot, name);
		for (const auto& [slot, name] : textures) {
		    if (shader.getVertex ().requestsTextureFormat (slot)
			|| shader.getFragment ().requestsTextureFormat (slot)) formatRequests.insert (slot);
		}
		ComboMap discovered = shader.getVertex ().getDiscoveredCombos ();
		for (const auto& [name, value] : shader.getFragment ().getDiscoveredCombos ()) discovered[name] = value;
		for (const auto& [name, value] : pass->combos) discovered[name] = value;
		if (const auto fog = discovered.find ("FOG"); fog != discovered.end ()) shaderFogEnabled = fog->second != 0;
		shaderMetadataReady = true;
	    } catch (const std::exception& error) {
		sLog.error ("Cannot precache shader defaults for ", pass->shader, ": ", error.what ());
	    }
	    const auto acquireTexture = [&] (const std::string& name) {
		if (name.empty () || name.starts_with ("_rt_") || name.starts_with ("_alias_")
		    || name.starts_with ('$') || !precachedTextures.insert (name).second) return;
		try {
		    // Shader-only defaults are discovered after the initial texture batch.
		    const auto prepared = context.prepareTextureFallbacks (
			{ name }, this->getAssetLocator (), scene->project.properties, this->m_textureReduction
		    );
		    auto texture = this->resolveTexture (name);
		    precachedTextureMetadata.emplace (
			name, std::pair { static_cast<int> (texture->getFormat ()),
			    static_cast<int> ((texture->getFlags () & TextureFlags_ComponentMask) >> 20) }
		    );
		    this->m_precachedTextures.emplace_back (std::move (texture));
		} catch (const std::exception& error) {
		    sLog.error ("Cannot precache texture ", name, ": ", error.what ());
		}
		// Service outgoing frames between dependencies, including cache misses.
		this->loadingCheckpoint (true);
	    };
	    // Authored slots and format opt-ins affect compilation even when inactive.
	    for (const auto& [slot, name] : textures) {
		if (pass->textures.contains (slot) || formatRequests.contains (slot)) acquireTexture (name);
	    }
	    std::shared_ptr<const Shaders::ShaderProgramCache::SharedProgram> program;
	    // Native material acquisition selects only pass zero and retains its shader resource.
	    if (pass == material->passes.front () && shaderMetadataReady) {
		try {
		    combos.insert_or_assign ("HDR", this->isHdr () ? 1 : 0);
		    combos.insert_or_assign ("SCENE_ORTHO", isPerspective ? 0 : 1);
		    combos.insert_or_assign ("ALPHATOCOVERAGE", pass->blending == BlendingMode_AlphaToCoverage ? 1 : 0);
		    combos.insert_or_assign (
			"FOG_DIST", isPerspective && shaderFogEnabled && scene->fog.distance.enabled->value->getBool ()
		    );
		    combos.insert_or_assign (
			"FOG_HEIGHT", isPerspective && shaderFogEnabled && scene->fog.height.enabled->value->getBool ()
		    );
		    for (const auto& [slot, name] : textures) {
			if (const auto metadata = precachedTextureMetadata.find (name); metadata != precachedTextureMetadata.end ()) {
			    const std::string prefix = "TEX" + std::to_string (slot);
			    if (pass->textures.contains (slot)) combos.insert_or_assign (prefix + "COMPONENTS", metadata->second.second);
			    if (formatRequests.contains (slot)) combos.insert_or_assign (prefix + "FORMAT", metadata->second.first);
			}
		    }
		    Shaders::Shader shader (
			this->getAssetLocator (), pass->shader, combos, noCombos, pass->textures, noTextures, pass->constants
		    );
		    const auto [vertex, fragment] = Shaders::GLSLContext::get ().toGlsl (shader.vertex (), shader.fragment ());
		    std::shared_ptr<Shaders::ShaderProgramCache::SharingGroup> group;
		    program = context.getShaderProgramCache ().interfaceProgram (vertex, fragment, group);
		    this->m_precachedPrograms.emplace_back (program);
		} catch (const std::exception& error) {
		    sLog.error ("Cannot precache shader program ", pass->shader, ": ", error.what ());
		}
		this->loadingCheckpoint (true);
	    }
	    if (program) {
		// Native fills missing providers only for reflected texture bindings.
		for (const auto& [slot, name] : textures) {
		    if (!pass->textures.contains (slot) && !formatRequests.contains (slot)
			&& glGetUniformLocation (program->id, ("g_Texture" + std::to_string (slot)).c_str ()) >= 0)
			acquireTexture (name);
		}
	    }
	}
    }
    // Module-scope scene scripts can query the output during registration.
    this->registerFogScripts ();
    this->updateFogState ();
    const auto requestedSamples = context.getApp ().getContext ().settings.render.msaaSamples;
    const auto samples = scene->hasModelLayer ? requestedSamples : 1;
    this->setupFramebuffers (isPerspective || hasModels || samples > 1, this->m_outputSize, samples, this->getColorFormat (), m_camera->usesReverseDepth ());
    if (this->m_hdrEnabled) {
        const auto size = glm::max (glm::uvec2 (this->getFramebufferSize ()), glm::uvec2 (2));
        this->find ("_rt_FullFrameBuffer")->resize (size.x, size.y);
    }
    sLog.out ("Scene MSAA: requested=", requestedSamples, " actual=", this->m_sceneFBO->getSamples ());

    const uint32_t sceneWidth = this->m_camera->getWidth ();
    const uint32_t sceneHeight = this->m_camera->getHeight ();

    if (this->m_lights.shadowViewCount > 0) {
	glGetIntegerv (GL_MAX_TEXTURE_SIZE, &this->m_shadowAtlasMaxExtent);
	this->_rt_shadowAtlas = this->create (
	    "_rt_shadowAtlas", TextureFormat_ARGB8888, TextureFlags_ClampUVsBorder, 1.0,
	    { 2, 2 }, { 2, 2 }, false, true,
            1, this->getCamera ().usesReverseDepth ()
	);
    } else {
	// Keep the legacy placeholder available for shaders that declare the atlas but
	// compile their shadow combo out.
	this->_rt_shadowAtlas = this->create (
	    "_rt_shadowAtlas", TextureFormat_ARGB8888, TextureFlags_ClampUVs, 1.0,
	    { sceneWidth, sceneHeight }, { sceneWidth, sceneHeight }
	);
    }
    // Native creates the alias from its 6x6 diagnostic texture. Material
    // resolution uniforms retain this initial size when the selected pixels change.
    this->m_lightCookieAlias = this->create (
	"_alias_lightCookie", TextureFormat_ARGB8888, TextureFlags_NoInterpolation | TextureFlags_ClampUVs,
	1.0, { 6, 6 }, { 6, 6 }
    );
    std::array<unsigned char, 6 * 6 * 4> cookieFallback {};
    for (int y = 0; y < 6; ++y) for (int x = 0; x < 6; ++x) {
	const int pixel = (y * 6 + x) * 4;
	cookieFallback[pixel] = cookieFallback[pixel + 1] = (x + y) % 2 ? 255 : 0;
	cookieFallback[pixel + 3] = 255;
    }
    glBindTexture (GL_TEXTURE_2D, m_lightCookieAlias->getTextureID (0));
    glTexSubImage2D (GL_TEXTURE_2D, 0, 0, 0, 6, 6, GL_RGBA, GL_UNSIGNED_BYTE, cookieFallback.data ());

    // set clear color
    const glm::vec3 clearColor = scene->colors.clear->value->getVec3 ();

    glClearColor (clearColor.r, clearColor.g, clearColor.b, 1.0f);

    // create all objects based off their dependencies
    for (const auto& object : scene->objects) {
	this->createObject (*object);
	this->loadingCheckpoint (true);
    }

    // Shader light slots are assigned in authored order, while dependency
    // construction may create their parents first. Keep per-frame uniforms and
    // cookie/shadow masks on the same light identities.
    m_lightObjects.clear ();
    for (const auto& object : scene->objects) {
	if (const auto found = m_objects.find (object->id); found != m_objects.end ()) {
	    if (auto* light = dynamic_cast<Objects::CLight*> (found->second)) {
		m_lightObjects.push_back (light);
		if (light->getLight ().castShadow) m_shadowLightOrder.push_back (light);
	    }
	}
    }
    for (auto* light : m_lightObjects) light->initializeCookie ();
    // copy over objects by render order
    for (const auto& object : scene->objects) {
	this->addObjectToRenderOrder (*object);
	if (const auto rendered = m_objects.find (object->id); rendered != m_objects.end ())
	    m_objectsByCursorOrder.push_back (rendered->second);
    }

    // Property-script init hooks commonly resolve other layers by name. Construction can
    // follow dependency order rather than authored order, so wait until both registries are
    // complete before allowing those hooks to cache their targets.
    this->m_scriptEngine->initializeQueuedScripts ();
    this->loadingCheckpoint (true);

    // Native bloom truncates divided raster dimensions, with a two-pixel minimum.
    // The main target already includes render scale; don't scale these twice.
    const auto createBloomTarget = [this] (const char* name, uint32_t divisor) {
	const auto size = glm::max (glm::uvec2 (this->getFramebufferSize ()) / divisor, glm::uvec2 (2));
	return this->alias (name, std::make_shared<CFBO> (
	    name, TextureFormat_ARGB8888, TextureFlags_ClampUVs, 1.0f, size.x, size.y, size.x, size.y
	));
    };
    this->_rt_4FrameBuffer = createBloomTarget ("_rt_4FrameBuffer", 4);
    this->_rt_8FrameBuffer = createBloomTarget ("_rt_8FrameBuffer", 8);
    this->_rt_Bloom = createBloomTarget ("_rt_Bloom", 8);

    if (this->m_hdrEnabled) this->m_hdrBloom = std::make_unique<HdrBloom> (*this);
    this->updateBloomState ();
    sLog.out ("Scene post-processing: hdr=", this->m_hdrEnabled ? 1 : 0);

    this->loadingCheckpoint (true);
    this->m_constructing = false;
    constructionGuard.cancel ();
}

CScene::~CScene () {
    this->destroyObjects ();
    if (m_lightCookieSampler != GL_NONE) glDeleteSamplers (1, &m_lightCookieSampler);
}

GLuint CScene::getLightCookieSampler () {
    if (m_lightCookieSampler == GL_NONE) {
	glGenSamplers (1, &m_lightCookieSampler);
	glSamplerParameteri (m_lightCookieSampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glSamplerParameteri (m_lightCookieSampler, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glSamplerParameteri (m_lightCookieSampler, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glSamplerParameteri (m_lightCookieSampler, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    }
    return m_lightCookieSampler;
}

void CScene::loadingCheckpoint (bool canCancel) const {
    if (!this->m_loadingCheckpoint) return;
    if (!canCancel) {
	// CPU submission time alone is not a GPU budget: hundreds of quick draw
	// submissions can queue a long stall ahead of the outgoing wallpaper.
	// Keep just one owned loading unit in flight. Bounded waits still let the
	// application service outgoing frames while that unit reaches the GPU.
	const GLsync fence = glFenceSync (GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
	WallpaperEngine::Data::Utils::ScopeGuard releaseFence ([fence] { glDeleteSync (fence); });
	while (glClientWaitSync (fence, GL_SYNC_FLUSH_COMMANDS_BIT, 1000000) == GL_TIMEOUT_EXPIRED)
	    this->m_loadingCheckpoint (false);
    }
    this->m_loadingCheckpoint (canCancel);
}

double CScene::getRenderTime () const {
    return this->isLoading () ? this->m_loadingRenderTime : this->getContext ().getDriver ().getRenderTime ();
}

std::chrono::steady_clock::time_point CScene::getTimerTime () const {
    return this->isLoading () ? this->m_loadingTimerTime : std::chrono::steady_clock::now ();
}

void CScene::prepareFirstFrame () {
    // Render the normal initial frame at time zero into private targets. CWallpaper
    // will present it once; drawing it again would advance feedback/uncleared layers.
    const glm::ivec4 viewport { 0, 0, this->m_outputSize.x, this->m_outputSize.y };
    this->updateUVs (viewport, this->getContext ().getOutput ().renderVFlip ());
    this->renderFrame (viewport);
    // Include resolves, retained-history copies and post-processing in readiness.
    this->loadingCheckpoint ();
    this->m_firstFramePrepared = true;
    this->loadingCheckpoint (true);
}

void CScene::holdLoading () {
    // Keep loading clocks frozen without retaining the builder's stack references.
    this->m_loadingCheckpoint = [] (bool) {};
    for (const auto& [_, entry] : this->m_videoActivity) {
	if (const auto texture = entry.texture.lock ()) texture->getVideoPlayer ()->setSceneActive (*this, false);
    }
}

void CScene::finishLoading () {
    const double renderOffset = this->getContext ().getDriver ().getRenderTime () - this->m_loadingRenderTime;
    const auto timerOffset = std::chrono::steady_clock::now () - this->m_loadingTimerTime;
    for (const auto& [id, object] : this->m_objects) {
	if (auto* renderable = dynamic_cast<Objects::CRenderable*> (object))
	    renderable->rebaseRenderTime (renderOffset);
    }
    this->m_previousSceneTime = this->getContext ().getDriver ().getRenderTime ();
    this->m_sceneDeltaTime = 0.0f;
    this->m_loadingCheckpoint = {};
    this->updateVideoActivity ();
    this->m_scriptEngine->finishLoading (timerOffset);
    for (const auto& [id, object] : this->m_objects) {
	if (auto* sound = dynamic_cast<Objects::CSound*> (object)) sound->activate ();
    }
}

void CScene::initializeBloom () {
    const uint32_t sceneWidth = this->getWidth ();
    const uint32_t sceneHeight = this->getHeight ();
    //
    // Had to get a little creative with the effects to achieve the same bloom effect without any custom code
    // this custom image loads some effect files from the virtual container to achieve the same bloom effect
    // this approach requires of two extra draw calls due to the way the effect works in official WPE
    // (it renders directly to the screen, whereas here we never do that from a scene)
    //

    const auto bloomOrigin = glm::vec3 { sceneWidth / 2, sceneHeight / 2, 0.0f };
    const auto bloomSize = glm::vec2 { sceneWidth, sceneHeight };
    // g_BloomTint is a vec3 constant, which is authored as a whitespace-separated string
    const glm::vec3 bloomTint = this->getScene ().camera.bloom.tint->value->getVec3 ();
    const JSON bloomConstants
	= { { "constantshadervalues",
	      { { "bloomstrength", this->getScene ().camera.bloom.strength->value->getFloat () },
		{ "bloomthreshold", this->getScene ().camera.bloom.threshold->value->getFloat () },
		{ "bloomtint",
		  std::to_string (bloomTint.x) + " " + std::to_string (bloomTint.y) + " "
		      + std::to_string (bloomTint.z) } } } };

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
		      { "passes", JSON::array ({ bloomConstants, bloomConstants, bloomConstants }) } } }
	      ) } };

    this->m_bloomObjectData = ObjectParser::parse (bloom, this->getScene ().project);
    this->m_bloomObject = this->createObject (*this->m_bloomObjectData);
    if (this->m_bloomObject != nullptr) {
	this->m_objectsByRenderOrder.push_back (this->m_bloomObject);
    }
}

void CScene::updateBloomState () {
    const auto& bloom = this->getScene ().camera.bloom;
    if (this->m_hdrEnabled || this->getContext ().getApp ().getContext ().settings.render.postProcessing
        == Application::ApplicationContext::PostProcessing::Disabled) return;
    if (!bloom.enabled->value->getBool ()) {
	return;
    }

    // A disabled wallpaper should not compile bloom shaders until it needs them.
    // Keep a successfully created chain ready for subsequent disable/enable cycles.
    if (!this->m_bloomSetupAttempted) {
	this->m_bloomSetupAttempted = true;
	try {
	    this->initializeBloom ();
	} catch (const std::exception& error) {
	    sLog.error ("Failed to initialize scene bloom: ", error.what ());
	}
    }
    if (this->m_bloomObject == nullptr) {
	return;
    }

    const double sceneTime = this->getElapsedTime ();
    const float strength = bloom.strength->evaluateFloat (sceneTime);
    const float threshold = bloom.threshold->evaluateFloat (sceneTime);
    const glm::vec3 tint = bloom.tint->evaluateVec3 (sceneTime);
    // CPass retains pointers into these constants. Update their values in place
    // so scripts, user properties, and keyframes reach the uniforms every frame.
    const auto* image = this->m_bloomObjectData->as<Image> ();
    for (const auto& pass : image->effects.front ()->passOverrides) {
	pass->constants.at ("bloomstrength")->value->update (strength, DynamicValue::Initialization);
	pass->constants.at ("bloomthreshold")->value->update (threshold, DynamicValue::Initialization);
	pass->constants.at ("bloomtint")->value->update (tint, DynamicValue::Initialization);
    }
}

void CScene::destroyObjects () noexcept {
    // Also runs during constructor unwinding. Cached textures can outlive this scene.
    for (const auto& [provider, activity] : this->m_videoActivity) {
	if (const auto texture = activity.texture.lock ()) texture->getVideoPlayer ()->removeSceneActivity (*this);
    }
    this->m_videoActivity.clear ();
    // bloom object is in the objects list, so no need to explicitly delete it
    this->m_bloomObject = nullptr;
    this->m_shadowLightOrder.clear ();

    for (const auto& val : this->m_objects | std::views::values) {
	delete val;
    }

    this->m_objectsByRenderOrder.clear ();
    this->m_deferredRenderOrder.clear ();
    this->m_objectsByCursorOrder.clear ();
    this->m_objects.clear ();
    this->m_lightObjects.clear ();
    this->m_scriptedValues.clear ();
}

Render::CObject* CScene::createObject (const Object& object) {
    Render::CObject* renderObject = nullptr;

    // ensure the item is not loaded already
    if (const auto current = this->m_objects.find (object.id); current != this->m_objects.end ()) {
	return current->second;
    }

    if (this->m_objectsInCreation.contains (object.id)) {
	return nullptr;
    }

    this->m_objectsInCreation.insert (object.id);
    WallpaperEngine::Data::Utils::ScopeGuard creationGuard ([this, &object] {
	this->m_objectsInCreation.erase (object.id);
    });

    // check dependencies too!
    // a dependency parented to this very object is the normal shape for faces: the model
    // samples the child image layer's _rt_imageLayerComposite_<id>_a, and the child hangs off
    // the model for its transform. it has to be created first so the render target exists by
    // the time this object's passes resolve their textures. the cycle that creates is broken by
    // m_objectsInCreation above, and a child that resolves its transform before the parent
    // exists simply skips the missing ancestor (CImage::resolveTransform) and corrects itself on
    // the next frame, since render() re-resolves it every time
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

    if (object.is<Image> ()) {
	renderObject = new Objects::CImage (*this, *object.as<Image> ());
    } else if (object.is<Sound> ()) {
	renderObject = new Objects::CSound (*this, *object.as<Sound> ());
    } else if (object.is<Text> ()) {
	renderObject = new Objects::CText (*this, *object.as<Text> ());
    } else if (object.is<Shape> ()) {
	renderObject = new Objects::CShape (*this, *object.as<Shape> ());
    } else if (object.is<Data::Model::Model3D> ()) {
	renderObject = new Objects::CModel (*this, *object.as<Data::Model::Model3D> ());
    } else if (object.is<Data::Model::Light> ()) {
	auto* light = new Objects::CLight (*this, *object.as<Data::Model::Light> ());
	this->m_lightObjects.push_back (light);
	renderObject = light;
    } else if (object.is<Particle> ()) {
	const auto& particleData = *object.as<Particle> ();

	if (this->getContext ().getApp ().getContext ().settings.general.disableParticles == true) {
	    sLog.debug ("Ignoring particle system (disabled in settings): ", particleData.name);
	    return nullptr;
	}

	// the parser leaves the material null when the particle or its material file could
	// not be loaded (it logs why); CParticle dereferences it unconditionally
	if (particleData.material == nullptr || particleData.material->material == nullptr) {
	    sLog.error ("Ignoring particle system without a material: ", particleData.name);
	    return nullptr;
	}

	renderObject = new Objects::CParticle (*this, particleData);
    } else {
	// containers/groups: no rendering of their own, but their transform properties can be
	// script-driven (e.g. orbital mechanics groups), so they must register with the script engine
	renderObject = new Scripting::ScriptableObject (*this, object);
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

void CScene::setScriptCameraTransform (const CameraTransform& transform) {
    static const bool traceScripts = std::getenv ("WPE_SCRIPT_TRACE") != nullptr;
    if (traceScripts) {
	sLog.out (
	    "setScriptCameraTransform eye=", transform.eye.x, ",", transform.eye.y, ",", transform.eye.z
	);
    }
    this->m_scriptCameraTransform = transform;
}

const CScene::SceneFog& CScene::getFog () const { return this->m_fog; }

const glm::vec2& CScene::getOutputSize () const { return this->m_outputSize; }

std::shared_ptr<CFBO> CScene::getMipMappedFramebuffer () {
    if (this->m_mipMappedFramebuffer == nullptr) {
	this->m_mipMappedFramebuffer = this->create (
	    "_rt_MipMappedFrameBuffer", this->getColorFormat (), TextureFlags_ClampUVs, 1.0f,
	    this->m_outputSize, this->m_outputSize
	);
    }
    return this->m_mipMappedFramebuffer;
}

GLuint CScene::getWallpaperFramebuffer () const {
    return this->m_hdrBloom ? this->m_hdrBloom->output ().getFramebuffer () : CWallpaper::getWallpaperFramebuffer ();
}

GLuint CScene::getWallpaperTexture () const {
    return this->m_hdrBloom ? this->m_hdrBloom->output ().getTextureID (0) : CWallpaper::getWallpaperTexture ();
}

glm::ivec2 CScene::getFramebufferSize () const {
    return { this->m_sceneFBO->getRealWidth (), this->m_sceneFBO->getRealHeight () };
}

WallpaperState::TextureUVs CScene::getPresentationUVs () const {
    const auto framing = this->getState ().getTextureUVs ();
    return framing.vstart < framing.vend ? WallpaperState::TextureUVs { 0, 1, 0, 1 }
	: WallpaperState::TextureUVs { 0, 1, 1, 0 };
}

void CScene::updateOutputSize (const glm::ivec4& viewport) {
    glm::vec2 size (glm::max (glm::ivec2 (viewport.z, viewport.w), glm::ivec2 (1)));
    if (this->getSpanInfo () != nullptr) {
	// Span bounds and pointer positions are logical desktop units. Rasterize at
	// the highest participating output density so no monitor enlarges its slice.
	std::vector<std::string> screens;
	for (const auto& [name, wallpaper] : this->getContext ().getWallpapers ()) {
	    if (wallpaper.get () == this) screens.push_back (name);
	}
	if (const auto geometry = this->getContext ().getSpanGeometry (screens)) {
	    size = glm::round (size * geometry->pixelScale);
	}
    }
    if (size != this->m_outputSize) {
	this->m_outputSize = size;
	const auto sceneSize = glm::max (calculateTargetSize (size, this->getRenderScale ()),
            glm::uvec2 (this->m_hdrEnabled ? 2 : 1));
	this->find ("_rt_FullFrameBuffer")->resize (sceneSize.x, sceneSize.y);
	for (const auto& [name, divisor] : std::array<std::pair<const char*, uint32_t>, 3> {
	    { { "_rt_4FrameBuffer", 4 }, { "_rt_8FrameBuffer", 8 }, { "_rt_Bloom", 8 } } }) {
	    const auto target = glm::max (sceneSize / divisor, glm::uvec2 (2));
	    this->find (name)->resize (target.x, target.y);
	}
	if (this->m_mipMappedFramebuffer != nullptr) {
	    this->m_mipMappedFramebuffer->resize (sceneSize.x, sceneSize.y);
	}
	if (!this->m_camera->isOrthogonal ()) {
	    this->m_camera->setPerspectiveProjection (size.x, size.y, this->m_camera->isYFlipped ());
	    const auto uv = this->getPresentationUVs ();
	    this->updateUVs (viewport, uv.vstart < uv.vend);
	}
    }
    const auto framing = this->getState ().getTextureUVs ();
    this->m_camera->setViewportScale ({ 1.0f / (framing.uend - framing.ustart),
	1.0f / std::abs (framing.vend - framing.vstart) });
}

void CScene::renderFrame (const glm::ivec4& viewport) {
    this->destroyQueuedLayers ();
    ++this->m_sceneFrame;
    this->updateVideoActivity ();
    this->updateOutputSize (viewport);
    // Frame callbacks are per-output on Wayland. A 60 Hz scene can therefore render only once
    // while a 165 Hz output advances the application loop several times. Deriving dt only from
    // g_TimeLast loses those skipped intervals and stretches camera shots on the slower output.
    // Subtract double driver samples before narrowing; process-global g_Time is
    // already a float and quantizes frame intervals after long process uptimes.
    const double currentTime = this->isLoading ()
        ? this->m_loadingRenderTime : this->getContext ().getDriver ().getRenderTime ();
    this->m_previousSceneDeltaTime = this->m_sceneDeltaTime;
    this->m_sceneDeltaTime = calculateSceneDeltaTime (currentTime, this->m_previousSceneTime);
    this->m_previousSceneTime = currentTime;
    this->m_sceneElapsedTime += this->m_sceneDeltaTime;
    // Native scene time resets after its float projection exceeds five days.
    // Local animation anchors keep the stable elapsed clock above.
    this->m_sceneTime += this->m_sceneDeltaTime;
    if (static_cast<float> (this->m_sceneTime) > 432000.0f) this->m_sceneTime = 0.0;

    // Camera layers are ordinary scriptable objects. Resolve their parent chain
    // before paths and mouse projection so a scripted camera rig supplies the
    // resting eye/orientation rather than the editor's top-level viewport.
    this->updateCameraObject ();

    // Advance visible paths before scripts read cursorWorldPosition and before
    // any object renders. Script-driven camera visibility from the previous tick
    // selects the source, matching the normal one-frame property update cadence.
    this->updateCameraPath (glm::max (0.0f, this->getDeltaTime ()));

    // ensure the virtual mouse position is up to date
    this->updateMouse (viewport);

    // update the parallax position if required
    if (this->getScene ().camera.parallax.enabled->value->getBool ()
	&& !this->getContext ().getApp ().getContext ().settings.mouse.disableparallax) {
	const float influence = this->getScene ().camera.parallax.mouseInfluence->value->getFloat ();
	const float delay = this->getScene ().camera.parallax.delay->value->getFloat ();
	const float alpha = calculateParallaxSmoothingAlpha (delay, this->getDeltaTime ());

	// per-object depth and the global parallax amount are applied by each renderable,
	// this only tracks the smoothed mouse offset (-1 to 1 across the screen) scaled by
	// influence; only y is negated because m_mousePosition already has y flipped
	// relative to x by the viewport UV mapping in updateMouse
	const glm::vec2 centeredMouse = {
	    this->m_mousePosition.x * 2.0f - 1.0f,
	    1.0f - this->m_mousePosition.y * 2.0f,
	};
	this->m_parallaxDisplacement = glm::mix (this->m_parallaxDisplacement, centeredMouse * influence, alpha);
	// Shader-driven parallax effects consume the influenced mouse position directly.
	// cameraparallaxamount belongs only to whole-layer camera translation; multiplying the
	// shader input by it suppresses depth-map-only scenes whose camera amount is zero.
	this->m_parallaxPosition = calculateShaderParallaxPosition (this->m_parallaxDisplacement);
    }

    // run a tick in the javascript logic
    this->getScriptEngine ().tick ();
    this->updateBloomState ();

    // Apply camera-rig movement from this tick to the frame being rendered. A
    // non-empty path still owns the final transform, but uses this pose as its
    // live fallback for any channels it does not animate.
    const bool cameraObjectActive = this->updateCameraObject ();
    const bool pathOwnsCamera
	= this->m_activeCameraPathSource != nullptr && this->m_activeCameraPathIndex.has_value ();
    if (pathOwnsCamera) {
	const CameraPath& path = this->m_activeCameraPathSource->paths[*this->m_activeCameraPathIndex];
	this->m_camera->setTransform (
	    path.evaluate (this->m_cameraPathElapsed, this->m_camera->getDefaultTransform ())
	);
    }
    // Native gives visible camera layers priority even when their path list is
    // empty. Keep the script pose as a persistent fallback for free-camera mode.
    if (this->m_scriptCameraTransform.has_value () && !cameraObjectActive && !pathOwnsCamera) {
	CameraTransform transform = *this->m_scriptCameraTransform;
	// CameraTransforms owns eye/center/up/zoom; FOV remains a scene or camera-layer property.
	transform.fov = this->m_camera->getDefaultTransform ().fov;
	this->m_camera->setTransform (transform);
    }


    // Fog colors and ranges can be SceneScript- or user-property-driven.
    this->updateFogState ();

    // Refresh all initialized materials before drawing, so hidden consumers and
    // shared user images observe the same selection. Retire old images only
    // after every affected pass has rebound; sampling stays first-load-wins.
    std::set<std::string> fallbackNames;
    for (const auto* pass : m_userTexturePasses) pass->collectUserTextureFallbacks (fallbackNames);
    // Hold prepared providers through all rebindings, even if cache pressure trims other entries.
    const auto preparedFallbacks = getContext ().prepareTextureFallbacks (
	fallbackNames, getAssetLocator (), getScene ().project.properties, m_textureReduction
    );
    bool changedUserTextures = false;
    // Resolving a newly selected layer target can create/register its material.
    for (size_t index = 0; index < m_userTexturePasses.size (); ++index) {
	changedUserTextures |= m_userTexturePasses[index]->updateUserTextures ();
    }
    if (changedUserTextures) {
	std::erase_if (m_userTextures, [] (const auto& entry) { return entry.second.use_count () == 1; });
    }
    // Shared GIFs advance once per scene update, including hidden consumers;
    // multiple material passes and texture slots all sample the same playhead.
    for (const auto& texture : m_userTextures | std::views::values) {
	texture->updateUserAnimation (getElapsedTime (), m_sceneFrame);
    }

    // Material init hooks can append or reorder layers; destruction waits for the next frame.
    // Prepare this frame's existing models once. Init-created models join the next preparation.
    const auto modelsToPrepare = this->m_objectsByRenderOrder;
    for (const auto& object : modelsToPrepare) {
	if (auto* model = dynamic_cast<Objects::CModel*> (object)) model->prepareRender ();
    }

    // Render light-space depth before any material pass samples _rt_shadowAtlas.
    this->updateLightState ();
    this->renderShadowAtlas ();

    // update every cached texture instead of walking image objects: textures that
    // are only referenced as effect/pass inputs have no CImage driving them, so
    // video-backed effect inputs never decoded a single frame (upstream c3f526f)
#if !NDEBUG
    glPushDebugGroup (GL_DEBUG_SOURCE_APPLICATION, 0, -1, "Updating textures");
#endif

    this->getContext ().updateAllTextures ();

#if !NDEBUG
    glPopDebugGroup ();
#endif

    // bind the vertex array
    glBindVertexArray (this->m_vaoBuffer);
    // Keep ordinary scene geometry multisampled, including after intermediate
    // scene-color reads. Post-processing uses the resolved single-sample target.
    const auto sceneTarget = this->find ("_rt_FullFrameBuffer");
    sceneTarget->setMultisampleRendering (true);
    glEnable (GL_MULTISAMPLE);
    glBindFramebuffer (GL_FRAMEBUFFER, sceneTarget->getDrawFramebuffer ());
    // ensure we render over the whole framebuffer
    glViewport (0, 0, this->m_sceneFBO->getRealWidth (), this->m_sceneFBO->getRealHeight ());

    // passes with depthwrite disabled leave the mask off, which would block the depth clear
    glDepthMask (true);
    // the clear must write all channels: a leaked alpha-disabled color mask would silently keep the
    // framebuffer's stale alpha, which every alpha-blended effect writeback then composites against
    glColorMask (true, true, true, true);
    // Scripts and other scenes can change this context state between frames.
    const glm::vec3 clearColor = this->getScene ().colors.clear->evaluateVec3 (this->getElapsedTime ());
    glClearColor (clearColor.r, clearColor.g, clearColor.b, 1.0f);
    const GLbitfield colorClear = this->getScene ().clearEnabled->value->getBool () ? GL_COLOR_BUFFER_BIT : 0;
    glClearDepth (m_sceneFBO->getFarDepth ());
    glClear (colorClear | GL_DEPTH_BUFFER_BIT);

    // Discover newly enabled shader inputs before an earlier hidden source is
    // considered for culling. Retention is conservative for the scene lifetime:
    // disabled consumers may become visible again without a stale first frame.
    for (const auto& [id, object] : this->m_objects) {
	if (auto* image = dynamic_cast<Objects::CImage*> (object)) {
	    image->prepareRenderEffects ();
	}
	if (auto* text = dynamic_cast<Objects::CText*> (object)) {
	    text->prepareRenderEffects ();
	}
	for (const int dependency : object->getObject ().dependencies) {
	    m_sampledRenderTargets.insert ("_rt_imageLayerComposite_" + std::to_string (dependency) + "_a");
	    m_sampledRenderTargets.insert ("_rt_imageLayerComposite_" + std::to_string (dependency) + "_b");
	}
    }

    const std::vector<FrameRenderEntry> renderOrder = this->buildFrameRenderOrder ();
    const auto& debug = this->getContext ().getApp ().getContext ().settings.render.debug;
    const auto enabledByDebug = [this, &debug] (const CObject* object) {
	if (object == this->m_bloomObject && !this->getScene ().camera.bloom.enabled->value->getBool ()) {
	    return false;
	}
	if (debug.objectFilter.has_value () && object->getId () != debug.objectFilter.value ()) {
	    return false;
	}
	return std::ranges::find (debug.skipObjects, object->getId ()) == debug.skipObjects.end ();
    };
    const auto renderEntry = [&enabledByDebug] (const FrameRenderEntry& entry) {
	if (!enabledByDebug (entry.object)) {
	    return;
	}
	entry.object->render ();
    };

    // Wallpaper Engine renders a composition layer's child subtree into the
    // layer surface first. Reconstruct only that relationship around the otherwise
    // flat render order so ordinary layer/dependency ordering remains unchanged.
    const auto compositionAncestor = [this] (const CObject* object) -> Objects::CImage* {
	const Object* current = &object->getObject ();
	for (int depth = 0; current->parent.has_value () && depth < 32; depth++) {
	    const auto parentIt = this->m_objects.find (*current->parent);
	    if (parentIt == this->m_objects.end ()) {
		break;
	    }
	    CObject* parent = parentIt->second;
	    auto* image = dynamic_cast<Objects::CImage*> (parent);
	    if (image != nullptr && image->isCompositionLayer ()) {
		return image;
	    }
	    current = &parent->getObject ();
	}
	return nullptr;
    };

    std::set<int> submittedCompositions;
    std::function<void (Objects::CImage*)> renderComposition;
    renderComposition = [&] (Objects::CImage* composition) {
	if (composition == nullptr || submittedCompositions.contains (composition->getId ())) {
	    return;
	}
	submittedCompositions.insert (composition->getId ());

	const auto target = composition->getCompositionFBO ();
	if (target == nullptr) {
	    if (enabledByDebug (composition)) {
		composition->render ();
	    }
	    return;
	}

	const auto previousTarget = this->m_compositionRenderTarget;
	const bool previousAlphaMode = this->m_compositionAlphaMax;
	// Native copybackground samples the scene, including for nested groups.
	const auto source = this->getFBO ();
	GLfloat previousClearColor[4];
	glGetFloatv (GL_COLOR_CLEAR_VALUE, previousClearColor);
	glBindFramebuffer (GL_FRAMEBUFFER, target->getFramebuffer ());
	glViewport (0, 0, target->getRealWidth (), target->getRealHeight ());
	glColorMask (true, true, true, true);
	glDepthMask (true);
	glClearColor (0.0f, 0.0f, 0.0f, 0.0f);
	glClearDepth (target->getFarDepth ());
	glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glClearColor (
	    previousClearColor[0], previousClearColor[1], previousClearColor[2], previousClearColor[3]
	);

	if (composition->copiesCompositionBackground () && source != nullptr && source != target) {
	    glBindFramebuffer (GL_READ_FRAMEBUFFER, source->getFramebuffer ());
	    glBindFramebuffer (GL_DRAW_FRAMEBUFFER, target->getFramebuffer ());
	    glBlitFramebuffer (
		0, 0, source->getRealWidth (), source->getRealHeight (), 0, 0, target->getRealWidth (),
		target->getRealHeight (), GL_COLOR_BUFFER_BIT, GL_LINEAR
	    );
	}

	this->m_compositionRenderTarget = target;
	this->m_compositionAlphaMax = previousAlphaMode || !composition->copiesCompositionBackground ();
	for (const FrameRenderEntry& entry : renderOrder) {
	    CObject* child = entry.object;
	    if (compositionAncestor (child) != composition) {
		continue;
	    }
	    if (auto* nested = dynamic_cast<Objects::CImage*> (child);
		nested != nullptr && nested->isCompositionLayer ()) {
		renderComposition (nested);
	    } else {
		renderEntry (entry);
	    }
	}
	this->m_compositionRenderTarget = previousTarget;
	this->m_compositionAlphaMax = previousAlphaMode;

	// The stock composelayer material samples the locally shadowed
	// _rt_FullFrameBuffer and composites the effected group into the prior target.
	if (enabledByDebug (composition)) {
	    composition->render ();
	}
    };

    std::vector<Objects::CLight*> volumeBatch;
    const auto renderVolumes = [&] {
        if (volumeBatch.empty ()) return;
        if (!this->m_volumetricLights) this->m_volumetricLights = std::make_unique<VolumetricLights> (*this);
        this->m_volumetricLights->render (this->m_lightObjects, volumeBatch);
        volumeBatch.clear ();
    };
    for (const FrameRenderEntry& entry : renderOrder) {
	CObject* cur = entry.object;
	if (cur == this->m_bloomObject || compositionAncestor (cur) != nullptr) {
	    continue;
	}
	if (auto* light = dynamic_cast<Objects::CLight*> (cur); light != nullptr) {
	    if (!this->m_camera->isOrthogonal () && enabledByDebug (light)
	        && (light->getLight ().type == LightData::Type_Spot
	            || light->getLight ().type == LightData::Type_Point)
	        && light->getLight ().castVolumetrics->value->getBool ()) {
	        volumeBatch.push_back (light);
	    }
	    continue;
	}
	// Native 14018aac0/140198d00 flushes at admitted non-light objects,
	// including generic barriers and hidden retained image/text sources.
	if (enabledByDebug (cur) && cur->isVolumetricBarrier ()) {
	    renderVolumes ();
	}
	if (auto* composition = dynamic_cast<Objects::CImage*> (cur);
	    composition != nullptr && composition->isCompositionLayer ()) {
	    renderComposition (composition);
	    continue;
	}
	renderEntry (entry);
    }
    renderVolumes ();

    sceneTarget->setMultisampleRendering (false);

    // Native materials sample the previous completed scene. Refresh a separate
    // texture after ordinary draws and before bloom, avoiding framebuffer feedback
    // and keeping bloom out of the reflection used by the following frame.
    if (this->m_mipMappedFramebuffer != nullptr) {
	const auto& target = this->m_mipMappedFramebuffer;
	const GLuint sourceFramebuffer = this->m_sceneFBO->getFramebuffer ();
	const GLuint targetFramebuffer = target->getFramebuffer ();
	GLint previousRead = GL_NONE, previousDraw = GL_NONE;
	glGetIntegerv (GL_READ_FRAMEBUFFER_BINDING, &previousRead);
	glGetIntegerv (GL_DRAW_FRAMEBUFFER_BINDING, &previousDraw);
	const GLboolean scissor = glIsEnabled (GL_SCISSOR_TEST);
	glDisable (GL_SCISSOR_TEST);
	glBindFramebuffer (GL_READ_FRAMEBUFFER, sourceFramebuffer);
	glBindFramebuffer (GL_DRAW_FRAMEBUFFER, targetFramebuffer);
	glBlitFramebuffer (
	    0, 0, this->m_sceneFBO->getRealWidth (), this->m_sceneFBO->getRealHeight (),
	    0, 0, target->getRealWidth (), target->getRealHeight (), GL_COLOR_BUFFER_BIT, GL_NEAREST
	);
	target->generateMipmaps ();
	glBindFramebuffer (GL_READ_FRAMEBUFFER, previousRead);
	glBindFramebuffer (GL_DRAW_FRAMEBUFFER, previousDraw);
	if (scissor) glEnable (GL_SCISSOR_TEST);
    }
    if (this->m_hdrBloom != nullptr) {
        this->m_hdrBloom->render (!renderOrder.empty ()
            && this->getScene ().camera.bloom.enabled->value->getBool ()
            && (!debug.objectFilter.has_value () || debug.objectFilter.value () == -1)
            && std::ranges::find (debug.skipObjects, -1) == debug.skipObjects.end ());
    } else if (this->m_bloomObject != nullptr && enabledByDebug (this->m_bloomObject)) {
	this->m_bloomObject->render ();
    }
}

float CScene::calculateParallaxSmoothingAlpha (const float delay, const float deltaTime) {
    if (delay <= 0.0f) {
	return 1.0f;
    }

    return glm::clamp ((1.0f - delay / PARALLAX_DELAY_LIMIT) * PARALLAX_DELAY_RATE * deltaTime, 0.0f, 1.0f);
}

glm::vec2 CScene::calculateShaderParallaxPosition (const glm::vec2& displacement) {
    // displacement uses [-1,1], whereas g_ParallaxPosition uses [0,1].
    return glm::vec2 (0.5f) + displacement * 0.5f;
}

glm::vec4
CScene::calculateFogParams (const float start, const float end, const float startDensity, const float endDensity) {
    return glm::vec4 (start, end - start, startDensity, endDensity - startDensity);
}

void CScene::registerFogScripts () {
    const auto queue = [this] (const std::string& key, const UserSettingUniquePtr& setting) {
	this->m_scriptEngine->queueSceneScript (key, *setting);
    };
    const auto queueLayer = [&queue] (const std::string& prefix, const SceneData::Fog::Layer& layer) {
	queue (prefix, layer.enabled);
	queue (prefix + "color", layer.color);
	queue (prefix + "start", layer.start);
	queue (prefix + "end", layer.end);
	queue (prefix + "startdensity", layer.startDensity);
	queue (prefix + "enddensity", layer.endDensity);
    };

    queueLayer ("fogdistance", this->getScene ().fog.distance);
    queueLayer ("fogheight", this->getScene ().fog.height);

    // Any general property can carry a script, and scenes use that as a place to
    // park scene-wide logic regardless of what the property itself does - Passing
    // Breeze 2244339517 drives its whole orbiting camera from general.bloom.
    // Queueing only the fog block left those scripts never running.
    const auto& scene = this->getScene ();
    queue ("ambientcolor", scene.colors.ambient);
    queue ("skylightcolor", scene.colors.skylight);
    queue ("clearcolor", scene.colors.clear);
    queue ("clearenabled", scene.clearEnabled);
    queue ("gravitydirection", scene.physics.gravityDirection);
    queue ("gravitystrength", scene.physics.gravityStrength);
    queue ("windenabled", scene.physics.windEnabled);
    queue ("winddirection", scene.physics.windDirection);
    queue ("windstrength", scene.physics.windStrength);
    queue ("camerafade", scene.camera.fade);
    queue ("bloom", scene.camera.bloom.enabled);
    queue ("bloomstrength", scene.camera.bloom.strength);
    queue ("bloomthreshold", scene.camera.bloom.threshold);
    queue ("bloomtint", scene.camera.bloom.tint);
    queue ("bloomhdrstrength", scene.camera.bloom.hdr.strength);
    queue ("bloomhdrthreshold", scene.camera.bloom.hdr.threshold);
    queue ("bloomhdrfeather", scene.camera.bloom.hdr.feather);
    queue ("bloomhdrscatter", scene.camera.bloom.hdr.scatter);
    queue ("bloomhdriterations", scene.camera.bloom.hdr.iterations);
    queue ("cameraparallax", scene.camera.parallax.enabled);
    queue ("cameraparallaxamount", scene.camera.parallax.amount);
    queue ("cameraparallaxdelay", scene.camera.parallax.delay);
    queue ("cameraparallaxmouseinfluence", scene.camera.parallax.mouseInfluence);
    queue ("camerashake", scene.camera.shake.enabled);
    queue ("camerashakeamplitude", scene.camera.shake.amplitude);
    queue ("camerashakeroughness", scene.camera.shake.roughness);
    queue ("camerashakespeed", scene.camera.shake.speed);
    queue ("fov", scene.camera.projection.fov);
    queue ("perspectiveoverridefov", scene.camera.projection.perspectiveOverrideFov);
    queue ("zoom", scene.camera.projection.zoom);
    queue ("nearz", scene.camera.projection.nearz);
    queue ("farz", scene.camera.projection.farz);
}

void CScene::updateFogState () {
    const auto& fog = this->getScene ().fog;
    const bool perspective = this->getScene ().camera.projection.isPerspective;
    this->m_fog.distanceEnabled = perspective && fog.distance.enabled->value->getBool ();
    this->m_fog.heightEnabled = perspective && fog.height.enabled->value->getBool ();
    this->m_fog.distanceParams = calculateFogParams (
	fog.distance.start->value->getFloat (), fog.distance.end->value->getFloat (),
	fog.distance.startDensity->value->getFloat (), fog.distance.endDensity->value->getFloat ()
    );
    this->m_fog.heightParams = calculateFogParams (
	fog.height.start->value->getFloat (), fog.height.end->value->getFloat (),
	fog.height.startDensity->value->getFloat (), fog.height.endDensity->value->getFloat ()
    );
}

float CScene::calculateCameraFadeAlpha (const float elapsedTime, const float duration) {
    if (duration <= 0.0f) {
	return 0.0f;
    }

    const float remaining = glm::max (duration - elapsedTime, 0.0f);
    // Reference order matters for shots shorter than one second: the end
    // envelope wins when it overlaps the opening envelope.
    if (remaining < CAMERA_FADE_DURATION) {
	return glm::clamp (1.0f - remaining / CAMERA_FADE_DURATION, 0.0f, 1.0f);
    }
    if (elapsedTime < CAMERA_FADE_DURATION) {
	return glm::clamp (1.0f - elapsedTime / CAMERA_FADE_DURATION, 0.0f, 1.0f);
    }
    return 0.0f;
}

float CScene::calculateSceneDeltaTime (
    const double currentTime, const std::optional<double> previousSceneTime
) {
    // Loading and shader compilation happen before this scene can present its first frame.
    // Charging that time to a new scene skips one-shot entrances on cold starts and switches.
    return previousSceneTime.has_value ()
        ? static_cast<float> (glm::max (0.0, currentTime - *previousSceneTime)) : 0.0f;
}

float CScene::getSceneFadeAlpha () const {
    if (!this->getScene ().camera.fade->value->getBool () || this->m_activeCameraPathSource == nullptr
	|| !this->m_activeCameraPathIndex.has_value ()) {
	return 0.0f;
    }

    const CameraPath& path = this->m_activeCameraPathSource->paths[*this->m_activeCameraPathIndex];
    return calculateCameraFadeAlpha (this->m_cameraPathElapsed, path.duration);
}

size_t CScene::chooseCameraPathIndex (
    const std::optional<size_t> current, const size_t count, const std::string& queueMode,
    std::vector<size_t>& remaining, const std::function<uint32_t ()>& nextRandom
) {
    if (count == 0) {
	return 0;
    }
    if (queueMode != "random") {
	return current.has_value () ? (*current + 1) % count : 0;
    }
    if (remaining.empty ()) {
	remaining.resize (count);
	std::iota (remaining.begin (), remaining.end (), 0);
    }

    // Native uses multiply-high selection with rejection, rather than remainder.
    const uint32_t range = static_cast<uint32_t> (remaining.size ());
    const uint32_t threshold = (0U - range) % range;
    uint64_t product;
    do {
	product = static_cast<uint64_t> (nextRandom ()) * range;
    } while (static_cast<uint32_t> (product) < threshold);
    size_t slot = product >> 32;

    // At a refill boundary native skips a repeated shot by advancing the slot;
    // it neither removes the previous shot from the pool nor draws again.
    if (remaining.size () > 1 && current.has_value () && remaining[slot] == *current) {
	slot = (slot + 1) % remaining.size ();
    }
    const size_t selected = remaining[slot];
    remaining.erase (remaining.begin () + static_cast<std::ptrdiff_t> (slot));
    return selected;
}

std::vector<size_t>
CScene::calculateTransparentSortPermutation (const std::vector<TransparentSortKey>& keys) {
    std::vector<size_t> permutation;
    permutation.reserve (keys.size ());

    std::vector<size_t> sortedEntries;
    for (size_t index = 0; index < keys.size (); index++) {
	if (keys[index].sortable) {
	    sortedEntries.push_back (index);
	} else {
	    permutation.push_back (index);
	}
    }

    std::stable_sort (sortedEntries.begin (), sortedEntries.end (), [&keys] (const size_t left, const size_t right) {
	const TransparentSortKey& lhs = keys[left];
	const TransparentSortKey& rhs = keys[right];
	if (lhs.sortLast != rhs.sortLast) {
	    return rhs.sortLast;
	}
	if (!lhs.sortLast) {
	    const float leftDepth = std::isfinite (lhs.cameraDepth) ? lhs.cameraDepth : 0.0f;
	    const float rightDepth = std::isfinite (rhs.cameraDepth) ? rhs.cameraDepth : 0.0f;
	    if (leftDepth != rightDepth) {
		return leftDepth < rightDepth;
	    }
	}
	return lhs.previousOrder < rhs.previousOrder;
    });

    permutation.insert (permutation.end (), sortedEntries.begin (), sortedEntries.end ());
    return permutation;
}

std::vector<CScene::FrameRenderEntry> CScene::buildFrameRenderOrder () {
    const bool automaticSorting = this->getScene ().camera.projection.isPerspective
	&& this->getScene ().transparentSorting && !this->getScene ().customSortOrder;
    std::vector<FrameRenderEntry> entries;
    entries.reserve (this->m_objectsByRenderOrder.size ());
    for (CObject* object : this->m_objectsByRenderOrder) {
	entries.push_back ({ .object = object });
    }
    if (!automaticSorting) {
	return entries;
    }

    std::vector<TransparentSortKey> keys;
    keys.reserve (entries.size ());
    const glm::mat4& view = this->m_camera->getLookAt ();
    const glm::vec3 cameraBackward (view[0][2], view[1][2], view[2][2]);
    for (const FrameRenderEntry& entry : entries) {
	CObject* const object = entry.object;
	// Native 14018aac0 first traverses objects without flag 0x100, then
	// depth-sorts the deferred list. A model with any opaque/coverage pass
	// remains one complete first-phase draw, including its blended passes.
	bool deferred = object->getObject ().is<Particle> ()
	    || dynamic_cast<const Objects::CLight*> (object) != nullptr
	    || dynamic_cast<const Objects::CText*> (object) != nullptr;
	bool fullscreen = false;
	if (const auto* model = dynamic_cast<const Objects::CModel*> (object)) {
	    const auto& classes = model->getRenderSortClasses ();
	    deferred = std::ranges::find (classes, RenderSortClass::Opaque) == classes.end ();
	} else if (const auto* image = dynamic_cast<const Objects::CImage*> (object)) {
	    const auto& data = image->getImage ();
	    const auto& passes = data.model->material->passes;
	    deferred = (data.model->passthrough && !data.copyBackground)
		|| (!passes.empty () && passes.front ()->blending != BlendingMode_Normal
		    && passes.front ()->blending != BlendingMode_AlphaToCoverage);
	    fullscreen = data.model->fullscreen;
	}
	float cameraDepth = 0.0f;
	if (deferred && !fullscreen) {
	    // Native 1401865c0 projects local origins onto the view axis without
	    // camera translation, which can round distinct depths into a tie.
	    const glm::vec3 origin = object->getObject ().origin->evaluateVec3 (this->getElapsedTime ());
	    cameraDepth = glm::dot (origin, cameraBackward);
	}
	const auto previous = this->m_deferredRenderOrder.find (object);
	keys.push_back (TransparentSortKey {
	    .sortable = deferred,
	    .cameraDepth = cameraDepth,
	    .sortLast = fullscreen,
	    .previousOrder = previous == this->m_deferredRenderOrder.end ()
		? std::numeric_limits<size_t>::max () : previous->second,
	});
    }

    const std::vector<size_t> permutation = calculateTransparentSortPermutation (keys);
    std::vector<FrameRenderEntry> result = entries;
    this->m_deferredRenderOrder.clear ();
    for (size_t index = 0; index < permutation.size (); index++) {
	result[index] = entries[permutation[index]];
	if (keys[permutation[index]].sortable) {
	    this->m_deferredRenderOrder.emplace (result[index].object, index);
	}
    }
    return result;
}

const CameraPathSource* CScene::findActiveCameraPathSource () const {
    const CameraPathSource* active = nullptr;
    for (const CameraPathSource& source : this->getScene ().camera.paths) {
	if (!source.objectId.has_value ()) {
	    active = &source;
	}
    }

    for (const int id : this->getScene ().camera.objectIds) {
	const CObject* object = this->getObject (id);
	if (object == nullptr || !object->getObject ().groupVisible->value->getBool ()
	    || !object->isVisibleThroughParents ()) {
	    continue;
	}
	// A selected camera owns the view even when its path is empty or absent.
	// Its static pose must not inherit an older camera's or the scene's path.
	const auto source = std::ranges::find_if (
	    this->getScene ().camera.paths,
	    [id] (const CameraPathSource& candidate) { return candidate.objectId == id; }
	);
	active = source != this->getScene ().camera.paths.end () ? &*source : nullptr;
    }
    return active;
}

bool CScene::updateCameraObject () {
    const CObject* active = nullptr;
    const SceneData::Camera::ObjectProjection* activeProjection = nullptr;
    for (const int id : this->getScene ().camera.objectIds) {
	const CObject* object = this->getObject (id);
	if (object == nullptr || !object->getObject ().groupVisible->value->getBool ()
	    || !object->isVisibleThroughParents ()) {
	    continue;
	}
	// Later visible camera layers override earlier/default cameras, matching
	// the selection rule already used by camera path sources.
	active = object;
	const auto projection = std::ranges::find_if (
	    this->getScene ().camera.objectProjections,
	    [id] (const SceneData::Camera::ObjectProjection& candidate) { return candidate.id == id; }
	);
	activeProjection
	    = projection != this->getScene ().camera.objectProjections.end () ? &*projection : nullptr;
    }

    if (active == nullptr) {
	// Scenes without a camera layer still have live projection properties. Refresh
	// the fallback without claiming a runtime pose for the editor's 2D camera.
	CameraTransform transform = this->m_camera->getDefaultTransform ();
	if (!this->m_camera->isOrthogonal ()) {
	    // Native restores the scene pose when the last camera layer is hidden.
	    // The previous visible layer may have replaced the resting transform.
	    const auto& configuration = this->getScene ().camera.configuration;
	    transform.eye = configuration.eye;
	    transform.center = configuration.center;
	    transform.up = configuration.up;
	}
	const auto& projection = this->getScene ().camera.projection;
	transform.fov = this->m_camera->getDefaultFov (this->getElapsedTime ());
	transform.zoom = projection.zoom->evaluateFloat (this->getElapsedTime ());
	this->m_camera->setDefaultTransform (transform, false);
	const bool pathActive = this->m_activeCameraPathSource != nullptr
	    && this->m_activeCameraPathIndex.has_value ();
	if (!pathActive && this->m_scriptCameraTransform.has_value () && projection.zoom->animation != nullptr) {
	    // Refresh the animated projection before scripts read the camera. A
	    // controller that copies eye/center must not pin the zoom to the value
	    // returned on its first tick (the 2887099508 entrance combines both).
	    this->m_scriptCameraTransform->zoom = transform.zoom;
	    this->m_camera->setTransform (*this->m_scriptCameraTransform);
	}
	if (!pathActive && !this->m_scriptCameraTransform.has_value ()) {
	    this->m_camera->resetTransform ();
	}
	return false;
    }

    const double sceneTime = this->getElapsedTime ();
    const auto& fallback = this->getScene ().camera.projection;
    const float fov = activeProjection != nullptr && activeProjection->fov != nullptr
	? activeProjection->fov->evaluateFloat (sceneTime)
	: this->m_camera->getDefaultFov (sceneTime);
    const float zoom = activeProjection != nullptr && activeProjection->zoom != nullptr
	? activeProjection->zoom->evaluateFloat (sceneTime)
	: fallback.zoom->evaluateFloat (sceneTime);
    const CameraTransform transform = Camera::objectTransform (
	active->resolveWorldMatrix (), fov, zoom
    );
    const bool pathActive = this->m_activeCameraPathSource != nullptr && this->m_activeCameraPathIndex.has_value ();
    // A selected layer supplies the pose even if a hidden free-camera script
    // keeps submitting transforms. Its path, when present, is evaluated next.
    this->m_camera->setDefaultTransform (transform, !pathActive);
    return true;
}

// Playback modes, straight off the reference runtime's advance step. A path
// only ever hands the queue on when it is a single-shot one: looping and
// mirrored paths keep the same shot forever.
CScene::CameraPathPlayback
CScene::advanceCameraPath (const CameraPath& path, const CameraPathPlayback current, const float deltaTime) {
    const uint32_t flags = path.flags | current.state;
    // startpaused parks a shot on its first frame; nothing resumes it yet.
    if ((flags & (CameraPathStartPaused | CameraPathFinished)) != 0) {
	return current;
    }
    if ((flags & CameraPathSingle) != 0 && path.duration <= current.elapsed) {
	return current;
    }
    if (path.duration <= 0.0f) {
	return current;
    }

    const float elapsed = current.elapsed + ((flags & CameraPathReversed) != 0 ? -deltaTime : deltaTime);

    if ((flags & CameraPathSingle) != 0) {
	if (elapsed < path.duration) {
	    return { elapsed, current.state };
	}
	// Only a single-shot path ever reports back; loops and mirrors keep going.
	return { path.duration, current.state | CameraPathFinished };
    }

    if ((flags & CameraPathMirror) == 0) {
	float wrapped = elapsed < 0.0f ? std::fmod (elapsed + path.duration, path.duration) : elapsed;
	if (wrapped >= path.duration) {
	    wrapped = std::fmod (wrapped, path.duration);
	}
	return { wrapped, current.state };
    }

    if ((flags & CameraPathReversed) == 0) {
	if (elapsed < path.duration) {
	    return { elapsed, current.state };
	}
	return { path.duration - std::fmod (elapsed, path.duration), current.state | CameraPathReversed };
    }

    if (elapsed > 0.0f) {
	return { elapsed, current.state };
    }
    return { std::fabs (std::fmod (elapsed, path.duration)), current.state & ~CameraPathReversed };
}

void CScene::updateCameraPath (const float deltaTime) {
    const CameraPathSource* source = this->findActiveCameraPathSource ();
    if (this->m_activeCameraPathSource != nullptr) {
	this->m_cameraPathSourceStates[this->m_activeCameraPathSource] = {
	    this->m_activeCameraPathIndex, std::move (this->m_remainingCameraPaths),
	    { this->m_cameraPathElapsed, this->m_cameraPathState }
	};
    }

    // Every visible camera samples its queue before the final visible camera
    // supplies the view. Legacy scene paths advance only as the selected fallback.
    for (const CameraPathSource& candidate : this->getScene ().camera.paths) {
	if (candidate.paths.empty ()) continue;
	if (candidate.objectId.has_value ()) {
	    const CObject* object = this->getObject (*candidate.objectId);
	    if (object == nullptr || !object->getObject ().groupVisible->value->getBool ()
		|| !object->isVisibleThroughParents ()) {
		continue;
	    }
	} else if (&candidate != source) {
	    continue;
	}

	auto& state = this->m_cameraPathSourceStates[&candidate];
	if (!state.index.has_value ()) {
	    state.index = chooseCameraPathIndex (
		std::nullopt, candidate.paths.size (), candidate.queueMode, state.remaining,
		[this] { return this->m_cameraPathRandom (); }
	    );
	}
	state.playback = advanceCameraPath (candidate.paths[*state.index], state.playback, deltaTime);
	if ((state.playback.state & CameraPathFinished) != 0) {
	    state.playback = { 0.0f, 0 };
	    state.index = chooseCameraPathIndex (
		state.index, candidate.paths.size (), candidate.queueMode, state.remaining,
		[this] { return this->m_cameraPathRandom (); }
	    );
	}
    }

    if (source != this->m_activeCameraPathSource) {
	this->m_activeCameraPathSource = source;
	this->m_activeCameraPathIndex = std::nullopt;
	this->m_remainingCameraPaths.clear ();
	this->m_cameraPathElapsed = 0.0f;
	this->m_cameraPathState = 0;
	if (source == nullptr || source->paths.empty ()) {
	    this->m_camera->resetTransform ();
	    return;
	}
    }

    if (source == nullptr || source->paths.empty ()) return;
    auto& state = this->m_cameraPathSourceStates.at (source);
    this->m_activeCameraPathIndex = state.index;
    this->m_remainingCameraPaths = std::move (state.remaining);
    this->m_cameraPathElapsed = state.playback.elapsed;
    this->m_cameraPathState = state.playback.state;
    this->m_cameraPathSourceStates.erase (source);

    const CameraPath& path = source->paths[*this->m_activeCameraPathIndex];
    const CameraTransform transform = path.evaluate (this->m_cameraPathElapsed, this->m_camera->getDefaultTransform ());

    // Opt-in trace (WPE_CAMERA_TRACE=1) for comparing shot pacing against the
    // reference: authored handles must show up here as a non-constant rate.
    static const bool trace = std::getenv ("WPE_CAMERA_TRACE") != nullptr;
    if (trace) {
	sLog.out (
	    "camera path '", path.name, "' t=", this->m_cameraPathElapsed, "/", path.duration,
	    " flags=", path.flags | this->m_cameraPathState, " eye=", transform.eye.x, ",", transform.eye.y, ",",
	    transform.eye.z, " fov=", transform.fov
	);
    }

    this->m_camera->setTransform (transform);
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

    // This is the authored canvas rectangle visible through the framed camera.
    // Presentation uses full UVs because the framing is already rasterized.
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

const CScene::SceneLights& CScene::getLights () const { return this->m_lights; }

void CScene::applyLightCombos (ComboMap& combos) const {
    const auto& lights = this->getLights ();

    if (lights.directionalCount > 0) {
	combos.insert_or_assign ("LIGHTS_DIRECTIONAL", lights.directionalCount);
    }

    if (lights.directionalShadowCount > 0) {
	combos.insert_or_assign ("LIGHTS_DIRECTIONAL_SHADOW", lights.directionalShadowCount);
	combos.insert_or_assign (
	    "LIGHTS_DIRECTIONAL_SHADOW_MASK", static_cast<int> (lights.directionalShadowMask)
	);
    }

    if (lights.pointCount > 0) {
	combos.insert_or_assign ("LIGHTS_POINT", lights.pointCount);
    }

    if (lights.pointShadowCount > 0) {
	combos.insert_or_assign ("LIGHTS_POINT_SHADOW", lights.pointShadowCount);
	combos.insert_or_assign ("LIGHTS_POINT_SHADOW_MASK", static_cast<int> (lights.pointShadowMask));
    }

    if (lights.spotCount > 0) {
	combos.insert_or_assign ("LIGHTS_SPOT", lights.spotCount);
    }
    if (lights.spotCookieMask != 0) {
	combos.insert_or_assign ("LIGHTS_COOKIE", 1);
	combos.insert_or_assign ("LIGHTS_SPOT_COOKIE_MASK", static_cast<int> (lights.spotCookieMask));
    }

    if (lights.spotShadowCount > 0) {
	combos.insert_or_assign ("LIGHTS_SPOT_SHADOW", lights.spotShadowCount);
	combos.insert_or_assign ("LIGHTS_SPOT_SHADOW_MASK", static_cast<int> (lights.spotShadowMask));
    }

    if (lights.shadowViewCount > 0) {
	combos.insert_or_assign ("LIGHTS_SHADOW_FEATURES", lights.shadowFeatureCount);
	combos.insert_or_assign ("LIGHTS_SHADOW_MAPPING", 1);
        combos.insert_or_assign ("REVERSEDEPTH", this->getCamera ().usesReverseDepth () ? 1 : 0);
	combos.insert_or_assign ("LIGHTS_SHADOW_MAPPING_QUALITY", 2);
    }

    if (lights.tubeCount > 0) {
	combos.insert_or_assign ("LIGHTS_TUBE", lights.tubeCount);
    }
}

void CScene::updateShadowAtlasLayout () {
    auto& lights = this->m_lights;
    if (lights.shadowViewCount == 0) return;

    std::fill (lights.shadowTransforms.begin (), lights.shadowTransforms.end (), glm::vec4 (0));
    std::fill (lights.pointShadowTransforms.begin (), lights.pointShadowTransforms.end (), glm::vec4 (0));
    const int rows = this->m_shadowAtlasMaxExtent / SHADOW_FEATURE_SIZE;
    const int columns = std::min (rows, SHADOW_ATLAS_MAX_WIDTH / SHADOW_FEATURE_SIZE);
    if (columns <= 0) return;
    const size_t capacity = static_cast<size_t> (columns) * rows;

    const auto& view = this->getCamera ().getLookAt ();
    const auto forward = -glm::vec3 (view[0][2], view[1][2], view[2][2]);
    const double time = this->getElapsedTime ();
    const auto depth = [forward, time] (const Light& light) {
	const float value = glm::dot (light.origin->evaluateVec3 (time), forward);
	return std::isnan (value) ? 0.0f : value;
    };

    // Native sorts its persistent light list before visibility filtering.
    std::stable_sort (this->m_shadowLightOrder.begin (), this->m_shadowLightOrder.end (),
	[&depth] (const auto* a, const auto* b) {
	    const auto& first = a->getLight ();
	    const auto& second = b->getLight ();
	    if (first.type != second.type) return first.type < second.type;
	    if (first.useCookie != second.useCookie) return first.useCookie;
	    return depth (first) < depth (second);
	});

    struct Region {
	int point;
	int feature;
    };
    std::vector<Region> regions;
    for (const auto* light : this->m_shadowLightOrder) {
	if (regions.size () == capacity) break;
	const auto& data = light->getLight ();
	if (!data.groupVisible->value->getBool () || !light->isVisibleThroughParents ()) continue;
	const int slot = this->m_authoredLightSlots.at (light->getId ());
	if (data.type == LightData::Type_Point) {
	    if (slot < 32 && (lights.pointShadowMask & (uint32_t (1) << slot)))
		regions.push_back ({ slot, -1 });
	} else if (data.type == LightData::Type_Spot) {
	    const int feature = lights.spotShadowFeatures[slot];
	    if (feature >= 0) regions.push_back ({ -1, feature });
	} else if (data.type == LightData::Type_Directional && capacity - regions.size () >= 3) {
	    const auto features = lights.directionalShadowFeatures[slot];
	    if (features.x >= 0) {
		for (int cascade = 0; cascade < 3; cascade++)
		    regions.push_back ({ -1, features[cascade] });
	    }
	}
    }

    // Medium quality packs one square per point, spot or cascade in rows.
    // Each point's six faces share the same square.
    const int count = static_cast<int> (regions.size ());
    const int width = std::max (2, std::min (count, columns) * SHADOW_FEATURE_SIZE);
    const int height = std::max (2, ((count + columns - 1) / columns) * SHADOW_FEATURE_SIZE);
    if (width > static_cast<int> (this->_rt_shadowAtlas->getRealWidth ())
	|| height > static_cast<int> (this->_rt_shadowAtlas->getRealHeight ())) {
	// Resize the existing texture so material aliases and uniform providers stay valid.
	this->find ("_rt_shadowAtlas")->resize (width, height);
    }
    const glm::vec4 atlasSize (this->_rt_shadowAtlas->getRealWidth (), this->_rt_shadowAtlas->getRealHeight (),
	this->_rt_shadowAtlas->getRealWidth (), this->_rt_shadowAtlas->getRealHeight ());
    for (int index = 0; index < count; index++) {
	const auto& region = regions[index];
	const glm::ivec4 viewport ((index % columns) * SHADOW_FEATURE_SIZE, (index / columns) * SHADOW_FEATURE_SIZE,
	    SHADOW_FEATURE_SIZE, SHADOW_FEATURE_SIZE);
	const auto transform = glm::vec4 (viewport) / atlasSize;
	if (region.point >= 0) {
	    lights.pointShadowTransforms[region.point] = transform;
	    for (int face = 0; face < 6; face++) {
		lights.pointShadowViewports[region.point][face] = glm::ivec4 (
		    viewport.x + (face % 2) * (SHADOW_FEATURE_SIZE / 2),
		    viewport.y + (face / 2) * (SHADOW_FEATURE_SIZE / 3), SHADOW_FEATURE_SIZE / 2, SHADOW_FEATURE_SIZE / 3
		);
	    }
	} else {
	    lights.shadowTransforms[region.feature] = transform;
	    lights.shadowViewports[region.feature] = viewport;
	}
    }
}

void CScene::updateLightState () {
    this->updateShadowAtlasLayout ();
    const auto& camera = this->getCamera ();
    const bool reverseDepth = camera.usesReverseDepth ();
    auto& lights = this->m_lights;
    // Shader masks and uniform storage retain their authored slots after deletion.
    const glm::vec4 dark (0.0f, 0.0f, 0.0f, 1.0f);
    std::fill (lights.directionalColors.begin (), lights.directionalColors.end (), dark);
    std::fill (lights.pointColors.begin (), lights.pointColors.end (), dark);
    std::fill (lights.spotColors.begin (), lights.spotColors.end (), dark);
    std::fill (lights.tubeColors.begin (), lights.tubeColors.end (), dark);
    std::fill (lights.directionalShadowEnabled.begin (), lights.directionalShadowEnabled.end (), 0.0f);
    std::fill (lights.pointShadowEnabled.begin (), lights.pointShadowEnabled.end (), 0.0f);
    std::fill (lights.spotShadowEnabled.begin (), lights.spotShadowEnabled.end (), 0.0f);
    std::fill (lights.shadowFeatureEnabled.begin (), lights.shadowFeatureEnabled.end (), 0.0f);
    m_lightCookie.reset ();

    for (const auto* light : this->m_lightObjects) {
	const auto& data = light->getLight ();
	const auto authored = this->m_authoredLightSlots.find (light->getId ());
	if (authored == this->m_authoredLightSlots.end ()) continue;
	const int slot = authored->second;

	if (data.type == LightData::Type_Directional) {
	    // The shader expects the direction towards the light.
	    lights.directionalDirections[slot] = glm::vec4 (-light->getWorldDirection (), 0.0f);
	    // The legacy versioned shaders multiply radiance by this neutral radius squared.
	    lights.directionalColors[slot] = glm::vec4 (light->getPremultipliedColor (), 1.0f);
	    const glm::ivec3 features = lights.directionalShadowFeatures[slot];
	    if (features.x >= 0 && lights.shadowTransforms[features.x].z > 0.0f) {
		const float cameraNear = std::max (camera.getNearZ (), 0.0001f);
		const float cameraFar = std::max (camera.getFarZ (), cameraNear + 0.0001f);
		glm::vec3 cascadeFar = glm::abs (data.cascadeDistances);
		cascadeFar.x = glm::clamp (cascadeFar.x, cameraNear + 0.0001f, cameraFar);
		cascadeFar.y = glm::clamp (cascadeFar.y, cascadeFar.x, cameraFar);
		cascadeFar.z = glm::clamp (cascadeFar.z, cascadeFar.y, cameraFar);

		for (int cascade = 0; cascade < 3; cascade++) {
		    const int feature = features[cascade];
		    if (reverseDepth) {
			lights.shadowMatrices[feature] = light->getDirectionalShadowProjection (
			    camera.getEye (), camera.getLookAt (), cascade, SHADOW_FEATURE_SIZE
			);
		    } else {
			// Orthographic zoom does not apply to perspective frustum fitting.
			lights.shadowMatrices[feature] = Objects::CLight::calculateDirectionalShadowViewProjection (
			    camera.getEye (), camera.getCenter (), camera.getUp (), camera.getFov (),
			    camera.getWidth () / std::max (camera.getHeight (), 1.0f), 1.0f, cameraNear,
			    cascadeFar[cascade], light->getWorldDirection (), SHADOW_FEATURE_SIZE
			);
		    }
		    lights.shadowFeatureEnabled[feature] = 1.0f;
		}
		lights.directionalShadowEnabled[slot] = 1.0f;
	    }
	} else if (data.type == LightData::Type_Point) {
	    const auto origin = light->getWorldPosition ();
	    const float radius = data.radius->value->getFloat ();
	    lights.pointOrigins[slot] = glm::vec4 (origin, data.exponent->value->getFloat ());
	    lights.pointColors[slot] = glm::vec4 (light->getPremultipliedColor (), radius);
	    if (lights.pointShadowTransforms[slot].z > 0.0f) {
		const float sourceSize = data.sourceSize->evaluateFloat (this->getElapsedTime ());
		const bool orthographic = camera.isOrthogonal ();
		lights.pointShadowProjections[slot]
		    = Objects::CLight::calculatePointShadowProjectionInfo (radius, sourceSize, orthographic, reverseDepth);
		lights.pointShadowMatrices[slot]
		    = Objects::CLight::calculatePointShadowViewProjections (origin, radius, sourceSize, orthographic, reverseDepth);
		lights.pointShadowEnabled[slot] = 1.0f;
	    }
	} else if (data.type == LightData::Type_Spot) {
	    const glm::vec2 cones = Objects::CLight::calculateSpotConeCosines (
		data.innerCone->value->getFloat (), data.outerCone->value->getFloat ()
	    );
	    lights.spotOrigins[slot] = glm::vec4 (light->getWorldPosition (), cones.x);
	    lights.spotDirections[slot] = glm::vec4 (light->getWorldForwardAxis (), cones.y);
	    lights.spotColors[slot] = glm::vec4 (light->getPremultipliedColor (), data.radius->value->getFloat ());
	    lights.spotExponents[slot] = glm::vec4 (data.exponent->value->getFloat (), 0.0f, 0.0f, 0.0f);
	    if (slot < 32 && (lights.spotCookieMask & (uint32_t (1) << slot))) {
		lights.spotCookieMatrices[slot] = light->getCookieProjection ();
		if (light->isVisibleThroughParents () && data.groupVisible->value->getBool ()) {
		    m_lightCookie = light->getCookie ();
		}
	    }

	    const int feature = lights.spotShadowFeatures[slot];
	    if (feature >= 0 && lights.shadowTransforms[feature].z > 0.0f) {
		lights.shadowMatrices[feature] = (reverseDepth || data.useCookie) ? light->getCookieProjection (reverseDepth)
		    : Objects::CLight::calculateSpotShadowViewProjection (
			light->getWorldPosition (), light->getWorldDirection (), data.outerCone->value->getFloat (),
			data.radius->value->getFloat ()
		    );
		lights.spotShadowEnabled[slot] = 1.0f;
		lights.shadowFeatureEnabled[feature] = 1.0f;
	    }
	} else if (data.type == LightData::Type_Tube) {
	    lights.tubeOriginsA[slot] = glm::vec4 (light->getWorldPosition (), data.exponent->value->getFloat ());
	    lights.tubeOriginsB[slot] = glm::vec4 (light->getTubeEndPosition (), 0.0f);
	    lights.tubeColors[slot] = glm::vec4 (light->getPremultipliedColor (), data.radius->value->getFloat ());
	}
    }
    lights.updateLegacyPointLights ();
}

void CScene::SceneLights::updateLegacyPointLights () {
    this->legacyPositions.fill (glm::vec3 (0.0f));
    // Legacy shaders divide by radius even for unused lights.
    this->legacyColors.fill (glm::vec4 (0.0f, 0.0f, 0.0f, 1.0f));
    this->legacyPremultipliedColors.fill (glm::vec4 (0.0f));
    for (int index = 0; index < std::min (this->pointCount, 4); ++index) {
	this->legacyPositions[index] = glm::vec3 (this->pointOrigins[index]);
	this->legacyColors[index] = this->pointColors[index];
	const auto& color = this->pointColors[index];
	const glm::vec3 radiance = glm::vec3 (color) * color.w * color.w;
	if (index < 3) {
	    this->legacyPremultipliedColors[index] = glm::vec4 (radiance, 0.0f);
	} else {
	    for (int channel = 0; channel < 3; ++channel) {
		this->legacyPremultipliedColors[channel].w = radiance[channel];
	    }
	}
    }
}

void CScene::renderShadowAtlas () {
    if (this->m_lights.shadowViewCount == 0 || this->_rt_shadowAtlas == nullptr) {
	return;
    }

#if !NDEBUG
    glPushDebugGroup (GL_DEBUG_SOURCE_APPLICATION, 0, -1, "Scene shadow atlas");
#endif

    const bool reverseDepth = this->_rt_shadowAtlas->usesReverseDepth ();
    const ClipDepthScope clipDepth (reverseDepth);
    glBindFramebuffer (GL_FRAMEBUFFER, this->_rt_shadowAtlas->getFramebuffer ());
    // The full retained atlas must clear even when the caller uses a scissor.
    const GLboolean scissor = glIsEnabled (GL_SCISSOR_TEST);
    glDisable (GL_SCISSOR_TEST);
    glColorMask (false, false, false, false);
    glDisable (GL_BLEND);
    glEnable (GL_DEPTH_TEST);
    glDepthFunc (reverseDepth ? GL_GREATER : GL_LESS);
    glDepthMask (true);
    glClearDepth (this->_rt_shadowAtlas->getFarDepth ());
    glClear (GL_DEPTH_BUFFER_BIT);
    glEnable (GL_POLYGON_OFFSET_FILL);
    glPolygonOffset (reverseDepth ? -4.0f : 2.0f, reverseDepth ? 0.0f : 4.0f);
    // Keep native atlas coordinates across cells, including out-of-tile samples.
    glm::mat4 shadowClip (1.0f);
    shadowClip[1][1] = -1.0f;

    for (int feature = 0; feature < this->m_lights.shadowFeatureCount; feature++) {
	if (this->m_lights.shadowFeatureEnabled[feature] < 0.5f) {
	    continue;
	}

	const glm::ivec4& viewport = this->m_lights.shadowViewports[feature];
	glViewport (viewport.x, viewport.y, viewport.z, viewport.w);
        // Native biases only the caster matrix; receivers use the original VP.
        glm::mat4 casterProjection = shadowClip * this->m_lights.shadowMatrices[feature];
        if (reverseDepth) casterProjection[3][2] -= 0.0005f;

	for (auto* object : this->m_objectsByRenderOrder) {
	    if (auto* model = dynamic_cast<Objects::CModel*> (object); model != nullptr) {
		model->renderShadow (casterProjection);
		this->loadingCheckpoint ();
	    }
	}
    }

    for (int point = 0; point < this->m_lights.pointCount; point++) {
	if (this->m_lights.pointShadowEnabled[point] < 0.5f) {
	    continue;
	}

	for (int face = 0; face < 6; face++) {
	    const glm::ivec4& viewport = this->m_lights.pointShadowViewports[point][face];
	    glViewport (viewport.x, viewport.y, viewport.z, viewport.w);
	    const glm::mat4 casterProjection = shadowClip * this->m_lights.pointShadowMatrices[point][face];
	    for (auto* object : this->m_objectsByRenderOrder) {
		if (auto* model = dynamic_cast<Objects::CModel*> (object); model != nullptr) {
		    model->renderShadow (casterProjection);
		    this->loadingCheckpoint ();
		}
	    }
	}
    }

    glDisable (GL_POLYGON_OFFSET_FILL);
    glDisable (GL_CULL_FACE);
    glFrontFace (GL_CCW);
    glColorMask (true, true, true, true);
    glDepthMask (true);
    glUseProgram (GL_NONE);
    if (scissor) glEnable (GL_SCISSOR_TEST);

#if !NDEBUG
    glPopDebugGroup ();
#endif
}

const Scene& CScene::getScene () const { return *this->getWallpaperData ().as<Scene> (); }

int CScene::getWidth () const { return this->m_camera->getWidth (); }

int CScene::getHeight () const { return this->m_camera->getHeight (); }

std::optional<glm::vec2> CScene::prepareStaticAutoCollisionBounds (const Scene& scene) {
    if (!scene.camera.projection.isAuto || scene.camera.projection.isPerspective || scene.hasPropertyScripts)
        return std::nullopt;
    const bool hasBounds = std::ranges::any_of (scene.objects, [] (const auto& object) {
        if (!object->template is<Particle> ()) return false;
        const auto* particle = object->template as<Particle> ();
        return std::ranges::any_of (particle->operators, [] (const auto& op) {
            return op->template is<CollisionBoundsOperator> ();
        });
    });
    if (!hasBounds) return std::nullopt;
    const auto materialScripts = [] (const Material* material) {
        return material && std::ranges::any_of (material->passes, [] (const auto& pass) {
            return std::ranges::any_of (pass->constants, [] (const auto& constant) {
                return constant.second->value->getScriptSource ().has_value ();
            });
        });
    };
    const Image* firstImage = nullptr;
    for (const auto& object : scene.objects) {
        if (object->is<Image> ()) {
            const auto* image = object->as<Image> ();
            if (!image->effects.empty () || materialScripts (image->model->material.get ())) return std::nullopt;
            if (!firstImage) firstImage = image;
        } else if (object->is<Particle> ()) {
            const auto* particle = object->as<Particle> ();
            if (!particle->children.empty () || materialScripts (particle->material ? particle->material->material.get () : nullptr))
                return std::nullopt;
            if (!particle->particleFile.empty ()) {
                const auto definition = WallpaperEngine::Data::JSON::parseCompatible (
                    scene.project.assetLocator->readString (particle->particleFile), particle->particleFile
                );
                if (definition.dump ().find ("\"script\":") != std::string::npos) return std::nullopt;
            }
        } else if (object->is<Model3D> () || (object->is<Text> () && !object->as<Text> ()->effects.empty ())) {
            return std::nullopt;
        }
    }
    // Native initializes the collision context to zero and holds it when no image exists.
    if (!firstImage) return glm::vec2 (0.0f);
    const auto& image = *firstImage;
    if (image.model->fullscreen || image.model->autosize || image.model->passthrough || image.attachment
        || image.origin->property || image.origin->animation || image.origin->value->getScriptSource ()) return std::nullopt;
    const auto configuration = JSON::parse (image.initialConfiguration);
    const auto size = configuration.find ("size");
    if (size == configuration.end () || (!size->is_string () && !size->is_array ())) return std::nullopt;
    const float integerLimit = static_cast<float> (std::numeric_limits<int>::max ());
    if (!std::isfinite (image.size.x) || !std::isfinite (image.size.y) || image.size.x <= 0.0f || image.size.y <= 0.0f
        || image.size.x >= integerLimit || image.size.y >= integerLimit) return std::nullopt;
    // Recenter LOCAL origin before any descendants resolve through the normal parent chain.
    image.origin->value->update (glm::vec3 (image.size * .5f, 0.0f), DynamicValue::UpdateSource::Initialization);
    return image.size;
}

float CScene::getTime () const { return static_cast<float> (this->m_sceneTime); }

float CScene::getDeltaTime () const { return this->m_sceneDeltaTime; }

float CScene::getFps () const {
    const float dt = this->getDeltaTime ();
    // Guard against duplicate renders at one timestamp and division by zero on the first call.
    if (dt <= 1e-6f) {
	return 60.0f;
    }
    return 1.0f / dt;
}

const glm::vec2* CScene::getMousePosition () const { return &this->m_mousePosition; }

const glm::vec2* CScene::getMousePositionLast () const { return &this->m_mousePositionLast; }

const glm::vec2* CScene::getMousePositionNormalized () const { return &this->m_mousePositionNormalized; }

const glm::vec2* CScene::getParallaxDisplacement () const { return &this->m_parallaxDisplacement; }

const glm::vec2* CScene::getParallaxPosition () const { return &this->m_parallaxPosition; }

const std::vector<CObject*>& CScene::getObjectsByRenderOrder () const { return this->m_objectsByRenderOrder; }
const std::vector<CObject*>& CScene::getObjectsByCursorOrder () const { return this->m_objectsByCursorOrder; }

const CObject* CScene::getObject (int id) const {
    const auto object = this->m_objects.find (id);
    return object == this->m_objects.end () ? nullptr : object->second;
}

bool CScene::hasAuthoredChildren (const SceneData& scene, const int parentId) {
    return std::ranges::any_of (scene.objects, [parentId] (const auto& object) {
	return object != nullptr && object->parent.has_value () && *object->parent == parentId;
    });
}

bool CScene::hasAuthoredChildren (const int parentId) const {
    return hasAuthoredChildren (this->getScene (), parentId);
}

std::shared_ptr<const CFBO> CScene::getActiveRenderTarget () const {
    return this->m_compositionRenderTarget != nullptr ? this->m_compositionRenderTarget : this->getFBO ();
}

std::shared_ptr<const CFBO>
CScene::resolveRenderTarget (const std::shared_ptr<const CFBO>& requested) const {
    if (requested == this->getFBO () && this->m_compositionRenderTarget != nullptr) {
	return this->m_compositionRenderTarget;
    }
    return requested;
}

bool CScene::isRenderingToComposition () const { return this->m_compositionRenderTarget != nullptr; }

bool CScene::usesCompositionAlphaMax () const { return this->m_compositionAlphaMax; }

Render::CObject* CScene::createLayer (const std::string& modelPath, const std::string& workshopId) {
    // Resolve the asset path: scripts reference bar models by their bare workshop-relative path
    // (e.g. "models/full-pixel.json"), but the packaged asset lives under the script's workshop id
    // ("models/workshop/<id>/full-pixel.json"). Try the bare path first, then the workshop-scoped one.
    std::string path = modelPath;
    const auto resolves = [this] (const std::string& candidate) {
	try {
	    this->getAssetLocator ().readString (candidate);
	    return true;
	} catch (const std::exception&) {
	    return false;
	}
    };

    if (!resolves (path) && !workshopId.empty ()) {
	if (const auto slash = path.find ('/'); slash != std::string::npos) {
	    std::string scoped
		= path.substr (0, slash + 1) + "workshop/" + workshopId + "/" + path.substr (slash + 1);
	    if (resolves (scoped)) {
		path = std::move (scoped);
	    }
	}
    }

    JSON layer;
    const std::string extension = std::filesystem::path (path).extension ().string ();
    if (extension == ".mdl") {
	layer["model"] = path;
    } else if (extension == ".ogg" || extension == ".wav" || extension == ".mp3" || extension == ".flac") {
	layer["sound"] = JSON::array ({ path });
    } else {
	try {
	    const auto asset = Data::JSON::parseCompatible (this->getAssetLocator ().readString (path), path);
	    layer[asset.contains ("emitter") || asset.contains ("initializer") ? "particle" : "image"] = path;
	} catch (const std::exception& e) {
	    sLog.error ("createLayer failed for ", modelPath, ": ", e.what ());
	    return nullptr;
	}
    }
    return this->createLayer (std::move (layer), workshopId);
}

const char* CScene::validateModelDataMaterial (const Scripting::ModelDataShape& shape, bool allowUnknownSignature) const {
    try {
        const auto material = MaterialParser::load (getScene ().project, shape.material, true);
        if (material->passes.empty ()) return "Material or shader missing";
        const auto& pass = *material->passes.front ();
        const auto& assets = getAssetLocator ();
        // Keep compilation failure ahead of signature mismatch, as in native.
        // No pass, script owner, texture provider or live program is created here.
        const ComboMap overrides;
        const TextureMap textureOverrides;
        Shaders::Shader shader (assets, pass.shader, pass.combos, overrides, pass.textures, textureOverrides, pass.constants);
        const auto& vertex = shader.vertex ();
        const auto selected = Shaders::GLSLContext::get ().preprocess (Shaders::GLSLContext::UnitType_Vertex, vertex);
        const auto semantics = Objects::modelInputSemantics (selected);
        const auto compiled = Shaders::GLSLContext::get ().toGlsl (vertex, shader.fragment ());
        if (compiled.first.empty () || compiled.second.empty ()) return "Material or shader missing";
        if (!semantics) return allowUnknownSignature ? nullptr : "Model shader input declaration requires wider native validation";
        if ((*semantics & shape.vertexFormat) != *semantics) return "Incompatible vertex format";
        return nullptr;
    } catch (const std::exception&) {
        return "Material or shader missing";
    }
}

uint32_t CScene::createModelData (
    std::vector<Scripting::ModelDataShape> shapes, std::optional<std::pair<glm::vec3, glm::vec3>> bounds
) {
    // Native visits every shape, preserves the last failed material/signature
    // check, and publishes no token on failure. Validate before transient uploads.
    const char* validationError = nullptr;
    for (const auto& shape : shapes) {
        if (const char* error = validateModelDataMaterial (shape, true)) validationError = error;
    }
    if (validationError) throw std::runtime_error (validationError);
    auto resource = std::make_unique<Objects::ModelData> (std::move (shapes));
    resource->validateMaterial = [this] (const Scripting::ModelDataShape& shape) {
        return validateModelDataMaterial (shape);
    };
    if (bounds) {
	resource->mesh.boundingBoxMin = bounds->first;
	resource->mesh.boundingBoxMax = bounds->second;
	resource->mesh.hasBoundingBox = true;
    }
    resource->upload ();
    return m_modelData.insert (std::move (resource));
}

void CScene::destroyModelData (uint32_t token) { m_modelData.release (token); }
std::shared_ptr<Objects::ModelData> CScene::findModelData (uint32_t token) { return m_modelData.find (token); }

Render::CObject* CScene::createLayer (JSON layer, const std::string& workshopId) {
    if (!layer.is_object ()) return nullptr;
    // Allocate a fresh id above everything currently known (live objects + parse-time objects), so it
    // never collides with an existing key in m_objects or a not-yet-instantiated dependency.
    int newId = 0;
    for (const auto& id : this->m_objects | std::views::keys) {
	newId = std::max (newId, id);
    }
    for (const auto& object : this->getScene ().objects) {
	newId = std::max (newId, object->id);
    }
    for (const auto& object : m_runtimeLayerData) newId = std::max (newId, object->id);
    ++newId;

    const glm::vec3 origin = { this->m_camera->getWidth () / 2.0f, this->m_camera->getHeight () / 2.0f, 0.0f };

    layer["id"] = newId;
    if (!layer.contains ("name")) layer["name"] = "runtime-layer-" + std::to_string (newId);
    if (!layer.contains ("origin")) {
	layer["origin"] = this->m_camera->isOrthogonal ()
	    ? std::to_string (origin.x) + " " + std::to_string (origin.y) + " 0" : "0 0 0";
    }

    try {
	ObjectUniquePtr data;
	const auto model = layer.find ("model");
	if (model != layer.end () && model->is_number ()) {
	    const auto token = model->get<uint32_t> ();
	    const auto resource = findModelData (token);
	    if (!resource || resource->shapes.empty ()) {
		return nullptr;
	    }
	    const auto& project = getScene ().project;
	    Model3DData modelData;
	    modelData.modelDataToken = token;
	    modelData.mesh = resource->mesh;
	    for (const auto& shape : resource->shapes) {
		modelData.materials.push_back (MaterialParser::load (project, shape.input.material, true));
	    }
	    modelData.castShadow = layer.user ("castshadow", project.properties, true);
	    modelData.perspective = layer.user ("perspective", project.properties, false);
	    data = std::make_unique<Model3D> (ObjectParser::parseBase (layer, project), std::move (modelData));
	} else {
	    data = ObjectParser::parse (layer, this->getScene ().project);
	}
	Object* raw = data.get ();
	this->m_runtimeLayerData.push_back (std::move (data));

	Render::CObject* renderObject = this->createObject (*raw);
	if (renderObject == nullptr) {
	    return nullptr;
	}

	if (auto* light = dynamic_cast<Objects::CLight*> (renderObject)) light->initializeCookie ();
	this->m_objectsByRenderOrder.push_back (renderObject);
	this->m_objectsByCursorOrder.push_back (renderObject);
	if (auto* scriptable = dynamic_cast<Scripting::ScriptableObject*> (renderObject))
	    this->m_scriptEngine->initializeQueuedScripts (scriptable);
	return renderObject;
    } catch (const std::exception& e) {
	sLog.error ("createLayer failed: ", e.what ());
	return nullptr;
    }
}

bool CScene::destroyLayer (CObject* layer) {
    if (layer == nullptr || getObject (layer->getId ()) != layer) return false;
    return m_layersToDestroy.insert (layer->getId ()).second;
}

void CScene::destroyQueuedLayers () {
    for (const int id : m_layersToDestroy) {
	const auto it = m_objects.find (id);
	if (it == m_objects.end ()) continue;
	auto* object = it->second;
	std::erase (m_objectsByRenderOrder, object);
	m_deferredRenderOrder.erase (object);
	std::erase (m_objectsByCursorOrder, object);
	std::erase_if (m_lightObjects, [object] (const auto* light) { return light == object; });
	std::erase_if (m_shadowLightOrder, [object] (const auto* light) { return light == object; });
	m_objects.erase (it);
	delete object;
	std::erase_if (m_runtimeLayerData, [id] (const auto& data) { return data->id == id; });
    }
    m_layersToDestroy.clear ();
}

int CScene::getScriptableLayerIndex (const CObject* layer) const {
    int index = 0;
    for (const auto* object : this->m_objectsByCursorOrder) {
	if (object == nullptr || !object->is<Scripting::ScriptableObject> ()) {
	    continue;
	}
	if (object == layer) {
	    return index;
	}
	++index;
    }
    return -1;
}

void CScene::moveLayerToScriptableIndex (CObject* layer, int index) {
    // Authored and dependency render indices can differ. A script-facing no-op
    // must not reschedule a dependency after its consumer.
    if (index == this->getScriptableLayerIndex (layer)) return;
    for (auto* layers : { &m_objectsByRenderOrder, &m_objectsByCursorOrder }) {
	auto& order = *layers;
	const auto current = std::ranges::find (order, layer);
	if (current == order.end ()) {
	    continue;
	}
	order.erase (current);

	// Insert just before the index-th scriptable layer; a negative / past-the-end index appends,
	// leaving the layer on top of the render order.
	auto insertPos = order.end ();
	if (index >= 0) {
	    int scriptIndex = 0;
	    for (auto it = order.begin (); it != order.end (); ++it) {
		if (*it == nullptr || !(*it)->is<Scripting::ScriptableObject> ()) {
		    continue;
		}
		if (scriptIndex == index) {
		    insertPos = it;
		    break;
		}
		++scriptIndex;
	    }
	}
	order.insert (insertPos, layer);
    }
}

std::shared_ptr<const TextureProvider> CScene::resolveTexture (const std::string& name) const {
    auto provider = this->getContext ().resolveTexture (name, this->getAssetLocator (), this->getScene ().project.properties, this->m_textureReduction);
    if (auto texture = std::dynamic_pointer_cast<const CTexture> (provider); texture && texture->getVideoPlayer ()) {
	auto [entry, inserted] = this->m_videoActivity.try_emplace (provider.get (), VideoActivity { texture });
	if (inserted || entry->second.texture.expired ()) {
	    entry->second = { texture };
	    // Native initializes last use to frame zero, even for a texture created later.
	    texture->getVideoPlayer ()->setSceneActive (*this, this->m_sceneFrame <= 30);
	}
    }
    return provider;
}

void CScene::markTextureUsed (const TextureProvider& provider) const {
    if (const auto entry = this->m_videoActivity.find (&provider); entry != this->m_videoActivity.end ()) {
	if (const auto texture = entry->second.texture.lock ()) {
	    entry->second.lastUsedFrame = this->m_sceneFrame;
	    texture->getVideoPlayer ()->setSceneActive (*this, true);
	}
    }
}

void CScene::updateVideoActivity () {
    std::erase_if (this->m_videoActivity, [this] (const auto& entry) {
	const auto texture = entry.second.texture.lock ();
	if (!texture) return true;
	texture->getVideoPlayer ()->setSceneActive (*this, this->m_sceneFrame - entry.second.lastUsedFrame <= 30);
	return false;
    });
}

void CScene::registerUserTexturePass (Objects::Effects::CPass* pass) { m_userTexturePasses.push_back (pass); }
void CScene::unregisterUserTexturePass (Objects::Effects::CPass* pass) { std::erase (m_userTexturePasses, pass); }

std::shared_ptr<const TextureProvider> CScene::resolveUserTexture (const std::string& filename, uint32_t flags, float aspect) const {
    const std::filesystem::path path (filename);
    if (!path.is_absolute ()) return resolveTexture (filename);
    // Native shares normalized user paths at the same reference aspect. The
    // first load fixes sampling for every use of that image, independent of the
    // property name. Keep these separate from ordinary compiled-texture entries.
    const std::string key = path.lexically_normal ().string ()
	+ "\x1fuser:" + std::to_string (std::lround (aspect * 10.0f));
    if (const auto found = m_userTextures.find (key); found != m_userTextures.end ()) {
	return found->second;
    }
    std::ifstream input (path, std::ios::binary);
    if (!input) throw std::runtime_error ("Cannot open user image " + filename);
    const std::string contents (std::istreambuf_iterator<char> (input), {});
    auto parsed = TextureParser::parseImage (contents, Data::JSON::JSON {
	{"nomip", true},
	{"nointerpolation", (flags & TextureFlags_NoInterpolation) != 0},
	{"clampuvs", (flags & TextureFlags_ClampUVs) != 0}
    });
    parsed->anisotropy = 1.0f;
    auto texture = std::make_shared<CTexture> (getContext (), std::move (parsed), this->m_textureReduction);
    texture->startUserAnimation (getElapsedTime (), m_sceneFrame);
    m_userTextures.emplace (key, texture);
    return texture;
}
