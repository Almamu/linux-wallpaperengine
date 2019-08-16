#ifndef WALLENGINE_MODEL_H
#define WALLENGINE_MODEL_H

#include <nlohmann/json.hpp>
#include <irrlicht/irrlicht.h>

#include <WallpaperEngine/FileSystem/utils.h>
#include <WallpaperEngine/texture.h>

namespace WallpaperEngine
{
    using json = nlohmann::json;

    class image : public object3d
    {
    public:
        image(json json_data, WallpaperEngine::object* parent);
        irr::video::SMaterial& getMaterial ();
        void setFlag(irr::video::E_MATERIAL_FLAG flag, bool newvalue);
        void setType(irr::video::E_MATERIAL_TYPE newType);
        void render ();

    private:
        bool m_visible;

        irr::video::S3DVertex m_vertices [4];
        irr::video::SMaterial m_material;

        irr::io::path m_file;
        std::string m_content;

        std::vector<WallpaperEngine::texture*> m_textures;
    };
};

#endif //WALLENGINE_MODEL_H
