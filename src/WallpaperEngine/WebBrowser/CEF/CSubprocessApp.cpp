#include "CSubprocessApp.h"
#include "CWPSchemeHandlerFactory.h"

using namespace WallpaperEngine::WebBrowser::CEF;

CSubprocessApp::CSubprocessApp (WallpaperEngine::Application::CWallpaperApplication& application) :
    m_application (application) {
    for (const auto& [_, info] : this->m_application.getBackgrounds()) {
        this->m_handlerFactories [info->getWorkshopId ()] = new CWPSchemeHandlerFactory (info);
    }
}

void CSubprocessApp::OnRegisterCustomSchemes (CefRawPtr <CefSchemeRegistrar> registrar) {
    // register all the needed schemes, "wp" + the background id is going to be our scheme
    for (const auto& [workshopId, _] : this->m_handlerFactories) {
        registrar->AddCustomScheme (
            CWPSchemeHandlerFactory::generateSchemeName (workshopId),
            CEF_SCHEME_OPTION_STANDARD | CEF_SCHEME_OPTION_SECURE | CEF_SCHEME_OPTION_FETCH_ENABLED
        );
    }
}

const WallpaperEngine::Application::CWallpaperApplication& CSubprocessApp::getApplication () const {
    return this->m_application;
}

const std::map<std::string, CWPSchemeHandlerFactory*>& CSubprocessApp::getHandlerFactories () const {
    return this->m_handlerFactories;
}