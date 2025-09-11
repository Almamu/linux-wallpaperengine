#include "EffectParser.h"
#include "MaterialParser.h"

#include "WallpaperEngine/Data/Model/Effect.h"
#include "WallpaperEngine/Data/Model/Material.h"
#include "WallpaperEngine/Data/Model/Project.h"
#include "WallpaperEngine/FileSystem/Container.h"

using namespace WallpaperEngine::Data::Parsers;
using namespace WallpaperEngine::Data::Model;

EffectUniquePtr EffectParser::load (const Project& project, const std::string& filename) {
    const auto effectJson = JSON::parse (project.container->readString (filename));

    return parse (effectJson, project);
}

EffectUniquePtr EffectParser::parse (const JSON& it, const Project& project) {
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

std::vector <EffectPassUniquePtr> EffectParser::parseEffectPasses (const JSON& it, const Project& project) {
    std::vector <EffectPassUniquePtr> result = {};

    if (!it.is_array ()) {
        return result;
    }

    for (const auto& cur : it) {
        const auto binds = cur.optional ("bind");
        const auto command = cur.optional ("command");
        const auto material = cur.optional ("material");

        // TODO: CAN TARGET BE SET IF MATERIAL IS SET?

        result.push_back (std::make_unique <EffectPass> (EffectPass {
            .material = material.has_value () ? MaterialParser::load (project, *material) : std::optional<MaterialUniquePtr> {},
            .binds = binds.has_value () ? parseBinds (binds.value ()) : std::map<int, std::string> {},
            .command = command.has_value () ? (command.value () == "copy" ? Command_Copy : Command_Swap) : std::optional<PassCommandType> {},
            .source = command.has_value () ? cur.require <std::string> ("source", "Effect command must have a source") : cur.optional <std::string> ("source"),
            .target = command.has_value () ? cur.require <std::string> ("target", "Effect command must have a target") : cur.optional <std::string> ("target"),
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
            .unique = cur.optional ("unique", false),
        }));
    }

    return result;
}