#pragma once

#include "WallpaperEngine/Testing/Render/TestingOpenGLDriver.h"

namespace WallpaperEngine::Testing::Harnesses {
using namespace WallpaperEngine::Testing::Render;
/**
 * Set of tools to interact and inspect what the render is doing
 *
 * IMPORTANT: THIS REPLACES SOME gl* METHODS WITH SHIMS
 * TO PROPERLY ALLOW FOR TRACKING AND INSPECTING DATA, SO BE CAREFUL
 * IF YOU NEED TO USE ANY gl* CALLS IN YOUR TESTS
 */
class RenderHarness {
  public:
    static RenderHarness* build (std::filesystem::path base);

    ~RenderHarness ();
  protected:
    RenderHarness (ApplicationContext* context, WallpaperApplication* app);

  private:
    TestingOpenGLDriver m_driver;
    ApplicationContext* m_context;
    WallpaperApplication* m_app;
};
} // namespace WallpaperEngine::Testing::Harnesses
