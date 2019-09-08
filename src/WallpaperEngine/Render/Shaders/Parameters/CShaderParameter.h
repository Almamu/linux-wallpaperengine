#pragma once

#include <string>

namespace WallpaperEngine::Render::Shaders::Parameters
{
    class CShaderParameter
    {
    public:
        CShaderParameter (void* defaultValue, void* value, std::string type);

        template<class T> const T* As () const { assert (Is<T> ()); return (const T*) this; }
        template<class T> T* As () { assert (Is<T> ()); return (T*) this; }

        template<class T> bool Is () { return this->m_type == T::Type; }

        std::string getIdentifierName ();
        std::string getName ();

        void setIdentifierName (std::string identifierName);
        void setName (std::string name);
        void* getValue ();

        virtual int getSize () = 0;

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
