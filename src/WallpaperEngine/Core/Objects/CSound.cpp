#include "CSound.h"
#include "WallpaperEngine/Core/CObject.h"
#include "WallpaperEngine/Logging/CLog.h"

using namespace WallpaperEngine::Core::Objects;

CSound::CSound (
    const Wallpapers::CScene* scene, const CUserSettingBoolean* visible, int id, std::string name,
    const CUserSettingVector3* origin, const CUserSettingVector3* scale, glm::vec3 angles, bool repeat,
    std::vector<std::string> sounds, std::vector<int> dependencies
) :
    CObject (scene, visible, id, name, Type, origin, scale, angles, dependencies),
    m_repeat (repeat),
    m_sounds (sounds) {}

const WallpaperEngine::Core::CObject* CSound::fromJSON (
    const Wallpapers::CScene* scene, const json& data, const CUserSettingBoolean* visible, int id,
    std::string name, const CUserSettingVector3* origin, const CUserSettingVector3* scale, glm::vec3 angles,
    std::vector<int> dependencies
) {
    // TODO: PARSE AUDIO VOLUME
    std::vector<std::string> sounds;
    const auto sound_it = jsonFindRequired (data, "sound", "Sound information not present");

    if (!sound_it->is_array ())
        sLog.exception ("Expected sound list on element ", name);

    for (const auto& cur : (*sound_it))
        sounds.push_back (cur);

    return new CSound (
        scene,
        visible,
        id,
        name,
        origin,
        scale,
        angles,
        jsonFindDefault<std::string> (data, "playbackmode", "") == "loop",
        sounds,
        dependencies
    );
}

const std::vector<std::string>& CSound::getSounds () const {
    return this->m_sounds;
}

bool CSound::isRepeat () const {
    return this->m_repeat;
}

const std::string CSound::Type = "sound";