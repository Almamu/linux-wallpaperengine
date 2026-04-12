#pragma once

#include "WallpaperEngine/Application/ApplicationContext.h"
#include "WallpaperEngine/Desktop/Environment.h"
#include "WallpaperEngine/Desktop/Output.h"

#include <GLFW/glfw3.h>

namespace WallpaperEngine::Desktop::Universal {
class Environment : public Desktop::Environment {
public:
	explicit Environment (Application::ApplicationContext& context);
	~Environment () override;

	void render () override;
	void detectFullscreen () override;
	uint64_t getCurrentFrame () override;
	bool isCloseRequested () override;

	[[nodiscard]] Output* requestOutput (const std::string& name) override;
	[[nodiscard]] Output* getOutput (const std::string& name) override;

private:
	uint64_t m_framecount;
	Application::ApplicationContext& m_context;
	GLFWwindow* m_window;
	Output m_output;
};
}