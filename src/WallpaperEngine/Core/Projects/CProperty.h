#pragma once

#include "WallpaperEngine/Core/Core.h"

namespace WallpaperEngine::Core::Projects {
using json = nlohmann::json;

/**
 * Represents a property in a background
 *
 * Properties are settings that alter how the background looks or works
 * and are configurable by the user so they can customize it to their likings
 */
class CPropertyColor;

class CProperty {
  public:
    virtual ~CProperty () = default;
    static const CProperty* fromJSON (const json& data, const std::string& name);

    template <class T> const T* as () const {
        assert (is<T> ());
        return reinterpret_cast<const T*> (this);
    }

    template <class T> T* as () {
        assert (is<T> ());
        return reinterpret_cast<T*> (this);
    }

    template <class T> bool is () const {
        return this->m_type == T::Type;
    }

    /**
     * @return Representation of what the property does and the default values
     */
    [[nodiscard]] virtual std::string dump () const = 0;
    /**
     * Updates the value of the property with the one in the string
     *
     * @param value New value for the property
     */
    virtual void update (const std::string& value) const = 0;

    /**
     * @return Name of the property
     */
    [[nodiscard]] const std::string& getName () const;
    /**
     * @return Type of the property
     */
    [[nodiscard]] const std::string& getType () const;
    /**
     * @return Text of the property
     */
    [[nodiscard]] const std::string& getText () const;

  protected:
    CProperty (std::string name, std::string type, std::string text);

    /** Type of property */
    const std::string m_type;
    /** Name of the property */
    const std::string m_name;
    /** Description of the property for the user */
    mutable std::string m_text;
};
} // namespace WallpaperEngine::Core::Projects
