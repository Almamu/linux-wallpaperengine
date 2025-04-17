#pragma once

#include <map>
#include <qapplication.h>
#include <qcombobox.h>
#include <qgridlayout.h>
#include <qlineedit.h>
#include <qobjectdefs.h>
#include <qprocess.h>
#include <qwidget.h>
#include <QScrollArea>
#include <QProcess>
#include <QObject>
#include <QPushButton>
#include <QString>
#include <QScreen>
#include <QComboBox>
#include <QLineEdit>
#include <QLabel>
#include <string>
#include <vector>
#include <bits/stdc++.h>
#include <iostream>

class UIWindow : public QWidget {
  Q_OBJECT

  public:
    UIWindow(QWidget* parent, QApplication* qapp);
    void setupUIWindow(std::vector<std::string> wallpaperPaths);

  private:
    QApplication* qapp;
    QComboBox* screenSelector;
    QLineEdit* extraFlagsInput;
    std::map<std::string, std::string> selectedWallpapers;
    std::map<std::string, std::vector<std::string>> extraFlags;
    QProcess* wallpaperEngine;

    void startNewWallpaperEngine();
    static std::vector<std::string> split(std::string str, char r);
};
