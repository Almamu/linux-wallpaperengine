#include "WallpaperSettingsWidget.h"
#include "WallpaperEngine/Logging/CLog.h"
#include <fstream>
#include <nlohmann/json_fwd.hpp>
#include <qchar.h>
#include <qcheckbox.h>
#include <qcombobox.h>
#include <qformlayout.h>
#include <qgroupbox.h>
#include <qlineedit.h>
#include <qmovie.h>
#include <qnamespace.h>
#include <qobjectdefs.h>
#include <qpushbutton.h>
#include <qslider.h>
#include <qwidget.h>
#include <string>
#include <nlohmann/json.hpp>
#include <QCheckBox>
#include <QLineEdit>
#include <QComboBox>
#include <QListView>
#include <variant>

WallpaperSettingsWidget::WallpaperSettingsWidget(QWidget* parent)
  : QWidget(parent) {
  layout->setSpacing(5);

  preview.title = new QLabel("...", this);
  preview.title->setAlignment(Qt::AlignTop);
  preview.title->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
  preview.image = new QLabel(this);
  preview.image->setFixedSize(256, 256);
  preview.image->setAlignment(Qt::AlignCenter);

  auto* settingsBox = new QGroupBox("Settings:", this);
  settingsBox->setStyleSheet(
    "font-size: 16px; "
    "color: white; "
    "background: transparent; "
  );
  this->settingsLayout = new QFormLayout(settingsBox);
  settingsBox->setAlignment(Qt::AlignTop);
  settingsBox->setLayout(this->settingsLayout);

  auto* applyButton = new QPushButton("Apply", this);

  connect(applyButton, &QPushButton::clicked, this, [applyButton, this]() {
    this->apply(); 
  });

  layout->addWidget(preview.image);
  layout->addWidget(preview.title);
  layout->addWidget(settingsBox);
  layout->addWidget(applyButton);

  setLayout(layout);
}

void WallpaperSettingsWidget::update(const std::string& selected) {
  if (selected.empty()) return;

  this->currentWallpaperPath = selected;

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

  file.close();

  updateSettings(selected, wallpaperJSON);
  apply();
}

QCheckBox* createStyledCheckBox(const QString& style) {
  auto* cb = new QCheckBox();
  cb->setStyleSheet(style);
  return cb;
}

void WallpaperSettingsWidget::updateSettings(const std::string& wallpaperPath, const nlohmann::json& wallpaper) {
  clearSettings();
   
  // TODO: add individual settings depending on wallpaper 
  /*
  auto general = wallpaper.find("general");
  if (general != wallpaper.end()) {
    if (general->find("properties") != general->end()) {
      const auto properties = general->at("properties");
      if (properties != nullptr) {
        sLog.out(properties);
      }
    }
  }
  */
  auto * scalingBox = new QComboBox();
  scalingBox->addItems({"stretch", "fit", "fill", "default"});
  scalingBox->setView(new QListView());
  scalingBox->view()->setStyleSheet(
    "QListView { background-color:#2B2A33; selection-background-color:#4488FF; color:white; }"
    "QListView::item:hover { background-color:#4488FF; }"
  );

  auto* clampingBox = new QComboBox();
  // clampingBox->addItems({"clamp", "border", "repeat"});

  this->currentOptions.push_back({createStyledCheckBox(this->checkboxStyleSheet), "Mute Audio:", "mute_audio", "--silent", false, false});
  this->currentOptions.push_back({new QSlider(Qt::Horizontal), "Volume:", "volume", "--volume", true, 50});
  this->currentOptions.push_back({createStyledCheckBox(this->checkboxStyleSheet), "Disable Automute:", "disable_automute", "--noautomute", false, false});
  this->currentOptions.push_back({createStyledCheckBox(this->checkboxStyleSheet), "Disable Audio Reaction:", "disable_audio_reaction", "--no-audio-processing", false, false});
  this->currentOptions.push_back({new QLineEdit(), "FPS:", "fps", "--fps", true, 30});
  this->currentOptions.push_back({scalingBox, "Scaling:", "scaling", "--scaling", true, "default"});
  this->currentOptions.push_back({clampingBox, "Clamping:", "clamping", "--clamping", true, ""});
  this->currentOptions.push_back({createStyledCheckBox(this->checkboxStyleSheet), "Disable Mouse:", "diable_mouse", "--disable-mouse", false, false});
  this->currentOptions.push_back({createStyledCheckBox(this->checkboxStyleSheet), "Disable Parallax", "disable_parallax", "--disable-parallax", false, true});
  this->currentOptions.push_back({createStyledCheckBox(this->checkboxStyleSheet), "Disable Fullscreen Pause:", "disable_fullscreen_pause", "--no-fullscreen-pause", true, false});

  for (const Option& opt : this->currentOptions) {
    this->settingsLayout->addRow(opt.labelName, opt.widget);

    if (auto* cb = dynamic_cast<QCheckBox*>(opt.widget)) {
      if (std::holds_alternative<bool>(opt.defaultValue))  {
        cb->setChecked(std::get<bool>(opt.defaultValue));
      }
      continue;
    }
    if (auto* slider = dynamic_cast<QSlider*>(opt.widget)) {
      if (std::holds_alternative<int>(opt.defaultValue)) {
        slider->setValue(std::get<int>(opt.defaultValue));
      }
      continue;
    }
    if (auto* lineEdit = dynamic_cast<QLineEdit*>(opt.widget)) {
      if (std::holds_alternative<int>(opt.defaultValue)) {
        lineEdit->setText(QString::fromStdString(std::to_string(std::get<int>(opt.defaultValue))));
      }
      continue;
    }
    if (auto* comboBox = dynamic_cast<QComboBox*>(opt.widget)) {
      if (std::holds_alternative<QString>(opt.defaultValue)) {
        comboBox->setCurrentText(std::get<QString>(opt.defaultValue));
      }
      continue;
    }
  }
}
  
void WallpaperSettingsWidget::clearSettings() {
  QLayoutItem* item;
  while ((this->settingsLayout->count() != 0) && (item = this->settingsLayout->takeAt(0)) != nullptr) {
    if (item->widget()) {
      delete item->widget();
    }
    if (item->layout()) {
      delete item->layout();
    }
    delete item;
  }
  
  this->currentOptions.clear();
}

void WallpaperSettingsWidget::apply() {
  if (currentWallpaperPath.empty()) return;
  std::string flags;

  for (const Option& opt : this->currentOptions) {
    if (auto* cb = dynamic_cast<QCheckBox*>(opt.widget)) {
      if (cb->isChecked()) {
        flags.append(opt.flag + " ");
        if (opt.flagHasValue) flags.append("true ");
      }
      continue;
    }
    if (auto* slider = dynamic_cast<QSlider*>(opt.widget)) {
      int value = slider->value();
      flags.append(opt.flag + " ");
      if (opt.flagHasValue) flags.append(std::to_string(value) + " ");
      continue;
    }
    if (auto* lineEdit = dynamic_cast<QLineEdit*>(opt.widget)) {
      std::string value = lineEdit->text().toStdString();
      flags.append(opt.flag + " ");
      if (opt.flagHasValue) flags.append(value + " ");
      continue;
    }
    if (auto* comboBox = dynamic_cast<QComboBox*>(opt.widget)) {
      QString value = comboBox->currentText();
      flags.append(opt.flag + " ");
      if (opt.flagHasValue) flags.append(value.toStdString() + " ");
      continue;
    }
  }
  emit applySettings(flags);
}
