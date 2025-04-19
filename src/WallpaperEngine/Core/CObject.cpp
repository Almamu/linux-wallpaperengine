#include "CObject.h"

#include <utility>

#include "WallpaperEngine/Core/CProject.h"
#include "WallpaperEngine/Core/Objects/CImage.h"
#include "WallpaperEngine/Core/Objects/CParticle.h"
#include "WallpaperEngine/Core/Objects/CSound.h"
#include "WallpaperEngine/Core/UserSettings/CUserSettingBoolean.h"
#include "WallpaperEngine/Core/Wallpapers/CScene.h"

#include "WallpaperEngine/Assets/CContainer.h"
#include "WallpaperEngine/Logging/CLog.h"

using namespace WallpaperEngine::Core;
using namespace WallpaperEngine::Assets;
using namespace WallpaperEngine::Core::UserSettings;

CObject::CObject (
    const Wallpapers::CScene* scene, const CUserSettingBoolean* visible, int id, std::string name, std::string type,
    const CUserSettingVector3* origin, const CUserSettingVector3* scale, const CUserSettingVector3* angles,
    std::vector<int> dependencies
) :
    m_type (std::move(type)),
    m_visible (visible),
    m_id (id),
    m_name (std::move(name)),
    m_origin (origin),
    m_scale (scale),
    m_angles (angles),
    m_scene (scene),
    m_dependencies (std::move(dependencies)) {}

const CObject* CObject::fromJSON (
    const json& data, const Wallpapers::CScene* scene, const CContainer* container
) {
    const auto id = jsonFindRequired <int> (data, "id", "Objects must have id");
    const auto visible = jsonFindUserConfig<CUserSettingBoolean> (data, scene->getProject(), "visible", true);
    const auto origin = jsonFindUserConfig<CUserSettingVector3> (data, scene->getProject(), "origin", {0, 0, 0});
    const auto scale = jsonFindUserConfig<CUserSettingVector3> (data, scene->getProject(), "scale", {1, 1, 1});
    const auto angles_val = jsonFindUserConfig<CUserSettingVector3> (data, scene->getProject(), "angles", glm::vec3 (0, 0, 0));
    const auto name = jsonFindRequired <std::string> (data, "name", "Objects must have name");
    const auto effects_it = data.find ("effects");
    const auto dependencies_it = data.find ("dependencies");

    const auto image_it = data.find ("image");
    const auto sound_it = data.find ("sound");
    const auto particle_it = data.find ("particle");
    const auto text_it = data.find ("text");
    const auto light_it = data.find ("light");

    std::vector<int> dependencies;
    std::vector<const Objects::CEffect*> effects;

    const CObject* object;

    if (dependencies_it != data.end () && dependencies_it->is_array ())
        for (const auto& cur : *dependencies_it)
            dependencies.push_back (cur);

    if (image_it != data.end () && !image_it->is_null ()) {
        object = Objects::CImage::fromJSON (
            scene, data, container, visible, id, name, origin, scale, angles_val, effects_it, dependencies);
    } else if (sound_it != data.end () && !sound_it->is_null ()) {
        object = Objects::CSound::fromJSON (scene, data, visible, id, name, origin, scale, angles_val, dependencies);
    } else if (particle_it != data.end () && !particle_it->is_null ()) {
        /// TODO: XXXHACK -- TO REMOVE WHEN PARTICLE SUPPORT IS PROPERLY IMPLEMENTED
        try {
            object = Objects::CParticle::fromFile (
                scene, particle_it->get<std::string> (), container, visible, id, name, origin, angles_val, scale, dependencies);
        } catch (std::runtime_error&) {
            return nullptr;
        }
    } else if (text_it != data.end () && !text_it->is_null ()) {
        /// TODO: XXXHACK -- TO REMOVE WHEN TEXT SUPPORT IS IMPLEMENTED
        return nullptr;
    } else if (light_it != data.end () && !light_it->is_null ()) {
        /// TODO: XXXHACK -- TO REMOVE WHEN LIGHT SUPPORT IS IMPLEMENTED
        return nullptr;
    } else {
        sLog.exception ("Unknown object type detected: ", name);
    }

    return object;
}

const glm::vec3& CObject::getOrigin () const {
    return this->m_origin->getVec3 ();
}

const glm::vec3& CObject::getScale () const {
    return this->m_scale->getVec3 ();
}

const glm::vec3& CObject::getAngles () const {
    return this->m_angles->getVec3 ();
}

const std::string& CObject::getName () const {
    return this->m_name;
}

const std::vector<int>& CObject::getDependencies () const {
    return this->m_dependencies;
}

bool CObject::isVisible () const {
    return this->m_visible->getBool ();
}

const Wallpapers::CScene* CObject::getScene () const {
    return this->m_scene;
}

int CObject::getId () const {
    return this->m_id;
}
