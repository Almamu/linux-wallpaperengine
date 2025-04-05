#pragma once

namespace WallpaperEngine::WebBrowser {
    class CWebBrowserContext {
      public:
        CWebBrowserContext (int argc, char** argv);
        ~CWebBrowserContext();

        void markAsUsed();
        bool isUsed();
        void stop();

      private:
        bool m_stopped;
        bool m_inUse;
    };
} // namespace WallpaperEngine::WebBrowser
