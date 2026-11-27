#include "CText.h"
#include "TextMsdf.h"
#include "WallpaperEngine/Render/Depth.h"
#include "CImage.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <limits>
#include <mutex>
#include <stdexcept>
#include <vector>

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H
#include <fontconfig/fontconfig.h>
#include <hb.h>
#include <hb-ft.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "WallpaperEngine/Data/Model/DynamicValue.h"
#include "WallpaperEngine/Data/Model/Effect.h"
#include "WallpaperEngine/Data/Model/Material.h"
#include "WallpaperEngine/Data/Model/Object.h"
#include "WallpaperEngine/Data/Model/UserSetting.h"
#include "WallpaperEngine/Data/Parsers/MaterialParser.h"
#include "WallpaperEngine/Logging/Log.h"
#include "WallpaperEngine/Render/CFBO.h"
#include "WallpaperEngine/Render/Camera.h"
#include "WallpaperEngine/Render/FBOProvider.h"
#include "WallpaperEngine/Render/Objects/CRenderable.h"
#include "WallpaperEngine/Render/Objects/Effects/CPass.h"
#include "WallpaperEngine/Render/Wallpapers/CScene.h"
#include "WallpaperEngine/Scripting/ScriptEngine.h"

using namespace WallpaperEngine::Render::Objects;

namespace {
// Fall back to a system font if the authored embedded font cannot be loaded.
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
out vec2 vPosition;
void main() {
    vUV = aUV;
    vPosition = aPos;
    gl_Position = uMVP * vec4(aPos, 0.0, 1.0);
}
)glsl";

const char* kFragmentShader = R"glsl(
#version 330 core
in vec2 vUV;
uniform sampler2D uTexture;
uniform vec4 uColor;
uniform bool uSolid;
uniform bool uMsdf;
uniform ivec3 uStyleFlags;
uniform vec4 uRenderVars[4];
uniform vec2 uTextureSize;
out vec4 FragColor;
float distanceValue(vec3 channels) {
    return max(min(channels.r,channels.g),min(max(channels.r,channels.g),channels.b))-.5;
}
float coverage(float distance, float threshold, float radius) {
    if (uStyleFlags.y != 0 || uStyleFlags.z != 0) {
        float halfWidth=max(radius,.5);
        return smoothstep(-halfWidth,halfWidth,distance+threshold-.5);
    }
    return clamp(distance+threshold,0.,1.);
}
void main() {
    if (uSolid) { FragColor=uColor; return; }
    if (!uMsdf) { FragColor=vec4(uColor.rgb,uColor.a*texture(uTexture,vUV).r); return; }
    float range=uRenderVars[0].x;
    vec2 dx=dFdx(vUV),dy=dFdy(vUV);
    float screenRange=max(.5*dot(vec2(range)/uTextureSize,vec2(1.)/sqrt(dx*dx+dy*dy)),1.);
    bool blurred=uStyleFlags.y != 0;
    float distance=distanceValue(texture(uTexture,vUV).rgb)*(blurred ? range : screenRange);
    float width=uRenderVars[0].y*(blurred ? 1. : screenRange/range);
    float radius=blurred ? uRenderVars[0].z : 0.;
    float fill=coverage(distance,.5,radius);
    float alpha=fill*uColor.a;
    vec3 color=uColor.rgb;
    if (uStyleFlags.x != 0) {
        alpha=coverage(distance,.5+width,radius)*uColor.a;
        color=mix(uRenderVars[1].xyz,color,fill);
    }
    if (uStyleFlags.z != 0) {
        vec2 offset=vec2(uRenderVars[1].w,uRenderVars[2].w)/uTextureSize;
        float shadowDistance=range*distanceValue(texture(uTexture,vUV-offset).rgb);
        float threshold=.5+(uStyleFlags.x != 0 ? uRenderVars[0].y : 0.);
        float shadowAlpha=clamp(uRenderVars[3].x*coverage(shadowDistance,threshold,uRenderVars[0].w),0.,1.)*uColor.a;
        float combinedAlpha=alpha+shadowAlpha*(1.-alpha);
        color=(color*alpha+uRenderVars[2].xyz*shadowAlpha*(1.-alpha))/max(combinedAlpha,1e-6);
        alpha=combinedAlpha;
    }
    FragColor=vec4(color,alpha);
}
)glsl";

