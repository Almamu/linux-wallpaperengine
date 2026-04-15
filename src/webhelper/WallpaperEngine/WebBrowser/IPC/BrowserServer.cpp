#include "BrowserServer.h"

#include <cerrno>
#include <cstring>
#include <stdexcept>

#include <sys/socket.h>
#include <unistd.h>

#include "include/cef_app.h"
#include "include/cef_browser.h"
#include "include/cef_task.h"

#include "WallpaperEngine/Logging/Log.h"
#include "WallpaperEngine/WebBrowser/CEF/BrowserClient.h"
#include "WallpaperEngine/WebBrowser/CEF/RenderHandler.h"

using namespace WallpaperEngine::WebBrowser::IPC;

std::unique_ptr<BrowserServer> BrowserServer::s_instance;

BrowserServer::BrowserServer (int socketFd) : m_socket (socketFd) {
    m_running = true;
    m_receiveThread = std::thread (&BrowserServer::receiveLoop, this);
}

BrowserServer::~BrowserServer () {
    m_running = false;
    if (m_socket >= 0) {
        ::close (m_socket);
        m_socket = -1;
    }
    if (m_receiveThread.joinable ())
        m_receiveThread.join ();
}

void BrowserServer::init (int socketFd) {
    s_instance = std::make_unique<BrowserServer> (socketFd);
}

void BrowserServer::shutdown () {
    s_instance.reset ();
}

BrowserServer& BrowserServer::get () {
    return *s_instance;
}

// ---- socket I/O -------------------------------------------------------

void BrowserServer::send (MessageType type, const MessageWriter& payload) {
    const uint32_t t   = static_cast<uint32_t> (type);
    const uint32_t len = static_cast<uint32_t> (payload.data.size ());

    std::lock_guard lock (m_sendMutex);
    if (::send (m_socket, &t,   4,   MSG_NOSIGNAL) != 4 ||
        ::send (m_socket, &len, 4,   MSG_NOSIGNAL) != 4) {
        sLog.error ("BrowserServer: send (header): ", std::strerror (errno));
        return;
    }
    if (len > 0 &&
        ::send (m_socket, payload.data.data (), len, MSG_NOSIGNAL) !=
            static_cast<ssize_t> (len)) {
        sLog.error ("BrowserServer: send (payload): ", std::strerror (errno));
    }
}

bool BrowserServer::readExact (void* buf, size_t len) {
    auto* p = static_cast<uint8_t*> (buf);
    while (len > 0) {
        const ssize_t n = ::recv (m_socket, p, len, MSG_WAITALL);
        if (n <= 0) return false;
        p   += n;
        len -= static_cast<size_t> (n);
    }
    return true;
}

// ---- receive loop -----------------------------------------------------

void BrowserServer::receiveLoop () {
    while (m_running) {
        uint32_t type, payloadLen;
        if (!readExact (&type, 4) || !readExact (&payloadLen, 4))
            break;

        std::vector<uint8_t> payload (payloadLen);
        if (payloadLen > 0 && !readExact (payload.data (), payloadLen))
            break;

        MessageReader r (payload.data (), payload.size ());
        try {
            switch (static_cast<MessageType> (type)) {
                case MessageType::CreateBrowser:  handleCreateBrowser (r);  break;
                case MessageType::DestroyBrowser: handleDestroyBrowser (r); break;
                case MessageType::MouseMove:      handleMouseMove (r);      break;
                case MessageType::MouseButton:    handleMouseButton (r);    break;
                case MessageType::Resize:         handleResize (r);         break;
                case MessageType::AssetResponse:  handleAssetResponse (r);  break;
                default:
                    sLog.error ("BrowserServer: unknown message type ", type);
            }
        } catch (const std::exception& ex) {
            sLog.error ("BrowserServer: error handling message ", type, ": ", ex.what ());
        }
    }
    sLog.out ("BrowserServer: receive loop exited, quitting CEF");
    // Quit CEF's message loop so the webhelper can exit cleanly when the
    // core closes the socket.
    CefPostTask (TID_UI, makeCefTask ([] () { CefQuitMessageLoop (); }));
}

// ---- message handlers -------------------------------------------------

