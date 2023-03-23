#pragma once

#include <string>

namespace WallpaperEngine::Core::Objects::Effects::Constants
{
    /**
     * Shader constants base class
     */
    class CShaderConstant
    {
    public:
        explicit CShaderConstant (std::string type);

        template<class T> const T* as () const { assert (is <T> ()); return (const T*) this; }
        template<class T> T* as () { assert (is <T> ()); return (T*) this; }

        template<class T> bool is () { return this->m_type == T::Type; }

        /**
         * @return The type name of this constant
         */
        [[nodiscard]] const std::string& getType () const;

    private:
        std::string m_type;
    };
}
