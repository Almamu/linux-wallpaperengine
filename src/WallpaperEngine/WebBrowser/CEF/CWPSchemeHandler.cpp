#include "CWPSchemeHandler.h"
#include "WallpaperEngine/Assets/CAssetLoadException.h"
#include <iostream>

#include "MimeTypes.h"
#include "include/cef_parser.h"

using namespace WallpaperEngine::Assets;
using namespace WallpaperEngine::WebBrowser::CEF;

CWPSchemeHandler::CWPSchemeHandler(std::shared_ptr<const Core::CProject> project) :
    m_project (project) {
    this->m_container = this->m_project->getWallpaper ()->getProject ()->getContainer ();
}

bool CWPSchemeHandler::Open(CefRefPtr<CefRequest> request,
                             bool& handle_request,
                             CefRefPtr<CefCallback> callback) {
    DCHECK(!CefCurrentlyOn(TID_UI) && !CefCurrentlyOn(TID_IO));

#if !NDEBUG
    std::cout << "Processing request for path " << request->GetURL ().c_str () << std::endl;
#endif
    // url contains the full path, we need to get rid of the protocol
    // otherwise files won't be found
    CefURLParts parts;

    // url parsing is a must
    if (!CefParseURL (request->GetURL (), parts)) {
        return false;
    }

    std::string host = CefString(&parts.host);
    std::string path = CefString(&parts.path);

    std::string file = path.substr(1);

    try {
        // try to read the file on the current container, if the file doesn't exists
        // an exception will be thrown
        const char* mime = MimeTypes::getType (file.c_str ());

        if (!mime) {
            this->m_mimeType = "application/octet+stream";
        } else {
            this->m_mimeType = mime;
        }

        this->m_contents = this->m_container->readFile (file, &this->m_filesize);
        callback->Continue ();
    } catch (CAssetLoadException&) {
#if !NDEBUG
        std::cout << "Cannot read file " << file << std::endl;
#endif
    }

    handle_request = true;

    return true;
}


void CWPSchemeHandler::GetResponseHeaders(CefRefPtr<CefResponse> response,
                         int64_t& response_length,
                         CefString& redirectUrl) {
    CEF_REQUIRE_IO_THREAD();

    if (!this->m_contents) {
        response->SetError (ERR_FILE_NOT_FOUND);
        response->SetStatus (404);
        response_length = 0;
        return;
    }

    response->SetMimeType (this->m_mimeType);
    response->SetStatus (200);

    response_length = this->m_filesize;
}

void CWPSchemeHandler::Cancel () {
    CEF_REQUIRE_IO_THREAD();
}

bool CWPSchemeHandler::Read(void* data_out, int bytes_to_read, int& bytes_read,
                             CefRefPtr<CefResourceReadCallback> callback) {
    DCHECK(!CefCurrentlyOn(TID_UI) && !CefCurrentlyOn(TID_IO));

    bytes_read = 0;

    if (this->m_contents && this->m_offset < this->m_filesize) {
        int bytes_to_transfer = std::min (bytes_to_read, static_cast <int> (this->m_filesize - this->m_offset));

        memcpy (data_out, &this->m_contents [this->m_offset], bytes_to_transfer);

        this->m_offset += bytes_read = bytes_to_transfer;
        return true;
    }

    return false;
}