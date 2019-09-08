#pragma once

#include <string>

namespace WallpaperEngine::Core::Objects::Effects
{
    class CShaderConstant
    {
    public:
        CShaderConstant (std::string  type);

        template<class T> const T* As () const { assert (Is<T> ()); return (const T*) this; }
        template<class T> T* As () { assert (Is<T> ()); return (T*) this; }

        template<class T> bool Is () { return this->m_type == T::Type; }

    private:
        std::string m_type;
    };
}
