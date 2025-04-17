#include "UIWindow.h"
#include <iostream>
#include <qapplication.h>
#include <qboxlayout.h>
#include <qcombobox.h>
#include <qcursor.h>
#include <qlabel.h>
#include <qlineedit.h>
#include <qprocess.h>
#include <qwidget.h>
#include <string>
#include <vector>

UIWindow::UIWindow(QWidget* parent, QApplication* qapp) {
  this->qapp = qapp; 
  this->screenSelector = new QComboBox(this);
  this->extraFlagsInput = new QLineEdit(this);
  this->wallpaperEngine = new QProcess(this);
}

void UIWindow::setupUIWindow(std::vector<std::string> wallpaperPaths) {
  this->setWindowTitle("Wallpapers :3");
  
  // palette
  auto* pal = new QPalette();
  pal->setColor(QPalette::Window, QColor(100, 100, 100, 255));
  this->setAutoFillBackground(true);
  this->setPalette(*pal);
   
  auto* scrollArea = new QScrollArea(this); 
  scrollArea->setWidgetResizable(true);
  scrollArea->setPalette(*pal);

  auto* container = new QWidget();

  auto* layout = new QGridLayout(container);
  
  int cols = 5; 

  for (size_t i = 0; i < wallpaperPaths.size(); i++) {
    QPixmap pixmap(QString::fromStdString(wallpaperPaths[i] + "/preview.jpg"));
    if (pixmap.isNull()) {
      pixmap = QPixmap(256, 256);
      pixmap.fill(Qt::black);
    }
    
    auto* button = new QPushButton();
    button->setIcon(pixmap.scaled(256, 256, Qt::KeepAspectRatio));
    button->setIconSize(QSize(256, 256));
    button->setFixedSize(256, 256);
    button->setProperty("path", QString::fromStdString(wallpaperPaths[i]));

    
    QAbstractButton::connect(button, &QPushButton::clicked, [button, this]() {
      QString clickedPath = button->property("path").toString();
      button->setEnabled(false);

      this->selectedWallpapers[this->screenSelector->currentText().toStdString()] = clickedPath.toStdString();
      this->extraFlags[this->screenSelector->currentText().toStdString()] = split(this->extraFlagsInput->text().toStdString(), ' ');

      startNewWallpaperEngine();

      QObject::connect(wallpaperEngine, &QProcess::started, button, [=]() {
        button->setEnabled(true);
      });
      // qapp.exit();
    });

    int row = i / cols;
    int col = i % cols;
    layout->addWidget(button, row, col);
  }

  QObject::connect(this->qapp, &QCoreApplication::aboutToQuit, this, [this]() {
    wallpaperEngine->terminate(); 
    wallpaperEngine->waitForFinished(3000);
  });

  container->setLayout(layout);
  scrollArea->setWidget(container);

  // screen select dropdown
  const QList<QScreen*> screens = QGuiApplication::screens();
  for (QScreen* screen : screens) {
    this->screenSelector->addItem(screen->name());
  }
  this->screenSelector->setCurrentIndex(0);
  this->screenSelector->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  this->screenSelector->setFixedHeight(48);

  auto font = screenSelector->font();
  font.setPointSize(18);
  this->screenSelector->setFont(font);

  this->screenSelector->setPalette(*pal);

  auto* screenSelectorLayout = new QVBoxLayout();
  auto* label = new QLabel("Screen Selector:");
  label->setFont(font);
  screenSelectorLayout->addWidget(label);
  screenSelectorLayout->addWidget(screenSelector);

  auto* screenSelectContainer = new QWidget();
  screenSelectContainer->setLayout(screenSelectorLayout);
  
  // Flags Inputfield
  //    

  auto* mainlayout = new QVBoxLayout(this);
  mainlayout->addWidget(screenSelectContainer);
  mainlayout->addWidget(scrollArea);
  mainlayout->addWidget(extraFlagsInput);
  this->setLayout(mainlayout);
}

void UIWindow::startNewWallpaperEngine() {
  if (wallpaperEngine->state() == QProcess::Running) {
    // Stop WallpaperProcess
    wallpaperEngine->terminate();
    if (!wallpaperEngine->waitForFinished(3000)) {
      wallpaperEngine->kill();
      wallpaperEngine->waitForFinished();
    }
  }
  // create args
  QStringList args;

  for (auto wallpaper : this->selectedWallpapers) {
    args.push_back("--screen-root");
    args.push_back(QString::fromStdString(wallpaper.first));
    if (!extraFlags[wallpaper.first].empty()) {
      for (std::string a : extraFlags[wallpaper.first]) args.push_back(QString::fromStdString(a));
    }
    args.push_back("--bg");
    args.push_back(QString::fromStdString(wallpaper.second));

    for (QString s : args) {
      std::cout << s.toStdString();
    }
    std::cout << "\r\n";
  }

  // start Wallpaper Process
  wallpaperEngine->start(QCoreApplication::applicationFilePath(), args);
}

std::vector<std::string> UIWindow::split(std::string str, char delimiter) {
  // Using str in a string stream
    std::stringstream ss(str);
    std::vector<std::string> res;
    std::string token;
    while (getline(ss, token, delimiter)) {
        res.push_back(token);
    }
    return res;
}

