#pragma once

#include "WallpaperEngine/Core/Objects/Images/CMaterial.h"

#include "WallpaperEngine/Core/CObject.h"
#include "WallpaperEngine/Core/Core.h"

#include "WallpaperEngine/Assets/CContainer.h"

#include "WallpaperEngine/Core/UserSettings/CUserSettingBoolean.h"
#include "WallpaperEngine/Core/UserSettings/CUserSettingFloat.h"
#include "WallpaperEngine/Core/UserSettings/CUserSettingVector3.h"

namespace WallpaperEngine::Core::Wallpapers {
class CScene;
}

namespace WallpaperEngine::Core::Objects {
using json = nlohmann::json;
using namespace WallpaperEngine::Assets;
using namespace WallpaperEngine::Core::UserSettings;

/**
 * Represents an image in a background
 */
class CImage : public CObject {
    friend class CObject;

  public:
    static const CObject* fromJSON (
        const Wallpapers::CScene* scene, const json& data, const CContainer* container,
        const CUserSettingBoolean* visible, int id, std::string name, const CUserSettingVector3* origin,
        const CUserSettingVector3* scale, const CUserSettingVector3* angles, const json::const_iterator& effects_it,
        std::vector<int> dependencies);

    /**
     * @return The base material to use for the image
     */
    [[nodiscard]] const Images::CMaterial* getMaterial () const;
    /**
     * @return The size of the image
     */
    [[nodiscard]] const glm::vec2& getSize () const;
    /**
     * @return The type of alignment to use for image positioning
     */
    [[nodiscard]] const std::string& getAlignment () const;
    /**
     * @return The alpha value for the image's rendering
     */
    [[nodiscard]] float getAlpha () const;
    /**
     * @return The color to use for the image
     */
    [[nodiscard]] const glm::vec3& getColor () const;
    /**
     * @return The brightness to use for the image
     */
    [[nodiscard]] float getBrightness () const;
    /**
     * @return The color blending mode to be used, special value for shaders
     */
    [[nodiscard]] uint32_t getColorBlendMode () const;
    /**
     * @return Parallax depth of the image
     */
    [[nodiscard]] const glm::vec2& getParallaxDepth () const;
    /**
     * @return If the image is fullscreen or not
     */
    [[nodiscard]] bool isFullscreen () const;
    /**
     * @return If the image is passthrough or not
     */
    [[nodiscard]] bool isPassthrough () const;
    /**
     * @return If the image is autosized or not
     */
    [[nodiscard]] bool isAutosize () const;
    /**
     * @return All of the effects applied to this image
     */
    [[nodiscard]] const std::vector<const Objects::CEffect*>& getEffects () const;

  protected:
    CImage (
        const Wallpapers::CScene* scene, const Images::CMaterial* material, const CUserSettingBoolean* visible, int id,
        std::string name, const CUserSettingVector3* origin, const CUserSettingVector3* scale,
        const CUserSettingVector3* angles, glm::vec2 size, std::string alignment, const CUserSettingVector3* color,
        const CUserSettingFloat* alpha, float brightness, uint32_t colorBlendMode, glm::vec2 parallaxDepth,
        bool fullscreen, bool passthrough, bool autosize, std::vector<const Objects::CEffect*> effects,
        std::vector<int> dependencies);

    /**
     * Type value used to differentiate the different types of objects in a background
     */
    static const std::string Type;

  private:
    /** The image's size */
    const glm::vec2 m_size;
    /** Parallax depth */
    const glm::vec2 m_parallaxDepth;
    /** Base material for the image */
    const Images::CMaterial* m_material;
    /** What type of alignment to use for the image's position */
    const std::string m_alignment;
    /** The alpha value for the image */
    const CUserSettingFloat* m_alpha;
    /** The brightness for the image */
    float m_brightness;
    /** The color to use for the image */
    const CUserSettingVector3* m_color;
    /** The color blending mode used for the image, special value for shaders */
    const uint32_t m_colorBlendMode;
    /** Override for effects */
    const std::vector<const Objects::CEffect*> m_effects;
    /** If the image is fullscreen or not */
    bool m_fullscreen;
    /** If the image is passthrough or not */
    bool m_passthrough;
    /** If the image's size should be automatically determined */
    bool m_autosize;
};
} // namespace WallpaperEngine::Core::Objects
