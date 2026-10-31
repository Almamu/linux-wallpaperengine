#include "CText.h"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <limits>
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
#include "WallpaperEngine/Scripting/ScriptEngine.h"

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

CText::CText (Wallpapers::CScene& scene, const Text& text) :
    CObject (scene, text), ScriptableObject (scene, text), m_text (text) {
    this->registerProperty ("color", *text.color->value);
    this->registerProperty ("alpha", *text.alpha->value);
    this->registerProperty ("origin", *text.origin->value);
    this->registerProperty ("scale", *text.scale->value);
    this->registerProperty ("visible", *text.visible->value);
    this->registerProperty ("pointSize", *text.pointSize->value);
    this->registerProperty ("text", *text.text->value);
    this->registerProperty ("pointSize", *text.pointSize->value);
}

CText::~CText () {
    if (m_layerHandle != Scripting::kInvalidLayerHandle) {
	this->getScene ().getScriptEngine ().destroyLayer (m_layerHandle);
	m_layerHandle = Scripting::kInvalidLayerHandle;
    }
    if (m_vbo != 0) {
	glDeleteBuffers (1, &m_vbo);
    }
    if (m_vao != 0) {
	glDeleteVertexArrays (1, &m_vao);
    }
    if (m_program != 0) {
	glDeleteProgram (m_program);
    }
    if (m_texture != 0) {
	glDeleteTextures (1, &m_texture);
    }
    if (m_ftFace != nullptr) {
	FT_Done_Face (m_ftFace);
    }
    if (m_ftLibrary != nullptr) {
	FT_Done_FreeType (m_ftLibrary);
    }
}

void CText::setup () {
    const bool scripted = m_text.text->value->getScriptSource ().has_value ();
    const auto& text = m_text.text->value->getString ();

    // Nothing to render and no script to produce text later → bail.
    if (text.empty () && !scripted) {
	return;
    }

    if (!initFreeType ()) {
	return;
    }

    if (!loadEmbeddedFont () && !loadSystemFont ()) {
	return;
    }

    m_requestedPixelSize = computeRequestedPixelSize ();
    m_lastPixelSize = computeEffectivePixelSize (m_requestedPixelSize);
    FT_Set_Pixel_Sizes (m_ftFace, 0, static_cast<FT_UInt> (m_lastPixelSize));

    buildShader ();
    // Scripted text may have an empty placeholder; use a single space so the
    // glyph texture has non-zero dimensions until the script produces a value.
    rebuildTextureFrom (text.empty () ? std::string (" ") : text);

    if (scripted) {
	initScriptLayer ();
    }

    m_valid = m_texture != 0 && m_program != 0 && m_vao != 0;
}

bool CText::initFreeType () {
    if (FT_Init_FreeType (&m_ftLibrary) == 0) {
	return true;
    }
    sLog.error ("CText: FT_Init_FreeType failed for object ", m_text.name);
    return false;
}

bool CText::loadEmbeddedFont () {
    // Wallpapers packed in .pkg don't expose physical paths, so we read the font
    // into memory and use FT_New_Memory_Face. m_fontData must outlive the face.
    // `systemfont_*` references signal "use a system font"; let the fallback handle them.
    if (m_text.font.empty () || m_text.font.rfind ("systemfont_", 0) == 0) {
	return false;
    }

    try {
	auto stream = getAssetLocator ().read (m_text.font);
	stream->seekg (0, std::ios::end);
	const auto size = stream->tellg ();
	stream->seekg (0, std::ios::beg);
	m_fontData.resize (static_cast<size_t> (size));
	stream->read (reinterpret_cast<char*> (m_fontData.data ()), size);

	if (FT_New_Memory_Face (
		m_ftLibrary, m_fontData.data (), static_cast<FT_Long> (m_fontData.size ()), 0, &m_ftFace
	    )
	    == 0) {
	    return true;
	}

	sLog.error ("CText: FT_New_Memory_Face failed for '", m_text.font, "', falling back to system font");
    } catch (const std::exception& e) {
	sLog.error ("CText: cannot read font '", m_text.font, "': ", e.what (), ", falling back to system font");
    }

    m_fontData.clear ();
    return false;
}

