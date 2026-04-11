// This code is a modification of the original projects that can be found at
// https://github.com/if1live/cef-gl-example
// https://github.com/andmcgregor/cefgui
#include "CWeb.h"

#include "WallpaperEngine/Context.h"
#include "WallpaperEngine/WebBrowser/CEF/WPSchemeHandlerFactory.h"

#include "WallpaperEngine/Data/Model/Project.h"
#include "WallpaperEngine/Data/Model/Wallpaper.h"
#include "WallpaperEngine/Input/MouseClickStatus.h"
#include "WallpaperEngine/Utils/UUID.h"
#include "WallpaperEngine/WebBrowser/CEF/BrowserApp.h"
#include "include/cef_app.h"

using namespace WallpaperEngine::Render;
using namespace WallpaperEngine::Render::Wallpapers;

using namespace WallpaperEngine::WebBrowser;
using namespace WallpaperEngine::WebBrowser::CEF;

CWeb::CWeb (
	const Wallpaper& wallpaper, RenderContext& context, AudioContext& audioContext, wp_mouse_input* mouseInput
) : CWallpaper (wallpaper, context, audioContext, mouseInput) {
	const CefMainArgs main_args;
	// setup framebuffers
	this->setupFramebuffers ();

	this->m_browserApplication
		= new CEF::BrowserApp (context.getContext ().config.assets_dir, "", this->getAssetLocator ());

	// this blocks for anything not-main-thread
	CefExecuteProcess (main_args, this->m_browserApplication, nullptr);

	CefSettings settings;
	const std::string cache_path
		= (std::filesystem::temp_directory_path () / WallpaperEngine::Utils::UUID::UUIDv4 ()).string ();

#ifdef WPENGINE_WEBHELPER_PATH
	CefString (&settings.browser_subprocess_path) = WPENGINE_WEBHELPER_PATH;
#endif

#ifdef WPENGINE_CEF_RESOURCES_PATH
	CefString (&settings.resources_dir_path) = WPENGINE_CEF_RESOURCES_PATH;
	CefString (&settings.locales_dir_path) = WPENGINE_CEF_RESOURCES_PATH "/locales";
#endif

	cef_string_utf8_to_utf16 (cache_path.c_str (), cache_path.length (), &settings.root_cache_path);
	settings.windowless_rendering_enabled = true;
#if defined(CEF_NO_SANDBOX)
	settings.no_sandbox = true;
#endif

	// spawns two new processess

	if (!CefInitialize (main_args, settings, this->m_browserApplication, nullptr)) {
		sLog.exception ("CefInitialize: failed");
	}

	CefWindowInfo window_info;
	window_info.SetAsWindowless (0);

	this->m_renderHandler = new WebBrowser::CEF::RenderHandler (this);

	CefBrowserSettings browserSettings;
	// documentaion says that 60 fps is maximum value
	browserSettings.windowless_frame_rate = std::max (60, context.getContext ().config.web_fps);

	this->m_client = new WebBrowser::CEF::BrowserClient (m_renderHandler);
	// use the custom scheme for the wallpaper's files
	const std::string htmlURL = WPSchemeHandlerFactory::generateSchemeName (this->getWeb ().project.workshopId)
		+ "://root/" + this->getWeb ().filename;
	this->m_browser
		= CefBrowserHost::CreateBrowserSync (window_info, this->m_client, htmlURL, browserSettings, nullptr, nullptr);
}

void CWeb::setSize (const int width, const int height) {
	this->m_width = width > 0 ? width : this->m_width;
	this->m_height = height > 0 ? height : this->m_height;

	// do not refresh the texture if any of the sizes are invalid
	if (this->m_width <= 0 || this->m_height <= 0) {
		return;
	}

	// reconfigure the texture
	glBindTexture (GL_TEXTURE_2D, this->getWallpaperTexture ());
	glTexImage2D (
		GL_TEXTURE_2D, 0, GL_RGBA8, this->getWidth (), this->getHeight (), 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr
	);

	// Notify cef that it was resized(maybe it's not even needed)
	this->m_browser->GetHost ()->WasResized ();
}

void CWeb::renderFrame () {
	// TODO: IMPLEMENT BACK THIS CHANGE SOME WAY
	// ensure the viewport matches the window size, and resize if needed
	// if (viewport.z != this->getWidth () || viewport.w != this->getHeight ()) {
	//  this->setSize (viewport.z, viewport.w);
	//}

	// ensure the virtual mouse position is up to date
	this->updateMouse ();
	// use the scene's framebuffer by default
	glBindFramebuffer (GL_FRAMEBUFFER, this->getWallpaperFramebuffer ());
	// ensure we render over the whole framebuffer
	glViewport (0, 0, this->getWidth (), this->getHeight ());

	// Cef processes all messages, including OnPaint, which renders frame
	// If there is no OnPaint in message loop, we will not update(render) frame
	//  This means some frames will not have OnPaint call in cef messageLoop
	//  Because of that glClear will result in flickering on higher fps
	//  Do not use glClear until some method to control rendering with cef is supported
	// We might actually try to use cef to execute javascript, and not using off-screen rendering at all
	// But for now let it be like this
	//  glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	CefDoMessageLoopWork ();
}

void CWeb::updateMouse () {
	const glm::dvec2 position = this->getLiveMousePosition ();
	const auto buttonStatus = this->getMouseInputHandler ()->is_pressed (
		this->getMouseInputHandler ()->user_parameter, WP_MOUSE_INPUT_BUTTON_LEFT | WP_MOUSE_INPUT_BUTTON_RIGHT
	);
	const auto leftClick = (buttonStatus & WP_MOUSE_INPUT_BUTTON_LEFT) > 0;
	const auto rightClick = (buttonStatus & WP_MOUSE_INPUT_BUTTON_RIGHT) > 0;

	CefMouseEvent evt;
	// Set mouse current position. Maybe clamps are not needed
	evt.x = position.x;
	// Convert from OpenGL coordinates (Y=0 at bottom) to CEF coordinates (Y=0 at top)
	evt.y = position.y;
	// Send mouse position to cef
	this->m_browser->GetHost ()->SendMouseMoveEvent (evt, false);

	const auto leftClickStatus = leftClick ? WallpaperEngine::Input::MouseClickStatus::Clicked
										   : WallpaperEngine::Input::MouseClickStatus::Released;
	const auto rightClickStatus = rightClick ? WallpaperEngine::Input::MouseClickStatus::Clicked
											 : WallpaperEngine::Input::MouseClickStatus::Released;

	if (leftClickStatus != this->m_leftClick) {
		this->m_browser->GetHost ()->SendMouseClickEvent (
			evt, CefBrowserHost::MouseButtonType::MBT_LEFT,
			leftClickStatus == WallpaperEngine::Input::MouseClickStatus::Released, 1
		);
	}

	if (rightClickStatus != this->m_rightClick) {
		this->m_browser->GetHost ()->SendMouseClickEvent (
			evt, CefBrowserHost::MouseButtonType::MBT_RIGHT,
			rightClickStatus == WallpaperEngine::Input::MouseClickStatus::Released, 1
		);
	}

	this->m_leftClick = leftClickStatus;
	this->m_rightClick = rightClickStatus;
}

CWeb::~CWeb () {
	CefDoMessageLoopWork ();
	this->m_browser->GetHost ()->CloseBrowser (true);

	delete this->m_renderHandler;
}
