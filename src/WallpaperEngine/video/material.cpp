#include <WallpaperEngine/Irrlicht/Irrlicht.h>
#include <WallpaperEngine/video/material.h>

namespace WallpaperEngine
{
    namespace video
    {
        material::material (irr::core::vector3df origin, WallpaperEngine::scene *scene)
        {
            m_origin = origin;
            m_scene = scene;
            m_material.Wireframe = false;
            m_material.Lighting = false;

            irr::f32 xright = this->m_origin.X;
            irr::f32 xleft = -this->m_origin.X;
            irr::f32 ztop = this->m_origin.Y;
            irr::f32 zbottom = -this->m_origin.Y;
            irr::f32 z = this->m_scene->getCamera ()->getEye ().Z;

            m_vertices [0].Pos = irr::core::vector3df (xleft,   ztop,    z); // top left
            m_vertices [1].Pos = irr::core::vector3df (xright,  ztop,    z); // top right
            m_vertices [2].Pos = irr::core::vector3df (xright,  zbottom, z); // bottom right
            m_vertices [3].Pos = irr::core::vector3df (xleft,   zbottom, z); // bottom left

            m_vertices [0].TCoords = irr::core::vector2df (1.0f, 0.0f);
            m_vertices [1].TCoords = irr::core::vector2df (0.0f, 0.0f);
            m_vertices [2].TCoords = irr::core::vector2df (0.0f, 1.0f);
            m_vertices [3].TCoords = irr::core::vector2df (1.0f, 1.0f);

            m_vertices [0].Color = irr::video::SColor (255, 255, 255, 255);
            m_vertices [1].Color = irr::video::SColor (255, 255, 255, 255);
            m_vertices [2].Color = irr::video::SColor (255, 255, 255, 255);
            m_vertices [3].Color = irr::video::SColor (255, 255, 255, 255);
        }

        void material::setFlag(irr::video::E_MATERIAL_FLAG flag, bool newvalue)
        {
            this->getMaterial ().setFlag (flag, newvalue);
        }

        void material::setType(irr::video::E_MATERIAL_TYPE newType)
        {
            this->getMaterial ().MaterialType = newType;
        }

        irr::video::SMaterial& material::getMaterial ()
        {
            return this->m_material;
        }

        void material::render ()
        {
            uint16_t indices[] =
            {
                    0, 1, 2, 3
            };

            WallpaperEngine::Irrlicht::driver->setMaterial (m_material);
            WallpaperEngine::Irrlicht::driver->drawVertexPrimitiveList (m_vertices, 4, indices, 1, irr::video::EVT_STANDARD, irr::scene::EPT_QUADS, irr::video::EIT_16BIT);
        }
    }
}