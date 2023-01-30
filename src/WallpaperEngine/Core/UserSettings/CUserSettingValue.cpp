#include "CUserSettingValue.h"

#include <utility>

using namespace WallpaperEngine::Core::UserSettings;

CUserSettingValue::CUserSettingValue (std::string type) :
    m_type (std::move(type))
{
}