#pragma once

namespace WallpaperEngine::WebBrowser {
    class CWebBrowserContext {
      public:
        CWebBrowserContext (int argc, char** argv);
        ~CWebBrowserContext();

        void markAsUsed();
        void stop();

      private:
        /**
         * Handles the actual initialization logic
         */
        void delayedInitialization();

        int m_argc;
        char** m_argv;
        bool m_stopped;
    };
} // namespace WallpaperEngine::WebBrowser
