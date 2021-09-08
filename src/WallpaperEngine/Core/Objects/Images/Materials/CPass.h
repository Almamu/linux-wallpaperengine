#pragma once

#include "WallpaperEngine/Core/Core.h"

#include "WallpaperEngine/Core/Objects/Effects/Constants/CShaderConstant.h"

namespace WallpaperEngine::Core::Objects
{
    class CEffect;
};

namespace WallpaperEngine::Core::Objects::Images::Materials
{
    using json = nlohmann::json;

    class CPass
    {
        friend class Core::Objects::CEffect;
    public:
        static CPass* fromJSON (json data);

        const std::vector<std::string>& getTextures () const;
        const std::map<std::string, Effects::Constants::CShaderConstant*>& getConstants () const;

        std::map<std::string, int>* getCombos ();
        const std::string& getShader () const;
        const std::string& getBlendingMode () const;
        const std::string& getCullingMode () const;
        const std::string& getDepthTest () const;
        const std::string& getDepthWrite () const;

        void insertCombo (const std::string& name, int value);
        void insertConstant (const std::string& name, Effects::Constants::CShaderConstant* constant);

    protected:
        CPass (std::string blending, std::string cullmode, std::string depthtest, std::string depthwrite, std::string shader);

        void insertTexture (const std::string& texture);
        void setTexture (int index, const std::string& texture);

    private:
        std::string m_blending;
        std::string m_cullmode;
        std::string m_depthtest;
        std::string m_depthwrite;
        std::string m_shader;
        std::vector<std::string> m_textures;
        std::map<std::string, int> m_combos;
        std::map<std::string, Core::Objects::Effects::Constants::CShaderConstant*> m_constants;
    };
}
