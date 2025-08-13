#pragma once

#include <map>
#include <vector>
#include <string>
#include <optional>

#include "Types.h"

namespace WallpaperEngine::Data::Model {
struct MaterialPass {
    // TODO: WRITE ENUMS FOR THESE
    /** Blending mode */
    std::string blending;
    /** Culling mode */
    std::string cullmode;
    /** Depth test mode */
    std::string depthtest;
    /** Depth write mode */
    std::string depthwrite;
    /** Shader file to use for this pass */
    std::string shader;
    /** List of textures defined for this pass */
    TextureMap textures;
    /** The combos and their values to pass onto the shader */
    ComboMap combos;
};

struct Material {
    /** The name of the file this material is defined in */
    std::string filename;
    /** The passes that compose this material */
    std::vector <MaterialPassUniquePtr> passes;
};

} // namespace WallpaperEngine::Data::Model