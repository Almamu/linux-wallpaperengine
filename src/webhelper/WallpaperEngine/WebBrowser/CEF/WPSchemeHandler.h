#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "include/cef_resource_handler.h"
#include "include/wrapper/cef_helpers.h"

namespace WallpaperEngine::WebBrowser::IPC {
class BrowserServer;
}

namespace WallpaperEngine::WebBrowser::CEF {

// Resource handler for the wp:// scheme.  Instead of reading files directly
// from an AssetLocator it forwards requests to the core process via IPC and
// blocks the CEF worker thread until the response arrives.
class WPSchemeHandler : public CefResourceHandler {
public:
    WPSchemeHandler (const std::string& uuid, WallpaperEngine::WebBrowser::IPC::BrowserServer* server);

    bool Open (CefRefPtr<CefRequest> request, bool& handle_request, CefRefPtr<CefCallback> callback) override;

    void
    GetResponseHeaders (CefRefPtr<CefResponse> response, int64_t& response_length, CefString& redirectUrl) override;

    void Cancel () override { }

    bool
    Read (void* data_out, int bytes_to_read, int& bytes_read, CefRefPtr<CefResourceReadCallback> callback) override;

private:
    std::string m_uuid;
    WallpaperEngine::WebBrowser::IPC::BrowserServer* m_server;

    std::string m_mimeType;
    std::vector<uint8_t> m_data;
    size_t m_offset = 0;
    bool m_found = false;

    IMPLEMENT_REFCOUNTING (WPSchemeHandler);
    DISALLOW_COPY_AND_ASSIGN (WPSchemeHandler);
};

} // namespace WallpaperEngine::WebBrowser::CEF
