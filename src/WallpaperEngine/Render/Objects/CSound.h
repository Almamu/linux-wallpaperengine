#pragma once

#include "WallpaperEngine/Audio/AudioStream.h"
#include "WallpaperEngine/Render/CObject.h"
#include "WallpaperEngine/Scripting/ScriptableObject.h"

using namespace WallpaperEngine;

namespace WallpaperEngine::Render::Wallpapers {
class CScene;
}

namespace WallpaperEngine::Render::Objects {
using namespace WallpaperEngine::Data::Model;

class CSound final : public Scripting::ScriptableObject {
public:
    CSound (Wallpapers::CScene& scene, const Sound& sound);
    ~CSound () override;

    void render () override;

    void play ();
    void pause ();
    void stop ();
    [[nodiscard]] bool isPlaying () const;
    [[nodiscard]] float getVolume () const;
    void setVolume (float volume);

protected:
    void load ();

private:
    std::map<int, Audio::AudioStream*> m_audioStreams = {};

    const Sound& m_sound;
    float m_volume = 1.0f;
};
} // namespace WallpaperEngine::Render::Objects
