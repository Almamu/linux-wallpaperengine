#include "RenderHarness.h"

using namespace WallpaperEngine::Testing::Harnesses;

const char* argv[] = {
    "",
};

RenderHarness::RenderHarness(ApplicationContext* context, WallpaperApplication* app) :
    m_context (context),
    m_app (app),
    m_driver (*context, *app) {
}

RenderHarness::~RenderHarness() {
    delete this->m_app;
    delete this->m_context;
}

RenderHarness* RenderHarness::build (std::filesystem::path base) {
    // build context, app and return a harness that owns it
    auto context = new ApplicationContext (1, const_cast<char**> (argv));

    return new RenderHarness (
        context,
        new WallpaperApplication (*context)
    );
}