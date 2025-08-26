#pragma once

#include <QWidget>
#include <QString>
#include <nlohmann/json.hpp>
#include <qboxlayout.h>
#include <qchar.h>
#include <qlabel.h>
#include <qobjectdefs.h>
#include <qwidget.h>
#include <QFormLayout>
#include <variant>
#include <vector>

class UIWindow;

struct Preview {
  QLabel* image;
  QLabel* title;
};

struct Option {
  QWidget* widget;
  QString labelName;
  std::string optionName;
  std::string flag;
  bool flagHasValue;

  std::variant<bool, int, float, QString> defaultValue;
};

class WallpaperSettingsWidget : public QWidget {
  Q_OBJECT

  public:
  explicit WallpaperSettingsWidget(QWidget* parent = nullptr);
  void update(const std::string& currentWallpaperPath);

  private:
  QVBoxLayout* layout = new QVBoxLayout(this);
  QFormLayout* settingsLayout = nullptr;
  Preview preview = {nullptr, nullptr};
  std::vector<Option> currentOptions;
  std::string currentWallpaperPath;
  void updateSettings(const std::string& wallpaperPath, const nlohmann::json& wallpaper);
  void clearSettings();
  void apply();


  QString checkboxStyleSheet = 
  "QCheckBox {"
  " color: white;"
  "}"
  "QCheckBox::indicator {"
  " width: 16px;"
  " height: 16px;"
  " background-color: white;"
  " border: 1px solid gray;"
  "}"
  "QCheckBox::indicator:checked {"
  " background-color: #4CAF50;"
  "}";

  signals:
  void applySettings(const std::string& flags);

  protected:
};
