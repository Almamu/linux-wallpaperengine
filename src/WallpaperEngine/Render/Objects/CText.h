#pragma once

#include "Effects/EffectSwapCommand.h"
#include "TextStyle.h"
#include "TextMsdf.h"

#include <functional>
#include <memory>
#include <map>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <GL/glew.h>
#include <glm/mat3x3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

#include "WallpaperEngine/Render/CObject.h"
#include "WallpaperEngine/Scripting/ScriptableObject.h"

// Forward-declare FreeType types to avoid leaking the header into users.
struct FT_LibraryRec_;
struct FT_FaceRec_;
struct FT_Bitmap_;
typedef struct FT_LibraryRec_* FT_Library;
typedef struct FT_FaceRec_* FT_Face;

namespace WallpaperEngine::Render::Wallpapers {
class CScene;
}

namespace WallpaperEngine::Render {
class CFBO;
class FBOProvider;
}

namespace WallpaperEngine::Render::Objects::Effects {
class CPass;
}

namespace WallpaperEngine::Render::Objects {
using namespace WallpaperEngine::Data::Model;

/** Decodes the UTF-8 codepoint starting at @p offset and advances it past the sequence.
 *  Malformed bytes decode as U+FFFD one byte at a time. No overlong/range validation —
 *  the result only feeds glyph lookup, where a bogus codepoint just misses the charmap. */
uint32_t nextUtf8Codepoint (const std::string& text, size_t& offset);

/** Composite a FreeType coverage bitmap over the classic text raster. */
void blitTextBitmap (
    const FT_Bitmap_& bitmap, glm::ivec2 origin, glm::ivec2 size, std::vector<uint8_t>& coverage
);

struct TextLayoutLimits {
    float width = 0.0f; // zero disables wrapping
    size_t rows = 0; // zero disables truncation
    bool ellipsis = false;
    bool operator== (const TextLayoutLimits&) const = default;
};

struct TextGlyphMetrics {
    float advance;
    float right; // hinted outline CBox xMax, relative to the glyph pen
};

struct TextShapedGlyph {
    FT_Face face;
    uint32_t index;
    uint32_t code;
    size_t cluster; // UTF-8 source offset, transported from the UTF-32 HB cluster
    size_t runStart;
    float advance;
    float right; // hinted CBox xMax including the integer shaping offset
    int xOffset;
    int yOffset;
    bool safeBreak;
};

struct TextShapedLine {
    std::string text;
    std::vector<TextShapedGlyph> glyphs;
};

std::vector<TextShapedGlyph> shapeTextLine (
    const std::string& text, const std::function<FT_Face (uint32_t)>& fontForGlyph
);
std::vector<TextShapedLine> layoutShapedTextLines (
    const std::string& text, const TextLayoutLimits& limits,
    const std::function<std::vector<TextShapedGlyph> (const std::string&)>& shape, float spacing = 0.0f
);

/** Codepoint-metric adapter for callers without a font/shaping stage. */
std::vector<std::string> layoutTextLines (
    const std::string& text, const TextLayoutLimits& limits, const std::function<TextGlyphMetrics (uint32_t)>& glyphMetrics,
    float spacing = 0.0f
);

/**
 * Return Wallpaper Engine's glyph-bounds alignment offset in y-up font space.
 *
 * The native renderer aligns the generated glyph mesh around the layer origin; the
 * serialized layer size is not an inset layout box. Horizontal alignment uses the
 * actual mesh width. Vertical alignment uses FreeType's ascender/descender and the
 * generated row spacing (recovered from wallpaper64.exe's text update routine).
 */
glm::vec2 computeTextAlignmentOffset (
    const std::string& horizontalAlign, const std::string& verticalAlign, const glm::vec4& glyphBounds, float ascender,
    float descender, float lineSpacing, size_t lineCount
);

struct TextEffectLayout {
    /** Native line-box surface dimensions, including authored padding on both sides. */
    glm::vec2 surfaceSize;
    /** Glyph center inside the effect surface, in local +y-down coordinates. */
    glm::vec2 rasterOffset;
    /** Effect-surface center relative to the layer origin, in local +y-down coordinates. */
    glm::vec2 compositeOffset;
};

/**
 * Reproduce Wallpaper Engine's split text-effect placement.
 *
 * The generated font mesh stores a line box expanded to the ascender and full row
 * height. Native code centers glyphs inside that padded box, then aligns the whole
 * effect surface around the layer origin. Keeping those two offsets separate matters
 * to resolution-dependent effects even though their sum equals direct glyph placement.
 */
TextEffectLayout computeTextEffectLayout (
    const std::string& horizontalAlign, const std::string& verticalAlign, const glm::vec4& glyphBounds, float ascender,
    float descender, float lineSpacing, size_t lineCount, const glm::vec2& padding
);

/**
 * FreeType text renderer with live SceneScript properties, authored layout,
 * parent transforms and optional material effect passes.
 */
class CText final : virtual public CObject, public Scripting::ScriptableObject {
public:
    CText (Wallpapers::CScene& scene, const Text& text);
    ~CText () override;

    void setup () override;
    void render () override;
    [[nodiscard]] bool isVolumetricBarrier () const override;
    void prepareRenderEffects () { updateEffectVisibility (); }
    [[nodiscard]] glm::vec2 getSize ();
    [[nodiscard]] std::optional<glm::mat3> transformAttachmentToTexture (
        const CObject& source, const glm::mat4& attachmentWorld
    );

private:
    // Rebuilds the glyph texture (and matching quad VBO) from the given string.
    // Reuses existing GL handles if already allocated, so this is safe to call
    // every time the rendered text changes.
    void rebuildTextureFrom (const std::string& text);
    void buildShader ();
    void uploadQuadVertices ();
    void drawGlyphs ();
    TextStyleUniforms currentStyle () const;

