#include "WaylandMouseInput.h"
#include "WallpaperEngine/Render/Drivers/WaylandOpenGLDriver.h"
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <regex>
#include <string>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/un.h>
#include <unistd.h>

using namespace WallpaperEngine::Input::Drivers;

WaylandMouseInput::WaylandMouseInput (const WallpaperEngine::Render::Drivers::WaylandOpenGLDriver& driver) :
    m_waylandDriver (driver) { }

void WaylandMouseInput::update () {
    if (!this->m_waylandDriver.getApp ().getContext ().settings.mouse.enabled) {
	return;
    }

    // Surface events already update that output, including between frame callbacks.
    if (m_waylandDriver.viewportInFocus) {
	return;
    }

    const auto now = std::chrono::steady_clock::now ();
    if (now - this->m_lastHyprlandQuery < std::chrono::milliseconds (16)) {
	return;
    }
    this->m_lastHyprlandQuery = now;

    const auto globalCursor = this->queryHyprlandCursorPosition ();
    if (!globalCursor.has_value ()) {
	return;
    }

    for (auto* viewport : this->m_waylandDriver.m_screens) {
	if (!viewport || !viewport->layerSurface || viewport->logicalSize.x <= 0 || viewport->logicalSize.y <= 0) {
	    continue;
	}

	const double localX = globalCursor->x - viewport->globalPosition.x;
	const double localY = globalCursor->y - viewport->globalPosition.y;
	if (localX < 0.0 || localY < 0.0 || localX >= viewport->logicalSize.x || localY >= viewport->logicalSize.y) {
	    continue;
	}

	// Keep positions per output: other wallpapers retain their last cursor position.
	// Hyprland reports logical desktop coordinates; renderers consume framebuffer pixels.
	viewport->mousePos = {
	    localX / viewport->logicalSize.x * viewport->viewport.z,
	    (1.0 - localY / viewport->logicalSize.y) * viewport->viewport.w,
	};
	viewport->hasMousePosition = true;
	return;
    }
}

glm::dvec2 WaylandMouseInput::position () const {
    if (!this->m_waylandDriver.getApp ().getContext ().settings.mouse.enabled) {
	return { 0, 0 };
    }

    const auto* viewport = this->getActiveOutputViewport ();

    if (!viewport) {
	return { 0, 0 };
    }

    if (viewport->hasMousePosition) {
	return viewport->mousePos;
    }

    return {
	static_cast<double> (viewport->size.x * viewport->scale) / 2.0,
	static_cast<double> (viewport->size.y * viewport->scale) / 2.0,
    };
}

glm::dvec2 WaylandMouseInput::screenPosition () const {
    const auto* viewport = this->getActiveOutputViewport ();
    if (!viewport || !this->m_waylandDriver.getApp ().getContext ().settings.mouse.enabled) {
	return {};
    }

    const glm::dvec2 position = this->position ();
    return { position.x, viewport->viewport.w - position.y };
}

WallpaperEngine::Input::MouseClickStatus WaylandMouseInput::leftClick () const {
    const auto* viewport = this->getActiveOutputViewport ();
    if (viewport && viewport == this->m_waylandDriver.viewportInFocus) {
	return viewport->leftClick;
    }

    return MouseClickStatus::Released;
}

const WallpaperEngine::Render::Drivers::Output::WaylandOutputViewport*
WaylandMouseInput::getActiveOutputViewport () const {
    for (const auto* viewport : this->m_waylandDriver.m_screens) {
	if (viewport && viewport->rendering) {
	    return viewport;
	}
    }

    return nullptr;
}

std::optional<glm::dvec2> WaylandMouseInput::queryHyprlandCursorPosition () const {
    const char* signature = std::getenv ("HYPRLAND_INSTANCE_SIGNATURE");
    const char* runtime = std::getenv ("XDG_RUNTIME_DIR");
    if (!signature || !runtime) {
	return std::nullopt;
    }

    const std::string socketPath = std::string (runtime) + "/hypr/" + signature + "/.socket.sock";

    int fd = socket (AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (fd < 0) {
	return std::nullopt;
    }

    timeval timeout {};
    timeout.tv_usec = 50000;
    if (setsockopt (fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof (timeout)) != 0
	|| setsockopt (fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof (timeout)) != 0) {
	close (fd);
	return std::nullopt;
    }

    sockaddr_un addr {};
    addr.sun_family = AF_UNIX;
    if (socketPath.size () >= sizeof (addr.sun_path)) {
	close (fd);
	return std::nullopt;
    }
    std::strncpy (addr.sun_path, socketPath.c_str (), sizeof (addr.sun_path) - 1);

    if (connect (fd, reinterpret_cast<sockaddr*> (&addr), sizeof (addr)) != 0) {
	close (fd);
	return std::nullopt;
    }

    constexpr const char* request = "j/cursorpos";
    if (send (fd, request, std::strlen (request), MSG_NOSIGNAL) < 0) {
	close (fd);
	return std::nullopt;
    }
    shutdown (fd, SHUT_WR);

    std::string response;
    char buffer[256];
    ssize_t readBytes = 0;
    while ((readBytes = recv (fd, buffer, sizeof (buffer), 0)) > 0) {
	response.append (buffer, static_cast<std::size_t> (readBytes));
    }
    close (fd);

    static const std::regex xRegex (R"("x"\s*:\s*(-?\d+(?:\.\d+)?))");
    static const std::regex yRegex (R"("y"\s*:\s*(-?\d+(?:\.\d+)?))");
    std::smatch xMatch;
    std::smatch yMatch;
    if (!std::regex_search (response, xMatch, xRegex) || !std::regex_search (response, yMatch, yRegex)) {
	return std::nullopt;
    }

    try {
	return glm::dvec2 { std::stod (xMatch[1].str ()), std::stod (yMatch[1].str ()) };
    } catch (const std::exception&) {
	return std::nullopt;
    }
}

WallpaperEngine::Input::MouseClickStatus WaylandMouseInput::rightClick () const {
    const auto* viewport = this->getActiveOutputViewport ();

    if (viewport && viewport == this->m_waylandDriver.viewportInFocus) {
	return viewport->rightClick;
    }

    return MouseClickStatus::Released;
}
