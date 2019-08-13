#pragma once

#include <nlohmann/json.hpp>

namespace wp::core::objects::images::materials
{
    using json = nlohmann::json;

    class passes
    {
    public:
        static passes* fromJSON (json data);

        std::vector<std::string>* getTextures ();
    protected:
        passes (std::string blending, std::string cullmode, std::string depthtest, std::string depthwrite, std::string shader);

        void insertTexture (std::string texture);
    private:
        std::string m_blending;
        std::string m_cullmode;
        std::string m_depthtest;
        std::string m_depthwrite;
        std::string m_shader;
        std::vector<std::string> m_textures;
    };
}
