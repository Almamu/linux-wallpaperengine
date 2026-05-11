#include "CText.h"

#include "WallpaperEngine/Logging/Log.h"
#include "WallpaperEngine/Render/Camera.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <ctime>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iomanip>
#include <iterator>
#include <sstream>

#include <ft2build.h>
#include FT_FREETYPE_H

using namespace WallpaperEngine::Render::Objects;
using namespace WallpaperEngine::Data::Model;

namespace {
const char* TEXT_VERTEX_SHADER = R"GLSL(
#version 330
in vec3 a_Position;
in vec2 a_TexCoord;
out vec2 v_TexCoord;
uniform mat4 g_ModelViewProjectionMatrix;
void main() {
    v_TexCoord = a_TexCoord;
    gl_Position = g_ModelViewProjectionMatrix * vec4(a_Position, 1.0);
}
)GLSL";

const char* TEXT_FRAGMENT_SHADER = R"GLSL(
#version 330
in vec2 v_TexCoord;
out vec4 out_FragColor;
uniform sampler2D u_Texture;
uniform vec4 u_Color;
void main() {
    float alpha = texture(u_Texture, v_TexCoord).r;
    out_FragColor = vec4(u_Color.rgb, u_Color.a * alpha);
}
)GLSL";

glm::vec2 rotateVec2 (const glm::vec2& value, float angle) {
    const float cosAngle = std::cos (angle);
    const float sinAngle = std::sin (angle);
    return { value.x * cosAngle - value.y * sinAngle, value.x * sinAngle + value.y * cosAngle };
}

std::vector<unsigned char> readFilesystemFont (const std::filesystem::path& path) {
    std::ifstream stream (path, std::ios::binary);
    if (!stream) {
	return {};
    }

    return { std::istreambuf_iterator<char> (stream), std::istreambuf_iterator<char> () };
}

std::string lowercase (std::string value) {
    std::ranges::transform (value, value.begin (), [] (unsigned char c) { return static_cast<char> (std::tolower (c)); });
    return value;
}

std::vector<std::filesystem::path> splitFontDirs (const char* value) {
    std::vector<std::filesystem::path> result;
    if (value == nullptr) {
	return result;
    }

    std::stringstream stream (value);
    std::string item;
    while (std::getline (stream, item, ':')) {
	if (!item.empty ()) {
	    result.emplace_back (item);
	}
    }
    return result;
}

bool isSupportedFontFile (const std::filesystem::directory_entry& entry) {
    std::error_code entryError;
    if (!entry.is_regular_file (entryError) || entryError) {
	return false;
    }

    const auto extension = lowercase (entry.path ().extension ().string ());
    return extension == ".ttf" || extension == ".otf" || extension == ".ttc";
}

void collectFontCandidates (
    const std::filesystem::path& root,
    const std::vector<std::string>& requestedHints,
    std::vector<std::filesystem::path>& matches,
    std::vector<std::filesystem::path>& fallbacks
) {
    try {
	for (const auto& entry : std::filesystem::recursive_directory_iterator (
		 root, std::filesystem::directory_options::skip_permission_denied
	     )) {
	    if (!isSupportedFontFile (entry)) {
		continue;
	    }
	    const auto filename = lowercase (entry.path ().filename ().string ());
	    fallbacks.push_back (entry.path ());
	    for (const auto& hint : requestedHints) {
		if (!hint.empty () && filename.find (hint) != std::string::npos) {
		    matches.push_back (entry.path ());
		    break;
		}
	    }
	}
    } catch (const std::filesystem::filesystem_error& e) {
	sLog.error ("Font discovery skipped ", root.string (), ": ", e.what ());
    }
}

