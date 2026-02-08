#include "Output.h"

using namespace WallpaperEngine::Render::Drivers::Output;

Output::Output (ApplicationContext& context, VideoDriver& driver) : m_context (context), m_driver (driver) { }

const std::map<std::string, OutputViewport*>& Output::getViewports () const { return this->m_viewports; }

int Output::getFullWidth () const { return this->m_fullWidth; }

int Output::getFullHeight () const { return this->m_fullHeight; }
