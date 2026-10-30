#include "EmbeddedOpenGLDriver.h"
#include "WallpaperEngine/Logging/Log.h"
#include "WallpaperEngine/Render/Drivers/Output/EmbeddedOutput.h"

#include <GL/glew.h>

using namespace WallpaperEngine::Render::Drivers;

EmbeddedOpenGLDriver::EmbeddedOpenGLDriver (
    ApplicationContext& context, WallpaperApplication& app, EmbeddedHost& host
) :
    VideoDriver (app, m_mouseInput), m_context (context), m_host (host), m_mouseInput (host) {
    // The host's context has to be current already: everything below touches GL.
    glewExperimental = GL_TRUE;

    if (const GLenum result = glewInit (); result != GLEW_OK) {
	// Non-fatal for the same reason the Wayland driver tolerates it: GLEW
	// reports a missing GLX display on EGL-only setups even though every
	// entrypoint it resolved is perfectly usable.
	sLog.error (
	    "Failed to initialize GLEW, but continuing with the host context: ",
	    reinterpret_cast<const char*> (glewGetErrorString (result))
	);
    }

    this->m_output = new Output::EmbeddedOutput (context, *this, host);

    // The host's framebuffer is deliberately not published here. This runs from
    // inside WallpaperApplication::setup(), before the render context exists, so
    // there is nothing to propagate to yet. The host sets it once setup() has
    // returned, and again whenever its framebuffer is recreated.
}

EmbeddedOpenGLDriver::~EmbeddedOpenGLDriver () { delete this->m_output; }

Output::Output& EmbeddedOpenGLDriver::getOutput () { return *this->m_output; }

float EmbeddedOpenGLDriver::getRenderTime () const { return this->m_host.getRenderTime (); }

bool EmbeddedOpenGLDriver::closeRequested () {
    // The host decides our lifetime; we never ask to be torn down.
    return false;
}

void EmbeddedOpenGLDriver::resizeWindow (glm::ivec2 size) {
    // Sizing belongs to the host. updateRender() picks the new size up.
}

void EmbeddedOpenGLDriver::resizeWindow (glm::ivec4 sizeandpos) {
    // See above.
}

void EmbeddedOpenGLDriver::showWindow () { }

void EmbeddedOpenGLDriver::hideWindow () { }

glm::ivec2 EmbeddedOpenGLDriver::getFramebufferSize () const { return this->m_host.getFramebufferSize (); }

uint32_t EmbeddedOpenGLDriver::getFrameCounter () const { return this->m_frameCounter; }

void* EmbeddedOpenGLDriver::getProcAddress (const char* name) const { return this->m_host.getProcAddress (name); }

void EmbeddedOpenGLDriver::dispatchEventQueue () {
    // Qt hands out a fresh FBO whenever the item is resized or its scene graph
    // node is rebuilt, so re-point the wallpapers every frame rather than
    // trusting the handle captured at construction time.
    this->getApp ().setDestinationFramebuffer (this->m_host.getDestinationFramebuffer ());

    this->m_output->updateRender ();

    for (const auto& viewport : this->m_output->getViewports () | std::views::values) {
	this->getApp ().update (viewport);
    }

    // No buffer swap and no FPS limiter: the host presents the framebuffer and
    // owns the frame clock, so throttling here would just fight with it.
    this->m_frameCounter++;
}
