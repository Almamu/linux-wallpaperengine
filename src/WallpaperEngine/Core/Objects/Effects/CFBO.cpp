#include "CFBO.h"

#include <utility>

using namespace WallpaperEngine::Core;
using namespace WallpaperEngine::Core::Objects::Effects;

CFBO::CFBO (std::string name, float scale, std::string format) :
    m_name (std::move(name)),
    m_scale (scale),
    m_format (std::move(format)) {}

const CFBO* CFBO::fromJSON (const json& data) {
    return new CFBO (
        jsonFindRequired <std::string> (data, "name", "Name for an FBO is required"),
        jsonFindDefault <float> (data, "scale", 1.0),
        jsonFindDefault <std::string> (data, "format", "")
    );
}

const std::string& CFBO::getName () const {
    return this->m_name;
}

const float& CFBO::getScale () const {
    return this->m_scale;
}

const std::string& CFBO::getFormat () const {
    return this->m_format;
}