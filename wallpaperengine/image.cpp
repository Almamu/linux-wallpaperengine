#include <irrlicht/irrlicht.h>
#include <fstream>

#include <wallpaperengine/fs/utils.h>
#include <wallpaperengine/object3d.h>
#include <wallpaperengine/image.h>

#include <wallpaperengine/irrlicht.h>
#include <wallpaperengine/core.h>

namespace wp
{
    image::image (json json_data, wp::object* parent) : object3d (object3d::Type::Type_Material, parent)
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
            this->m_content = wp::fs::utils::loadFullFile (this->m_file);

            json content = json::parse (this->m_content);
            json::const_iterator it = content.find ("material");

            if (it != content.end () && (*it).is_string () == true)
            {
                irr::io::path materialfile = (*it).get <std::string> ().c_str ();
                std::string texturejson_content = wp::fs::utils::loadFullFile (materialfile);
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

                                this->m_textures.push_back (new wp::texture (texturepath));
                            }
                        }
                    }
                }
            }
        }

        // initialize actual material properties
        irr::f32 xsize = this->m_parent->getSize ().X;
        irr::f32 ysize = this->m_parent->getSize ().Y;
        if (xsize == 0. || ysize == 0.)
        {
            xsize = 1920.;
            ysize = 1080.;
            wp::irrlicht::device->getLogger ()->log ("Initializing xsize and ysize as default values 1920 and 1080", this->m_file.c_str(), irr::ELL_INFORMATION);
        }
        irr::f32 xscale = this->m_parent->getScale ().X;
        irr::f32 yscale = this->m_parent->getScale ().Y;

        float scene_w = this->m_parent->getScene ()->getProjectionWidth ();
        float scene_h = this->m_parent->getScene ()->getProjectionHeight ();
        irr::f32 xright  = -scene_w/2. + this->m_parent->getOrigin ().X + xsize*xscale/2.;
        irr::f32 xleft   = -scene_w/2. + this->m_parent->getOrigin ().X - xsize*xscale/2.;
        irr::f32 ytop    = -scene_h/2. + this->m_parent->getOrigin ().Y + ysize*yscale/2.;
        irr::f32 ybottom = -scene_h/2. + this->m_parent->getOrigin ().Y - ysize*yscale/2.;
        irr::f32 z = this->m_parent->getScene ()->getCamera ()->getEye ().Z;

        m_vertices [0].Pos = irr::core::vector3df (xleft,   ytop,    z); // top left
        m_vertices [1].Pos = irr::core::vector3df (xright,  ytop,    z); // top right
        m_vertices [2].Pos = irr::core::vector3df (xright,  ybottom, z); // bottom right
        m_vertices [3].Pos = irr::core::vector3df (xleft,   ybottom, z); // bottom left

        m_vertices [0].TCoords = irr::core::vector2df (0.0f, 0.0f);
        m_vertices [1].TCoords = irr::core::vector2df (1.0f, 0.0f);
        m_vertices [2].TCoords = irr::core::vector2df (1.0f, 1.0f);
        m_vertices [3].TCoords = irr::core::vector2df (0.0f, 1.0f);

        m_vertices [0].Color = irr::video::SColor (255, 255, 255, 255);
        m_vertices [1].Color = irr::video::SColor (255, 255, 255, 255);
        m_vertices [2].Color = irr::video::SColor (255, 255, 255, 255);
        m_vertices [3].Color = irr::video::SColor (255, 255, 255, 255);

        // once textures are defined, prepare them to be displayed
        std::vector<wp::texture*>::const_iterator cur = this->m_textures.begin ();
        std::vector<wp::texture*>::const_iterator end = this->m_textures.end ();

        for (irr::u32 i = 0; cur != end; cur ++, i ++)
        {
            this->getMaterial ().setTexture (i, (*cur)->getIrrTexture ());
        }

        std::vector<wp::effect*>::const_iterator effect = this->m_parent->getEffects ().begin ();

        if (effect != this->m_parent->getEffects ().end ())
        {
            // loop through the textures the effect is using
            std::vector<wp::texture*> list = (*effect)->getTextureList ();

            std::vector<wp::texture*>::const_iterator curtex = list.begin ();
            std::vector<wp::texture*>::const_iterator endtex = list.end ();

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

        wp::irrlicht::driver->setMaterial (this->getMaterial ());
        wp::irrlicht::driver->drawVertexPrimitiveList (this->m_vertices, 4, indices, 1, irr::video::EVT_STANDARD, irr::scene::EPT_QUADS, irr::video::EIT_16BIT);
    }

}