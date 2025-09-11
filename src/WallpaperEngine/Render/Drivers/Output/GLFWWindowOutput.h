#pragma once

#include "Output.h"
#include "WallpaperEngine/Render/Drivers/VideoDriver.h"

namespace WallpaperEngine::Render::Drivers::Output {
class GLFWWindowOutput final : public Output {
  public:
    GLFWWindowOutput (ApplicationContext& context, VideoDriver& driver);

    void reset () override;
    bool renderVFlip () const override;
    bool renderMultiple () const override;
    bool haveImageBuffer () const override;
    void* getImageBuffer () const override;
    uint32_t getImageBufferSize () const override;
    void updateRender () const override;

  private:
    void repositionWindow ();
};
} // namespace WallpaperEngine::Render::Drivers::Output