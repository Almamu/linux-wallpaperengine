#include "CImage.h"

#include "WallpaperEngine/Core/CScene.h"
#include "WallpaperEngine/Core/Objects/Images/CMaterial.h"
#include "WallpaperEngine/Core/UserSettings/CUserSettingBoolean.h"
#include "WallpaperEngine/Core/UserSettings/CUserSettingFloat.h"
#include "WallpaperEngine/Core/UserSettings/CUserSettingVector3.h"
#include <utility>

#include "WallpaperEngine/FileSystem/FileSystem.h"

using namespace WallpaperEngine::Core::Objects;
using namespace WallpaperEngine::Core::UserSettings;

CImage::CImage (
    CScene* scene,
    Images::CMaterial* material,
    CUserSettingBoolean* visible,
    uint32_t id,
    std::string name,
    CUserSettingVector3* origin,
    CUserSettingVector3* scale,
    const glm::vec3& angles,
    const glm::vec2& size,
    std::string alignment,
                CUserSettingVector3* color,
    CUserSettingFloat* alpha,
    float brightness,
    uint32_t colorBlendMode,
    const glm::vec2& parallaxDepth
) :
    CObject (scene, visible, id, std::move(name), Type, origin, scale, angles),
    m_size (size),
    m_material (material),
    m_alignment (std::move(alignment)),
    m_color (color),
    m_alpha (alpha),
    m_brightness (brightness),
    m_colorBlendMode (colorBlendMode),
    m_parallaxDepth(parallaxDepth)
{
}

WallpaperEngine::Core::CObject* CImage::fromJSON (
    CScene* scene,
    json data,
    const CContainer* container,
    CUserSettingBoolean* visible,
    uint32_t id,
    std::string name,
    CUserSettingVector3* origin,
    CUserSettingVector3* scale,
    const glm::vec3& angles)
{
    auto image_it = data.find ("image");
    auto size_val = jsonFindDefault <std::string> (data, "size", "0.0 0.0"); // this one might need some adjustment
    auto alignment = jsonFindDefault <std::string> (data, "alignment", "center");
    auto alpha = jsonFindUserConfig <CUserSettingFloat, double> (data, "alpha", 1.0);
    auto color = jsonFindUserConfig <CUserSettingVector3, glm::vec3> (data, "color", {1, 1, 1});
    auto brightness_val = jsonFindDefault <float> (data, "brightness", 1.0);
    auto colorBlendMode_val = jsonFindDefault <uint32_t> (data, "colorBlendMode", 0);
    auto parallaxDepth_val = jsonFindDefault <std::string> (data, "parallaxDepth", "0 0");

    json content = json::parse (WallpaperEngine::FileSystem::loadFullFile ((*image_it).get <std::string> (), container));

    auto material_it = jsonFindRequired (content, "material", "Image must have a material");

    return new CImage (
        scene,
        Images::CMaterial::fromFile ((*material_it).get <std::string> (), container),
        visible,
        id,
        name,
        origin,
        scale,
        angles,
        WallpaperEngine::Core::aToVector2 (size_val),
        alignment,
        color,
        alpha,
        brightness_val,
        colorBlendMode_val,
        WallpaperEngine::Core::aToVector2 (parallaxDepth_val)
    );
}

const Images::CMaterial* CImage::getMaterial () const
{
    return this->m_material;
}

const glm::vec2& CImage::getSize () const
{
    return this->m_size;
}

const std::string& CImage::getAlignment () const
{
    return this->m_alignment;
}

const float CImage::getAlpha () const
{
    return this->m_alpha->processValue (this->getScene ()->getProject ()->getProperties ());
}

glm::vec3 CImage::getColor () const
{
    return this->m_color->processValue (this->getScene ()->getProject ()->getProperties ());
}

const float CImage::getBrightness () const
{
    return this->m_brightness;
}

const uint32_t CImage::getColorBlendMode () const
{
    return this->m_colorBlendMode;
}


const glm::vec2& CImage::getParallaxDepth () const
{
    return this->m_parallaxDepth;
}

const std::string CImage::Type = "image";