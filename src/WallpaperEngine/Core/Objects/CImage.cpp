#include "CImage.h"
#include "WallpaperEngine/Core/Objects/Images/CMaterial.h"

#include "WallpaperEngine/Core/Core.h"
#include "WallpaperEngine/FileSystem/FileSystem.h"

using namespace WallpaperEngine::Core::Objects;

CImage::CImage (
        Images::CMaterial* material,
        bool visible,
        irr::u32 id,
        std::string name,
        const irr::core::vector3df& origin,
        const irr::core::vector3df& scale,
        const irr::core::vector3df& angles,
        const irr::core::vector2df& size) :
        CObject (visible, id, std::move(name), Type, origin, scale, angles),
        m_size (size),
        m_material (material)
{
}

WallpaperEngine::Core::CObject* CImage::fromJSON (
    json data,
    bool visible,
    irr::u32 id,
    std::string name,
    const irr::core::vector3df& origin,
    const irr::core::vector3df& scale,
    const irr::core::vector3df& angles)
{
    auto image_it = data.find ("image");
    auto size_it = jsonFindValueRequired(&data, "size", "Images must have size");

    json content = json::parse (WallpaperEngine::FileSystem::loadFullFile ((*image_it).get <std::string> ().c_str ()));

    auto material_it = jsonFindValueRequired(&content, "material", "Image must have a material");

    return new CImage (
        Images::CMaterial::fromFile ((*material_it).get <std::string> ().c_str ()),
        visible,
        id,
        name,
        origin,
        scale,
        angles,
        WallpaperEngine::Core::ato2vf (*size_it)
    );
}

const Images::CMaterial* CImage::getMaterial () const
{
    return this->m_material;
}

const irr::core::vector2df& CImage::getSize () const
{
    return this->m_size;
}


const std::string CImage::Type = "image";