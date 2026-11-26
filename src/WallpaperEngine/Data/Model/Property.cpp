#include "Property.h"
#include "UserSetting.h"
#include "WallpaperEngine/Data/Builders/ColorBuilder.h"

using namespace WallpaperEngine::Data::Model;

void PropertyColor::update (const std::string& value, UpdateSource source) {
    this->update (Builders::ColorBuilder::parse (value), source);
}
