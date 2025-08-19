#include "CPropertySlider.h"
#include "WallpaperEngine/Logging/CLog.h"
#include <sstream>

using namespace WallpaperEngine::Core::Projects;

std::shared_ptr<CPropertySlider> CPropertySlider::fromJSON (const JSON& data, const std::string& name) {
    const auto value = data.find ("value");
    const auto text = data.optional <std::string> ("text", "");
    const auto min = data.optional ("min", 0.0f);
    const auto max = data.optional ("max", 0.0f);
    const auto step = data.optional ("step", 0.0f);

    return std::make_shared <CPropertySlider> (*value, name, text, min, max, step);
}

const float& CPropertySlider::getMinValue () const {
    return this->m_min;
}

const float& CPropertySlider::getMaxValue () const {
    return this->m_max;
}

const float& CPropertySlider::getStep () const {
    return this->m_step;
}

std::string CPropertySlider::dump () const {
    std::stringstream ss;

    ss << this->m_name << " - slider" << std::endl
       << "\t"
       << "Description: " << this->m_text << std::endl
       << "\t"
       << "Value: " << &this->getFloat () << std::endl
       << "\t"
       << "Minimum value: " << this->m_min << std::endl
       << "\t"
       << "Maximum value: " << this->m_max << std::endl
       << "\t"
       << "Step: " << this->m_step << std::endl;

    return ss.str ();
}

void CPropertySlider::set (const std::string& value) {
    const auto newValue = strtof (value.c_str (), nullptr);

    if (newValue < this->m_min || newValue > this->m_max)
        sLog.exception ("Slider value (", newValue, ") is out of range (", this->m_min, ",", this->m_max, ")");

    this->update (newValue);
}

const char* CPropertySlider::getPropertyType () const {
    return "slider";
}

CPropertySlider::CPropertySlider (float value, const std::string& name, const std::string& text, float min,
                                  float max, float step) :
    CProperty (name, text),
    m_min (min),
    m_max (max),
    m_step (step) {
    this->update (value);
}