bool CText::loadSystemFont () {
    std::string fontPath;
    for (const auto& candidate : kFontCandidates) {
	if (std::filesystem::exists (candidate)) {
	    fontPath = candidate;
	    break;
	}
    }
    if (fontPath.empty ()) {
	sLog.error ("CText: no usable system font found");
	return false;
    }
    if (FT_New_Face (m_ftLibrary, fontPath.c_str (), 0, &m_ftFace) != 0) {
	sLog.error ("CText: FT_New_Face failed for ", fontPath);
	return false;
    }
    return true;
}

namespace {
// Wallpaper Engine sizes text at about four scene pixels per point. Measured from
// the bounding "size" the editor stores for each text object: rendering its text
// with its own font, 12 of 18 objects in 5 workshop scenes match at 3.9-4.3 px per
// point (median 4.07). Sizing glyphs at one pixel per point drew all text about
// four times too small.
constexpr float kScenePixelsPerPoint = 4.0f;
// Upper bound for the glyph pixel size, far above anything real scenes use
constexpr float kMaxPixelSize = 16384.0f;
} // namespace

float CText::computeRequestedPixelSize () const {
    // WE text objects often come with scale ~0.09, which would shrink glyphs
    // rasterized at the plain pixel size to a few pixels on screen. Rasterize
    // at higher resolution so that after the model scale is applied in render()
    // the on-screen size matches the intended pointsize.
    const glm::vec3 initialScale = m_text.scale->value->getVec3 ();
    const float avgScale = (initialScale.x + initialScale.y) * 0.5f;
    const float compensate = (avgScale > 0.0f && avgScale < 1.0f) ? std::min (1.0f / avgScale, 32.0f) : 1.0f;
    const float pixelSize = m_text.pointSize->value->getFloat () * kScenePixelsPerPoint * compensate;

    // a negative or NaN point size (e.g. from a script) can't be converted to unsigned
    if (!(pixelSize >= 1.0f)) {
	return 1.0f;
    }

    return pixelSize;
}

unsigned int CText::computeEffectivePixelSize (float requested) {
    return static_cast<unsigned int> (std::min (requested, kMaxPixelSize));
}

void CText::initScriptLayer () {
    const auto& script = m_text.text->value->getScriptSource ();

    if (!script.has_value ()) {
	return;
    }

    m_layerHandle = this->getScene ().getScriptEngine ().createLayerScript (
	*script, m_text.text->value->getProperties (), m_text.text->value->getString ()
    );

    if (m_layerHandle == Scripting::kInvalidLayerHandle) {
	sLog.error ("CText: createLayerScript failed for '", m_text.name, "'");
    }
}

