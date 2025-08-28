#include "UIWindow.h"
#include "Qt/SingleInstanceManager.h"
#include <QListView>
#include "Qt/WallpaperButton.h"
#include <QtConcurrent/qtconcurrentrun.h>
#include <X11/X.h>
#include <cstddef>
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
#include "Qt/WallpaperSettingsWidget.h"
#include "WallpaperButton.h"

#define PICTURE_SIZE 128

UIWindow::UIWindow(QWidget* parent, QApplication* qapp, SingleInstanceManager* ig) {
  this->qapp = qapp; 
  this->screenSelector = new QComboBox(this);
  this->wallpaperEngine = new QProcess(this);
  this->instanceGuard = ig;
  this->buttonLayout = new QGridLayout(this);

  this->wallpaperSettingsWidget = nullptr;
}

void UIWindow::setupUIWindow(std::vector<std::string> wallpaperPaths) {
  this->setWindowTitle("Linux-WallpaperEngine");

  this->setStyleSheet(R"(
    QWidget {
      background-color: #2B2A33;
      color: white;
    }
    )");

  this->setAttribute(Qt::WA_StyledBackground, true);
  
  auto* scrollArea = new QScrollArea(this); 
  scrollArea->setWidgetResizable(true);

  auto* container = new QWidget();

  int cols = 6; 

  for (size_t i = 0; i < wallpaperPaths.size(); i++) {
    auto* button = new WallpaperButton(this, wallpaperPaths[i]);
    
    QAbstractButton::connect(button, &QPushButton::clicked, [button, this]() {
      QString clickedPath = button->property("path").toString();
      button->setEnabled(false);

      this->selectedWallpapers[this->screenSelector->currentText().toStdString()] = clickedPath.toStdString();
      
      // startNewWallpaperEngine();
      // Doesn't need to start a new WallpaperEngine here since update wallpaperSettings does emit applySettings()
      updateSelectedButton();
      this->wallpaperSettingsWidget->update(this->selectedWallpapers[this->screenSelector->currentText().toStdString()]);
    });
    int row = i / cols;
    int col = i % cols;
    buttonLayout->addWidget(button, row, col);
  }

  QObject::connect(wallpaperEngine, &QProcess::started, this, [this]() {
    updateSelectedButton();
  });

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

  this->screenSelector->setView(new QListView());
  this->screenSelector->view()->setStyleSheet(
    "QListView { background-color:#2B2A33; selection-background-color:#4488FF; color:white; }"
    "QListView::item:hover { background-color:#4488FF; }"
  );

  this->screenSelector->setStyleSheet(
    "font-size: 24px;"
  );


  QObject::connect(this->screenSelector, QOverload<int>::of(&QComboBox::currentIndexChanged), [this](int index) {
    updateSelectedButton();
    this->wallpaperSettingsWidget->update(this->selectedWallpapers[this->screenSelector->currentText().toStdString()]);
  });

  auto* screenSelectorLayout = new QVBoxLayout(this);
  auto* label = new QLabel("Screen Selector:");
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
  this->wallpaperSettingsWidget = new WallpaperSettingsWidget(splitWidget);

  connect(this->wallpaperSettingsWidget, &WallpaperSettingsWidget::applySettings, this, [this](const std::string& flags) {
    this->extraFlags[this->screenSelector->currentText().toStdString()] = split(flags, ' ');
    startNewWallpaperEngine();
  });
  
  splitLayout->addWidget(leftWidget, 2);
  splitLayout->addWidget(this->wallpaperSettingsWidget, 1);

  mainlayout->addWidget(splitWidget);
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
  // delete this->wallpaperEngine;
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

void UIWindow::updateSelectedButton() {
  for (int i = 0; i < this->buttonLayout->rowCount(); i++) {
    for (int j = 0; j < this->buttonLayout->columnCount(); j++) {
      auto* item = this->buttonLayout->itemAtPosition(i, j);
      if (!item) continue;
      
      auto* widget = item->widget();
      if (!widget) continue;

      auto* button = dynamic_cast<WallpaperButton*>(widget);
      if (!button) continue;

      button->setEnabled(true);

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

