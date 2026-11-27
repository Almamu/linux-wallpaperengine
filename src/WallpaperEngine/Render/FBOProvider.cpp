#include "FBOProvider.h"
#include <algorithm>
#include <cmath>
#include <gmpxx.h>
#include <sstream>
#include <utility>

using namespace WallpaperEngine::Render;
using namespace WallpaperEngine::Data::Model;

FBOProvider::FBOProvider (const FBOProvider* parent) :
    m_parent (parent), m_renderScale (parent != nullptr ? parent->m_renderScale : 1.0f) { }

bool FBOProvider::isFixedSizeTarget (const std::string_view name) {
    // These sizes are part of the renderer protocol rather than scene resolution.
    // Shadow viewport transforms are expressed against SHADOW_ATLAS_SIZE, and a
    // light cookie alias retains the native diagnostic texture's initial dimensions.
    return name == "_rt_shadowAtlas" || name == "_alias_lightCookie";
}

glm::uvec2 FBOProvider::calculateTargetSize (
    const glm::vec2 size, const float renderScale, const bool scalable, const int fit
) {
    const float scale = scalable ? std::clamp (renderScale, 0.5f, 2.0f) : 1.0f;
    glm::uvec2 result {
	static_cast<uint32_t> (std::max (1.0f, std::round (size.x * scale))),
	static_cast<uint32_t> (std::max (1.0f, std::round (size.y * scale))),
    };
    // wallpaper64.exe caps the longer edge without upscaling small sources,
    // then truncates the proportional shorter edge. Apply after scene scaling:
    // a fixed-budget simulation must stay capped under supersampling too.
    const auto longest = std::max (result.x, result.y);
    if (fit > 0 && static_cast<uint32_t> (fit) < longest) {
	result = glm::max (glm::uvec2 (glm::vec2 (result) * (static_cast<float> (fit) / longest)), glm::uvec2 (1));
    }
    return result;
}

glm::uvec2 FBOProvider::calculateEffectTargetExtent (const FBO& base, const glm::vec2 size, const float renderScale) {
    auto result = calculateTargetSize (size, renderScale, !isFixedSizeTarget (base.name));
    if (base.width <= 4096) result.x = base.width;
    if (base.height <= 4096) result.y = base.height;
    const auto longest = std::max (result.x, result.y);
    if (base.fit <= 4096 && static_cast<uint32_t> (base.fit) < longest) {
	result = glm::uvec2 (glm::vec2 (result) * (static_cast<float> (base.fit) / longest));
    }
    return result;
}

glm::uvec2 FBOProvider::calculateEffectTargetSize (const FBO& base, const glm::vec2 size, const float renderScale) {
    // Native overrides each source extent before fit, then divides by target scale.
    const auto extent = calculateEffectTargetExtent (base, size, renderScale);
    return glm::max (glm::uvec2 (glm::vec2 (extent) / base.scale), glm::uvec2 (2));
}

void FBOProvider::setRenderScale (const float scale) { this->m_renderScale = std::clamp (scale, 0.5f, 2.0f); }

float FBOProvider::getRenderScale () const { return this->m_renderScale; }

void FBOProvider::setBackbufferFormat (const TextureFormat format) { this->m_backbufferFormat = format; }

TextureFormat FBOProvider::getBackbufferFormat () const {
    return this->m_parent != nullptr ? this->m_parent->getBackbufferFormat () : this->m_backbufferFormat;
}

TextureFormat FBOProvider::resolveTargetFormat (const std::string_view format, const TextureFormat backbufferFormat) {
    if (format == "r8") return TextureFormat_R8;
    if (format == "rg1616f") return TextureFormat_RG1616f;
    if (format == "r16f") return TextureFormat_R16f;
    if (format == "rgba_backbuffer") return backbufferFormat;
    return (format == "rgba16161616f" || format == "rgba16f")
	? TextureFormat_RGBA16161616f : TextureFormat_ARGB8888;
}

