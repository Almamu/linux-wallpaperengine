#pragma once

#include <vector>

#include "WallpaperEngine/Data/JSON.h"
#include "WallpaperEngine/Data/Model/Object.h"

namespace WallpaperEngine::Data::Model {
struct ObjectData;
}

namespace WallpaperEngine::Data::Parsers {
using JSON = WallpaperEngine::Data::JSON::JSON;
using namespace WallpaperEngine::Data::Model;

class ObjectParser {
  public:
    static ObjectUniquePtr parse (const JSON& it, const ProjectWeakPtr& project);

  private:
    static std::vector<int> parseDependencies (const JSON& it);
    static SoundUniquePtr parseSound (const JSON& it, const ProjectWeakPtr& project, ObjectData base);
    static ImageUniquePtr parseImage (
        const JSON& it, const ProjectWeakPtr& project, ObjectData base, const std::string& image);
    static std::vector <ImageEffectUniquePtr> parseEffects (const JSON& it, const ProjectWeakPtr& project);
    static ImageEffectUniquePtr parseEffect (const JSON& it, const ProjectWeakPtr& project);
    static std::vector <ImageEffectPassOverrideUniquePtr> parseEffectPassOverrides (const JSON& it, const ProjectWeakPtr& project);
    static ImageEffectPassOverrideUniquePtr parseEffectPass (const JSON& it, const ProjectWeakPtr& project);
    static TextureMap parseTextureMap (const JSON& it);
    static ComboMap parseComboMap (const JSON& it);
};
} // namespace WallpaperEngine::Data::Parsers
