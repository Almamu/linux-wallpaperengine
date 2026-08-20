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

    [[nodiscard]] WallpaperEngine::Render::RenderContext& getRenderContext ();
    [[nodiscard]] WallpaperEngine::Audio::AudioContext& getAudioContext ();

protected:
    RenderHarness (ApplicationContext* context, WallpaperApplication* app);

private:
    TestingOpenGLDriver m_driver;
    ApplicationContext* m_context;
    WallpaperApplication* m_app;
    std::unique_ptr<WallpaperEngine::Render::Drivers::Detectors::FullScreenDetector> m_fullScreenDetector;
    std::unique_ptr<WallpaperEngine::Audio::Drivers::Detectors::AudioPlayingDetector> m_audioDetector;
    std::unique_ptr<WallpaperEngine::Audio::Drivers::Recorders::PlaybackRecorder> m_audioRecorder;
    std::unique_ptr<WallpaperEngine::Audio::Drivers::SDLAudioDriver> m_audioDriver;
    std::unique_ptr<WallpaperEngine::Audio::AudioContext> m_audioContext;
    std::unique_ptr<WallpaperEngine::Media::MediaSource> m_mediaSource;
    std::unique_ptr<WallpaperEngine::Render::RenderContext> m_renderContext;
};
} // namespace WallpaperEngine::Testing::Harnesses
