#include "ModelParser.h"
#include "MaterialParser.h"

#include "WallpaperEngine/Data/Model/Material.h"
#include "WallpaperEngine/Data/Model/Model.h"
#include "WallpaperEngine/Data/Model/Project.h"
#include "WallpaperEngine/FileSystem/Container.h"

using namespace WallpaperEngine::Data::Parsers;
using namespace WallpaperEngine::Data::Model;

ModelUniquePtr ModelParser::load (Project& project, const std::string& filename) {
    const auto model = JSON::parse (project.container->readString (filename));

    return parse (model, project, filename);
}

ModelUniquePtr ModelParser::parse (const JSON& file, Project& project, const std::string& filename) {
    const auto material = file.require <std::string> ("material", "Model must have a material");

    return std::make_unique <ModelStruct> (ModelStruct {
        .filename = filename,
        .material = MaterialParser::load (project, material),
        .solidlayer = file.optional ("solidlayer", false),
        .fullscreen = file.optional ("fullscreen", false),
        .passthrough = file.optional ("passthrough", false),
        .autosize = file.optional ("autosize", false),
        .nopadding = file.optional ("nopadding", false),
        .width = file.optional <int> ("width"),
        .height = file.optional <int> ("height"),
    });
}