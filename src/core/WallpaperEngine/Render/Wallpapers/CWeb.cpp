#include "CWeb.h"

#include "WallpaperEngine/Context.h"
#include "WallpaperEngine/Data/Model/Project.h"
#include "WallpaperEngine/Data/Model/Wallpaper.h"
#include "WallpaperEngine/Input/MouseClickStatus.h"
#include "WallpaperEngine/Utils/UUID.h"
#include "WallpaperEngine/WebBrowser/IPC/WebManager.h"

using namespace WallpaperEngine::Render;
using namespace WallpaperEngine::Render::Wallpapers;

CWeb::CWeb (
    const Wallpaper& wallpaper, RenderContext& context, AudioContext& audioContext, wp_mouse_input* mouseInput
) : CWallpaper (wallpaper, context, audioContext, mouseInput), m_uuid (Utils::UUID::UUIDv4 ()) {
    this->setupFramebuffers ();

    sWebManager.init ();
    sWebManager.registerAssetLocator (this->m_uuid, &this->getAssetLocator ());

    const std::string htmlURL = "wp://" + this->m_uuid + "/" + this->getWeb ().filename;
    sWebManager.createBrowser (
	this->m_uuid, htmlURL, this->getWidth (), this->getHeight (), context.getContext ().config.web_fps
    );
}

void CWeb::setSize (const int width, const int height) {
    this->m_width = width > 0 ? width : this->m_width;
    this->m_height = height > 0 ? height : this->m_height;

    if (this->m_width <= 0 || this->m_height <= 0) {
	return;
    }

    glBindTexture (GL_TEXTURE_2D, this->getWallpaperTexture ());
    glTexImage2D (
	GL_TEXTURE_2D, 0, GL_RGBA8, this->getWidth (), this->getHeight (), 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr
    );

    sWebManager.resize (this->m_uuid, this->m_width, this->m_height);
}

void CWeb::renderFrame () {
    this->updateMouse ();

    glBindFramebuffer (GL_FRAMEBUFFER, this->getWallpaperFramebuffer ());
    glViewport (0, 0, this->getWidth (), this->getHeight ());

    sWebManager.uploadFrameIfReady (this->m_uuid, this->getWallpaperTexture ());
}

void CWeb::updateMouse () {
    const auto handler = this->getMouseInputHandler ();
    const glm::dvec2 position = this->getLiveMousePosition ();
    const auto buttonStatus = handler != nullptr && handler->is_pressed != nullptr
	? handler->is_pressed (handler->user_parameter, WP_MOUSE_INPUT_BUTTON_LEFT | WP_MOUSE_INPUT_BUTTON_RIGHT)
	: 0;
    const bool leftClick = (buttonStatus & WP_MOUSE_INPUT_BUTTON_LEFT) > 0;
    const bool rightClick = (buttonStatus & WP_MOUSE_INPUT_BUTTON_RIGHT) > 0;

    sWebManager.sendMouseMove (this->m_uuid, static_cast<float> (position.x), static_cast<float> (position.y));

    const auto leftStatus = leftClick ? WallpaperEngine::Input::MouseClickStatus::Clicked
				      : WallpaperEngine::Input::MouseClickStatus::Released;
    const auto rightStatus = rightClick ? WallpaperEngine::Input::MouseClickStatus::Clicked
					: WallpaperEngine::Input::MouseClickStatus::Released;

    if (leftStatus != this->m_leftClick) {
	sWebManager.sendMouseButton (
	    this->m_uuid, static_cast<float> (position.x), static_cast<float> (position.y), 0,
	    leftStatus == WallpaperEngine::Input::MouseClickStatus::Released
	);
	this->m_leftClick = leftStatus;
    }

    if (rightStatus != this->m_rightClick) {
	sWebManager.sendMouseButton (
	    this->m_uuid, static_cast<float> (position.x), static_cast<float> (position.y), 1,
	    rightStatus == WallpaperEngine::Input::MouseClickStatus::Released
	);
	this->m_rightClick = rightStatus;
    }
}

CWeb::~CWeb () {
    sWebManager.destroyBrowser (this->m_uuid);
    sWebManager.unregisterAssetLocator (this->m_uuid);
}
