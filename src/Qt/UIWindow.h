#pragma once

#include "Qt/SingleInstanceManager.h"
#include <map>
#include <qapplication.h>
#include <qboxlayout.h>
#include <qcombobox.h>
#include <qglobal.h>
#include <qgridlayout.h>
#include <qlabel.h>
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
#include <QMovie>
#include <QCloseEvent>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QAction>
#include <QtConcurrent/QtConcurrent>
#include <string>
#include <vector>
#include <bits/stdc++.h>
#include <QVBoxLayout>
#include <iostream>

class UIWindow : public QWidget {
  Q_OBJECT

  public:
    UIWindow(QWidget* parent, QApplication* qapp, SingleInstanceManager* instanceGuard);
    void setupUIWindow(std::vector<std::string> wallpaperPaths);

  private:
    // Components
    QApplication* qapp;
    SingleInstanceManager* instanceGuard;
    QComboBox* screenSelector;
    QLineEdit* extraFlagsInput;
    QGridLayout* buttonLayout;
    QLabel* previewImageLabel;
    QLabel* previewTitleLabel; 
    
    // Important Fields
    std::map<std::string, std::string> selectedWallpapers;
    std::map<std::string, std::vector<std::string>> extraFlags;
    QProcess* wallpaperEngine;

    void startNewWallpaperEngine();
    void updateSelectedButton();
    void updateConfigLayout();
    static std::vector<std::string> split(const std::string &str, char r);

  protected:
    void showEvent(QShowEvent* event) override;
    void closeEvent(QCloseEvent* event) override;
};
