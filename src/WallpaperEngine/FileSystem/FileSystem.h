/**
 * @author Alexis Maiquez Murcia <almamu@almamu.com>
 */
#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <vector>

#include "WallpaperEngine/Assets/CContainer.h"

namespace WallpaperEngine::FileSystem {
/**
 * Loads a full file into an std::string
 *
 * @param file
 * @param containers
 * @return
 */
std::string loadFullFile (const std::string& file, const WallpaperEngine::Assets::CContainer* containers);
} // namespace WallpaperEngine::FileSystem
