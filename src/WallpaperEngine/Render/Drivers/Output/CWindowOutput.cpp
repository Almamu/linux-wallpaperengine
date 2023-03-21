#include "CWindowOutput.h"
#include "WallpaperEngine/Logging/CLog.h"

using namespace WallpaperEngine::Render::Drivers::Output;

CWindowOutput::CWindowOutput (CApplicationContext& context, CVideoDriver& driver) :
	COutput (context),
	m_driver (driver)
{
	if (
		this->m_context.windowMode != Application::CApplicationContext::NORMAL_WINDOW &&
		this->m_context.windowMode != Application::CApplicationContext::EXPLICIT_WINDOW)
		sLog.exception ("Inititalizing window output when not in output mode, how did you get here?!");

	// window should be visible
	driver.showWindow ();

	if (this->m_context.windowMode == Application::CApplicationContext::EXPLICIT_WINDOW)
	{
		this->m_fullWidth = this->m_context.windowGeometry.z;
		this->m_fullHeight = this->m_context.windowGeometry.w;
		this->repositionWindow ();
	}
	else
	{
		// take the size from the driver (default window size)
		this->m_fullWidth = this->m_driver.getFramebufferSize ().x;
		this->m_fullHeight = this->m_driver.getFramebufferSize ().y;
	}

	// register the default viewport
	this->m_viewports ["default"] = {{0, 0, this->m_fullWidth, this->m_fullHeight}, "default"};
}

void CWindowOutput::repositionWindow ()
{
	// reposition the window
	this->m_driver.resizeWindow (this->m_context.windowGeometry);
}

void CWindowOutput::reset ()
{
	if (this->m_context.windowMode == Application::CApplicationContext::EXPLICIT_WINDOW)
		this->repositionWindow ();
}

bool CWindowOutput::renderVFlip () const
{
	return true;
}

bool CWindowOutput::renderMultiple () const
{
	return false;
}

bool CWindowOutput::haveImageBuffer () const
{
	return false;
}

void* CWindowOutput::getImageBuffer () const
{
	return nullptr;
}
void CWindowOutput::updateRender () const
{
	if (this->m_context.windowMode != Application::CApplicationContext::NORMAL_WINDOW)
		return;

	// take the size from the driver (default window size)
	this->m_fullWidth = this->m_driver.getFramebufferSize ().x;
	this->m_fullHeight = this->m_driver.getFramebufferSize ().y;

	// update the default viewport
	this->m_viewports ["default"] = {{0, 0, this->m_fullWidth, this->m_fullHeight}, "default"};
}