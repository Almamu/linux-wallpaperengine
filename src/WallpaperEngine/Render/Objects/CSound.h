#pragma once

#include "WallpaperEngine/Audio/CAudioStream.h"
#include "WallpaperEngine/Render/CObject.h"

using namespace WallpaperEngine;

namespace WallpaperEngine::Render::Wallpapers {
class CScene;
}

namespace WallpaperEngine::Render::Objects {
using namespace WallpaperEngine::Data::Model;

class CSound final : public CObject {
  public:
    CSound (Wallpapers::CScene& scene, const Sound& sound);
    ~CSound () override;

    void render () override;

  protected:
    void load ();

  private:
    std::vector<Audio::CAudioStream*> m_audioStreams = {};

    const Sound& m_sound;
};
} // namespace WallpaperEngine::Render::Objects
