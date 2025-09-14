#pragma once

#include <glm/vec4.hpp>
#include <map>
#include <string>

#include <X11/Xlib.h>

#include "Output.h"
#include "WallpaperEngine/Render/Drivers/VideoDriver.h"

namespace WallpaperEngine::Render::Drivers::Output {
class X11Output final : public Output {
  public:
    X11Output (ApplicationContext& context, VideoDriver& driver);
    ~X11Output () override;

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

    Display* m_display = nullptr;
    Pixmap m_pixmap;
    Window m_root;
    GC m_gc;
    char* m_imageData = nullptr;
    uint32_t m_imageSize = 0;
    XImage* m_image = nullptr;
    std::vector<OutputViewport*> m_screens = {};
};
} // namespace WallpaperEngine::Render::Drivers::Output