#pragma once

#include "Output.h"
#include "WallpaperEngine/Render/Drivers/VideoDriver.h"

namespace WallpaperEngine::Render::Drivers {
class EmbeddedHost;
}

namespace WallpaperEngine::Render::Drivers::Output {
/**
 * Output that targets the framebuffer owned by an embedding host.
 *
 * There is exactly one viewport covering the whole framebuffer: the host
 * instantiates one wallpaper per screen, so screen splitting is the host's job
 * rather than ours.
 */
class EmbeddedOutput final : public Output {
public:
    EmbeddedOutput (ApplicationContext& context, VideoDriver& driver, const EmbeddedHost& host);

    void reset () override;
    bool renderVFlip () const override;
    bool renderMultiple () const override;
    bool haveImageBuffer () const override;
    void* getImageBuffer () const override;
    uint32_t getImageBufferSize () const override;
    void updateRender () const override;

private:
    const EmbeddedHost& m_host;
};
} // namespace WallpaperEngine::Render::Drivers::Output
