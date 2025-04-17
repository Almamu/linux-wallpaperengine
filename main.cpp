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
class UIWindow;

void signalhandler(int sig)
{
    if (appPointer == nullptr)
        return;

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

      for (const std::string s : wallpaperPaths) {
        std::cout << s << "\r\n";
      }
      

      QApplication qapp(argc, argv);

      auto* uiWindow = new UIWindow(nullptr, &qapp);

      uiWindow->setupUIWindow(wallpaperPaths);

      uiWindow->show();

      return qapp.exec();


      /*

      auto* window = new QWidget();
      window->setWindowTitle("Wallpapers :3");
     
      auto* scrollArea = new QScrollArea(window); 
      scrollArea->setWidgetResizable(true);

      auto* container = new QWidget();

      auto* layout = new QGridLayout(container);
      
      int cols = 3; 

      // Wallpaper Process
      auto* wallpaperEngine = new QProcess(window);
      
      for (size_t i = 0; i < wallpaperPaths.size(); i++) {
        QPixmap pixmap(QString::fromStdString(wallpaperPaths[i] + "/preview.jpg"));
        if (pixmap.isNull()) {
          pixmap = QPixmap(256, 256);
          pixmap.fill(Qt::black);
        }
        
        auto* button = new QPushButton();
        button->setIcon(pixmap.scaled(256, 256, Qt::KeepAspectRatio));
        button->setText("Hii :3");
        button->setIconSize(QSize(256, 256));
        button->setFixedSize(384, 364);
        button->setProperty("path", QString::fromStdString(wallpaperPaths[i]));

        
        QAbstractButton::connect(button, &QPushButton::clicked, [button, &qapp, argc, argv, window, wallpaperEngine]() {
          QString clickedPath = button->property("path").toString();
          button->setEnabled(false);

          if (wallpaperEngine->state() == QProcess::Running) {
            // Stop WallpaperProcess
            wallpaperEngine->terminate();
            if (!wallpaperEngine->waitForFinished(3000)) {
              wallpaperEngine->kill();
              wallpaperEngine->waitForFinished();
            }
          }
          // start Wallpaper Process
          wallpaperEngine->start(QCoreApplication::applicationFilePath(), {"--screen-root", "DP-2", clickedPath});
          
          QObject::connect(wallpaperEngine, &QProcess::started, button, [=]() {
            button->setEnabled(true);
          });

          QObject::connect(wallpaperEngine, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), window, [](int exitcode, QProcess::ExitStatus status) {
            std::cout << exitcode << "\r\n";
          });
          // qapp.exit();
        });

        int row = i / cols;
        int col = i % cols;
        layout->addWidget(button, row, col);
      }

      QObject::connect(&qapp, &QCoreApplication::aboutToQuit, window, [&qapp, wallpaperEngine]() {
        wallpaperEngine->terminate(); 
        wallpaperEngine->waitForFinished(3000);
      });

      container->setLayout(layout);
      scrollArea->setWidget(container);

      auto* mainlayout = new QVBoxLayout(window);
      mainlayout->addWidget(scrollArea);
      window->setLayout(mainlayout);

      window->show();
      return qapp.exec();
      */
    }

    WallpaperEngine::WebBrowser::CWebBrowserContext webBrowserContext(argc, argv);
    std::string wallpaperPath = "/home/delia/.local/share/Steam/steamapps/workshop/content/431960/1838246129"; 
    std::string display = "DP-2";
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
