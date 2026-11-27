#include "CPass.h"
#include "WallpaperEngine/Render/Depth.h"
#include <filesystem>
#include <sstream>
#include <utility>

#include "WallpaperEngine/Render/Helpers/ContextAware.h"

#include "WallpaperEngine/Data/Model/DynamicValue.h"
#include "WallpaperEngine/Data/Model/Effect.h"
#include "WallpaperEngine/Data/Model/Material.h"
#include "WallpaperEngine/Data/Model/PropertyAnimation.h"
// usertextures name user properties, so the pass needs the full Property definition to read them
#include "WallpaperEngine/Data/Model/Property.h"
#include "WallpaperEngine/Data/Model/UserSetting.h"
#include "WallpaperEngine/Data/Parsers/TextureParser.h"

#include "WallpaperEngine/Render/CFBO.h"
#include "WallpaperEngine/Render/Objects/CImage.h"

#include "WallpaperEngine/Render/Shaders/Variables/ShaderVariable.h"
#include "WallpaperEngine/Render/Shaders/Variables/ShaderVariableFloat.h"
#include "WallpaperEngine/Render/Shaders/Variables/ShaderVariableInteger.h"
#include "WallpaperEngine/Render/Shaders/Variables/ShaderVariableVector2.h"
#include "WallpaperEngine/Render/Shaders/Variables/ShaderVariableVector3.h"
#include "WallpaperEngine/Render/Shaders/Variables/ShaderVariableVector4.h"

#include "WallpaperEngine/Logging/Log.h"

using namespace WallpaperEngine;
using namespace WallpaperEngine::Render;
using namespace WallpaperEngine::Render::Objects;

using namespace WallpaperEngine::Render::Shaders::Variables;
using namespace WallpaperEngine::Render::Objects::Effects;

extern float g_Daytime;

const TextureMap DEFAULT_BINDS = {};
const ImageEffectPassOverride DEFAULT_OVERRIDE = {};

const glm::mat4 CPass::s_defaultMatrix = glm::mat4 (1.0f);

namespace {
bool usesPuppetMorphAlpha (const CRenderable& renderable, const MaterialPass& pass) {
    return renderable.is<CImage> () && static_cast<const CImage&> (renderable).hasPuppetMorphAlpha ()
	&& (pass.shader == "genericimage3" || pass.shader == "genericimage4" || pass.shader == "clippingmaskimage4");
}

TextureMap mergedTextures (const TextureMap& material, const TextureMap& overrides) {
    TextureMap result = material;
    for (const auto& [index, texture] : overrides) result.insert_or_assign (index, texture);
    return result;
}

std::string textureSizeLabel (const std::shared_ptr<const TextureProvider>& texture) {
    if (texture == nullptr) {
	return "<null>";
    }

    return std::to_string (texture->getRealWidth ()) + "x" + std::to_string (texture->getRealHeight ());
}
}

CPass::CPass (
    CRenderable& renderable, std::shared_ptr<const FBOProvider> fboProvider, const MaterialPass& pass,
    std::optional<std::reference_wrapper<const ImageEffectPassOverride>> override,
    std::optional<std::reference_wrapper<const TextureMap>> binds,
    std::optional<std::reference_wrapper<std::string>> target, ComboMap runtimeCombos, bool deferShaderSetup,
    std::shared_ptr<const EffectFBOBindings> effectFBOBindings
) :
    Helpers::ContextAware (renderable), m_renderable (renderable), m_fboProvider (std::move (fboProvider)),
    m_pass (pass), m_binds (binds.has_value () ? binds.value ().get () : DEFAULT_BINDS),
    m_override (override.has_value () ? override.value ().get () : DEFAULT_OVERRIDE),
    m_userTextureEntries (mergedTextures (m_pass.usertextures, m_override.usertextures)),
    m_runtimeCombos (std::move (runtimeCombos)), m_target (target),
    m_effectFBOBindings (std::move (effectFBOBindings)),
    m_blendingmode (pass.blending), m_depthtestmode (pass.depthtest), m_depthwritemode (pass.depthwrite) {
    if (!deferShaderSetup) this->initialize ();
    if (!m_userTextureEntries.empty ()) {
	m_renderable.getScene ().registerUserTexturePass (this);
    }
    // NOTE: m_vao is created lazily in render(): VAOs are not shared between GL
    // contexts, so it cannot be created here when built on the async switch worker
}

void CPass::initialize () {
    if (m_shaderInitialized) return;
    m_userTextureValues = userTextureValues ();
    m_userTextureProviders.clear ();
    setupShaders ();
    m_shaderInitialized = true;
    ++m_shaderRevision;
}

CPass::~CPass () {
    if (!m_userTextureEntries.empty ()) {
	m_renderable.getScene ().unregisterUserTexturePass (this);
    }
    this->releaseShaders ();

    glDeleteVertexArrays (1, &m_vao);
    this->m_vao = GL_NONE;
}

void CPass::releaseShaders () {
    // release the usage counts taken in setupTextureUniforms so videos that are no
    // longer referenced by any pass can stop decoding
    this->adjustTextureUsageCounts (false);

    this->m_sharedProgram.reset ();
    this->m_programSharingGroup.reset ();
    this->m_programSharingChecked = false;

    if (this->m_privateProgram != GL_NONE) {
	glDeleteProgram (this->m_privateProgram);
	this->m_privateProgram = GL_NONE;
    }
    this->m_interfaceProgram.reset ();
    this->m_programID = GL_NONE;
}

std::shared_ptr<const TextureProvider> CPass::resolveTexture (
    std::shared_ptr<const TextureProvider> expected, int index, std::shared_ptr<const TextureProvider> previous
) {
    if (expected == nullptr) {
	if (const auto it = this->m_fbos.find (index); it != this->m_fbos.end ()) {
	    expected = it->second;
	}
    }

    // first check in the binds and replace it if necessary
    const auto it = this->m_binds.find (index);

    if (it == this->m_binds.end ()) {
	return expected;
    }

    // a bind named "previous" is just another way of telling it to use whatever texture there was already
    if (it->second == "previous") {
	return this->m_previousInput ?: (previous ?: expected);
    }

    // the bind actually has a name, search the FBO in the effect and return it
    return this->resolveFBO (m_effectFBOBindings ? m_effectFBOBindings->resolveName (it->second) : it->second);
}

std::shared_ptr<const CFBO> CPass::resolveFBO (const std::string& name) const {
    auto fbo = this->m_fboProvider->find (name);
    if (fbo == nullptr && name == "_rt_MipMappedFrameBuffer") {
	fbo = this->m_renderable.getScene ().getMipMappedFramebuffer ();
    }

    if (fbo == nullptr) {
	sLog.exception ("Tried to resolve and FBO without any luck: ", name);
    }

    this->m_renderable.getScene ().retainRenderTarget (fbo);
    return fbo;
}

