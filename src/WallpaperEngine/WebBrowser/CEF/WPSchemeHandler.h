#pragma once

#include <string>

#include "WallpaperEngine/Assets/AssetLocator.h"

#include "include/cef_resource_handler.h"
#include "include/wrapper/cef_helpers.h"

namespace WallpaperEngine::Data::Model {
struct Project;
}

namespace WallpaperEngine::WebBrowser::CEF {

using namespace WallpaperEngine::Assets;
using namespace WallpaperEngine::Data::Model;

/**
 * wp{id}:// actual handler called by cef to access files
 */
class WPSchemeHandler : public CefResourceHandler {
public:
    explicit WPSchemeHandler (const Project& project);

    bool Open (CefRefPtr<CefRequest> request, bool& handle_request, CefRefPtr<CefCallback> callback) override;

    void
    GetResponseHeaders (CefRefPtr<CefResponse> response, int64_t& response_length, CefString& redirectUrl) override;

    void Cancel () override;

    bool Skip (int64_t bytes_to_skip, int64_t& bytes_skipped, CefRefPtr<CefResourceSkipCallback> callback) override;

    bool
    Read (void* data_out, int bytes_to_read, int& bytes_read, CefRefPtr<CefResourceReadCallback> callback) override;

private:
    void parseRange (const std::string& range);

    const Project& m_project;

    const AssetLocator& m_assetLoader;
    ReadStreamSharedPtr m_contents = nullptr;
    std::string m_mimeType;
    // Total size of m_contents, or -1 when the stream cannot be measured.
    int64_t m_length = -1;
    // Requested byte range resolved against m_length; m_rangeStart < 0 means no range.
    int64_t m_rangeStart = -1;
    int64_t m_rangeEnd = -1;
    // The Range header asked for bytes past the end: answer 416 instead of the file.
    bool m_rangeUnsatisfiable = false;
    // Bytes left to send for this response, or -1 when unbounded.
    int64_t m_remaining = -1;
    // The handler already positioned the stream at the range start; CEF still
    // calls Skip for that same offset, which must not move the stream again.
    int64_t m_skipCredit = 0;

    IMPLEMENT_REFCOUNTING (WPSchemeHandler);
    DISALLOW_COPY_AND_ASSIGN (WPSchemeHandler);
};
};