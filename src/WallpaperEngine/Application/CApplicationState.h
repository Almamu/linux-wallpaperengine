#pragma once

#include "CApplicationContext.h"

namespace WallpaperEngine::Application {
/**
 * Represents current application state
 */
class CApplicationState {
  public:
    struct {
        bool keepRunning;
    } general {};

    struct {
        bool enabled;
        int volume;
    } audio {};

    struct {
        bool enabled;
    } mouse {};
};
} // namespace WallpaperEngine::Application