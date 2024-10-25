#pragma once

#ifdef ENABLE_WAYLAND

#include <glm/vec4.hpp>

#include "CFullScreenDetector.h"

struct wl_display;
struct wl_registry;
struct zwlr_foreign_toplevel_manager_v1;

namespace WallpaperEngine::Render::Drivers::Detectors {
class CWaylandFullScreenDetector final : public CFullScreenDetector {
  public:
    explicit CWaylandFullScreenDetector (Application::CApplicationContext& appContext);
    ~CWaylandFullScreenDetector () override;

    [[nodiscard]] bool anythingFullscreen () const override;
    void reset () override;

  private:
    wl_display* m_display = nullptr;
    zwlr_foreign_toplevel_manager_v1* m_toplevelManager = nullptr;

    uint32_t m_fullscreenCount = 0;

    friend void handleGlobal (void* data, struct wl_registry* registry, uint32_t name, const char* interface,
                              uint32_t version);
};
} // namespace WallpaperEngine::Render::Drivers::Detectors

#endif /* ENABLE_WAYLAND */
