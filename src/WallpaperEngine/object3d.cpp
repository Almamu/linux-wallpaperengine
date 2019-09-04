#include <WallpaperEngine/object3d.h>
#include <WallpaperEngine/image.h>

namespace WallpaperEngine
{
    object3d::object3d (object3d::Type type, WallpaperEngine::object* parent)
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
        return this->m_type == Type::Type_Material;
    }

    void object3d::render ()
    {

    }
}