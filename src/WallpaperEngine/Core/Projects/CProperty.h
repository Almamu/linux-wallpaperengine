#pragma once

#include "WallpaperEngine/Data/Utils/TypeCaster.h"
#include "WallpaperEngine/Data/Model/DynamicValue.h"
#include "WallpaperEngine/Data/JSON.h"

namespace WallpaperEngine::Core::Projects {
using JSON = WallpaperEngine::Data::JSON::JSON;
using namespace WallpaperEngine::Data::Model;
using namespace WallpaperEngine::Data::Utils;
/**
 * Represents a property in a background
 *
 * Properties are settings that alter how the background looks or works
 * and are configurable by the user so they can customize it to their likings
 */
class CProperty : public DynamicValue, public TypeCaster {
  public:
    using UniquePtr = std::unique_ptr<CProperty>;
    using SharedPtr = std::shared_ptr<CProperty>;
    using WeakPtr = std::weak_ptr<CProperty>;

    static std::shared_ptr<CProperty> fromJSON (const JSON& data, const std::string& name);

    /**
     * @return Representation of what the property does and the default values
     */
    [[nodiscard]] virtual std::string dump () const = 0;
    /**
     * Updates the value of the property with the one in the string
     *
     * @param value New value for the property
     */
    virtual void set (const std::string& value) = 0;
    /**
     * @return Name of the property
     */
    [[nodiscard]] const std::string& getName () const;
    /**
     * @return Textual type representation of this property
     */
    [[nodiscard]] virtual const char* getPropertyType () const = 0;
    /**
     * @return Text of the property
     */
    [[nodiscard]] const std::string& getText () const;

  protected:
    CProperty (std::string name, std::string text);
    /** Name of the property */
    const std::string m_name;
    /** Description of the property for the user */
    mutable std::string m_text;
};
} // namespace WallpaperEngine::Core::Projects