    // setup() helpers (kept small to keep the setup flow linear).
    bool initFreeType ();
    bool loadEmbeddedFont ();
    bool loadSystemFont ();
    FT_Face fontForGlyph (uint32_t code);
    unsigned int currentFontSize () const;
    TextLayoutLimits currentLayoutLimits () const;
    glm::vec2 currentSpacing () const;
    glm::vec2 currentPadding () const;
    bool depthTestEnabled () const;
    void drawBackground (const glm::mat4& mvp, float alpha);

    // text-effect chain (rebuilt when dynamic glyph metrics change, like the native renderer)
    void setupEffectChain ();
    void updateEffectVisibility ();
    void destroyEffectChain ();
    void renderEffectChain (const glm::mat4& mvp, float brightness, float alpha, bool drawToScene);

    const Text& m_text;
    std::string m_lastRenderedText;
    std::optional<std::string> m_lastSizedText;
    glm::vec2 m_scriptSize = { 0.0f, 0.0f };
    glm::vec2 m_scriptAlignmentOffset = { 0.0f, 0.0f };
    unsigned int m_lastFontSize = 0;
    TextLayoutLimits m_lastLayoutLimits;
    glm::vec2 m_lastSpacing = glm::vec2 (0.0f);
    glm::vec2 m_lastPadding = glm::vec2 (32.0f);

    FT_Library m_ftLibrary = nullptr;
    FT_Face m_ftFace = nullptr;
    std::unordered_map<std::string, FT_Face> m_fallbackFaces;
    std::unordered_map<uint32_t, FT_Face> m_glyphFaces;
    /** shared by every text object using the same font file; must outlive m_ftFace */
    std::shared_ptr<const std::vector<uint8_t>> m_fontData;

    /**
     * Create the quad/composite VAOs on first use. VAOs are container objects and are
     * NOT shared between GL contexts: async-built wallpapers upload the VBOs on the
     * worker context, but the VAOs must be created lazily on the render thread.
     */
    void ensureVao ();
    void ensureCompositeVao ();

    GLuint m_texture = 0;
    GLuint m_program = 0;
    GLuint m_vao = 0;
    GLuint m_vbo = 0;
    GLuint m_glyphVbo = 0;
    GLuint m_glyphVao = 0;
    GLsizei m_glyphVertexCount = 0;
    bool m_usingMsdf = false;
    bool m_lastMsdfRequest = false;
    std::map<std::pair<FT_Face, uint32_t>, TextMsdfGlyph> m_msdfGlyphs;

    GLint m_uMVP = -1;
    GLint m_uColor = -1;
    GLint m_uTexture = -1;
    GLint m_uSolid = -1;
    GLint m_uMsdf = -1;
    GLint m_uStyleFlags = -1;
    GLint m_uRenderVars = -1;
    GLint m_uTextureSize = -1;

    glm::ivec2 m_textureSize = { 0, 0 };
    glm::vec2 m_quadSize = { 0.0f, 0.0f };
    /** glyph bbox center relative to the layer origin, local +y-down space */
    glm::vec2 m_quadOffset = { 0.0f, 0.0f };

    // ---- text-effect chain state (only used when the object authors effects) ----
    /** satisfies the CRenderable contract for CPass (uniform sources, FBO resolution) */
    std::unique_ptr<class CTextEffectHost> m_effectHost;
    Data::Model::MaterialUniquePtr m_effectMaterial;
    std::vector<Effects::CPass*> m_effectPasses;
    std::vector<Effects::CPass*> m_activeEffectPasses;
    std::unordered_map<Effects::CPass*, const UserSetting*> m_passVisibility;
    std::unordered_set<Effects::CPass*> m_copyPasses;
    std::unordered_set<const UserSetting*> m_failedEffects;
    std::vector<Effects::EffectSwapCommand> m_swapCommands;
    std::vector<std::shared_ptr<const FBOProvider>> m_effectProviders;
    std::shared_ptr<CFBO> m_fboA;
    std::shared_ptr<CFBO> m_fboB;
    /** Stable published result; other layers keep references across glyph rebuilds. */
    std::shared_ptr<CFBO> m_publishedFBO;
    std::shared_ptr<const CFBO> m_effectResult;
    std::unique_ptr<Effects::CPass> m_colorBlendPass;
    std::unique_ptr<ImageEffectPassOverride> m_colorBlendOverride;
    std::shared_ptr<FBOProvider> m_colorBlendProvider;
    std::shared_ptr<CFBO> m_blendBackground;
    glm::mat4 m_colorBlendMVP { 1.0f };
    int m_colorBlendMode = 0;
    GLuint m_ndcPosition = 0;
    GLuint m_passTexCoord = 0;
    GLuint m_compositeVao = 0;
    GLuint m_compositeVbo = 0;
    GLuint m_compositeProgram = 0;
    GLint m_cuMVP = -1;
    GLint m_cuTexture = -1;
    glm::mat4 m_baseMVP = glm::mat4 (1.0f);
    glm::vec2 m_effectSurface = { 0.0f, 0.0f };
    glm::vec2 m_effectRasterOffset = { 0.0f, 0.0f };
    glm::vec2 m_effectCompositeOffset = { 0.0f, 0.0f };
    bool m_effectsEnabled = false;

    bool m_valid = false;
};
} // namespace WallpaperEngine::Render::Objects
