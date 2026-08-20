#include "RenderHarness.h"

using namespace WallpaperEngine::Testing::Harnesses;

const char* argv[] = {
    "",
};

namespace {
class TestMediaSource final : public WallpaperEngine::Media::MediaSource {
public:
    TestMediaSource () : MediaSource (std::chrono::milliseconds::max ()) { }

private:
    void performUpdate () override { }
};
} // namespace

RenderHarness::RenderHarness (ApplicationContext* context, WallpaperApplication* app) :
    m_context (context), m_app (app), m_driver (*context, *app) {
    m_fullScreenDetector
	= std::make_unique<WallpaperEngine::Render::Drivers::Detectors::FullScreenDetector> (*m_context);
    m_audioDetector = std::make_unique<WallpaperEngine::Audio::Drivers::Detectors::AudioPlayingDetector> (
	*m_context, *m_fullScreenDetector
    );
    m_audioRecorder = std::make_unique<WallpaperEngine::Audio::Drivers::Recorders::PlaybackRecorder> ();
    m_audioDriver = std::make_unique<WallpaperEngine::Audio::Drivers::SDLAudioDriver> (
	*m_context, *m_audioDetector, *m_audioRecorder
    );
    m_audioContext = std::make_unique<WallpaperEngine::Audio::AudioContext> (*m_audioDriver);
    m_mediaSource = std::make_unique<TestMediaSource> ();
    m_renderContext = std::make_unique<WallpaperEngine::Render::RenderContext> (m_driver, *m_app, *m_mediaSource);
}

RenderHarness::~RenderHarness () {
    this->m_renderContext.reset ();
    this->m_mediaSource.reset ();
    this->m_audioContext.reset ();
    this->m_audioDriver.reset ();
    this->m_audioRecorder.reset ();
    this->m_audioDetector.reset ();
    this->m_fullScreenDetector.reset ();

    delete this->m_app;
    delete this->m_context;
}

WallpaperEngine::Render::RenderContext& RenderHarness::getRenderContext () { return *this->m_renderContext; }
WallpaperEngine::Audio::AudioContext& RenderHarness::getAudioContext () { return *this->m_audioContext; }

RenderHarness* RenderHarness::build (std::filesystem::path base) {
    // build context, app and return a harness that owns it
    auto context = new ApplicationContext (1, const_cast<char**> (argv));
    if (!base.empty ()) {
	context->settings.general.defaultBackground = base.string ();
    }

    return new RenderHarness (context, new WallpaperApplication (*context));
}
