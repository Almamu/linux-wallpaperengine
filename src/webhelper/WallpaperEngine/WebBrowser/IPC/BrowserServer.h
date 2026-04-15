#pragma once

#include <atomic>
#include <future>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "include/cef_browser.h"
#include "include/cef_task.h"
#include "WallpaperEngine/WebBrowser/IPC/Protocol.h"

namespace WallpaperEngine::WebBrowser::CEF {
class RenderHandler;
}

namespace WallpaperEngine::WebBrowser::IPC {

struct AssetData {
    std::string          mimeType;
    std::vector<uint8_t> data;
    bool                 found = false;
};

// Minimal CefTask wrapper for posting lambdas to CEF threads.
template <typename F>
class FuncTask : public CefTask {
public:
    explicit FuncTask (F fn) : m_fn (std::move (fn)) {}
    void Execute () override { m_fn (); }
    IMPLEMENT_REFCOUNTING (FuncTask);
private:
    F m_fn;
};

template <typename F>
CefRefPtr<CefTask> makeCefTask (F fn) {
    return new FuncTask<F> (std::move (fn));
}

class BrowserServer {
public:
    explicit BrowserServer (int socketFd);
    ~BrowserServer ();

    static void            init (int socketFd);
    static void            shutdown ();
    static BrowserServer&  get ();

    // Called by RenderHandler::OnPaint — sends FrameReady to core.
    void sendFrameReady (const std::string& uuid);

    // Called by WPSchemeHandler::Open — sends AssetRequest and blocks until
    // the core responds with the file contents.
    AssetData fetchAsset (const std::string& uuid, const std::string& path);

private:
    void send (MessageType type, const MessageWriter& payload);
    bool readExact (void* buf, size_t len);
    void receiveLoop ();

    void handleCreateBrowser  (MessageReader& r);
    void handleDestroyBrowser (MessageReader& r);
    void handleMouseMove      (MessageReader& r);
    void handleMouseButton    (MessageReader& r);
    void handleResize         (MessageReader& r);
    void handleAssetResponse  (MessageReader& r);

    struct BrowserEntry {
        CefRefPtr<CefBrowser>                        browser;
        CefRefPtr<WallpaperEngine::WebBrowser::CEF::RenderHandler> renderHandler;
    };

    int         m_socket = -1;
    std::mutex  m_sendMutex;
    std::thread m_receiveThread;
    std::atomic<bool> m_running {false};

    std::mutex m_browsersMutex;
    std::map<std::string, BrowserEntry> m_browsers;

    std::mutex m_assetMutex;
    std::map<uint32_t, std::promise<AssetData>> m_pendingAssets;
    std::atomic<uint32_t> m_nextRequestId {0};

    static std::unique_ptr<BrowserServer> s_instance;
};

} // namespace WallpaperEngine::WebBrowser::IPC

#define sBrowserServer (WallpaperEngine::WebBrowser::IPC::BrowserServer::get ())
