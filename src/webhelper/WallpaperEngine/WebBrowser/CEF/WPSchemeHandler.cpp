#include "WPSchemeHandler.h"

#include <algorithm>
#include <cstring>

#include "include/cef_parser.h"
#include "WallpaperEngine/Logging/Log.h"
#include "WallpaperEngine/WebBrowser/IPC/BrowserServer.h"

using namespace WallpaperEngine::WebBrowser::CEF;

WPSchemeHandler::WPSchemeHandler (
    const std::string& uuid, WallpaperEngine::WebBrowser::IPC::BrowserServer* server
) : m_uuid (uuid), m_server (server) {}

bool WPSchemeHandler::Open (
    CefRefPtr<CefRequest> request, bool& handle_request, CefRefPtr<CefCallback> /*callback*/
) {
    DCHECK (!CefCurrentlyOn (TID_UI) && !CefCurrentlyOn (TID_IO));

    CefURLParts parts;
    if (!CefParseURL (request->GetURL (), parts)) {
        handle_request = true;
        return true;
    }

    const std::string path = CefString (&parts.path).ToString ().substr (1);

    const auto asset = m_server->fetchAsset (m_uuid, path);
    m_found    = asset.found;
    m_mimeType = asset.mimeType;
    m_data     = asset.data;
    m_offset   = 0;

    handle_request = true;
    return true;
}

void WPSchemeHandler::GetResponseHeaders (
    CefRefPtr<CefResponse> response, int64_t& response_length, CefString& /*redirectUrl*/
) {
    CEF_REQUIRE_IO_THREAD ();

    if (!m_found) {
        response->SetError (ERR_FILE_NOT_FOUND);
        response->SetStatus (404);
        response_length = 0;
        return;
    }

    response->SetMimeType (m_mimeType);
    response->SetStatus (200);
    response_length = static_cast<int64_t> (m_data.size ());
}

bool WPSchemeHandler::Read (
    void* data_out, int bytes_to_read, int& bytes_read, CefRefPtr<CefResourceReadCallback> /*callback*/
) {
    DCHECK (!CefCurrentlyOn (TID_UI) && !CefCurrentlyOn (TID_IO));

    if (m_offset >= m_data.size ()) {
        bytes_read = 0;
        return false;
    }

    const size_t available = m_data.size () - m_offset;
    bytes_read = static_cast<int> (std::min (static_cast<size_t> (bytes_to_read), available));
    std::memcpy (data_out, m_data.data () + m_offset, static_cast<size_t> (bytes_read));
    m_offset += static_cast<size_t> (bytes_read);
    return true;
}
