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
    static ObjectUniquePtr parse (const JSON& it, const Project& project);

  private:
    static std::vector<int> parseDependencies (const JSON& it);
    static SoundUniquePtr parseSound (const JSON& it, ObjectData base);
    static ImageUniquePtr parseImage (const JSON& it, const Project& project, ObjectData base, const std::string& image);
    static ParticleUniquePtr parseParticle (const JSON& it, const Project& project, ObjectData base);
    static std::vector <ImageEffectUniquePtr> parseEffects (const JSON& it, const Project& project);
    static ImageEffectUniquePtr parseEffect (const JSON& it, const Project& project);
    static std::vector <ImageEffectPassOverrideUniquePtr> parseEffectPassOverrides (const JSON& it, const Project& project);
    static ImageEffectPassOverrideUniquePtr parseEffectPass (const JSON& it, const Project& project);
    static TextureMap parseTextureMap (const JSON& it);
    static ComboMap parseComboMap (const JSON& it);
    static std::vector <ImageAnimationLayerUniquePtr> parseAnimationLayers (const JSON& it, const Project& project);
    static ImageAnimationLayerUniquePtr parseAnimationLayer (const JSON& it, const Project& project);

    // Particle parsing helpers
    static ParticleEmitter parseParticleEmitter (const JSON& it);
    static ParticleInitializerUniquePtr parseParticleInitializer (const JSON& it, const Properties& properties);
    static ParticleOperatorUniquePtr parseParticleOperator (const JSON& it, const Properties& properties);
    static ParticleRenderer parseParticleRenderer (const JSON& it);
    static ParticleControlPoint parseParticleControlPoint (const JSON& it);
    static ParticleChild parseParticleChild (const JSON& it, const Project& project);
    static ParticleInstanceOverride parseParticleInstanceOverride (const JSON& it, const Properties& properties);
};
} // namespace WallpaperEngine::Data::Parsers
