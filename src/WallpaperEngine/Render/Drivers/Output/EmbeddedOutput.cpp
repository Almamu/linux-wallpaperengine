#include "EmbeddedOutput.h"
#include "EmbeddedOutputViewport.h"
#include "WallpaperEngine/Render/Drivers/EmbeddedHost.h"

using namespace WallpaperEngine::Render::Drivers::Output;

EmbeddedOutput::EmbeddedOutput (ApplicationContext& context, VideoDriver& driver, const EmbeddedHost& host) :
    Output (context, driver), m_host (host) {
    const glm::ivec2 size = this->m_host.getFramebufferSize ();

    this->m_fullWidth = size.x;
    this->m_fullHeight = size.y;

    this->m_viewports ["default"]
	= new EmbeddedOutputViewport { { 0, 0, this->m_fullWidth, this->m_fullHeight }, "default" };
}

void EmbeddedOutput::reset () { }

bool EmbeddedOutput::renderVFlip () const { return this->m_host.wantsVerticalFlip (); }

bool EmbeddedOutput::renderMultiple () const { return false; }

bool EmbeddedOutput::haveImageBuffer () const { return false; }

void* EmbeddedOutput::getImageBuffer () const { return nullptr; }

uint32_t EmbeddedOutput::getImageBufferSize () const { return 0; }

void EmbeddedOutput::updateRender () const {
    // The host is free to resize us at any point, so re-read the size every
    // frame and re-stretch the viewport instead of cropping the old render.
    const glm::ivec2 size = this->m_host.getFramebufferSize ();

    this->m_fullWidth = size.x;
    this->m_fullHeight = size.y;

    this->m_viewports.at ("default")->viewport = { 0, 0, this->m_fullWidth, this->m_fullHeight };
}
