#include "object3d.h"
#include "image.h"

namespace wp
{
    object3d::object3d (object3d::Type type)
    {
        this->m_type = type;
    }

    template <class T> T* object3d::as()
    {
        return (T*) this;
    }

    template <class T> bool object3d::is()
    {
        return false;
    }

    template <> bool object3d::is<image>()
    {
        return this->m_type == Type::Type_Image;
    }
}