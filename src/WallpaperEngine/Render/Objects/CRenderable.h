#pragma once
#include "WallpaperEngine/Render/CObject.h"
#include "WallpaperEngine/Render/FBOProvider.h"
#include "WallpaperEngine/Render/Objects/Effects/CPass.h"
#include "WallpaperEngine/Render/Wallpapers/CScene.h"

#include "WallpaperEngine/Render/Shaders/Shader.h"

#include <cstddef>

using namespace WallpaperEngine;
using namespace WallpaperEngine::Render;

namespace WallpaperEngine::Render::Objects {
class CRenderable : virtual public CObject, public FBOProvider {
    friend CObject;

public:
    CRenderable (Wallpapers::CScene& scene, const Object& object, const Material& material);

    [[nodiscard]] std::shared_ptr<const TextureProvider> getTexture () const;
    /** Original layer transform in authored coordinates, independent of the current effect pass. */
    [[nodiscard]] virtual glm::mat4 resolveLayerModelMatrix () const;

    [[nodiscard]] double getAnimationTime () const;

    /** SceneScript texture-animation controller state for this layer's albedo texture. */
    [[nodiscard]] bool hasTextureAnimation () const;
    [[nodiscard]] size_t getTextureAnimationFrameCount () const;
    [[nodiscard]] double getTextureAnimationDuration () const;
    [[nodiscard]] float getTextureAnimationRate () const;
    void setTextureAnimationRate (float rate);
    void playTextureAnimation ();
    void pauseTextureAnimation ();
    void stopTextureAnimation ();
    [[nodiscard]] bool isTextureAnimationPlaying () const;
    [[nodiscard]] size_t getTextureAnimationFrame () const;
    void setTextureAnimationFrame (size_t frame);
    void joinTextureAnimation ();

    /** Playback time used by material passes. Non-albedo animated textures stay on
     *  Wallpaper Engine's shared render timer. */
    [[nodiscard]] double getTextureAnimationTime (const std::shared_ptr<const TextureProvider>& texture) const;

    void setup () override;
    virtual void rebaseRenderTime (double offset);

    [[nodiscard]] virtual const float& getBrightness () const = 0;
    [[nodiscard]] virtual const float& getUserAlpha () const = 0;
    [[nodiscard]] virtual const float& getAlpha () const = 0;
    [[nodiscard]] virtual const glm::vec3& getColor () const = 0;
    [[nodiscard]] virtual const glm::vec4& getColor4 () const = 0;
    [[nodiscard]] virtual const glm::vec3& getCompositeColor () const = 0;

protected:
    void detectTexture ();
    void detectTexture (const Material& material);

    double m_animationTime = 0.0;

    struct TextureAnimationPlayback {
	enum class Clock { Driver, Scene, Detached };
	Clock clock = Clock::Driver;
	bool playing = true;
	float rate = 1.0f;
	double anchorTime = 0.0;
	double anchorRenderTime = 0.0;
    };

    [[nodiscard]] double getTextureAnimationRenderTime () const;
    [[nodiscard]] double getControlledTextureAnimationTime () const;
    void detachTextureAnimationClock ();

    TextureAnimationPlayback m_textureAnimationPlayback = {};

    std::shared_ptr<const TextureProvider> m_texture = nullptr;
    const Material& m_material;
};
}
