#pragma once

#include "WallpaperEngine/Core/Core.h"
#include "WallpaperEngine/Core/CObject.h"
#include "WallpaperEngine/Core/UserSettings/CUserSettingBoolean.h"

namespace WallpaperEngine::Core::Objects
{
    using json = nlohmann::json;
    using namespace WallpaperEngine::Core::UserSettings;

    /**
     * Represents a sound played while the background is working
     */
    class CSound : public CObject
    {
        friend class CObject;

    public:
        static CObject* fromJSON (
            CScene* scene,
            json data,
            CUserSettingBoolean* visible,
            uint32_t id,
            const std::string& name,
            CUserSettingVector3* origin,
            CUserSettingVector3* scale,
            const glm::vec3& angles
        );

        /**
         * @return The list of sounds to play
         */
        [[nodiscard]] const std::vector<std::string>& getSounds () const;
        /**
         * @return If the sound should repeat or not
         */
        [[nodiscard]] bool isRepeat () const;

    protected:
        CSound (
            CScene* scene,
            CUserSettingBoolean* visible,
            uint32_t id,
            std::string name,
            CUserSettingVector3* origin,
            CUserSettingVector3* scale,
            const glm::vec3& angles,
            bool repeat
        );

        /**
         * @param filename The sound to add
         */
        void insertSound (const std::string& filename);

        /**
         * Type value used to differentiate the different types of objects in a background
         */
        static const std::string Type;

    private:
        /** If the sounds should repeat or not */
        bool m_repeat;
        /** The list of sounds to play */
        std::vector<std::string> m_sounds;
    };
}
