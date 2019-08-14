#pragma once

#include <irrlicht/irrlicht.h>
#include <nlohmann/json.hpp>

#include "controlpoint.h"
#include "emitter.h"
#include "initializer.h"

#include "../../object.h"

namespace wp::core::objects::particles
{
    using json = nlohmann::json;

    class particle : object
    {
    public:
        std::vector<emitter*>* getEmitters ();
        std::vector<controlpoint*>* getControlPoints ();
        std::vector<initializer*>* getInitializers ();
    protected:
        friend class object;

        static particle* fromFile (
                const irr::io::path& filename,
                irr::u32 id,
                std::string name,
                const irr::core::vector3df& origin,
                const irr::core::vector3df& scale
        );

        particle (
            irr::u32 starttime,
            irr::u32 maxcount,
            irr::u32 id,
            std::string name,
            const irr::core::vector3df& origin,
            const irr::core::vector3df& scale
        );
        void insertControlPoint (controlpoint* controlpoint);
        void insertEmitter (emitter* emitter);
        void insertInitializer (initializer* initializer);
    private:
        irr::u32 m_starttime;
        irr::u32 m_maxcount;
        std::vector<controlpoint*> m_controlpoints;
        std::vector<emitter*> m_emitters;
        std::vector<initializer*> m_initializers;
    };
};
