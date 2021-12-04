#include "CImage.h"

#include <utility>
#include "WallpaperEngine/Core/Objects/Images/CMaterial.h"

#include "WallpaperEngine/FileSystem/FileSystem.h"

using namespace WallpaperEngine::Core::Objects;

CImage::CImage (
        Images::CMaterial* material,
        bool visible,
        uint32_t id,
        std::string name,
        const glm::vec3& origin,
        const glm::vec3& scale,
        const glm::vec3& angles,
        const glm::vec2& size,
        std::string alignment,
        const glm::vec3& color,
        float alpha,
        float brightness) :
        CObject (visible, id, std::move(name), Type, origin, scale, angles),
        m_size (size),
        m_material (material),
        m_alignment (std::move(alignment)),
        m_color (color),
        m_alpha (alpha),
        m_brightness (brightness)
{
}

WallpaperEngine::Core::CObject* CImage::fromJSON (
    json data,
    CContainer* container,
    bool visible,
    uint32_t id,
    std::string name,
    const glm::vec3& origin,
    const glm::vec3& scale,
    const glm::vec3& angles)
{
    auto image_it = data.find ("image");
    auto size_val = jsonFindDefault <std::string> (data, "size", "0.0 0.0"); // this one might need some adjustment
    auto alignment = jsonFindDefault <std::string> (data, "alignment", "center");
    auto alpha = jsonFindDefault <float> (data, "alpha", 1.0);
    auto color_val = jsonFindDefault <std::string> (data, "color", "1.0 1.0 1.0");
    auto brightness_val = jsonFindDefault <float> (data, "brightness", 1.0);

    json content = json::parse (WallpaperEngine::FileSystem::loadFullFile ((*image_it).get <std::string> (), container));

    auto material_it = jsonFindRequired (content, "material", "Image must have a material");

    return new CImage (
        Images::CMaterial::fromFile ((*material_it).get <std::string> (), container),
        visible,
        id,
        name,
        origin,
        scale,
        angles,
        WallpaperEngine::Core::aToVector2 (size_val),
        alignment,
        WallpaperEngine::Core::aToVector3 (color_val),
        alpha,
        brightness_val
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
    return this->m_alpha;
}

const glm::vec3& CImage::getColor () const
{
    return this->m_color;
}

const float CImage::getBrightness () const
{
    return this->m_brightness;
}

const std::string CImage::Type = "image";