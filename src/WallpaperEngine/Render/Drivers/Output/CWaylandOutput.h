#pragma once

#ifdef ENABLE_WAYLAND

#include <glm/vec4.hpp>
#include <map>
#include <string>

#include "COutput.h"
#include "WallpaperEngine/Render/Drivers/CVideoDriver.h"

namespace WallpaperEngine::Render::Drivers {
class CWaylandOpenGLDriver;

namespace Output {
class CWaylandOutput final : public COutput {
  public:
    CWaylandOutput (CApplicationContext& context, CWaylandOpenGLDriver& driver);
    ~CWaylandOutput () override;

    void reset () override;

    bool renderVFlip () const override;
    bool renderMultiple () const override;
    bool haveImageBuffer () const override;
    void* getImageBuffer () const override;
    uint32_t getImageBufferSize () const override;
    void updateRender () const override;

  private:
    void updateViewports ();
};
} // namespace Output
} // namespace WallpaperEngine::Render::Drivers
#endif /* ENABLE_WAYLAND */