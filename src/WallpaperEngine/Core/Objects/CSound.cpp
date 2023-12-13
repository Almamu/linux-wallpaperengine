#include "CSound.h"
#include "WallpaperEngine/Core/CObject.h"
#include "common.h"

using namespace WallpaperEngine::Core::Objects;

CSound::CSound (CScene* scene, CUserSettingBoolean* visible, uint32_t id, std::string name, CUserSettingVector3* origin,
                CUserSettingVector3* scale, const glm::vec3& angles, bool repeat) :
    CObject (scene, visible, id, std::move (name), Type, origin, scale, angles),
    m_repeat (repeat) {}

WallpaperEngine::Core::CObject* CSound::fromJSON (CScene* scene, json data, CUserSettingBoolean* visible, uint32_t id,
                                                  const std::string& name, CUserSettingVector3* origin,
                                                  CUserSettingVector3* scale, const glm::vec3& angles) {
    bool repeat = false;
    // TODO: PARSE AUDIO VOLUME
    const auto sound_it = jsonFindRequired (data, "sound", "Sound information not present");
    const auto playbackmode = jsonFindDefault<std::string> (data, "playbackmode", "");

    if (playbackmode == "loop")
        repeat = true;

    if (!sound_it->is_array ())
        sLog.exception ("Expected sound list on element ", name);

    auto* sound = new CSound (scene, visible, id, name, origin, scale, angles, repeat);

    for (const auto& cur : (*sound_it))
        sound->insertSound (cur);

    return sound;
}

void CSound::insertSound (const std::string& filename) {
    this->m_sounds.push_back (filename);
}

const std::vector<std::string>& CSound::getSounds () const {
    return this->m_sounds;
}

bool CSound::isRepeat () const {
    return this->m_repeat;
}

const std::string CSound::Type = "sound";