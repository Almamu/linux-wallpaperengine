#include "EffectParser.h"
#include "MaterialParser.h"

#include "WallpaperEngine/Data/Model/Material.h"
#include "WallpaperEngine/Data/Model/Effect.h"
#include "WallpaperEngine/Data/Model/Project.h"

using namespace WallpaperEngine::Data::Parsers;
using namespace WallpaperEngine::Data::Model;

EffectUniquePtr EffectParser::load (Project& project, const std::string& filename) {
    const auto effectJson = JSON::parse (project.container->readFileAsString (filename));

    return parse (effectJson, project);
}

EffectUniquePtr EffectParser::parse (const JSON& it, Project& project) {
    const auto dependencies = it.optional ("dependencies");
    const auto fbos = it.optional ("fbos");

    return std::make_unique <Effect> (Effect {
        .name = it.optional <std::string> ("name", ""),
        .description = it.optional <std::string> ("description", ""),
        .group = it.optional <std::string> ("group", ""),
        .preview = it.optional <std::string> ("preview", ""),
        .dependencies = dependencies.has_value () ? parseDependencies (*dependencies) : std::vector <std::string> {},
        .passes = parseEffectPasses (it.require ("passes", "Effect file must have passes"), project),
        .fbos = fbos.has_value () ? parseFBOs (*fbos) : std::vector <FBOUniquePtr> {},
    });
}

std::vector <std::string> EffectParser::parseDependencies (const JSON& it) {
    std::vector <std::string> result = {};

    if (!it.is_array ()) {
        return result;
    }

    for (const auto& cur : it) {
        result.push_back (cur);
    }

    return result;
}

std::vector <EffectPassUniquePtr> EffectParser::parseEffectPasses (const JSON& it, Project& project) {
    std::vector <EffectPassUniquePtr> result = {};

    if (!it.is_array ()) {
        return result;
    }

    for (const auto& cur : it) {
        const auto binds = cur.optional ("bind");
        std::optional <PassCommand> command = std::nullopt;

        if (cur.contains ("command")) {
            command = {
                .command = cur.require <std::string> ("command", "Material command must have a command") == "copy"
                               ? Command_Copy
                               : Command_Swap,
                .target = cur.require <std::string> ("target", "Material command must have a target"),
                .source = cur.require <std::string> ("source", "Material command must have a source"),
            };
        }

        result.push_back (std::make_unique <EffectPass> (EffectPass {
            .material = MaterialParser::load (project, cur.require <std::string> ("material", "Effect pass must have a material")),
            .binds = binds.has_value () ? parseBinds (binds.value ()) : std::map<int, std::string> {},
            .command = command,
            // target is a bit special: if the material is a command this will be nullopt
            // and the actual target will be set in the command itself, otherwise it will
            // be set to whatever is in the json, which is not required to be present for
            // normal materials
            .target = command.has_value () ? std::nullopt : cur.optional <std::string> ("target"),
        }));
    }

    return result;
}

std::map <int, std::string> EffectParser::parseBinds (const JSON& it) {
    std::map <int, std::string> result = {};

    if (!it.is_array ()) {
        return result;
    }

    for (const auto& cur : it) {
        result.emplace (
            cur.require ("index", "Texture binds must have an index"),
            cur.require ("name", "Texture bind must name the FBO that should be used")
        );
    }

    return result;
}

std::vector <FBOUniquePtr> EffectParser::parseFBOs (const JSON& it) {
    std::vector <FBOUniquePtr> result = {};

    if (!it.is_array ()) {
        return result;
    }

    for (const auto& cur : it) {
        result.push_back(std::make_unique<FBO>(FBO {
            .name = cur.require <std::string> ("name", "FBO must have a name"),
            .format = cur.optional <std::string> ("format", "rgba8888"),
            .scale = cur.optional ("scale", 1.0f),
        }));
    }

    return result;
}