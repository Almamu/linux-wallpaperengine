#include "CImage.h"

#include <utility>

#include "WallpaperEngine/Core/Objects/Images/CMaterial.h"
#include "WallpaperEngine/Core/UserSettings/CUserSettingBoolean.h"
#include "WallpaperEngine/Core/UserSettings/CUserSettingFloat.h"
#include "WallpaperEngine/Core/UserSettings/CUserSettingVector3.h"
#include "WallpaperEngine/Core/Wallpapers/CScene.h"

using namespace WallpaperEngine::Core::Objects;
using namespace WallpaperEngine::Core::UserSettings;

CImage::CImage (
    const Wallpapers::CScene* scene, const Images::CMaterial* material, const CUserSettingBoolean* visible, int id,
    std::string name, const CUserSettingVector3* origin, const CUserSettingVector3* scale, glm::vec3 angles,
    glm::vec2 size, std::string alignment, const CUserSettingVector3* color, const CUserSettingFloat* alpha,
    float brightness, uint32_t colorBlendMode, glm::vec2 parallaxDepth, bool fullscreen, bool passthrough,
    bool autosize, std::vector<const Objects::CEffect*> effects, std::vector<int> dependencies
) :
    CObject (scene, visible, id, std::move(name), Type, origin, scale, angles, std::move(dependencies)),
    m_size (size),
    m_parallaxDepth (parallaxDepth),
    m_material (material),
    m_alignment (std::move(alignment)),
    m_alpha (alpha),
    m_brightness (brightness),
    m_color (color),
    m_colorBlendMode (colorBlendMode),
    m_fullscreen (fullscreen),
    m_passthrough (passthrough),
    m_autosize (autosize),
    m_effects (std::move(effects)) {}

const WallpaperEngine::Core::CObject* CImage::fromJSON (
    const Wallpapers::CScene* scene, const json& data, const CContainer* container,
    const CUserSettingBoolean* visible, int id, std::string name, const CUserSettingVector3* origin,
    const CUserSettingVector3* scale, glm::vec3 angles, const json::const_iterator& effects_it,
    std::vector<int> dependencies
) {
    const auto image = jsonFindRequired <std::string>(data, "image", "Image must have an image");
    std::vector<const Objects::CEffect*> effects;
    json content = json::parse (container->readFileAsString (image));

    const auto material = Images::CMaterial::fromFile (
        jsonFindRequired<std::string> (content, "material", "Image must have a material"),
        container
    );

    if (effects_it != data.end () && effects_it->is_array ()) {
        for (auto& cur : *effects_it) {
            const auto effectVisible = jsonFindUserConfig<CUserSettingBoolean> (cur, "visible", true);

            if (!effectVisible->processValue (scene->getProject ().getProperties ()))
                continue;

            effects.push_back (
                Objects::CEffect::fromJSON (
                    cur, effectVisible, scene->getProject (), material, container
                )
            );
        }
    }

    return new CImage (
        scene,
        material,
        visible,
        id,
        std::move(name),
        origin,
        scale,
        angles,
        jsonFindDefault<glm::vec2> (data, "size", glm::vec2 (0.0, 0.0)),
        jsonFindDefault<std::string> (data, "alignment", "center"),
        jsonFindUserConfig<CUserSettingVector3> (data, "color", {1, 1, 1}),
        jsonFindUserConfig<CUserSettingFloat> (data, "alpha", 1.0),
        jsonFindDefault<float> (data, "brightness", 1.0),
        jsonFindDefault<uint32_t> (data, "colorBlendMode", 0),
        jsonFindDefault<glm::vec2> (data, "parallaxDepth", glm::vec2 (0.0, 0.0)),
        jsonFindDefault<bool> (content, "fullscreen", false),
        jsonFindDefault<bool> (content, "passthrough", false),
        jsonFindDefault<bool> (content, "autosize", false),
        effects,
        std::move(dependencies)
    );
}

const Images::CMaterial* CImage::getMaterial () const {
    return this->m_material;
}

const glm::vec2& CImage::getSize () const {
    return this->m_size;
}

const std::string& CImage::getAlignment () const {
    return this->m_alignment;
}

float CImage::getAlpha () const {
    return this->m_alpha->processValue (this->getScene ()->getProject ().getProperties ());
}

const glm::vec3& CImage::getColor () const {
    return this->m_color->processValue (this->getScene ()->getProject ().getProperties ());
}

float CImage::getBrightness () const {
    return this->m_brightness;
}

uint32_t CImage::getColorBlendMode () const {
    return this->m_colorBlendMode;
}

const glm::vec2& CImage::getParallaxDepth () const {
    return this->m_parallaxDepth;
}

bool CImage::isFullscreen () const {
    return this->m_fullscreen;
}

bool CImage::isPassthrough () const {
    return this->m_passthrough;
}

bool CImage::isAutosize () const {
    return this->m_autosize;
}

const std::vector<const CEffect*>& CImage::getEffects () const {
    return this->m_effects;
}

const std::string CImage::Type = "image";