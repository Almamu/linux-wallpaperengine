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
                CScene* scene,
                const std::string& filename,
                const CContainer* container,
                CUserSettingBoolean* visible,
                uint32_t id,
                std::string name,
                CUserSettingVector3* origin,
                CUserSettingVector3* scale
        );

        const std::vector<Particles::CEmitter*>& getEmitters () const;
        const std::vector<Particles::CControlPoint*>& getControlPoints () const;
        const std::vector<Particles::CInitializer*>& getInitializers () const;

    protected:
        CParticle (
            CScene* scene,
            uint32_t starttime,
            uint32_t maxcount,
            CUserSettingBoolean* visible,
            uint32_t id,
            std::string name,
            CUserSettingVector3* origin,
            CUserSettingVector3* scale
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
