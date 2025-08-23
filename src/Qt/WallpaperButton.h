#pragma once

#include <qcoreevent.h>
#include <qevent.h>
#include <qobjectdefs.h>
#include <qtoolbutton.h>
#include <qwidget.h>
#include <string>
#include "UIWindow.h"

#define PICTURE_SIZE 128

class WallpaperButton : public QToolButton {
  Q_OBJECT

  public:
    explicit WallpaperButton(UIWindow* mainWindow, std::string& wallpaperPath, QWidget* parent = nullptr);

  protected:
    // void mousePressEvent(QMouseEvent* event) override;
    // void enterEvent(QEvent* event) override;
    // void leaveEvent(QEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

  private: 
    UIWindow* mainWindow;
    std::string wallpaperPath;
};
