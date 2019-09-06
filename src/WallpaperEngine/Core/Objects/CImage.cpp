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
    json::const_iterator image_it = data.find ("image");
    json::const_iterator size_it = data.find ("size");

    if (size_it == data.end ())
    {
        throw std::runtime_error ("Images must have size");
    }

    json content = json::parse (WallpaperEngine::FileSystem::loadFullFile ((*image_it).get <std::string> ().c_str ()));

    json::const_iterator material_it = content.find ("material");

    if (material_it == content.end ())
    {
        throw std::runtime_error ("Image must have a material");
    }

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

Images::CMaterial* CImage::getMaterial ()
{
    return this->m_material;
}

const std::string CImage::Type = "image";