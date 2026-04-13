#pragma once

#include <string>

#include "Output.h"
#include "ScreenAvailableNotification.h"
#include "ScreenUnavailableNotification.h"

namespace WallpaperEngine::Application {
class ApplicationContext;
}

namespace WallpaperEngine::Desktop {
class Environment : protected ScreenAvailableNotification, protected ScreenUnavailableNotification {
	friend class Output;

public:
	Environment (
		Application::ApplicationContext& context, ScreenAvailableNotification& availableNotification,
		ScreenUnavailableNotification& unavailableNotification
	);
	virtual ~Environment () = default;

	virtual void render () = 0;
	virtual void detectFullscreen () = 0;
	virtual uint64_t getCurrentFrame () = 0;
	virtual bool isCloseRequested () = 0;

	Application::ApplicationContext& getContext () const;

	wp_time_counter counter;
	wp_gl_proc_address gl_proc_address;
	wp_mouse_input mouse_input;
	bool anything_fullscreen = false;

	virtual void onScreenAvailable (const std::string& name, Output* output) override;
	virtual void onScreenUnavailable (const std::string& name, Output* output) override;

protected:
	Application::ApplicationContext& m_context;
	ScreenAvailableNotification& m_availableNotification;
	ScreenUnavailableNotification& m_unavailableNotification;
};
}
