#include "CWPSchemeHandler.h"
#include "WallpaperEngine/Assets/CAssetLoadException.h"
#include <iostream>

#include "MimeTypes.h"
#include "include/cef_parser.h"

using namespace WallpaperEngine::Assets;
using namespace WallpaperEngine::WebBrowser::CEF;

CWPSchemeHandler::CWPSchemeHandler(const Core::CProject* project) :
    m_project (project),
    m_contents (nullptr),
    m_filesize (0),
    m_mimeType (),
    m_offset (0) {
    this->m_container = this->m_project->getWallpaper ()->getProject ().getContainer ();
}

bool CWPSchemeHandler::ProcessRequest(CefRefPtr<CefRequest> request,
                     CefRefPtr<CefCallback> callback) {
    CEF_REQUIRE_IO_THREAD();

    // free previous file so we can properly build the right chain of responses
    delete this->m_contents;

    std::cout << "ProcessRequest for " << request->GetURL ().c_str () << std::endl;
    // url contains the full path, we need to get rid of the protocol
    // otherwise files won't be found
    CefURLParts parts;

    // url parsing is a must
    if (!CefParseURL (request->GetURL (), parts)) {
        return false;
    }

    std::string path = CefString(&parts.path);

    // remove leading slashes from the path
    path = path.substr (2);

    try {
        // try to read the file on the current container, if the file doesn't exists
        // an exception will be thrown
        this->m_mimeType = MimeTypes::getType (path.c_str ());
        this->m_contents = this->m_container->readFile (path, &this->m_filesize);
        callback->Continue ();
    } catch (CAssetLoadException&) {
        // not found in this container, next try
        return false;
    }

    return true;
}


void CWPSchemeHandler::GetResponseHeaders(CefRefPtr<CefResponse> response,
                         int64_t& response_length,
                         CefString& redirectUrl) {
    CEF_REQUIRE_IO_THREAD();

    if (!this->m_contents) {
        response->SetStatus (404);
        return;
    }

    response->SetMimeType (this->m_mimeType);
    response->SetStatus (200);

    response_length = this->m_filesize;
}

void CWPSchemeHandler::Cancel () {
    CEF_REQUIRE_IO_THREAD();
}

bool CWPSchemeHandler::ReadResponse(void* data_out,
                   int bytes_to_read,
                   int& bytes_read,
                   CefRefPtr<CefCallback> callback) {
    CEF_REQUIRE_IO_THREAD();

    bytes_read = 0;

    if (this->m_offset < this->m_filesize) {
        int bytes_to_transfer = std::min (bytes_to_read, static_cast <int> (this->m_filesize - this->m_offset));

        memcpy (data_out, &this->m_contents [this->m_offset], bytes_to_transfer);

        bytes_read = bytes_to_transfer;
        return true;
    }

    return false;
}