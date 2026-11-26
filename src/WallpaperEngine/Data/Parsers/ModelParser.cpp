#include "ModelParser.h"
#include "MaterialParser.h"
#include "MdlParser.h"

#include "WallpaperEngine/Data/Model/Material.h"
#include "WallpaperEngine/Data/Model/Model.h"
#include "WallpaperEngine/Data/Model/Project.h"
#include "WallpaperEngine/Data/Utils/JsonTelemetry.h"
#include "WallpaperEngine/FileSystem/Container.h"
#include "WallpaperEngine/Logging/Log.h"

using namespace WallpaperEngine::Data::Parsers;
using namespace WallpaperEngine::Data::Model;

ModelUniquePtr ModelParser::load (const Project& project, const std::string& filename) {
    const auto model
	= WallpaperEngine::Data::JSON::parseCompatible (project.assetLocator->readString (filename), filename);

    auto result = parse (model, project, filename);

    WallpaperEngine::Data::Utils::JsonTelemetry::scan (model, filename);

    return result;
}

ModelUniquePtr ModelParser::parse (const JSON& file, const Project& project, const std::string& filename) {
    const auto material = file.require<std::string> ("material", "Model must have a material");
    const bool fullscreen = file.optional ("fullscreen", false);
    const bool passthrough = file.optional ("passthrough", false);
    const auto puppet = file.optional<std::string> ("puppet");
    std::optional<MdlMesh> puppetMesh;

    // Native image initialization skips puppet resources for these model modes.
    if (puppet.has_value () && !fullscreen && !passthrough) {
	try {
	    puppetMesh = MdlParser::load (project, *puppet);
	} catch (const std::exception& ex) {
	    // A malformed puppet must not discard the image layer itself; CImage keeps
	    // rendering the undeformed material when the parsed mesh is unavailable.
	    sLog.error ("Could not load puppet model ", *puppet, ": ", ex.what ());
	}
    }

    return std::make_unique<ModelStruct> (ModelStruct {
	.filename = filename,
	.material = MaterialParser::load (project, material),
	.solidlayer = file.optional ("solidlayer", false),
	.instanced = file.optional ("instanced", false),
	.fullscreen = fullscreen,
	.passthrough = passthrough,
	.autosize = file.optional ("autosize", false),
	.nopadding = file.optional ("nopadding", false),
	.width = file.optional<int> ("width"),
	.height = file.optional<int> ("height"),
	.puppet = puppet,
	.puppetMesh = std::move (puppetMesh),
    });
}
