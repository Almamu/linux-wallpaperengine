#pragma once

#include "WallpaperEngine/Core/Objects/CSound.h"

#include "WallpaperEngine/Audio/CAudioStream.h"
#include "WallpaperEngine/Render/CObject.h"

using namespace WallpaperEngine;

namespace WallpaperEngine::Render::Objects {
class CSound final : public CObject {
  public:
    CSound (CScene* scene, Core::Objects::CSound* sound);
    ~CSound ();

    void render () override;

  protected:
    static const std::string Type;

    void load ();

  private:
    std::vector<const void*> m_soundBuffer;
    std::vector<Audio::CAudioStream*> m_audioStreams;

    Core::Objects::CSound* m_sound;
};
} // namespace WallpaperEngine::Render::Objects
