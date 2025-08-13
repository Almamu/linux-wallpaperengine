#include "MaterialParser.h"

#include "WallpaperEngine/Data/Model/Material.h"
#include "WallpaperEngine/Data/Model/Project.h"

using namespace WallpaperEngine::Data::Parsers;
using namespace WallpaperEngine::Data::Model;

MaterialUniquePtr MaterialParser::load (Project& project, const std::string& filename) {
    const auto materialJson = JSON::parse (project.container->readFileAsString (filename));

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
    const auto combos = it.optional ("combos");
    const auto constants = it.optional ("constants");

    return std::make_unique <MaterialPass>(MaterialPass {
        //TODO: REMOVE THIS UGLY STD::STRING CREATION
        .blending = it.optional ("blending", std::string ("normal")),
        .cullmode = it.optional ("cullmode", std::string ("disabled")),
        .depthtest = it.optional ("depthtest", std::string ("disabled")),
        .depthwrite = it.optional ("depthwrite", std::string ("disabled")),
        .shader = it.require <std::string> ("shader", "Material pass must have a shader"),
        .textures = textures.has_value () ? parseTextures (*textures) : std::map <int, std::string> {},
        .combos = combos.has_value () ? parseCombos (*combos) : std::map <std::string, int> {},
    });
}

std::map <int, std::string> MaterialParser::parseTextures (const JSON& it) {
    std::map <int, std::string> result = {};

    if (!it.is_array ()) {
        return result;
    }

    int index = 0;

    for (const auto& cur : it) {
        if (!it.is_null ()) {
            result.emplace (index, cur);
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