#pragma once

#include <QWidget>
#include <qboxlayout.h>
#include <qlabel.h>
#include <qwidget.h>

struct Preview {
  QLabel* image;
  QLabel* title;
};

class WallpaperSettingsWidget : public QWidget {
  public:
  explicit WallpaperSettingsWidget(QWidget* parent = nullptr);
  void update(const std::string& currentWallpaperPath);

  private:
  QVBoxLayout* layout = new QVBoxLayout(this);
  Preview preview = {nullptr, nullptr};

  protected:
};
