#pragma once

#include "include/cef_client.h"

namespace WallpaperEngine::WebBrowser::CEF {
// *************************************************************************
//! \brief Provide access to browser-instance-specific callbacks. A single
//! CefClient instance can be shared among any number of browsers.
// *************************************************************************
class CBrowserClient: public CefClient
{
  public:
    explicit CBrowserClient(CefRefPtr<CefRenderHandler> ptr);

    [[nodiscard]] CefRefPtr<CefRenderHandler> GetRenderHandler() override;

    CefRefPtr<CefRenderHandler> m_renderHandler = nullptr;

    IMPLEMENT_REFCOUNTING(CBrowserClient);
};
} // namespace WallpaperEngine::WebBrowser::CEF