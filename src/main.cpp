#include <csignal>
#include <iostream>
#include <QApplication>
#include <QProcess>
#include <QObject>
#include <qcoreapplication.h>
#include <qglobal.h>
#include <qnamespace.h>
#include <qstandardpaths.h>
#include <string>
#include <filesystem>
#include <vector>
#include "Qt/SingleInstanceManager.h"
#include "Qt/UIWindow.h"
#include "Steam/FileSystem/FileSystem.h"
#include "WallpaperEngine/Application/CApplicationContext.h"
#include "WallpaperEngine/Application/CWallpaperApplication.h"
#include "WallpaperEngine/Logging/CLog.h"

WallpaperEngine::Application::CWallpaperApplication* appPointer;
QCoreApplication* globalApp = nullptr;
SingleInstanceManager* g_instanceManager = nullptr;


class UIWindow;

void signalhandler(int sig)
{
    if (appPointer == nullptr) {
      if(globalApp != nullptr) {
        if (g_instanceManager) g_instanceManager->cleanUpServer();
        globalApp->quit();
      } else return;
    }

    appPointer->signal (sig);
}

void initLogging ()
{
    sLog.addOutput (new std::ostream (std::cout.rdbuf ()));
    sLog.addError (new std::ostream (std::cerr.rdbuf ()));
}

int main (int argc, char* argv[]) {
    initLogging ();

    if (argc <= 1) {
      QApplication qapp(argc, argv);
      globalApp = &qapp;

      g_instanceManager = new SingleInstanceManager("linux-wallpaperengine");

      if (!g_instanceManager->tryListen()) {
        sLog.out("App is already running!");
        return 0;
      }

      std::string appDataLocation;
      
      // TODO: Use desktop file as marker
      // Data directory
      if (QCoreApplication::applicationDirPath() == INSTALL_PREFIX) {
        // is installed properly
        appDataLocation = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation).toStdString() + "/";
      } else {
        // is not installed
        appDataLocation = QCoreApplication::applicationDirPath().toStdString() + "/appData/";
      }
      sLog.out("AppDataLocation: " + appDataLocation);
      if (!std::filesystem::exists(appDataLocation)) {
        if (!std::filesystem::create_directory(appDataLocation)) {
          sLog.error("Could't create appData directory");
        }
      }

      std::string path = Steam::FileSystem::workshopDirectory(431960);
      sLog.out("Found workshopDirectory: " + path);

      std::vector<std::string> wallpaperPaths;

      for (const std::filesystem::directory_entry & entry : std::filesystem::directory_iterator(path)) {
        wallpaperPaths.push_back(entry.path());
      }

      sLog.out("Found " + std::to_string(wallpaperPaths.size()) + " Installed Wallpapers!");

      // Signal for properly close the app 
      std::signal (SIGINT, signalhandler);
      std::signal (SIGTERM, signalhandler);

      sLog.out("Starting App..");

      auto* uiWindow = new UIWindow(nullptr, &qapp, g_instanceManager, appDataLocation);

      uiWindow->setupUIWindow(wallpaperPaths);

      uiWindow->show();

      return qapp.exec();
    }

    // WallpaperEngine::WebBrowser::CWebBrowserContext webBrowserContext(argc, argv);
    WallpaperEngine::Application::CApplicationContext appContext (argc, argv);
    
    // halt if the list-properties option was specified
    if (appContext.settings.general.onlyListProperties)
      return 0;

    WallpaperEngine::Application::CWallpaperApplication app (appContext);

    appPointer = &app;

    // attach signals to gracefully stop
    std::signal (SIGINT, signalhandler);
    std::signal (SIGTERM, signalhandler);

    // show the wallpaper application
    app.show ();

    appPointer = nullptr;
}
