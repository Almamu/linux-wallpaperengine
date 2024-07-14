#pragma once

#include "WallpaperEngine/Core/Wallpapers/CWeb.h"
#include "WallpaperEngine/Logging/CLog.h"

// TODO: FIND A BETTER PLACE TO DO THIS? OLD_API MIGHT EXIST BUT THIS DEFINE MIGHT NOT BE DEFINED...
#ifndef FF_API_FIFO_OLD_API
#define 	FF_API_FIFO_OLD_API   (LIBAVUTIL_VERSION_MAJOR < 59)
#endif