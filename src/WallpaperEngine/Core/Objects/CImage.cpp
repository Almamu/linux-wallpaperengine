#include "CImage.h"

#include <utility>
#include "WallpaperEngine/Core/Objects/Images/CMaterial.h"

#include "WallpaperEngine/FileSystem/FileSystem.h"

using namespace WallpaperEngine::Core::Objects;

CImage::CImage (
        Images::CMaterial* material,
        bool visible,
        irr::u32 id,
        std::string name,
        const glm::vec3& origin,
        const glm::vec3& scale,
        const glm::vec3& angles,
        const glm::vec2& size,
        std::string alignment) :
        CObject (visible, id, std::move(name), Type, origin, scale, angles),
        m_size (size),
        m_material (material),
        m_alignment (std::move(alignment))
{
}

WallpaperEngine::Core::CObject* CImage::fromJSON (
    json data,
    CContainer* container,
    bool visible,
    irr::u32 id,
    std::string name,
    const glm::vec3& origin,
    const glm::vec3& scale,
    const glm::vec3& angles)
{
    auto image_it = data.find ("image");
    auto size_it = jsonFindRequired (data, "size", "Images must have size");
    auto alignment = jsonFindDefault <std::string> (data, "alignment", "center");

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
        WallpaperEngine::Core::aToVector2 (*size_it),
        alignment
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


const std::string CImage::Type = "image";