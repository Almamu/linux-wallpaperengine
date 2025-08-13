#pragma once

#include <string>

#include "include/cef_resource_handler.h"
#include "include/wrapper/cef_helpers.h"

namespace WallpaperEngine::Assets {
class CContainer;
}

namespace WallpaperEngine::Data::Model {
class Project;
}

namespace WallpaperEngine::WebBrowser::CEF {

using namespace WallpaperEngine::Data::Model;

/**
 * wp{id}:// actual handler called by cef to access files
 */
class CWPSchemeHandler : public CefResourceHandler {
  public:
    explicit CWPSchemeHandler(const Project& project);

    bool Open(CefRefPtr<CefRequest> request,
               bool& handle_request,
               CefRefPtr<CefCallback> callback) override;

    void GetResponseHeaders(CefRefPtr<CefResponse> response,
                             int64_t& response_length,
                             CefString& redirectUrl) override;

    void Cancel() override;

    bool Read(void* data_out, int bytes_to_read, int& bytes_read,
               CefRefPtr<CefResourceReadCallback> callback) override;

  private:
    const Project& m_project;

    const Assets::CContainer& m_container;
    std::shared_ptr<const uint8_t[]> m_contents = nullptr;
    uint32_t m_filesize = 0;
    std::string m_mimeType = "";
    uint32_t m_offset = 0;


    IMPLEMENT_REFCOUNTING(CWPSchemeHandler);
    DISALLOW_COPY_AND_ASSIGN(CWPSchemeHandler);
};
};