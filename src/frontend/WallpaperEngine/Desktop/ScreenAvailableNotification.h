#pragma once

#include <string>

namespace WallpaperEngine::Desktop {
class ScreenAvailableNotification {
public:
	virtual ~ScreenAvailableNotification () = default;

	virtual void onScreenAvailable (const std::string& name, Output* output) = 0;
};
}
