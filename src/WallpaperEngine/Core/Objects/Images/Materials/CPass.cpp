#include "CPass.h"
#include "WallpaperEngine/Logging/CLog.h"

#include <utility>

using namespace WallpaperEngine::Core::Objects::Effects::Constants;
using namespace WallpaperEngine::Core::Objects::Images::Materials;

CPass::CPass (std::string blending, std::string cullmode, std::string depthtest, std::string depthwrite,
              std::string shader, std::map<int, std::string> textures, std::map<std::string, int> combos,
              std::map<std::string, const Core::Objects::Effects::Constants::CShaderConstant*> constants) :
    m_blending (std::move(blending)),
    m_cullmode (std::move(cullmode)),
    m_depthtest (std::move(depthtest)),
    m_depthwrite (std::move(depthwrite)),
    m_shader (std::move(shader)),
    m_textures (std::move(textures)),
    m_combos (std::move(combos)),
    m_constants (std::move(constants))
{}

const CPass* CPass::fromJSON (const json& data, const CMaterial::OverrideInfo* overrides) {
    // TODO: FIGURE OUT DEFAULT BLENDING MODE
    const auto textures_it = data.find ("textures");
    const auto combos_it = data.find ("combos");

    std::map<int, std::string> textures;
    std::map<std::string, int> combos;
    std::map<std::string, const Core::Objects::Effects::Constants::CShaderConstant*> constants;

    if (textures_it != data.end ()) {
        // TODO: FETCH THIS FROM CImage TO MAKE IT COMPATIBLE WITH OLDER WALLPAPERS
        if (!textures_it->is_array ())
            sLog.exception ("Material's textures must be a list");

        int textureNumber = -1;
        for (const auto& cur : (*textures_it))
            textures.emplace (++textureNumber, cur.is_null () ? "" : cur);
    }

    if (combos_it != data.end ()) {
        for (const auto& cur : combos_it->items ()) {
            if (cur.value ().is_number_integer ()) {
                std::string uppercase = std::string (cur.key ());

                std::transform (uppercase.begin (), uppercase.end (), uppercase.begin (), ::toupper);
                combos.emplace (uppercase, cur.value ());
            } else {
                sLog.exception ("unexpected non-integer combo on pass");
            }
        }
    }

    // apply overrides
    if (overrides != nullptr) {
        for (const auto& [name, value] : overrides->combos)
            combos[name] = value;
        for (const auto& [name, value] : overrides->constants)
            constants[name] = value;
        for (const auto& [id, value] : overrides->textures)
            textures[id] = value;
    }

    return new CPass (
        jsonFindDefault<std::string> (data, "blending", "normal"),
        jsonFindDefault<std::string> (data, "cullmode", "nocull"),
        jsonFindRequired <std::string> (data, "depthtest", "Material pass must have depthtest specified"),
        jsonFindRequired <std::string> (data, "depthwrite", "Material pass must have depthwrite specified"),
        jsonFindRequired <std::string> (data, "shader", "Material pass must have shader specified"),
        textures,
        combos,
        constants
    );
}

const std::map<int, std::string>& CPass::getTextures () const {
    return this->m_textures;
}

const std::map<std::string, const CShaderConstant*>& CPass::getConstants () const {
    return this->m_constants;
}

const std::map<std::string, int>& CPass::getCombos () const {
    return this->m_combos;
}

const std::string& CPass::getShader () const {
    return this->m_shader;
}

const std::string& CPass::getBlendingMode () const {
    return this->m_blending;
}

const std::string& CPass::getCullingMode () const {
    return this->m_cullmode;
}

const std::string& CPass::getDepthTest () const {
    return this->m_depthtest;
}

const std::string& CPass::getDepthWrite () const {
    return this->m_depthwrite;
}
