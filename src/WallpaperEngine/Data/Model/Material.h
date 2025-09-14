#pragma once

#include <map>
#include <vector>
#include <string>
#include <optional>

#include "Types.h"

namespace WallpaperEngine::Data::Model {

enum BlendingMode {
    BlendingMode_Unknown = 0,
    BlendingMode_Normal = 1,
    BlendingMode_Translucent = 2,
    BlendingMode_Additive = 3,
};

enum CullingMode {
    CullingMode_Unknown = 0,
    CullingMode_Normal = 1,
    CullingMode_Disable = 2
};

enum DepthtestMode {
    DepthtestMode_Unknown = 0,
    DepthtestMode_Disabled = 1,
    DepthtestMode_Enabled = 2,
};

enum DepthwriteMode {
    DepthwriteMode_Unknown = 0,
    DepthwriteMode_Disabled = 1,
    DepthwriteMode_Enabled = 2,
};

struct MaterialPass {
    /** Blending mode */
    BlendingMode blending;
    /** Culling mode */
    CullingMode cullmode;
    /** Depth test mode */
    DepthtestMode depthtest;
    /** Depth write mode */
    DepthwriteMode depthwrite;
    /** Shader file to use for this pass */
    std::string shader;
    /** List of textures defined for this pass */
    TextureMap textures;
    /** List of user textures defined for this pass */
    TextureMap usertextures;
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