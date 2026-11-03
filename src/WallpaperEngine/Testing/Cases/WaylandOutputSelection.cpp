#include <catch2/catch_test_macros.hpp>

#include "WallpaperEngine/Application/ApplicationContext.h"
#include "WallpaperEngine/Render/Drivers/Output/WaylandOutputSelection.h"

using WallpaperEngine::Application::ApplicationContext;
using namespace WallpaperEngine::Render::Drivers::Output::WaylandOutputSelection;

namespace {
const char* selectionArgv[] = { "linux-wallpaperengine" };

/** Stands in for WaylandOutputViewport: the fields the selection helpers read */
struct FakeScreen {
    uint32_t waylandName;
    bool initialized = false;
    bool pendingSetup = false;
};

ApplicationContext makeContext () { return ApplicationContext (1, const_cast<char**> (selectionArgv)); }
} // namespace

TEST_CASE ("Wayland outputs: --screen-root outputs are requested") {
    auto context = makeContext ();
    context.settings.general.screenBackgrounds["HDMI-A-1"] = "1234567890";

    CHECK (isOutputRequested ("HDMI-A-1", context));
    CHECK_FALSE (isOutputRequested ("DP-1", context));
    // output names are matched exactly
    CHECK_FALSE (isOutputRequested ("HDMI-A-10", context));
    CHECK_FALSE (isOutputRequested ("", context));
}

TEST_CASE ("Wayland outputs: span group members are requested") {
    auto context = makeContext ();
    context.settings.general.spanGroups.push_back ({ .screens = { "DP-1", "DP-2" }, .background = "1234567890" });

    CHECK (isOutputRequested ("DP-1", context));
    CHECK (isOutputRequested ("DP-2", context));
    CHECK_FALSE (isOutputRequested ("HDMI-A-1", context));
}

TEST_CASE ("Wayland outputs: nothing is requested without --screen-root or span groups") {
    const auto context = makeContext ();

    CHECK_FALSE (isOutputRequested ("HDMI-A-1", context));
}

TEST_CASE ("Wayland outputs: removed outputs are found by their registry name") {
    FakeScreen first { .waylandName = 41 };
    FakeScreen second { .waylandName = 57 };
    std::vector<FakeScreen*> screens = { &first, &second };

    const auto found = findByWaylandName (screens, 57);
    REQUIRE (found != screens.end ());
    CHECK (*found == &second);

    // registry names of other globals (seat, compositor...) match no output
    CHECK (findByWaylandName (screens, 3) == screens.end ());

    std::vector<FakeScreen*> none;
    CHECK (findByWaylandName (none, 41) == none.end ());
}

TEST_CASE ("Wayland outputs: a re-added output is set up only after wl_output.done") {
    // the output that was there at startup, plus the same monitor announced again after a power cycle
    FakeScreen startup { .waylandName = 41, .initialized = true, .pendingSetup = false };
    FakeScreen readded { .waylandName = 63, .initialized = false, .pendingSetup = true };
    std::vector<FakeScreen*> screens = { &startup, &readded };

    // its name isn't known yet, so it can't be matched against --screen-root
    CHECK (outputsReadyForSetup (screens).empty ());

    readded.initialized = true;
    CHECK (outputsReadyForSetup (screens) == std::vector<uint32_t> { 63 });

    // once handled it is not set up a second time
    readded.pendingSetup = false;
    CHECK (outputsReadyForSetup (screens).empty ());
}

TEST_CASE ("Wayland outputs: several outputs coming back are all set up") {
    FakeScreen a { .waylandName = 70, .initialized = true, .pendingSetup = true };
    FakeScreen b { .waylandName = 71, .initialized = false, .pendingSetup = true };
    FakeScreen c { .waylandName = 72, .initialized = true, .pendingSetup = true };
    std::vector<FakeScreen*> screens = { &a, &b, &c };

    CHECK (outputsReadyForSetup (screens) == std::vector<uint32_t> { 70, 72 });
}

TEST_CASE ("Wayland outputs: an output removed while others are set up is skipped") {
    // setupLS does roundtrips, so an output on the ready list can be gone by the time its turn comes
    FakeScreen a { .waylandName = 80, .initialized = true, .pendingSetup = true };
    FakeScreen b { .waylandName = 81, .initialized = true, .pendingSetup = true };
    std::vector<FakeScreen*> screens = { &a, &b };

    const auto ready = outputsReadyForSetup (screens);
    REQUIRE (ready == std::vector<uint32_t> { 80, 81 });

    std::erase (screens, &b);

    CHECK (findByWaylandName (screens, ready[0]) != screens.end ());
    CHECK (findByWaylandName (screens, ready[1]) == screens.end ());
}
