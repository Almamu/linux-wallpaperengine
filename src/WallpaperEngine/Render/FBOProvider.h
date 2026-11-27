#pragma once

#include <glm/vec2.hpp>

#include <string_view>
#include <variant>

#include "CFBO.h"
#include "WallpaperEngine/Data/Model/Effect.h"

namespace WallpaperEngine::Render {
using namespace WallpaperEngine::Data::Model;

class FBOProvider {
public:
    explicit FBOProvider (const FBOProvider* parent);

    std::shared_ptr<CFBO> create (const FBO& base, glm::vec2 size, bool fullscreen = false);
    std::shared_ptr<CFBO> create (
	const std::string& name, TextureFormat format, uint32_t flags, float scale, glm::vec2 realSize,
	glm::vec2 textureSize, bool withDepthBuffer = false, bool depthTexture = false, uint32_t samples = 1, bool reverseDepth = false
    );
    std::shared_ptr<CFBO> alias (const std::string& newName, const std::string& original);
    std::shared_ptr<CFBO> alias (const std::string& newName, std::shared_ptr<CFBO> original);
    /** Expose a layer-owned target to the scene without extending its lifetime. */
    void publish (const std::string& name, const std::shared_ptr<CFBO>& target);
    void pruneExpiredTargets ();
    [[nodiscard]] std::shared_ptr<CFBO> find (const std::string& name) const;

    /** Set the scene supersampling factor inherited by child effect providers. */
    void setRenderScale (float scale);
    [[nodiscard]] float getRenderScale () const;

    /** Scene color format used by rgba_backbuffer targets in child effect providers. */
    void setBackbufferFormat (TextureFormat format);
    [[nodiscard]] TextureFormat getBackbufferFormat () const;
    [[nodiscard]] static TextureFormat resolveTargetFormat (
	std::string_view format, TextureFormat backbufferFormat = TextureFormat_ARGB8888
    );

    /** Pure sizing helpers kept public so quality scaling can be regression-tested without GL. */
    [[nodiscard]] static bool isFixedSizeTarget (std::string_view name);
    [[nodiscard]] static glm::uvec2 calculateTargetSize (
	glm::vec2 size, float renderScale, bool scalable = true, int fit = 0
    );
    [[nodiscard]] static glm::uvec2 calculateEffectTargetExtent (
	const FBO& base, glm::vec2 size, float renderScale
    );
    [[nodiscard]] static glm::uvec2 calculateEffectTargetSize (
	const FBO& base, glm::vec2 size, float renderScale
    );

private:
    const FBOProvider* m_parent;
    float m_renderScale = 1.0f;
    TextureFormat m_backbufferFormat = TextureFormat_ARGB8888;
    std::map<std::string, std::variant<std::shared_ptr<CFBO>, std::weak_ptr<CFBO>>> m_fbos = {};
    /** Scene-local declared targets; physical storage belongs to its effect references. */
    mutable std::map<std::string, std::weak_ptr<CFBO>> m_sharedTargets = {};
};
}
