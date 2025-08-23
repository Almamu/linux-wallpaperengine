#include "WallpaperSettingsWidget.h"
#include <fstream>
#include <qmovie.h>
#include <qwidget.h>
#include <string>
#include <nlohmann/json.hpp>

WallpaperSettingsWidget::WallpaperSettingsWidget(QWidget* parent)
  : QWidget(parent) {

  preview.title = new QLabel("...", this);
  preview.title->setAlignment(Qt::AlignTop);
  preview.image = new QLabel(this);
  preview.image->setFixedSize(256, 256);
  preview.image->setAlignment(Qt::AlignCenter);

  layout->addWidget(preview.image);
  layout->addWidget(preview.title);

  setLayout(layout);
}

void WallpaperSettingsWidget::update(const std::string& selected) {
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
  preview.image->setPixmap(pixmap);
  // edit Title
  preview.title->setText(QString::fromStdString(title));
}
