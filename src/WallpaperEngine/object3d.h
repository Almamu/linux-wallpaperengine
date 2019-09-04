#ifndef WALLENGINE_OBJECT3D_H
#define WALLENGINE_OBJECT3D_H

#include <WallpaperEngine/video/node.h>

#include <WallpaperEngine/scene.h>

namespace WallpaperEngine
{
    class object3d : WallpaperEngine::video::node
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

        object3d (Type type, WallpaperEngine::object* parent);

        virtual void render ();

        template <class T> T* as();
        template <class T> bool is();

    protected:
        WallpaperEngine::object* m_parent;

    private:
        Type m_type;
    };
};

#endif //WALLENGINE_OBJECT3D_H