std::vector<std::filesystem::path> findFallbackFonts (const std::string& requestedFont, bool preferSymbols) {
    std::vector<std::filesystem::path> roots = splitFontDirs (std::getenv ("LWE_FONT_DIRS"));
    roots.emplace_back ("/usr/share/fonts");
    roots.emplace_back ("/usr/local/share/fonts");
    if (const char* home = std::getenv ("HOME"); home != nullptr) {
	roots.emplace_back (std::filesystem::path (home) / ".local/share/fonts");
	roots.emplace_back (std::filesystem::path (home) / ".fonts");
    }

    const std::vector<std::string> requestedHints = preferSymbols
	? std::vector<std::string> { "symbols", "noto", "dejavu", "liberation", "sans" }
	: std::vector<std::string> { lowercase (requestedFont), "liberation", "noto", "dejavu", "cantarell", "sans" };

    std::vector<std::filesystem::path> matches;
    std::vector<std::filesystem::path> fallbacks;
    for (const auto& root : roots) {
	std::error_code existsError;
	if (!std::filesystem::exists (root, existsError) || existsError) {
	    continue;
	}
	collectFontCandidates (root, requestedHints, matches, fallbacks);
    }

    matches.insert (matches.end (), fallbacks.begin (), fallbacks.end ());
    return matches;
}

struct TextLayout {
    int maxWidth = 1;
    int ascent = 0;
    int descent = 0;
    int lineHeight = 0;
    int padding = 0;
    std::vector<int> lineWidths = {};
    std::vector<std::vector<uint32_t>> lineCodepoints = {};
};

std::vector<uint32_t> decodeUtf8 (const std::string& text);

TextLayout measureTextLayout (FT_Face face, const std::vector<std::string>& lines, int pixelSize) {
    TextLayout layout {
	.ascent = static_cast<int> (face->size->metrics.ascender >> 6),
	.descent = static_cast<int> (-(face->size->metrics.descender >> 6)),
	.lineHeight = glm::max (static_cast<int> (face->size->metrics.height >> 6), pixelSize),
	.padding = glm::max (4, pixelSize / 8),
    };
    layout.lineWidths.reserve (lines.size ());
    layout.lineCodepoints.reserve (lines.size ());

    for (const auto& line : lines) {
	auto codepoints = decodeUtf8 (line);
	int width = 0;
	for (const uint32_t character : codepoints) {
	    if (FT_Load_Char (face, character, FT_LOAD_DEFAULT) == 0) {
		width += static_cast<int> (face->glyph->advance.x >> 6);
	    }
	}
	layout.lineCodepoints.push_back (std::move (codepoints));
	layout.lineWidths.push_back (width);
	layout.maxWidth = glm::max (layout.maxWidth, width);
    }
    return layout;
}

int alignedPenX (const TextLayout& layout, size_t lineIndex, const std::string& alignment) {
    int penX = layout.padding;
    if (alignment.find ("right") != std::string::npos) {
	penX += layout.maxWidth - layout.lineWidths[lineIndex];
    } else if (alignment.find ("left") == std::string::npos) {
	penX += (layout.maxWidth - layout.lineWidths[lineIndex]) / 2;
    }
    return penX;
}

void blendGlyph (std::vector<unsigned char>& bitmap, int textureWidth, int textureHeight, const FT_Bitmap& glyph, int dstX, int dstY) {
    for (unsigned int row = 0; row < glyph.rows; row++) {
	const int y = dstY + static_cast<int> (row);
	if (y < 0 || y >= textureHeight) {
	    continue;
	}
	for (unsigned int col = 0; col < glyph.width; col++) {
	    const int x = dstX + static_cast<int> (col);
	    if (x < 0 || x >= textureWidth) {
		continue;
	    }
	    const auto source = glyph.buffer[row * glyph.pitch + col];
	    auto& target = bitmap[static_cast<size_t> (y * textureWidth + x)];
	    target = std::max (target, source);
	}
    }
}

void rasterizeTextLayout (
    FT_Face face,
    const TextLayout& layout,
    const std::string& alignment,
    int textureWidth,
    int textureHeight,
    std::vector<unsigned char>& bitmap
) {
    for (size_t lineIndex = 0; lineIndex < layout.lineCodepoints.size (); lineIndex++) {
	int penX = alignedPenX (layout, lineIndex, alignment);
	const int baseline = layout.padding + layout.ascent + static_cast<int> (lineIndex) * layout.lineHeight;
	for (const uint32_t character : layout.lineCodepoints[lineIndex]) {
	    if (FT_Load_Char (face, character, FT_LOAD_RENDER) != 0) {
		continue;
	    }
	    const auto* glyph = face->glyph;
	    blendGlyph (bitmap, textureWidth, textureHeight, glyph->bitmap, penX + glyph->bitmap_left, baseline - glyph->bitmap_top);
	    penX += static_cast<int> (glyph->advance.x >> 6);
	}
    }
}

