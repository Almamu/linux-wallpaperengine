#include <sstream>
#include <utility>

#include "CPropertyColor.h"

using namespace WallpaperEngine::Data::Builders;
using namespace WallpaperEngine::Core::Projects;

glm::vec3 ParseColor (std::string value) {
    // TODO: ENSURE THIS PARSING IS ACTUALLY ACCURATE
    if (value.find (',') != std::string::npos) {
        // replace commas with dots so it can be parsed
        std::replace (value.begin (), value.end (), ',', ' ');
    }

    if (value.find ('.') == std::string::npos && value != "0 0 0" && value != "1 1 1") {
        if (value.find ('#') == 0) {
            // hex color, parse it to int color and then convert it to float
            auto number = value.substr (1);

            if (number.size () == 3) {
                // CSS short-number, expand to the proper size
                number = number[0] + number[0] + number[1] + number[1] + number[2] + number[2];
            }

            // remove alpha if it's present, should look into it more closely
            if (number.size () > 6) {
                sLog.error ("Color value has alpha channel, which is not supported");
                number = number.substr (0, 6);
            }

            const auto color = std::stoi (number, nullptr, 16);

            return {
                (((color >> 16) & 0xFF) / 255.0),
                (((color >> 8) & 0xFF) / 255.0),
                (((color >> 0) & 0xFF) / 255.0)
            };
        }

        const auto intcolor = VectorBuilder::parse <glm::ivec3> (value);

        return {intcolor.r / 255.0, intcolor.g / 255.0, intcolor.b / 255.0};
    }

    return VectorBuilder::parse <glm::vec3> (value);
}

std::shared_ptr<CPropertyColor> CPropertyColor::fromJSON (const JSON& data, std::string name) {
    return std::make_shared <CPropertyColor> (
        data.require ("value", "Color property must have a value"),
        std::move(name),
        data.optional <std::string> ("text", "")
    );
}

void CPropertyColor::set (const std::string& value) {
    this->update (ParseColor (std::string (value)));
}

std::string CPropertyColor::dump () const {
    const auto color = this->getVec3 ();
    std::stringstream ss;

    ss << this->m_name << " - color" << std::endl
       << "\t"
       << "Description: " << this->m_text << std::endl
       << "\t"
       << "R: " << color.r << " G: " << color.g << " B: " << color.b;

    return ss.str ();
}

const char* CPropertyColor::getPropertyType () const {
    return "color";
}

CPropertyColor::CPropertyColor (const std::string& color, std::string name, std::string text) :
    CProperty (std::move(name), std::move(text)) {
    this->set (color);
}
