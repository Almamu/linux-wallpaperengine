#pragma once

#include "WallpaperEngine/Audio/AudioStream.h"
#include "WallpaperEngine/Render/CWallpaper.h"
#include "WallpaperEngine/VideoPlayback/MPV/GLPlayer.h"

#include <map>

namespace WallpaperEngine::Render::Wallpapers {
using namespace WallpaperEngine::VideoPlayback::MPV;

class CVideo final : public CWallpaper {
public:
    CVideo (
	const Wallpaper& wallpaper, RenderContext& context, AudioContext& audioContext,
	const WallpaperState::TextureUVsScaling& scalingMode, const uint32_t& clampMode
    );

    ~CVideo () override;

    const Data::Model::Video& getVideo () const;
    [[nodiscard]] bool isReadyForScreenshot () const;

    [[nodiscard]] int getWidth () const override;
    [[nodiscard]] int getHeight () const override;

    [[nodiscard]] WallpaperState::TextureUVs getPresentationUVs () const override;

    void setPause (bool newState) override;
    [[nodiscard]] bool hasHDRSource () const override;
    void prepareHDRPresentation (bool enabled) override;
    bool setColorProperty (const std::string& key, const std::string& value);

protected:
    void renderFrame (const glm::ivec4& viewport) override;
    [[nodiscard]] glm::vec4 getColorCorrection () const override;
    [[nodiscard]] float getHDRSourceScale () const override;
    [[nodiscard]] std::pair<GLuint, float> getColorLut () const override;

    friend class CWallpaper;

private:
    void setupColorLut ();
    GLPlayerUniquePtr m_player;
    std::shared_ptr<const TextureProvider> m_colorLut;
    float m_colorLutStrength = 1.0f;
    bool m_colorLutDirty = false;

    bool m_muted = false;
    std::map<std::string, std::string> m_liveColorProperties;
};
} // namespace WallpaperEngine::Render::Wallpapers
