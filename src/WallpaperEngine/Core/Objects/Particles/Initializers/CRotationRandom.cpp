#include "CRotationRandom.h"

#include "WallpaperEngine/Core/Core.h"

using namespace WallpaperEngine::Core::Objects::Particles::Initializers;

CRotationRandom* CRotationRandom::fromJSON (json data, irr::u32 id)
{
    auto min_it = data.find ("minVector");
    auto max_it = data.find ("max");

    irr::core::vector3df minVector = irr::core::vector3df ();
    irr::core::vector3df maxVector = irr::core::vector3df ();
    irr::f64 minNumber = 0.0f;
    irr::f64 maxNumber = 0.0f;
    bool isMinVector = false;
    bool isMaxVector = false;

    if (min_it != data.end () && min_it->is_string ())
    {
        minVector = WallpaperEngine::Core::ato3vf (*min_it);
        isMinVector = true;
    }
    else if (min_it != data.end () && min_it->is_number ())
    {
        minNumber = *min_it;
        isMinVector = false;
    }

    if (max_it != data.end () && max_it->is_string ())
    {
        maxVector = WallpaperEngine::Core::ato3vf (*max_it);
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
    irr::u32 id,
    irr::core::vector3df minVector,
    irr::f64 minNumber,
    bool isMinimumVector,
    irr::core::vector3df maxVector,
    irr::f64 maxNumber,
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

const irr::core::vector3df CRotationRandom::getMinimumVector () const
{
    return this->m_minVector;
}

const irr::core::vector3df CRotationRandom::getMaximumVector () const
{
    return this->m_maxVector;
}

const irr::f64 CRotationRandom::getMinimumNumber () const
{
    return this->m_minNumber;
}

const irr::f64 CRotationRandom::getMaximumNumber () const
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