// Composite the already colored effect result, or initialize its transparent
// background from the corresponding position in the scene.
const char* kFragmentShaderRGBA = R"glsl(
#version 330 core
in vec2 vUV;
in vec2 vPosition;
uniform sampler2D uTexture;
uniform vec4 uColor;
uniform bool uCopyBackground;
uniform mat4 uSceneMVP;
out vec4 FragColor;
void main() {
    if (uCopyBackground) {
        vec4 projected = uSceneMVP * vec4(vPosition, 0.0, 1.0);
        FragColor = vec4(texture(uTexture, projected.xy / projected.w * 0.5 + 0.5).rgb, 0.0);
    } else {
        FragColor = texture(uTexture, vUV) * uColor;
    }
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

uint32_t WallpaperEngine::Render::Objects::nextUtf8Codepoint (const std::string& text, size_t& offset) {
    const auto lead = static_cast<uint8_t> (text[offset]);
    int continuations;
    uint32_t code;

    if (lead < 0x80) {
	continuations = 0;
	code = lead;
    } else if ((lead & 0xE0) == 0xC0) {
	continuations = 1;
	code = lead & 0x1Fu;
    } else if ((lead & 0xF0) == 0xE0) {
	continuations = 2;
	code = lead & 0x0Fu;
    } else if ((lead & 0xF8) == 0xF0) {
	continuations = 3;
	code = lead & 0x07u;
    } else {
	// stray continuation byte or invalid lead
	offset++;
	return 0xFFFD;
    }

    if (offset + continuations >= text.size ()) {
	// truncated sequence at end of string
	offset++;
	return 0xFFFD;
    }

    for (int i = 1; i <= continuations; i++) {
	const auto cont = static_cast<uint8_t> (text[offset + i]);
	if ((cont & 0xC0) != 0x80) {
	    offset++;
	    return 0xFFFD;
	}
	code = (code << 6) | (cont & 0x3Fu);
    }

    offset += continuations + 1;
    return code;
}

std::vector<TextShapedGlyph> WallpaperEngine::Render::Objects::shapeTextLine (
    const std::string& text, const std::function<FT_Face (uint32_t)>& fontForGlyph
) {
    std::vector<uint32_t> codes;
    std::vector<size_t> offsets;
    std::vector<FT_Face> faces;
    for (size_t offset = 0; offset < text.size ();) {
        offsets.push_back (offset);
        const auto code = nextUtf8Codepoint (text, offset);
        codes.push_back (code);
        faces.push_back (fontForGlyph (code));
    }
    std::vector<TextShapedGlyph> glyphs;
    for (size_t start = 0; start < codes.size ();) {
        size_t end = start + 1;
        while (end < codes.size () && faces[end] == faces[start]) end++;
        const auto face = faces[start];
        FT_Set_Transform (face, nullptr, nullptr);
        hb_font_t* font = hb_ft_font_create (face, nullptr);
        // The native hb-ft adapter uses unhinted advances; outline/bitmap loads
        // below still retain their existing hinting.
        hb_ft_font_set_load_flags (font, FT_LOAD_NO_HINTING);
        hb_buffer_t* buffer = hb_buffer_create ();
        hb_buffer_add_utf32 (buffer, codes.data () + start, end - start, 0, end - start);
        hb_buffer_guess_segment_properties (buffer);
        hb_shape (font, buffer, nullptr, 0);
        unsigned int count;
        const auto infos = hb_buffer_get_glyph_infos (buffer, &count);
        const auto positions = hb_buffer_get_glyph_positions (buffer, nullptr);
        for (unsigned int i = 0; i < count; i++) {
            const auto source = start + infos[i].cluster;
            const int xOffset = positions[i].x_offset >> 6;
            const int yOffset = positions[i].y_offset >> 6;
            float right = static_cast<float> (xOffset);
            if (FT_Load_Glyph (face, infos[i].codepoint, FT_LOAD_NO_BITMAP) == 0) {
                FT_Glyph glyph;
                if (FT_Get_Glyph (face->glyph, &glyph) == 0) {
                    FT_BBox bounds;
                    FT_Glyph_Get_CBox (glyph, FT_GLYPH_BBOX_PIXELS, &bounds);
                    FT_Done_Glyph (glyph);
                    right += static_cast<float> (bounds.xMax);
                }
            }
            glyphs.push_back ({ face, infos[i].codepoint, codes[source], offsets[source], offsets[start],
                static_cast<float> (positions[i].x_advance >> 6), right, xOffset, yOffset,
                (infos[i].mask & HB_GLYPH_FLAG_UNSAFE_TO_BREAK) == 0 });
        }
        hb_buffer_destroy (buffer);
        hb_font_destroy (font);
        start = end;
    }
    return glyphs;
}

std::vector<TextShapedLine> WallpaperEngine::Render::Objects::layoutShapedTextLines (
    const std::string& text, const TextLayoutLimits& limits,
    const std::function<std::vector<TextShapedGlyph> (const std::string&)>& shape, const float spacing
) {
    const auto whitespace = [] (uint32_t code) { return code == ' ' || code == '\t' || code == '\r'; };
    std::vector<std::string> pending;
    for (size_t paragraph = 0, end = 0; end <= text.size (); end++) {
        if (end != text.size () && text[end] != '\n') continue;
        size_t paragraphEnd = end;
        if (paragraphEnd > paragraph && text[paragraphEnd - 1] == '\r') paragraphEnd--;
        pending.push_back (text.substr (paragraph, paragraphEnd - paragraph));
        paragraph = end + 1;
    }
    std::vector<TextShapedLine> lines;
    for (size_t row = 0; row < pending.size ();) {
        auto glyphs = shape (pending[row]);
        // Source-cut wrapping is proven for forward horizontal clusters. Keep
        // other output orders intact so an unsupported cut cannot repeat the row.
        const bool forwardClusters = std::is_sorted (glyphs.begin (), glyphs.end (),
            [] (const auto& first, const auto& second) { return first.cluster < second.cluster; });
        float pen = 0.0f;
        size_t stop = 0;
        for (; stop < glyphs.size (); stop++) {
            const auto& glyph = glyphs[stop];
            if (forwardClusters && limits.width > 0.0f && stop > 0 && !whitespace (glyph.code)
                && glyph.cluster != glyphs[stop - 1].cluster && pen + glyph.right > limits.width) break;
            pen += glyph.advance + spacing;
        }
        if (stop < glyphs.size ()) {
            size_t cut = glyphs[stop].cluster;
            while (cut > 1 && whitespace (static_cast<unsigned char> (pending[row][cut]))) cut--;
            for (size_t previous = cut; previous > 0; previous--) {
                if (whitespace (static_cast<unsigned char> (pending[row][previous - 1]))) {
                    cut = previous - 1;
                    break;
                }
            }
            size_t tail = cut;
            while (tail < pending[row].size () && whitespace (static_cast<unsigned char> (pending[row][tail]))) tail++;
            pending.insert (pending.begin () + row + 1, pending[row].substr (tail));
            pending[row].resize (cut);
            while (!pending[row].empty () && whitespace (static_cast<unsigned char> (pending[row].back ())))
                pending[row].pop_back ();
            cut = pending[row].size ();
            glyphs.resize (stop); // the overflowing glyph was never accepted
            size_t retained = glyphs.size ();
            while (retained > 0 && glyphs[retained - 1].cluster >= cut) retained--;
            bool safe = true;
            if (retained > 0) {
                const auto run = glyphs[retained - 1].runStart;
                // Native ignores removed complete later runs. Within the last
                // surviving run, the first popped output record supplies the flag.
                for (size_t i = glyphs.size (); i > retained; i--)
                    if (glyphs[i - 1].runStart == run) safe = glyphs[i - 1].safeBreak;
            }
            glyphs.resize (retained);
            if (!glyphs.empty () && !safe) {
                continue;
            }
        }
        lines.push_back ({ pending[row], std::move (glyphs) });
        row++;
    }
    if (limits.rows > 0 && lines.size () > limits.rows) {
        lines.resize (limits.rows);
        if (limits.ellipsis && !lines.back ().text.ends_with ("…")) {
            auto& last = lines.back ();
            const auto measure = [&] (const std::string& value) {
                float pen = 0.0f, right = 0.0f;
                for (const auto& glyph : shape (value)) {
                    right = pen + glyph.right;
                    pen += glyph.advance;
                }
                return right;
            };
            const std::string original = last.text;
            while (!last.text.empty () && (whitespace (static_cast<unsigned char> (last.text.back ()))
                || (limits.width > 0.0f && measure (last.text + "…") > limits.width))) {
                size_t offset = last.text.size () - 1;
                while (offset > 0 && (static_cast<unsigned char> (last.text[offset]) & 0xC0) == 0x80) offset--;
                last.text.resize (offset);
            }
            if (last.text.empty () && !original.empty ()) {
                last.text = original;
            } else {
                last.text += "…";
                last.glyphs = shape (last.text);
            }
        }
    }
    return lines;
}

std::vector<std::string> WallpaperEngine::Render::Objects::layoutTextLines (
    const std::string& text, const TextLayoutLimits& limits, const std::function<TextGlyphMetrics (uint32_t)>& glyphMetrics,
    const float spacing
) {
    const auto shape = [&] (const std::string& row) {
        std::vector<TextShapedGlyph> glyphs;
        for (size_t offset = 0; offset < row.size ();) {
            const auto cluster = offset;
            const auto code = nextUtf8Codepoint (row, offset);
            const auto metrics = glyphMetrics (code);
            glyphs.push_back ({ nullptr, code, code, cluster, 0, metrics.advance, metrics.right, 0, 0, true });
        }
        return glyphs;
    };
    std::vector<std::string> lines;
    for (auto& line : layoutShapedTextLines (text, limits, shape, spacing)) lines.push_back (std::move (line.text));
    return lines;
}

glm::vec2 WallpaperEngine::Render::Objects::computeTextAlignmentOffset (
    const std::string& horizontalAlign, const std::string& verticalAlign, const glm::vec4& glyphBounds,
    const float ascender, const float descender, const float lineSpacing, const size_t lineCount
) {
    const float lineLeft = std::min (0.0f, glyphBounds.x);
    const float lineRight = std::max (0.0f, glyphBounds.z);
    const float width = lineRight - lineLeft;
    const float boundsCenterY = (glyphBounds.y + glyphBounds.w) * 0.5f;
    const float followingRows = static_cast<float> (lineCount > 0 ? lineCount - 1 : 0);

    // Native mesh bounds start at the pen origin, including positive bearings.
    float x = (glyphBounds.x + glyphBounds.z - lineLeft - lineRight) * 0.5f;
    if (horizontalAlign == "left") {
	x += width * 0.5f;
    } else if (horizontalAlign == "right") {
	x -= width * 0.5f;
    }

    float verticalReference;
    if (verticalAlign == "top") {
	verticalReference = ascender;
    } else if (verticalAlign == "bottom") {
	verticalReference = descender - followingRows * lineSpacing;
    } else {
	verticalReference = (ascender - followingRows * lineSpacing) * 0.5f;
    }

    return { x, boundsCenterY - verticalReference };
}

TextEffectLayout WallpaperEngine::Render::Objects::computeTextEffectLayout (
    const std::string& horizontalAlign, const std::string& verticalAlign, const glm::vec4& glyphBounds,
    const float ascender, const float descender, const float lineSpacing, const size_t lineCount,
    const glm::vec2& padding
) {
    // wallpaper64.exe's font generator starts every mesh bound at zero, then expands
    // the vertical range to [ascender - rows * lineSpacing, ascender]. This is the
    // generated line box stored at mesh offsets 0x90..0x9c and used to size effects.
    const float rows = static_cast<float> (std::max<size_t> (lineCount, 1));
    const glm::vec4 lineBounds = {
	std::min (0.0f, glyphBounds.x),
	std::min ({ 0.0f, glyphBounds.y, ascender - rows * lineSpacing }),
	std::max (0.0f, glyphBounds.z),
	std::max ({ 0.0f, glyphBounds.w, ascender }),
    };

    const glm::vec2 glyphAlignment = computeTextAlignmentOffset (
	horizontalAlign, verticalAlign, glyphBounds, ascender, descender, lineSpacing, lineCount
    );
    const glm::vec2 surfaceAlignment = computeTextAlignmentOffset (
	horizontalAlign, verticalAlign, lineBounds, ascender, descender, lineSpacing, lineCount
    );
    const glm::vec2 glyphDown = { glyphAlignment.x, -glyphAlignment.y };
    const glm::vec2 surfaceDown = { surfaceAlignment.x, -surfaceAlignment.y };

    return {
	.surfaceSize = {
	    lineBounds.z - lineBounds.x + std::min (padding.x, 512.0f) * 2.0f,
	    lineBounds.w - lineBounds.y + std::min (padding.y, 512.0f) * 2.0f,
	},
	.rasterOffset = glyphDown - surfaceDown,
	.compositeOffset = surfaceDown,
    };
}

namespace WallpaperEngine::Render::Objects {
/**
 * CPass needs a CRenderable owner for its uniform sources (brightness/alpha/color) and the
 * texture-0 fallback. Text effect passes run value-neutral — brightness and object alpha are
 * applied when rasterizing the text input — so every getter
 * returns a constant and the texture is the rasterized-text FBO.
 */
class CTextEffectHost final : public CRenderable {
public:
    CTextEffectHost (Wallpapers::CScene& scene, const Text& text, const Material& material,
	const glm::vec2& surfaceOffset) :
	CObject (scene, text), CRenderable (scene, text, material), m_surfaceOffset (surfaceOffset) { }

    void setTexture (std::shared_ptr<const TextureProvider> texture) { this->m_texture = std::move (texture); }

    [[nodiscard]] glm::mat4 resolveLayerModelMatrix () const override {
	return glm::translate (this->resolveWorldMatrix (), glm::vec3 (m_surfaceOffset.x, -m_surfaceOffset.y, 0.0f));
    }

    [[nodiscard]] const float& getBrightness () const override { return m_one; }
    [[nodiscard]] const float& getUserAlpha () const override { return m_one; }
    [[nodiscard]] const float& getAlpha () const override { return m_one; }
    [[nodiscard]] const glm::vec3& getColor () const override { return m_white3; }
    [[nodiscard]] const glm::vec4& getColor4 () const override { return m_white4; }
    [[nodiscard]] const glm::vec3& getCompositeColor () const override { return m_white3; }

private:
    const glm::vec2& m_surfaceOffset;
    const float m_one = 1.0f;
    const glm::vec3 m_white3 = glm::vec3 (1.0f);
    const glm::vec4 m_white4 = glm::vec4 (1.0f);
};
} // namespace WallpaperEngine::Render::Objects

CText::CText (Wallpapers::CScene& scene, const Text& text) :
    CObject (scene, text), ScriptableObject (scene, text), m_text (text) {
    this->registerProperty ("color", *text.color);
    this->registerProperty ("alpha", *text.alpha);
    this->registerProperty ("colorBlendMode", *text.colorBlendMode);
    this->registerProperty ("ledsource", *text.ledSource);
    this->registerProperty ("origin", *text.origin);
    this->registerProperty ("scale", *text.scale);
    this->registerProperty ("visible", *text.visible);
    this->registerProperty ("perspective", *text.perspective);
    this->registerProperty ("depthtest", *text.depthTest);
    this->registerProperty ("msdf", *text.msdf);
    this->registerProperty ("outline", *text.outline);
    this->registerProperty ("outlinethickness", *text.outlineWidth);
    this->registerProperty ("outlinecolor", *text.outlineColor);
    this->registerProperty ("blur", *text.blur);
    this->registerProperty ("blursize", *text.blurRadius);
    this->registerProperty ("dropshadow", *text.dropShadow);
    this->registerProperty ("dropshadowsize", *text.dropShadowRadius);
    this->registerProperty ("dropshadowopacity", *text.dropShadowOpacity);
    this->registerProperty ("dropshadowoffset", *text.dropShadowOffset);
    this->registerProperty ("dropshadowcolor", *text.dropShadowColor);
    this->registerProperty ("pointSize", *text.pointSize);
    this->registerProperty ("text", *text.text);
    this->registerProperty ("spacing", *text.spacing);
    this->registerProperty ("padding", *text.padding);
    this->registerProperty ("opaquebackground", *text.opaqueBackground);
    this->registerProperty ("backgroundcolor", *text.backgroundColor);
    this->registerProperty ("backgroundbrightness", *text.backgroundBrightness);
    this->registerProperty ("limitwidth", *text.limitWidth);
    this->registerProperty ("maxwidth", *text.maxWidth);
    this->registerProperty ("limitrows", *text.limitRows);
    this->registerProperty ("maxrows", *text.maxRows);
    this->registerProperty ("limituseellipsis", *text.limitUseEllipsis);

    // Effect parameters carry property scripts just like image effects. Queue them
    // once for this layer, so rebuilding changing glyphs does not restart fades.
    for (const auto& effect : text.effects) {
	registerProperty ("visible_fx" + std::to_string (effect->id), *effect->visible);
	int passIndex = 0;
	for (const auto& pass : effect->passOverrides) {
	    const auto scope = "effect_" + std::to_string (effect->id) + "_pass"
		+ std::to_string (passIndex) + "_";
	    for (const auto& [name, setting] : pass->constants) {
		if (setting->value != nullptr && (setting->value->getScriptSource ().has_value ()
		    || setting->animation != nullptr)) {
		    registerProperty (scope + name, *setting, scope);
		    if (setting->animation != nullptr && !setting->animation->name.empty ())
			registerAnimation (setting->animation->name, *setting->animation);
		}
	    }
	    ++passIndex;
	}
    }
}

CText::~CText () {
    destroyEffectChain ();
    if (m_glyphVbo != 0) glDeleteBuffers (1, &m_glyphVbo);
    if (m_glyphVao != 0) glDeleteVertexArrays (1, &m_glyphVao);
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
    for (const auto& [key, face] : m_fallbackFaces) {
	FT_Done_Face (face);
    }
    if (m_ftLibrary != nullptr) {
	FT_Done_FreeType (m_ftLibrary);
    }
}

void CText::setup () {
    const auto& text = m_text.text->value->getString ();

    if (!initFreeType ()) {
	return;
    }

    if (!loadEmbeddedFont () && !loadSystemFont ()) {
	return;
    }

    m_lastFontSize = currentFontSize ();
    FT_Set_Char_Size (m_ftFace, 0, m_lastFontSize, 300, 300);

    buildShader ();
    // Initially empty text can be written later by any layer's script.
    rebuildTextureFrom (text.empty () ? std::string (" ") : text);

    try {
	setupEffectChain ();
    } catch (const std::exception& e) {
	// A failed optional effect degrades to plain text instead of killing the layer.
	sLog.error ("CText: effect chain failed for '", m_text.name, "', rendering without effects: ", e.what ());
	destroyEffectChain ();
    }

    // m_vao is created lazily on the render thread (see ensureVao); the VBO is the
    // part guaranteed to exist after a successful build, even on the async worker
    m_valid = m_texture != 0 && m_program != 0 && m_vbo != 0;
}

void CText::setupEffectChain () {
    m_colorBlendMode = m_text.colorBlendMode->value->getInt ();
    const bool sampledByLayer = std::ranges::any_of (getScene ().getScene ().objects, [this] (const auto& object) {
	return std::ranges::find (object->dependencies, getId ()) != object->dependencies.end ();
    });
    const auto& fog = getScene ().getScene ().fog;
    const bool sceneForcesOffscreen = fog.distance.enabled->value->getBool () || fog.height.enabled->value->getBool ();
    if (m_text.effects.empty () && !sampledByLayer && !sceneForcesOffscreen && !m_text.ledSource->value->getBool ()
        && (m_colorBlendMode == 0 || m_colorBlendMode == 31)) return;

    // wallpaper64.exe sizes this surface from the generated font line box plus the
    // authored padding. The serialized editor size is not part of text rendering.
    const glm::vec2 surface = m_effectSurface;

    m_effectMaterial = Data::Parsers::MaterialParser::load (
	this->getScene ().getScene ().project, "materials/util/effectpassthrough.json"
    );
    m_effectHost = std::make_unique<CTextEffectHost> (
	this->getScene (), m_text, *m_effectMaterial, m_effectCompositeOffset
    );

    // Keep working surfaces private. Consumers need the completed result, not
    // whichever ping-pong surface happens to contain it this frame.
    m_fboA = std::make_shared<CFBO> (
        "_text_raster_" + std::to_string (getId ()), getScene ().getColorFormat (),
        TextureFlags_ClampUVs, 1, surface.x, surface.y, surface.x, surface.y
    );
    m_fboB = std::make_shared<CFBO> (
        "_text_pingpong_" + std::to_string (getId ()), getScene ().getColorFormat (),
        TextureFlags_ClampUVs, 1, surface.x, surface.y, surface.x, surface.y
    );
    const auto targetSize = FBOProvider::calculateTargetSize (surface, getScene ().getRenderScale ());
    if (m_publishedFBO == nullptr) {
        const std::string base = "_rt_imageLayerComposite_" + std::to_string (getId ());
        m_publishedFBO = std::make_shared<CFBO> (
            base + "_a", getScene ().getColorFormat (), TextureFlags_ClampUVs, 1,
            targetSize.x, targetSize.y, targetSize.x, targetSize.y
        );
        getScene ().publish (m_publishedFBO->getName (), m_publishedFBO);
        getScene ().publish (base + "_b", m_publishedFBO);
    } else {
        m_publishedFBO->resize (targetSize.x, targetSize.y);
    }
    m_effectHost->setTexture (m_fboA);

    // NDC quad + texcoords for the intermediate passes, same layout as CImage's pass buffers
    constexpr GLfloat passPosition[]
	= { -1.0, 1.0, 0.0f, -1.0, -1.0, 0.0f, 1.0, 1.0, 0.0f, 1.0, 1.0, 0.0f, -1.0, -1.0, 0.0f, 1.0, -1.0, 0.0f };
    constexpr GLfloat passTexcoord[] = { 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f };

    glGenBuffers (1, &m_ndcPosition);
    glBindBuffer (GL_ARRAY_BUFFER, m_ndcPosition);
    glBufferData (GL_ARRAY_BUFFER, sizeof (passPosition), passPosition, GL_STATIC_DRAW);
    glGenBuffers (1, &m_passTexCoord);
    glBindBuffer (GL_ARRAY_BUFFER, m_passTexCoord);
    glBufferData (GL_ARRAY_BUFFER, sizeof (passTexcoord), passTexcoord, GL_STATIC_DRAW);

    // Build material/copy passes; swaps run between draws without consuming a target.
    for (const auto& effect : m_text.effects) {
	const auto fboProvider = std::make_shared<FBOProvider> (&getScene ());
	m_effectProviders.push_back (fboProvider);

	for (const auto& fbo : effect->effect->fbos) {
	    fboProvider->create (*fbo, surface);
	}
	registerEffectFunctions (*effect, fboProvider);
	std::vector<std::string> bufferNames;
	for (const auto& fbo : effect->effect->fbos) bufferNames.push_back (fbo->name);
	const auto effectBindings = std::make_shared<Effects::EffectFBOBindings> (bufferNames);

	auto curOverride = effect->passOverrides.begin ();
	const auto endOverride = effect->passOverrides.end ();
	for (const auto& effectPass : effect->effect->passes) {
	    const auto override = curOverride != endOverride
		? **curOverride
		: std::optional<std::reference_wrapper<const ImageEffectPassOverride>> (std::nullopt);
	    if (curOverride != endOverride) ++curOverride;
	    if (!effectPass->enabled) continue;
	    std::vector<const MaterialPass*> materialPasses;
	    const bool copy = !effectPass->material.has_value ();
	    if (copy) {
		if (!effectPass->command || !effectPass->source || !effectPass->target) continue;
		if (*effectPass->command == Command_Swap) {
		    m_swapCommands.push_back ({ m_effectPasses.size (), effectBindings,
			*effectPass->source, *effectPass->target, effect->visible.get () });
		    continue;
		}
		auto material = std::make_unique<MaterialPass> (MaterialPass {
		    .blending = BlendingMode_Normal, .cullmode = CullingMode_Disable,
		    .depthtest = DepthtestMode_Disabled, .depthwrite = DepthwriteMode_Disabled,
		    .shader = "commands/copy", .textures = {{ 0, *effectPass->source }},
		});
		materialPasses.push_back (material.get ());
		m_effectMaterial->passes.push_back (std::move (material));
	    } else {
		for (const auto& pass : effectPass->material.value ()->passes) materialPasses.push_back (pass.get ());
	    }

	    for (const auto* pass : materialPasses) {
		const auto target = effectPass->target.has_value ()
		    ? *effectPass->target
		    : std::optional<std::reference_wrapper<std::string>> (std::nullopt);

		auto* cpass
		    = new Effects::CPass (*m_effectHost, fboProvider, *pass, override, effectPass->binds, target, {}, true, effectBindings);
		if (copy) {
		    cpass->setCopySource (*effectPass->source);
		    m_copyPasses.insert (cpass);
		}
		cpass->setPosition (m_ndcPosition);
		cpass->setTexCoord (m_passTexCoord);
		// matrices keep CPass' shared identity defaults: intermediate passes are 1:1 blits

		m_effectPasses.push_back (cpass);
		m_passVisibility.emplace (cpass, effect->visible.get ());
	    }
	}
    }

    updateEffectVisibility ();

    // composite program: same vertex shader, RGBA fragment
    GLuint vs = compileShader (GL_VERTEX_SHADER, kVertexShader);
    GLuint fs = compileShader (GL_FRAGMENT_SHADER, kFragmentShaderRGBA);
    if (vs == 0 || fs == 0 || m_effectResult == nullptr) {
	if (vs) {
	    glDeleteShader (vs);
	}
	if (fs) {
	    glDeleteShader (fs);
	}
	destroyEffectChain ();
	return;
    }

    m_compositeProgram = glCreateProgram ();
    glAttachShader (m_compositeProgram, vs);
    glAttachShader (m_compositeProgram, fs);
    glLinkProgram (m_compositeProgram);
    glDeleteShader (vs);
    glDeleteShader (fs);

    GLint status = GL_FALSE;
    glGetProgramiv (m_compositeProgram, GL_LINK_STATUS, &status);
    if (status != GL_TRUE) {
	destroyEffectChain ();
	return;
    }

    m_cuMVP = glGetUniformLocation (m_compositeProgram, "uMVP");
    m_cuTexture = glGetUniformLocation (m_compositeProgram, "uTexture");

    // The direct glyph VBO already contains m_quadOffset. Cancel that and move it to
    // its native center inside the line-box FBO. Alignment is applied separately to
    // the whole surface by the composite quad below.
    m_baseMVP = glm::ortho (-surface.x * 0.5f, surface.x * 0.5f, -surface.y * 0.5f, surface.y * 0.5f)
	* glm::translate (glm::mat4 (1.0f), glm::vec3 (m_effectRasterOffset - m_quadOffset, 0.0f));

    // Composite the tight native surface at its font-metric alignment offset.
    const float hx = surface.x * 0.5f;
    const float hy = surface.y * 0.5f;
    const float x0 = m_effectCompositeOffset.x - hx;
    const float x1 = m_effectCompositeOffset.x + hx;
    const float y0 = m_effectCompositeOffset.y - hy;
    const float y1 = m_effectCompositeOffset.y + hy;
    const float quad[] = {
	// pos      // uv
	x0, y0, 0.0f, 0.0f, x1, y0, 1.0f, 0.0f, x1, y1, 1.0f, 1.0f,
	x0, y0, 0.0f, 0.0f, x1, y1, 1.0f, 1.0f, x0, y1, 0.0f, 1.0f,
    };

    // VBO only (shared GL object, safe on the async build worker); the VAO is created
    // lazily in ensureCompositeVao on the render thread
    glGenBuffers (1, &m_compositeVbo);
    glBindBuffer (GL_ARRAY_BUFFER, m_compositeVbo);
    glBufferData (GL_ARRAY_BUFFER, sizeof (quad), quad, GL_STATIC_DRAW);

    if (m_colorBlendMode > 0 && m_colorBlendMode != 31) {
	m_colorBlendProvider = std::make_shared<FBOProvider> (&getScene ());
	// All text draws are sequential. Share one scene snapshot instead of keeping
	// a full-resolution copy for every clock/date/media label.
	m_blendBackground = getScene ().find ("_rt_TextBlendBackground");
	if (!m_blendBackground) {
	    const auto target = getScene ().getFBO ();
	    const glm::vec2 size (target->getRealWidth (), target->getRealHeight ());
	    m_blendBackground = getScene ().create (
		"_rt_TextBlendBackground", getScene ().getColorFormat (), TextureFlags_ClampUVs, 1.0f, size, size
	    );
	}
	m_colorBlendProvider->alias ("_rt_FullFrameBuffer", m_blendBackground);
	m_colorBlendOverride = std::make_unique<ImageEffectPassOverride> ();
	m_colorBlendOverride->combos["BLENDMODE"] = m_colorBlendMode;
	static const TextureMap blendBinds { { 0, "previous" } };
	m_colorBlendPass = std::make_unique<Effects::CPass> (
	    *m_effectHost, m_colorBlendProvider, **m_effectMaterial->passes.begin (),
	    *m_colorBlendOverride, blendBinds, std::nullopt
	);
	m_colorBlendPass->setInput (m_publishedFBO);
	m_colorBlendPass->setDestination (getScene ().getFBO ());
	m_colorBlendPass->setPosition (m_ndcPosition);
	m_colorBlendPass->setTexCoord (m_passTexCoord);
	m_colorBlendPass->setModelViewProjectionMatrix (&m_colorBlendMVP);
    }
    m_effectsEnabled = true;
}

void CText::updateEffectVisibility () {
    const auto enabled = [this] (Effects::CPass* pass) {
	const auto* visible = m_passVisibility.at (pass);
	return visible->value->getBool () && !m_failedEffects.contains (visible);
    };
    size_t active = 0;
    bool changed = false;
    for (auto* pass : m_effectPasses) {
	if (!enabled (pass)) continue;
	changed |= active >= m_activeEffectPasses.size () || m_activeEffectPasses[active] != pass;
	++active;
    }
    if (!changed && active == m_activeEffectPasses.size () && m_effectResult != nullptr) return;

    // Retain hidden effects' buffers and swap state, preparing their shaders on
    // first activation. A failed optional effect is skipped as a whole.
    for (auto* pass : m_effectPasses) {
	if (!enabled (pass)) continue;
	try {
	    pass->initialize ();
	} catch (const std::exception& error) {
	    m_failedEffects.insert (m_passVisibility.at (pass));
	    sLog.error ("Disabling text effect on object ", getId (), " after shader setup failed: ", error.what ());
	}
    }
    m_activeEffectPasses.clear ();
    for (auto* pass : m_effectPasses) {
	if (enabled (pass)) m_activeEffectPasses.push_back (pass);
    }

    std::shared_ptr<const TextureProvider> asInput = m_fboA;
    std::shared_ptr<const CFBO> drawTo = m_fboB;
    std::shared_ptr<const FBOProvider> effectProvider;
    std::shared_ptr<const TextureProvider> effectInput;
    bool inTargetSequence = false;
    for (auto* pass : m_activeEffectPasses) {
	if (effectProvider != pass->getFBOProvider ()) {
	    effectProvider = pass->getFBOProvider ();
	    effectInput = nullptr;
	    inTargetSequence = false;
	}
	const auto previousDrawTo = drawTo;
	const bool copy = m_copyPasses.contains (pass);
	bool writesToTarget = false;
	if (pass->getTarget ()) {
	    const std::string& targetName = pass->getTarget ()->get ();
	    if (const auto target = effectProvider->find (targetName)) {
		if (!copy && !inTargetSequence) {
		    effectInput = asInput;
		    inTargetSequence = true;
		}
		drawTo = target;
		writesToTarget = true;
	    } else {
		sLog.error ("CText: pass target FBO '", targetName, "' not found, object ", getId ());
	    }
	}
	pass->setDestination (drawTo);
	pass->setInput (asInput);
	pass->setPreviousInput (inTargetSequence ? effectInput : nullptr);

	if (copy) {
	    drawTo = previousDrawTo;
	} else if (writesToTarget) {
	    asInput = drawTo;
	    drawTo = previousDrawTo;
	} else {
	    asInput = drawTo;
	    drawTo = drawTo == m_fboA ? m_fboB : m_fboA;
	    inTargetSequence = false;
	}
    }
    m_effectResult = std::dynamic_pointer_cast<const CFBO> (asInput);
}

void CText::destroyEffectChain () {
    m_colorBlendPass.reset ();
    m_colorBlendOverride.reset ();
    m_colorBlendProvider.reset ();
    m_blendBackground.reset ();
    m_colorBlendMode = 0;
    for (const auto* pass : m_effectPasses) {
	delete pass;
    }
    m_effectPasses.clear ();
    m_activeEffectPasses.clear ();
    m_passVisibility.clear ();
    m_copyPasses.clear ();
    m_failedEffects.clear ();
    m_effectProviders.clear ();
    m_swapCommands.clear ();
    clearEffectFunctions ();
    m_effectResult = nullptr;
    m_fboA = nullptr;
    m_fboB = nullptr;
    m_effectHost = nullptr;
    m_effectMaterial = nullptr;
    if (m_compositeProgram != 0) {
	glDeleteProgram (m_compositeProgram);
	m_compositeProgram = 0;
    }
    if (m_compositeVbo != 0) {
	glDeleteBuffers (1, &m_compositeVbo);
	m_compositeVbo = 0;
    }
    if (m_compositeVao != 0) {
	glDeleteVertexArrays (1, &m_compositeVao);
	m_compositeVao = 0;
    }
    if (m_ndcPosition != 0) {
	glDeleteBuffers (1, &m_ndcPosition);
	m_ndcPosition = 0;
    }
    if (m_passTexCoord != 0) {
	glDeleteBuffers (1, &m_passTexCoord);
	m_passTexCoord = 0;
    }
    m_effectsEnabled = false;
}

void CText::renderEffectChain (const glm::mat4& mvp, const float brightness, const float alpha, const bool drawToScene) {
    const ClipDepthScope localDepth (false);
    updateEffectVisibility ();
    const glm::vec4 color = m_text.color->value->getVec4 ();

    // the scene sets its clear color once at setup, not per frame — leaking ours would
    // make every subsequent scene clear transparent black (same pattern as CFBO's ctor)
    GLfloat previousClearColor[4];
    glGetFloatv (GL_COLOR_CLEAR_VALUE, previousClearColor);
    const GLboolean depthWasEnabled = glIsEnabled (GL_DEPTH_TEST);

    // Native clears opaque backgrounds into the effect input, then draws the
    // glyph with the layer's opacity. Its translucent material also blends alpha.
    glBindFramebuffer (GL_FRAMEBUFFER, m_fboA->getFramebuffer ());
    glViewport (0, 0, m_fboA->getRealWidth (), m_fboA->getRealHeight ());
    glColorMask (GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDisable (GL_DEPTH_TEST);
    glDisable (GL_CULL_FACE);
    if (m_text.opaqueBackground->value->getBool ()) {
	const float backgroundBrightness = getScene ().isHdr ()
	    ? m_text.backgroundBrightness->evaluateFloat (getScene ().getElapsedTime ()) : 1.0f;
	const glm::vec3 background = m_text.backgroundColor->value->getVec3 () * backgroundBrightness;
	glClearColor (background.r, background.g, background.b, alpha);
	glClear (GL_COLOR_BUFFER_BIT);
    } else {
	// Native composelayer_clearalpha preserves projected scene RGB, including
	// beneath transparent glyphs, while resetting only the input alpha.
	glDisable (GL_BLEND);
	glUseProgram (m_compositeProgram);
	const glm::mat4 rasterMVP = glm::ortho (
	    -m_effectSurface.x * 0.5f, m_effectSurface.x * 0.5f,
	    -m_effectSurface.y * 0.5f, m_effectSurface.y * 0.5f
	) * glm::translate (glm::mat4 (1.0f), glm::vec3 (-m_effectCompositeOffset, 0.0f));
	glUniformMatrix4fv (m_cuMVP, 1, GL_FALSE, glm::value_ptr (rasterMVP));
	glUniformMatrix4fv (glGetUniformLocation (m_compositeProgram, "uSceneMVP"), 1, GL_FALSE, glm::value_ptr (mvp));
	glUniform1i (glGetUniformLocation (m_compositeProgram, "uCopyBackground"), 1);
	glActiveTexture (GL_TEXTURE0);
	glBindTexture (GL_TEXTURE_2D, getScene ().getFBO ()->getTextureID (0));
	glUniform1i (m_cuTexture, 0);
	ensureCompositeVao ();
	glBindVertexArray (m_compositeVao);
	glDrawArrays (GL_TRIANGLES, 0, 6);
	glBindVertexArray (0);
	glUniform1i (glGetUniformLocation (m_compositeProgram, "uCopyBackground"), 0);
    }
    glEnable (GL_BLEND);
    glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glBlendEquationSeparate (GL_FUNC_ADD, getScene ().usesCompositionAlphaMax () ? GL_MAX : GL_FUNC_ADD);
    glDisable (GL_DEPTH_TEST);

    glUseProgram (m_program);
    glUniformMatrix4fv (m_uMVP, 1, GL_FALSE, glm::value_ptr (m_baseMVP));
    glUniform4f (m_uColor, color.r * brightness, color.g * brightness, color.b * brightness, color.a * alpha);
    glUniform1i (m_uSolid, 0);
    glActiveTexture (GL_TEXTURE0);
    glBindTexture (GL_TEXTURE_2D, m_texture);
    glUniform1i (m_uTexture, 0);
    drawGlyphs ();

    // Named simulation targets persist between frames; only the working
    // compositing surface is reset here. Authored clear functions reset state.
    glBindFramebuffer (GL_FRAMEBUFFER, m_fboB->getFramebuffer ());
    glClearColor (0.0f, 0.0f, 0.0f, 0.0f);
    glClear (GL_COLOR_BUFFER_BIT);

    auto command = m_swapCommands.begin ();
    auto pass = m_activeEffectPasses.begin ();
    for (size_t index = 0; index <= m_effectPasses.size (); ++index) {
	for (; command != m_swapCommands.end () && command->beforePass == index; ++command) {
	    if (!m_failedEffects.contains (command->visible)) command->execute ();
	}
	if (index < m_effectPasses.size () && pass != m_activeEffectPasses.end () && *pass == m_effectPasses[index]) {
	    (*pass++)->render ();
	}
    }

    // Publish the finished layer through a stable target. A later
    // clock change can resize it without leaving consumers with an old texture.
    glDisable (GL_SAMPLE_ALPHA_TO_COVERAGE);
    glBindFramebuffer (GL_FRAMEBUFFER, m_publishedFBO->getFramebuffer ());
    glViewport (0, 0, m_publishedFBO->getRealWidth (), m_publishedFBO->getRealHeight ());
    glDisable (GL_BLEND);
    glDisable (GL_DEPTH_TEST);
    glDisable (GL_CULL_FACE);
    glUseProgram (m_compositeProgram);
    const glm::mat4 publishMVP = glm::ortho (
        -m_effectSurface.x * 0.5f, m_effectSurface.x * 0.5f,
        -m_effectSurface.y * 0.5f, m_effectSurface.y * 0.5f
    ) * glm::translate (glm::mat4 (1.0f), glm::vec3 (-m_effectCompositeOffset, 0.0f));
    glUniformMatrix4fv (m_cuMVP, 1, GL_FALSE, glm::value_ptr (publishMVP));
    const GLint uColor = glGetUniformLocation (m_compositeProgram, "uColor");
    glUniform4f (uColor, 1.0f, 1.0f, 1.0f, 1.0f);
    glActiveTexture (GL_TEXTURE0);
    glBindTexture (GL_TEXTURE_2D, m_effectResult->getTextureID (0));
    glUniform1i (m_cuTexture, 0);
    ensureCompositeVao ();
    glBindVertexArray (m_compositeVao);
    glDrawArrays (GL_TRIANGLES, 0, 6);

    // Visibility controls the scene draw, not the offscreen layer used by others.
    const auto sceneTarget = this->getScene ().getActiveRenderTarget ();
    glBindFramebuffer (GL_FRAMEBUFFER, sceneTarget->getDrawFramebuffer ());
    glViewport (0, 0, sceneTarget->getRealWidth (), sceneTarget->getRealHeight ());
    glColorMask (GL_TRUE, GL_TRUE, GL_TRUE, getScene ().isRenderingToComposition () ? GL_TRUE : GL_FALSE);
    glBlendEquationSeparate (GL_FUNC_ADD, getScene ().usesCompositionAlphaMax () ? GL_MAX : GL_FUNC_ADD);
    const ClipDepthScope sceneDepth (getScene ().getCamera ().usesReverseDepth ());
    if (drawToScene && m_colorBlendPass) {
	const auto width = sceneTarget->getRealWidth ();
	const auto height = sceneTarget->getRealHeight ();
	if (m_blendBackground->getRealWidth () != width || m_blendBackground->getRealHeight () != height) {
	    m_blendBackground->resize (width, height);
	}
	glBindFramebuffer (GL_READ_FRAMEBUFFER, sceneTarget->getFramebuffer ());
	glBindFramebuffer (GL_DRAW_FRAMEBUFFER, m_blendBackground->getFramebuffer ());
	glBlitFramebuffer (0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
	m_colorBlendMVP = mvp
	    * glm::translate (glm::mat4 (1.0f), glm::vec3 (m_effectCompositeOffset, 0.0f))
	    * glm::scale (glm::mat4 (1.0f), glm::vec3 (m_effectSurface * 0.5f, 1.0f));
	// Only the forced color-blend passthrough has this legacy version gate.
	m_colorBlendPass->setDepthtestMode (depthTestEnabled () && getScene ().getScene ().version > 1
	    ? DepthtestMode_Enabled : DepthtestMode_Disabled);
	m_colorBlendPass->setModelViewProjectionMatrix (&m_colorBlendMVP, getScene ().getCamera ().usesReverseDepth ());
	m_colorBlendPass->setDepthwriteMode (DepthwriteMode_Disabled);
	m_colorBlendPass->render ();
    } else if (drawToScene) {
        if (depthTestEnabled ()) glEnable (GL_DEPTH_TEST);
        else glDisable (GL_DEPTH_TEST);
        glDepthFunc (sceneTarget->usesReverseDepth () ? GL_GREATER : GL_LESS);
        glDepthMask (GL_FALSE);
        glEnable (GL_BLEND);
        glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glUniformMatrix4fv (m_cuMVP, 1, GL_FALSE, glm::value_ptr (mvp));
        glDrawArrays (GL_TRIANGLES, 0, 6);
    }
    glBindVertexArray (0);

    // restore the state this chain touched that nothing downstream re-sets per draw
    glColorMask (GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glClearColor (previousClearColor[0], previousClearColor[1], previousClearColor[2], previousClearColor[3]);
    if (depthWasEnabled == GL_TRUE) {
	glEnable (GL_DEPTH_TEST);
    } else {
	glDisable (GL_DEPTH_TEST);
    }
}

bool CText::depthTestEnabled () const {
    return getScene ().getScene ().camera.projection.isPerspective
	&& m_text.depthTest->value->getString () == "enabled";
}

bool CText::initFreeType () {
    if (FT_Init_FreeType (&m_ftLibrary) == 0) {
	return true;
    }
    sLog.error ("CText: FT_Init_FreeType failed for object ", m_text.name);
    return false;
}

namespace {
/**
 * Text objects commonly share one font; CJK fonts are 10-20 MB, and reading one per object
 * made a scene with 34 texts on a 20 MB .ttc copy ~1.4 GB at startup and keep ~700 MB resident.
 */
std::shared_ptr<const std::vector<uint8_t>> sharedFontData (
    const WallpaperEngine::Assets::AssetLocator& locator, const std::string& font) {
    // ponytail: expired entries keep their key until the same font loads again; keys are tiny
    static std::mutex mutex;
    static std::unordered_map<std::string, std::weak_ptr<const std::vector<uint8_t>>> fonts;
    const std::lock_guard lock (mutex);
    auto& cached = fonts[locator.identity () + '\0' + font];
    if (auto data = cached.lock ()) {
	return data;
    }

    auto stream = locator.read (font);
    stream->seekg (0, std::ios::end);
    const auto size = stream->tellg ();
    stream->seekg (0, std::ios::beg);
    auto bytes = std::vector<uint8_t> (static_cast<size_t> (size));
    stream->read (reinterpret_cast<char*> (bytes.data ()), size);

    auto data = std::make_shared<const std::vector<uint8_t>> (std::move (bytes));
    cached = data;
    return data;
}
} // namespace

bool CText::loadEmbeddedFont () {
    // Wallpapers packed in .pkg don't expose physical paths, so we read the font
    // into memory and use FT_New_Memory_Face. m_fontData must outlive the face.
    // `systemfont_*` references signal "use a system font"; let the fallback handle them.
    if (m_text.font.empty () || m_text.font.rfind ("systemfont_", 0) == 0) {
	return false;
    }

    try {
	m_fontData = sharedFontData (getAssetLocator (), m_text.font);

	if (FT_New_Memory_Face (
		m_ftLibrary, m_fontData->data (), static_cast<FT_Long> (m_fontData->size ()), 0, &m_ftFace
	    )
	    == 0) {
	    return true;
	}

	sLog.error ("CText: FT_New_Memory_Face failed for '", m_text.font, "', falling back to system font");
    } catch (const std::exception& e) {
	sLog.error ("CText: cannot read font '", m_text.font, "': ", e.what (), ", falling back to system font");
    }

    m_fontData.reset ();
    return false;
}

bool CText::loadSystemFont () {
    // systemfont_* stores an authored family, not a request for our default
    // sans-serif face. Fontconfig also honors installed Windows fonts and the
    // user's substitutions when that family is unavailable on Linux.
    if (m_text.font.starts_with ("systemfont_")) {
	std::string family = m_text.font.substr (11);
	std::ranges::replace (family, '_', ' ');
	if (auto* pattern = FcNameParse (reinterpret_cast<const FcChar8*> (family.c_str ()))) {
	    FcConfigSubstitute (nullptr, pattern, FcMatchPattern);
	    FcDefaultSubstitute (pattern);
	    FcResult result;
	    auto* match = FcFontMatch (nullptr, pattern, &result);
	    FcPatternDestroy (pattern);
	    if (match != nullptr) {
		FcChar8* file = nullptr;
		int faceIndex = 0;
		FcPatternGetInteger (match, FC_INDEX, 0, &faceIndex);
		const bool loaded = FcPatternGetString (match, FC_FILE, 0, &file) == FcResultMatch
		    && FT_New_Face (m_ftLibrary, reinterpret_cast<const char*> (file), faceIndex, &m_ftFace) == 0;
		FcPatternDestroy (match);
		if (loaded) return true;
	    }
	}
    }
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

FT_Face CText::fontForGlyph (const uint32_t code) {
    if (FT_Get_Char_Index (m_ftFace, code) != 0) return m_ftFace;
    if (const auto found = m_glyphFaces.find (code); found != m_glyphFaces.end ()) return found->second;

    // Keep the authored face's existing glyphs and line metrics. Ask Fontconfig
    // for a matching installed face only when its charmap misses a character.
    auto* pattern = FcPatternCreate ();
    auto* characters = FcCharSetCreate ();
    FcCharSetAddChar (characters, code);
    FcPatternAddCharSet (pattern, FC_CHARSET, characters);
    FcCharSetDestroy (characters);
    FcPatternAddString (pattern, FC_FAMILY, reinterpret_cast<const FcChar8*> (
	m_ftFace->family_name != nullptr ? m_ftFace->family_name : "sans-serif"));
    FcPatternAddInteger (pattern, FC_WEIGHT,
	m_ftFace->style_flags & FT_STYLE_FLAG_BOLD ? FC_WEIGHT_BOLD : FC_WEIGHT_REGULAR);
    FcPatternAddInteger (pattern, FC_SLANT,
	m_ftFace->style_flags & FT_STYLE_FLAG_ITALIC ? FC_SLANT_ITALIC : FC_SLANT_ROMAN);
    FcPatternAddBool (pattern, FC_SCALABLE, FcTrue);
    FcConfigSubstitute (nullptr, pattern, FcMatchPattern);
    FcDefaultSubstitute (pattern);
    FcResult result;
    auto* match = FcFontMatch (nullptr, pattern, &result);
    FcPatternDestroy (pattern);

    FT_Face face = m_ftFace;
    if (match != nullptr) {
	FcChar8* file = nullptr;
	int index = 0;
	FcPatternGetInteger (match, FC_INDEX, 0, &index);
	if (FcPatternGetString (match, FC_FILE, 0, &file) == FcResultMatch) {
	    const std::string key = std::string (reinterpret_cast<const char*> (file)) + ":" + std::to_string (index);
	    if (const auto found = m_fallbackFaces.find (key); found != m_fallbackFaces.end ()) {
		face = found->second;
	    } else {
		FT_Face fallback = nullptr;
		if (FT_New_Face (m_ftLibrary, reinterpret_cast<const char*> (file), index, &fallback) == 0) {
		    if (FT_IS_SCALABLE (fallback) && FT_Get_Char_Index (fallback, code) != 0
			&& FT_Set_Char_Size (fallback, 0, m_lastFontSize, 300, 300) == 0) {
			m_fallbackFaces.emplace (key, fallback);
			face = fallback;
		    } else {
			FT_Done_Face (fallback);
		    }
		}
	    }
	}
	FcPatternDestroy (match);
    }
    m_glyphFaces.emplace (code, face);
    return face;
}

unsigned int CText::currentFontSize () const {
    // Native keeps fractional points in FreeType's 26.6 units at 300 DPI.
    // Converting to whole pixels first changes both ink and padded line bounds.
    return static_cast<unsigned int> (std::clamp (m_text.pointSize->value->getFloat (), 1.0f, 256.0f) * 64.0f);
}

TextLayoutLimits CText::currentLayoutLimits () const {
    const float width = m_text.maxWidth->value->getFloat ();
    return {
	.width = m_text.limitWidth->value->getBool () && std::isfinite (width) ? std::max (0.0f, width) : 0.0f,
	.rows = m_text.limitRows->value->getBool ()
	    ? static_cast<size_t> (std::max (1, m_text.maxRows->value->getInt ()))
	    : 0,
	.ellipsis = m_text.limitUseEllipsis->value->getBool (),
    };
}

glm::vec2 CText::currentSpacing () const {
    return glm::vec2 (m_text.spacing->evaluateVec3 (getScene ().getElapsedTime ()));
}

glm::vec2 CText::currentPadding () const {
    return glm::vec2 (m_text.padding->evaluateVec3 (getScene ().getElapsedTime ()));
}

void CText::drawBackground (const glm::mat4& mvp, const float alpha) {
    if (!m_text.opaqueBackground->value->getBool ()) return;
    const float brightness = getScene ().isHdr ()
	? m_text.backgroundBrightness->evaluateFloat (getScene ().getElapsedTime ()) : 1.0f;
    const glm::vec3 color = m_text.backgroundColor->value->getVec3 () * brightness;
    const glm::mat4 backgroundMVP = mvp
	* glm::translate (glm::mat4 (1.0f), glm::vec3 (m_effectCompositeOffset, 0.0f))
	* glm::scale (glm::mat4 (1.0f), glm::vec3 (m_effectSurface / m_quadSize, 1.0f))
	* glm::translate (glm::mat4 (1.0f), glm::vec3 (-m_quadOffset, 0.0f));
    glEnable (GL_BLEND);
    glBlendFunc (GL_SRC_ALPHA, m_text.colorBlendMode->value->getInt () == 31 ? GL_ONE : GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram (m_program);
    glUniformMatrix4fv (m_uMVP, 1, GL_FALSE, glm::value_ptr (backgroundMVP));
    glUniform4f (m_uColor, color.r, color.g, color.b, alpha);
    glUniform1i (m_uSolid, 1);
    ensureVao ();
    glBindVertexArray (m_vao);
    glDrawArrays (GL_TRIANGLES, 0, 6);
    glBindVertexArray (0);
    glUniform1i (m_uSolid, 0);
}

glm::vec2 CText::getSize () {
    const auto surfaceSize = [this] {
        // Native offscreen surfaces/opaque backgrounds add padding to the mesh;
        // direct text ignores padding. Keep the cached line box unpadded.
        const auto& fog = getScene ().getScene ().fog;
        const glm::vec2 padding = m_effectsEnabled || !m_text.effects.empty () || m_text.opaqueBackground->value->getBool ()
            || fog.distance.enabled->value->getBool () || fog.height.enabled->value->getBool () || m_text.ledSource->value->getBool ()
            ? glm::min (currentPadding (), glm::vec2 (512.0f)) : glm::vec2 (0);
        return m_scriptSize + padding * 2.0f;
    };
    const auto& text = m_text.text->value->getString ();
    if (m_ftFace == nullptr || text == m_lastSizedText) return surfaceSize ();

    // Native size reads the generated font mesh immediately after a text write.
    // Its HarfBuzz font uses unhinted advances; bounds still use hinted glyphs.
    // Rendering owns texture/effect rebuilds independently of this CPU measurement.
    const glm::vec2 spacing = currentSpacing ();
    const float lineHeight = static_cast<float> (m_ftFace->size->metrics.height >> 6) + spacing.y;
    const float ascender = static_cast<float> (m_ftFace->size->metrics.ascender >> 6);
    const float descender = static_cast<float> (m_ftFace->size->metrics.descender >> 6);
    const auto loadGlyph = [] (const TextShapedGlyph& glyph, FT_Int32 flags, FT_Vector delta = { 0, 0 }) -> FT_GlyphSlot {
	FT_Set_Transform (glyph.face, nullptr, &delta);
	return FT_Load_Glyph (glyph.face, glyph.index, flags) == 0 ? glyph.face->glyph : nullptr;
    };
    const auto lines = layoutShapedTextLines (text, currentLayoutLimits (), [this] (const std::string& row) {
	return shapeTextLine (row, [this] (uint32_t code) { return fontForGlyph (code); });
    }, spacing.x);
    std::vector<float> widths;
    float maxWidth = 0.0f;
    for (const auto& line : lines) {
	float width = 0.0f;
        for (const auto& glyph : line.glyphs) width += glyph.advance + spacing.x;
	widths.push_back (width);
	maxWidth = std::max (maxWidth, width);
    }
    glm::vec4 bounds (0.0f);
    for (size_t i = 0; i < lines.size (); ++i) {
	float pen = m_text.alignment == "right" ? maxWidth - widths[i]
	    : m_text.alignment == "left" ? 0.0f : (maxWidth - widths[i]) * 0.5f;
	if (m_text.alignment != "left" && m_text.alignment != "right" && spacing.x == 0.0f) pen = std::round (pen);
	const float baseline = static_cast<float> (i) * lineHeight;
	for (const auto& glyph : lines[i].glyphs) {
            const float x = pen + glyph.xOffset;
            const float y = baseline - glyph.yOffset;
	    const FT_Vector delta = {
		static_cast<FT_Pos> (std::round ((x - std::floor (x)) * 64.0f)),
		static_cast<FT_Pos> (std::round (-(y - std::floor (y)) * 64.0f)),
	    };
	    const float advance = glyph.advance + spacing.x;
	    const auto slot = loadGlyph (glyph, FT_LOAD_RENDER, delta);
	    if (slot == nullptr) continue;
	    if (slot->bitmap.width != 0 && slot->bitmap.rows != 0) {
		const float left = std::floor (x) + slot->bitmap_left;
		const float top = std::floor (y) - slot->bitmap_top;
		bounds.x = std::min (bounds.x, left);
		bounds.y = std::min (bounds.y, -top - slot->bitmap.rows);
		bounds.z = std::max (bounds.z, left + slot->bitmap.width);
		bounds.w = std::max (bounds.w, -top);
	    }
	    pen += advance;
	}
    }
    const TextEffectLayout layout = computeTextEffectLayout (
	m_text.alignment, m_text.verticalalign, bounds, ascender, descender, lineHeight, lines.size (), { 0, 0 }
    );
    m_scriptSize = layout.surfaceSize;
    m_scriptAlignmentOffset = layout.compositeOffset * glm::vec2 (1, -1);
    m_lastSizedText = text;
    return surfaceSize ();
}

std::optional<glm::mat3> CText::transformAttachmentToTexture (
    const CObject& source, const glm::mat4& attachmentWorld
) {
    const auto& camera = getScene ().getCamera ();
    const Object& object = source.getObject ();
    const bool virtualReceiver = m_text.perspective->value->getBool ();
    bool virtualSource = false;
    if (object.is<Image> ()) {
        const Image& image = *object.as<Image> ();
        if (image.model->fullscreen) return std::nullopt;
        virtualSource = image.perspective->value->getBool ();
    } else if (object.is<Model3D> ()) {
        virtualSource = object.as<Model3D> ()->perspective->value->getBool ();
    } else {
        return std::nullopt;
    }
    const glm::vec2 size = getSize ();
    // Native text vtable+0x80 includes the generated font-mesh alignment.
    // Measure on the CPU so a text write does not depend on a later GL rebuild.
    const glm::mat4 receiverWorld = glm::translate (resolveWorldMatrix (), glm::vec3 (m_scriptAlignmentOffset, 0));
    if (!camera.isOrthogonal ()) {
        const glm::mat4 viewProjection = camera.getProjection () * camera.getLookAt ();
        const glm::vec3 receiverEye = virtualReceiver
            ? glm::vec3 (glm::inverse (camera.getLookAt ())[3]) : camera.getEye ();
        return CImage::calculateOrthographicAttachmentTextureMatrix (
            attachmentWorld, receiverWorld, viewProjection, viewProjection,
            camera.getEye ().z, size, receiverEye, virtualReceiver ? 1.0f : 0.0f);
    }
    const glm::mat4 sceneToRender = glm::translate (glm::mat4 (1),
        glm::vec3 (-camera.getWidth () * .5f, camera.getHeight () * .5f, 0))
        * glm::scale (glm::mat4 (1), glm::vec3 (1, -1, 1));
    const glm::mat4 viewProjection = camera.getProjection () * camera.getLookAt () * sceneToRender;
    glm::mat4 virtualView (1), virtualViewProjection (1);
    if (virtualSource || virtualReceiver) {
        const float fov = glm::radians (glm::clamp (camera.getFov (), 0.1f, 179.9f));
        const float distance = 1.0f / (std::tan (fov * 0.5f) * camera.getProjection ()[1][1]);
        glm::mat4 view = camera.getLookAt ();
        view[3][2] = -distance;
        virtualView = view * sceneToRender;
        const auto& output = getScene ().getOutputSize ();
        virtualViewProjection = glm::perspective (fov, output.x / output.y, 5.0f,
            glm::max (15000.0f, distance + 1000.0f)) * virtualView;
    }
    const glm::vec2 sourceOffset = source.resolveParallaxOffset () * glm::vec2 (1, -1);
    const glm::vec2 receiverOffset = resolveParallaxOffset () * glm::vec2 (1, -1);
    std::optional<glm::vec3> receiverEye;
    if (virtualReceiver) {
        receiverEye = glm::vec3 (glm::inverse (glm::translate (virtualView, glm::vec3 (receiverOffset, 0)))[3]);
    }
    return CImage::calculateOrthographicAttachmentTextureMatrix (
        attachmentWorld, receiverWorld,
        glm::translate (virtualSource ? virtualViewProjection : viewProjection, glm::vec3 (sourceOffset, 0)),
        glm::translate (virtualReceiver ? virtualViewProjection : viewProjection, glm::vec3 (receiverOffset, 0)),
        camera.getEye ().z, size, receiverEye);
}

void CText::rebuildTextureFrom (const std::string& text) {
    // Wallpaper Engine lays out a glyph mesh, then aligns that mesh around the layer
    // origin from its actual bounds and the font's ascender/descender. The serialized
    // layer size is editor metadata, not a text box, and padding grows the optional
    // background/effect surface rather than insetting the glyphs. This behavior and the
    // slightly unusual vertical-center formula are recovered from wallpaper64.exe's
    // text update and matrix routines (FUN_140256f20/FUN_140256e10).
    //
    // Safe to call repeatedly: GL handles (texture, VAO, VBO) are reused when
    // already allocated, so dynamic/scripted text can regenerate the glyph
    // bitmap every time the rendered string changes without leaking.
    const auto loadGlyph = [] (const TextShapedGlyph& glyph, FT_Int32 flags, FT_Vector delta = { 0, 0 }) -> FT_GlyphSlot {
	FT_Set_Transform (glyph.face, nullptr, &delta);
	return FT_Load_Glyph (glyph.face, glyph.index, flags) == 0 ? glyph.face->glyph : nullptr;
    };
    m_lastMsdfRequest = currentStyle ().msdf;
    m_usingMsdf = m_lastMsdfRequest && FT_IS_SCALABLE (m_ftFace) && !FT_HAS_COLOR (m_ftFace);
    if (m_lastMsdfRequest && !m_usingMsdf) {
        sLog.error ("CText: MSDF styles require a non-color outline font: ", m_text.name);
    }
    struct GlyphPlacement { size_t image; glm::vec4 bounds; };
    std::vector<GlyphPlacement> glyphPlacements;
    std::vector<const TextMsdfGlyph*> glyphImages;
    std::map<std::pair<FT_Face, uint32_t>, size_t> glyphImageIndices;
    m_lastSpacing = currentSpacing ();
    m_lastPadding = currentPadding ();

    // Metrics-based row spacing is stable across glyph mixes and meaningful for empty rows.
    // Native spacing adds pixels after converting FreeType's 26.6 metrics; it
    // neither scales with pointsize nor rounds the resulting glyph/row advance.
    const float lineHeight = static_cast<float> (m_ftFace->size->metrics.height >> 6) + m_lastSpacing.y;
    const int ascender = static_cast<int> (m_ftFace->size->metrics.ascender >> 6);
    const int descender = static_cast<int> (m_ftFace->size->metrics.descender >> 6);

    m_lastLayoutLimits = currentLayoutLimits ();
    struct Line {
        std::vector<TextShapedGlyph> glyphs;
	float width = 0.0f;
	float x = 0.0f;
    };
    std::vector<Line> lines;
    for (auto& line : layoutShapedTextLines (text, m_lastLayoutLimits, [this] (const std::string& row) {
	     return shapeTextLine (row, [this] (uint32_t code) { return fontForGlyph (code); });
	 }, m_lastSpacing.x)) {
	lines.push_back ({ .glyphs = std::move (line.glyphs) });
    }

    float maxLineWidth = 0.0f;
    for (auto& line : lines) {
        for (const auto& glyph : line.glyphs) line.width += glyph.advance + m_lastSpacing.x;
	maxLineWidth = std::max (maxLineWidth, line.width);
    }

    // The same alignment also controls how shorter rows sit inside the generated block.
    for (auto& line : lines) {
	if (m_text.alignment == "left") {
	    line.x = 0;
	} else if (m_text.alignment == "right") {
	    line.x = maxLineWidth - line.width;
	} else {
	    line.x = (maxLineWidth - line.width) * 0.5f;
	    if (m_lastSpacing.x == 0.0f) {
		line.x = std::round (line.x);
	    }
	}
    }

    // FreeType translates scalable glyph outlines at 26.6 precision. Keep the
    // integer pen in the bitmap destination and apply its fractional part to the
    // outline, so measuring and rasterizing use identical subpixel ink bounds.
    const auto loadPositionedGlyph = [&loadGlyph] (const TextShapedGlyph& glyph, float x, float y) {
	FT_Vector delta = {
	    static_cast<FT_Pos> (std::round ((x - std::floor (x)) * 64.0f)),
	    static_cast<FT_Pos> (std::round (-(y - std::floor (y)) * 64.0f)),
	};
	return loadGlyph (glyph, FT_LOAD_RENDER, delta);
    };

    // Measure the actual bitmap bounds in a first-baseline coordinate system with +y down.
    // Native alignment uses these glyph bounds rather than advances or a line-height box.
    int inkLeft = std::numeric_limits<int>::max ();
    int inkTop = std::numeric_limits<int>::max ();
    int inkRight = std::numeric_limits<int>::lowest ();
    int inkBottom = std::numeric_limits<int>::lowest ();
    bool hasInk = false;

    for (size_t i = 0; i < lines.size (); ++i) {
	const auto& line = lines[i];
	float penX = line.x;
	const float baseline = static_cast<float> (i) * lineHeight;

	for (const auto& glyph : line.glyphs) {
            const float x = penX + glyph.xOffset;
            const float y = baseline - glyph.yOffset;
	    const auto slot = m_usingMsdf ? loadGlyph (glyph, FT_LOAD_NO_BITMAP)
                : loadPositionedGlyph (glyph, x, y);
	    if (slot == nullptr) {
		continue;
	    }

            if (m_usingMsdf) {
                auto [imageIndex, firstUse] = glyphImageIndices.try_emplace (std::pair { glyph.face, glyph.index }, glyphImages.size ());
                if (firstUse) {
                    auto [cached, inserted] = m_msdfGlyphs.try_emplace (std::pair { glyph.face, glyph.index });
                    if (inserted) cached->second = generateTextMsdfGlyph (slot->face);
                    glyphImages.push_back (&cached->second);
                }
                const auto& image = *glyphImages[imageIndex->second];
                if (!image.pixels.empty ()) {
                    const auto bounds = image.inkBounds;
                    const float left = x + bounds.x;
                    const float top = y - bounds.w;
                    const float right = x + bounds.z;
                    const float bottom = y - bounds.y;
                    inkLeft = std::min (inkLeft, static_cast<int> (std::floor (left)));
                    inkTop = std::min (inkTop, static_cast<int> (std::floor (top)));
                    inkRight = std::max (inkRight, static_cast<int> (std::ceil (right)));
                    inkBottom = std::max (inkBottom, static_cast<int> (std::ceil (bottom)));
                    hasInk = true;
                    glyphPlacements.push_back ({ imageIndex->second,
                        { left - image.margin, top - image.margin, right + image.margin, bottom + image.margin } });
                }
            } else {
	    const auto& bmp = slot->bitmap;
	    if (bmp.width != 0 && bmp.rows != 0) {
		const int glyphLeft = static_cast<int> (std::floor (x)) + slot->bitmap_left;
		const int glyphTop = static_cast<int> (std::floor (y)) - slot->bitmap_top;
		inkLeft = std::min (inkLeft, glyphLeft);
		inkTop = std::min (inkTop, glyphTop);
		inkRight = std::max (inkRight, glyphLeft + static_cast<int> (bmp.width));
		inkBottom = std::max (inkBottom, glyphTop + static_cast<int> (bmp.rows));
		hasInk = true;
	    }
            }
	    penX += glyph.advance + m_lastSpacing.x;
	}
    }

    if (!hasInk) {
	// A scripted layer is initially represented by a space. Keep a meaningful,
	// transparent surface and native font-metric alignment until it gets real glyphs.
	inkLeft = 0;
	inkRight = std::max (1, static_cast<int> (std::ceil (maxLineWidth)));
	const float lastBaseline = static_cast<float> (lines.size () - 1) * lineHeight;
	inkTop = -ascender + static_cast<int> (std::floor (std::min (0.0f, lastBaseline)));
	inkBottom = -descender + static_cast<int> (std::ceil (std::max (0.0f, lastBaseline)));
    }

    // Equal margins preserve the glyph-bounds center while protecting filter sampling.
    constexpr int kInkMargin = 4;
    const int x0 = inkLeft - kInkMargin;
    const int y0 = inkTop - kInkMargin;
    const int width = std::max (1, inkRight - inkLeft + kInkMargin * 2);
    const int height = std::max (1, inkBottom - inkTop + kInkMargin * 2);
    std::vector<uint8_t> pixels (m_usingMsdf ? 0 : static_cast<size_t> (width) * height, 0);

    if (!m_usingMsdf) {

    for (size_t i = 0; i < lines.size (); i++) {
	const auto& line = lines[i];
	float penX = line.x;
	const float baseline = static_cast<float> (i) * lineHeight;

	for (const auto& glyph : line.glyphs) {
            const float x = penX + glyph.xOffset;
            const float y = baseline - glyph.yOffset;
	    const auto slot = loadPositionedGlyph (glyph, x, y);
	    if (slot == nullptr) {
		continue;
	    }

	    const auto& bmp = slot->bitmap;
	    const int originX = static_cast<int> (std::floor (x)) + slot->bitmap_left - x0;
	    const int originY = static_cast<int> (std::floor (y)) - slot->bitmap_top - y0;

	    for (unsigned int row = 0; row < bmp.rows; ++row) {
		for (unsigned int col = 0; col < bmp.width; ++col) {
		    const int dstX = originX + static_cast<int> (col);
		    const int dstY = originY + static_cast<int> (row);
		    if (dstX < 0 || dstX >= width || dstY < 0 || dstY >= height) {
			continue;
		    }
		    // Glyph bitmaps overlap when ink extends past the advance (italic text
		    // and display-clock fonts). Transparent texels in the next glyph must
		    // not erase the previous one; accumulate coverage as source-over alpha.
		    auto& coverage = pixels[static_cast<size_t> (dstY) * width + dstX];
		    const unsigned int incoming = bmp.buffer[row * bmp.pitch + col];
		    coverage = incoming + (coverage * (255 - incoming) + 127) / 255;
		}
	    }

	    penX += glyph.advance + m_lastSpacing.x;
	}
    }
    }
    FT_Set_Transform (m_ftFace, nullptr, nullptr);

    const bool firstUpload = (m_texture == 0);
    if (firstUpload) {
	glGenTextures (1, &m_texture);
    }
    glBindTexture (GL_TEXTURE_2D, m_texture);
    glPixelStorei (GL_UNPACK_ALIGNMENT, 1);
    std::vector<glm::ivec2> atlasOrigins;
    if (m_usingMsdf) {
        GLint maximumSize;
        glGetIntegerv (GL_MAX_TEXTURE_SIZE, &maximumSize);
        int atlasWidth = std::min (1024, maximumSize);
        for (const auto* image : glyphImages) atlasWidth = std::max (atlasWidth, image->width + 1);
        int x = 0, y = 0, rowHeight = 0;
        for (const auto* image : glyphImages) {
            if (x + image->width + 1 > atlasWidth) { x = 0; y += rowHeight; rowHeight = 0; }
            atlasOrigins.emplace_back (x, y);
            x += image->width + 1;
            rowHeight = std::max (rowHeight, image->height + 1);
        }
        const int atlasHeight = std::max (1, y + rowHeight);
        if (atlasWidth > maximumSize || atlasHeight > maximumSize) {
            throw std::runtime_error ("CText MSDF atlas exceeds maximum texture size");
        }
        std::vector<uint8_t> atlas (static_cast<size_t> (atlasWidth) * atlasHeight * 4, 0);
        for (size_t i = 0; i < glyphImages.size (); ++i) {
            const auto& image = *glyphImages[i];
            const auto origin = atlasOrigins[i];
            for (int row = 0; row < image.height; ++row) {
                std::copy_n (image.pixels.data () + static_cast<size_t> (row) * image.width * 4, image.width * 4,
                    atlas.data () + (static_cast<size_t> (origin.y + row) * atlasWidth + origin.x) * 4);
            }
        }
        glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA8, atlasWidth, atlasHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, atlas.data ());
        m_textureSize = { atlasWidth, atlasHeight };
    } else {
        glTexImage2D (GL_TEXTURE_2D, 0, GL_RED, width, height, 0, GL_RED, GL_UNSIGNED_BYTE, pixels.data ());
        m_textureSize = { width, height };
    }
    if (firstUpload) {
	glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }

    m_quadSize = { static_cast<float> (width), static_cast<float> (height) };
    const glm::vec4 glyphBounds = {
	static_cast<float> (inkLeft),
	static_cast<float> (-inkBottom),
	static_cast<float> (inkRight),
	static_cast<float> (-inkTop),
    };
    const glm::vec2 nativeOffset = computeTextAlignmentOffset (
	m_text.alignment, m_text.verticalalign, glyphBounds, static_cast<float> (ascender),
	static_cast<float> (descender), static_cast<float> (lineHeight), lines.size ()
    );
    // The glyph VBO uses +y down; the recovered native offset is in FreeType's +y-up space.
    m_quadOffset = { nativeOffset.x, -nativeOffset.y };
    const TextEffectLayout effectLayout = computeTextEffectLayout (
	m_text.alignment, m_text.verticalalign, glyphBounds, static_cast<float> (ascender),
	static_cast<float> (descender), static_cast<float> (lineHeight), lines.size (), m_lastPadding
    );
    m_effectSurface = glm::max (effectLayout.surfaceSize, glm::vec2 (1.0f));
    m_effectRasterOffset = effectLayout.rasterOffset;
    m_effectCompositeOffset = effectLayout.compositeOffset;
    m_lastSizedText.reset ();
    m_lastRenderedText = text;

    if (m_usingMsdf) {
        const glm::vec2 placementOffset = m_quadOffset
            - glm::vec2 (static_cast<float> (inkLeft + inkRight), static_cast<float> (inkTop + inkBottom)) * .5f;
        std::vector<float> vertices;
        for (const auto& placement : glyphPlacements) {
            const auto& image = *glyphImages[placement.image];
            const auto origin = atlasOrigins[placement.image];
            const float u0 = static_cast<float> (origin.x) / m_textureSize.x;
            const float v0 = static_cast<float> (origin.y) / m_textureSize.y;
            const float u1 = static_cast<float> (origin.x + image.width) / m_textureSize.x;
            const float v1 = static_cast<float> (origin.y + image.height) / m_textureSize.y;
            const float left = placement.bounds.x + placementOffset.x, right = placement.bounds.z + placementOffset.x;
            const float top = placement.bounds.y + placementOffset.y, bottom = placement.bounds.w + placementOffset.y;
            const float quad[] = { left,top,u0,v0, right,top,u1,v0, right,bottom,u1,v1,
                left,top,u0,v0, right,bottom,u1,v1, left,bottom,u0,v1 };
            vertices.insert (vertices.end (), std::begin (quad), std::end (quad));
        }
        if (m_glyphVbo == 0) glGenBuffers (1, &m_glyphVbo);
        glBindBuffer (GL_ARRAY_BUFFER, m_glyphVbo);
        glBufferData (GL_ARRAY_BUFFER, vertices.size () * sizeof (float), vertices.data (), GL_DYNAMIC_DRAW);
        m_glyphVertexCount = static_cast<GLsizei> (vertices.size () / 4);
    }
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
    m_uSolid = glGetUniformLocation (m_program, "uSolid");
    m_uMsdf = glGetUniformLocation (m_program, "uMsdf");
    m_uStyleFlags = glGetUniformLocation (m_program, "uStyleFlags");
    m_uRenderVars = glGetUniformLocation (m_program, "uRenderVars");
    m_uTextureSize = glGetUniformLocation (m_program, "uTextureSize");
}

void CText::uploadQuadVertices () {
    // Quad in local raster-space coordinates (+y down, matching the texture rows; render()'s
    // mirror pair maps this into the y-up world so the glyphs stay upright). The quad covers
    // the glyph bounding box, offset from the object origin by native font-metric alignment.
    // VBO contents are re-uploaded whenever
    // the glyph bitmap is rebuilt so the quad always matches the current texture.
    const float hx = m_quadSize.x * 0.5f;
    const float hy = m_quadSize.y * 0.5f;
    const float x0 = m_quadOffset.x - hx;
    const float x1 = m_quadOffset.x + hx;
    const float y0 = m_quadOffset.y - hy;
    const float y1 = m_quadOffset.y + hy;
    // UV.v=0 = texture top row = the visual top of the text
    const float verts[] = {
	// pos      // uv
	x0, y0, 0.0f, 0.0f, x1, y0, 1.0f, 0.0f, x1, y1, 1.0f, 1.0f,
	x0, y0, 0.0f, 0.0f, x1, y1, 1.0f, 1.0f, x0, y1, 0.0f, 1.0f,
    };

    // only the VBO (a shared GL object) is touched here: this path also runs during
    // async wallpaper builds on the worker context. The VAO is created lazily in
    // ensureVao on the render thread, since VAOs are not shared between GL contexts.
    if (m_vbo == 0) {
	glGenBuffers (1, &m_vbo);
    }
    glBindBuffer (GL_ARRAY_BUFFER, m_vbo);
    glBufferData (GL_ARRAY_BUFFER, sizeof (verts), verts, GL_DYNAMIC_DRAW);
}

void CText::ensureVao () {
    if (m_vao != 0) {
	return;
    }

    glGenVertexArrays (1, &m_vao);
    glBindVertexArray (m_vao);
    glBindBuffer (GL_ARRAY_BUFFER, m_vbo);
    glEnableVertexAttribArray (0);
    glVertexAttribPointer (0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof (float), reinterpret_cast<void*> (0));
    glEnableVertexAttribArray (1);
    glVertexAttribPointer (1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof (float), reinterpret_cast<void*> (2 * sizeof (float)));
    glBindVertexArray (0);
}

TextStyleUniforms CText::currentStyle () const {
    const auto time = getScene ().getElapsedTime ();
    return computeTextStyleUniforms ({
        .msdf = m_text.msdf->value->getBool (),
        .outline = m_text.outline->value->getBool (),
        .blur = m_text.blur->value->getBool (),
        .shadow = m_text.dropShadow->value->getBool (),
        .outlineWidth = m_text.outlineWidth->evaluateFloat (time),
        .outlineColor = m_text.outlineColor->evaluateVec3 (time),
        .blurRadius = m_text.blurRadius->evaluateFloat (time),
        .shadowRadius = m_text.dropShadowRadius->evaluateFloat (time),
        .shadowOpacity = m_text.dropShadowOpacity->evaluateFloat (time),
        .shadowOffset = glm::vec2 (m_text.dropShadowOffset->evaluateVec3 (time)),
        .shadowColor = m_text.dropShadowColor->evaluateVec3 (time),
    }, m_text.pointSize->value->getFloat ());
}

void CText::drawGlyphs () {
    if (m_usingMsdf) {
        if (m_glyphVao == 0) {
            glGenVertexArrays (1, &m_glyphVao);
            glBindVertexArray (m_glyphVao);
            glBindBuffer (GL_ARRAY_BUFFER, m_glyphVbo);
            glEnableVertexAttribArray (0);
            glVertexAttribPointer (0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof (float), reinterpret_cast<void*> (0));
            glEnableVertexAttribArray (1);
            glVertexAttribPointer (1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof (float), reinterpret_cast<void*> (2 * sizeof (float)));
        }
        glBindVertexArray (m_glyphVao);
        glDrawArrays (GL_TRIANGLES, 0, m_glyphVertexCount);
    } else {
        ensureVao ();
        glBindVertexArray (m_vao);
        glDrawArrays (GL_TRIANGLES, 0, 6);
    }
    glBindVertexArray (0);
}

void CText::ensureCompositeVao () {
    if (m_compositeVao != 0) {
	return;
    }

    glGenVertexArrays (1, &m_compositeVao);
    glBindVertexArray (m_compositeVao);
    glBindBuffer (GL_ARRAY_BUFFER, m_compositeVbo);
    glEnableVertexAttribArray (0);
    glVertexAttribPointer (0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof (float), reinterpret_cast<void*> (0));
    glEnableVertexAttribArray (1);
    glVertexAttribPointer (1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof (float), reinterpret_cast<void*> (2 * sizeof (float)));
    glBindVertexArray (0);
}

bool CText::isVolumetricBarrier () const {
    // Native 1402577e0 requires a valid text layout and nonempty live string,
    // then admits visible layers or retained/LED sources (image flags0x1100).
    return this->m_valid && !this->m_text.text->value->getString ().empty ()
	&& (CObject::isVolumetricBarrier () || this->m_text.ledSource->value->getBool ()
	    || this->getScene ().isRenderTargetRetained (this->m_publishedFBO));
}

void CText::render () {
    if (!m_valid) {
	return;
    }
    const bool emptyEffect = std::ranges::any_of (m_text.effects, [] (const auto& effect) {
	return effect->visible->value->getBool ()
	    && std::ranges::none_of (effect->effect->passes, [] (const auto& pass) { return pass->enabled; });
    });
    const bool drawToScene = !emptyEffect && m_text.visible->value->getBool () && this->isVisibleThroughParents ();
    if (!drawToScene && !m_effectsEnabled) {
        return;
    }

#if !NDEBUG
    std::string str = "Text " + this->getObject ().name + " (" + std::to_string (this->getObject ().id) + ")";
    glPushDebugGroup (GL_DEBUG_SOURCE_APPLICATION, 0, -1, str.c_str ());
#endif /* DEBUG */
    // Property scripts run once in the scene's normal lifecycle, including media
    // events and writes to thisLayer.text from other properties/layers.
    const auto& current = m_text.text->value->getString ();
    const std::string renderedText = current.empty () ? std::string (" ") : current;

    const glm::vec2 previousSurface = m_effectSurface;
    const glm::vec2 previousRasterOffset = m_effectRasterOffset;
    const glm::vec2 previousCompositeOffset = m_effectCompositeOffset;
    const glm::vec2 previousQuadOffset = m_quadOffset;
    bool rebuiltGlyphs = false;
    const unsigned int fontSize = currentFontSize ();
    if (fontSize != m_lastFontSize) {
	m_lastFontSize = fontSize;
        m_msdfGlyphs.clear ();
	FT_Set_Char_Size (m_ftFace, 0, m_lastFontSize, 300, 300);
	for (const auto& [key, face] : m_fallbackFaces) {
	    FT_Set_Char_Size (face, 0, m_lastFontSize, 300, 300);
	}
	rebuildTextureFrom (renderedText);
	rebuiltGlyphs = true;
    } else if (renderedText != m_lastRenderedText || currentLayoutLimits () != m_lastLayoutLimits
	       || currentSpacing () != m_lastSpacing || currentPadding () != m_lastPadding
               || currentStyle ().msdf != m_lastMsdfRequest) {
	rebuildTextureFrom (renderedText);
	rebuiltGlyphs = true;
    }

    // Glyph pixels and their quad are already refreshed. Keep programs, bindings and
    // simulation targets when their tight surface and cached transforms still match.
    // Changed metrics must rebuild so the native target size and effect UVs stay exact.
    const bool effectLayoutChanged = previousSurface != m_effectSurface
	|| previousRasterOffset != m_effectRasterOffset || previousCompositeOffset != m_effectCompositeOffset
	|| previousQuadOffset != m_quadOffset;
    if ((rebuiltGlyphs && m_effectsEnabled && effectLayoutChanged)
	|| m_text.colorBlendMode->value->getInt () != m_colorBlendMode) {
	destroyEffectChain ();
	try {
	    setupEffectChain ();
	} catch (const std::exception& e) {
	    sLog.error ("CText: effect-chain rebuild failed for '", m_text.name, "': ", e.what ());
	    destroyEffectChain ();
	}
    }

    // Native composites text directly while every authored effect is hidden.
    // Published layer textures still need refreshing for other layers to sample.
    const bool compositeEffects = m_effectsEnabled
	&& (!drawToScene || m_colorBlendPass != nullptr
	    || std::ranges::any_of (m_text.effects, [] (const auto& effect) { return effect->visible->value->getBool (); }));

    const auto style = currentStyle ();
    const std::array<glm::vec4, 4> renderVars { style.renderVar0, style.renderVar1, style.renderVar2, style.renderVar3 };
    glUseProgram (m_program);
    glUniform1i (m_uMsdf, m_usingMsdf);
    glUniform3i (m_uStyleFlags, style.enabled.x, style.enabled.y, style.enabled.z);
    glUniform4fv (m_uRenderVars, 4, glm::value_ptr (renderVars[0]));
    glUniform2f (m_uTextureSize, m_textureSize.x, m_textureSize.y);

    const glm::vec4 color = m_text.color->value->getVec4 ();
    const float alpha = m_text.alpha->evaluateFloat (getScene ().getElapsedTime ());
    // Native text draws (140257d70 / 140258050) multiply RGB by brightness
    // only with the HDR bloom flag (0x2000). LDR projects may save zero here.
    const float brightness = getScene ().isHdr ()
	? m_text.brightness->evaluateFloat (getScene ().getElapsedTime ()) : 1.0f;

    // A preceding hidden image can leave its composite FBO and viewport bound.
    // Direct text draws must select the active scene/composition target themselves.
    const auto sceneTarget = this->getScene ().getActiveRenderTarget ();
    glBindFramebuffer (GL_FRAMEBUFFER, sceneTarget->getDrawFramebuffer ());
    glViewport (0, 0, sceneTarget->getRealWidth (), sceneTarget->getRealHeight ());
    // Glyph plates are double-sided, including the mirrored 2D path and the
    // initial raster pass of an effect chain. Never inherit image-material culling.
    glDisable (GL_CULL_FACE);
    glDisable (GL_SAMPLE_ALPHA_TO_COVERAGE);
    glColorMask (GL_TRUE, GL_TRUE, GL_TRUE, getScene ().isRenderingToComposition () ? GL_TRUE : GL_FALSE);
    glBlendEquationSeparate (GL_FUNC_ADD, getScene ().usesCompositionAlphaMax () ? GL_MAX : GL_FUNC_ADD);
    if (depthTestEnabled ()) glEnable (GL_DEPTH_TEST);
    else glDisable (GL_DEPTH_TEST);
    glDepthFunc (sceneTarget->usesReverseDepth () ? GL_GREATER : GL_LESS);
    // Native font materials are translucent: they read depth but never write it.
    glDepthMask (GL_FALSE);

    const ClipDepthScope clipDepth (getScene ().getCamera ().usesReverseDepth ());

    // 3D scenes: no screen-space centering or parallax; the world matrix carries the
    // transform chain (origin, parents, text scale)
    if (getScene ().getScene ().camera.projection.isPerspective) {
	// the shared VBO bakes the 2D path's vflip into its UVs (glyph top at -y); world
	// space is y-up, so mirror the centered quad back around its own center
	const glm::mat4 model
	    = this->resolveWorldMatrix () * glm::scale (glm::mat4 (1.0f), glm::vec3 (1.0f, -1.0f, 1.0f));
	const glm::mat4 mvp = getScene ().getCamera ().getRasterProjection () * getScene ().getCamera ().getLookAt () * model;

	if (m_effectsEnabled) {
	    renderEffectChain (mvp, brightness, alpha, compositeEffects && drawToScene);
	    if (compositeEffects) {
#if !NDEBUG
		glPopDebugGroup ();
#endif /* DEBUG */
		return;
	    }
	    glColorMask (GL_TRUE, GL_TRUE, GL_TRUE, getScene ().isRenderingToComposition () ? GL_TRUE : GL_FALSE);
	}

	drawBackground (mvp, alpha);
	glEnable (GL_BLEND);
	glBlendFunc (GL_SRC_ALPHA, m_text.colorBlendMode->value->getInt () == 31 ? GL_ONE : GL_ONE_MINUS_SRC_ALPHA);
	glUseProgram (m_program);
	glUniformMatrix4fv (m_uMVP, 1, GL_FALSE, glm::value_ptr (mvp));
	glUniform4f (m_uColor, color.r * brightness, color.g * brightness, color.b * brightness, color.a * alpha);

	glActiveTexture (GL_TEXTURE0);
	glBindTexture (GL_TEXTURE_2D, m_texture);
	glUniform1i (m_uTexture, 0);

	drawGlyphs ();
        glColorMask (GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
#if !NDEBUG
	glPopDebugGroup ();
#endif /* DEBUG */
	return;
    }

    const float scene_w = getScene ().getCamera ().getWidth ();
    const float scene_h = getScene ().getCamera ().getHeight ();

    // Scene coordinates are y-up (bottom-left origin) — the same convention CImage maps with
    // (x - w/2, h/2 - y) before the Wayland/GLFW vflip corrects the presentation on screen.
    // Mirror the composed world transform (origin, parent chain, text scale — via
    // resolveWorldMatrix) into that convention, then re-mirror the quad around its own center
    // so the glyph texture stays upright. Verified against MyGO 3558034522: the clock stack
    // sits at 0.93 of the canvas height (near the top) and its Date/Clock children hang at
    // negative offsets (below the container): day line, then date, then time.
    const glm::vec3 parallaxShift (this->resolveParallaxOffset (), 0.0f);

    const glm::mat4 model = glm::translate (glm::mat4 (1.0f), parallaxShift)
	* glm::scale (glm::mat4 (1.0f), glm::vec3 (1.0f, -1.0f, 1.0f))
	* glm::translate (glm::mat4 (1.0f), glm::vec3 (-scene_w * 0.5f, -scene_h * 0.5f, 0.0f))
	* this->resolveWorldMatrix () * glm::scale (glm::mat4 (1.0f), glm::vec3 (1.0f, -1.0f, 1.0f));

    const auto& camera = getScene ().getCamera ();
    glm::mat4 viewProjection = camera.getProjection () * camera.getLookAt ();
    if (m_text.perspective->value->getBool ()) {
	// Native text shares the image draw path and its virtual perspective camera.
	const float fov = glm::radians (glm::clamp (camera.getFov (), 0.1f, 179.9f));
	const float distance = 1.0f / (std::tan (fov * 0.5f) * camera.getProjection ()[1][1]);
	glm::mat4 view = camera.getLookAt ();
	view[3][2] = -distance;
	const auto& output = getScene ().getOutputSize ();
	viewProjection = glm::perspective (fov, output.x / output.y, 5.0f,
	    glm::max (15000.0f, distance + 1000.0f)) * view;
    }
    const glm::mat4 mvp = viewProjection * model;

    if (m_effectsEnabled) {
	renderEffectChain (mvp, brightness, alpha, compositeEffects && drawToScene);
	if (compositeEffects) {
#if !NDEBUG
	    glPopDebugGroup ();
#endif /* DEBUG */
	    return;
	}
	glColorMask (GL_TRUE, GL_TRUE, GL_TRUE, getScene ().isRenderingToComposition () ? GL_TRUE : GL_FALSE);
    }

    drawBackground (mvp, alpha);
    glEnable (GL_BLEND);
    glBlendFunc (GL_SRC_ALPHA, m_text.colorBlendMode->value->getInt () == 31 ? GL_ONE : GL_ONE_MINUS_SRC_ALPHA);

    glUseProgram (m_program);
    glUniformMatrix4fv (m_uMVP, 1, GL_FALSE, glm::value_ptr (mvp));
    glUniform4f (m_uColor, color.r * brightness, color.g * brightness, color.b * brightness, color.a * alpha);

    glActiveTexture (GL_TEXTURE0);
    glBindTexture (GL_TEXTURE_2D, m_texture);
    glUniform1i (m_uTexture, 0);

    drawGlyphs ();
    glColorMask (GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
#if !NDEBUG
    glPopDebugGroup ();
#endif /* DEBUG */
}
