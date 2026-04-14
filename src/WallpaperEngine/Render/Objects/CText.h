#pragma once

#include <GL/glew.h>
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

#include "WallpaperEngine/Render/CObject.h"

// Forward-declare FreeType types to avoid leaking the header into users.
struct FT_LibraryRec_;
struct FT_FaceRec_;
typedef struct FT_LibraryRec_* FT_Library;
typedef struct FT_FaceRec_* FT_Face;

namespace WallpaperEngine::Render::Wallpapers {
class CScene;
}

namespace WallpaperEngine::Render::Objects {
using namespace WallpaperEngine::Data::Model;

/**
 * Phase 1 text renderer.
 *
 * Renders static text objects as a single FreeType-rasterized RGBA texture
 * drawn on a textured quad with its own minimal GLSL shader. Does NOT go
 * through CRenderable / materials / passes — Phase 1 does not need effects.
 *
 * Phase 2 (scripted/dynamic text, alignment from properties, effect passes)
 * is intentionally not implemented here. When the scene provides a dynamic
 * `text: { script: "..." }` object this class captures the script source in
 * the data model but renders an empty string — the Wallpaper Engine JS
 * runtime required to evaluate it is out of scope for Phase 1.
 */
class CText final : public CObject {
public:
    CText (Wallpapers::CScene& scene, const Text& text);
    ~CText () override;

    void setup ();
    void render () override;

private:
    void buildTexture ();
    void buildShader ();
    void buildQuad ();

    const Text& m_text;

    FT_Library m_ftLibrary = nullptr;
    FT_Face m_ftFace = nullptr;

    GLuint m_texture = 0;
    GLuint m_program = 0;
    GLuint m_vao = 0;
    GLuint m_vbo = 0;

    GLint m_uMVP = -1;
    GLint m_uColor = -1;
    GLint m_uTexture = -1;

    glm::ivec2 m_textureSize = {0, 0};
    glm::vec2 m_quadSize = {0.0f, 0.0f};

    bool m_valid = false;
};
} // namespace WallpaperEngine::Render::Objects
