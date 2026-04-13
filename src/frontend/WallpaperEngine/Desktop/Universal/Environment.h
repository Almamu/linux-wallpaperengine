#pragma once

#include "WallpaperEngine/Application/ApplicationContext.h"
#include "WallpaperEngine/Desktop/Environment.h"
#include "WallpaperEngine/Desktop/Output.h"

#include <GLFW/glfw3.h>

namespace WallpaperEngine::Desktop::Universal {
class Environment : public Desktop::Environment {
public:
	Environment (
		Application::ApplicationContext& context, ScreenAvailableNotification& availableNotification,
		ScreenUnavailableNotification& unavailableNotification
	);
	~Environment () override;

	void render () override;
	void detectFullscreen () override;
	uint64_t getCurrentFrame () override;
	bool isCloseRequested () override;

private:
	uint64_t m_framecount;
	GLFWwindow* m_window;
	Output m_output;
};
}