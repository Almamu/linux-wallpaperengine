#include "MaterialParser.h"

#include "WallpaperEngine/Data/Model/Material.h"
#include "WallpaperEngine/Data/Model/Project.h"
#include "WallpaperEngine/FileSystem/Container.h"

using namespace WallpaperEngine::Data::Parsers;
using namespace WallpaperEngine::Data::Model;

MaterialUniquePtr MaterialParser::load (Project& project, const std::string& filename) {
    const auto materialJson = JSON::parse (project.container->readString (filename));

    return parse (materialJson, project, filename);
}

MaterialUniquePtr MaterialParser::parse (const JSON& it, Project& project, const std::string& filename) {
    return std::make_unique <Material> (Material {
        .filename = filename,
        .passes = parsePasses (it.require ("passes", "Material must have passes to render"), project),
    });
}

std::vector <MaterialPassUniquePtr> MaterialParser::parsePasses (const JSON& it, Project& project) {
    std::vector <MaterialPassUniquePtr> result = {};

    if (!it.is_array ()) {
        return result;
    }

    for (const auto& cur : it) {
        result.push_back (parsePass (cur, project));
    }

    return result;
}

MaterialPassUniquePtr MaterialParser::parsePass (const JSON& it, Project& project) {
    const auto textures = it.optional ("textures");
    const auto usertextures = it.optional ("usertextures");
    const auto combos = it.optional ("combos");
    const auto constants = it.optional ("constants");

    return std::make_unique <MaterialPass>(MaterialPass {
        //TODO: REMOVE THIS UGLY STD::STRING CREATION
        .blending = parseBlendMode (it.optional ("blending", std::string ("normal"))),
        .cullmode = parseCullMode (it.optional ("cullmode", std::string ("nocull"))),
        .depthtest = parseDepthtestMode (it.optional ("depthtest", std::string ("disabled"))),
        .depthwrite = parseDepthwriteMode (it.optional ("depthwrite", std::string ("disabled"))),
        .shader = it.require <std::string> ("shader", "Material pass must have a shader"),
        .textures = textures.has_value () ? parseTextures (*textures) : TextureMap {},
        .usertextures = usertextures.has_value () ? parseTextures (*usertextures) : TextureMap {},
        .combos = combos.has_value () ? parseCombos (*combos) : ComboMap {},
    });
}

std::map <int, std::string> MaterialParser::parseTextures (const JSON& it) {
    std::map <int, std::string> result = {};

    if (!it.is_array ()) {
        return result;
    }

    int index = 0;

    for (const auto& cur : it) {
        if (!cur.is_null ()) {
            if (!cur.is_string ()) {
                sLog.error ("Detected a non-string texture, most likely a special value: ", cur.dump ());
                result.emplace (index, "");
            } else {
                result.emplace (index, cur);
            }
        }

        index++;
    }

    return result;
}

std::map <std::string, int> MaterialParser::parseCombos (const JSON& it) {
    std::map <std::string, int> result = {};

    if (!it.is_object ()) {
        return result;
    }

    for (const auto& cur : it.items ()) {
        result.emplace (cur.key (), cur.value ());
    }

    return result;
}

BlendingMode MaterialParser::parseBlendMode (const std::string& mode) {
    if (mode == "normal") {
        return BlendingMode_Normal;
    }

    if (mode == "additive") {
        return BlendingMode_Additive;
    }

    if (mode == "translucent") {
        return BlendingMode_Translucent;
    }

    sLog.error ("Unknown blending mode: ", mode, " defaulting to normal");
    return BlendingMode_Normal;
}

CullingMode MaterialParser::parseCullMode (const std::string& mode) {
    if (mode == "nocull") {
        return CullingMode_Disable;
    }

    if (mode == "normal") {
        return CullingMode_Normal;
    }

    sLog.error ("Unknown culling mode: ", mode, " defaulting to nocull");
    return CullingMode_Disable;
}

DepthtestMode MaterialParser::parseDepthtestMode (const std::string& mode) {
    if (mode == "disabled") {
        return DepthtestMode_Disabled;
    }

    if (mode == "enabled") {
        return DepthtestMode_Enabled;
    }

    sLog.error ("Unknown depthtest mode: ", mode, " defaulting to disabled");
    return DepthtestMode_Disabled;
}

DepthwriteMode MaterialParser::parseDepthwriteMode (const std::string& mode) {
    if (mode == "disabled") {
        return DepthwriteMode_Disabled;
    }

    if (mode == "enabled") {
        return DepthwriteMode_Enabled;
    }

    sLog.error ("Unknown depthwrite mode: ", mode, " defaulting to disabled");
    return DepthwriteMode_Disabled;
}
