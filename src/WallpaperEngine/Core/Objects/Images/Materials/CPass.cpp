#include "CPass.h"

using namespace WallpaperEngine::Core::Objects::Effects::Constants;
using namespace WallpaperEngine::Core::Objects::Images::Materials;

CPass::CPass (std::string blending, std::string cullmode, std::string depthtest, std::string depthwrite, std::string shader) :
    m_blending (std::move(blending)),
    m_cullmode (std::move(cullmode)),
    m_depthtest (std::move(depthtest)),
    m_depthwrite (std::move(depthwrite)),
    m_shader (std::move(shader))
{
}

CPass* CPass::fromJSON (json data)
{
    auto blending_it = jsonFindRequired (data, "blending", "Material pass must have blending specified");
    auto cullmode_it = jsonFindRequired (data, "cullmode", "Material pass must have cullmode specified");
    auto depthtest_it = jsonFindRequired (data, "depthtest", "Material pass must have depthtest specified");
    auto depthwrite_it = jsonFindRequired (data, "depthwrite", "Material pass must have depthwrite specified");
    auto shader_it = jsonFindRequired (data, "shader", "Material pass must have shader specified");
    auto textures_it = data.find ("textures");
    auto combos_it = data.find ("combos");

    if (textures_it != data.end ())
    {
        // TODO: FETCH THIS FROM CImage TO MAKE IT COMPATIBLE WITH OLDER WALLPAPERS
        if ((*textures_it).is_array () == false)
        {
            throw std::runtime_error ("Textures for material must be a list");
        }
    }

    CPass* pass = new CPass (
        *blending_it,
        *cullmode_it,
        *depthtest_it,
        *depthwrite_it,
        *shader_it
    );

    if (textures_it != data.end ())
    {
        auto cur = (*textures_it).begin ();
        auto end = (*textures_it).end ();

        for (;cur != end; cur ++)
        {
            if ((*cur).is_null () == true)
            {
                pass->insertTexture ("");
            }
            else
            {
                pass->insertTexture (*cur);
            }
        }
    }

    if (combos_it != data.end ())
    {
        auto cur = (*combos_it).begin ();
        auto end = (*combos_it).end ();

        for (; cur != end; cur ++)
        {
            std::string name = cur.key ();

            if ((*cur).is_number_integer () == true)
            {
                pass->insertCombo (name, *cur);
            }
            else
            {
                throw std::runtime_error ("unexpected non-integer combo");
            }
        }
    }

    return pass;
}

void CPass::insertTexture (const std::string& texture)
{
    this->m_textures.push_back (texture);
}

void CPass::setTexture (int index, const std::string& texture)
{
    this->m_textures.at (index) = texture;
}

void CPass::insertCombo (const std::string& name, int value)
{
    std::string uppercase = std::string (name);

    std::transform (uppercase.begin (), uppercase.end (), uppercase.begin (), ::toupper);
    this->m_combos.insert (std::pair <std::string, int> (uppercase, value));
}

const std::vector<std::string>& CPass::getTextures () const
{
    return this->m_textures;
}

const std::map<std::string, CShaderConstant*>& CPass::getConstants () const
{
    return this->m_constants;
}

std::map<std::string, int>* CPass::getCombos ()
{
    return &this->m_combos;
}

const std::string& CPass::getShader () const
{
    return this->m_shader;
}

const std::string& CPass::getBlendingMode () const
{
    return this->m_blending;
}

const std::string& CPass::getCullingMode () const
{
    return this->m_cullmode;
}

const std::string& CPass::getDepthTest () const
{
    return this->m_depthtest;
}

const std::string& CPass::getDepthWrite ()const
{
    return this->m_depthwrite;
}

void CPass::insertConstant (const std::string& name, CShaderConstant* constant)
{
    this->m_constants.insert (std::pair <std::string, CShaderConstant*> (name, constant));
}