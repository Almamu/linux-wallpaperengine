#include "common.h"
#include "WallpaperEngine/Core/CObject.h"
#include "CSound.h"

using namespace WallpaperEngine::Core::Objects;

CSound::CSound (
        CScene* scene,
        CUserSettingBoolean* visible,
        uint32_t id,
        std::string name,
        CUserSettingVector3* origin,
        CUserSettingVector3* scale,
        const glm::vec3& angles) :
        CObject (scene, visible, id, std::move(name), Type, origin, scale, angles)
{
}

WallpaperEngine::Core::CObject* CSound::fromJSON (
        CScene* scene,
        json data,
        CUserSettingBoolean* visible,
        uint32_t id,
        std::string name,
        CUserSettingVector3* origin,
        CUserSettingVector3* scale,
        const glm::vec3& angles)
{
    // TODO: PARSE AUDIO VOLUME
    auto sound_it = jsonFindRequired (data, "sound", "Sound information not present");

    if ((*sound_it).is_array () == false)
        sLog.exception ("Expected sound list on element ", name);

    CSound* sound = new CSound (
        scene,
        visible,
        id,
        name,
        origin,
        scale,
        angles
    );

    for (const auto& cur : (*sound_it))
        sound->insertSound (cur);

    return sound;
}

void CSound::insertSound (std::string filename)
{
    this->m_sounds.push_back (filename);
}

const std::vector<std::string>& CSound::getSounds () const
{
    return this->m_sounds;
}

const std::string CSound::Type = "sound";