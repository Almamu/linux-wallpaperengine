#include "WPSchemeHandler.h"
#include "WallpaperEngine/Assets/AssetLoadException.h"
#include <algorithm>
#include <cstdlib>
#include <iostream>

#include "MimeTypes.h"
#include "include/cef_parser.h"

#include "WallpaperEngine/Data/Model/Project.h"

using namespace WallpaperEngine::WebBrowser::CEF;

WPSchemeHandler::WPSchemeHandler (const Project& project) :
    m_project (project), m_assetLoader (*this->m_project.assetLocator) { }

bool WPSchemeHandler::Open (CefRefPtr<CefRequest> request, bool& handle_request, CefRefPtr<CefCallback> callback) {
    DCHECK (!CefCurrentlyOn (TID_UI) && !CefCurrentlyOn (TID_IO));

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

    const std::string host = CefString (&parts.host);
    const std::string path = CefString (&parts.path);

    const std::string file = path.substr (1);

    try {
	// try to read the file on the current container, if the file doesn't exists
	// an exception will be thrown
	if (const char* mime = MimeTypes::getType (file.c_str ()); !mime) {
	    this->m_mimeType = "application/octet+stream";
	} else {
	    this->m_mimeType = mime;
	}

	this->m_contents = this->m_assetLoader.read (file);

	if (this->m_contents) {
	    // Range responses need the total length; media elements cannot seek without them.
	    this->m_contents->seekg (0, std::ios::end);
	    this->m_length = this->m_contents->tellg ();
	    this->m_contents->clear ();
	    this->m_contents->seekg (0, std::ios::beg);

	    if (this->m_length < 0 || !this->m_contents->good ()) {
		this->m_contents->clear ();
		this->m_length = -1;
	    }

	    this->parseRange (request->GetHeaderByName ("Range"));

	    // serve the range ourselves: CEF handles neither the end of a range nor
	    // the "bytes=-N" suffix form, it only skips to the start of "bytes=N-"
	    if (this->m_rangeStart >= 0) {
		this->m_contents->seekg (this->m_rangeStart, std::ios::beg);
		this->m_remaining = this->m_rangeEnd - this->m_rangeStart + 1;
		this->m_skipCredit = this->m_rangeStart;
	    }

#if !NDEBUG
	    std::cout << "Serving " << file << " range '" << request->GetHeaderByName ("Range").ToString () << "' of "
		      << this->m_length << " bytes" << std::endl;
#endif
	}

	callback->Continue ();
    } catch (AssetLoadException&) {
#if !NDEBUG
	std::cout << "Cannot read file " << file << std::endl;
#endif
    }

    handle_request = true;

    return true;
}

void WPSchemeHandler::GetResponseHeaders (
    CefRefPtr<CefResponse> response, int64_t& response_length, CefString& redirectUrl
) {
    CEF_REQUIRE_IO_THREAD ();

    if (!this->m_contents) {
	response->SetError (ERR_FILE_NOT_FOUND);
	response->SetStatus (404);
	response_length = 0;
	return;
    }

    response->SetMimeType (this->m_mimeType);

    CefResponse::HeaderMap headers;
    response->GetHeaderMap (headers);

    if (this->m_length >= 0) {
	headers.insert ({ "Accept-Ranges", "bytes" });
    }

    if (this->m_rangeUnsatisfiable) {
	headers.insert ({ "Content-Range", "bytes */" + std::to_string (this->m_length) });
	response->SetStatus (416);
	response->SetStatusText ("Range Not Satisfiable");
	response->SetHeaderMap (headers);
	response_length = 0;
	return;
    }

    // the status line and Content-Range must come from the handler
    if (this->m_rangeStart >= 0) {
	headers.insert (
	    { "Content-Range",
	      "bytes " + std::to_string (this->m_rangeStart) + "-" + std::to_string (this->m_rangeEnd) + "/"
		  + std::to_string (this->m_length) }
	);
	response->SetStatus (206);
	response->SetStatusText ("Partial Content");
    } else {
	response->SetStatus (200);
    }

    response->SetHeaderMap (headers);

    // length of the body actually sent; -1 signals an unknown-length file
    response_length = this->m_rangeStart >= 0 ? this->m_rangeEnd - this->m_rangeStart + 1 : this->m_length;
}

