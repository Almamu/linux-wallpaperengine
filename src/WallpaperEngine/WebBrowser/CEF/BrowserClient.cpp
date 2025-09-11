#include "BrowserClient.h"

using namespace WallpaperEngine::WebBrowser::CEF;

BrowserClient::BrowserClient(CefRefPtr<CefRenderHandler> ptr) :
    m_renderHandler(std::move(ptr)) {}

CefRefPtr<CefRenderHandler> BrowserClient::GetRenderHandler()
{
    return m_renderHandler;
}