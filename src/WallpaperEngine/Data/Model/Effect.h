#pragma once

#include <string>

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
};

struct PassCommand {
    /** The type of command to execute */
    PassCommandType command;
    /** The target of the command (where to draw to) */
    std::string target;
    /** The source of the command (where to draw from) */
    std::string source;
};

struct EffectPass {
    /** The material to use for this effect's pass */
    MaterialUniquePtr material;
    /** Texture bindings for this effect's pass */
    TextureMap binds;
    /** The command this material executes (if specified) */
    std::optional <PassCommand> command;
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