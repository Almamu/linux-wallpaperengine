#include "CParticle.h"

#include <utility>

using namespace WallpaperEngine::Core::Objects;

const CParticle* CParticle::fromFile (
    const Wallpapers::CScene* scene, const std::string& filename, const CContainer* container,
    const CUserSettingBoolean* visible, int id, const std::string& name, const CUserSettingVector3* origin,
    const CUserSettingVector3* angles, const CUserSettingVector3* scale, std::vector<int> dependencies
) {
    json data = json::parse (container->readFileAsString (filename));
    const auto controlpoint_it = data.find ("controlpoint");
    const auto emitter_it = jsonFindRequired (data, "emitter", "Particles must have emitters");
    const auto initializer_it = jsonFindRequired (data, "initializer", "Particles must have initializers");

    std::vector<const Particles::CControlPoint*> controlpoints;
    std::vector<const Particles::CEmitter*> emitters;
    std::vector<const Particles::CInitializer*> initializers;

    if (controlpoint_it != data.end ())
        for (const auto& cur : (*controlpoint_it))
            controlpoints.push_back (Particles::CControlPoint::fromJSON (cur));

    for (const auto& cur : (*emitter_it))
        emitters.push_back (Particles::CEmitter::fromJSON (cur));
    for (const auto& cur : (*initializer_it))
        initializers.push_back (Particles::CInitializer::fromJSON (cur));

    return new CParticle (
        scene,
        jsonFindRequired <uint32_t> (data, "starttime", "Particles must have start time"),
        jsonFindRequired <uint32_t> (data, "maxcount", "Particles must have maximum count"),
        visible,
        id,
        name,
        origin,
        scale,
        angles,
        controlpoints,
        emitters,
        initializers,
        std::move(dependencies)
    );
}

CParticle::CParticle (
    const Wallpapers::CScene* scene, uint32_t starttime, uint32_t maxcount, const CUserSettingBoolean* visible, int id,
    const std::string& name, const CUserSettingVector3* origin, const CUserSettingVector3* scale,
    const CUserSettingVector3* angles, const std::vector<const Particles::CControlPoint*>& controlpoints,
    const std::vector<const Particles::CEmitter*>& emitters,
    const std::vector<const Particles::CInitializer*>& initializers, std::vector<int> dependencies
) :
    CObject (scene, visible, id, name, Type, origin, scale, angles, std::move(dependencies)),
    m_starttime (starttime),
    m_maxcount (maxcount),
    m_controlpoints (controlpoints),
    m_emitters (emitters),
    m_initializers (initializers) {}

const std::vector<const Particles::CEmitter*>& CParticle::getEmitters () const {
    return this->m_emitters;
}

const std::vector<const Particles::CControlPoint*>& CParticle::getControlPoints () const {
    return this->m_controlpoints;
}

const std::vector<const Particles::CInitializer*>& CParticle::getInitializers () const {
    return this->m_initializers;
}

const std::string CParticle::Type = "particle";