std::vector<uint32_t> decodeUtf8 (const std::string& text) {
    std::vector<uint32_t> result;
    for (size_t i = 0; i < text.size ();) {
	const auto c = static_cast<unsigned char> (text[i]);
	uint32_t codepoint = 0;
	size_t extra = 0;
	if ((c & 0x80) == 0) {
	    codepoint = c;
	} else if ((c & 0xE0) == 0xC0) {
	    codepoint = c & 0x1F;
	    extra = 1;
	} else if ((c & 0xF0) == 0xE0) {
	    codepoint = c & 0x0F;
	    extra = 2;
	} else if ((c & 0xF8) == 0xF0) {
	    codepoint = c & 0x07;
	    extra = 3;
	} else {
	    i++;
	    continue;
	}

	if (i + extra >= text.size ()) {
	    break;
	}

	bool valid = true;
	for (size_t j = 1; j <= extra; j++) {
	    const auto next = static_cast<unsigned char> (text[i + j]);
	    if ((next & 0xC0) != 0x80) {
		valid = false;
		break;
	    }
	    codepoint = (codepoint << 6) | (next & 0x3F);
	}

	if (valid && codepoint != 0xFE0E && codepoint != 0xFE0F) {
	    result.push_back (codepoint);
	    i += extra + 1;
	} else {
	    i++;
	}
    }
    return result;
}
}

CText::CText (Wallpapers::CScene& scene, const Text& text) : CObject (scene, text), m_text (text) {
    if (std::getenv ("LWE_TEXT_DEBUG") != nullptr) {
	sLog.out ("Text debug create id=", text.id, " name=", text.name);
    }
    this->setup ();
}

CText::~CText () {
    if (this->m_texture != 0) {
	glDeleteTextures (1, &this->m_texture);
    }
    if (this->m_vbo != 0) {
	glDeleteBuffers (1, &this->m_vbo);
    }
    if (this->m_vao != 0) {
	glDeleteVertexArrays (1, &this->m_vao);
    }
    if (this->m_program != 0) {
	glDeleteProgram (this->m_program);
    }
}

GLuint CText::compileShader (GLenum type, const char* source) {
    const GLuint shader = glCreateShader (type);
    glShaderSource (shader, 1, &source, nullptr);
    glCompileShader (shader);

    GLint result = GL_FALSE;
    glGetShaderiv (shader, GL_COMPILE_STATUS, &result);
    if (result == GL_FALSE) {
	GLint length = 0;
	glGetShaderiv (shader, GL_INFO_LOG_LENGTH, &length);
	std::string log (length, '\0');
	glGetShaderInfoLog (shader, length, nullptr, log.data ());
	sLog.exception ("Text shader compilation failed: ", log);
    }

    return shader;
}

void CText::setup () {
    const GLuint vertex = this->compileShader (GL_VERTEX_SHADER, TEXT_VERTEX_SHADER);
    const GLuint fragment = this->compileShader (GL_FRAGMENT_SHADER, TEXT_FRAGMENT_SHADER);

    this->m_program = glCreateProgram ();
    glAttachShader (this->m_program, vertex);
    glAttachShader (this->m_program, fragment);
    glLinkProgram (this->m_program);

    glDeleteShader (vertex);
    glDeleteShader (fragment);

    GLint result = GL_FALSE;
    glGetProgramiv (this->m_program, GL_LINK_STATUS, &result);
    if (result == GL_FALSE) {
	GLint length = 0;
	glGetProgramiv (this->m_program, GL_INFO_LOG_LENGTH, &length);
	std::string log (length, '\0');
	glGetProgramInfoLog (this->m_program, length, nullptr, log.data ());
	sLog.exception ("Text shader linking failed: ", log);
    }

    this->m_positionLocation = glGetAttribLocation (this->m_program, "a_Position");
    this->m_texCoordLocation = glGetAttribLocation (this->m_program, "a_TexCoord");
    this->m_mvpLocation = glGetUniformLocation (this->m_program, "g_ModelViewProjectionMatrix");
    this->m_colorLocation = glGetUniformLocation (this->m_program, "u_Color");
    this->m_textureLocation = glGetUniformLocation (this->m_program, "u_Texture");

    glGenBuffers (1, &this->m_vbo);
    glGenVertexArrays (1, &this->m_vao);
}

