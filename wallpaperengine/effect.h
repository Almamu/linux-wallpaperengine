#ifndef WALLENGINE_EFFECT_H
#define WALLENGINE_EFFECT_H

#include <nlohmann/json.hpp>
#include <irrlicht/irrlicht.h>

#include "texture.h"

namespace wp
{
    using json = nlohmann::json;

    class effect : public irr::video::IShaderConstantSetCallBack
    {
    public:
        effect (json json_data);

        virtual void OnSetConstants (irr::video::IMaterialRendererServices* services, int32_t userData);

        std::vector <wp::texture*>& getTextureList ();
        irr::s32 getMaterialType ();
    private:
        std::vector <wp::texture*> m_textures;
        std::string m_content;
        json m_json;
        irr::io::path m_file;
        irr::s32 m_materialType;
        std::vector<void*> m_passes;
    };
}


#endif //WALLENGINE_EFFECT_H
