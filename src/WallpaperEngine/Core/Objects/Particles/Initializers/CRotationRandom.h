#pragma once

#include "WallpaperEngine/Core/Objects/Particles/CInitializer.h"

#include <nlohmann/json.hpp>

namespace WallpaperEngine::Core::Objects::Particles::Initializers {
/**
 * Initializer for particles that decides the base rotation for the particles
 */
class CRotationRandom : CInitializer {
  public:
    /**
     * @return The minimum rotation in vector format if available
     */
    [[nodiscard]] glm::vec3 getMinimumVector () const;
    /**
     * @return The maximum rotation in vector format if available
     */
    [[nodiscard]] glm::vec3 getMaximumVector () const;
    /**
     * @return The minimum rotation in angle format if available
     */
    [[nodiscard]] double getMinimumNumber () const;
    /**
     * @return The maximum rotation in angle format if available
     */
    [[nodiscard]] double getMaximumNumber () const;

    /**
     * @return Indicates if the minimum rotation is a vector
     */
    [[nodiscard]] bool isMinimumVector () const;
    /**
     * @return Indicates if the minimum rotation is an angle
     */
    [[nodiscard]] bool isMinimumNumber () const;
    /**
     * @return Indicates if the maximum rotation is a vector
     */
    [[nodiscard]] bool isMaximumVector () const;
    /**
     * @return Indicates if the maximum rotation is an angle
     */
    [[nodiscard]] bool isMaximumNumber () const;

  protected:
    friend class CInitializer;

    static const CRotationRandom* fromJSON (const json& data, uint32_t id);

    CRotationRandom (uint32_t id, glm::vec3 minVector, double minNumber, bool isMinimumVector, glm::vec3 maxVector,
                     double maxNumber, bool isMaximumVector);

  private:
    /** Maximum rotation vector */
    const glm::vec3 m_maxVector;
    /** Maximum rotation angle */
    const double m_maxNumber;
    /** Minimum rotation vector */
    const glm::vec3 m_minVector;
    /** Minimum rotation angle */
    const double m_minNumber;

    /** If minimum is a vector */
    const bool m_isMinimumVector;
    /** If maximum is a vector */
    const bool m_isMaximumVector;
};
} // namespace WallpaperEngine::Core::Objects::Particles::Initializers
