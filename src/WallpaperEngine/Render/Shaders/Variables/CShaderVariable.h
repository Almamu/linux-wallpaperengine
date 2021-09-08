#pragma once

#include <string>

namespace WallpaperEngine::Render::Shaders::Variables
{
    class CShaderVariable
    {
    public:
        CShaderVariable (void* defaultValue, void* value, std::string type);

        template<class T> const T* as () const { assert (is <T> ()); return (const T*) this; }
        template<class T> T* as () { assert (is <T> ()); return (T*) this; }

        template<class T> bool is () { return this->m_type == T::Type; }

        const std::string& getIdentifierName () const;
        const std::string& getName () const;
        const std::string& getType () const;

        void setIdentifierName (std::string identifierName);
        void setName (std::string name);
        const void* getValue () const;

        virtual const int getSize () const = 0;

    protected:
        void setValue (void* value);

    private:
        std::string m_identifierName;
        std::string m_name;
        std::string m_type;

        void* m_defaultValue;
        void* m_value;
    };
}
