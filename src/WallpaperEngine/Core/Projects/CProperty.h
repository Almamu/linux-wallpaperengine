#pragma once

#include "WallpaperEngine/Core/DynamicValues/CDynamicValue.h"
#include "WallpaperEngine/Core/Core.h"

namespace WallpaperEngine::Core::Projects {
using json = nlohmann::json;
using namespace WallpaperEngine::Core::DynamicValues;
/**
 * Represents a property in a background
 *
 * Properties are settings that alter how the background looks or works
 * and are configurable by the user so they can customize it to their likings
 */
class CProperty : public CDynamicValue {
  public:
    using UniquePtr = std::unique_ptr<CProperty>;
    using SharedPtr = std::shared_ptr<CProperty>;
    using WeakPtr = std::weak_ptr<CProperty>;

    typedef std::function<void(const CProperty*)> function_type;
    virtual ~CProperty () = default;
    static std::shared_ptr<CProperty> fromJSON (const json& data, const std::string& name);

    template <class T> [[nodiscard]] const T* as () const {
        if (is <T> ()) {
            return static_cast <const T*> (this);
        }

        throw std::bad_cast ();
    }

    template <class T> [[nodiscard]] T* as () {
        if (is <T> ()) {
            return static_cast <T*> (this);
        }

        throw std::bad_cast ();
    }

    template <class T> [[nodiscard]] bool is () const {
        return typeid (*this) == typeid(T);
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
    virtual void set (const std::string& value) = 0;
    /**
     * @return Name of the property
     */
    [[nodiscard]] const std::string& getName () const;
    /**
     * @return Textual type representation of this property
     */
    [[nodiscard]] virtual const char* getType () const = 0;
    /**
     * @return Text of the property
     */
    [[nodiscard]] const std::string& getText () const;
    /**
     * Registers a function to be called when this instance's value changes
     *
     * @param callback
     */
    void subscribe (const function_type& callback) const;

  protected:
    void propagate () const override;

    CProperty (std::string name, std::string text);

    /** Functions to call when this property's value changes */
    mutable std::vector<function_type> m_subscriptions;
    /** Name of the property */
    const std::string m_name;
    /** Description of the property for the user */
    mutable std::string m_text;
};
} // namespace WallpaperEngine::Core::Projects
