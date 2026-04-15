#include "CText.h"

#include <filesystem>
#include <vector>

#include <ft2build.h>
#include FT_FREETYPE_H

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "WallpaperEngine/Data/Model/DynamicValue.h"
#include "WallpaperEngine/Data/Model/Object.h"
#include "WallpaperEngine/Data/Model/UserSetting.h"
#include "WallpaperEngine/Logging/Log.h"
#include "WallpaperEngine/Render/Camera.h"
#include "WallpaperEngine/Render/Wallpapers/CScene.h"

using namespace WallpaperEngine::Render::Objects;

namespace {
// TODO: Phase 2 – load font from wallpaper's materials/fonts/ using AssetLocator
// Phase 1 uses a system font instead of the font shipped by the wallpaper.
// Wallpaper Engine bundles .ttf files in `materials/fonts/`; wiring those in
// is deferred to Phase 2 along with dynamic/scripted text.
const std::vector<std::string> kFontCandidates = {
    "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
    "/usr/share/fonts/TTF/DejaVuSans.ttf",
    "/usr/share/fonts/dejavu/DejaVuSans.ttf",
    "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
};

const char* kVertexShader = R"glsl(
#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aUV;
uniform mat4 uMVP;
out vec2 vUV;
void main() {
    vUV = aUV;
    gl_Position = uMVP * vec4(aPos, 0.0, 1.0);
}
)glsl";

const char* kFragmentShader = R"glsl(
#version 330 core
in vec2 vUV;
uniform sampler2D uTexture;
uniform vec4 uColor;
out vec4 FragColor;
void main() {
    float coverage = texture(uTexture, vUV).r;
    FragColor = vec4(uColor.rgb, uColor.a * coverage);
}
)glsl";

GLuint compileShader (GLenum type, const char* source) {
    GLuint shader = glCreateShader (type);
    glShaderSource (shader, 1, &source, nullptr);
    glCompileShader (shader);

    GLint status = GL_FALSE;
    glGetShaderiv (shader, GL_COMPILE_STATUS, &status);
    if (status != GL_TRUE) {
	char log[1024];
	glGetShaderInfoLog (shader, sizeof (log), nullptr, log);
	sLog.error ("CText shader compile failed: ", log);
	glDeleteShader (shader);
	return 0;
    }
    return shader;
}
} // namespace

CText::CText (Wallpapers::CScene& scene, const Text& text) : CObject (scene, text), m_text (text) {}

CText::~CText () {
    if (m_vbo != 0) glDeleteBuffers (1, &m_vbo);
    if (m_vao != 0) glDeleteVertexArrays (1, &m_vao);
    if (m_program != 0) glDeleteProgram (m_program);
    if (m_texture != 0) glDeleteTextures (1, &m_texture);
    if (m_ftFace != nullptr) FT_Done_Face (m_ftFace);
    if (m_ftLibrary != nullptr) FT_Done_FreeType (m_ftLibrary);
}

void CText::setup () {
    // Phase 1: only static text is supported. If the scene provides a scripted
    // text source we keep the object alive but render nothing — the JS-driven
    // value evaluator needed to produce the string is a Phase 2 concern.
    if (m_text.text.empty ()) {
	if (!m_text.script.empty ()) {
	    sLog.out ("CText: skipping dynamic/scripted text (Phase 1 static-only): ", m_text.name);
	}
	return;
    }

    if (FT_Init_FreeType (&m_ftLibrary) != 0) {
	sLog.error ("CText: FT_Init_FreeType failed for object ", m_text.name);
	return;
    }

    std::string fontPath;
    for (const auto& candidate : kFontCandidates) {
	if (std::filesystem::exists (candidate)) {
	    fontPath = candidate;
	    break;
	}
    }

    if (fontPath.empty ()) {
	sLog.error ("CText: no usable system font found (Phase 1 uses system fonts)");
	return;
    }

    if (FT_New_Face (m_ftLibrary, fontPath.c_str (), 0, &m_ftFace) != 0) {
	sLog.error ("CText: FT_New_Face failed for ", fontPath);
	return;
    }

    FT_Set_Pixel_Sizes (m_ftFace, 0, static_cast<FT_UInt> (m_text.pointsize));

    buildTexture ();
    buildShader ();
    buildQuad ();

    m_valid = m_texture != 0 && m_program != 0 && m_vao != 0;
}