void CText::rebuildTextureFrom (const std::string& text) {
    // Two-pass rasterization: first measure the bounding box, then rasterize
    // every glyph into a single grayscale bitmap. Phase 1 renders one line —
    // multi-line wrapping, alignment, and padding come with Phase 2.
    //
    // Safe to call repeatedly: GL handles (texture, VAO, VBO) are reused when
    // already allocated, so dynamic/scripted text can regenerate the glyph
    // bitmap every time the rendered string changes without leaking.
    FT_GlyphSlot slot = m_ftFace->glyph;

    // Measures the text at the face's current pixel size
    const auto measure = [&] (int& width, int& height, int& ascent) {
	// 64-bit so a very long line at a huge size can't overflow
	int64_t penX = 0;
	int maxAscent = 0;
	int maxDescent = 0;

	for (unsigned char c : text) {
	    if (FT_Load_Char (m_ftFace, static_cast<FT_ULong> (c), FT_LOAD_RENDER) != 0) {
		continue;
	    }
	    penX += slot->advance.x >> 6;
	    maxAscent = std::max (maxAscent, slot->bitmap_top);
	    maxDescent = std::max (maxDescent, static_cast<int> (slot->bitmap.rows) - slot->bitmap_top);
	}

	width = static_cast<int> (std::clamp<int64_t> (penX, 1, std::numeric_limits<int>::max ()));
	height = std::max (1, maxAscent + maxDescent);
	ascent = maxAscent;
    };

    int width = 0;
    int height = 0;
    int maxAscent = 0;
    measure (width, height, maxAscent);

    // The quad always gets the full size, so the text covers the same area on
    // screen even when its texture has to be smaller. When the glyph size was
    // capped at kMaxPixelSize, stretch the quad back to the requested size too.
    const float capStretch
	= m_requestedPixelSize > kMaxPixelSize ? m_requestedPixelSize / static_cast<float> (m_lastPixelSize) : 1.0f;
    const glm::vec2 quadSize = { static_cast<float> (width) * capStretch, static_cast<float> (height) * capStretch };

    // A long line at a large point size can exceed the GPU's texture size limit,
    // and glTexImage2D would then fail and leave the text blank. Rasterize at a
    // smaller pixel size instead; the quad stretches it back to full size.
    GLint maxTextureSize = 0;
    glGetIntegerv (GL_MAX_TEXTURE_SIZE, &maxTextureSize);
    const bool shrunk = maxTextureSize > 0 && std::max (width, height) > maxTextureSize;

    // Glyph metrics don't scale exactly with the pixel size, so shrink again if
    // the first guess still doesn't fit. Clipping is only a last resort.
    unsigned int reducedSize = m_lastPixelSize;
    for (int attempt = 0; shrunk && attempt < 4 && std::max (width, height) > maxTextureSize && reducedSize > 1;
	 attempt++) {
	const float factor = static_cast<float> (maxTextureSize) / static_cast<float> (std::max (width, height));
	reducedSize = std::max (1u, static_cast<unsigned int> (static_cast<float> (reducedSize) * factor));

	if (FT_Set_Pixel_Sizes (m_ftFace, 0, static_cast<FT_UInt> (reducedSize)) != 0) {
	    break;
	}

	measure (width, height, maxAscent);
    }

    if (shrunk) {
	width = std::min (width, static_cast<int> (maxTextureSize));
	height = std::min (height, static_cast<int> (maxTextureSize));
    }

    std::vector<uint8_t> pixels (static_cast<size_t> (width) * height, 0);

    int64_t penX = 0;
    for (unsigned char c : text) {
	if (FT_Load_Char (m_ftFace, static_cast<FT_ULong> (c), FT_LOAD_RENDER) != 0) {
	    continue;
	}

	const auto& bmp = slot->bitmap;
	const int64_t originX = penX + slot->bitmap_left;
	const int originY = maxAscent - slot->bitmap_top;

	for (unsigned int row = 0; row < bmp.rows; ++row) {
	    for (unsigned int col = 0; col < bmp.width; ++col) {
		const int64_t dstX = originX + static_cast<int64_t> (col);
		const int dstY = originY + static_cast<int> (row);
		if (dstX < 0 || dstX >= width || dstY < 0 || dstY >= height) {
		    continue;
		}
		pixels[static_cast<size_t> (dstY) * width + static_cast<size_t> (dstX)]
		    = bmp.buffer[row * bmp.pitch + col];
	    }
	}

	penX += slot->advance.x >> 6;
    }

    const bool firstUpload = (m_texture == 0);
    if (firstUpload) {
	glGenTextures (1, &m_texture);
    }
    glBindTexture (GL_TEXTURE_2D, m_texture);
    glPixelStorei (GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D (GL_TEXTURE_2D, 0, GL_RED, width, height, 0, GL_RED, GL_UNSIGNED_BYTE, pixels.data ());
    if (firstUpload) {
	glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }

    if (shrunk) {
	FT_Set_Pixel_Sizes (m_ftFace, 0, static_cast<FT_UInt> (m_lastPixelSize));
    }

    m_textureSize = { width, height };
    m_quadSize = quadSize;
    m_lastRenderedText = text;

    uploadQuadVertices ();
}

void CText::buildShader () {
    GLuint vs = compileShader (GL_VERTEX_SHADER, kVertexShader);
    GLuint fs = compileShader (GL_FRAGMENT_SHADER, kFragmentShader);
    if (vs == 0 || fs == 0) {
	if (vs) {
	    glDeleteShader (vs);
	}
	if (fs) {
	    glDeleteShader (fs);
	}
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

void CText::uploadQuadVertices () {
    // Quad centered at the origin, sized in pixels. Scene-space placement is
    // done via the model matrix using the object's origin/scale. VBO contents
    // are re-uploaded whenever the glyph bitmap is rebuilt so the quad always
    // matches the current texture dimensions.
    const float hx = m_quadSize.x * 0.5f;
    const float hy = m_quadSize.y * 0.5f;
    // With vflip=true (Wayland/GLFW), GL y- = screen top. So the quad bottom (y=-hy,
    // lower GL y) appears at screen top. UV.v=0 here = FT glyph top → shows at screen top ✓
    const float verts[] = {
	// pos        // uv
	-hx, -hy, 0.0f, 0.0f, hx, -hy, 1.0f, 0.0f, hx,  hy, 1.0f, 1.0f,
	-hx, -hy, 0.0f, 0.0f, hx, hy,  1.0f, 1.0f, -hx, hy, 0.0f, 1.0f,
    };

    const bool firstUpload = (m_vao == 0);
    if (firstUpload) {
	glGenVertexArrays (1, &m_vao);
	glGenBuffers (1, &m_vbo);
    }
    glBindVertexArray (m_vao);
    glBindBuffer (GL_ARRAY_BUFFER, m_vbo);
    glBufferData (GL_ARRAY_BUFFER, sizeof (verts), verts, GL_DYNAMIC_DRAW);
    if (firstUpload) {
	glEnableVertexAttribArray (0);
	glVertexAttribPointer (0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof (float), reinterpret_cast<void*> (0));
	glEnableVertexAttribArray (1);
	glVertexAttribPointer (
	    1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof (float), reinterpret_cast<void*> (2 * sizeof (float))
	);
    }
    glBindVertexArray (0);
}

void CText::render () {
    if (!m_valid) {
	return;
    }
    if (!m_text.visible->value->getBool ()) {
	return;
    }

#if !NDEBUG
    std::string str = "Text " + this->getObject ().name + " (" + std::to_string (this->getObject ().id) + ")";
    glPushDebugGroup (GL_DEBUG_SOURCE_APPLICATION, 0, -1, str.c_str ());
#endif /* DEBUG */
    std::string renderedText = m_lastRenderedText;
    if (m_layerHandle != Scripting::kInvalidLayerHandle) {
	auto& se = this->getScene ().getScriptEngine ();
	se.tickLayer (
	    m_layerHandle, static_cast<double> (getScene ().getTime ()),
	    static_cast<double> (getScene ().getDeltaTime ()), static_cast<double> (getScene ().getFps ())
	);
	const std::string current = se.layerText (m_layerHandle);
	renderedText = current.empty () ? std::string (" ") : current;
    }

    // Past the cap the glyph size stays put, but the quad stretch still follows
    // the requested size, so rebuild when either changes.
    const float requested = computeRequestedPixelSize ();
    const unsigned int pixelSize = computeEffectivePixelSize (requested);
    const bool capChanged
	= requested != m_requestedPixelSize && (requested > kMaxPixelSize || m_requestedPixelSize > kMaxPixelSize);
    m_requestedPixelSize = requested;
    if (pixelSize != m_lastPixelSize) {
	m_lastPixelSize = pixelSize;
	FT_Set_Pixel_Sizes (m_ftFace, 0, static_cast<FT_UInt> (m_lastPixelSize));
	rebuildTextureFrom (renderedText);
    } else if (capChanged || renderedText != m_lastRenderedText) {
	rebuildTextureFrom (renderedText);
    }

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

    const glm::mat4 mvp = getScene ().getCamera ().getProjection () * getScene ().getCamera ().getLookAt () * model;

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
#if !NDEBUG
    glPopDebugGroup ();
#endif /* DEBUG */
}
