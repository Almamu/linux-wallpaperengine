#include "CRotationRandom.h"

#include "WallpaperEngine/Core/Core.h"

using namespace WallpaperEngine::Core::Objects::Particles::Initializers;

CRotationRandom* CRotationRandom::fromJSON (json data, uint32_t id)
{
    auto min_it = data.find ("minVector");
    auto max_it = data.find ("max");

    glm::vec3 minVector = glm::vec3 ();
    glm::vec3 maxVector = glm::vec3 ();
    double minNumber = 0.0f;
    double maxNumber = 0.0f;
    bool isMinVector = false;
    bool isMaxVector = false;

    if (min_it != data.end () && min_it->is_string ())
    {
        minVector = WallpaperEngine::Core::aToVector3 (*min_it);
        isMinVector = true;
    }
    else if (min_it != data.end () && min_it->is_number ())
    {
        minNumber = *min_it;
        isMinVector = false;
    }

    if (max_it != data.end () && max_it->is_string ())
    {
        maxVector = WallpaperEngine::Core::aToVector3 (*max_it);
        isMaxVector = true;
    }
    else if(max_it != data.end () && max_it->is_number ())
    {
        maxNumber = *max_it;
        isMaxVector = false;
    }

    return new CRotationRandom (id, minVector, minNumber, isMinVector, maxVector, maxNumber, isMaxVector);
}

CRotationRandom::CRotationRandom (
    uint32_t id,
    glm::vec3 minVector,
    double minNumber,
    bool isMinimumVector,
    glm::vec3 maxVector,
    double maxNumber,
    bool isMaximumVector
) :
        CInitializer (id, "rotationrandom"),
        m_minVector (minVector),
        m_maxVector (maxVector),
        m_minNumber (minNumber),
        m_maxNumber (maxNumber),
        m_isMinimumVector (isMinimumVector),
        m_isMaximumVector (isMaximumVector)
{
}

const glm::vec3 CRotationRandom::getMinimumVector () const
{
    return this->m_minVector;
}

const glm::vec3 CRotationRandom::getMaximumVector () const
{
    return this->m_maxVector;
}

const double CRotationRandom::getMinimumNumber () const
{
    return this->m_minNumber;
}

const double CRotationRandom::getMaximumNumber () const
{
    return this->m_maxNumber;
}

const bool CRotationRandom::isMinimumVector () const
{
    return this->m_isMinimumVector == true;
}

const bool CRotationRandom::isMinimumNumber () const
{
    return this->m_isMinimumVector == false;
}

const bool CRotationRandom::isMaximumVector () const
{
    return this->m_isMaximumVector == true;
}

const bool CRotationRandom::isMaximumNumber () const
{
    return this->m_isMaximumVector == false;
}