void CText::buildTexture () {
    // Two-pass rasterization: first measure the bounding box, then rasterize
    // every glyph into a single grayscale bitmap. Phase 1 renders one line —
    // multi-line wrapping, alignment, and padding come with Phase 2.
    FT_GlyphSlot slot = m_ftFace->glyph;

    int penX = 0;
    int maxAscent = 0;
    int maxDescent = 0;

    for (unsigned char c : m_text.text) {
	if (FT_Load_Char (m_ftFace, static_cast<FT_ULong> (c), FT_LOAD_RENDER) != 0) {
	    continue;
	}
	penX += slot->advance.x >> 6;
	maxAscent = std::max (maxAscent, slot->bitmap_top);
	maxDescent = std::max (maxDescent, static_cast<int> (slot->bitmap.rows) - slot->bitmap_top);
    }

    const int width = std::max (1, penX);
    const int height = std::max (1, maxAscent + maxDescent);
    std::vector<uint8_t> pixels (static_cast<size_t> (width) * height, 0);

    penX = 0;
    for (unsigned char c : m_text.text) {
	if (FT_Load_Char (m_ftFace, static_cast<FT_ULong> (c), FT_LOAD_RENDER) != 0) {
	    continue;
	}

	const auto& bmp = slot->bitmap;
	const int originX = penX + slot->bitmap_left;
	const int originY = maxAscent - slot->bitmap_top;

	for (unsigned int row = 0; row < bmp.rows; ++row) {
	    for (unsigned int col = 0; col < bmp.width; ++col) {
		const int dstX = originX + static_cast<int> (col);
		const int dstY = originY + static_cast<int> (row);
		if (dstX < 0 || dstX >= width || dstY < 0 || dstY >= height) continue;
		pixels[static_cast<size_t> (dstY) * width + dstX] = bmp.buffer[row * bmp.pitch + col];
	    }
	}

	penX += slot->advance.x >> 6;
    }

    glGenTextures (1, &m_texture);
    glBindTexture (GL_TEXTURE_2D, m_texture);
    glPixelStorei (GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D (GL_TEXTURE_2D, 0, GL_RED, width, height, 0, GL_RED, GL_UNSIGNED_BYTE, pixels.data ());
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    m_textureSize = {width, height};
    m_quadSize = {static_cast<float> (width), static_cast<float> (height)};
}

void CText::buildShader () {
    GLuint vs = compileShader (GL_VERTEX_SHADER, kVertexShader);
    GLuint fs = compileShader (GL_FRAGMENT_SHADER, kFragmentShader);
    if (vs == 0 || fs == 0) {
	if (vs) glDeleteShader (vs);
	if (fs) glDeleteShader (fs);
	return;
    }

    m_program = glCreateProgram ();
    glAttachShader (m_program, vs);
    glAttachShader (m_program, fs);
    glLinkProgram (m_program);
    glDeleteShader (vs);
    glDeleteShader (fs);

    GLint status = GL_FALSE;
    glGetProgramiv (m_program, GL_LINK_STATUS, &status);
    if (status != GL_TRUE) {
	char log[1024];
	glGetProgramInfoLog (m_program, sizeof (log), nullptr, log);
	sLog.error ("CText program link failed: ", log);
	glDeleteProgram (m_program);
	m_program = 0;
	return;
    }

    m_uMVP = glGetUniformLocation (m_program, "uMVP");
    m_uColor = glGetUniformLocation (m_program, "uColor");
    m_uTexture = glGetUniformLocation (m_program, "uTexture");
}

void CText::buildQuad () {
    // Quad centered at the origin, sized in pixels. Scene-space placement is
    // done via the model matrix using the object's origin/scale.
    const float hx = m_quadSize.x * 0.5f;
    const float hy = m_quadSize.y * 0.5f;
    // With vflip=true (Wayland/GLFW), GL y- = screen top. So the quad bottom (y=-hy,
    // lower GL y) appears at screen top. UV.v=0 here = FT glyph top → shows at screen top ✓
    const float verts[] = {
	// pos        // uv
	-hx, -hy, 0.0f, 0.0f,
	 hx, -hy, 1.0f, 0.0f,
	 hx,  hy, 1.0f, 1.0f,
	-hx, -hy, 0.0f, 0.0f,
	 hx,  hy, 1.0f, 1.0f,
	-hx,  hy, 0.0f, 1.0f,
    };

    glGenVertexArrays (1, &m_vao);
    glGenBuffers (1, &m_vbo);
    glBindVertexArray (m_vao);
    glBindBuffer (GL_ARRAY_BUFFER, m_vbo);
    glBufferData (GL_ARRAY_BUFFER, sizeof (verts), verts, GL_STATIC_DRAW);
    glEnableVertexAttribArray (0);
    glVertexAttribPointer (0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof (float), reinterpret_cast<void*> (0));
    glEnableVertexAttribArray (1);
    glVertexAttribPointer (1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof (float), reinterpret_cast<void*> (2 * sizeof (float)));
    glBindVertexArray (0);
}

void CText::render () {
    if (!m_valid) return;
    if (!m_text.visible->value->getBool ()) return;

    const glm::vec4 color = m_text.color->value->getVec4 ();
    const float alpha = m_text.alpha->value->getFloat ();
    const glm::vec3 scale = m_text.scale->value->getVec3 ();
    const glm::vec3 origin = m_text.origin->value->getVec3 ();

    // WE uses a Y-down coordinate system (origin at top-left, y increases downward).
    // The final FBO is presented to screen with vflip=true on Wayland/GLFW, which maps
    // GL y- to screen top and GL y+ to screen bottom. This effectively inverts Y again,
    // so we need: gl_y = origin.y - scene_h/2  (not the CImage-style scene_h/2 - origin.y).
    // CImage pre-compensates for X11 (no vflip) and gets corrected by the Wayland vflip.
    // CText renders with direct vflip-aware coordinates.
    const float scene_w = getScene ().getCamera ().getWidth ();
    const float scene_h = getScene ().getCamera ().getHeight ();
    const glm::vec3 gl_origin = {
	origin.x - scene_w * 0.5f,
	origin.y - scene_h * 0.5f,
	origin.z,
    };

    glm::mat4 model = glm::translate (glm::mat4 (1.0f), gl_origin);
    model = glm::scale (model, scale);

    const glm::mat4 mvp = getScene ().getCamera ().getProjection ()
			  * getScene ().getCamera ().getLookAt ()
			  * model;

    glEnable (GL_BLEND);
    glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glUseProgram (m_program);
    glUniformMatrix4fv (m_uMVP, 1, GL_FALSE, glm::value_ptr (mvp));
    glUniform4f (m_uColor, color.r, color.g, color.b, color.a * alpha);

    glActiveTexture (GL_TEXTURE0);
    glBindTexture (GL_TEXTURE_2D, m_texture);
    glUniform1i (m_uTexture, 0);

    glBindVertexArray (m_vao);
    glDrawArrays (GL_TRIANGLES, 0, 6);
    glBindVertexArray (0);
}