void WPSchemeHandler::parseRange (const std::string& range) {
    this->m_rangeStart = this->m_rangeEnd = -1;
    this->m_rangeUnsatisfiable = false;

    // only a single "bytes=start-[end]" or "bytes=-suffix" range is honoured
    if (this->m_length < 0 || range.rfind ("bytes=", 0) != 0 || range.find (',') != std::string::npos) {
	return;
    }

    const std::string spec = range.substr (6);
    const size_t dash = spec.find ('-');

    if (dash == std::string::npos) {
	return;
    }

    const std::string first = spec.substr (0, dash);
    const std::string last = spec.substr (dash + 1);
    int64_t start;
    int64_t end = this->m_length - 1;

    if (first.empty ()) {
	if (last.empty ()) {
	    return;
	}

	const int64_t suffix = std::strtoll (last.c_str (), nullptr, 10);

	// "bytes=-0" asks for nothing
	if (suffix <= 0) {
	    this->m_rangeUnsatisfiable = true;
	    return;
	}

	start = suffix >= this->m_length ? 0 : this->m_length - suffix;
    } else {
	start = std::strtoll (first.c_str (), nullptr, 10);

	// media probes the end of the file with "bytes=<size>-"; a 200 here makes it retry forever
	if (start >= this->m_length) {
	    this->m_rangeUnsatisfiable = true;
	    return;
	}

	if (!last.empty ()) {
	    end = std::min<int64_t> (std::strtoll (last.c_str (), nullptr, 10), this->m_length - 1);
	}
    }

    if (start < 0) {
	return;
    }

    // nothing of an empty file can be served, whatever the range
    if (start >= this->m_length) {
	this->m_rangeUnsatisfiable = true;
	return;
    }

    // "bytes=50-10" and the like
    if (end < start) {
	this->m_rangeUnsatisfiable = true;
	return;
    }

    this->m_rangeStart = start;
    this->m_rangeEnd = end;
}

void WPSchemeHandler::Cancel () { CEF_REQUIRE_IO_THREAD (); }

bool WPSchemeHandler::Skip (
    int64_t bytes_to_skip, int64_t& bytes_skipped, CefRefPtr<CefResourceSkipCallback> callback
) {
    bytes_skipped = 0;

    if (!this->m_contents || bytes_to_skip <= 0) {
	return false;
    }

    // the stream already sits at the range start, so CEF's skip to it is a no-op
    const int64_t credited = std::min (bytes_to_skip, this->m_skipCredit);
    this->m_skipCredit -= credited;
    bytes_skipped = credited;

    if (credited == bytes_to_skip) {
	return true;
    }

    const std::streamoff before = this->m_contents->tellg ();
    this->m_contents->seekg (bytes_to_skip - credited, std::ios::cur);

    if (before < 0 || !this->m_contents->good ()) {
	bytes_skipped = -2; // ERR_FAILED
	return false;
    }

    const int64_t moved = this->m_contents->tellg () - before;
    bytes_skipped += moved;

    if (this->m_remaining > 0) {
	this->m_remaining = std::max<int64_t> (0, this->m_remaining - moved);
    }

    return bytes_skipped > 0;
}

bool WPSchemeHandler::Read (
    void* data_out, int bytes_to_read, int& bytes_read, CefRefPtr<CefResourceReadCallback> callback
) {
    DCHECK (!CefCurrentlyOn (TID_UI) && !CefCurrentlyOn (TID_IO));

    bytes_read = 0;

    if (this->m_rangeUnsatisfiable || this->m_remaining == 0 || this->m_contents->eof ()) {
	return false;
    }

    if (this->m_remaining > 0 && bytes_to_read > this->m_remaining) {
	bytes_to_read = static_cast<int> (this->m_remaining);
    }

    try {
	this->m_contents->read (static_cast<std::istream::char_type*> (data_out), bytes_to_read);
    } catch (std::ios::failure&) {
	bytes_read = -1;
	return false;
    }

    bytes_read = this->m_contents->gcount ();

    if (this->m_remaining > 0) {
	this->m_remaining -= bytes_read;
    }

    return bytes_read > 0;
}