void CPass::setupRenderFramebuffer (const std::shared_ptr<const CFBO>& drawTo) const {
    // set the framebuffer we're drawing to
    glBindFramebuffer (GL_FRAMEBUFFER, drawTo->getDrawFramebuffer ());

    // set proper viewport based on what we're drawing to
    glViewport (0, 0, drawTo->getRealWidth (), drawTo->getRealHeight ());

    // Alpha-to-coverage is sticky OpenGL state, so always reset it before
    // selecting the pass' blending mode.
    glDisable (GL_SAMPLE_ALPHA_TO_COVERAGE);
    // Blend equations are sticky state. Most passes use ordinary addition, while
    // puppet mask generation selects MAX for RGB to union overlapping source parts.
    const auto blending = this->getBlendingMode ();
    const bool compositionAlpha = this->m_renderable.getScene ().usesCompositionAlphaMax ()
	&& (blending == BlendingMode_Translucent || blending == BlendingMode_Additive);
    glBlendEquationSeparate (this->m_colorBlendEquation, compositionAlpha ? GL_MAX : this->m_alphaBlendEquation);

    // set texture blending
    switch (this->getBlendingMode ()) {
	case BlendingMode_Translucent:
	    glEnable (GL_BLEND);
	    glBlendFuncSeparate (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	    break;
	case BlendingMode_Additive:
	    glEnable (GL_BLEND);
	    glBlendFuncSeparate (GL_SRC_ALPHA, GL_ONE, GL_SRC_ALPHA, GL_ONE);
	    break;
	case BlendingMode_Normal:
	    glEnable (GL_BLEND);
	    glBlendFuncSeparate (GL_ONE, GL_ZERO, GL_ONE, GL_ZERO);
	    break;
	case BlendingMode_AlphaToCoverage:
	    glDisable (GL_BLEND);
	    glEnable (GL_SAMPLE_ALPHA_TO_COVERAGE);
	    break;
	default:
	    glDisable (GL_BLEND);
	    break;
    }

    switch (this->m_depthtestmode) {
	case DepthtestMode_Enabled:
	    glEnable (GL_DEPTH_TEST);
	    // Native uses strict GREATER with reversed depth. In our conventional
	    // depth buffer the equivalent is LESS: coplanar additive surfaces must
	    // not brighten an opaque surface that already wrote the same depth.
	    glDepthFunc (drawTo->usesReverseDepth () ? GL_GREATER : GL_LESS);
	    break;
	case DepthtestMode_Disabled:
	default:
	    glDisable (GL_DEPTH_TEST);
	    break;
    }

    switch (this->m_pass.cullmode) {
	case CullingMode_Normal:
	    glEnable (GL_CULL_FACE);
	    break;

	case CullingMode_Disable:
	default:
	    glDisable (GL_CULL_FACE);
	    break;
    }

    switch (this->m_depthwritemode) {
	case DepthwriteMode_Enabled:
	    // Native material "enabled" preserves the blend mode's depth policy:
	    // translucent/additive passes never write depth, even with this setting.
	    // Otherwise invisible flare pixels and atmosphere shells hide later models.
	    glDepthMask (
		this->getBlendingMode () == BlendingMode_Normal
		|| this->getBlendingMode () == BlendingMode_AlphaToCoverage
	    );
	    break;

	case DepthwriteMode_Disabled:
	default:
	    glDepthMask (false);
	    break;
    }
}

void CPass::setupRenderTexture () {
    // Only explicit effect bindings participate in native swap commands.
    // Material texture references and the provider's physical registry stay fixed.
    if (m_effectFBOBindings) {
	if (m_copySource && m_textures.contains (0)) {
	    if (const auto source = m_fboProvider->find (m_effectFBOBindings->resolveName (*m_copySource))) {
		m_textures.at (0)->texture = source;
	    }
	}
	for (const auto& [index, name] : m_binds) {
	    if (name == "previous") continue;
	    if (const auto chain = m_textures.find (index); chain != m_textures.end ()) {
		chain->second->texture = resolveFBO (m_effectFBOBindings->resolveName (name));
	    }
	}
    }
    this->refreshUniformBindings ();
    // use the shader we have registered
    glUseProgram (this->m_sharedProgram ? this->m_sharedProgram->id : this->privateProgram ());

    auto texture0 = this->resolveTexture0 ();
    const auto animation = this->resolveTextureAnimationState (texture0);

    this->m_textureMipMapCounts.fill (1.0f);
    this->bindTextureUnit (0, texture0, animation.currentTexture);
    this->bindTextureOverrides (animation.currentTexture, texture0);

    if (texture0 != nullptr) {
	this->m_texture0Resolution = *texture0->getResolution ();
    }

    // used in animations when one of the frames is vertical instead of horizontal
    // rotation with translation = origin and end of the image to display
    if (this->g_Texture0Rotation != -1) {
	glUniform4f (
	    this->g_Texture0Rotation, animation.rotation.x, animation.rotation.y, animation.rotation.z,
	    animation.rotation.w
	);
    }
    // this actually picks the origin point of the image from the atlast
    if (this->g_Texture0Translation != -1) {
	glUniform2f (this->g_Texture0Translation, animation.translation.x, animation.translation.y);
    }
}

bool CPass::bindAlbedoTexture () {
    const auto texture = this->resolveTexture0 ();
    if (texture == nullptr || !texture->isReady ()) return false;
    const auto animation = this->resolveTextureAnimationState (texture);
    this->bindTextureUnit (0, texture, animation.currentTexture);
    return true;
}

std::shared_ptr<const TextureProvider> CPass::resolveTexture0 () {
    auto texture0 = this->resolveTexture (this->m_input, 0, this->m_input);
    const auto it = this->m_textures.find (0);

    if (it == this->m_textures.end ()) {
	return texture0;
    }

    // An unavailable system image may become ready later. Walking the stored
    // head would permanently discard it when selecting this frame's fallback.
    auto chain = it->second;

    do {
	texture0 = chain->texture;

	if (texture0 == nullptr) {
	    if (this->m_previousInput != nullptr && this->m_previousInput->isReady ()) {
		return this->m_previousInput;
	    }

	    if (this->m_input != nullptr && this->m_input->isReady ()) {
		return this->m_input;
	    }
	} else if (texture0->isReady ()) {
	    return texture0;
	}

	chain = chain->next;
    } while (chain != nullptr);

    // got to the end of the chain, use previous input or current input if available
    if (this->m_previousInput != nullptr && this->m_previousInput->isReady ()) {
	return this->m_previousInput;
    }

    // last resort, doesn't matter if the input is ready or not
    return this->m_input;
}

CPass::TextureAnimationState
CPass::resolveTextureAnimationState (const std::shared_ptr<const TextureProvider>& texture) const {
    TextureAnimationState state;

    if (texture == nullptr || !texture->isAnimated ()) {
	return state;
    }

    double currentRenderTime = this->m_renderable.getTextureAnimationTime (texture);

    for (const auto& frameCur : texture->getFrames ()) {
	currentRenderTime -= frameCur->frametime;

	if (currentRenderTime > 0.0f) {
	    continue;
	}

	state.currentTexture = frameCur->frameNumber;
	const glm::vec2 frameSize = texture->getAnimationUVSize (state.currentTexture);
	state.translation.x = frameCur->x / frameSize.x;
	state.translation.y = frameCur->y / frameSize.y;

	state.rotation.x = frameCur->width1 / frameSize.x;
	state.rotation.y = frameCur->width2 / frameSize.y;
	state.rotation.z = frameCur->height2 / frameSize.x;
	state.rotation.w = frameCur->height1 / frameSize.y;
	break;
    }

    return state;
}

void CPass::bindTextureUnit (int index, const std::shared_ptr<const TextureProvider>& texture, uint32_t frame) {
    if (texture == nullptr) {
	return;
    }

    // Metadata can describe samplers in disabled shader branches. Fetching an FBO's
    // texture resolves its full multisampled surface, even when no shader reads it.
    const bool samplerActive = index >= 0 && index < static_cast<int> (m_activeSamplers.size ())
	? m_activeSamplers[index]
	: m_uniforms.contains ("g_Texture" + std::to_string (index))
	    || m_referenceUniforms.contains ("g_Texture" + std::to_string (index));
    const auto& scene = this->m_renderable.getScene ();
    const bool cookie = scene.isLightCookieAlias (texture.get ());
    const auto bound = cookie && scene.getLightCookie () && scene.getLightCookie ()->isReady ()
	? scene.getLightCookie () : texture;
    if (samplerActive) {
	scene.markTextureUsed (*bound);
	glActiveTexture (GL_TEXTURE0 + index);
	glBindTexture (bound->getTextureTarget (), bound->getTextureID (frame));
	if (cookie) {
	    if (std::ranges::find (m_cookieSamplerBindings, index, &std::pair<int, GLint>::first)
		== m_cookieSamplerBindings.end ()) {
		GLint previous;
		glGetIntegeri_v (GL_SAMPLER_BINDING, index, &previous);
		m_cookieSamplerBindings.emplace_back (index, previous);
	    }
	    const GLuint sampler = this->m_renderable.getScene ().getLightCookieSampler ();
	    // The cookie overrides wrapping only. Native retains point/linear and mip
	    // filtering of the source, including textures also used by ordinary layers.
	    GLint minFilter, magFilter;
	    glGetTexParameteriv (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, &minFilter);
	    glGetTexParameteriv (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, &magFilter);
	    glSamplerParameteri (sampler, GL_TEXTURE_MIN_FILTER, minFilter);
	    glSamplerParameteri (sampler, GL_TEXTURE_MAG_FILTER, magFilter);
	    // Native alias sampling disables source anisotropy (140099980 mode1).
	    glSamplerParameterf (sampler, GL_TEXTURE_MAX_ANISOTROPY, 1.0f);
	    glBindSampler (index, sampler);
	}
    }
    // Resolution/texel/mipmap uniforms can be active without the sampler itself.
    if (const auto resolution = m_effectTextureResolutions.find (index); resolution != m_effectTextureResolutions.end ()) {
	resolution->second = *texture->getResolution ();
    }
    if (index >= 0 && index < static_cast<int> (this->m_textureMipMapCounts.size ())) {
	this->m_textureMipMapCounts[index] = static_cast<float> (texture->getMipMapCount (frame));
    }
    if (const auto texel = this->m_textureTexels.find (index); texel != this->m_textureTexels.end ()) {
	const glm::vec2 size (glm::max (texture->getTextureWidth (frame), 1u),
	    glm::max (texture->getTextureHeight (frame), 1u));
	texel->second = glm::vec4 (1.0f / size, size);
    }
}

void CPass::bindTextureOverrides (uint32_t currentTexture, std::shared_ptr<const TextureProvider>& texture0) {
    for (auto [index, chain] : this->m_textures) {
	// find the expected texture
	auto expectedTexture = chain->texture;

	do {
	    if (expectedTexture == nullptr) {
		if (this->m_previousInput != nullptr && this->m_previousInput->isReady ()) {
		    expectedTexture = this->m_previousInput;
		    break;
		}

		if (this->m_input != nullptr && this->m_input->isReady ()) {
		    expectedTexture = this->m_input;
		    break;
		}
	    } else if (expectedTexture->isReady ()) {
		break;
	    }

	    chain = chain->next;
	    expectedTexture = chain == nullptr ? nullptr : chain->texture;
	} while (chain != nullptr);

	if (expectedTexture == nullptr && this->m_previousInput != nullptr && this->m_previousInput->isReady ()) {
	    expectedTexture = this->m_previousInput;
	}

	if (expectedTexture == nullptr) {
	    expectedTexture = this->m_input;
	}

	this->bindTextureUnit (index, expectedTexture, index == 0 ? currentTexture : 0);

	if (index == 0) {
	    texture0 = expectedTexture;
	}
    }
}

void CPass::refreshUniformBindings () {
    if (!this->m_uniformBindingsDirty) return;
    this->m_drawUniforms.clear ();
    this->m_drawReferenceUniforms.clear ();
    std::set<GLint> writtenLocations;
    this->m_cacheUniformValues = true;
    for (const auto& value : this->m_uniforms | std::views::values) {
	static constexpr size_t sizes[] = { sizeof (float), 9 * sizeof (float), 16 * sizeof (float),
	    12 * sizeof (float), sizeof (int), 2 * sizeof (float), 3 * sizeof (float),
	    4 * sizeof (float), sizeof (double) };
	const int count = value->type == Vector2 || value->type == Matrix3 ? 1 : value->count;
	this->m_drawUniforms.push_back ({ value.get (), sizes[value->type] * static_cast<size_t> (std::max (count, 0)) });
	if (count <= 0) this->m_cacheUniformValues = false;
	for (int element = 0; element < count; ++element) {
	    if (!writtenLocations.insert (value->id + element).second) this->m_cacheUniformValues = false;
	}
    }
    // Aliased array writes or values written by another upload path cannot use
    // independent snapshots. Retain the original ordered uploads for that layout.
    for (const auto& value : this->m_referenceUniforms | std::views::values) {
	if (writtenLocations.contains (value->id)) this->m_cacheUniformValues = false;
    }
    if (writtenLocations.contains (this->g_Texture0Rotation)
	|| writtenLocations.contains (this->g_Texture0Translation)) this->m_cacheUniformValues = false;
    for (const auto& value : this->m_referenceUniforms | std::views::values) {
	this->m_drawReferenceUniforms.push_back (value.get ());
    }
    for (size_t index = 0; index < this->m_activeSamplers.size (); ++index) {
	const auto name = "g_Texture" + std::to_string (index);
	this->m_activeSamplers[index] = m_uniforms.contains (name) || m_referenceUniforms.contains (name);
    }
    this->m_uniformBindingsDirty = false;
}

void CPass::setupRenderReferenceUniforms () {
    this->refreshUniformBindings ();
    // add reference uniforms
    for (const auto* value : this->m_drawReferenceUniforms) {
	switch (value->type) {
	    case Double:
		glUniform1d (value->id, *static_cast<const double*> (*value->value));
		break;
	    case Float:
		glUniform1f (value->id, *static_cast<const float*> (*value->value));
		break;
	    case Integer:
		glUniform1i (value->id, *static_cast<const int*> (*value->value));
		break;
	    case Vector4:
		glUniform4fv (value->id, 1, glm::value_ptr (*static_cast<const glm::vec4*> (*value->value)));
		break;
	    case Vector3:
		glUniform3fv (value->id, 1, glm::value_ptr (*static_cast<const glm::vec3*> (*value->value)));
		break;
	    case Vector2:
		glUniform2fv (value->id, 1, glm::value_ptr (*static_cast<const glm::vec2*> (*value->value)));
		break;
	    case Matrix4:
		glUniformMatrix4fv (
		    value->id, 1, GL_FALSE, glm::value_ptr (*static_cast<const glm::mat4*> (*value->value))
		);
		break;
	    case Matrix4x3:
		glUniformMatrix4x3fv (
		    value->id, 1, GL_FALSE, glm::value_ptr (*static_cast<const glm::mat4x3*> (*value->value))
		);
		break;
	    case Matrix3:
		glUniformMatrix3fv (
		    value->id, 1, GL_FALSE, glm::value_ptr (*static_cast<const glm::mat3*> (*value->value))
		);
		break;
	}
    }
}

void CPass::setupRenderUniforms () {
    this->refreshUniformBindings ();
    if (m_usesLayerModelMatrix) m_layerModelMatrix = m_renderable.resolveLayerModelMatrix ();
    const double time = m_renderable.getScene ().getElapsedTime ();
    for (auto& [name, uniform] : m_animatedUniforms) {
	const auto& animation = *uniform.setting->animation;
	glm::vec4 value = uniform.setting->value->getVec4 ();
	for (int channel = 0; channel < 4; ++channel) {
	    value[channel] = animation.relative
		? value[channel] + animation.evaluateChannel (channel, time, 0.0f)
		: animation.evaluateChannel (channel, time, value[channel]);
	}
	uniform.sampled->update (value, DynamicValue::UpdateSource::Initialization);
    }
    // The snapshot belongs to the shared GL program, never an individual material.
    size_t offset = 0;
    for (const auto& binding : this->m_drawUniforms) {
	const auto* value = binding.entry;
	if (this->m_sharedProgram && this->m_cacheUniformValues) {
	    const bool changed = this->m_sharedProgram->updateUniform (offset, value->value, binding.bytes);
	    offset += binding.bytes;
	    if (!changed) continue;
	}
	switch (value->type) {
	    case Double:
		glUniform1dv (value->id, value->count, static_cast<const double*> (value->value));
		break;
	    case Float:
		glUniform1fv (value->id, value->count, static_cast<const float*> (value->value));
		break;
	    case Integer:
		glUniform1iv (value->id, value->count, static_cast<const int*> (value->value));
		break;
	    // Vector arrays share the same tightly packed storage as scalar uniforms.
	    case Vector4:
		glUniform4fv (value->id, value->count, glm::value_ptr (*static_cast<const glm::vec4*> (value->value)));
		break;
	    case Vector3:
		glUniform3fv (value->id, value->count, glm::value_ptr (*static_cast<const glm::vec3*> (value->value)));
		break;
	    case Vector2:
		glUniform2fv (value->id, 1, glm::value_ptr (*static_cast<const glm::vec2*> (value->value)));
		break;
	    case Matrix4:
		glUniformMatrix4fv (
		    value->id, value->count, GL_FALSE, glm::value_ptr (*static_cast<const glm::mat4*> (value->value))
		);
		break;
	    case Matrix4x3:
		glUniformMatrix4x3fv (
		    value->id, value->count, GL_FALSE,
		    glm::value_ptr (*static_cast<const glm::mat4x3*> (value->value))
		);
		break;
	    case Matrix3:
		glUniformMatrix3fv (
		    value->id, 1, GL_FALSE, glm::value_ptr (*static_cast<const glm::mat3*> (value->value))
		);
		break;
	}
    }
}

void CPass::setupRenderAttributes () const {
    // Offscreen image passes keep neutral coverage; the final puppet callback
    // replaces this constant with the per-vertex native morph alpha product.
    if (usesPuppetMorphAlpha (this->m_renderable, this->m_pass)) {
	const GLint color = glGetAttribLocation (this->m_programID, "a_Color");
	if (color >= 0) {
	    glDisableVertexAttribArray (color);
	    glVertexAttrib4f (color, 1, 1, 1, 1);
	}
    }
    if (this->m_setupAttribsCallback) {
	this->m_setupAttribsCallback ();
	return;
    }

    for (const auto& cur : this->m_attribs) {
	glEnableVertexAttribArray (cur->id);
	glBindBuffer (GL_ARRAY_BUFFER, *cur->value);
	glVertexAttribPointer (cur->id, cur->elements, cur->type, GL_FALSE, 0, nullptr);

#if !NDEBUG
	glObjectLabel (
	    GL_BUFFER, *cur->value, -1,
	    ("Image " + std::to_string (this->m_renderable.getId ()) + " Pass " + this->m_pass.shader + " " + cur->name)
		.c_str ()
	);
#endif /* DEBUG */
    }
}

void CPass::renderGeometry () const {
    if (this->m_drawGeometryCallback) {
	this->m_drawGeometryCallback ();
	return;
    }

    // start actual rendering now
    glBindBuffer (GL_ARRAY_BUFFER, this->a_Position);
    glDrawArrays (GL_TRIANGLES, 0, 6);
}

void CPass::cleanupRenderSetup () {
    for (const auto& [index, sampler] : m_cookieSamplerBindings) glBindSampler (index, sampler);
    m_cookieSamplerBindings.clear ();
    if (this->m_cleanupAttribsCallback) {
	this->m_cleanupAttribsCallback ();
    } else {
	// disable vertex attribs array
	for (const auto& cur : this->m_attribs) {
	    glDisableVertexAttribArray (cur->id);
	}
    }

    // unbind all the used textures
    glActiveTexture (GL_TEXTURE0);
    glBindTexture (GL_TEXTURE_2D, 0);
    glBindTexture (GL_TEXTURE_3D, 0);

    // continue on the map from the second texture
    for (const auto& index : this->m_textures | std::views::keys) {
	glActiveTexture (GL_TEXTURE0 + index);
	glBindTexture (GL_TEXTURE_2D, 0);
	glBindTexture (GL_TEXTURE_3D, 0);
    }
}

void CPass::render () {
    this->updateFogShader ();
    // created lazily on the render thread: VAOs are not shared between GL contexts,
    // so an async-built wallpaper cannot create it on the worker's context
    if (this->m_vao == GL_NONE) {
	glGenVertexArrays (1, &this->m_vao);
    }

    // set the VAO for now
    glBindVertexArray (this->m_vao);

    const auto& debug = this->getContext ().getApp ().getContext ().settings.render.debug;
    if (debug.passLog) {
	sLog.out (
	    "Render pass object=", this->m_renderable.getId (), " shader=", this->m_pass.shader,
	    " target=", this->m_target.has_value () ? this->m_target.value ().get () : std::string ("<screen/local>"),
	    " drawTo=", this->m_drawTo ? this->m_drawTo->getName () : std::string ("<null>"),
	    " drawSize=", textureSizeLabel (this->m_drawTo), " inputSize=", textureSizeLabel (this->m_input)
	);
	for (const auto* uniformName :
	     { "g_TintColor", "g_CompositeColor", "g_BlendAlpha", "g_CompositeAlpha", "g_UserAlpha" }) {
	    const auto uniform = this->m_uniforms.find (uniformName);
	    if (uniform == this->m_uniforms.end ()) {
		continue;
	    }

	    switch (uniform->second->type) {
		case Vector3:
		    {
			const auto* v = static_cast<const glm::vec3*> (uniform->second->value);
			sLog.out ("  uniform ", uniformName, "=", v->x, " ", v->y, " ", v->z);
			break;
		    }
		case Float:
		    {
			const auto* v = static_cast<const float*> (uniform->second->value);
			sLog.out ("  uniform ", uniformName, "=", *v);
			break;
		    }
		default:
		    break;
	    }
	}
    }

    auto destination = m_drawTo;
    if (m_effectFBOBindings && m_target) {
	const auto& name = m_effectFBOBindings->resolveName (m_target->get ());
	if (const auto target = m_fboProvider->find (name)) destination = target;
    }
    const auto drawTo = this->m_renderable.getScene ().resolveRenderTarget (destination);
    if (drawTo == nullptr) {
	sLog.error ("Skipping render pass for object ", this->m_renderable.getId (), ": no destination FBO set");
	return;
    }

    if (this->m_input == nullptr) {
	sLog.error ("Skipping render pass for object ", this->m_renderable.getId (), ": no input texture set");
	return;
    }

    if (!this->m_programSharingChecked) {
	this->setupProgramSharing ();
    }
    const ClipDepthScope clipDepth (m_reverseDepth);
    this->setupRenderFramebuffer (drawTo);
    this->setupRenderTexture ();
    // genericimage3/4 (and VERSION-enabled genericimage2) encode object opacity in
    // g_Color4.a and do not expose g_UserAlpha. Refresh the combined value here so
    // live property changes affect those shaders without double-applying alpha to
    // older shaders that consume g_UserAlpha directly.
    this->m_effectiveColor4 = this->m_renderable.getColor4 ();
    this->m_effectiveColor4.a *= this->m_renderable.getUserAlpha ();
    // Feedback simulations such as Wallpaper Engine's cursor-ripple effect multiply both
    // force injection and propagation by g_Frametime. Leaving the native global unbound
    // gives GLSL's zero default and freezes the simulation completely. Use CScene's
    // per-output delta so a slower monitor receives all elapsed time instead of only the
    // most recent application-loop interval.
    this->m_sceneTime = this->m_renderable.getScene ().getTime ();
    this->m_frameTime = this->m_renderable.getScene ().getDeltaTime ();
    this->m_sceneTexelSize = 1.0f / glm::vec2 (this->m_renderable.getScene ().getFramebufferSize ());
    this->m_sceneTexelSizeHalf = this->m_sceneTexelSize * 0.5f;
    const glm::vec2 outputSize = this->m_renderable.getScene ().getOutputSize ();
    this->m_screen = glm::vec3 (outputSize, outputSize.x / outputSize.y);
    const bool singleSampleCoverage = this->getBlendingMode () == BlendingMode_AlphaToCoverage
	&& drawTo->getDrawSamples () == 1;
    this->m_singleSampleCoverageHeight = singleSampleCoverage ? static_cast<int> (drawTo->getRealHeight ()) : 0;
    // A flipped presentation already places framebuffer row zero at the top.
    if (singleSampleCoverage && this->getContext ().getOutput ().renderVFlip ()) {
	this->m_singleSampleCoverageHeight = -this->m_singleSampleCoverageHeight;
    }
    this->setupRenderUniforms ();
    this->setupRenderReferenceUniforms ();
    this->setupRenderAttributes ();
    this->renderGeometry ();
    this->cleanupRenderSetup ();

    // Refresh only Wallpaper Engine's dedicated mipmapped target. Ordinary
    // _rt_imageLayerComposite_* targets intentionally remain single-level.
    drawTo->generateMipmaps ();
    this->m_renderable.getScene ().loadingCheckpoint ();
}

std::shared_ptr<const FBOProvider> CPass::getFBOProvider () const { return this->m_fboProvider; }

const CRenderable& CPass::getRenderable () const { return this->m_renderable; }

void CPass::setCopySource (const std::string& source) { m_copySource = source; }

void CPass::setDestination (std::shared_ptr<const CFBO> drawTo) { this->m_drawTo = std::move (drawTo); }

void CPass::setInput (std::shared_ptr<const TextureProvider> input) { this->m_input = std::move (input); }

void CPass::setPreviousInput (std::shared_ptr<const TextureProvider> input) {
    this->m_previousInput = std::move (input);
}

void CPass::setModelViewProjectionMatrix (const glm::mat4* projection, const bool reverseDepth) {
    m_reverseDepth = reverseDepth;
    this->m_modelViewProjectionMatrix = projection;
}

void CPass::setModelViewProjectionMatrixInverse (const glm::mat4* projection) {
    this->m_modelViewProjectionMatrixInverse = projection;
}

void CPass::setModelMatrix (const glm::mat4* model) { this->m_modelMatrix = model; }

void CPass::setViewProjectionMatrix (const glm::mat4* viewProjection) { this->m_viewProjectionMatrix = viewProjection; }

void CPass::setEffectTextureProjectionMatrix (const glm::mat4* matrix, const glm::mat4* inverse) {
    this->m_effectTextureProjectionMatrix = matrix;
    this->m_effectTextureProjectionMatrixInverse = inverse;
}

void CPass::setBlendingMode (BlendingMode blendingmode) { this->m_blendingmode = blendingmode; }

BlendingMode CPass::getBlendingMode () const { return this->m_blendingmode; }

void CPass::setDepthtestMode (DepthtestMode depthtestmode) { this->m_depthtestmode = depthtestmode; }

DepthtestMode CPass::getDepthtestMode () const { return this->m_depthtestmode; }

void CPass::setDepthwriteMode (DepthwriteMode depthwritemode) { this->m_depthwritemode = depthwritemode; }

DepthwriteMode CPass::getDepthwriteMode () const { return this->m_depthwritemode; }

void CPass::setBlendEquation (const GLenum color, const GLenum alpha) {
    this->m_colorBlendEquation = color;
    this->m_alphaBlendEquation = alpha;
}

void CPass::setTexCoord (GLuint texcoord) { this->a_TexCoord = texcoord; }

void CPass::setPosition (GLuint position) { this->a_Position = position; }

const MaterialPass& CPass::getPass () const { return this->m_pass; }

const ImageEffectPassOverride& CPass::getOverride () const { return this->m_override; }

const TextureMap& CPass::getBinds () const { return this->m_binds; }

std::optional<std::reference_wrapper<std::string>> CPass::getTarget () const { return this->m_target; }

Render::Shaders::Shader* CPass::getShader () const { return this->m_shader.get (); }

GLuint CPass::getProgramID () const { return this->m_programID; }

uint64_t CPass::getShaderRevision () const { return this->m_shaderRevision; }

void CPass::setRuntimeCombos (const ComboMap& combos) {
    if (combos.empty ()) return;
    for (const auto& [name, value] : combos) m_runtimeCombos.insert_or_assign (name, value);
    if (m_shaderInitialized) this->rebuildShaders ();
}

void CPass::setGeometryCallback (
    GeometryCallback setupAttribs, GeometryCallback drawGeometry, GeometryCallback cleanupAttribs
) {
    this->m_setupAttribsCallback = std::move (setupAttribs);
    this->m_drawGeometryCallback = std::move (drawGeometry);
    this->m_cleanupAttribsCallback = std::move (cleanupAttribs);
}

void CPass::setupShaders () {
    // ensure the constants are defined
    const auto texture0 = this->m_renderable.getTexture ();

    // copy the combos from the pass
    this->m_combos.insert (this->m_pass.combos.begin (), this->m_pass.combos.end ());
    for (const auto& [name, value] : this->m_runtimeCombos) {
	this->m_combos.insert_or_assign (name, value);
    }

    if (usesPuppetMorphAlpha (this->m_renderable, this->m_pass))
	this->m_combos.insert_or_assign (this->m_pass.shader == "clippingmaskimage4" ? "PUPPET_MORPH_ALPHA" : "VERTEXCOLOR", 1);

    this->m_combos.insert_or_assign ("HDR", this->m_renderable.getScene ().isHdr () ? 1 : 0);

    // This combo selects the shader's coverage path. Multisampled targets use
    // the resulting alpha through GL_SAMPLE_ALPHA_TO_COVERAGE.
    this->m_combos.insert_or_assign (
	"ALPHATOCOVERAGE", this->m_pass.blending == BlendingMode_AlphaToCoverage ? 1 : 0
    );

    // WE compiles every scene shader with SCENE_ORTHO describing the camera type; lit shaders
    // (genericimage3/4) use it to pick a fixed view vector on 2D scenes vs. the perspective
    // view direction on 3D scenes (combo list mined from wallpaper64.exe)
    this->m_combos.insert_or_assign (
	"SCENE_ORTHO", this->m_renderable.getScene ().getScene ().camera.projection.isPerspective ? 0 : 1
    );

    // CImage supplies FOG_COMPUTED only to modern scene presentation passes.
    // Inferring it here would bake fog into reusable image composites.
    // generic4 model shaders select fog directly with FOG_DIST/FOG_HEIGHT.
    this->m_fogCombos = this->fogCombos ();
    this->m_combos.insert_or_assign ("FOG_DIST", this->m_fogCombos.x);
    this->m_combos.insert_or_assign ("FOG_HEIGHT", this->m_fogCombos.y);

    this->m_renderable.getScene ().applyLightCombos (this->m_combos);

    // Image samplers opt into TEX<n>FORMAT with formatcombo metadata. Keep
    // concrete formats until preprocessing discovers those requests. DecompressNormal
    // reads RG88 from .rg, while DXT normals are stored in .ay. Treating an
    // RG88 normal as RGBA forces one axis to 1 and produces extreme lighting.
    // The high texture flag nibble is the matching packed-component occupancy
    // mask consumed by sampler metadata's `components` array.
    const bool imageFormatRequests = m_renderable.is<CImage> ();
    std::map<int, int> textureFormats;
    const auto registerTextureMetadata = [this, &textureFormats, imageFormatRequests] (
	const int index, const std::shared_ptr<const TextureProvider>& texture, const bool overwrite = true
    ) {
	      if (texture == nullptr) {
		  return;
	      }

	      const std::string prefix = "TEX" + std::to_string (index);

	      // A slot the pass authors wins over the shader's own default: that is the texture
	      // setupTextureUniforms ends up binding, so its format is the one the shader must see.
	      if (!overwrite && (textureFormats.contains (index) || this->m_combos.contains (prefix + "FORMAT"))) {
		  return;
	      }

	      textureFormats.insert_or_assign (index, static_cast<int> (texture->getFormat ()));
	      if (!imageFormatRequests) {
		  this->m_combos.insert_or_assign (prefix + "FORMAT", static_cast<int> (texture->getFormat ()));
	      }
	      // Native component combos come from existing slots, never shader defaults.
	      if (overwrite) {
		  this->m_combos.insert_or_assign (
		      prefix + "COMPONENTS", static_cast<int> ((texture->getFlags () & TextureFlags_ComponentMask) >> 20)
		  );
	      }
	  };

    // Initial format defaults and failed selected images can expose cold sources.
    // Keep their prepared providers alive until setupUniforms installs the bindings.
    std::vector<std::shared_ptr<const TextureProvider>> preparedUserFallbacks;
    const auto registerAuthoredTextures
	= [this, &registerTextureMetadata, &preparedUserFallbacks] (const TextureMap& textures, const bool overwrite = true) {
	      for (const auto& [index, name] : textures) {
		  if (name.starts_with ("_rt_") || name.starts_with ("_alias_")) {
		      continue;
		  }

		  try {
		      std::shared_ptr<const TextureProvider> selected;
		      if (m_userTextureEntries.contains (index)) {
			  try { selected = resolveUserTexture (index); }
			  catch (const std::runtime_error&) { }
		      }
		      const auto& scene = m_renderable.getScene ();
		      if (!selected && !name.starts_with ('$')
			  && ((!overwrite && scene.isConstructing ())
			      || (m_userTextureEntries.contains (index) && m_shaderRevision != 0 && overwrite
				  && std::filesystem::path (m_userTextureValues.at (index)).is_absolute ()))) {
			  const auto prepared = getContext ().prepareTextureFallbacks (
			      { name }, m_renderable.getAssetLocator (), scene.getScene ().project.properties,
			      scene.getTextureReduction ()
			  );
			  preparedUserFallbacks.insert (preparedUserFallbacks.end (), prepared.begin (), prepared.end ());
		      }
		      registerTextureMetadata (index, selected ? selected : this->m_renderable.getScene ().resolveTexture (name), overwrite);
		  } catch (const std::runtime_error&) {
		      // setupTextureUniforms reports unresolved authored textures later;
		      // metadata discovery should not turn that recoverable path into a fatal one.
		  }
	      }
	  };

    const auto registerUserTextures = [&] (const TextureMap& textures) {
	for (const auto index : textures | std::views::keys) {
	    try {
		registerTextureMetadata (index, resolveUserTexture (index));
	    } catch (const std::runtime_error&) {
		// Report unavailable selections during setup; retain authored metadata.
	    }
	}
    };
    registerTextureMetadata (0, texture0);
    registerAuthoredTextures (mergedTextures (m_pass.textures, m_override.textures));
    registerUserTextures (mergedTextures (m_pass.usertextures, m_override.usertextures));
    // Selected pixels can change while native retains the initial format branch.
    // Freeze values before publishing, so a cached value cannot bypass opt-in.
    const auto retainUserTextureFormats = [&] {
	for (const auto index : m_userTextureEntries | std::views::keys) {
	    if (const auto initial = m_userTextureFormats.find (index); initial != m_userTextureFormats.end ()) {
		textureFormats.insert_or_assign (index, initial->second);
	    } else if (const auto current = textureFormats.find (index); current != textureFormats.end ()) {
		m_userTextureFormats.emplace (index, current->second);
	    }
	    if (!imageFormatRequests && textureFormats.contains (index)) {
		m_combos.insert_or_assign ("TEX" + std::to_string (index) + "FORMAT", textureFormats.at (index));
	    }
	}
    };
    if (!imageFormatRequests) retainUserTextureFormats ();

    // TODO: REVIEW THE SHADER TEXTURES HERE, THE ONES PASSED ON TO THE SHADER SHOULD NOT BE IN THE LIST
    // TODO: USED TO BUILD THE TEXTURES LATER
    // use the combos copied from the pass so it includes the texture format
    const std::string& shaderName
	= this->m_override.shaderOverride.has_value () ? this->m_override.shaderOverride.value () : this->m_pass.shader;

    TextureMap passTextures = this->m_pass.textures;
    for (const auto& [index, texture] : this->m_pass.usertextures) {
	passTextures.insert_or_assign (index, texture);
    }

    TextureMap overrideTextures = this->m_override.textures;
    for (const auto& [index, texture] : this->m_override.usertextures) {
	overrideTextures.insert_or_assign (index, texture);
    }

    this->m_shader = std::make_unique<Render::Shaders::Shader> (
	this->m_renderable.getAssetLocator (), shaderName, this->m_combos, this->m_override.combos, passTextures,
	overrideTextures, this->m_override.constants
    );

    // A shader default takes precedence over the image's provisional slot-zero
    // provider, but never over an authored or user-selected slot.
    if (imageFormatRequests && !passTextures.contains (0) && !overrideTextures.contains (0)
	&& (m_shader->getVertex ().getTextures ().contains (0)
	    || m_shader->getFragment ().getTextures ().contains (0))) {
	textureFormats.erase (0);
	// Shader defaults keep the preprocessed component metadata.
    }

    // Samplers the shader declares itself (the "formatcombo":true defaults, like generic4's
    // toon shading gradient) are only discovered while the units preprocess, which happens in
    // the constructor above. ShaderUnit keeps m_combos by reference and only emits the #define
    // block on compile (), so registering them here still reaches the generated source. Without
    // this an R8 gradient reads through the RGBA path and every lit pixel comes out red.
    const auto registerShaderFormats = [&] (const TextureMap& textures) {
	for (const auto& [index, name] : textures) {
	    if (m_shader->getVertex ().requestsTextureFormat (index)
		|| m_shader->getFragment ().requestsTextureFormat (index)) {
		registerAuthoredTextures ({ { index, name } }, false);
	    }
	}
    };
    registerShaderFormats (this->m_shader->getVertex ().getTextures ());
    registerShaderFormats (this->m_shader->getFragment ().getTextures ());

    if (imageFormatRequests) {
	retainUserTextureFormats ();
	for (const auto& [index, format] : textureFormats) {
	    if (m_shader->getVertex ().requestsTextureFormat (index)
		|| m_shader->getFragment ().requestsTextureFormat (index)) {
		m_combos.insert_or_assign ("TEX" + std::to_string (index) + "FORMAT", format);
	    }
	}
    }

    const auto [vertex, fragment]
	= Shaders::GLSLContext::get ().toGlsl (this->m_shader->vertex (), this->m_shader->fragment ());

    this->m_interfaceProgram = this->m_renderable.getScene ().getContext ().getShaderProgramCache ().interfaceProgram (
	vertex, fragment, this->m_programSharingGroup
    );
    this->m_programID = this->m_interfaceProgram->id;
#if !NDEBUG
    glObjectLabel (GL_PROGRAM, this->m_programID, -1, shaderName.c_str ());
#endif

    // first setup the default values, these will be overwritten by future values
    this->setupShaderVariables ();
    // setup uniforms
    this->setupUniforms ();
    // setup attributes too
    this->setupAttributes ();
    // get information from the program, like uniforms, etc
    // support three textures for now
    this->g_Texture0Rotation = glGetUniformLocation (this->m_programID, "g_Texture0Rotation");
    this->g_Texture0Translation = glGetUniformLocation (this->m_programID, "g_Texture0Translation");
}

glm::ivec2 CPass::fogCombos () const {
    const auto fogOverride = this->m_override.combos.find ("FOG");
    const auto fogMaterial = this->m_pass.combos.find ("FOG");
    const bool disabled = fogOverride != this->m_override.combos.end () ? fogOverride->second == 0
	: fogMaterial != this->m_pass.combos.end () && fogMaterial->second == 0;
    const auto& fog = this->m_renderable.getScene ().getFog ();
    return { fog.distanceEnabled && !disabled, fog.heightEnabled && !disabled };
}

void CPass::updateFogShader () {
    if (this->m_fogCombos == this->fogCombos ()) return;

    // Scene scripts update these flags after construction. Re-select the actual
    // shader branches; zeroing density alone would leave authored #if behavior
    // and newly active uniforms wrong. GLSL/program caches reuse prior variants.
    this->rebuildShaders ();
}

void CPass::rebuildShaders () {
    this->releaseShaders ();
    // Keep authored/default providers resident until their replacement chains
    // have been built. Cache pressure must not turn a rebind into a cold decode.
    const auto previousTextures = std::move (this->m_textures);
    this->m_shaderInitialized = false;
    this->m_uniformBindingsDirty = true;
    this->m_uniforms.clear ();
    this->m_referenceUniforms.clear ();
    this->m_animatedUniforms.clear ();
    this->m_constantUniforms.clear ();
    this->m_attribs.clear ();
    this->m_textureTexels.clear ();
    this->m_combos.clear ();
    this->initialize ();

    for (const auto& [name, binding] : this->m_externalUniforms) {
	const GLint id = glGetUniformLocation (this->m_programID, name.c_str ());
	if (id < 0) continue;
	this->m_referenceUniforms.erase (name);
	this->m_uniforms.insert_or_assign (
	    name, std::make_unique<UniformEntry> (id, name, binding.type, binding.value, binding.count)
	);
    }
}

const std::string& CPass::userTextureValue (const std::string& entry) const {
    static const std::string empty;
    const auto& properties = m_renderable.getScene ().getScene ().project.properties;
    if (const auto property = properties.find (entry); property != properties.end ()) {
	return property->second->getType () == DynamicValue::String ? property->second->getString () : empty;
    }
    return entry;
}

TextureMap CPass::userTextureValues () const {
    auto values = m_userTextureEntries;
    for (auto& [index, entry] : values) entry = userTextureValue (entry);
    return values;
}

void CPass::collectUserTextureFallbacks (std::set<std::string>& names) const {
    if (!m_shaderInitialized) return;
    for (const auto& [index, entry] : m_userTextureEntries) {
	if (!userTextureValue (entry).empty () || m_userTextureValues.at (index).empty ()) continue;
	const auto& originals = m_override.textures.contains (index) ? m_override.textures : m_pass.textures;
	const auto authored = originals.find (index);
	if (authored != originals.end () && !authored->second.empty () && !authored->second.starts_with ("_rt_")
	    && !authored->second.starts_with ("_alias_") && !authored->second.starts_with ('$')) names.insert (authored->second);
    }
}

bool CPass::updateUserTextures () {
    // Deferred effects read the latest selection when first initialized. Equal
    // paths do not reload changed file contents, matching native property updates.
    if (!m_shaderInitialized) return false;
    for (const auto& [index, entry] : m_userTextureEntries) {
	if (m_userTextureValues.at (index) != userTextureValue (entry)) {
	    rebuildShaders ();
	    return true;
	}
    }
    return false;
}

void CPass::setupAttributes () {
    this->addAttribute ("a_TexCoord", GL_FLOAT, 2, &this->a_TexCoord);
    this->addAttribute ("a_Position", GL_FLOAT, 3, &this->a_Position);
}

void CPass::setupProgramSharing () {
    this->m_programSharingChecked = true;
    if (!this->m_programSharingGroup) {
	return;
    }
    // Every entry is supplied before every draw. Including names, locations,
    // types and array counts prevents a pass from inheriting another pass's
    // extra uniforms or array tail. Unregistered uniforms retain GLSL defaults.
    std::ostringstream layout;
    for (const auto& [name, uniform] : this->m_uniforms) {
	layout << name << ':' << uniform->id << ':' << uniform->type << ':' << uniform->count << ';';
    }
    layout << '|';
    for (const auto& [name, uniform] : this->m_referenceUniforms) {
	layout << name << ':' << uniform->id << ':' << uniform->type << ';';
    }
    layout << '|' << this->g_Texture0Rotation << ':' << this->g_Texture0Translation;
    this->m_sharedProgram
	= Shaders::ShaderProgramCache::shareProgram (this->m_programID, this->m_programSharingGroup, layout.str ());
}

GLuint CPass::privateProgram () {
    // Without a sharing group the interface program belongs to this pass alone.
    if (this->m_privateProgram == GL_NONE && this->m_programSharingGroup) {
	this->m_privateProgram = Shaders::ShaderProgramCache::cloneProgram (this->m_programID);
	if (this->m_privateProgram == GL_NONE) {
	    const auto [vertex, fragment]
		= Shaders::GLSLContext::get ().toGlsl (this->m_shader->vertex (), this->m_shader->fragment ());
	    this->m_privateProgram = Shaders::ShaderProgramCache::compileCompatibleProgram (
		this->m_programID, vertex, fragment
	    );
	}
    }
    return this->m_privateProgram != GL_NONE ? this->m_privateProgram : this->m_programID;
}

void CPass::leaveProgramSharing () {
    if (!this->m_sharedProgram) {
	return;
    }
    // Registration normally finishes before the first render. If a caller
    // changes it later, keep this pass private from now on. Materialize its own
    // values before changing the old layout, preserving partially written arrays
    // without copying whichever other material last used the shared program.
    GLint previous = 0;
    glGetIntegerv (GL_CURRENT_PROGRAM, &previous);
    glUseProgram (this->privateProgram ());
    const auto shared = std::move (this->m_sharedProgram);
    this->setupRenderUniforms ();
    this->setupRenderReferenceUniforms ();
    glUseProgram (previous);
    this->m_programSharingGroup.reset ();
}

std::shared_ptr<const TextureProvider> CPass::resolveUserTexture (int index) {
    if (const auto cached = m_userTextureProviders.find (index); cached != m_userTextureProviders.end ()) return cached->second;
    const auto& scene = m_renderable.getScene ();
    const auto& name = m_userTextureValues.at (index);
    if (name.empty ()) return nullptr;
    const auto& originals = m_override.textures.contains (index) ? m_override.textures : m_pass.textures;
    const auto authored = originals.find (index);
    if (authored == originals.end ()) return nullptr;
    glm::vec2 size (m_override.userTextureReferenceWidth.value_or (m_pass.userTextureReference.x),
	m_override.userTextureReferenceHeight.value_or (m_pass.userTextureReference.y));
    const bool reference = size.x > 0.0f && size.y > 0.0f;
    uint32_t flags = 0;
    if (!reference) {
	Data::Assets::TextureUniquePtr header;
	if (!authored->second.starts_with ("_rt_") && !authored->second.starts_with ("_alias_")
	    && !authored->second.starts_with ('$')) {
	    try {
		header = Data::Parsers::TextureParser::loadHeader (m_renderable.getAssetLocator (), authored->second);
	    } catch (const std::runtime_error&) {
		// Source-import compatibility and runtime aliases retain the ordinary resolver.
	    }
	}
	if (header && !header->isAnimated () && header->variants.empty ()) {
	    size = glm::vec2 (header->width, header->height);
	    flags = header->flags;
	} else {
	    const auto original = authored->second.starts_with ("_rt_") || authored->second.starts_with ("_alias_")
		? resolveFBO (authored->second) : scene.resolveTexture (authored->second);
	    size = glm::vec2 (original->getRealWidth (), original->getRealHeight ());
	    flags = original->getFlags ();
	}
    }
    auto texture = name.starts_with ("_rt_") || name.starts_with ("_alias_") ? resolveFBO (name)
	: scene.resolveUserTexture (name, flags, size.y > 0.0f ? size.x / size.y : 0.0f);
    // Only raw absolute selections suppress the authored chain. Relative/runtime
    // compatibility providers can become unready and keep their existing fallback.
    if (texture && std::filesystem::path (name).is_absolute ()) m_userTextureProviders.emplace (index, texture);
    return texture;
}

void CPass::setupUserTexture (int index, const std::string& entry) {
    try {
	const auto texture = resolveUserTexture (index);
	if (!texture) return;
	const auto it = m_textures.find (index);
	m_textures[index] = std::make_shared<TextureChainEntry> (TextureChainEntry {
	    .texture = texture,
	    .next = it != m_textures.end () ? it->second : nullptr,
	});
    } catch (const std::runtime_error& ex) {
	sLog.error ("Cannot resolve user texture ", entry, " for pass ", ex.what ());
    }
}

void CPass::setupTextureUniforms () {
    const auto resolveDefaultTexture = [this] (const std::string& name) -> std::shared_ptr<const TextureProvider> {
	if (name.starts_with ("_rt_") || name.starts_with ("_alias_")) return this->resolveFBO (name);
	const auto& scene = m_renderable.getScene ();
	std::vector<std::shared_ptr<const TextureProvider>> prepared;
	if (scene.isConstructing () && !name.starts_with ('$')) {
	    prepared = getContext ().prepareTextureFallbacks (
		{ name }, m_renderable.getAssetLocator (), scene.getScene ().project.properties, scene.getTextureReduction ()
	    );
	}
	// The returned provider takes ownership before the local prepared hold ends.
	return scene.resolveTexture (name);
    };
    // first set default textures extracted from the shader
    // vertex shader doesn't seem to have texture info
    // but for now just set first vertex's textures
    // and then try with fragment's and override any existing
    for (const auto& [index, textureName] : this->m_shader->getVertex ().getTextures ()) {
	if (m_userTextureProviders.contains (index)) continue;
	// Metadata includes inactive branches; only reflected samplers acquire defaults.
	if (glGetUniformLocation (this->m_programID, ("g_Texture" + std::to_string (index)).c_str ()) < 0) {
	    continue;
	}
	try {
	    auto texture = resolveDefaultTexture (textureName);

	    // create chain entry
	    this->m_textures[index] = std::make_shared<TextureChainEntry> (TextureChainEntry {
		.texture = texture,
		.next = nullptr,
	    });
	} catch (std::runtime_error& ex) {
	    sLog.error ("Cannot resolve texture ", textureName, " for fragment shader ", ex.what ());
	}
    }

    for (const auto& [index, textureName] : this->m_shader->getFragment ().getTextures ()) {
	if (m_userTextureProviders.contains (index)) continue;
	if (glGetUniformLocation (this->m_programID, ("g_Texture" + std::to_string (index)).c_str ()) < 0) {
	    continue;
	}
	try {
	    auto texture = resolveDefaultTexture (textureName);

	    const auto it = this->m_textures.find (index);
	    const auto chain = std::make_shared<TextureChainEntry> (TextureChainEntry {
		.texture = texture,
		.next = it != this->m_textures.end () ? it->second : nullptr,
	    });

	    this->m_textures[index] = chain;
	} catch (std::runtime_error& ex) {
	    sLog.error ("Cannot resolve texture ", textureName, " for fragment shader ", ex.what ());
	}
    }

    for (const auto& [index, textureName] : mergedTextures (m_pass.textures, m_override.textures)) {
	if (m_userTextureProviders.contains (index)) continue;
	try {
	    auto texture = textureName.find ("_rt_") == 0 || textureName.find ("_alias_") == 0
		? this->resolveFBO (textureName)
		: this->m_renderable.getScene ().resolveTexture (textureName);

	    const auto it = this->m_textures.find (index);
	    const auto chain = std::make_shared<TextureChainEntry> (TextureChainEntry {
		.texture = texture,
		.next = it != this->m_textures.end () ? it->second : nullptr,
	    });

	    this->m_textures[index] = chain;
	} catch (std::runtime_error& ex) {
	    sLog.error ("Cannot resolve texture ", textureName, " for pass ", ex.what ());
	}
    }

    for (const auto& [index, entry] : mergedTextures (m_pass.usertextures, m_override.usertextures)) {
	setupUserTexture (index, entry);
    }

    // binds are set last as they're the most important to be set
    for (const auto& [index, bind] : this->m_binds) {
	const auto texture = bind == "previous" ? nullptr : this->resolveFBO (bind);
	const auto it = this->m_textures.find (index);
	const auto chain = std::make_shared<TextureChainEntry> (TextureChainEntry {
	    .texture = texture,
	    .next = it != this->m_textures.end () ? it->second : nullptr,
	});

	this->m_textures[index] = chain;
    }

    // resolve the main texture
    std::shared_ptr<const TextureProvider> texture = this->resolveTexture (this->m_renderable.getTexture (), 0);
    // A material can leave slot zero empty and use the shader's declared texture.
    if (!texture) texture = this->resolveTexture0 ();
    // register all the texture uniforms with correct values
    this->addUniform ("g_Texture0", 0);
    this->addUniform ("g_Texture1", 1);
    this->addUniform ("g_Texture2", 2);
    this->addUniform ("g_Texture3", 3);
    this->addUniform ("g_Texture4", 4);
    this->addUniform ("g_Texture5", 5);
    this->addUniform ("g_Texture6", 6);
    this->addUniform ("g_Texture7", 7);
    this->addUniform ("g_Texture8", 8);
    this->addUniform ("g_Texture9", 9);
    this->addUniform (
	"g_TextureReductionScale", static_cast<float> (
	    this->m_renderable.getScene ().getTextureReduction ()
	)
    );

    // Wallpaper Engine initializes resolution uniforms even when the matching texture
    // slot is not bound. Workshop shaders sometimes use one of those spare uniforms
    // solely to build UVs for a mask stored in another slot (for example, an opacity
    // mask in g_Texture3 with UVs derived from g_Texture7Resolution). Leaving it at
    // OpenGL's zero default turns the ratio below into 0/0 and makes the mask sample
    // undefined. Identity dimensions preserve the authored UVs; resolved textures
    // overwrite their own slots below.
    for (int index = 0; index < 10; index++) {
	this->addUniform ("g_Texture" + std::to_string (index) + "Resolution", glm::vec4 (1.0f));
	this->addUniform ("g_Texture" + std::to_string (index) + "MipMapInfo", &this->m_textureMipMapCounts[index]);
    }

    this->m_texture0Resolution = texture ? *texture->getResolution () : glm::vec4 (1.0f);
    this->addUniform ("g_Texture0Resolution", &this->m_texture0Resolution);

    const auto registerTexelUniform = [this] (const int index, const TextureProvider& provider) {
	const float width = static_cast<float> (glm::max (provider.getTextureWidth (0), 1u));
	const float height = static_cast<float> (glm::max (provider.getTextureHeight (0), 1u));
	this->m_textureTexels.insert_or_assign (index, glm::vec4 (1.0f / width, 1.0f / height, width, height));
	this->addUniform ("g_Texture" + std::to_string (index) + "Texel", &this->m_textureTexels.at (index));
    };
    if (texture) registerTexelUniform (0, *texture);

    for (const auto& [textureIndex, expectedTexture] : this->m_textures) {
	std::ostringstream namestream;

	namestream << "g_Texture" << textureIndex << "Resolution";

	texture = this->resolveTexture (expectedTexture->texture, textureIndex, texture);
	if (!texture) continue;
	if (m_effectFBOBindings && m_binds.contains (textureIndex) && m_binds.at (textureIndex) != "previous") {
	    m_effectTextureResolutions[textureIndex] = *texture->getResolution ();
	    this->addUniform (namestream.str (), &m_effectTextureResolutions.at (textureIndex));
	} else {
	    this->addUniform (namestream.str (), texture->getResolution ());
	}
	registerTexelUniform (textureIndex, *texture);
    }

    this->addUniform ("g_Texture0Resolution", &this->m_texture0Resolution);

    // pin every referenced texture so video-backed effect inputs actually decode
    this->adjustTextureUsageCounts (true);
}

void CPass::adjustTextureUsageCounts (const bool increment) const {
    // idempotent: holds exactly one pin no matter how many times setup runs
    if (increment == this->m_texturesUsageCounted) {
	return;
    }

    this->m_texturesUsageCounted = increment;

    for (const auto& [index, chain] : this->m_textures) {
	for (auto cur = chain.get (); cur != nullptr; cur = cur->next.get ()) {
	    if (cur->texture == nullptr) {
		continue;
	    }

	    if (increment) {
		cur->texture->incrementUsageCount ();
	    } else {
		cur->texture->decrementUsageCount ();
	    }
	}
    }
}

void CPass::setupUniforms () {
    this->setupTextureUniforms ();
    this->addUniform ("lwe_SingleSampleCoverageHeight", &this->m_singleSampleCoverageHeight);

    const auto& renderable = this->m_renderable;
    const auto& scene = this->m_renderable.getScene ();
    const auto& sceneData = this->m_renderable.getScene ().getScene ();
    const auto& recorder = this->m_renderable.getScene ().getAudioContext ().getRecorder ();

    // lighting variables
    const auto& ambient = *sceneData.colors.ambient;
    m_animatedUniforms.erase ("g_LightAmbientColor");
    if (ambient.animation != nullptr) {
        auto [entry, inserted] = m_animatedUniforms.emplace ("g_LightAmbientColor", AnimatedUniform {
            .setting = &ambient, .sampled = std::make_unique<DynamicValue> (*ambient.value)
        });
        this->addUniform ("g_LightAmbientColor", &entry->second.sampled->getVec3 ());
    } else {
        this->addUniform ("g_LightAmbientColor", &ambient.value->getVec3 ());
    }
    this->addUniform ("g_LightSkylightColor", &sceneData.colors.skylight->value->getVec3 ());
    this->addUniform ("g_EyePosition", &scene.getCamera ().getEye ());

    const auto& fog = scene.getFog ();
    this->addUniform ("g_FogDistanceColor", &sceneData.fog.distance.color->value->getVec3 ());
    this->addUniform ("g_FogDistanceParams", &fog.distanceParams);
    this->addUniform ("g_FogHeightColor", &sceneData.fog.height.color->value->getVec3 ());
    this->addUniform ("g_FogHeightParams", &fog.heightParams);

    // dynamic light state, refreshed by CScene::updateLightState every frame
    const auto& lights = scene.getLights ();

    this->addUniform ("g_LightsPosition", UniformType::Vector3, lights.legacyPositions.data (), 4);
    this->addUniform ("g_LightsColorRadius", lights.legacyColors.data (), 4);
    this->addUniform ("g_LightsColorPremultiplied", lights.legacyPremultipliedColors.data (), 3);

    if (lights.directionalCount > 0) {
	this->addUniform ("g_LDirectional_Direction", lights.directionalDirections.data (), lights.directionalCount);
	this->addUniform ("g_LDirectional_Color", lights.directionalColors.data (), lights.directionalCount);
	if (lights.directionalShadowCount > 0) {
	    this->addUniform (
		"g_LDirectional_ShadowEnabled", lights.directionalShadowEnabled.data (), lights.directionalCount
	    );
	}
    }

    if (lights.pointCount > 0) {
	this->addUniform ("g_LPoint_Origin", lights.pointOrigins.data (), lights.pointCount);
	this->addUniform ("g_LPoint_Color", lights.pointColors.data (), lights.pointCount);
	if (lights.pointShadowCount > 0) {
	    this->addUniform (
		"g_LFeature_ShadowPointProjection", lights.pointShadowProjections.data (), lights.pointCount
	    );
	    this->addUniform (
		"g_LFeature_ShadowPointProjectionTransform", lights.pointShadowTransforms.data (),
		lights.pointCount
	    );
	    this->addUniform ("g_LPoint_ShadowEnabled", lights.pointShadowEnabled.data (), lights.pointCount);
	}
    }

    if (lights.spotCount > 0) {
	this->addUniform ("g_LSpot_Origin", lights.spotOrigins.data (), lights.spotCount);
	this->addUniform ("g_LSpot_Direction", lights.spotDirections.data (), lights.spotCount);
	this->addUniform ("g_LSpot_Color", lights.spotColors.data (), lights.spotCount);
	this->addUniform ("g_LSpot_Exponent", lights.spotExponents.data (), lights.spotCount);
	if (lights.spotCookieMask != 0) {
	    this->addUniform ("g_LSpot_CookieProjection", lights.spotCookieMatrices.data (), lights.spotCount);
	}
	if (lights.spotShadowCount > 0) {
	    this->addUniform ("g_LSpot_ShadowEnabled", lights.spotShadowEnabled.data (), lights.spotCount);
	}
    }

    if (lights.shadowFeatureCount > 0) {
	this->addUniform (
	    "g_LFeature_ShadowProjection", lights.shadowMatrices.data (), lights.shadowFeatureCount
	);
	this->addUniform (
	    "g_LFeature_ShadowProjectionTransform", lights.shadowTransforms.data (), lights.shadowFeatureCount
	);
    }

    if (lights.tubeCount > 0) {
	this->addUniform ("g_LTube_OriginA", lights.tubeOriginsA.data (), lights.tubeCount);
	this->addUniform ("g_LTube_OriginB", lights.tubeOriginsB.data (), lights.tubeCount);
	this->addUniform ("g_LTube_Color", lights.tubeColors.data (), lights.tubeCount);
    }
    // register variables like brightness and alpha by pointer so the values are re-read every
    // frame: they can be driven by user-property bindings (e.g. alpha bound to a "brightness"
    // slider) that are applied after pass construction, so a by-value copy here would freeze the
    // pre-binding defaults (alpha = 1.0) and render e.g. darkening overlays fully opaque
    // Effect/pass constants are the highest-priority source. Custom material shaders can map an
    // authored constant onto a generic object uniform, so do not replace those values with object
    // defaults. Passing Breeze (Workshop 2244339517), for example, maps "Bright" onto g_Brightness:
    // replacing its authored 5.5/10 values with the model default of 1 turns its pow(..., 4) palm
    // materials nearly black and keeps the pink triangle below the bloom threshold.
    if (!this->m_constantUniforms.contains ("g_Brightness")) {
	this->addUniform ("g_Brightness", &renderable.getBrightness ());
    }
    // Opacity effects similarly map their authored "alpha" constant onto g_UserAlpha; replacing
    // that pointer with the object's static alpha makes scripted music overlays stay permanently
    // opaque (Legendaries of Hoenn, Workshop 3101147701).
    if (!this->m_constantUniforms.contains ("g_UserAlpha")) {
	this->addUniform ("g_UserAlpha", &renderable.getUserAlpha ());
    }
    this->addUniform ("g_Alpha", &renderable.getAlpha ());
    this->addUniform ("g_Color", &renderable.getColor ());
    this->m_effectiveColor4 = renderable.getColor4 ();
    this->m_effectiveColor4.a *= renderable.getUserAlpha ();
    this->addUniform ("g_Color4", &this->m_effectiveColor4);
    if (!this->m_uniforms.contains ("g_CompositeColor")) {
	this->addUniform ("g_CompositeColor", &renderable.getCompositeColor ());
    }
    // add some external variables
    this->addUniform ("g_Time", &this->m_sceneTime);
    this->addUniform ("g_Frametime", &this->m_frameTime);
    this->addUniform ("g_Daytime", &g_Daytime);
    // add model-view-projection matrix
    this->addUniform ("g_ModelViewProjectionMatrixInverse", &this->m_modelViewProjectionMatrixInverse);
    this->addUniform ("g_ModelViewProjectionMatrix", &this->m_modelViewProjectionMatrix);
    this->addUniform ("g_EffectModelViewProjectionMatrix", &this->m_modelViewProjectionMatrix);
    this->addUniform ("g_ModelMatrix", &this->m_modelMatrix);
    this->addUniform ("g_LayerModelMatrix", &this->m_layerModelMatrix);
    this->m_usesLayerModelMatrix = this->m_uniforms.contains ("g_LayerModelMatrix");
    this->addUniform ("g_EffectModelMatrix", &this->m_modelMatrix);
    this->addUniform ("g_NormalModelMatrix", glm::identity<glm::mat3> ());
    this->addUniform ("g_ViewProjectionMatrix", &this->m_viewProjectionMatrix);
    this->addUniform ("g_PointerPosition", scene.getMousePosition ());
    this->addUniform ("g_PointerPositionLast", scene.getMousePositionLast ());
    this->addUniform ("g_ParallaxPosition", scene.getParallaxPosition ());
    this->addUniform ("g_EffectTextureProjectionMatrix", &this->m_effectTextureProjectionMatrix);
    this->addUniform ("g_EffectTextureProjectionMatrixInverse", &this->m_effectTextureProjectionMatrixInverse);
    this->addUniform ("g_TexelSize", &this->m_sceneTexelSize);
    this->addUniform ("g_TexelSizeHalf", &this->m_sceneTexelSizeHalf);
    this->addUniform ("g_Screen", &this->m_screen);
    this->addUniform ("g_AudioSpectrum16Left", recorder.audio16Left, 16);
    this->addUniform ("g_AudioSpectrum16Right", recorder.audio16Right, 16);
    this->addUniform ("g_AudioSpectrum32Left", recorder.audio32Left, 32);
    this->addUniform ("g_AudioSpectrum32Right", recorder.audio32Right, 32);
    this->addUniform ("g_AudioSpectrum64Left", recorder.audio64Left, 64);
    this->addUniform ("g_AudioSpectrum64Right", recorder.audio64Right, 64);
}

void CPass::addAttribute (const std::string& name, GLint type, GLint elements, const GLuint* value) {
    const GLint id = glGetAttribLocation (this->m_programID, name.c_str ());

    if (id == -1) {
	return;
    }

    this->m_attribs.emplace_back (std::make_unique<AttribEntry> (id, name, type, elements, value));
}

template <typename T> void CPass::addUniform (const std::string& name, UniformType type, T value) {
    GLint id = glGetUniformLocation (this->m_programID, name.c_str ());

    // parameter not found, can be ignored
    if (id == -1) {
	return;
    }
    this->leaveProgramSharing ();

    // build a copy of the value and allocate it somewhere
    auto newValue = std::make_shared<T> (value);

    // uniform found, add it to the list
    this->m_uniformBindingsDirty = true;
    this->m_referenceUniforms.erase (name);
    this->m_uniforms.insert_or_assign (
	name, std::make_unique<UniformEntry> (id, name, type, std::move (newValue), 1)
    );
}

template <typename T> void CPass::addUniform (const std::string& name, UniformType type, T* value, int count) {
    if (this->m_shaderInitialized) {
	this->m_externalUniforms.insert_or_assign (name, ExternalUniform { type, value, count });
    }
    // this version is used to reference to system variables so things like g_Time works fine
    GLint id = glGetUniformLocation (this->m_programID, name.c_str ());

    // parameter not found, can be ignored
    if (id == -1) {
	return;
    }
    this->leaveProgramSharing ();

    // uniform found, add it to the list
    this->m_uniformBindingsDirty = true;
    this->m_referenceUniforms.erase (name);
    this->m_uniforms.insert_or_assign (name, std::make_unique<UniformEntry> (id, name, type, value, count));
}

template <typename T> void CPass::addUniform (const std::string& name, UniformType type, T** value) {
    // this version is used to reference to system variables so things like g_Time works fine
    const GLint id = glGetUniformLocation (this->m_programID, name.c_str ());

    // parameter not found, can be ignored
    if (id == -1) {
	return;
    }
    this->leaveProgramSharing ();

    // uniform found, add it to the list
    this->m_uniformBindingsDirty = true;
    this->m_uniforms.erase (name);
    this->m_referenceUniforms.insert_or_assign (
	name, std::make_unique<ReferenceUniformEntry> (
		  id, name, type, reinterpret_cast<const void**> (value)
	      )
    );
}

void CPass::setupShaderVariables () {
    for (const auto& cur : this->m_shader->getVertex ().getParameters ()) {
	if (!this->m_uniforms.contains (cur->getName ())) {
	    this->addUniform (cur);
	}
    }

    for (const auto& cur : this->m_shader->getFragment ().getParameters ()) {
	if (!this->m_uniforms.contains (cur->getName ())) {
	    this->addUniform (cur);
	}
    }

    // apply material pass constants (e.g. constantshadervalues from the material JSON)
    for (const auto& [name, value] : this->m_pass.constants) {
	const auto [vertex, fragment] = this->m_shader->findParameter (name);

	if (vertex == nullptr && fragment == nullptr) {
	    continue;
	}

	ShaderVariable* var = vertex == nullptr ? fragment : vertex;
	this->addUniform (var, *value);
	this->m_constantUniforms.emplace (var->getName ());
    }

    // apply override constants (highest priority, overrides both defaults and pass constants)
    for (const auto& [name, value] : this->m_override.constants) {
	const auto [vertex, fragment] = this->m_shader->findParameter (name);

	if (vertex == nullptr && fragment == nullptr) {
	    continue;
	}

	ShaderVariable* var = vertex == nullptr ? fragment : vertex;
	this->addUniform (var, *value);
	this->m_constantUniforms.emplace (var->getName ());
    }
}

// define some basic methods for the template
void CPass::addUniform (const ShaderVariable* value, const UserSetting& setting) {
    m_animatedUniforms.erase (value->getName ());
    if (setting.animation == nullptr) {
	addUniform (value, setting.value.get ());
	return;
    }
    auto [entry, inserted] = m_animatedUniforms.emplace (value->getName (), AnimatedUniform {
	.setting = &setting, .sampled = std::make_unique<DynamicValue> (*setting.value)
    });
    addUniform (value, entry->second.sampled.get ());
}

void CPass::addUniform (ShaderVariable* value) {
    // no need to re-implement this, call the version that takes a CDynamicValue as second parameter
    // and that handles casting and everything
    this->addUniform (value, value);
}

void CPass::addUniform (const ShaderVariable* value, const DynamicValue* setting) {
    if (value->is<ShaderVariableFloat> ()) {
	this->addUniform (value->getName (), &setting->getFloat ());
    } else if (value->is<ShaderVariableInteger> ()) {
	this->addUniform (value->getName (), &setting->getInt ());
    } else if (value->is<ShaderVariableVector2> ()) {
	this->addUniform (value->getName (), &setting->getVec2 ());
    } else if (value->is<ShaderVariableVector3> ()) {
	this->addUniform (value->getName (), &setting->getVec3 ());
    } else if (value->is<ShaderVariableVector4> ()) {
	this->addUniform (value->getName (), &setting->getVec4 ());
    } else {
	sLog.error ("Cannot convert setting dynamic value  to ", value->getName (), ". Using default value");
    }
}

void CPass::addUniform (const std::string& name, int value) { this->addUniform (name, UniformType::Integer, value); }

void CPass::addUniform (const std::string& name, const int* value, int count) {
    this->addUniform (name, UniformType::Integer, value, count);
}

void CPass::addUniform (const std::string& name, const int** value) {
    this->addUniform (name, UniformType::Integer, value);
}

void CPass::addUniform (const std::string& name, double value) { this->addUniform (name, UniformType::Double, value); }

void CPass::addUniform (const std::string& name, const double* value, int count) {
    this->addUniform (name, UniformType::Double, value, count);
}

void CPass::addUniform (const std::string& name, const double** value) {
    this->addUniform (name, UniformType::Double, value);
}

void CPass::addUniform (const std::string& name, float value) { this->addUniform (name, UniformType::Float, value); }

void CPass::addUniform (const std::string& name, const float* value, int count) {
    this->addUniform (name, UniformType::Float, value, count);
}

void CPass::addUniform (const std::string& name, const float** value) {
    this->addUniform (name, UniformType::Float, value);
}

void CPass::addUniform (const std::string& name, glm::vec2 value) {
    this->addUniform (name, UniformType::Vector2, value);
}

void CPass::addUniform (const std::string& name, const glm::vec2* value) {
    this->addUniform (name, UniformType::Vector2, value, 1);
}

void CPass::addUniform (const std::string& name, const glm::vec2** value) {
    this->addUniform (name, UniformType::Vector2, value, 1);
}

void CPass::addUniform (const std::string& name, glm::vec3 value) {
    this->addUniform (name, UniformType::Vector3, value);
}

void CPass::addUniform (const std::string& name, const glm::vec3* value) {
    this->addUniform (name, UniformType::Vector3, value, 1);
}

void CPass::addUniform (const std::string& name, const glm::vec3** value) {
    this->addUniform (name, UniformType::Vector3, value);
}

void CPass::addUniform (const std::string& name, const glm::vec4 value) {
    this->addUniform (name, UniformType::Vector4, value);
}

void CPass::addUniform (const std::string& name, const glm::vec4* value, int count) {
    this->addUniform (name, UniformType::Vector4, value, count);
}

void CPass::addUniform (const std::string& name, const glm::vec4** value) {
    this->addUniform (name, UniformType::Vector4, value);
}

void CPass::addUniform (const std::string& name, const glm::mat3& value) {
    this->addUniform (name, UniformType::Matrix3, value);
}

void CPass::addUniform (const std::string& name, const glm::mat3* value) {
    this->addUniform (name, UniformType::Matrix3, value, 1);
}

void CPass::addUniform (const std::string& name, const glm::mat3** value) {
    this->addUniform (name, UniformType::Matrix3, value);
}

void CPass::addUniform (const std::string& name, const glm::mat4 value) {
    this->addUniform (name, UniformType::Matrix4, value);
}

void CPass::addUniform (const std::string& name, const glm::mat4* value, int count) {
    this->addUniform (name, UniformType::Matrix4, value, count);
}

void CPass::addUniform (const std::string& name, const glm::mat4x3* value, int count) {
    this->addUniform (name, UniformType::Matrix4x3, value, count);
}

void CPass::addUniform (const std::string& name, const glm::mat4** value) {
    this->addUniform (name, UniformType::Matrix4, value);
}
