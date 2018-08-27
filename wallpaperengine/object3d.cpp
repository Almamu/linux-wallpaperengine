#include <wallpaperengine/object3d.h>
#include <wallpaperengine/image.h>

namespace wp
{
    object3d::object3d (object3d::Type type, wp::scene* scene)
    {
        this->m_type = type;
        this->m_scene = scene;
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
        return this->m_type == Type::Type_Material;
    }

    void object3d::render ()
    {

    }
}