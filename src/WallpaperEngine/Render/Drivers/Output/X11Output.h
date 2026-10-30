#pragma once

#include <chrono>
#include <glm/vec4.hpp>
#include <map>
#include <string>

#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>
#include <X11/extensions/XShm.h>

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
    void discoverOutputs (XRRScreenResources* screenResources);
    void validateOutputs () const;
    void initX11Background ();
    void initImageBuffer (unsigned int depth);
    bool initShmImageBuffer (unsigned int depth);
    void initFallbackImageBuffer (unsigned int depth);
    void initPerOutputWindows ();
    void freePerOutputWindows ();
    void freeImageBuffer ();
    void freeRootPixmap ();
    void discardShmImageBuffer ();
    void free ();

    Display* m_display = nullptr;
    Pixmap m_pixmap = None;
    Window m_root = None;
    GC m_gc = None;
    Atom m_rootPixmapAtom = None;
    Atom m_esetrootPixmapAtom = None;
    int m_rootWidth = 0;
    int m_rootHeight = 0;
    int m_rootOffsetX = 0;
    int m_rootOffsetY = 0;
    bool m_usePerOutputWindows = false;
    bool m_useShm = false;
    mutable XShmSegmentInfo m_shmInfo = {};
    mutable std::chrono::steady_clock::time_point m_lastRootSync = {};
    std::map<std::string, Window> m_windows = {};
    std::map<std::string, GC> m_windowGCs = {};
    char* m_imageData = nullptr;
    uint32_t m_imageSize = 0;
    XImage* m_image = nullptr;
    std::vector<OutputViewport*> m_screens = {};
};
} // namespace WallpaperEngine::Render::Drivers::Output
