#pragma once

#include <string>

#include "WallpaperEngine/Core/CProject.h"
#include "include/cef_resource_handler.h"
#include "include/wrapper/cef_helpers.h"

namespace WallpaperEngine::Assets {
class CContainer;
}

namespace WallpaperEngine::WebBrowser::CEF {
/**
 * wp{id}:// actual handler called by cef to access files
 */
class CWPSchemeHandler : public CefResourceHandler {
  public:
    explicit CWPSchemeHandler(const Core::CProject* project);

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
    const Core::CProject* m_project;

    std::shared_ptr<const Assets::CContainer> m_container;
    std::shared_ptr<const uint8_t[]> m_contents;
    uint32_t m_filesize;
    std::string m_mimeType;
    uint32_t m_offset;


    IMPLEMENT_REFCOUNTING(CWPSchemeHandler);
    DISALLOW_COPY_AND_ASSIGN(CWPSchemeHandler);
};
};