#include "RenderHandler.h"

#include <cstring>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>

#include "WallpaperEngine/Logging/Log.h"
#include "WallpaperEngine/WebBrowser/IPC/BrowserServer.h"

using namespace WallpaperEngine::WebBrowser::CEF;

RenderHandler::RenderHandler (
    const std::string& uuid, WallpaperEngine::WebBrowser::IPC::BrowserServer* server
) : m_uuid (uuid), m_server (server) {}

RenderHandler::~RenderHandler () {
    closeShmem ();
}

void RenderHandler::closeShmem () {
    if (m_shmPtr && m_shmPtr != MAP_FAILED) {
        munmap (m_shmPtr, m_shmSize);
        m_shmPtr = nullptr;
    }
    if (m_shmFd >= 0) {
        ::close (m_shmFd);
        m_shmFd = -1;
    }
}

void RenderHandler::configureShmem (const std::string& shmName, int width, int height) {
    closeShmem ();

    m_width   = width;
    m_height  = height;
    m_shmSize = static_cast<size_t> (width) * static_cast<size_t> (height) * 4;

    m_shmFd = shm_open (shmName.c_str (), O_RDWR, 0);
    if (m_shmFd < 0) {
        sLog.error ("RenderHandler: shm_open(", shmName, "): ", strerror (errno));
        return;
    }

    m_shmPtr = mmap (nullptr, m_shmSize, PROT_WRITE, MAP_SHARED, m_shmFd, 0);
    if (m_shmPtr == MAP_FAILED) {
        sLog.error ("RenderHandler: mmap: ", strerror (errno));
        m_shmPtr = nullptr;
        ::close (m_shmFd);
        m_shmFd = -1;
    }
}

void RenderHandler::GetViewRect (CefRefPtr<CefBrowser> /*browser*/, CefRect& rect) {
    rect = CefRect (0, 0, m_width, m_height);
}

void RenderHandler::OnPaint (
    CefRefPtr<CefBrowser> /*browser*/, PaintElementType /*type*/,
    const RectList& /*dirtyRects*/, const void* buffer, int width, int height
) {
    if (!m_shmPtr || m_shmPtr == MAP_FAILED)
        return;

    const size_t needed = static_cast<size_t> (width) * static_cast<size_t> (height) * 4;
    if (needed > m_shmSize)
        return;

    std::memcpy (m_shmPtr, buffer, needed);
    m_server->sendFrameReady (m_uuid);
}
