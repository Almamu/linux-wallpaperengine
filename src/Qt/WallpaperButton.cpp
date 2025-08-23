#include "WallpaperButton.h"
#include "Qt/UIWindow.h"
#include <fstream>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <nlohmann/json_fwd.hpp>
#include <qevent.h>
#include <qnamespace.h>
#include <qpushbutton.h>
#include <qwidget.h>
#include <string>

WallpaperButton::WallpaperButton(UIWindow* window, std::string& path, QWidget* parent) {
  this->mainWindow = window;
  this->wallpaperPath = path;
  
  std::ifstream file(path + "/project.json");
  nlohmann::json wallpaperJSON = nlohmann::json::parse(file);

  std::string title = wallpaperJSON.at("title");
  if (title.size() > 15) {
    title = title.substr(0, 14) + "..";
  }

  QPixmap pixmap(QString::fromStdString(path + "/preview.jpg"));


  if (pixmap.isNull()) {
    pixmap = QPixmap(PICTURE_SIZE, PICTURE_SIZE);
    pixmap.fill(Qt::black);

    auto* movie = new QMovie(QString::fromStdString(path + "/preview.gif"));
    if (movie->isValid()) {
      movie->jumpToFrame(0);
      pixmap = movie->currentPixmap().scaled(PICTURE_SIZE, PICTURE_SIZE, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    delete movie;
  } else pixmap = pixmap.scaled(PICTURE_SIZE, PICTURE_SIZE, Qt::KeepAspectRatio, Qt::SmoothTransformation);


  setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
  setMinimumSize(100, 100);
  setIcon(QIcon(pixmap));
  setText(QString::fromStdString(title));
  setToolButtonStyle(Qt::ToolButtonStyle::ToolButtonTextUnderIcon);
  setIconSize(size() * 0.8);
  setProperty("path", QString::fromStdString(path));

  setStyleSheet(
    "QToolButton {"
    "background-color: #2B2A33; "
    "color: white; "
    "font-size: 15px;"
    "border: 1px solid #FFFFFF; "
    "border-radius: 5px; "
    "} "
  );
}

void WallpaperButton::resizeEvent(QResizeEvent* event) {
  QToolButton::resizeEvent(event);
  int size = std::min(event->size().width(), event->size().height()) * 0.8f;
  setIconSize(QSize(size, size));

}
