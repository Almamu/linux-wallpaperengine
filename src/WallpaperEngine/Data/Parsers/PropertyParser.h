#pragma once

#include "WallpaperEngine/Data/JSON.h"

namespace WallpaperEngine::Data::Parsers {
using JSON = WallpaperEngine::Data::JSON::JSON;
using namespace WallpaperEngine::Data::Model;

class PropertyParser {
  public:
    static PropertySharedPtr parse (const JSON& it, std::string name);

  private:
    static PropertySharedPtr parseCombo (const JSON& it, std::string name);
    static PropertySharedPtr parseColor (const JSON& it, std::string name);
    static PropertySharedPtr parseBoolean (const JSON& it, std::string name);
    static PropertySharedPtr parseSlider (const JSON& it, std::string name);
    static PropertySharedPtr parseText (const JSON& it, std::string name);
};
}