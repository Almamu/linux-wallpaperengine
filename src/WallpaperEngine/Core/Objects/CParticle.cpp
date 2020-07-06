#include "CParticle.h"
#include "WallpaperEngine/FileSystem/FileSystem.h"

#include <irrlicht/irrlicht.h>

using namespace WallpaperEngine::Core::Objects;

CParticle* CParticle::fromFile (
    const irr::io::path& filename,
    irr::u32 id,
    std::string name,
    const irr::core::vector3df& origin,
    const irr::core::vector3df& scale)
{
    json data = json::parse (WallpaperEngine::FileSystem::loadFullFile (filename));
    auto controlpoint_it = data.find ("controlpoint");
    auto starttime_it = jsonFindRequired (data, "starttime", "Particles must have start time");
    auto maxcount_it = jsonFindRequired (data, "maxcount", "Particles must have maximum count");
    auto emitter_it = jsonFindRequired (data, "emitter", "Particles must have emitters");
    auto initializer_it = jsonFindRequired (data, "initializer", "Particles must have initializers");

    CParticle* particle = new CParticle (
        *starttime_it,
        *maxcount_it,
        id,
        name,
        origin,
        scale
    );

    if (controlpoint_it != data.end ())
    {
        auto cur = (*controlpoint_it).begin ();
        auto end = (*controlpoint_it).end ();

        for (; cur != end; cur ++)
        {
            particle->insertControlPoint (
                    Particles::CControlPoint::fromJSON (*cur)
            );
        }
    }

    auto cur = (*emitter_it).begin ();
    auto end = (*emitter_it).end ();

    for (; cur != end; cur ++)
    {
        particle->insertEmitter (
                Particles::CEmitter::fromJSON (*cur)
        );
    }

    cur = (*initializer_it).begin ();
    end = (*initializer_it).end ();

    for (; cur != end; cur ++)
    {
        particle->insertInitializer (
                Particles::CInitializer::fromJSON (*cur)
        );
    }

    return particle;
}

CParticle::CParticle (
        irr::u32 starttime,
        irr::u32 maxcount,
        irr::u32 id,
        std::string name,
        const irr::core::vector3df& origin,
        const irr::core::vector3df& scale):
        CObject (true, id, std::move(name), Type, origin, scale, irr::core::vector3df ()),
        m_starttime (starttime),
        m_maxcount (maxcount)
{
}

const std::vector<Particles::CEmitter*>& CParticle::getEmitters () const
{
    return this->m_emitters;
}

const std::vector<Particles::CControlPoint*>& CParticle::getControlPoints () const
{
    return this->m_controlpoints;
}

const std::vector<Particles::CInitializer*>& CParticle::getInitializers () const
{
    return this->m_initializers;
}

void CParticle::insertControlPoint (Particles::CControlPoint* controlpoint)
{
    this->m_controlpoints.push_back (controlpoint);
}

void CParticle::insertEmitter (Particles::CEmitter* emitter)
{
    this->m_emitters.push_back (emitter);
}

void CParticle::insertInitializer (Particles::CInitializer* initializer)
{
    this->m_initializers.push_back (initializer);
}

const std::string CParticle::Type = "particle";