#include "CParticle.h"

#include "WallpaperEngine/FileSystem/FileSystem.h"
#include <utility>

using namespace WallpaperEngine::Core::Objects;

CParticle* CParticle::fromFile (CScene* scene, const std::string& filename, CContainer* container,
                                CUserSettingBoolean* visible, int id, std::string name, CUserSettingVector3* origin,
                                CUserSettingVector3* scale) {
    json data = json::parse (WallpaperEngine::FileSystem::loadFullFile (filename, container));
    const auto controlpoint_it = data.find ("controlpoint");
    const auto starttime_it = jsonFindRequired (data, "starttime", "Particles must have start time");
    const auto maxcount_it = jsonFindRequired (data, "maxcount", "Particles must have maximum count");
    const auto emitter_it = jsonFindRequired (data, "emitter", "Particles must have emitters");
    const auto initializer_it = jsonFindRequired (data, "initializer", "Particles must have initializers");

    auto* particle = new CParticle (scene, *starttime_it, *maxcount_it, visible, id, std::move (name), origin, scale);

    if (controlpoint_it != data.end ())
        for (const auto& cur : (*controlpoint_it))
            particle->insertControlPoint (Particles::CControlPoint::fromJSON (cur));

    for (const auto& cur : (*emitter_it))
        particle->insertEmitter (Particles::CEmitter::fromJSON (cur));
    for (const auto& cur : (*initializer_it))
        particle->insertInitializer (Particles::CInitializer::fromJSON (cur));

    return particle;
}

CParticle::CParticle (CScene* scene, uint32_t starttime, uint32_t maxcount, CUserSettingBoolean* visible, int id,
                      std::string name, CUserSettingVector3* origin, CUserSettingVector3* scale) :
    CObject (scene, visible, id, std::move (name), Type, origin, scale, glm::vec3 ()),
    m_starttime (starttime),
    m_maxcount (maxcount) {}

const std::vector<Particles::CEmitter*>& CParticle::getEmitters () const {
    return this->m_emitters;
}

const std::vector<Particles::CControlPoint*>& CParticle::getControlPoints () const {
    return this->m_controlpoints;
}

const std::vector<Particles::CInitializer*>& CParticle::getInitializers () const {
    return this->m_initializers;
}

void CParticle::insertControlPoint (Particles::CControlPoint* controlpoint) {
    this->m_controlpoints.push_back (controlpoint);
}

void CParticle::insertEmitter (Particles::CEmitter* emitter) {
    this->m_emitters.push_back (emitter);
}

void CParticle::insertInitializer (Particles::CInitializer* initializer) {
    this->m_initializers.push_back (initializer);
}

const std::string CParticle::Type = "particle";