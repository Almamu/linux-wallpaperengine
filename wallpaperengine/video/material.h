#ifndef WALLENGINE_MATERIAL_H
#define WALLENGINE_MATERIAL_H


#include <irrlicht/vector3d.h>
#include <wallpaperengine/scene.h>
#include <wallpaperengine/video/node.h>

namespace wp
{
    namespace video
    {
        class material : public node
        {
        public:
            material (irr::core::vector3df origin, wp::scene* scene);
            void render ();
            irr::video::SMaterial& getMaterial ();
            void setFlag(irr::video::E_MATERIAL_FLAG flag, bool newvalue);
            void setType(irr::video::E_MATERIAL_TYPE newType);

        private:
            irr::video::S3DVertex m_vertices [4];
            irr::video::SMaterial m_material;
            irr::core::vector3df m_origin;
            wp::scene* m_scene;
        };
    }
}


#endif //WALLENGINE_MATERIAL_H
