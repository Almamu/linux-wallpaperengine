#ifndef WALLENGINE_EFFECT_H
#define WALLENGINE_EFFECT_H

#include <nlohmann/json.hpp>
#include <irrlicht/irrlicht.h>

#include "texture.h"
#include "shaders/compiler.h"

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
        enum ParameterType
        {
            TYPE_VEC4,
            TYPE_VEC3,
            TYPE_VEC2,
            TYPE_FLOAT,
            TYPE_INT
        };

        struct ShaderParameter
        {
            void* value;
            ParameterType type;
        };

        void parseConstantValues (json data);
        void parseCombos (json data);

        wp::shaders::compiler* m_fragShader;
        wp::shaders::compiler* m_vertShader;

        std::map <std::string, ShaderParameter*> m_vertexVariables;
        std::map <std::string, ShaderParameter*> m_pixelVariables;
        std::map <std::string, int> m_combos;

        std::map <std::string, void*> m_constants;
        std::vector <wp::texture*> m_textures;
        std::string m_content;
        json m_json;
        irr::io::path m_file;
        irr::s32 m_materialType;
        std::vector<void*> m_passes;
    };
}


#endif //WALLENGINE_EFFECT_H
