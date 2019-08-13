#include "image.h"
#include "images/material.h"

#include "../../core.h"
#include "../../fs/utils.h"

using namespace wp::core::objects;

image::image (
        images::material* material,
        bool visible,
        irr::u32 id,
        std::string name,
        const irr::core::vector3df& origin,
        const irr::core::vector3df& scale,
        const irr::core::vector3df& angles,
        const irr::core::vector2df& size) :
    object (visible, id, std::move(name), origin, scale, angles),
    m_size (size)
{

}

wp::core::object* image::fromJSON (
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

    json content = json::parse (wp::fs::utils::loadFullFile ((*image_it).get <std::string> ().c_str ()));

    json::const_iterator material_it = content.find ("material");

    if (material_it == content.end ())
    {
        throw std::runtime_error ("Image must have a material");
    }

    return new image (
        images::material::fromFile ((*material_it).get <std::string> ().c_str ()),
        visible,
        id,
        name,
        origin,
        scale,
        angles,
        wp::core::ato2vf (*size_it)
    );
}