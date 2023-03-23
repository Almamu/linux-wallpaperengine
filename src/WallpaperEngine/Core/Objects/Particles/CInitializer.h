#pragma once

#include "WallpaperEngine/Core/Core.h"

namespace WallpaperEngine::Core::Objects::Particles
{
    using json = nlohmann::json;

    /**
     * Initializer for particles, controls the different attributes a particle will have
     * on emission
     */
    class CInitializer
    {
    public:
        static CInitializer* fromJSON (json data);

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

    private:
        /** ID for ordering purposes */
        uint32_t m_id;
        /** The name of the initializer, indicates what type of initialization to do */
        std::string m_name;
    };
}
