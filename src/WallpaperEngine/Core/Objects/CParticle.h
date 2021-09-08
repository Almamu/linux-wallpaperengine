#pragma once

#include "WallpaperEngine/Core/Objects/Particles/CControlPoint.h"
#include "WallpaperEngine/Core/Objects/Particles/CEmitter.h"
#include "WallpaperEngine/Core/Objects/Particles/CInitializer.h"

#include "WallpaperEngine/Core/Core.h"
#include "WallpaperEngine/Core/CObject.h"

namespace WallpaperEngine::Core::Objects
{
    using json = nlohmann::json;

    class CParticle : public CObject
    {
        friend class CObject;

    public:
        static CParticle* fromFile (
                const std::string& filename,
                CContainer* container,
                uint32_t id,
                std::string name,
                const glm::vec3& origin,
                const glm::vec3& scale
        );

        const std::vector<Particles::CEmitter*>& getEmitters () const;
        const std::vector<Particles::CControlPoint*>& getControlPoints () const;
        const std::vector<Particles::CInitializer*>& getInitializers () const;

    protected:
        CParticle (
            uint32_t starttime,
            uint32_t maxcount,
            uint32_t id,
            std::string name,
            const glm::vec3& origin,
            const glm::vec3& scale
        );
        void insertControlPoint (Particles::CControlPoint* controlpoint);
        void insertEmitter (Particles::CEmitter* emitter);
        void insertInitializer (Particles::CInitializer* initializer);

        static const std::string Type;
    private:
        uint32_t m_starttime;
        uint32_t m_maxcount;
        std::vector<Particles::CControlPoint*> m_controlpoints;
        std::vector<Particles::CEmitter*> m_emitters;
        std::vector<Particles::CInitializer*> m_initializers;
    };
};
