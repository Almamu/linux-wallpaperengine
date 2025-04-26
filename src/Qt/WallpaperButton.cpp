#include "WallpaperButton.h"
#include "Qt/UIWindow.h"
#include <qwidget.h>
#include <string>

WallpaperButton::WallpaperButton(UIWindow* window, std::string& path, QWidget* parent) {
  this->mainWindow = window;
  this->wallpaperPath = path;
}
