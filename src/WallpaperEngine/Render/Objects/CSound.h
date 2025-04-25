#pragma once

#include "WallpaperEngine/Core/Objects/CSound.h"

#include "WallpaperEngine/Audio/CAudioStream.h"
#include "WallpaperEngine/Render/CObject.h"

using namespace WallpaperEngine;

namespace WallpaperEngine::Render::Wallpapers {
class CScene;
}

namespace WallpaperEngine::Render::Objects {
class CSound final : public CObject {
  public:
    CSound (Wallpapers::CScene* scene, const Core::Objects::CSound* sound);
    ~CSound () override;

    void render () override;

  protected:
    static const std::string Type;

    void load ();

  private:
    std::vector<std::shared_ptr<const uint8_t[]>> m_soundBuffer;
    std::vector<Audio::CAudioStream*> m_audioStreams;

    const Core::Objects::CSound* m_sound;
};
} // namespace WallpaperEngine::Render::Objects
