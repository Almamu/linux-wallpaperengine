#ifndef WALLENGINE_OBJECT3D_H
#define WALLENGINE_OBJECT3D_H

#include <wallpaperengine/video/node.h>

#include <wallpaperengine/scene.h>

namespace wp
{
    class object3d : wp::video::node
    {
    public:
        enum Type
        {
            Type_Material = 0,
            Type_Model = 1,
            Type_Particle = 2,
            Type_Sound = 3,
            Type_None = 4
        };

        object3d (Type type, wp::object* parent);

        virtual void render ();

        template <class T> T* as();
        template <class T> bool is();

    protected:
        wp::object* m_parent;

    private:
        Type m_type;
    };
};

#endif //WALLENGINE_OBJECT3D_H
