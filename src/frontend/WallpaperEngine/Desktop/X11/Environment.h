#pragma once

#include <map>

#include "Output.h"
#include "WallpaperEngine/Desktop/Environment.h"
#include "WallpaperEngine/Desktop/VirtualOutput.h"

#include <GLFW/glfw3.h>
#include <X11/Xlib.h>

namespace WallpaperEngine::Desktop::X11 {
class Environment : public Desktop::Environment {
public:
	explicit Environment (Application::ApplicationContext& context);
	~Environment () override;

	void render() override;
	void detectFullscreen() override;
	uint64_t getCurrentFrame() override;
	bool isCloseRequested() override;

	void registerOutput (const std::string& name, glm::vec4 viewport);
	void deregisterOutput (Output* output);

	[[nodiscard]] Desktop::Output* requestOutput (const std::string& name) override;
	[[nodiscard]] Desktop::Output* getOutput (const std::string& name) override;

private:
	/**
	 * Updates the pixmap used to copy over data to X11
	 */
	void updatePixmap ();
	/**
	 * Detects all available outputs and registers them
	 */
	void detectOutputs ();

	GLFWwindow* m_window;
	Display* m_display;
	Pixmap m_pixmap;
	Window m_root;
	GC m_gc;
	XImage* m_image;
	int m_fullWidth;
	int m_fullHeight;
	int m_xrandrEventBase;
	uint32_t m_imageSize;
	char* m_imageData;
	uint64_t m_frameCount;
	bool m_closeRequested;

	GLuint m_framebuffer;
	GLuint m_texture;

	std::map<std::string, VirtualOutput<Output>*> m_requestedOutputs;
	std::vector<Output*> m_outputs;
};
}
