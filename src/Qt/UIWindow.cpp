#include "UIWindow.h"
#include "Qt/SingleInstanceManager.h"
#include "Qt/WallpaperButton.h"
#include <QtConcurrent/qtconcurrentrun.h>
#include <X11/X.h>
#include <algorithm>
#include <cstddef>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>
#include <qapplication.h>
#include <qboxlayout.h>
#include <QHBoxLayout>
#include <qcombobox.h>
#include <qcursor.h>
#include <qdebug.h>
#include <qevent.h>
#include <qglobal.h>
#include <qgroupbox.h>
#include <qlabel.h>
#include <qlayout.h>
#include <qlineedit.h>
#include <qlocalsocket.h>
#include <qmenu.h>
#include <qnamespace.h>
#include <qprocess.h>
#include <qpushbutton.h>
#include <qsystemtrayicon.h>
#include <qwidget.h>
#include <qwindowdefs.h>
#include <QByteArray>
#include <string>
#include <strings.h>
#include <vector>
#include <QToolButton>
#include <QGroupBox>
#include "WallpaperButton.h"

#define PICTURE_SIZE 128

UIWindow::UIWindow(QWidget* parent, QApplication* qapp, SingleInstanceManager* ig) {
  this->qapp = qapp; 
  this->screenSelector = new QComboBox(this);
  this->extraFlagsInput = new QLineEdit(this);
  this->wallpaperEngine = new QProcess(this);
  this->instanceGuard = ig;
  this->buttonLayout = new QGridLayout(this);
}

void UIWindow::setupUIWindow(std::vector<std::string> wallpaperPaths) {
  this->setWindowTitle("Wallpapers :3");
  
  // palette
  auto* pal = new QPalette();
  pal->setColor(QPalette::Window, QColor(0x2B, 0x2A, 0x33, 0xFF));
  this->setAutoFillBackground(true);
  this->setPalette(*pal);
   
  auto* scrollArea = new QScrollArea(this); 
  scrollArea->setWidgetResizable(true);
  scrollArea->setPalette(*pal);

  auto* container = new QWidget();

  int cols = 6; 

  for (size_t i = 0; i < wallpaperPaths.size(); i++) {
    auto* button = new WallpaperButton(this, wallpaperPaths[i]);
    
    QAbstractButton::connect(button, &QPushButton::clicked, [button, this]() {
      QString clickedPath = button->property("path").toString();
      button->setEnabled(false);

      this->selectedWallpapers[this->screenSelector->currentText().toStdString()] = clickedPath.toStdString();
      this->extraFlags[this->screenSelector->currentText().toStdString()] = split(this->extraFlagsInput->text().toStdString(), ' ');

      startNewWallpaperEngine();

      QObject::connect(wallpaperEngine, &QProcess::started, button, [=]() {
        button->setEnabled(true);
        updateSelectedButton();
        updateConfigLayout();
      });
    });

    int row = i / cols;
    int col = i % cols;
    buttonLayout->addWidget(button, row, col);
  }

  QObject::connect(this->qapp, &QCoreApplication::aboutToQuit, this, [this]() {
    wallpaperEngine->terminate(); 
    wallpaperEngine->waitForFinished(3000);

    instanceGuard->cleanUpServer();
  });

  QObject::connect(instanceGuard, &SingleInstanceManager::messageReceived, [this](const QByteArray& msg) {
    if (msg == "show") {
      if (this->isHidden()) show();
    }
  });

  container->setLayout(buttonLayout);
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

  QObject::connect(this->screenSelector, QOverload<int>::of(&QComboBox::currentIndexChanged), [this](int index) {
    updateSelectedButton();
    updateConfigLayout();
  });

  auto* screenSelectorLayout = new QVBoxLayout();
  auto* label = new QLabel("Screen Selector:");
  label->setFont(font);
  screenSelectorLayout->addWidget(label);
  screenSelectorLayout->addWidget(screenSelector);

  auto* screenSelectContainer = new QWidget();
  screenSelectContainer->setLayout(screenSelectorLayout);
  
  // Main Layout
  auto* mainlayout = new QVBoxLayout(this);
  mainlayout->addWidget(screenSelectContainer);

  auto* splitWidget = new QGroupBox("Wallpaper Selection", this);
  splitWidget->setStyleSheet(
    "font-size: 26px; "
    "color: white; "
    "background: transparent; "
  );
  auto* splitLayout = new QHBoxLayout(splitWidget);
  splitWidget->setLayout(splitLayout);

  // left side 
  auto* leftWidget = new QWidget(splitWidget);
  auto* leftLayout = new QVBoxLayout(leftWidget);
  leftLayout->addWidget(scrollArea);
  leftWidget->setLayout(leftLayout);

  // right side
  auto* rightWidget = new QWidget(splitWidget);
  auto* rightLayout = new QVBoxLayout(rightWidget);
  this->previewTitleLabel = new QLabel("...", rightWidget);
  this->previewTitleLabel->setAlignment(Qt::AlignTop);
  this->previewImageLabel = new QLabel(rightWidget);
  this->previewImageLabel->setFixedSize(256, 256);
  this->previewImageLabel->setAlignment(Qt::AlignCenter);
  rightLayout->addWidget(previewImageLabel);
  rightLayout->addWidget(previewTitleLabel);
  rightWidget->setLayout(rightLayout);
  
  splitLayout->addWidget(leftWidget, 2);
  splitLayout->addWidget(rightWidget, 1);

  mainlayout->addWidget(splitWidget);
  mainlayout->addWidget(extraFlagsInput);
  this->setLayout(mainlayout);
  
  // update Buttons
  updateSelectedButton();
  
  // SYSTEM TRAY
  auto* trayIcon = new QSystemTrayIcon(QIcon(":/assets/wallpaper-icon.png"));

  auto* trayMenu = new QMenu();
  
  trayMenu->addAction("Reload wallpapers", [this] { startNewWallpaperEngine(); });
  trayMenu->addAction("Quit", [this] { qApp->quit(); });

  trayIcon->setContextMenu(trayMenu);
  trayIcon->setToolTip("Linux-Wallpaperengine");
  trayIcon->show();

  connect(trayIcon, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
    if (reason == QSystemTrayIcon::Trigger) {
      if (isVisible()) {
        hide();
      } else {
        show();
      } 
    }
  });
}

