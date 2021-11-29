#include "CPropertySlider.h"

using namespace WallpaperEngine::Core::Projects;

CPropertySlider* CPropertySlider::fromJSON (json data, const std::string& name)
{
    auto value = data.find ("value");
    auto text = data.find ("type");
    auto min = jsonFindDefault(data, "min", 0.0);
    auto max = jsonFindDefault (data, "max", 0.0);
    auto step = jsonFindDefault (data, "step", 0.0);

    return new CPropertySlider (
        *value,
        name,
        *text,
        min,
        max,
        step
    );
}

const double& CPropertySlider::getValue () const
{
    return this->m_value;
}

const double& CPropertySlider::getMinValue () const
{
    return this->m_min;
}

const double& CPropertySlider::getMaxValue () const
{
    return this->m_max;
}

const double& CPropertySlider::getStep () const
{
    return this->m_step;
}

CPropertySlider::CPropertySlider (double value, const std::string& name, const std::string& text, double min, double max, double step) :
    CProperty (name, Type, text),
    m_value (value), m_min (min), m_max (max), m_step (step)
{
}

const std::string CPropertySlider::Type = "slider";