#pragma once

#include "WallpaperEngine/Core/Core.h"

namespace WallpaperEngine::Core::Projects
{
    using json = nlohmann::json;

    class CPropertyColor;

    class CProperty
    {
    public:
        static CProperty* fromJSON (json data, const std::string& name);

        template<class T> const T* as () const { assert (is <T> ()); return (const T*) this; }
        template<class T> T* as () { assert (is <T> ()); return (T*) this; }

        template<class T> bool is () { return this->m_type == T::Type; }

        const std::string& getName () const;
        const std::string& getType () const;
        const std::string& getText () const;
    protected:
        CProperty (std::string name, std::string type, std::string text);

        std::string m_type;
        std::string m_name;
        std::string m_text;
    };
}