std::vector<std::string> CText::splitLines (const std::string& text) {
    std::vector<std::string> lines;
    std::stringstream stream (text);
    std::string line;
    while (std::getline (stream, line)) {
	if (!line.empty () && line.back () == '\r') {
	    line.pop_back ();
	}
	lines.push_back (line);
    }
    if (lines.empty ()) {
	lines.emplace_back ();
    }
    return lines;
}

std::vector<unsigned char> CText::loadFontData () {
    if (this->m_cachedFontKey == this->m_text.font && !this->m_cachedFontData.empty ()) {
	return this->m_cachedFontData;
    }

    auto cacheFontData = [this] (std::vector<unsigned char> data) {
	this->m_cachedFontKey = this->m_text.font;
	this->m_cachedFontData = std::move (data);
	return this->m_cachedFontData;
    };

    if (!this->m_text.font.empty ()) {
	try {
	    auto stream = this->getAssetLocator ().read (this->m_text.font);
	    return cacheFontData ({ std::istreambuf_iterator<char> (*stream), std::istreambuf_iterator<char> () });
	} catch (const std::exception& e) {
	    sLog.debug ("Text asset font ", this->m_text.font, " not available: ", e.what ());
	} catch (...) {
	    sLog.debug ("Text asset font ", this->m_text.font, " not available: unknown exception");
	}
    }

    const std::filesystem::path requested = this->m_text.font;
    if (!requested.empty ()) {
	auto direct = readFilesystemFont (requested);
	if (!direct.empty ()) {
	    return cacheFontData (std::move (direct));
	}
    }

    const bool preferSymbols = this->m_text.font.rfind ("systemfont_", 0) == 0;

    for (const auto& fallback : findFallbackFonts (this->m_text.font, preferSymbols)) {
	auto data = readFilesystemFont (fallback);
	if (!data.empty ()) {
	    if (!this->m_text.font.empty ()) {
		sLog.out ("Text font ", this->m_text.font, " not found; using ", fallback.string ());
	    }
	    return cacheFontData (std::move (data));
	}
    }

    return {};
}

CText::ResolvedTransform CText::resolveTransform (const Object& object, const int depth) const {
    constexpr int kMaxParentDepth = 32;
    glm::vec3 origin = object.origin->value->getVec3 ();
    glm::vec3 scale = glm::vec3 (1.0f);
    float angle = 0.0f;

    if (object.is<Image> ()) {
	const auto* image = object.as<Image> ();
	scale = image->scale->value->getVec3 ();
	angle = image->angles->value->getVec3 ().z;
    } else if (object.is<Text> ()) {
	const auto* text = object.as<Text> ();
	scale = text->scale->value->getVec3 ();
	angle = text->angles->value->getVec3 ().z;
    } else {
	scale = object.groupScale->value->getVec3 ();
	angle = object.groupAngles->value->getVec3 ().z;
    }

    if (!object.parent.has_value ()) {
	return { origin, scale, angle };
    }

    if (depth >= kMaxParentDepth) {
	sLog.error ("Parent transform chain is too deep; possible cycle at text object id=", object.id);
	return { origin, scale, angle };
    }

    const auto* parentObject = this->getScene ().getObject (object.parent.value ());
    if (parentObject == nullptr) {
	return { origin, scale, angle };
    }

    const auto& parent = parentObject->getObject ();
    const auto parentTransform = this->resolveTransform (parent, depth + 1);
    const glm::vec2 local = rotateVec2 ({ origin.x * parentTransform.scale.x, origin.y * parentTransform.scale.y }, parentTransform.angle);
    origin.x = parentTransform.origin.x + local.x;
    origin.y = parentTransform.origin.y + local.y;
    origin.z = parentTransform.origin.z + origin.z * parentTransform.scale.z;
    scale *= parentTransform.scale;
    angle += parentTransform.angle;

    return { origin, scale, angle };
}

