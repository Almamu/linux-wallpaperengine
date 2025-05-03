#include "ShaderConstantParser.h"

#include "WallpaperEngine/Data/Model/Project.h"

using namespace WallpaperEngine::Data::Parsers;
using namespace WallpaperEngine::Data::Model;

ShaderConstantMap ShaderConstantParser::parse (const JSON& it, const ProjectWeakPtr& project) {
    if (!it.is_object ()) {
        return {};
    }

    std::map <std::string, ShaderConstantUniquePtr> result = {};

    for (const auto& cur : it.items ()) {
        result.emplace (cur.key(), parseConstant (cur.value(), project));
    }

    return result;
}

ShaderConstantUniquePtr ShaderConstantParser::parseConstant (const JSON& it, const ProjectWeakPtr& project) {
    ShaderConstant* constant = nullptr;
    auto valueIt = it;

    if (it.is_object ()) {
        auto user = it.find ("user");
        auto value = it.find ("value");

        if (user == it.end () && value == it.end ()) {
            sLog.error (R"(Found object for shader constant without "value" and "user" setting)");
            return ShaderConstantUniquePtr (constant);
        }

        if (user != it.end () && user->is_string ()) {
            const auto& properties = project.lock ()->properties;
            const auto& propertyIt = properties.find (*user);

            if (propertyIt != properties.end ()) {
                constant = new ShaderConstantProperty (propertyIt->second);
            } else {
                sLog.error ("Shader constant pointing to non-existant project property: ", user->get <std::string> ());
            }
        } else {
            valueIt = *value;
        }
    }

    // TODO: REFACTOR THIS, SAME OLD THING WE HAD BEFORE, THESE SHADERCONSTANT CLASSES HAVE TO GO AND WE SHOULD HAVE JUST ONE
    if (constant == nullptr) {
        if (valueIt.is_number_float ()) {
            constant = new ShaderConstantFloat (valueIt);
        } else if (valueIt.is_number_integer ()) {
            constant = new ShaderConstantInteger (valueIt);
        } else if (valueIt.is_string ()) {
            // TODO: USE VECTOR PARSER HERE? MAKE USE OF CDYNAMICVALUE FOR SHADERCONSTANTS TOO

            // count the number of spaces to determine which type of vector we have
            std::string value = valueIt;

            size_t spaces =
                std::count_if (value.begin (), value.end (), [&] (const auto& item) { return item == ' '; });

            if (spaces == 1) {
                constant = new ShaderConstantVector2 (valueIt);
            } else if (spaces == 2) {
                constant = new ShaderConstantVector3 (valueIt);
            } else if (spaces == 3) {
                constant = new ShaderConstantVector4 (valueIt);
            } else {
                sLog.exception ("unknown shader constant type ", value);
            }
        }
    }

    return ShaderConstantUniquePtr (constant);
}