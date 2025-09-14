#pragma once

#include <string>
#include <optional>

#include "Types.h"

namespace WallpaperEngine::Data::Model {
enum PassCommandType {
    Command_Copy = 0,
    Command_Swap = 1
};

struct FBO {
    std::string name;
    std::string format;
    float scale;
    bool unique;
};

struct EffectPass {
    /** The material to use for this effect's pass */
    std::optional<MaterialUniquePtr> material;
    /** Texture bindings for this effect's pass */
    TextureMap binds;
    /** The command this material executes (if specified) */
    std::optional <PassCommandType> command;
    /** The source this material renders from (if specified) */
    std::optional <std::string> source;
    /** The target this material renders to (if specified) */
    std::optional <std::string> target;
};

struct Effect {
    /** Effect's name for the UI */
    std::string name;
    /** Effect's description for the UI */
    std::string description;
    /** Effect's group for the UI */
    std::string group;
    /** Effect's preview project */
    std::string preview;
    /** Effect's dependencies */
    std::vector <std::string> dependencies;
    /** The different passes for this effect */
    std::vector <EffectPassUniquePtr> passes;
    /** The fbos declared by this effect */
    std::vector <FBOUniquePtr> fbos;
};
} // namespace WallpaperEngine::Data::Model