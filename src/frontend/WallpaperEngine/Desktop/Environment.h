#pragma once

#include <string>

#include "Output.h"

namespace WallpaperEngine::Desktop {
class Environment {
public:
	virtual ~Environment () = default;

	virtual void render () = 0;
	virtual void detectFullscreen () = 0;
	virtual uint64_t getCurrentFrame () = 0;
	virtual bool isCloseRequested () = 0;

	wp_time_counter counter;
	wp_gl_proc_address gl_proc_address;
	wp_mouse_input mouse_input;
	bool anything_fullscreen = false;

	virtual Output* requestOutput (const std::string& name) = 0;
	[[nodiscard]] virtual Output* getOutput (const std::string& name) = 0;
};
}
