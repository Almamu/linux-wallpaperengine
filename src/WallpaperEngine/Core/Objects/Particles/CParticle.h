#pragma once

#include <irrlicht/irrlicht.h>
#include <nlohmann/json.hpp>

#include "CControlPoint.h"
#include "CEmitter.h"
#include "CInitializer.h"

#include "WallpaperEngine/Core/CObject.h"

namespace WallpaperEngine::Core::Objects::Particles
{
    using json = nlohmann::json;

    class CParticle : CObject
    {
    public:
        std::vector<CEmitter*>* getEmitters ();
        std::vector<CControlPoint*>* getControlPoints ();
        std::vector<CInitializer*>* getInitializers ();
    protected:
        friend class CObject;

        static CParticle* fromFile (
                const irr::io::path& filename,
                irr::u32 id,
                std::string name,
                const irr::core::vector3df& origin,
                const irr::core::vector3df& scale
        );

        CParticle (
            irr::u32 starttime,
            irr::u32 maxcount,
            irr::u32 id,
            std::string name,
            const irr::core::vector3df& origin,
            const irr::core::vector3df& scale
        );
        void insertControlPoint (CControlPoint* controlpoint);
        void insertEmitter (CEmitter* emitter);
        void insertInitializer (CInitializer* initializer);
    private:
        irr::u32 m_starttime;
        irr::u32 m_maxcount;
        std::vector<CControlPoint*> m_controlpoints;
        std::vector<CEmitter*> m_emitters;
        std::vector<CInitializer*> m_initializers;
    };
};
