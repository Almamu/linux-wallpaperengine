#pragma once

#include "WallpaperEngine/Core/Objects/Images/Materials/CPass.h"
#include "WallpaperEngine/Core/Objects/Effects/CBind.h"

#include "WallpaperEngine/Core/Core.h"
#include "WallpaperEngine/Assets/CContainer.h"

namespace WallpaperEngine::Core::Objects::Images
{
    using json = nlohmann::json;
    using namespace WallpaperEngine::Assets;

    /**
     * Represents a material in use in the background
     */
    class CMaterial
    {
    public:
        static CMaterial* fromFile (const std::string& filename, Assets::CContainer* container);
        static CMaterial* fromJSON (const std::string& name, json data);
        static CMaterial* fromFile (const std::string& filename, const std::string& target, Assets::CContainer* container);
        static CMaterial* fromJSON (const std::string& name, json data, const std::string& target);

        /**
         * @param pass The rendering pass to add to the material
         */
        void insertPass (Materials::CPass* pass);
        /**
         * @param bind Texture bind override for the material
         */
        void insertTextureBind (Effects::CBind* bind);

        /**
         * @return All the rendering passes that happen for this material
         */
        [[nodiscard]] const std::vector <Materials::CPass*>& getPasses () const;
        /**
         * @return The textures that have to be bound while rendering the material.
         * 		   These act as an override of the textures specified by the parent effect
         */
        [[nodiscard]] const std::map <int, Effects::CBind*>& getTextureBinds () const;
        /**
         * @return The materials destination (fbo) if required
         */
        [[nodiscard]] const std::string& getTarget () const;
        /**
         * @return Indicates if this material has a specific destination (fbo) while rendering
         */
        [[nodiscard]] bool hasTarget () const;
        /**
         * @return The name of the material
         */
        [[nodiscard]] const std::string& getName () const;

    protected:
        explicit CMaterial (std::string  name);

        /**
         * @param target The new target while rendering this material
         */
        void setTarget (const std::string& target);

    private:
        /** All the shader passes required to render this material */
        std::vector <Materials::CPass*> m_passes;
        /** List of texture bind overrides to use for this material */
        std::map <int, Effects::CBind*> m_textureBindings;
        /** The FBO target to render to (if any) */
        std::string m_target;
        /** The material's name */
        std::string m_name;
    };
}
