#include "Environment.h"
#include "Wayland/Environment.h"

#include "WallpaperEngine/Application/ApplicationContext.h"

using namespace WallpaperEngine::Desktop;

Environment::Environment (Application::ApplicationContext& context) : m_context (context) {
	this->counter = { .user_parameter = this, .get_time = nullptr };

	this->gl_proc_address = { .user_parameter = this, .get_proc_address = nullptr };

	this->mouse_input = { .user_parameter = this, .get_x = nullptr, .get_y = nullptr, .is_pressed = nullptr };
}

WallpaperEngine::Application::ApplicationContext& Environment::getContext () const { return this->m_context; }