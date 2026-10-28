#include <catch2/catch_test_macros.hpp>

#include <initializer_list>
#include <string>
#include <utility>
#include <vector>

#include "WallpaperEngine/Application/ApplicationContext.h"

using WallpaperEngine::Application::ApplicationContext;

namespace {
template <typename Test>
void withParsedArguments (std::initializer_list<std::string> arguments, Test&& test) {
    std::vector<std::string> args (arguments);
    std::vector<char*> argv;
    argv.reserve (args.size ());
    for (auto& argument : args) {
	argv.push_back (argument.data ());
    }

    ApplicationContext context (static_cast<int> (argv.size ()), argv.data ());
    context.loadSettingsFromArgv ();
    std::forward<Test> (test) (context);
}
} // namespace

TEST_CASE ("Wayland background layer defaults to background") {
    withParsedArguments ({ "linux-wallpaperengine", "--screen-root", "HDMI-A-1", "/tmp/background" },
	[] (const ApplicationContext& context) {
	    CHECK (context.settings.render.mode == ApplicationContext::DESKTOP_BACKGROUND);
	    CHECK (context.settings.render.wayland.layer == ApplicationContext::WAYLAND_LAYER_BACKGROUND);
	});
}

TEST_CASE ("Wayland background layer can be overridden with bottom") {
    withParsedArguments ({ "linux-wallpaperengine", "--layer", "bottom", "--screen-root", "HDMI-A-1", "/tmp/background" },
	[] (const ApplicationContext& context) {
	    CHECK (context.settings.render.wayland.layer == ApplicationContext::WAYLAND_LAYER_BOTTOM);
	});
}
