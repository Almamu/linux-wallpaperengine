#pragma once

#include <glm/vec4.hpp>
#include <map>
#include <string>

#include <X11/Xlib.h>

#include "COutput.h"
#include "WallpaperEngine/Render/Drivers/CVideoDriver.h"

namespace WallpaperEngine::Render::Drivers::Output {
class CX11Output final : public COutput {
  public:
    CX11Output (CApplicationContext& context, CVideoDriver& driver);
    ~CX11Output () override;

    void reset () override;

    bool renderVFlip () const override;
    bool renderMultiple () const override;
    bool haveImageBuffer () const override;
    void* getImageBuffer () const override;
    uint32_t getImageBufferSize () const override;
    void updateRender () const override;

  private:
    void loadScreenInfo ();
    void free ();

    Display* m_display;
    Pixmap m_pixmap;
    Window m_root;
    GC m_gc;
    char* m_imageData;
    uint32_t m_imageSize;
    XImage* m_image;
    std::vector<COutputViewport*> m_screens;
};
} // namespace WallpaperEngine::Render::Drivers::Output