void BrowserServer::handleCreateBrowser (MessageReader& r) {
    const auto uuid    = r.readString ();
    const auto url     = r.readString ();
    const auto width   = r.readI32 ();
    const auto height  = r.readI32 ();
    const auto fps     = r.readI32 ();
    const auto shmName = r.readString ();

    CefPostTask (TID_UI, makeCefTask ([=, this] () {
        auto* renderHandler = new WallpaperEngine::WebBrowser::CEF::RenderHandler (uuid, this);
        renderHandler->configureShmem (shmName, width, height);

        CefWindowInfo windowInfo;
        windowInfo.SetAsWindowless (0);

        CefBrowserSettings browserSettings;
        browserSettings.windowless_frame_rate = std::min (fps, 60);

        auto client = new WallpaperEngine::WebBrowser::CEF::BrowserClient (renderHandler);
        auto browser = CefBrowserHost::CreateBrowserSync (
            windowInfo, client, url, browserSettings, nullptr, nullptr
        );

        std::lock_guard lock (m_browsersMutex);
        m_browsers[uuid] = {browser, renderHandler};
    }));
}

void BrowserServer::handleDestroyBrowser (MessageReader& r) {
    const auto uuid = r.readString ();

    CefPostTask (TID_UI, makeCefTask ([=, this] () {
        std::lock_guard lock (m_browsersMutex);
        auto it = m_browsers.find (uuid);
        if (it == m_browsers.end ())
            return;
        it->second.browser->GetHost ()->CloseBrowser (true);
        m_browsers.erase (it);
    }));
}

void BrowserServer::handleMouseMove (MessageReader& r) {
    const auto uuid = r.readString ();
    const auto x    = r.readFloat ();
    const auto y    = r.readFloat ();

    CefPostTask (TID_UI, makeCefTask ([=, this] () {
        std::lock_guard lock (m_browsersMutex);
        auto it = m_browsers.find (uuid);
        if (it == m_browsers.end ()) return;

        CefMouseEvent evt;
        evt.x = static_cast<int> (x);
        evt.y = static_cast<int> (y);
        it->second.browser->GetHost ()->SendMouseMoveEvent (evt, false);
    }));
}

void BrowserServer::handleMouseButton (MessageReader& r) {
    const auto uuid      = r.readString ();
    const auto x         = r.readFloat ();
    const auto y         = r.readFloat ();
    const auto button    = r.readU8 ();
    const auto isRelease = r.readU8 () != 0;

    CefPostTask (TID_UI, makeCefTask ([=, this] () {
        std::lock_guard lock (m_browsersMutex);
        auto it = m_browsers.find (uuid);
        if (it == m_browsers.end ()) return;

        CefMouseEvent evt;
        evt.x = static_cast<int> (x);
        evt.y = static_cast<int> (y);

        const auto btnType = (button == 0)
            ? CefBrowserHost::MouseButtonType::MBT_LEFT
            : CefBrowserHost::MouseButtonType::MBT_RIGHT;

        it->second.browser->GetHost ()->SendMouseClickEvent (evt, btnType, isRelease, 1);
    }));
}

void BrowserServer::handleResize (MessageReader& r) {
    const auto uuid    = r.readString ();
    const auto width   = r.readI32 ();
    const auto height  = r.readI32 ();
    const auto shmName = r.readString ();

    CefPostTask (TID_UI, makeCefTask ([=, this] () {
        std::lock_guard lock (m_browsersMutex);
        auto it = m_browsers.find (uuid);
        if (it == m_browsers.end ()) return;

        it->second.renderHandler->configureShmem (shmName, width, height);
        it->second.browser->GetHost ()->WasResized ();
    }));
}

void BrowserServer::handleAssetResponse (MessageReader& r) {
    const uint32_t    requestId = r.readU32 ();
    const std::string mimeType  = r.readString ();
    const auto        data      = r.readBytes ();

    std::lock_guard lock (m_assetMutex);
    auto it = m_pendingAssets.find (requestId);
    if (it == m_pendingAssets.end ())
        return;

    AssetData asset;
    asset.found    = !mimeType.empty ();
    asset.mimeType = mimeType;
    asset.data     = data;

    it->second.set_value (std::move (asset));
    m_pendingAssets.erase (it);
}

// ---- public API -------------------------------------------------------

void BrowserServer::sendFrameReady (const std::string& uuid) {
    MessageWriter w;
    w.writeString (uuid);
    send (MessageType::FrameReady, w);
}

AssetData BrowserServer::fetchAsset (const std::string& uuid, const std::string& path) {
    const uint32_t requestId = m_nextRequestId.fetch_add (1, std::memory_order_relaxed);

    std::future<AssetData> future;
    {
        std::lock_guard lock (m_assetMutex);
        auto& promise = m_pendingAssets[requestId];
        future = promise.get_future ();
    }

    MessageWriter w;
    w.writeU32 (requestId);
    w.writeString (uuid);
    w.writeString (path);
    send (MessageType::AssetRequest, w);

    return future.get ();
}
