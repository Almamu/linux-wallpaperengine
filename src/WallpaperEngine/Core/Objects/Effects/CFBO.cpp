#include "CFBO.h"

#include <utility>

using namespace WallpaperEngine::Core::Objects::Effects;

CFBO::CFBO (std::string name, float scale, std::string format) :
    m_name (std::move(name)),
    m_scale (scale),
    m_format(std::move(format))
{
}

CFBO* CFBO::fromJSON (json data)
{
    auto name_it = data.find ("name");
    auto scale_it = data.find ("scale");
    auto format_it = data.find ("format");

    return new CFBO (
        *name_it,
        *scale_it,
        *format_it
    );
}

const std::string& CFBO::getName () const
{
    return this->m_name;
}

const float& CFBO::getScale () const
{
    return this->m_scale;
}

const std::string& CFBO::getFormat () const
{
    return this->m_format;
}