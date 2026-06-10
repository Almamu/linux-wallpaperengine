#include "WebManager.h"

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <iterator>
#include <stdexcept>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

#include "MimeTypes.h"
#include "WallpaperEngine/Assets/AssetLoadException.h"
#include "WallpaperEngine/Logging/Log.h"

namespace WallpaperEngine::WebBrowser::IPC {

std::unique_ptr<WebManager> WebManager::s_instance;

WebManager::~WebManager () { shutdown (); }

WebManager& WebManager::get () {
    if (!s_instance) {
	s_instance = std::make_unique<WebManager> ();
    }
    return *s_instance;
}

// ---- lifecycle -------------------------------------------------------

void WebManager::init () {
    if (m_running) {
        return;
    }

    spawnWebhelper ();
    m_running = true;
    m_receiveThread = std::thread (&WebManager::receiveLoop, this);
    sLog.debug ("WebManager: initialized, webhelper pid=", m_helperPid);
}

void WebManager::shutdown () {
    if (!m_running.exchange (false)) {
	return;
    }

    // Closing the socket unblocks recv() in the receive thread.
    if (m_socket >= 0) {
	::close (m_socket);
	m_socket = -1;
    }

    if (m_receiveThread.joinable ()) {
	m_receiveThread.join ();
    }

    if (m_helperPid > 0) {
	// Give the webhelper a moment to exit cleanly after EOF on its socket.
	waitpid (m_helperPid, nullptr, 0);
	m_helperPid = -1;
    }

    // No concurrent threads remain — clean up shm without locking.
    for (auto& [uuid, frame] : m_frames) {
	if (frame->ptr && frame->ptr != MAP_FAILED) {
	    munmap (frame->ptr, frame->size);
	}
	if (frame->fd >= 0) {
	    ::close (frame->fd);
	}
	shm_unlink (frame->shmName.c_str ());
    }
    m_frames.clear ();
}

void WebManager::spawnWebhelper () {
    int fds[2];
    if (::socketpair (AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, fds) < 0) {
	sLog.exception ("WebManager: socketpair: ", std::strerror (errno));
    }

    const pid_t pid = ::fork ();
    if (pid < 0) {
	sLog.exception ("WebManager: fork: ", std::strerror (errno));
    }

    if (pid == 0) {
	// Child: close the parent's end, then exec the webhelper.
	::close (fds[0]);

	// SOCK_CLOEXEC set both fds; clear it on the child's end so it
	// survives exec.
	const int flags = ::fcntl (fds[1], F_GETFD);
	::fcntl (fds[1], F_SETFD, flags & ~FD_CLOEXEC);

	char fdArg[32];
	std::snprintf (fdArg, sizeof (fdArg), "--ipc-fd=%d", fds[1]);

	::execl (WPENGINE_WEBHELPER_PATH, "linux-wallpaperengine-webhelper", fdArg, nullptr);
	// execl only returns on failure.
	std::perror ("WebManager: execl");
	::_exit (1);
    }

    // Parent: close the child's end.
    ::close (fds[1]);
    m_socket = fds[0];
    m_helperPid = pid;
}

// ---- socket I/O ------------------------------------------------------

void WebManager::send (MessageType type, const MessageWriter& payload) {
    const auto t = static_cast<uint32_t> (type);
    const auto len = static_cast<uint32_t> (payload.data.size ());

    std::lock_guard lock (m_sendMutex);

    if (::send (m_socket, &t, 4, MSG_NOSIGNAL) != 4 || ::send (m_socket, &len, 4, MSG_NOSIGNAL) != 4) {
	sLog.error ("WebManager: send (header): ", std::strerror (errno));
	return;
    }
    if (len > 0 && ::send (m_socket, payload.data.data (), len, MSG_NOSIGNAL) != static_cast<ssize_t> (len)) {
	sLog.error ("WebManager: send (payload): ", std::strerror (errno));
    }
}

bool WebManager::readExact (void* buf, size_t len) const {
    auto* p = static_cast<uint8_t*> (buf);
    while (len > 0) {
	const ssize_t n = ::recv (m_socket, p, len, MSG_WAITALL);
	if (n <= 0) {
	    return false;
	}
	p += n;
	len -= static_cast<size_t> (n);
    }
    return true;
}

// ---- receive loop ----------------------------------------------------

void WebManager::receiveLoop () {
    while (m_running) {
	uint32_t type, payloadLen;

	if (!readExact (&type, 4) || !readExact (&payloadLen, 4)) {
	    break;
	}

	std::vector<uint8_t> payload (payloadLen);
	if (payloadLen > 0 && !readExact (payload.data (), payloadLen)) {
	    break;
	}

	MessageReader r (payload.data (), payload.size ());
	try {
	    switch (static_cast<MessageType> (type)) {
		case MessageType::FrameReady:
		    handleFrameReady (r);
		    break;
		case MessageType::AssetRequest:
		    handleAssetRequest (r);
		    break;
		default:
		    sLog.error ("WebManager: unknown inbound message type ", type);
	    }
	} catch (const std::exception& ex) {
	    sLog.error ("WebManager: error handling message ", type, ": ", ex.what ());
	}
    }
    sLog.debug ("WebManager: receive loop exited");
}

void WebManager::handleFrameReady (MessageReader& r) {
    const std::string uuid = r.readString ();
    std::lock_guard lock (m_framesMutex);
    const auto it = m_frames.find (uuid);
    if (it != m_frames.end ()) {
	it->second->ready.store (true, std::memory_order_release);
    }
}

void WebManager::handleAssetRequest (MessageReader& r) {
    const uint32_t requestId = r.readU32 ();
    const std::string uuid = r.readString ();
    const std::string path = r.readString ();

    // Resolve the asset locator while holding its lock briefly.
    const Assets::AssetLocator* locator = nullptr;
    {
	std::lock_guard lock (m_assetMutex);
	const auto it = m_assetLocators.find (uuid);
	if (it != m_assetLocators.end ()) {
	    locator = it->second;
	}
    }

    MessageWriter w;
    w.writeU32 (requestId);

    if (!locator) {
	w.writeString (""); // empty mime signals error to the webhelper
	w.writeBytes (nullptr, 0);
	send (MessageType::AssetResponse, w);
	return;
    }

    try {
	const char* mime = MimeTypes::getType (path.c_str ());
	w.writeString (mime ? mime : "application/octet-stream");

	auto stream = locator->read (path);
	const std::vector<uint8_t> data { std::istreambuf_iterator<char> { *stream },
					  std::istreambuf_iterator<char> {} };
	w.writeBytes (data.data (), data.size ());
    } catch (const WallpaperEngine::Assets::AssetLoadException&) {
	// File not found — re-encode as error.
	w = {};
	w.writeU32 (requestId);
	w.writeString ("");
	w.writeBytes (nullptr, 0);
    }

    send (MessageType::AssetResponse, w);
}

// ---- shared memory helpers ------------------------------------------

WebManager::ShmFrame& WebManager::ensureFrame (const std::string& uuid, int width, int height) {
    const auto it = m_frames.find (uuid);
    if (it != m_frames.end ()) {
	ShmFrame& f = *it->second;
	if (f.width == width && f.height == height) {
	    return f;
	}
	// Dimensions changed — tear down the old region.
	freeFrame (uuid);
    }

    auto frame = std::make_unique<ShmFrame> ();
    frame->shmName = "/wp-" + uuid;
    frame->width = width;
    frame->height = height;
    frame->size = static_cast<size_t> (width) * static_cast<size_t> (height) * 4;

    frame->fd = shm_open (frame->shmName.c_str (), O_CREAT | O_RDWR, 0600);
    if (frame->fd < 0) {
	sLog.exception ("WebManager: shm_open(", frame->shmName, "): ", std::strerror (errno));
    }

    if (ftruncate (frame->fd, static_cast<off_t> (frame->size)) < 0) {
	sLog.exception ("WebManager: ftruncate: ", std::strerror (errno));
    }

    frame->ptr = mmap (nullptr, frame->size, PROT_READ, MAP_SHARED, frame->fd, 0);
    if (frame->ptr == MAP_FAILED) {
	sLog.exception ("WebManager: mmap: ", std::strerror (errno));
    }

    ShmFrame& ref = *frame;
    m_frames[uuid] = std::move (frame);
    return ref;
}

void WebManager::freeFrame (const std::string& uuid) {
    const auto it = m_frames.find (uuid);
    if (it == m_frames.end ()) {
	return;
    }
    ShmFrame& f = *it->second;
    if (f.ptr && f.ptr != MAP_FAILED) {
	munmap (f.ptr, f.size);
    }
    if (f.fd >= 0) {
	::close (f.fd);
    }
    shm_unlink (f.shmName.c_str ());
    m_frames.erase (it);
}

// ---- public API ------------------------------------------------------

void WebManager::createBrowser (const std::string& uuid, const std::string& url, int width, int height, int fps) {
    std::lock_guard lock (m_framesMutex);
    const ShmFrame& frame = ensureFrame (uuid, width, height);

    MessageWriter w;
    w.writeString (uuid);
    w.writeString (url);
    w.writeI32 (width);
    w.writeI32 (height);
    w.writeI32 (fps);
    w.writeString (frame.shmName);
    send (MessageType::CreateBrowser, w);
}

void WebManager::destroyBrowser (const std::string& uuid) {
    MessageWriter w;
    w.writeString (uuid);
    send (MessageType::DestroyBrowser, w);

    std::lock_guard lock (m_framesMutex);
    freeFrame (uuid);
}

void WebManager::sendMouseMove (const std::string& uuid, float x, float y) {
    MessageWriter w;
    w.writeString (uuid);
    w.writeFloat (x);
    w.writeFloat (y);
    send (MessageType::MouseMove, w);
}

void WebManager::sendMouseButton (const std::string& uuid, float x, float y, uint8_t button, bool isRelease) {
    MessageWriter w;
    w.writeString (uuid);
    w.writeFloat (x);
    w.writeFloat (y);
    w.writeU8 (button);
    w.writeU8 (isRelease ? 1 : 0);
    send (MessageType::MouseButton, w);
}

void WebManager::resize (const std::string& uuid, int width, int height) {
    std::lock_guard lock (m_framesMutex);
    const ShmFrame& frame = ensureFrame (uuid, width, height);

    MessageWriter w;
    w.writeString (uuid);
    w.writeI32 (width);
    w.writeI32 (height);
    w.writeString (frame.shmName);
    send (MessageType::Resize, w);
}

void WebManager::registerAssetLocator (const std::string& uuid, const Assets::AssetLocator* locator) {
    std::lock_guard lock (m_assetMutex);
    m_assetLocators[uuid] = locator;
}

void WebManager::unregisterAssetLocator (const std::string& uuid) {
    std::lock_guard lock (m_assetMutex);
    m_assetLocators.erase (uuid);
}

bool WebManager::uploadFrameIfReady (const std::string& uuid, GLuint texture) {
    std::lock_guard lock (m_framesMutex);
    const auto it = m_frames.find (uuid);
    if (it == m_frames.end ()) {
	return false;
    }

    ShmFrame& frame = *it->second;
    if (!frame.ready.load (std::memory_order_acquire)) {
	return false;
    }

    frame.ready.store (false, std::memory_order_release);

    glActiveTexture (GL_TEXTURE0);
    glBindTexture (GL_TEXTURE_2D, texture);
    glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA, frame.width, frame.height, 0, GL_BGRA, GL_UNSIGNED_BYTE, frame.ptr);
    glBindTexture (GL_TEXTURE_2D, 0);

    return true;
}

} // namespace WallpaperEngine::WebBrowser::IPC
