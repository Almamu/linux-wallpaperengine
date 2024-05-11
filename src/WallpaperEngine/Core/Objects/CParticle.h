#pragma once

#include "WallpaperEngine/Core/Objects/Particles/CControlPoint.h"
#include "WallpaperEngine/Core/Objects/Particles/CEmitter.h"
#include "WallpaperEngine/Core/Objects/Particles/CInitializer.h"

#include "WallpaperEngine/Core/CObject.h"
#include "WallpaperEngine/Core/Core.h"

namespace WallpaperEngine::Core::Objects {
using json = nlohmann::json;

/**
 * Represents a particle system in the background
 */
class CParticle : public CObject {
    friend class CObject;

  public:
    static CParticle* fromFile (CScene* scene, const std::string& filename, CContainer* container,
                                CUserSettingBoolean* visible, int id, std::string name,
                                CUserSettingVector3* origin, CUserSettingVector3* scale);

    /**
     * @return The list of emitters for the particle system
     */
    [[nodiscard]] const std::vector<Particles::CEmitter*>& getEmitters () const;
    /**
     * @return The list of control points for the particle system
     */
    [[nodiscard]] const std::vector<Particles::CControlPoint*>& getControlPoints () const;
    /**
     * @return The list of initializers for the particle system
     */
    [[nodiscard]] const std::vector<Particles::CInitializer*>& getInitializers () const;

  protected:
    CParticle (CScene* scene, uint32_t starttime, uint32_t maxcount, CUserSettingBoolean* visible, int id,
               std::string name, CUserSettingVector3* origin, CUserSettingVector3* scale);

    /**
     * @param controlpoint The control point to add to the particle system
     */
    void insertControlPoint (Particles::CControlPoint* controlpoint);
    /**
     * @param emitter The emitter to add to the particle system
     */
    void insertEmitter (Particles::CEmitter* emitter);
    /**
     * @param initializer The initializer to add to the particle system
     */
    void insertInitializer (Particles::CInitializer* initializer);

    /**
     * Type value used to differentiate the different types of objects in a background
     */
    static const std::string Type;

  private:
    /** The time at which the particle system should start emitting */
    uint32_t m_starttime;
    /** Maximum number of particles at the same time */
    uint32_t m_maxcount;
    /** List of control points */
    std::vector<Particles::CControlPoint*> m_controlpoints;
    /** List of emitters */
    std::vector<Particles::CEmitter*> m_emitters;
    /** List of initializers */
    std::vector<Particles::CInitializer*> m_initializers;
};
} // namespace WallpaperEngine::Core::Objects
