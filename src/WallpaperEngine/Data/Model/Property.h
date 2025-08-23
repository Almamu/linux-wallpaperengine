#pragma once

#include <map>
#include <string>
#include <utility>
#include "DynamicValue.h"
#include "../Utils/TypeCaster.h"

namespace WallpaperEngine::Data::Model {
using namespace WallpaperEngine::Data::Utils;
using namespace WallpaperEngine::Data::Builders;

struct PropertyData {
    std::string name;
    std::string text;
};

struct SliderData {
    float min;
    float max;
    float step;
};

struct ComboData {
    std::map<std::string, std::string> values;
};

class Property : public DynamicValue, public TypeCaster, public PropertyData {
  public:
    explicit Property (PropertyData data) : PropertyData (std::move(data)), TypeCaster (), DynamicValue () {}

    using DynamicValue::update;
    virtual void update(const std::string& value) = 0;
};

class PropertySlider : public Property, SliderData {
  public:
    PropertySlider (PropertyData data, SliderData sliderData, float value) : Property (std::move(data)), SliderData (sliderData) {
        this->update (value);
    }

    using Property::update;
    void update(const std::string& value) override {
        this->update (std::stof (value));
    }
};

class PropertyBoolean : public Property {
  public:
    explicit PropertyBoolean (PropertyData data, bool value) : Property (std::move(data)) {
        this->update (value);
    }

    using Property::update;
    void update(const std::string& value) override {
        this->update (value == "true" || value == "1");
    }
};

class PropertyColor : public Property {
  public:
    explicit PropertyColor (PropertyData data, std::string value) : Property (std::move(data)) {
        this->update (value);
    }

    using Property::update;
    void update(const std::string& value) override {
        auto copy = value;

        // TODO: ENSURE ALL THIS PARSING IS CORRECT
        if (copy.find (',') != std::string::npos) {
            // replace comma separator with spaces so it's
            std::replace (copy.begin (), copy.end (), ',', ' ');
        }

        // hex colors should be converted to int colors
        if (copy.find ('#') == 0) {
            auto number = copy.substr (1);

            // support for css notation
            if (number.size () == 3) {
                number = number[0] + number[0] + number[1] + number[1] + number[2] + number[2];
            }

            // remove alpha if it's present, should look into it more closely
            if (number.size () > 6) {
                sLog.error ("Color value has alpha channel, which is not supported");
                number = number.substr (0, 6);
            }

            const auto color = std::stoi (number, nullptr, 16);

            // format the number as float vector
            copy =
                std::to_string (((color >> 16) & 0xFF) / 255.0) + " " +
                std::to_string (((color >> 8) & 0xFF) / 255.0) + " " +
                std::to_string ((color & 0xFF) / 255.0);
        } else if (copy.find ('.') == std::string::npos) {
            // integer vector, convert it to float vector
            const auto intcolor = VectorBuilder::parse <glm::ivec3> (copy);

            copy =
                std::to_string (intcolor.r / 255.0) + " " +
                std::to_string (intcolor.g / 255.0) + " " +
                std::to_string (intcolor.b / 255.0);
        }

        // finally parse the string as a float vector
        this->update (VectorBuilder::parse <glm::vec3> (copy));
    }
};

class PropertyCombo : public Property, ComboData {
  public:
    PropertyCombo (PropertyData data, ComboData comboData, std::string value) : Property (std::move(data)), ComboData (std::move(comboData)) {
        this->update (value);
    }

    using Property::update;
    void update(const std::string& value) override {
        this->update (std::stoi (value));
    }
};

class PropertyText : public Property {
  public:
    explicit PropertyText (PropertyData data) : Property (std::move(data)) {}

    using Property::update;
    void update(const std::string& value) override {
        throw std::runtime_error ("PropertyText::update() is not implemented");
    }

    [[nodiscard]] std::string toString () const override {
        return this->text;
    }
};

class PropertySceneTexture : public Property {
  public:
    explicit PropertySceneTexture (PropertyData data, std::string value) : Property (std::move(data)) {
        this->update (value);
    }

    void update(const std::string& value) override {
        this->m_value = value;
    }

  private:
    std::string m_value;
};
}