std::shared_ptr<CFBO> FBOProvider::create (const FBO& base, const glm::vec2 size, const bool fullscreen) {
    const auto targetSize = calculateEffectTargetSize (base, size, this->m_renderScale);
    // Native declared targets have their own linear sampler, independent of the
    // image reference's interpolation and wrapping flags.
    const auto flags = base.repeat ? TextureFlags_NoFlags : TextureFlags_ClampUVs;
    const auto format = resolveTargetFormat (base.format, this->getBackbufferFormat ());
    std::shared_ptr<CFBO> target;
    const auto* scene = this;
    while (scene->m_parent != nullptr) scene = scene->m_parent;
    std::string key;
    if (!base.unique) {
	// Native ordinary targets share by name, unscaled extent and scale. Fullscreen
	// targets use their plain name and keep the same storage when resized.
	key = base.name;
	if (!fullscreen) {
	    const auto extent = calculateEffectTargetExtent (base, size, this->m_renderScale);
	    std::ostringstream suffix;
	    suffix << '_' << extent.x << '_' << extent.y << '_' << base.scale;
	    key += suffix.str ();
	}
	std::erase_if (scene->m_sharedTargets, [] (const auto& entry) { return entry.second.expired (); });
	if (const auto found = scene->m_sharedTargets.find (key); found != scene->m_sharedTargets.end ()) {
	    target = found->second.lock ();
	}
    }
    if (target == nullptr) {
	target = std::make_shared<CFBO> (
	    base.name, format, flags, base.scale, targetSize.x, targetSize.y, targetSize.x, targetSize.y
	);
	if (!base.unique) scene->m_sharedTargets[key] = target;
    }
    this->m_fbos[base.name] = target;
    // Clearing is an effect declaration action, including when storage is shared.
    if (base.clearOnCreate) target->clear (base.clear);
    return target;
}

std::shared_ptr<CFBO> FBOProvider::create (
    const std::string& name, TextureFormat format, uint32_t flags, float scale, glm::vec2 realSize,
    glm::vec2 textureSize, bool withDepthBuffer, bool depthTexture, uint32_t samples, bool reverseDepth
) {
    const bool scalable = !isFixedSizeTarget (name);
    const auto scaledRealSize = calculateTargetSize (realSize, this->m_renderScale, scalable);
    const auto scaledTextureSize = calculateTargetSize (textureSize, this->m_renderScale, scalable);
    auto target = std::make_shared<CFBO> (
	name, format, flags, scale, scaledRealSize.x, scaledRealSize.y, scaledTextureSize.x,
	scaledTextureSize.y, withDepthBuffer, depthTexture, samples, reverseDepth
    );
    this->m_fbos[name] = target;
    return target;
}

std::shared_ptr<CFBO> FBOProvider::alias (const std::string& newName, const std::string& original) {
    return this->alias (newName, this->find (original));
}

std::shared_ptr<CFBO> FBOProvider::alias (const std::string& newName, std::shared_ptr<CFBO> original) {
    this->m_fbos[newName] = original;
    return original;
}

void FBOProvider::publish (const std::string& name, const std::shared_ptr<CFBO>& target) {
    this->m_fbos[name] = std::weak_ptr<CFBO> (target);
}

void FBOProvider::pruneExpiredTargets () {
    std::erase_if (this->m_fbos, [] (const auto& entry) {
	const auto* published = std::get_if<std::weak_ptr<CFBO>> (&entry.second);
	return published != nullptr && published->expired ();
    });
    std::erase_if (this->m_sharedTargets, [] (const auto& entry) { return entry.second.expired (); });
}

std::shared_ptr<CFBO> FBOProvider::find (const std::string& name) const {
    if (const auto it = this->m_fbos.find (name); it != this->m_fbos.end ()) {
	if (const auto* owned = std::get_if<std::shared_ptr<CFBO>> (&it->second)) return *owned;
	if (auto target = std::get<std::weak_ptr<CFBO>> (it->second).lock ()) return target;
    }

    if (this->m_parent == nullptr) {
	return nullptr;
    }

    return this->m_parent->find (name);
}
