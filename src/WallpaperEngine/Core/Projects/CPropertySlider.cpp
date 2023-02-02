#include "common.h"
#include <sstream>
#include "CPropertySlider.h"

using namespace WallpaperEngine::Core::Projects;

CPropertySlider* CPropertySlider::fromJSON (json data, const std::string& name)
{
    auto value = data.find ("value");
    std::string text = jsonFindDefault <std::string> (data, "text", "");
    auto min = jsonFindDefault(data, "min", 0.0);
    auto max = jsonFindDefault (data, "max", 0.0);
    auto step = jsonFindDefault (data, "step", 0.0);

    return new CPropertySlider (
        *value,
        name,
        text,
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

std::string CPropertySlider::dump () const
{
    std::stringstream ss;

    ss
        << this->m_name << " - slider" << std::endl
        << "\t" << "Description: " << this->m_text << std::endl
        << "\t" << "Value: " << this->m_value << std::endl
        << "\t" << "Minimum value: " << this->m_min << std::endl
        << "\t" << "Maximum value: " << this->m_max << std::endl
        << "\t" << "Step: " << this->m_step << std::endl;

    return ss.str();
}

void CPropertySlider::update (const std::string& value)
{
    double newValue = atof (value.c_str ());

    if (newValue < this->m_min || newValue > this->m_max)
        sLog.exception ("Slider value (", newValue, ") is out of range (", this->m_min, ",", this->m_max, ")");

    this->m_value = newValue;
}

CPropertySlider::CPropertySlider (double value, const std::string& name, const std::string& text, double min, double max, double step) :
    CProperty (name, Type, text),
    m_value (value), m_min (min), m_max (max), m_step (step)
{
}

const std::string CPropertySlider::Type = "slider";