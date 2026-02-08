#pragma once

#ifdef ENABLE_WAYLAND

#include <glm/vec4.hpp>
#include <map>
#include <string>

#include "Output.h"
#include "WallpaperEngine/Render/Drivers/VideoDriver.h"

namespace WallpaperEngine::Render::Drivers {
class WaylandOpenGLDriver;

namespace Output {
    class WaylandOutput final : public Output {
    public:
	WaylandOutput (ApplicationContext& context, WaylandOpenGLDriver& driver);
	~WaylandOutput () override = default;

	void reset () override;

	bool renderVFlip () const override;
	bool renderMultiple () const override;
	bool haveImageBuffer () const override;
	void* getImageBuffer () const override;
	uint32_t getImageBufferSize () const override;
	void updateRender () const override;

    private:
	void updateViewports ();
    };
} // namespace Output
} // namespace WallpaperEngine::Render::Drivers
#endif /* ENABLE_WAYLAND */