bool CText::updateTexture (const std::string& text, float pointSize, const std::string& alignment) {
    auto fontData = this->loadFontData ();
    if (fontData.empty ()) {
	sLog.error ("Cannot render text without font data for ", this->m_text.name);
	return false;
    }

    FT_Library library = nullptr;
    if (FT_Init_FreeType (&library) != 0) {
	sLog.error ("Cannot initialize FreeType for text object ", this->m_text.name);
	return false;
    }

    FT_Face face = nullptr;
    if (FT_New_Memory_Face (library, fontData.data (), static_cast<FT_Long> (fontData.size ()), 0, &face) != 0) {
	FT_Done_FreeType (library);
	sLog.error ("Cannot load font ", this->m_text.font, " for text object ", this->m_text.name);
	return false;
    }

    const int pixelSize = static_cast<int> (glm::clamp (std::round (pointSize * 5.0f), 6.0f, 512.0f));
    FT_Set_Pixel_Sizes (face, 0, static_cast<FT_UInt> (pixelSize));

    const auto lines = splitLines (text);
    const auto layout = measureTextLayout (face, lines, pixelSize);
    this->m_textureWidth = layout.maxWidth + layout.padding * 2;
    this->m_textureHeight = glm::max (1, layout.ascent + layout.descent + layout.lineHeight * static_cast<int> (lines.size () - 1))
	+ layout.padding * 2;
    std::vector<unsigned char> bitmap (static_cast<size_t> (this->m_textureWidth * this->m_textureHeight), 0);

    rasterizeTextLayout (face, layout, alignment, this->m_textureWidth, this->m_textureHeight, bitmap);

    FT_Done_Face (face);
    FT_Done_FreeType (library);

    if (this->m_texture == 0) {
	glGenTextures (1, &this->m_texture);
    }

    GLint previousUnpackAlignment = 4;
    glGetIntegerv (GL_UNPACK_ALIGNMENT, &previousUnpackAlignment);
    glBindTexture (GL_TEXTURE_2D, this->m_texture);
    glPixelStorei (GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D (
	GL_TEXTURE_2D,
	0,
	GL_R8,
	this->m_textureWidth,
	this->m_textureHeight,
	0,
	GL_RED,
	GL_UNSIGNED_BYTE,
	bitmap.data ()
    );
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glPixelStorei (GL_UNPACK_ALIGNMENT, previousUnpackAlignment);
    glBindTexture (GL_TEXTURE_2D, 0);

    return true;
}

std::string CText::resolveRenderText () const {
    std::string renderText = this->m_text.text->value->getString ();
    if (!renderText.empty ()) {
	return renderText;
    }

    const std::time_t now = std::time (nullptr);
    std::tm localTime {};
    localtime_r (&now, &localTime);
    std::ostringstream stream;

    if (this->m_text.name.find ("Clock") != std::string::npos) {
	stream << std::put_time (&localTime, "%H:%M:%S");
	return stream.str ();
    }
    if (this->m_text.name.find ("Date") != std::string::npos) {
	stream << std::put_time (&localTime, "%A, %B ") << localTime.tm_mday;
	return stream.str ();
    }
    if (this->m_text.name.find ("DAY DATE TIME") != std::string::npos) {
	stream << std::put_time (&localTime, "%a");
	renderText = stream.str ();
	std::ranges::transform (renderText, renderText.begin (), [] (unsigned char c) { return static_cast<char> (std::toupper (c)); });
    }
    return renderText;
}

void CText::adjustOriginForKnownLayout (glm::vec3& origin) const {
    if (this->m_text.name.find ("Canzone") != std::string::npos) {
	origin.y += 85.0f;
    } else if (this->m_text.name.find ("Artista") != std::string::npos) {
	origin.y += 75.0f;
    }
}

void CText::fitScaleToMaxWidth (glm::vec3& scale) const {
    if (this->m_text.maxWidth <= 0.0f || this->m_text.scale->value->getVec3 ().x == 0.0f) {
	return;
    }

    const float parentScaleX = std::abs (scale.x / this->m_text.scale->value->getVec3 ().x);
    const float maxSceneWidth = this->m_text.maxWidth * parentScaleX;
    const float currentSceneWidth = static_cast<float> (this->m_textureWidth) * std::abs (scale.x);
    if (maxSceneWidth > 0.0f && currentSceneWidth > maxSceneWidth) {
	const float fitScale = maxSceneWidth / currentSceneWidth;
	scale.x *= fitScale;
	scale.y *= fitScale;
    }
}

void CText::uploadTextQuad (const glm::vec3& origin, const glm::vec3& scale, float angle, const std::string& alignment) {
    float anchorSourceX = static_cast<float> (this->m_textureWidth) / 2.0f;
    if (alignment.find ("left") != std::string::npos) {
	anchorSourceX = 0.0f;
    } else if (alignment.find ("right") != std::string::npos) {
	anchorSourceX = static_cast<float> (this->m_textureWidth);
    }

    float anchorSourceY = static_cast<float> (this->m_textureHeight) / 2.0f;
    if (alignment.find ("top") != std::string::npos) {
	anchorSourceY = 0.0f;
    } else if (alignment.find ("bottom") != std::string::npos) {
	anchorSourceY = static_cast<float> (this->m_textureHeight);
    }

    const float sceneWidth = static_cast<float> (this->getScene ().getWidth ());
    const float sceneHeight = static_cast<float> (this->getScene ().getHeight ());
    const float anchorX = origin.x - sceneWidth / 2.0f;
    const float anchorY = sceneHeight / 2.0f - origin.y;

    auto point = [&] (float x, float y, float u, float v) {
	const glm::vec2 local = rotateVec2 ({ (x - anchorSourceX) * scale.x, -(y - anchorSourceY) * scale.y }, angle);
	return Vertex {
	    .x = anchorX + local.x,
	    .y = anchorY + local.y,
	    .z = 0.0f,
	    .u = u,
	    .v = v,
	};
    };

    const float width = static_cast<float> (this->m_textureWidth);
    const float height = static_cast<float> (this->m_textureHeight);
    this->m_vertices = {
	point (0.0f, 0.0f, 0.0f, 1.0f),
	point (0.0f, height, 0.0f, 0.0f),
	point (width, 0.0f, 1.0f, 1.0f),
	point (width, 0.0f, 1.0f, 1.0f),
	point (0.0f, height, 0.0f, 0.0f),
	point (width, height, 1.0f, 0.0f),
    };

    glBindBuffer (GL_ARRAY_BUFFER, this->m_vbo);
    glBufferData (
	GL_ARRAY_BUFFER,
	static_cast<GLsizeiptr> (this->m_vertices.size () * sizeof (Vertex)),
	this->m_vertices.data (),
	GL_DYNAMIC_DRAW
    );
}

void CText::updateBuffer () {
    std::string renderText = this->resolveRenderText ();

    const float pointSize = this->m_text.pointSize->value->getFloat ();
    const auto transform = this->resolveTransform (this->m_text);
    glm::vec3 origin = transform.origin;
    glm::vec3 scale = transform.scale;
    const glm::vec3 angles = { 0.0f, 0.0f, -transform.angle };
    const std::string alignment = this->m_text.alignment.empty () ? "center center" : this->m_text.alignment;

    this->adjustOriginForKnownLayout (origin);

    const bool textureDirty = !this->m_textureReady || renderText != this->m_lastText
	|| pointSize != this->m_lastPointSize || alignment != this->m_lastAlignment || this->m_text.font != this->m_lastFont;
    const bool geometryDirty = textureDirty || scale != this->m_lastScale || origin != this->m_lastOrigin
	|| angles != this->m_lastAngles;

    if (!geometryDirty) {
	return;
    }

    this->m_lastScale = scale;
    this->m_lastOrigin = origin;
    this->m_lastAngles = angles;
    this->m_vertices.clear ();

    if (renderText.empty ()) {
	this->m_textureReady = false;
	this->m_lastText = renderText;
	this->m_lastPointSize = pointSize;
	this->m_lastAlignment = alignment;
	this->m_lastFont = this->m_text.font;
	if (std::getenv ("LWE_TEXT_DEBUG") != nullptr) {
	    sLog.out ("Text debug id=", this->m_text.id, " name=", this->m_text.name, " skipped empty text");
	}
	return;
    }

    if (textureDirty) {
	if (!this->updateTexture (renderText, pointSize, alignment)) {
	    this->m_textureReady = false;
	    return;
	}
	this->m_textureReady = true;
	this->m_lastText = renderText;
	this->m_lastPointSize = pointSize;
	this->m_lastAlignment = alignment;
	this->m_lastFont = this->m_text.font;
    }

    this->fitScaleToMaxWidth (scale);

    if (std::getenv ("LWE_TEXT_DEBUG") != nullptr) {
	sLog.out (
	    "Text debug id=", this->m_text.id,
	    " name=", this->m_text.name,
	    " text=", renderText,
	    " origin=", origin.x, ",", origin.y,
	    " scale=", scale.x, ",", scale.y,
	    " angle=", angles.z,
	    " font=", this->m_text.font,
	    " alignment=", alignment
	);
    }

    this->uploadTextQuad (origin, scale, angles.z, alignment);
}

void CText::render () {
    if (!this->m_text.visible->value->getBool ()) {
	if (std::getenv ("LWE_TEXT_DEBUG") != nullptr) {
	    sLog.out ("Text debug id=", this->m_text.id, " name=", this->m_text.name, " skipped invisible");
	}
	return;
    }

    this->updateBuffer ();
    if (this->m_vertices.empty () || this->m_texture == 0) {
	return;
    }

    const glm::vec3 color = this->m_text.color->value->getVec3 ();
    const float alpha = this->m_text.alpha->value->getFloat ();
    const glm::mat4 mvp = this->getScene ().getCamera ().getProjection () * this->getScene ().getCamera ().getLookAt ();

    glUseProgram (this->m_program);
    glUniformMatrix4fv (this->m_mvpLocation, 1, GL_FALSE, glm::value_ptr (mvp));
    glUniform4f (this->m_colorLocation, color.r, color.g, color.b, alpha);
    glUniform1i (this->m_textureLocation, 0);

    GLint previousActiveTexture = GL_TEXTURE0;
    GLint previousVertexArray = 0;
    glGetIntegerv (GL_ACTIVE_TEXTURE, &previousActiveTexture);
    glGetIntegerv (GL_VERTEX_ARRAY_BINDING, &previousVertexArray);

    glActiveTexture (GL_TEXTURE0);
    glBindTexture (GL_TEXTURE_2D, this->m_texture);

    const GLboolean wasBlendEnabled = glIsEnabled (GL_BLEND);
    const GLboolean wasDepthTestEnabled = glIsEnabled (GL_DEPTH_TEST);
    GLint blendSrcRgb = GL_ONE;
    GLint blendDstRgb = GL_ZERO;
    GLint blendSrcAlpha = GL_ONE;
    GLint blendDstAlpha = GL_ZERO;
    glGetIntegerv (GL_BLEND_SRC_RGB, &blendSrcRgb);
    glGetIntegerv (GL_BLEND_DST_RGB, &blendDstRgb);
    glGetIntegerv (GL_BLEND_SRC_ALPHA, &blendSrcAlpha);
    glGetIntegerv (GL_BLEND_DST_ALPHA, &blendDstAlpha);

    glEnable (GL_BLEND);
    glBlendFuncSeparate (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable (GL_DEPTH_TEST);

    glBindVertexArray (this->m_vao);
    glBindBuffer (GL_ARRAY_BUFFER, this->m_vbo);
    glEnableVertexAttribArray (this->m_positionLocation);
    glVertexAttribPointer (this->m_positionLocation, 3, GL_FLOAT, GL_FALSE, sizeof (Vertex), nullptr);
    glEnableVertexAttribArray (this->m_texCoordLocation);
    glVertexAttribPointer (
	this->m_texCoordLocation,
	2,
	GL_FLOAT,
	GL_FALSE,
	sizeof (Vertex),
	reinterpret_cast<void*> (offsetof (Vertex, u))
    );
    glDrawArrays (GL_TRIANGLES, 0, static_cast<GLsizei> (this->m_vertices.size ()));
    glDisableVertexAttribArray (this->m_texCoordLocation);
    glDisableVertexAttribArray (this->m_positionLocation);
    glBindBuffer (GL_ARRAY_BUFFER, 0);
    glBindVertexArray (static_cast<GLuint> (previousVertexArray));
    glBindTexture (GL_TEXTURE_2D, 0);
    glUseProgram (0);
    glActiveTexture (static_cast<GLenum> (previousActiveTexture));
    glBlendFuncSeparate (blendSrcRgb, blendDstRgb, blendSrcAlpha, blendDstAlpha);
    if (wasBlendEnabled) {
	glEnable (GL_BLEND);
    } else {
	glDisable (GL_BLEND);
    }
    if (wasDepthTestEnabled) {
	glEnable (GL_DEPTH_TEST);
    } else {
	glDisable (GL_DEPTH_TEST);
    }
}

const Text& CText::getText () const { return this->m_text; }
