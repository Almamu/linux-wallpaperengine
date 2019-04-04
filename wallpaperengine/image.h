#ifndef WALLENGINE_MODEL_H
#define WALLENGINE_MODEL_H

#include <nlohmann/json.hpp>
#include <irrlicht/irrlicht.h>

#include <wallpaperengine/fs/utils.h>
#include <wallpaperengine/texture.h>

namespace wp
{
    using json = nlohmann::json;

    class image : public object3d
    {
    public:
        image(json json_data, wp::scene* scene);
        irr::video::SMaterial& getMaterial ();
        void setFlag(irr::video::E_MATERIAL_FLAG flag, bool newvalue);
        void setType(irr::video::E_MATERIAL_TYPE newType);
        void render ();

    private:
        bool m_visible;

        irr::video::S3DVertex m_vertices [4];
        irr::video::SMaterial m_material;

        irr::core::vector3df m_origin;
        irr::io::path m_file;
        std::string m_content;

        std::vector<wp::texture*> m_textures;
    };
};

#endif //WALLENGINE_MODEL_H
