#include "EffectSwapCommand.h"

#include "WallpaperEngine/Data/Model/DynamicValue.h"
#include "WallpaperEngine/Data/Model/UserSetting.h"

namespace WallpaperEngine::Render::Objects::Effects {
void EffectSwapCommand::execute () const {
    if (visible->value->getBool ()) bindings->swap (source, target);
}
}
