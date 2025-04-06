#include "CBrowserClient.h"

using namespace WallpaperEngine::WebBrowser::CEF;

CBrowserClient::CBrowserClient(CefRefPtr<CefRenderHandler> ptr)
    : m_renderHandler(std::move(ptr))
{}

CefRefPtr<CefRenderHandler> CBrowserClient::GetRenderHandler()
{
    return m_renderHandler;
}