#include "Environment.h"
#include "Wayland/Environment.h"

#include "WallpaperEngine/Application/ApplicationContext.h"
#include "WallpaperEngine/Logging/Log.h"

using namespace WallpaperEngine::Desktop;

Environment::Environment (
	Application::ApplicationContext& context, ScreenAvailableNotification& availableNotification,
	ScreenUnavailableNotification& unavailableNotification
) :
	m_context (context), m_availableNotification (availableNotification),
	m_unavailableNotification (unavailableNotification) {
	this->counter = { .user_parameter = this, .get_time = nullptr };

	this->gl_proc_address = { .user_parameter = this, .get_proc_address = nullptr };

	this->mouse_input = { .user_parameter = this, .get_x = nullptr, .get_y = nullptr, .is_pressed = nullptr };
}

WallpaperEngine::Application::ApplicationContext& Environment::getContext () const { return this->m_context; }

void Environment::onScreenAvailable (const std::string& name, Output* output) {
	sLog.debug ("New screen available: ", name);
	this->m_availableNotification.onScreenAvailable (name, output);
}

void Environment::onScreenUnavailable (const std::string& name, Output* output) {
	sLog.debug ("Screen unavailable: ", name);
	this->m_unavailableNotification.onScreenUnavailable (name, output);
}