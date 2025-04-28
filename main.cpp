#include <csignal>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <QApplication>
#include <QLabel>
#include <QGridLayout>
#include <QScrollArea>
#include <QPushButton>
#include <QProcess>
#include <QFileInfo>
#include <QObject>
#include <QLocalServer>
#include <iterator>
#include <qboxlayout.h>
#include <qcoreapplication.h>
#include <qglobal.h>
#include <qgridlayout.h>
#include <qlabel.h>
#include <qnamespace.h>
#include <qobject.h>
#include <qprocess.h>
#include <qpushbutton.h>
#include <qscrollarea.h>
#include <qwidget.h>
#include <string>
#include <filesystem>
#include <vector>

#include "Qt/SingleInstanceManager.h"
#include "Qt/UIWindow.h"
#include "Steam/FileSystem/FileSystem.h"
#include "WallpaperEngine/Application/CApplicationContext.h"
#include "WallpaperEngine/Application/CWallpaperApplication.h"
#include "WallpaperEngine/WebBrowser/CWebBrowserContext.h"
#include "common.h"

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

    g_instanceManager = new SingleInstanceManager("linux-wallpaperengine");

    if (argc <= 1) {
      if (!g_instanceManager->tryListen()) {
        std::cout << "App is already running!!!!\n";
        return 0;
      }
      std::string path = Steam::FileSystem::workshopDirectory(431960);

      std::vector<std::string> wallpaperPaths;

      for (const std::filesystem::directory_entry & entry : std::filesystem::directory_iterator(path)) {
        wallpaperPaths.push_back(entry.path());
      }

      QApplication qapp(argc, argv);
      globalApp = &qapp;

      // Signal for properly close the app 
      std::signal (SIGINT, signalhandler);
      std::signal (SIGTERM, signalhandler);

      auto* uiWindow = new UIWindow(nullptr, &qapp, g_instanceManager);

      uiWindow->setupUIWindow(wallpaperPaths);

      uiWindow->show();

      return qapp.exec();
    }

    WallpaperEngine::WebBrowser::CWebBrowserContext webBrowserContext(argc, argv);
    WallpaperEngine::Application::CApplicationContext appContext (argc, argv);
    
    // halt if the list-properties option was specified
    if (appContext.settings.general.onlyListProperties)
      return 0;

    WallpaperEngine::Application::CWallpaperApplication app (appContext, webBrowserContext);

    appPointer = &app;

    // attach signals to gracefully stop
    std::signal (SIGINT, signalhandler);
    std::signal (SIGTERM, signalhandler);

    // show the wallpaper application
    app.show ();

    appPointer = nullptr;
}
