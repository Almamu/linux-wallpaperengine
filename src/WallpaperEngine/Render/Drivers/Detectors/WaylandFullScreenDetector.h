#pragma once

#ifdef ENABLE_WAYLAND

#include <glm/vec4.hpp>
#include <map>
#include <set>
#include <string>

#include "FullScreenDetector.h"

struct wl_display;
struct wl_registry;
struct wl_output;
struct zwlr_foreign_toplevel_manager_v1;

namespace WallpaperEngine::Render::Drivers::Detectors {

class WaylandFullScreenDetector;
struct WaylandFullscreenToplevel;

struct WaylandFullscreenDetectorCallbackData {
    WaylandFullScreenDetector* detector;
    uint32_t* fullscreenCount;
    std::set<WaylandFullscreenToplevel*>* toplevels;
};

struct WaylandFullscreenDetectorOutput {
    uint32_t globalName;
    std::string name;
};

class WaylandFullScreenDetector final : public FullScreenDetector {
public:
    explicit WaylandFullScreenDetector (Application::ApplicationContext& appContext);
    ~WaylandFullScreenDetector () override;

    [[nodiscard]] bool anythingFullscreen () const override;
    [[nodiscard]] bool canTellOutputsApart () const override;
    [[nodiscard]] bool isFullscreenOn (const std::string& output) const override;
    void reset () override;

private:
    wl_display* m_display = nullptr;
    zwlr_foreign_toplevel_manager_v1* m_toplevelManager = nullptr;
    uint32_t m_toplevelManagerName = 0;

    uint32_t m_fullscreenCount = 0;
    std::set<WaylandFullscreenToplevel*> m_toplevels {};
    /** Outputs bound on the detector's own connection, used to tell which output a toplevel is on */
    std::map<wl_output*, WaylandFullscreenDetectorOutput> m_outputs {};

    WaylandFullscreenDetectorCallbackData m_callbackData {};

    friend void
    handleGlobal (void* data, struct wl_registry* registry, uint32_t name, const char* interface, uint32_t version);
    friend void handleGlobalRemoved (void* data, struct wl_registry* registry, uint32_t id);
    friend void outputHandleName (void* data, struct wl_output* output, const char* name);
};
} // namespace WallpaperEngine::Render::Drivers::Detectors

#endif /* ENABLE_WAYLAND */
