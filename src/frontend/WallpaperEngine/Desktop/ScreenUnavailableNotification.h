#pragma once

#include <string>

namespace WallpaperEngine::Desktop {
class ScreenUnavailableNotification {
public:
	virtual ~ScreenUnavailableNotification () = default;

	virtual void onScreenUnavailable (const std::string& name, Output* output) = 0;
};
}
