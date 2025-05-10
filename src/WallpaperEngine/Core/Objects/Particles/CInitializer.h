#pragma once

#include "WallpaperEngine/Core/Core.h"

namespace WallpaperEngine::Core::Objects::Particles {
using json = nlohmann::json;

/**
 * Initializer for particles, controls the different attributes a particle will have
 * on emission
 */
class CInitializer {
  public:
    static const CInitializer* fromJSON (const json& data);

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
     * @return The name of the particle initializer, indicates what type of initialization to do
     */
    [[nodiscard]] const std::string& getName () const;
    /**
     * @return The id of the initializer
     */
    [[nodiscard]] uint32_t getId () const;

  protected:
    CInitializer (uint32_t id, std::string name);

    virtual ~CInitializer() = default;

  private:
    /** ID for ordering purposes */
    const uint32_t m_id;
    /** The name of the initializer, indicates what type of initialization to do */
    const std::string m_name;
};
} // namespace WallpaperEngine::Core::Objects::Particles
