#pragma once

#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include <glad/glad.h>
#include <sys/types.h>

#include "WallpaperEngine/WebBrowser/IPC/Protocol.h"
#include "WallpaperEngine/Assets/AssetLocator.h"

namespace WallpaperEngine::WebBrowser::IPC {

class WebManager {
public:
    WebManager () = default;
    ~WebManager ();

    static WebManager& get ();

    // Spawn the webhelper process and start the receive loop.
    void init ();
    // Tear down IPC and wait for the webhelper to exit.
    void shutdown ();

    // Browser lifecycle — called from CWeb constructor/destructor.
    void createBrowser (const std::string& uuid, const std::string& url, int width, int height, int fps);
    void destroyBrowser (const std::string& uuid);

    // Input forwarding — called from CWeb::updateMouse.
    void sendMouseMove (const std::string& uuid, float x, float y);
    void sendMouseButton (const std::string& uuid, float x, float y, uint8_t button, bool isRelease);

    // Resize — called from CWeb::setSize.
    void resize (const std::string& uuid, int width, int height);

    // Asset serving — the receive loop calls the locator when the webhelper
    // requests a file via the wp:// scheme.
    void registerAssetLocator (const std::string& uuid, const WallpaperEngine::Assets::AssetLocator* locator);
    void unregisterAssetLocator (const std::string& uuid);

    // Called from CWeb::renderFrame. Uploads the latest frame from shared memory
    // into `texture` if a new frame has arrived since the last call.
    // Returns true when a frame was uploaded.
    bool uploadFrameIfReady (const std::string& uuid, GLuint texture);

private:
    // ---- socket I/O ----
    void send (MessageType type, const MessageWriter& payload);
    bool readExact (void* buf, size_t len) const;

    // ---- receive loop ----
    void receiveLoop ();
    void handleFrameReady (MessageReader& r);
    void handleAssetRequest (MessageReader& r);

    // ---- shared memory ----
    struct ShmFrame {
        std::string shmName;
        int   fd     = -1;
        void* ptr    = nullptr;
        size_t size  = 0;
        int width    = 0;
        int height   = 0;
        std::atomic<bool> ready {false};

        // Non-copyable / non-movable because of the atomic member.
        ShmFrame () = default;
        ShmFrame (const ShmFrame&) = delete;
        ShmFrame& operator= (const ShmFrame&) = delete;
        ShmFrame (ShmFrame&&) = delete;
        ShmFrame& operator= (ShmFrame&&) = delete;
    };

    // Creates (or recreates on resize) the shm region for `uuid`.
    // Must be called with m_framesMutex held.
    ShmFrame& ensureFrame (const std::string& uuid, int width, int height);

    // Unmaps and unlinks the shm for `uuid`, removes it from m_frames.
    // Must be called with m_framesMutex held.
    void freeFrame (const std::string& uuid);

    // ---- webhelper process ----
    void spawnWebhelper ();

    // ---- state ----
    int   m_socket    = -1;
    pid_t m_helperPid = -1;

    std::mutex  m_sendMutex;
    std::thread m_receiveThread;
    std::atomic<bool> m_running {false};

    std::mutex m_assetMutex;
    std::map<std::string, const WallpaperEngine::Assets::AssetLocator*> m_assetLocators;

    std::mutex m_framesMutex;
    std::map<std::string, std::unique_ptr<ShmFrame>> m_frames;

    static std::unique_ptr<WebManager> s_instance;
};

} // namespace WallpaperEngine::WebBrowser::IPC

#define sWebManager (WallpaperEngine::WebBrowser::IPC::WebManager::get ())
