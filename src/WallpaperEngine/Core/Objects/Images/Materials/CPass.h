#pragma once

#include "WallpaperEngine/Core/Core.h"

#include "WallpaperEngine/Core/Objects/Effects/Constants/CShaderConstant.h"

namespace WallpaperEngine::Core::Objects
{
    class CEffect;
}

namespace WallpaperEngine::Core::Objects::Images::Materials
{
    using json = nlohmann::json;

    /**
     * Represents a shader pass of an object
     */
    class CPass
    {
        friend class WallpaperEngine::Core::Objects::CEffect;
    public:
        static CPass* fromJSON (json data);

        /**
         * @return The list of textures to bind while rendering
         */
        [[nodiscard]] const std::vector<std::string>& getTextures () const;
        /**
         * @return Shader constants that alter how the shader should behave
         */
        [[nodiscard]] const std::map<std::string, Effects::Constants::CShaderConstant*>& getConstants () const;
        /**
         * @return Shader combos that alter how the shader should behave
         */
        [[nodiscard]] std::map<std::string, int>* getCombos ();
        /**
         * @return Shader to be used while rendering the pass
         */
        [[nodiscard]] const std::string& getShader () const;
        /**
         * @return The blending mode to use while rendering
         */
        [[nodiscard]] const std::string& getBlendingMode () const;
        /**
         * @return The culling mode to use while rendering
         */
        [[nodiscard]] const std::string& getCullingMode () const;
        /**
         * @return If depth testing has to happen while rendering
         */
        [[nodiscard]] const std::string& getDepthTest () const;
        /**
         * @return If depth write has to happen while rendering
         */
        [[nodiscard]] const std::string& getDepthWrite () const;
        /**
         * @param mode The new blending mode to use
         */
        void setBlendingMode (const std::string& mode);

        /**
         * Add a shader combo value to the list
         *
         * @param name The combo name
         * @param value It's value
         */
        void insertCombo (const std::string& name, int value);
        /**
         * Adds a shader constant to the list
         *
         * @param name The constant's name
         * @param constant It's value
         */
        void insertConstant (const std::string& name, Effects::Constants::CShaderConstant* constant);

    protected:
        CPass (std::string blending, std::string cullmode, std::string depthtest, std::string depthwrite, std::string shader);

        /**
         * Adds a new texture to the list of textures to bind while rendering
         *
         * @param texture
         */
        void insertTexture (const std::string& texture);
        /**
         * Updates a texture in the specified index for binding while rendering
         *
         * @param index
         * @param texture
         */
        void setTexture (int index, const std::string& texture);

    private:
        // TODO: CREATE ENUMERATIONS FOR THESE INSTEAD OF USING STRING VALUES!
        /** The blending mode to use */
        std::string m_blending;
        /** The culling mode to use */
        std::string m_cullmode;
        /** If depthtesting has to happen while drawing */
        std::string m_depthtest;
        /** If depthwrite has to happen while drawing */
        std::string m_depthwrite;
        /** The shader to use */
        std::string m_shader;
        /** The list of textures to use */
        std::vector<std::string> m_textures;
        /** Different combo settings for shader input */
        std::map<std::string, int> m_combos;
        /** Shader constant values to use for  the shaders */
        std::map<std::string, Core::Objects::Effects::Constants::CShaderConstant*> m_constants;
    };
}
