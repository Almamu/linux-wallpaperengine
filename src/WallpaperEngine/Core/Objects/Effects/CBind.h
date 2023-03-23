#pragma once

#include <string>

#include "WallpaperEngine/Core/Core.h"

namespace WallpaperEngine::Core::Objects::Effects
{
    using json = nlohmann::json;

    /**
     * Material's bind information, describes for passes what textures to bind
     * in what positions for shaders. Used to override the textures specified inside
     * the object's passes
     */
    class CBind
    {
    public:
        /**
         * Parses bind information off the given json data
         *
         * @param data
         * @return
         */
        static CBind* fromJSON (json data);

        CBind (std::string name, uint32_t index);

        /**
         * @return The texture name, previous to use the one already specified by the object's passes
         */
        [[nodiscard]] const std::string& getName () const;
        /**
         * @return The texture index to replace
         */
        [[nodiscard]] const uint32_t& getIndex () const;

    private:
        /** The texture's name */
        std::string m_name;
        /** The texture index to replace */
        uint32_t m_index;
    };
}