void UIWindow::showEvent(QShowEvent* event) {
  QtConcurrent::run([this]() {
    
  });
}

void UIWindow::closeEvent(QCloseEvent* event) {
  this->hide();
  event->ignore();
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

  for (const auto &wallpaper : this->selectedWallpapers) {
    if (wallpaper.first == "" || wallpaper.second == "") continue;
    args.push_back("--screen-root");
    args.push_back(QString::fromStdString(wallpaper.first));
    if (!extraFlags[wallpaper.first].empty()) {
      for (const std::string &a : extraFlags[wallpaper.first]) args.push_back(QString::fromStdString(a));
    }
    args.push_back("--bg");
    args.push_back(QString::fromStdString(wallpaper.second));
  }

  // start Wallpaper Process
  wallpaperEngine->start(QCoreApplication::applicationFilePath(), args);
}

void UIWindow::updateConfigLayout() {
  std::string selected = this->selectedWallpapers[this->screenSelector->currentText().toStdString()];
  if (selected.empty()) return;

  std::ifstream file(selected + "/project.json");
  nlohmann::json wallpaperJSON = nlohmann::json::parse(file);

  if (wallpaperJSON.empty()) {
    return;
  }

  std::string title = wallpaperJSON.at("title");
  if (title.size() > 25) {
    title = title.substr(0, 24) + "..";
  }

  QPixmap pixmap(QString::fromStdString(selected + "/preview.jpg"));


  if (pixmap.isNull()) {
    pixmap = QPixmap(256, 256);
    pixmap.fill(Qt::black);

    auto* movie = new QMovie(QString::fromStdString(selected + "/preview.gif"));
    if (movie->isValid()) {
      movie->jumpToFrame(0);
      pixmap = movie->currentPixmap().scaled(256, 256, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    delete movie;
  } else pixmap = pixmap.scaled(256, 256, Qt::KeepAspectRatio, Qt::SmoothTransformation);

  // edit previewLabel  
  this->previewImageLabel->setPixmap(pixmap);
  // edit Title
  this->previewTitleLabel->setText(QString::fromStdString(title));
}

void UIWindow::updateSelectedButton() {
  for (int i = 0; i < this->buttonLayout->rowCount(); i++) {
    for (int j = 0; j < this->buttonLayout->columnCount(); j++) {
      auto* item = this->buttonLayout->itemAtPosition(i, j);
      if (!item) continue;

      auto* widget = item->widget();
      if (!widget) continue;

      auto* button = dynamic_cast<WallpaperButton*>(widget);
      if (!button) continue;

      std::string selected = this->selectedWallpapers[this->screenSelector->currentText().toStdString()];
      QString currentStyle = button->styleSheet();
      QString newStyle = currentStyle;
      if (button->property("path").toString().toStdString() == selected) {
        newStyle = newStyle.replace(QRegularExpression("background-color:[^;]+;"), "background-color: #4488ff; ");
        button->setStyleSheet(newStyle);
      } else {
        newStyle = newStyle.replace(QRegularExpression("background-color:[^;]+;"), "background-color: #4A4D51; ");
        button->setStyleSheet(newStyle);
      }
    }
  }
}

std::vector<std::string> UIWindow::split(const std::string &str, char delimiter) {
  // Using str in a string stream
    std::stringstream ss(str);
    std::vector<std::string> res;
    std::string token;
    while (getline(ss, token, delimiter)) {
        res.push_back(token);
    }
    return res;
}

