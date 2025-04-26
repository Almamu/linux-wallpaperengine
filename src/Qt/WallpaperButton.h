#pragma once

#include <qcoreevent.h>
#include <qobjectdefs.h>
#include <qpushbutton.h>
#include <qwidget.h>
#include <string>
#include "UIWindow.h"

class WallpaperButton : public QPushButton {
  Q_OBJECT

  public:
    explicit WallpaperButton(UIWindow* mainWindow, std::string& wallpaperPath, QWidget* parent = nullptr);

  protected:
    void mousePressEvent(QMouseEvent* event) override;
    void enterEvent(QEvent* event) override;
    void leaveEvent(QEvent* event) override;

  private: 
    UIWindow* mainWindow;
    std::string wallpaperPath;
};
