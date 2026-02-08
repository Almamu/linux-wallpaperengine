#include "WaylandOutput.h"
#include "../WaylandOpenGLDriver.h"
#include "WallpaperEngine/Application/WallpaperApplication.h"

using namespace WallpaperEngine::Render::Drivers::Output;

WaylandOutput::WaylandOutput (ApplicationContext& context, WaylandOpenGLDriver& driver) : Output (context, driver) {
    updateViewports ();
}

void WaylandOutput::updateViewports () {
    m_viewports.clear ();
    const auto PDRIVER = dynamic_cast<WaylandOpenGLDriver*> (&m_driver);
    glm::ivec2 fullw = { 0, 0 };
    for (const auto& o : PDRIVER->m_screens) {
	if (!o->layerSurface) {
	    continue;
	}

	m_viewports[o->name] = o;

	fullw = fullw + glm::ivec2 { o->size.x * o->scale, 0 };
	if (o->size.y * o->scale > fullw.y) {
	    fullw.y = o->size.y * o->scale;
	}
    }

    m_fullWidth = fullw.x;
    m_fullHeight = fullw.y;
}

void WaylandOutput::reset () { updateViewports (); }

bool WaylandOutput::renderVFlip () const { return true; }

bool WaylandOutput::renderMultiple () const {
    return false; // todo
}

bool WaylandOutput::haveImageBuffer () const { return false; }

void* WaylandOutput::getImageBuffer () const { return nullptr; }

uint32_t WaylandOutput::getImageBufferSize () const { return 0; }

void WaylandOutput::updateRender () const { }