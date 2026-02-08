#pragma once

#include <memory>
#include <string>

#include "Types.h"
#include "WallpaperEngine/Assets/AssetLocator.h"

namespace WallpaperEngine::Data::Model {
using namespace WallpaperEngine::Assets;
/**
 * Represents a wallpaper engine project
 */
struct Project {
    enum Type { Type_Scene = 0, Type_Web = 1, Type_Video = 2, Type_Unknown = 3 };

    /** Wallpapers title */
    std::string title;
    /** Wallpaper's type */
    Type type;
    /** Workshop ID of the background or a negative id if not present */
    std::string workshopId;
    /** Indicates if the background uses audio processing or not */
    bool supportsAudioProcessing;
    /** All the available properties that the project defines for the user to change */
    Properties properties;
    /** The wallpaper this project defines */
    WallpaperUniquePtr wallpaper;
    /** Abstraction over asset loading to provide access to them */
    AssetLocatorUniquePtr assetLocator;
};
};
