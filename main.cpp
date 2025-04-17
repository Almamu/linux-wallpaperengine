#include <csignal>
#include <cstddef>
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
#include <iterator>
#include <qboxlayout.h>
#include <qcoreapplication.h>
#include <qglobal.h>
#include <qgridlayout.h>
#include <qlabel.h>
#include <qnamespace.h>
#include <qprocess.h>
#include <qpushbutton.h>
#include <qscrollarea.h>
#include <qwidget.h>
#include <string>
#include <filesystem>
#include <vector>

#include "Qt/UIWindow.h"
#include "Steam/FileSystem/FileSystem.h"
#include "WallpaperEngine/Application/CApplicationContext.h"
#include "WallpaperEngine/Application/CWallpaperApplication.h"
#include "WallpaperEngine/WebBrowser/CWebBrowserContext.h"
#include "common.h"

WallpaperEngine::Application::CWallpaperApplication* appPointer;
QCoreApplication* globalApp = nullptr;

class UIWindow;

void signalhandler(int sig)
{
    if (appPointer == nullptr) {
      if(globalApp != nullptr) {
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
    bool runGui = false; 

    for (int i = 1; i < argc; i++) {
      if(std::strcmp(argv[i], "--gui") == 0) {
        runGui = true;
        break;
      }
    }

    if (runGui) {
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

      auto* uiWindow = new UIWindow(nullptr, &qapp);

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
