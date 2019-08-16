#include <irrlicht/irrlicht.h>
#include <fstream>

#include <WallpaperEngine/FileSystem/utils.h>
#include <WallpaperEngine/object3d.h>
#include <WallpaperEngine/image.h>

#include <WallpaperEngine/Irrlicht/Irrlicht.h>
#include <WallpaperEngine/Core/Core.h>

namespace WallpaperEngine
{
    image::image (json json_data, WallpaperEngine::object* parent) : object3d (object3d::Type::Type_Material, parent)
    {
        this->m_parent = parent;

        json::const_iterator visible_it = json_data.find ("visible");
        json::const_iterator file_it = json_data.find ("image");
        json::const_iterator origin_it = json_data.find ("origin");

        if (visible_it != json_data.end () && (*visible_it).is_boolean () == true)
        {
            this->m_visible = *visible_it;
        }

        // basic texture, main thing to assign first
        if (file_it != json_data.end () && (*file_it).is_string () == true)
        {
            this->m_file = (*file_it).get <std::string> ().c_str ();
            this->m_content = WallpaperEngine::FileSystem::loadFullFile (this->m_file);

            json content = json::parse (this->m_content);
            json::const_iterator it = content.find ("material");

            if (it != content.end () && (*it).is_string () == true)
            {
                irr::io::path materialfile = (*it).get <std::string> ().c_str ();
                std::string texturejson_content = WallpaperEngine::FileSystem::loadFullFile (materialfile);
                json materialcontent = json::parse (texturejson_content);

                // now try to read the texture if any
                json::const_iterator it = materialcontent.find ("passes");

                if (it != materialcontent.end () && (*it).is_array() == true)
                {
                    json::const_iterator passesCur = (*it).begin ();
                    json::const_iterator passesEnd = (*it).end ();

                    for (; passesCur != passesEnd; passesCur ++)
                    {
                        json::const_iterator texture = (*passesCur).find ("textures");

                        if (texture != (*passesCur).end () && (*texture).is_array () == true)
                        {
                            json::const_iterator texturesCur = (*texture).begin ();
                            json::const_iterator texturesEnd = (*texture).end ();

                            for (; texturesCur != texturesEnd; texturesCur ++)
                            {
                                irr::io::path texturepath = ("materials/" + (*texturesCur).get <std::string> () + ".tex").c_str ();

                                this->m_textures.push_back (new WallpaperEngine::texture (texturepath));
                            }
                        }
                    }
                }
            }
        }

        // initialize actual material properties
        irr::f32 xright = this->m_parent->getOrigin ().X;
        irr::f32 xleft = -this->m_parent->getOrigin ().X;
        irr::f32 ztop = this->m_parent->getOrigin ().Y;
        irr::f32 zbottom = -this->m_parent->getOrigin ().Y;
        irr::f32 z = this->m_parent->getScene ()->getCamera ()->getEye ().Z;

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

        // once textures are defined, prepare them to be displayed
        std::vector<WallpaperEngine::texture*>::const_iterator cur = this->m_textures.begin ();
        std::vector<WallpaperEngine::texture*>::const_iterator end = this->m_textures.end ();

        for (irr::u32 i = 0; cur != end; cur ++, i ++)
        {
            this->getMaterial ().setTexture (i, (*cur)->getIrrTexture ());
        }

        std::vector<WallpaperEngine::effect*>::const_iterator effect = this->m_parent->getEffects ().begin ();

        if (effect != this->m_parent->getEffects ().end ())
        {
            // loop through the textures the effect is using
            std::vector<WallpaperEngine::texture*> list = (*effect)->getTextureList ();

            std::vector<WallpaperEngine::texture*>::const_iterator curtex = list.begin ();
            std::vector<WallpaperEngine::texture*>::const_iterator endtex = list.end ();

            for (int current = 0; curtex != endtex; curtex ++, current ++)
            {
                if ((*curtex) == nullptr)
                    continue;

                this->getMaterial ().setTexture (current, (*curtex)->getIrrTexture ());
            }

            this->setType ((irr::video::E_MATERIAL_TYPE) (*effect)->getMaterialType ());
        }
        else
        {
            this->setType (irr::video::EMT_TRANSPARENT_ALPHA_CHANNEL);
        }

        // set basic material flags
        this->setFlag (irr::video::EMF_LIGHTING, false);
        this->setFlag (irr::video::EMF_BLEND_OPERATION, true);
    }


    void image::setFlag(irr::video::E_MATERIAL_FLAG flag, bool newvalue)
    {
        this->getMaterial ().setFlag (flag, newvalue);
    }

    void image::setType(irr::video::E_MATERIAL_TYPE newType)
    {
        this->getMaterial ().MaterialType = newType;
    }

    irr::video::SMaterial& image::getMaterial ()
    {
        return this->m_material;
    }

    void image::render ()
    {
        if (this->m_visible == false) return;

        uint16_t indices[] =
        {
                0, 1, 2, 3
        };

        WallpaperEngine::Irrlicht::driver->setMaterial (this->getMaterial ());
        WallpaperEngine::Irrlicht::driver->drawVertexPrimitiveList (this->m_vertices, 4, indices, 1, irr::video::EVT_STANDARD, irr::scene::EPT_QUADS, irr::video::EIT_16BIT);
    }

}