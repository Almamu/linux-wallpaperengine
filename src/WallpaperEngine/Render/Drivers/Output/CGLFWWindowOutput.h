#pragma once

#include "COutput.h"
#include "WallpaperEngine/Render/Drivers/CVideoDriver.h"

namespace WallpaperEngine::Render::Drivers::Output {
class CGLFWWindowOutput final : public COutput {
  public:
    CGLFWWindowOutput (CApplicationContext& context, CVideoDriver& driver);

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