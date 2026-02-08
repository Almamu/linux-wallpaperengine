#include "ShaderConstantParser.h"

#include "WallpaperEngine/Data/Model/Project.h"
#include "WallpaperEngine/Data/Parsers/UserSettingParser.h"

using namespace WallpaperEngine::Data::Parsers;
using namespace WallpaperEngine::Data::Model;

ShaderConstantMap ShaderConstantParser::parse (const JSON& it, const Project& project) {
    if (!it.is_object ()) {
	return {};
    }

    ShaderConstantMap result = {};

    for (const auto& cur : it.items ()) {
	result.emplace (cur.key (), UserSettingParser::parse (cur.value (), project.properties));
    }

    return result;
}
