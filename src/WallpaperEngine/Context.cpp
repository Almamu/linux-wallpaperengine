#include "WallpaperEngine/Audio/Drivers/AudioDriver.h"
#include "Context.h"

WallpaperEngine::Context::Context (const Configuration* config) :
	texture_cache (std::make_unique<TextureCache> ()),
	audio (nullptr),
	config (config),
	gl_proc_address (nullptr),
	time_counter (nullptr),
	projects (),
	isRunning (true),
	renderTime (0.0f),
	renderTimeLast (0.0f),
	daytime (0.0f) {
}

void WallpaperEngine::Context::updateTime () {
	this->renderTimeLast = this->renderTime;
	this->renderTime = this->time_counter->get_time (this->time_counter->user_parameter);
}