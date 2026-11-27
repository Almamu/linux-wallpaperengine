#include "ShaderUnit.h"
#include "ShaderDiskCache.h"
#include "WallpaperEngine/Debug/RenderHealth.h"

#include "WallpaperEngine/Debug/DebugHelpers.h"
#include "WallpaperEngine/Logging/Log.h"
#include <array>
#include <cctype>
#include <cstdint>
#include <exception>
#include <map>
#include <mutex>
#include <optional>
#include <ranges>
#include <regex>
#include <set>
#include <sstream>
#include <stack>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

#include "GLSLContext.h"
#include "WallpaperEngine/Assets/AssetLoadException.h"
#include "WallpaperEngine/Render/Shaders/Variables/ShaderVariable.h"
#include "WallpaperEngine/Render/Shaders/Variables/ShaderVariableFloat.h"
#include "WallpaperEngine/Render/Shaders/Variables/ShaderVariableInteger.h"
#include "WallpaperEngine/Render/Shaders/Variables/ShaderVariableVector2.h"
#include "WallpaperEngine/Render/Shaders/Variables/ShaderVariableVector3.h"
#include "WallpaperEngine/Render/Shaders/Variables/ShaderVariableVector4.h"

#include "WallpaperEngine/Data/Builders/VectorBuilder.h"
#include "WallpaperEngine/FileSystem/Container.h"

#define SHADER_HEADER(filename)                                                                                        \
    "#version 330\n"                                                                                                   \
    "// ======================================================\n"                                                      \
    "// Processed shader "                                                                                             \
	+ filename                                                                                                     \
	+ "\n"                                                                                                         \
	  "// ======================================================\n"                                                \
	  "precision highp float;\n"                                                                                   \
	  "#define mul(x, y) ((y) * (x))\n"                                                                            \
	  "#define lerp mix\n"                                                                                         \
	  "#define frac fract\n"                                                                                       \
	  "#define CAST2(x) (vec2(x))\n"                                                                               \
	  "#define CAST3(x) (vec3(x))\n"                                                                               \
	  "#define CAST4(x) (vec4(x))\n"                                                                               \
	  "#define CAST3X3(x) (mat3(x))\n"                                                                             \
	  "#define CASTF(x) (float(x))\n"                                                                              \
	  "#define CASTU(x) (uint(x))\n"                                                                               \
	  "#define float2 vec2\n"                                                                                      \
	  "#define float3 vec3\n"                                                                                      \
	  "#define float4 vec4\n"                                                                                      \
	  "#define int2 ivec2\n"                                                                                       \
	  "#define int3 ivec3\n"                                                                                       \
	  "#define int4 ivec4\n"                                                                                       \
	  "#define saturate(x) (clamp(x, 0.0, 1.0))\n"                                                                 \
	  "#define DECLARE_SAMPLER2D_PARAMETER(name) in sampler2D name\n"                                              \
	  "#define MAKE_SAMPLER2D_ARGUMENT(name) name\n"                                                               \
	  "#define texSample2D texture\n"                                                                              \
	  "#define texSample3D texture\n"                                                                              \
	  "#define texSample2DLod textureLod\n"                                                                        \
	  "#define sampler2DBackBuffer sampler2D\n"                                                                    \
	  "#define texSample2DBackBuffer(s, uv, size) texture((s), (uv))\n"                                             \
	  "#define texLoad2D(s, uv, size) texelFetch((s), ivec2((uv) * (size)), 0)\n"                                  \
	  "#define sampler2DComparison sampler2DShadow\n"                                                              \
	  "#define texSample2DCompare(s, uv, reference) vec4(texture((s), vec3((uv), (reference))))\n"                   \
	  "#define log10(x) (log2(x) * 0.301029995663981)\n"                                                           \
	  "#define atan2 atan\n"                                                                                       \
	  "#define fmod(x, y) ((x)-(y)*trunc((x)/(y)))\n"                                                              \
	  "#define ddx dFdx\n"                                                                                         \
	  "#define ddy(x) dFdy(-(x))\n"                                                                                \
	  "#define GLSL 1\n\n";
#define FRAGMENT_SHADER_DEFINES                                                                                        \
    "out vec4 out_FragColor;\n"                                                                                        \
    "#define varying in\n"                                                                                           \
    "void clip(float x) { if (x < 0.0) discard; }\n"                                                                  \
    "void clip(vec2 x) { if (any(lessThan(x, vec2(0.0)))) discard; }\n"                                               \
    "void clip(vec3 x) { if (any(lessThan(x, vec3(0.0)))) discard; }\n"                                               \
    "void clip(vec4 x) { if (any(lessThan(x, vec4(0.0)))) discard; }\n"
#define VERTEX_SHADER_DEFINES                                                                                          \
    "#define attribute in\n"                                                                                           \
    "#define varying out\n"

using namespace WallpaperEngine::Render;
using namespace WallpaperEngine::Data::Builders;
using namespace WallpaperEngine::Render::Shaders;

namespace {
std::string spliceShaderLines (std::string source) {
    // Authored shaders use HLSL-style continuations, including outside macros.
    // Join logical lines before reading includes, comments, or parameter JSON;
    // GLSL 330 otherwise rejects the backslash even in inactive branches.
    size_t position = 0;
    while ((position = source.find ('\\', position)) != std::string::npos) {
	size_t newline = position + 1;
	if (newline < source.size () && source[newline] == '\r') {
	    newline++;
	}
	if (newline < source.size () && source[newline] == '\n') {
	    source.erase (position, newline - position + 1);
	} else {
	    position++;
	}
    }
    return source;
}

bool isStockGeneric4 (std::string_view source) {
    // FNV-1a of the Steam generic4.frag (build 23967692), with LF line endings.
    // A project can override the stock filename with its own shader.
    uint64_t hash = 14695981039346656037ULL;
    for (size_t i = 0; i < source.size (); ++i) {
	const auto byte = static_cast<unsigned char> (source[i]);
	if (byte == '\r' && i + 1 < source.size () && source[i + 1] == '\n') continue;
	hash = (hash ^ byte) * 1099511628211ULL;
    }
    return hash == 0xf8cc40571b3dae05ULL;
}
} // namespace

ShaderUnit::ShaderUnit (
    const GLSLContext::UnitType type, std::string file, std::string content, const AssetLocator& assetLocator,
    const ShaderConstantMap& constants, const TextureMap& passTextures, const TextureMap& overrideTextures,
    const ComboMap& combos, const ComboMap& overrideCombos
) :
    m_type (type), m_file (std::move (file)), m_content (std::move (content)), m_combos (combos),
    m_overrideCombos (overrideCombos), m_constants (constants), m_passTextures (passTextures),
    m_overrideTextures (overrideTextures), m_link (nullptr), m_assetLocator (assetLocator) {
    // pre-process the shader so the units are clear
    this->preprocess ();
}

void ShaderUnit::preprocess () {
    this->m_preprocessed = spliceShaderLines (this->m_content);
    this->m_includes = "";

    // OpenGL skips alpha-to-coverage on single-sample targets. Emulate the
    // ordered 2x2 mask measured on the D3D11/DXVK reference (RTX 3080); D3D
    // leaves the actual coverage conversion to the hardware.
    if (this->m_type == GLSLContext::UnitType_Fragment
	&& (this->m_file == "generic4" || this->m_file == "generic4.frag") && isStockGeneric4 (this->m_content)) {
	constexpr std::string_view discard = "if (gl_FragColor.a < 0.5) discard;";
	if (const auto pos = this->m_preprocessed.find (discard); pos != std::string::npos) {
	    this->m_preprocessed.replace (pos, discard.size (),
		"if (lwe_SingleSampleCoverageHeight != 0) {\n"
		"    ivec2 coveragePixel = ivec2(gl_FragCoord.xy);\n"
		"    if (lwe_SingleSampleCoverageHeight > 0) {\n"
		"        coveragePixel.y = lwe_SingleSampleCoverageHeight - 1 - coveragePixel.y;\n"
		"    }\n"
		"    const vec4 coverageThresholds = vec4(0.125, 0.625, 0.875, 0.375);\n"
		"    int coveragePhase = 2 * (coveragePixel.y & 1) + (coveragePixel.x & 1);\n"
		"    if (gl_FragColor.a < coverageThresholds[coveragePhase]) discard;\n"
		"}");
	    this->m_preprocessed.insert (0,
		"#if ALPHATOCOVERAGE\nuniform int lwe_SingleSampleCoverageHeight;\n#endif\n");
	}
    }

    // CPU puppet skinning supplies native mask alpha separately from color alpha:
    // clippingmaskimage4 uses raw target weights, without spatial modifiers.
    if (this->m_file == "clippingmaskimage4" && this->m_combos.contains ("PUPPET_MORPH_ALPHA")
	&& this->m_combos.at ("PUPPET_MORPH_ALPHA") != 0) {
	if (this->m_type == GLSLContext::UnitType_Vertex) {
	    this->m_preprocessed = "attribute vec4 a_Color;\nvarying float v_PuppetMaskAlpha;\n"
		+ std::regex_replace (this->m_preprocessed, std::regex (R"(\bvoid\s+main\s*\(\s*\)\s*\{)"),
		    "void main() {\nv_PuppetMaskAlpha = a_Color.x;");
	} else {
	    this->m_preprocessed = "varying float v_PuppetMaskAlpha;\n"
		+ std::regex_replace (this->m_preprocessed, std::regex (R"(\bcolor\.r\s*\*=\s*color\.a\s*;)"),
		    "color.a *= v_PuppetMaskAlpha;\ncolor.r *= color.a;");
	}
    }

    this->preprocessIncludes ();
    this->preprocessRequires ();
    this->preprocessVaryingDeclarations ();
    this->preprocessReservedIdentifiers ();
    this->preprocessVariables ();
    this->stripOrphanedEndifs ();

    // replace gl_FragColor with the equivalent
    const std::string from = "gl_FragColor";
    const std::string to = "out_FragColor";

    size_t start_pos = 0;
    while ((start_pos = this->m_preprocessed.find (from, start_pos)) != std::string::npos) {
	this->m_preprocessed.replace (start_pos, from.length (), to);
	start_pos += to.length (); // Handles case where 'to' is a substring of 'from'
    }
}

void ShaderUnit::preprocessVariables () {
    size_t start = 0, end = 0;
    while ((end = this->m_preprocessed.find ('\n', start)) != std::string::npos) {
	// Extract a line from the string
	std::string line = this->m_preprocessed.substr (start, end - start);
	const size_t combo = line.find ("// [COMBO] ");
	const size_t uniform = line.find ("uniform");
	const size_t comment = line.find ("// ");
	const size_t semicolon = line.find (';');

	if (combo != std::string::npos) {
	    this->parseComboConfiguration (line.substr (combo + strlen ("// [COMBO] ")), 0);
	} else if (
	    uniform != std::string::npos && comment != std::string::npos && semicolon != std::string::npos &&
	    // this check ensures that the comment is after the semicolon (so it's not a commented-out line)
	    // this needs further refining as it's not taking into account block comments
	    uniform < semicolon && semicolon < comment
	) {
	    // The final two declaration tokens are the type and name, regardless of
	    // alignment whitespace or a preceding precision qualifier.
	    std::istringstream declaration (line.substr (uniform, semicolon - uniform));
	    std::string keyword, type, name, token;
	    declaration >> keyword;
	    if (keyword == "uniform") {
		while (declaration >> token) {
		    type = std::move (name);
		    name = std::move (token);
		}
		if (!type.empty ()) {
		    this->parseParameterConfiguration (type, name, line.substr (comment + 2));
		}
	    }
	}

	// Move to the next line
	start = end + 1;
    }
}

namespace {
std::string maskShaderComments (const std::string& source) {
    std::string code = source;
    const auto blank = [&code] (size_t begin, size_t end) {
        for (size_t i = begin; i < end; ++i) {
            if (code[i] != '\n') code[i] = ' ';
        }
    };
    // Mask comments/strings without changing offsets or source text.
    for (size_t i = 0; i < code.size ();) {
        // Only these characters can open a comment or string; skipping straight to them
        // matters because every shader pass masks its whole include-expanded source
        if (code[i] != '/' && code[i] != '"' && code[i] != '\'') {
            ++i;
            continue;
        }
        size_t end = i;
        if (code.compare (i, 2, "//") == 0) {
            end = code.find ('\n', i + 2);
            if (end == std::string::npos) end = code.size ();
        } else if (code.compare (i, 2, "/*") == 0) {
            end = code.find ("*/", i + 2);
            end = end == std::string::npos ? code.size () : end + 2;
        } else if (code[i] == '"' || code[i] == '\'') {
            const char quote = code[i];
            for (end = i + 1; end < code.size (); ++end) {
                if (code[end] == '\\' && end + 1 < code.size ()) ++end;
                else if (code[end] == quote) { ++end; break; }
            }
        }
        if (end > i) { blank (i, end); i = end; }
        else ++i;
    }
    return code;
}

void adaptLegacyReflectionProjection (std::string& source) {
    if (source.find ("v_ScreenPos") == std::string::npos) return;
    const size_t headerEnd = source.find ('\n', source.find ("#version"));
    if (headerEnd == std::string::npos) return;
    const std::string code = maskShaderComments (source);
    static const std::regex projection (
	R"(\bvec2\s+([A-Za-z_]\w*)\s*=\s*\(\s*v_ScreenPos\s*\.\s*xy\s*/\s*v_ScreenPos\s*\.\s*z\s*\)\s*\*\s*0\.5\s*\+\s*0\.5\s*;)"
    );
    std::map<std::string, std::set<size_t>> allowedReferences;
    for (std::sregex_iterator it (code.begin (), code.end (), projection), end; it != end; ++it) {
	allowedReferences.try_emplace ((*it)[1].str (), std::set<size_t> { static_cast<size_t> (it->position (1)) });
    }
    if (allowedReferences.empty ()) return;

    static const std::regex lookup (
	R"(\btexture(?:Lod)?\s*\(\s*g_Texture([0-9])\s*,)"
    );
    static const std::regex token (R"([A-Za-z_]\w*|\+\+|--|\S)");
    static const std::set<std::string_view> constructors { "vec2", "vec3", "vec4", "float", "int", "uint", "bool" };
    std::map<size_t, std::pair<std::string, std::string>> replacements;
    for (std::sregex_iterator it (code.begin (), code.end (), lookup), end; it != end; ++it) {
	const size_t argumentStart = it->position () + it->length ();
	std::vector<bool> pureContext { true };
	std::vector<std::pair<size_t, std::string>> references;
	std::string previous;
	bool mutates = false;
	for (std::sregex_iterator part (code.begin () + argumentStart, code.end (), token); part != end; ++part) {
	    const std::string value = part->str ();
	    if ((value == ")" || value == ",") && pureContext.size () == 1) break;
	    if (value == "=" || value == "++" || value == "--") mutates = true;
	    if (value == "(" || value == "[") {
		const bool call = value == "(" && !previous.empty ()
		    && (std::isalpha (static_cast<unsigned char> (previous.front ())) || previous.front () == '_');
		pureContext.push_back (pureContext.back () && (!call || constructors.contains (previous)));
	    } else if (value == ")" || value == "]") {
		if (pureContext.size () == 1) break;
		pureContext.pop_back ();
	    } else if (pureContext.back () && allowedReferences.contains (value)) {
		references.emplace_back (argumentStart + part->position (), value);
	    }
	    previous = value;
	}
	if (mutates) continue;
	for (const auto& [position, uv] : references) {
	    allowedReferences.at (uv).insert (position);
	    replacements.try_emplace (position, uv, (*it)[1].str ());
	}
    }
    // Rewrite only an immutable local read by texture lookups. Any other
    // reference may be a second declaration, a write, or an escaped value.
    static const std::regex identifier (R"(\b[A-Za-z_]\w*\b)");
    for (std::sregex_iterator it (code.begin (), code.end (), identifier), end; it != end; ++it) {
	const auto refs = allowedReferences.find (it->str ());
	if (refs != allowedReferences.end () && !refs->second.contains (it->position ())) allowedReferences.erase (refs);
    }
    bool changed = false;
    for (const auto& [position, replacement] : replacements | std::views::reverse) {
	const auto& [uv, slot] = replacement;
	if (!allowedReferences.contains (uv)) continue;
	source.replace (position, uv.size (),
	    "lwe_LegacyProjectedUV(" + uv + ", v_ScreenPos.y, v_ScreenPos.z, " + slot + ")");
	changed = true;
    }
    if (!changed) return;
    // Restore the native projection basis only for a lookup of the legacy target.
    // Offsets added after projection and raw texture coordinates keep their own units.
    source.insert (headerEnd + 1, R"(uniform int lwe_LegacyReflectionSamplers;
vec2 lwe_LegacyProjectedUV(vec2 uv, float clipY, float clipW, int slot) {
    if ((lwe_LegacyReflectionSamplers & (1 << slot)) != 0)
        uv.y -= clipY / clipW;
    return uv;
}
)");
}

void normalizeAudioSampleConditions (std::string& source) {
    if (source.find ("AUDIOSAMPLES") == std::string::npos) return;
    // The stock HLSL preprocessor selects these branches despite reporting the
    // trailing semicolon. Preserve the selected branch when translating to GLSL.
    static const std::regex terminator (
	R"((^|\n)[ \t]*#[ \t]*elif[ \t]+AUDIOSAMPLES[ \t]*==[ \t]*(?:32|64)[ \t]*(;)[ \t\r]*(?=\n|$))"
    );
    const std::string code = maskShaderComments (source);
    for (std::sregex_iterator it (code.begin (), code.end (), terminator), end; it != end; ++it) {
	source[it->position (2)] = ' ';
    }
}

void flattenPackedAudioSpectrumIndices (std::string& source) {
    if (source.find ("g_AudioSpectrum") == std::string::npos) return;

    // Audio rings can address a flat spectrum uniform as groups of four bands.
    // The renderer uploads a scalar array, so unite the quotient and remainder
    // of the same band index before GLSL type-checks the subscripts.
    static const std::regex packedIndex (
	R"(\b(g_AudioSpectrum(?:16|32|64)(?:Left|Right))[ \t]*\[[ \t]*([A-Za-z_]\w*)[ \t]*/[ \t]*4[ \t]*\][ \t]*\[[ \t]*\2[ \t]*%[ \t]*4[ \t]*\])");
    const std::string code = maskShaderComments (source);
    const auto first = std::sregex_iterator (code.begin (), code.end (), packedIndex);
    const auto end = std::sregex_iterator ();
    if (first == end) return;

    static const std::regex spectrumUniform (
	R"(\buniform\s+(?:(?:lowp|mediump|highp)\s+)?float\s+(g_AudioSpectrum(16|32|64)(?:Left|Right))\s*\[\s*\2\s*\]\s*;)");
    std::set<std::string> flatUniforms;
    for (auto it = std::sregex_iterator (code.begin (), code.end (), spectrumUniform); it != end; ++it) {
	flatUniforms.insert ((*it)[1].str ());
    }

    // Record offsets against the original text and edit from the end.
    std::vector<std::smatch> replacements;
    for (auto it = first; it != end; ++it) {
	if (flatUniforms.contains ((*it)[1].str ())
	    && source.compare (it->position (), it->length (), code, it->position (), it->length ()) == 0) {
	    replacements.push_back (*it);
	}
    }
    for (auto it = replacements.rbegin (); it != replacements.rend (); ++it) {
	source.replace (it->position (), it->length (), (*it)[1].str () + "[int(" + (*it)[2].str () + ")]");
    }
}

void narrowRgbProducts (std::string& source) {
    // HLSL truncates the wider operand of a component-wise product. The scalar
    // tail of this constructor is its alpha, so only narrow the RGB product.
    static const std::regex product (
	R"(\b(?:vec4|float4)\s*\(\s*([A-Za-z_]\w*)\s*\*\s*([A-Za-z_]\w*)\s*,\s*[+-]?(?:[0-9]+(?:\.[0-9]*)?|\.[0-9]+)(?:[eE][+-]?[0-9]+)?\s*\))"
    );
    if (!std::regex_search (source, product)) return;

    const std::string code = maskShaderComments (source);
    const auto width = [&code] (const std::string& name) {
	const std::regex declaration (R"(\b([A-Za-z_]\w*)\s+)" + name + R"(\b\s*(?=[=;,\)\[])(\[)?)");
	const std::regex ambiguous (R"(#\s*define\s+)" + name + R"(\b|,\s*)" + name + R"(\b\s*(?=[=;,\[]))");
	if (std::regex_search (code, ambiguous)) return 0;
	int agreedWidth = 0;
	for (std::sregex_iterator match (code.begin (), code.end (), declaration), end; match != end; ++match) {
	    const std::string type = (*match)[1].str ();
	    if ((*match)[2].matched || (type != "vec3" && type != "float3" && type != "vec4" && type != "float4")) {
		return 0;
	    }
	    const int declaredWidth = type.back () - '0';
	    // Includes can reuse common names. Narrow only when all declarations
	    // agree, including declarations inside inactive combo branches.
	    if (agreedWidth != 0 && agreedWidth != declaredWidth) return 0;
	    agreedWidth = declaredWidth;
	}
	return agreedWidth;
    };
    std::vector<size_t> swizzles;
    for (std::sregex_iterator match (code.begin (), code.end (), product), end; match != end; ++match) {
	const int left = width ((*match)[1].str ());
	const int right = width ((*match)[2].str ());
	if ((left == 3 && right == 4) || (left == 4 && right == 3)) {
	    const int wider = left == 4 ? 1 : 2;
	    swizzles.push_back (match->position (wider) + match->length (wider));
	}
    }
    for (const size_t position : swizzles | std::views::reverse) {
	source.insert (position, ".xyz");
    }
}

// Keep native GLSL min/max overloads. Only the HLSL literal-first max idiom
// needs reordering: its literal is known to be scalar, while the other argument
// may be a vector. Each argument is still evaluated once and constants remain
// built-in constant expressions. Never infer types from local variable names.
std::string reorderLiteralMax (std::string source) {
    std::string code = maskShaderComments (source);
    const auto blank = [&code] (size_t begin, size_t end) {
	for (size_t i = begin; i < end; ++i) {
	    if (code[i] != '\n') code[i] = ' ';
	}
    };
    // An authored macro/function owns its own argument semantics.
    static const std::regex customMax (
        R"((^|\n)[ \t]*#[ \t]*(?:define|undef)[ \t]+max\b|\b(?:float[234]?|double|int[234]?|uint|[iu]?vec[234])\s+max\s*\()");
    if (std::regex_search (code, customMax)) return source;
    // Leave directives, including continued macro bodies, to the preprocessor.
    for (size_t begin = 0; begin < code.size ();) {
        size_t end = code.find ('\n', begin);
        if (end == std::string::npos) end = code.size ();
        const size_t first = code.find_first_not_of (" \t\r", begin);
        if (first < end && code[first] == '#') {
            size_t last = end;
            while (last > begin && (code[last - 1] == '\r' || code[last - 1] == ' ' || code[last - 1] == '\t')) --last;
            while (last > begin && code[last - 1] == '\\' && end < code.size ()) {
                end = code.find ('\n', end + 1);
                if (end == std::string::npos) end = code.size ();
                last = end;
                while (last > begin && (code[last - 1] == '\r' || code[last - 1] == ' ' || code[last - 1] == '\t')) --last;
            }
            blank (begin, end);
        }
        begin = end < code.size () ? end + 1 : end;
    }
    static const std::regex literal (
        R"(\s*[+-]?\s*(?:0[xX][0-9a-fA-F]+[uU]?|(?:[0-9]+(?:\.[0-9]*)?|\.[0-9]+)(?:[eE][+-]?[0-9]+)?[uUfF]?)\s*)");
    const auto word = [] (char c) { return std::isalnum (static_cast<unsigned char> (c)) || c == '_'; };
    for (size_t pos = 0; (pos = code.find ("max", pos)) != std::string::npos;) {
        const size_t name = pos;
        pos += 3;
        if ((name > 0 && word (code[name - 1])) || (pos < code.size () && word (code[pos]))) continue;
        const size_t open = code.find_first_not_of (" \t\r\n", pos);
        if (open == std::string::npos || code[open] != '(') continue;
        size_t comma = std::string::npos, close = open + 1;
        int depth = 1;
        bool extraArgument = false;
        for (; close < code.size (); ++close) {
            const char c = code[close];
            if (c == '(' || c == '[') ++depth;
            else if (c == ')' || c == ']') { if (--depth == 0) break; }
            else if (c == ',' && depth == 1) {
                if (comma != std::string::npos) extraArgument = true;
                else comma = close;
            }
        }
        if (close == code.size () || comma == std::string::npos || extraArgument) continue;
        if (!std::regex_match (code.cbegin () + open + 1, code.cbegin () + comma, literal)) continue;
        // Swapping is length-preserving. Resume inside the first argument so
        // nested calls are processed without repeatedly swapping the outer call.
        for (std::string* text : { &source, &code }) {
            const std::string first = text->substr (open + 1, comma - open - 1);
            const std::string second = text->substr (comma + 1, close - comma - 1);
            text->replace (open + 1, close - open - 1, second + "," + first);
        }
        pos = open + 1;
    }
    return source;
}

// memoization cache for the include+ifdef preprocessing passes: their result only
// depends on the unit's source text and the container includes resolve from, so
// repeated builds (combo variants, wallpaper re-switches) can skip the string work
struct PreprocessedIncludes {
    std::string preprocessed;
    std::string includes;
};

std::mutex sIncludeCacheMutex;
std::unordered_map<std::string, PreprocessedIncludes> sIncludeCache;
size_t sIncludeCacheBytes = 0;
constexpr size_t SHADER_TEXT_CACHE_BUDGET_BYTES = 64ULL * 1024 * 1024;

bool hasStatementMacro (const std::string& source) {
    // This scan precedes the compatibility cache, whose key needs the answer.
    // Cache both positive and negative results by exact source, across units.
    static std::mutex cacheMutex;
    static std::unordered_map<std::string, bool> cache;
    static size_t cacheBytes = 0;
    {
        const std::lock_guard lock (cacheMutex);
        if (const auto found = cache.find (source); found != cache.end ()) {
            return found->second;
        }
    }

    static const std::regex statementMacro (R"((^|\n)[ \t]*#[ \t]*define\b[^\n]*;)");
    const bool result = std::regex_search (maskShaderComments (source), statementMacro);
    const size_t entryBytes = source.size () + sizeof (decltype (cache)::value_type);
    if (entryBytes <= SHADER_TEXT_CACHE_BUDGET_BYTES) {
        const std::lock_guard lock (cacheMutex);
        if (cacheBytes + entryBytes > SHADER_TEXT_CACHE_BUDGET_BYTES) {
            cache.clear ();
            cacheBytes = 0;
        }
        if (cache.emplace (source, result).second) cacheBytes += entryBytes;
    }
    return result;
}

std::string buildIncludeCacheKey (const std::string& locatorIdentity, const std::string& file, const std::string& content) {
    std::string key = locatorIdentity;
    key += '|';
    key += file;
    key += '|';
    key += std::to_string (std::hash<std::string> {} (content));
    return key;
}

std::string_view preprocessorDirective (std::string_view line) {
    size_t cursor = 0;
    while (cursor < line.size () && std::isspace (static_cast<unsigned char> (line [cursor]))) {
	cursor++;
    }

    if (cursor == line.size () || line [cursor] != '#') {
	return {};
    }

    cursor++;
    while (cursor < line.size () && std::isspace (static_cast<unsigned char> (line [cursor]))) {
	cursor++;
    }

    const size_t start = cursor;
    while (
	cursor < line.size ()
	&& (std::isalnum (static_cast<unsigned char> (line [cursor])) || line [cursor] == '_')) {
	cursor++;
    }

    return line.substr (start, cursor - start);
}

struct IncludeDirective {
    size_t start;
    size_t end;
    std::string filename;
};

bool validComboName (std::string_view name) {
    const auto letter = [] (char c) { return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_'; };
    return !name.empty () && letter (name.front ())
	&& std::ranges::all_of (name, [&] (char c) { return letter (c) || (c >= '0' && c <= '9'); });
}

std::string comboDefinition (const std::string& name, int value) {
    // Material overrides and linked units can also provide empty/invalid names.
    if (!validComboName (name)) {
	return {};
    }
    return "#define " + name + " " + std::to_string (value) + "\n";
}

std::optional<IncludeDirective> nextInclude (const std::string& source, const std::string& file) {
    bool blockComment = false;
    size_t lineNumber = 1;
    for (size_t start = 0; start < source.size (); lineNumber++) {
	const size_t newline = source.find ('\n', start);
	const size_t end = newline == std::string::npos ? source.size () : newline;
	std::string line = source.substr (start, end - start);
	bool quoted = false;
	// Mask comments without changing positions, including block comments that
	// span lines. Quotes in real include paths must remain available below.
	for (size_t i = 0; i < line.size (); i++) {
	    if (blockComment) {
		if (line[i] == '*' && i + 1 < line.size () && line[i + 1] == '/') {
		    line[i++] = ' ';
		    blockComment = false;
		}
		line[i] = ' ';
	    } else if (quoted && line[i] == '\\' && i + 1 < line.size ()) {
		i++;
	    } else if (line[i] == '"') {
		quoted = !quoted;
	    } else if (!quoted && line[i] == '/' && i + 1 < line.size ()) {
		if (line[i + 1] == '/') {
		    line.replace (i, line.size () - i, line.size () - i, ' ');
		    break;
		}
		if (line[i + 1] == '*') {
		    line[i++] = ' ';
		    line[i] = ' ';
		    blockComment = true;
		}
	    }
	}
	if (preprocessorDirective (line) == "include") {
	    const size_t hash = line.find ('#');
	    const size_t keyword = line.find_first_not_of (" \t\r", hash + 1);
	    const size_t open = line.find_first_not_of (" \t\r", keyword + 7);
	    const size_t close = open == std::string::npos ? std::string::npos : line.find ('"', open + 1);
	    if (open == std::string::npos || line[open] != '"' || close == std::string::npos || close == open + 1
		|| line.find_first_not_of (" \t\r", close + 1) != std::string::npos) {
		throw std::runtime_error ("Malformed #include in " + file + " at line " + std::to_string (lineNumber));
	    }
	    return IncludeDirective { start + hash, start + close + 1, line.substr (open + 1, close - open - 1) };
	}
	start = end == source.size () ? end : end + 1;
    }
    return std::nullopt;
}

bool continuesOnNextLine (std::string_view line) {
    while (!line.empty () && std::isspace (static_cast<unsigned char> (line.back ()))) {
	line.remove_suffix (1);
    }

    return !line.empty () && line.back () == '\\';
}

size_t preprocessorConditionalDepthBefore (std::string_view source, const size_t limit) {
    size_t conditionalDepth = 0;
    size_t cursor = 0;

    while (cursor < limit) {
	size_t lineEnd = source.find ('\n', cursor);
	if (lineEnd == std::string_view::npos || lineEnd > limit) {
	    lineEnd = limit;
	}

	const std::string_view directive = preprocessorDirective (source.substr (cursor, lineEnd - cursor));
	if (directive == "if" || directive == "ifdef" || directive == "ifndef") {
	    conditionalDepth++;
	} else if (directive == "endif" && conditionalDepth > 0) {
	    conditionalDepth--;
	}

	cursor = lineEnd < limit ? lineEnd + 1 : limit;
    }

    return conditionalDepth;
}

constexpr std::string_view CONDITIONAL_INCLUDE_BEGIN = "// LWE conditional include begin";
constexpr std::string_view CONDITIONAL_INCLUDE_END = "// LWE conditional include end";

/**
 * Wallpaper Engine's shader compiler lets header macros affect the authored source at the
 * #include site while resolving global declarations used by header functions later. Desktop
 * GLSL cannot do both by pasting the whole header at either location: early header functions
 * cannot see later uniforms, and late header macros cannot affect earlier authored helpers.
 *
 * Copy only unconditional #defines out of the fully expanded header text. They are safe to
 * expose at the include site; the complete header also stays at the existing insertion point
 * after the shader's interface declarations. Conditional defines remain in place so their
 * #if/#endif scope is unchanged.
 */
std::string collectUnconditionalIncludeDefines (const std::string& includes) {
    std::string defines;
    size_t conditionalDepth = 0;
    size_t conditionalIncludeDepth = 0;
    size_t cursor = 0;

    while (cursor < includes.size ()) {
	size_t lineEnd = includes.find ('\n', cursor);
	const bool hasNewline = lineEnd != std::string::npos;
	if (!hasNewline) {
	    lineEnd = includes.size ();
	}

	const std::string_view line (includes.data () + cursor, lineEnd - cursor);
	const std::string_view directive = preprocessorDirective (line);

	if (line == CONDITIONAL_INCLUDE_BEGIN || line == CONDITIONAL_INCLUDE_END) {
	    if (line == CONDITIONAL_INCLUDE_BEGIN) {
		conditionalIncludeDepth++;
	    } else if (conditionalIncludeDepth > 0) {
		conditionalIncludeDepth--;
	    }
	    cursor = hasNewline ? lineEnd + 1 : lineEnd;
	    continue;
	}

	if (directive == "define" && conditionalDepth == 0 && conditionalIncludeDepth == 0) {
	    size_t logicalEnd = hasNewline ? lineEnd + 1 : lineEnd;
	    std::string_view logicalLine = line;

	    while (continuesOnNextLine (logicalLine) && logicalEnd < includes.size ()) {
		const size_t nextEnd = includes.find ('\n', logicalEnd);
		if (nextEnd == std::string::npos) {
		    logicalLine = std::string_view (includes.data () + logicalEnd, includes.size () - logicalEnd);
		    logicalEnd = includes.size ();
		} else {
		    logicalLine = std::string_view (includes.data () + logicalEnd, nextEnd - logicalEnd);
		    logicalEnd = nextEnd + 1;
		}
	    }

	    defines.append (includes, cursor, logicalEnd - cursor);
	    if (!defines.empty () && defines.back () != '\n') {
		defines.push_back ('\n');
	    }

	    cursor = logicalEnd;
	    continue;
	}

	if (directive == "if" || directive == "ifdef" || directive == "ifndef") {
	    conditionalDepth++;
	} else if (directive == "endif" && conditionalDepth > 0) {
	    conditionalDepth--;
	}

	cursor = hasNewline ? lineEnd + 1 : lineEnd;
    }

    return defines;
}
} // namespace

void ShaderUnit::preprocessVaryingDeclarations () {
    // Misty Sea declares "varying vec4 v_Size.xy". Native retains the vec4;
    // normalize before linked-varying discovery would insert a second declaration.
    static const std::regex declaration (
        R"(\bvarying\s+[biu]?vec[234]\s+[A-Za-z_][A-Za-z0-9_]*(\s*\.\s*(?:[xyzw]{1,4}|[rgba]{1,4}))\s*;)"
    );
    const std::string code = maskShaderComments (m_preprocessed);
    // std::regex over every shader cost ~0.7 s on 3D scenes; only run it when some
    // varying declaration actually contains a '.' before its ';'
    bool candidate = false;
    for (size_t at = code.find ("varying"); at != std::string::npos && !candidate; at = code.find ("varying", at + 1)) {
	const size_t end = code.find (';', at);
	candidate = code.find ('.', at) < end;
    }
    if (!candidate) {
	return;
    }
    size_t removed = 0;
    for (std::sregex_iterator it (code.begin (), code.end (), declaration), last; it != last; ++it) {
        const size_t length = (*it).length (1);
        m_preprocessed.erase ((*it).position (1) - removed, length);
        removed += length;
    }
}

void ShaderUnit::preprocessIncludes () {
    // check the memo cache first, keyed on the locator's mount fingerprint (stable across
    // engine restarts of the same project), the unit's file and its source text
    const std::string cacheKey = buildIncludeCacheKey (this->m_assetLocator.identity (), this->m_file, this->m_preprocessed);

    {
	std::lock_guard lock (sIncludeCacheMutex);
	if (const auto found = sIncludeCache.find (cacheKey); found != sIncludeCache.end ()) {
	    this->m_preprocessed = found->second.preprocessed;
	    this->m_includes = found->second.includes;
	    return;
	}
    }

    size_t start = 0, end = 0;
    size_t firstUnconditionalInclude = std::string::npos;
    // prepare the include content
    while (const auto include = nextInclude (this->m_preprocessed, this->m_file)) {
	start = include->start;
	const bool conditionalInclude = preprocessorConditionalDepthBefore (this->m_preprocessed, start) > 0;
	if (!conditionalInclude && firstUnconditionalInclude == std::string::npos) {
	    firstUnconditionalInclude = start;
	}

	const std::string& filename = include->filename;

	// Preserve the fallback for unavailable headers, including optional stock content.
	std::string content;

	try {
	    content += "// begin of include from file ";
	    content += filename;
	    content += "\n";
	    content += spliceShaderLines (this->m_assetLocator.includeShader (filename));
	    content += "\n// end of included from file ";
	    content += filename;
	    content += "\n";
	} catch (AssetLoadException&) {
	    content += "// tried including file ";
	    content += filename;
	    content += " but was not found\n";
	}

	if (conditionalInclude) {
	    content.insert (0, std::string (CONDITIONAL_INCLUDE_BEGIN) + '\n');
	    content += CONDITIONAL_INCLUDE_END;
	    content += '\n';
	}

	// Keep offsets and trailing comments intact; a block comment can continue
	// onto the next line and must not turn into live source after expansion.
	this->m_preprocessed.replace (start, include->end - start, include->end - start, ' ');

	this->m_includes += content;

    }

    // Expand nested includes in-place.
    while (const auto include = nextInclude (this->m_includes, this->m_file + " included headers")) {
	start = include->start;
	const std::string& filename = include->filename;

	// Preserve the fallback for unavailable headers, including optional stock content.
	std::string content;

	try {
	    content = "// begin of include from file ";
	    content += filename;
	    content += "\n";
	    content += spliceShaderLines (this->m_assetLocator.includeShader (filename));
	    content += "\n// end of included from file ";
	    content += filename;
	    content += "\n";
	} catch (AssetLoadException&) {
	    content = "// tried including file ";
	    content += filename;
	    content += " but was not found\n";
	}

	// file contents ready, replace things
	this->m_includes.replace (start, include->end - start, content);

    }

    // Also expose macros at the authored include location. Keep the original definitions
    // in m_includes because that block is relocated before the authored include line and
    // its own later helper functions must already be able to use those macros.
    const std::string includeDefines = collectUnconditionalIncludeDefines (this->m_includes);
    if (!includeDefines.empty () && firstUnconditionalInclude != std::string::npos) {
	this->m_preprocessed.insert (firstUnconditionalInclude, includeDefines);
    }

    // search for the main function and add the includes before that for now
    end = 0;
    bool includesAdded = false;

    // finally, try to place the include contents before the main function
    while ((start = this->m_preprocessed.find (" main", end)) != std::string::npos) {
	char value = this->m_preprocessed.at (start + 5);

	end = start + 5;

	if (value != ' ' && value != '(') {
	    continue;
	}

	// main located, search for uniforms and find the latest one available
	size_t lastAttribute = this->m_preprocessed.rfind ("attribute", start);
	size_t lastVarying = this->m_preprocessed.rfind ("varying", start);
	size_t lastUniform = this->m_preprocessed.rfind ("uniform", start);
	size_t latest = lastAttribute;

	if (latest == std::string::npos) {
	    latest = lastVarying;
	} else if (latest < lastVarying && lastVarying != std::string::npos) {
	    latest = lastVarying;
	}

	if (latest == std::string::npos) {
	    latest = lastUniform;
	} else if (latest < lastUniform && lastUniform != std::string::npos) {
	    latest = lastUniform;
	}

	if (latest < start) {
	    // find the end of the current line
	    latest = this->m_preprocessed.find ('\n', latest);
	} else {
	    // find the end of the previous line
	    latest = this->m_preprocessed.rfind ('\n', start);
	}

	// update the function start to point to the end of the previous line
	// as this will be used to determine the position of the includes
	start = this->m_preprocessed.rfind ('\n', start);

	// keeps track of the start and end of ifdefs to look for the right
	// place to put the includes in
	std::stack<size_t> ifdefStack;

	// start looking for #if and #endif results and add to the stack so we find the start of the current chain of
	// ifdefs and use that as point

	// for this we'll use regex
	static const std::regex ifdef (R"((#if|#endif))");
	std::smatch match;
	size_t current = 0;

	while (
	    std::regex_search (this->m_preprocessed.cbegin () + current, this->m_preprocessed.cend (), match, ifdef)) {
	    current += match.position ();

	    // if it's opening an #ifdef keep track of the start of the block
	    // and that's it
	    if (this->m_preprocessed.substr (current, 3) == "#if") {
		// go to the next character so the regex doesn't match with the same thing again
		ifdefStack.push (current++);
		continue;
	    }

	    // go to the next character so the regex doesn't match with the same thing again
	    current++;

	    // most likely a syntax error, but we'll ignore it for now...
	    if (ifdefStack.empty ()) {
		continue;
	    }

	    size_t stackStart = ifdefStack.top ();
	    ifdefStack.pop ();

	    if (latest > stackStart && latest <= current) {
		// The insertion point is inside a conditional block.
		// Move to BEFORE the #if so includes are available to all branches
		// (e.g. genericropeparticle.vert has #if GS_ENABLED wrapping two main() functions).
		size_t beforeIfdef = this->m_preprocessed.rfind ('\n', stackStart);
		latest = (beforeIfdef != std::string::npos) ? beforeIfdef : 0;
	    }
	}

	// no more matches, get the one that happens the earliest
	// TODO: IS THIS GOOD ENOUGH? MAYBE WE SHOULD BE GETTING THE FIRST #IF BLOCK INSTEAD?
	latest = std::min (latest, start);

	// finally insert it there
	this->m_preprocessed.insert (latest + 1, this->m_includes + '\n');
	includesAdded = true;
	break;
    }

    if (!includesAdded) {
	sLog.exception ("Could not find where to place includes for shader unit ", this->m_file);
    }

    // Memoize the result for the next build of this unit. Shader sizes vary by
    // orders of magnitude, so an entry-count cap still allowed a few complex
    // scenes to retain hundreds of MB. Bound the actual source bytes instead.
    {
	std::lock_guard lock (sIncludeCacheMutex);
	const size_t entryBytes = cacheKey.size () + this->m_preprocessed.size () + this->m_includes.size ();
	if (entryBytes > SHADER_TEXT_CACHE_BUDGET_BYTES)
	    return;
	if (sIncludeCacheBytes + entryBytes > SHADER_TEXT_CACHE_BUDGET_BYTES) {
	    sIncludeCache.clear ();
	    sIncludeCacheBytes = 0;
	}
	if (sIncludeCache.emplace (
		cacheKey, PreprocessedIncludes { this->m_preprocessed, this->m_includes }
	    ).second) {
	    sIncludeCacheBytes += entryBytes;
	}
    }
}

void ShaderUnit::preprocessRequires () {
    size_t start = 0, end = 0;

    while ((start = this->m_preprocessed.find ("#require", end)) != std::string::npos) {
	const size_t lineEnd = this->m_preprocessed.find_first_of ('\n', start);

	const size_t nameStart = start + std::string ("#require ").length ();

	if (nameStart >= lineEnd) {
	    sLog.error ("Malformed #require directive (no module name) in shader ", this->m_file);
	    end = lineEnd;
	    continue;
	}

	std::string moduleName = this->m_preprocessed.substr (nameStart, lineEnd - nameStart);

	while (!moduleName.empty () && (moduleName.back () == ' ' || moduleName.back () == '\r')) {
	    moduleName.pop_back ();
	}

	if (moduleName.empty ()) {
	    sLog.error ("Malformed #require directive (empty module name) in shader ", this->m_file);
	    end = lineEnd;
	    continue;
	}

	sLog.out ("Resolving require module: ", moduleName, " in shader ", this->m_file);

	std::string moduleCode = this->resolveRequireModule (moduleName);

	// comment out the #require directive
	this->m_preprocessed = this->m_preprocessed.replace (start, 2, "//");

	if (!moduleCode.empty ()) {
	    // insert the generated code directly into m_preprocessed at the #require location
	    // (m_includes was already consumed by preprocessIncludes, so appending there would be lost)
	    this->m_preprocessed.insert (start, moduleCode);
	    end = start + moduleCode.length ();
	} else {
	    end = lineEnd;
	}
    }
}

std::string ShaderUnit::resolveRequireModule (const std::string& moduleName) const {
    if (moduleName == "LightingV1") {
	return this->generateLightingV1 ();
    }

    sLog.error ("Unknown #require module: ", moduleName, " in shader ", this->m_file);
    return "";
}

std::string ShaderUnit::generateLightingV1 () const {
    // PerformLighting_V1 is dynamically generated by Wallpaper Engine based on the scene's
    // light sources: an unrolled block per light, calling the ComputePBRLightShadow* helpers
    // from common_pbr_2.h. CPass passes the scene's light counts in through the combo maps.
    // Shadow-capable directional and spot lights share Wallpaper Engine's feature
    // atlas. Directional lights consume three nested cascade features each.
    const auto comboValue = [this] (const std::string& name) -> int {
	if (const auto it = this->m_overrideCombos.find (name); it != this->m_overrideCombos.end ()) {
	    return it->second;
	}

	if (const auto it = this->m_combos.find (name); it != this->m_combos.end ()) {
	    return it->second;
	}

	return 0;
    };

    const int directionalLights = comboValue ("LIGHTS_DIRECTIONAL");
    const int directionalShadowLights = comboValue ("LIGHTS_DIRECTIONAL_SHADOW");
    const int pointLights = comboValue ("LIGHTS_POINT");
    const int pointShadowLights = comboValue ("LIGHTS_POINT_SHADOW");
    const int spotLights = comboValue ("LIGHTS_SPOT");
    const int spotShadowLights = comboValue ("LIGHTS_SPOT_SHADOW");
    const int tubeLights = comboValue ("LIGHTS_TUBE");
    const int shadowFeatures = comboValue ("LIGHTS_SHADOW_FEATURES");
    const uint32_t directionalShadowMask
	= static_cast<uint32_t> (comboValue ("LIGHTS_DIRECTIONAL_SHADOW_MASK"));
    const uint32_t pointShadowMask = static_cast<uint32_t> (comboValue ("LIGHTS_POINT_SHADOW_MASK"));
    const uint32_t spotShadowMask = static_cast<uint32_t> (comboValue ("LIGHTS_SPOT_SHADOW_MASK"));
    const uint32_t spotCookieMask = static_cast<uint32_t> (comboValue ("LIGHTS_SPOT_COOKIE_MASK"));

    std::vector<std::array<int, 3>> directionalFeatureIndices (
	directionalLights, std::array<int, 3> { -1, -1, -1 }
    );
    std::vector<int> spotFeatureIndices (spotLights, -1);
    int nextFeature = 0;
    for (int i = 0; i < directionalLights && i < 32; i++) {
	if ((directionalShadowMask & (uint32_t (1) << i)) != 0 && nextFeature + 2 < shadowFeatures) {
	    directionalFeatureIndices[i] = { nextFeature, nextFeature + 1, nextFeature + 2 };
	    nextFeature += 3;
	}
    }
    for (int i = 0; i < spotLights && i < 32; i++) {
	if ((spotShadowMask & (uint32_t (1) << i)) != 0 && nextFeature < shadowFeatures) {
	    spotFeatureIndices[i] = nextFeature++;
	}
    }

    std::string code = "// begin of generated module LightingV1\n";

    if (directionalLights > 0) {
	code += "uniform vec4 g_LDirectional_Direction[" + std::to_string (directionalLights) + "];\n";
	code += "uniform vec4 g_LDirectional_Color[" + std::to_string (directionalLights) + "];\n";
	if (directionalShadowLights > 0) {
	    code += "uniform float g_LDirectional_ShadowEnabled[" + std::to_string (directionalLights) + "];\n";
	}
    }

    if (pointLights > 0) {
	code += "uniform vec4 g_LPoint_Origin[" + std::to_string (pointLights) + "];\n";
	code += "uniform vec4 g_LPoint_Color[" + std::to_string (pointLights) + "];\n";
	if (pointShadowLights > 0) {
	    code += "uniform vec4 g_LFeature_ShadowPointProjection[" + std::to_string (pointLights) + "];\n";
	    code += "uniform vec4 g_LFeature_ShadowPointProjectionTransform[" + std::to_string (pointLights)
		+ "];\n";
	    code += "uniform float g_LPoint_ShadowEnabled[" + std::to_string (pointLights) + "];\n";
	}
    }

    if (spotLights > 0) {
	code += "uniform vec4 g_LSpot_Origin[" + std::to_string (spotLights) + "];\n";
	code += "uniform vec4 g_LSpot_Direction[" + std::to_string (spotLights) + "];\n";
	code += "uniform vec4 g_LSpot_Color[" + std::to_string (spotLights) + "];\n";
	code += "uniform vec4 g_LSpot_Exponent[" + std::to_string (spotLights) + "];\n";
	if (spotCookieMask != 0) {
	    code += "uniform mat4 g_LSpot_CookieProjection[" + std::to_string (spotLights) + "];\n";
	}
	if (spotShadowLights > 0) {
	    code += "uniform float g_LSpot_ShadowEnabled[" + std::to_string (spotLights) + "];\n";
	}
    }

    if (shadowFeatures > 0) {
	code += "uniform mat4 g_LFeature_ShadowProjection[" + std::to_string (shadowFeatures) + "];\n";
	code += "uniform vec4 g_LFeature_ShadowProjectionTransform[" + std::to_string (shadowFeatures)
	    + "];\n";
    }

    if (tubeLights > 0) {
	code += "uniform vec4 g_LTube_OriginA[" + std::to_string (tubeLights) + "];\n";
	code += "uniform vec4 g_LTube_OriginB[" + std::to_string (tubeLights) + "];\n";
	code += "uniform vec4 g_LTube_Color[" + std::to_string (tubeLights) + "];\n";
    }

    code += "vec3 PerformLighting_V1(vec3 worldPos, vec3 albedo, vec3 normal, vec3 viewDir,\n"
	    "    vec3 specularTint, vec3 baseReflectance, float roughness, float metallic)\n"
	    "{\n"
	    "    vec3 light = vec3(0.0);\n";

    for (int i = 0; i < pointLights; i++) {
	const std::string index = std::to_string (i);

	code += "    {\n"
		"        vec3 lightDelta = g_LPoint_Origin[" + index + "].xyz - worldPos;\n";
	if (i < 32 && (pointShadowMask & (uint32_t (1) << i)) != 0) {
	    code += "        float shadowFactor = 1.0;\n"
		    "        if (g_LPoint_ShadowEnabled[" + index + "] > 0.5) {\n"
		    "            vec4 pointAtlasTransform = g_LFeature_ShadowPointProjectionTransform[" + index
		    + "];\n"
		      "            vec4 projectedCoords = CalculateProjectedCoordsPoint(worldPos,\n"
		      "                g_LPoint_Origin[" + index + "].xyz,\n"
		      "                g_LFeature_ShadowPointProjection[" + index + "], pointAtlasTransform);\n"
		      "#if !REVERSEDEPTH\n"
		      "            projectedCoords.z = projectedCoords.z * 0.5 + 0.5 - 0.0015;\n"
		      "#endif\n"
		      "            shadowFactor = PerformPointShadowMapping(projectedCoords);\n"
		      "        }\n";
	} else {
	    code += "        float shadowFactor = 1.0;\n";
	}
	code +=
		"        light += ComputePBRLightShadow(normal, lightDelta, viewDir, albedo,\n"
		"            g_LPoint_Color[" + index + "].rgb, g_LPoint_Color[" + index + "].w,\n"
		"            g_LPoint_Origin[" + index + "].w, specularTint, baseReflectance,\n"
		"            roughness, metallic, shadowFactor);\n"
		"    }\n";
    }

    for (int i = 0; i < spotLights; i++) {
	const std::string index = std::to_string (i);
	const int shadowFeature = spotFeatureIndices[i];

	code += "    {\n"
		"        vec3 lightDelta = g_LSpot_Origin[" + index + "].xyz - worldPos;\n";
	if (i < 32 && (spotCookieMask & (uint32_t (1) << i)) != 0) {
	    code += "        vec3 cookieCoords = CalculateProjectedCoords(worldPos, g_LSpot_CookieProjection[" + index + "]);\n"
		    "        vec3 spotCookie = texSample2D(COOKIE_SAMPLER, cookieCoords.xy).rgb;\n";
	} else {
	    code += "        float spotCookie = -dot(normalize(lightDelta), g_LSpot_Direction[" + index + "].xyz);\n"
		    "        spotCookie = smoothstep(g_LSpot_Direction[" + index + "].w,\n"
		    "            g_LSpot_Origin[" + index + "].w, spotCookie);\n";
	}
	if (shadowFeature >= 0) {
	    const std::string featureIndex = std::to_string (shadowFeature);
	    // Caster rasterization already mirrors Y into native atlas coordinates.
	    // Keep finite out-of-tile spot samples, which can reach adjacent features.
	    code += "        float shadowFactor = 1.0;\n"
		    "        if (g_LSpot_ShadowEnabled[" + index + "] > 0.5) {\n"
		    "            vec3 projectedCoords = CalculateProjectedCoords(worldPos,\n"
		    "                g_LFeature_ShadowProjection[" + featureIndex + "]);\n"
		    "#if !REVERSEDEPTH\n"
		    "            projectedCoords.z = projectedCoords.z * 0.5 + 0.5 - 0.0015;\n"
		    "#endif\n"
		    "            shadowFactor = PerformShadowMapping(projectedCoords,\n"
		    "                g_LFeature_ShadowProjectionTransform[" + featureIndex + "]);\n"
		    "        }\n";
	} else {
	    code += "        float shadowFactor = 1.0;\n";
	}
	code +=
		"        light += ComputePBRLightShadow(normal, lightDelta, viewDir, albedo,\n"
		"            g_LSpot_Color[" + index + "].rgb * spotCookie, g_LSpot_Color[" + index + "].w,\n"
		"            g_LSpot_Exponent[" + index + "].x, specularTint, baseReflectance,\n"
		"            roughness, metallic, shadowFactor);\n"
		"    }\n";
    }

    for (int i = 0; i < tubeLights; i++) {
	const std::string index = std::to_string (i);

	code += "    {\n"
		"        vec3 lightDelta = PointSegmentDelta(worldPos, g_LTube_OriginA[" + index
		+ "].xyz, g_LTube_OriginB[" + index + "].xyz);\n"
		"        light += ComputePBRLightShadow(normal, lightDelta, viewDir, albedo,\n"
		"            g_LTube_Color[" + index + "].rgb, g_LTube_Color[" + index + "].w,\n"
		"            g_LTube_OriginA[" + index + "].w, specularTint, baseReflectance,\n"
		"            roughness, metallic, 1.0);\n"
		"    }\n";
    }

    for (int i = 0; i < directionalLights; i++) {
	const std::string index = std::to_string (i);
	const std::array<int, 3>& features = directionalFeatureIndices[i];

	code += "    {\n";
	if (features[0] >= 0) {
	    const std::string feature1 = std::to_string (features[0]);
	    const std::string feature2 = std::to_string (features[1]);
	    const std::string feature3 = std::to_string (features[2]);
	    code += "        float shadowFactor = 1.0;\n"
		    "        if (g_LDirectional_ShadowEnabled[" + index + "] > 0.5) {\n"
		    "            vec4 projectedCoords1 = CalculateProjectedCoordsCascades(worldPos,\n"
		    "                g_LFeature_ShadowProjection[" + feature1 + "]);\n"
		    "            vec4 projectedCoords2 = CalculateProjectedCoordsCascades(worldPos,\n"
		    "                g_LFeature_ShadowProjection[" + feature2 + "]);\n"
		    "            vec4 projectedCoords3 = CalculateProjectedCoordsCascades(worldPos,\n"
		    "                g_LFeature_ShadowProjection[" + feature3 + "]);\n"
		    "            vec4 uvTransforms = mix(g_LFeature_ShadowProjectionTransform[" + feature1 + "],\n"
		    "                g_LFeature_ShadowProjectionTransform[" + feature2 + "], projectedCoords1.w);\n"
		    "            uvTransforms = mix(uvTransforms, g_LFeature_ShadowProjectionTransform[" + feature3
		    + "], projectedCoords2.w);\n"
		      "            projectedCoords1.xyz = mix(projectedCoords1.xyz, projectedCoords2.xyz,\n"
		      "                projectedCoords1.w);\n"
		      "            projectedCoords1.xyz = mix(projectedCoords1.xyz, projectedCoords3.xyz,\n"
		      "                projectedCoords2.w);\n"
		      "#if !REVERSEDEPTH\n"
		      "            projectedCoords1.z = projectedCoords1.z * 0.5 + 0.5 - 0.0015;\n"
		      "#endif\n"
		      "            shadowFactor = max(projectedCoords3.w,\n"
		      "                PerformShadowMapping(projectedCoords1.xyz, uvTransforms));\n"
		      "        }\n";
	} else {
	    code += "        float shadowFactor = 1.0;\n";
	}
	code += "        light += ComputePBRLightShadowInfinite(normal, g_LDirectional_Direction[" + index
	    + "].xyz,\n"
	      "            viewDir, albedo, g_LDirectional_Color[" + index + "].rgb, specularTint,\n"
	      "            baseReflectance, roughness, metallic, shadowFactor);\n"
	      "    }\n";
    }

    code += "    return light;\n"
	    "}\n"
	    "// end of generated module LightingV1\n";

    return code;
}

std::string ShaderUnit::applyLinkedVaryingCompatibility (std::string source) const {
    if (this->m_type != GLSLContext::UnitType_Vertex || this->m_link == nullptr) {
	return source;
    }

    static const std::regex fragmentVec4Varying (R"(\bvarying\s+vec4\s+([A-Za-z_][A-Za-z0-9_]*)\s*;)");
    std::smatch varyingMatch;
    std::string linked = this->m_link->m_preprocessed;
    size_t linkedOffset = 0;

    while (std::regex_search (linked.cbegin () + linkedOffset, linked.cend (), varyingMatch, fragmentVec4Varying)) {
	const std::string name = varyingMatch[1].str ();
	linkedOffset += varyingMatch.position () + varyingMatch.length ();

	const std::regex vertexVec2Decl ("\\bvarying\\s+vec2\\s+" + name + "\\s*;");
	const std::regex vertexVec4Decl ("\\bvarying\\s+vec4\\s+" + name + "\\s*;");
	const bool vertexHasVec2 = std::regex_search (source, vertexVec2Decl);
	const bool vertexHasVec4 = std::regex_search (source, vertexVec4Decl);

	// Skip entirely if this varying isn't declared in the vertex shader at all.
	if (!vertexHasVec2 && !vertexHasVec4) {
	    continue;
	}

	// If the vertex shader already declares this varying as vec4 (even inside a combo
	// branch), the shader handles its own type variants — leave it untouched. The
	// second pass below handles any vec4->vec2 read contexts independently.
	if (vertexHasVec4) {
	    continue;
	}

	// Upgrade vec2 declaration to vec4 and wrap bare assignments.
	source = std::regex_replace (source, vertexVec2Decl, "varying vec4 " + name + ";");

	const std::regex assignment ("(^|\\n)([ \\t]*)" + name + "\\s*=\\s*([^;\\n]+);");
	std::smatch assignmentMatch;
	size_t offset = 0;
	while (std::regex_search (source.cbegin () + offset, source.cend (), assignmentMatch, assignment)) {
	    const std::string prefix = assignmentMatch[1].str ();
	    const std::string indent = assignmentMatch[2].str ();
	    const std::string expression = assignmentMatch[3].str ();
	    const std::string replacement = prefix + indent + name + " = vec4(" + expression + ", 0.0, 1.0);";
	    const size_t position = offset + assignmentMatch.position ();
	    source.replace (position, assignmentMatch.length (), replacement);
	    offset = position + replacement.length ();
	}

	// After upgrading the declaration and wrapping assignments, any remaining
	// unswizzled read of `name` in an arithmetic expression is now a vec4
	// where the surrounding code expects a vec2. Add .xy to each such read.
	//
	// A "read context" occurrence is: \bname\b NOT followed by [.([=]
	// (which would indicate a swizzle, index, call, or assignment LHS)
	// and NOT immediately preceded by a GLSL type keyword (declaration).
	static const auto isWordChar = [] (char c) {
	    return std::isalnum (static_cast<unsigned char> (c)) || c == '_';
	};
	static const auto isGlslType = [] (const std::string& w) {
	    static const std::set<std::string> types = {
		"vec2", "vec3", "vec4", "float", "int", "uint", "bool",
		"mat2", "mat3", "mat4", "mat4x3", "mat3x3",
		"sampler2D", "sampler3D", "uvec4", "ivec2", "ivec3", "ivec4",
		"varying", "uniform", "attribute", "in", "out",
	    };
	    return types.count (w) > 0;
	};

	size_t pos = 0;
	while ((pos = source.find (name, pos)) != std::string::npos) {
	    const size_t afterName = pos + name.size ();

	    // Must be a word boundary on both sides
	    if ((pos > 0 && isWordChar (source[pos - 1])) ||
		(afterName < source.size () && isWordChar (source[afterName]))) {
		pos = afterName;
		continue;
	    }

	    // Skip if followed by swizzle '.', index '[', call '(', or assignment '='
	    size_t j = afterName;
	    while (j < source.size () && source[j] == ' ') j++;
	    const char nextChar = j < source.size () ? source[j] : '\0';
	    if (nextChar == '.' || nextChar == '[' || nextChar == '(' || nextChar == '=') {
		pos = afterName;
		continue;
	    }

	    // Skip if immediately preceded by a GLSL type keyword (it's a declaration)
	    size_t k = pos;
	    while (k > 0 && source[k - 1] == ' ') k--;
	    if (k > 0 && isWordChar (source[k - 1])) {
		size_t wordEnd = k;
		while (k > 0 && isWordChar (source[k - 1])) k--;
		if (isGlslType (source.substr (k, wordEnd - k))) {
		    pos = afterName;
		    continue;
		}
	    }

	    source.insert (afterName, ".xy");
	    pos = afterName + 3;
	}
    }

    // SECOND PASS: fix unswizzled reads of vec4 identifiers in vec2 assignment contexts.
    // Covers: (1) varyings already declared as vec4 in the vertex shader,
    //         (2) local vec4 variables (e.g. from mat4 multiply results).
    // WE's own GLSL compiler permits implicit vec4->vec2 truncation; standard glslang
    // does not — so we must insert explicit .xy swizzles.
    {
	static const std::regex anyVec4Re (R"(\bvec4\s+([A-Za-z_][A-Za-z0-9_]*)\b)");
	std::sregex_iterator it (source.cbegin (), source.cend (), anyVec4Re);
	std::sregex_iterator end;
	std::set<std::string> processed;
	for (; it != end; ++it) {
	    const std::string vname = (*it)[1].str ();
	    if (!processed.insert (vname).second) continue;
	    // Find lines of the form "vec2 var = ... vname ..." where vname has no swizzle.
	    // Lazy [^\n]*? so we find the first unswizzled occurrence after the '='.
	    const std::regex vec2Line ("(vec2\\s+\\w+\\s*=[^\\n]*?)\\b(" + vname + ")\\b(?![.\\[\\(])");
	    const std::string patched = std::regex_replace (source, vec2Line, "$1$2.xy");
	    if (patched != source) {
		source = patched;
	    }
	}
    }

    return source;
}

std::string ShaderUnit::applyMissingFragmentVaryingCompatibility (std::string source) const {
    if (this->m_type != GLSLContext::UnitType_Fragment || this->m_link == nullptr) {
	return source;
    }

    static const std::regex varyingDeclaration (
	R"(\bvarying\s+((?:[biu]?vec[234]|float|int|uint|bool))\s+([A-Za-z_][A-Za-z0-9_]*)\s*;)"
    );
    std::string declarations;

    for (std::sregex_iterator it (this->m_link->m_preprocessed.cbegin (), this->m_link->m_preprocessed.cend (),
				  varyingDeclaration), last;
	 it != last; ++it) {
	const std::string type = (*it) [1].str ();
	const std::string name = (*it) [2].str ();
	const std::regex use ("\\b" + name + "\\b");
	const std::regex ownDeclaration ("\\bvarying\\s+[A-Za-z_][A-Za-z0-9_]*\\s+" + name + "\\s*;");

	if (std::regex_search (source, use) && !std::regex_search (source, ownDeclaration)) {
	    declarations += "varying " + type + " " + name + ";\n";
	}
    }

    if (!declarations.empty ()) {
	source.insert (0, declarations);
	sLog.out ("Applied missing fragment varying compatibility in ", this->m_file);
    }

    return source;
}

std::string ShaderUnit::applyNumericParameterCallCompatibility (std::string source) const {
    // Wallpaper Engine accepts HLSL-style implicit numeric conversions at user-function call
    // sites. GLSL does not. Explicit constructors are no-ops for matching types and reproduce
    // scalar splatting or vector truncation for mismatched input arguments.
    static const std::regex functionDefinition (
	R"(\b[A-Za-z_][A-Za-z0-9_]*\s+([A-Za-z_][A-Za-z0-9_]*)\s*\(([^)]*)\)\s*\{)");
    static const std::regex numericParameter (
	R"(^\s*(?:(?:in|const)\s+)*((?:[biu]?vec[234]|float[234]|int[234]|float|int|uint|bool))\s+[A-Za-z_][A-Za-z0-9_]*\s*$)");
    static const std::regex parameterDeclaration (
	R"(^\s*(?:(?:in|out|inout|const)\s+)*[A-Za-z_][A-Za-z0-9_]*\s+[A-Za-z_][A-Za-z0-9_]*\s*$)");
    static const auto isWordChar = [] (char c) {
	return std::isalnum (static_cast<unsigned char> (c)) || c == '_';
    };
    static const auto isSpace = [] (char c) { return std::isspace (static_cast<unsigned char> (c)) != 0; };

    using ParameterTypes = std::map<size_t, std::set<std::string>>;
    std::map<std::string, std::map<size_t, ParameterTypes>> parameterTypes;

    for (std::sregex_iterator it (source.cbegin (), source.cend (), functionDefinition), last; it != last; ++it) {
	const std::string name = (*it) [1].str ();
	const std::string parameters = (*it) [2].str ();

	if (name == "main") {
	    continue;
	}

	size_t index = 0;
	size_t cursor = 0;
	ParameterTypes signature;

	while (cursor <= parameters.size ()) {
	    size_t comma = parameters.find (',', cursor);

	    if (comma == std::string::npos) {
		comma = parameters.size ();
	    }

	    const std::string parameter = parameters.substr (cursor, comma - cursor);
	    std::smatch parameterMatch;
	    if (std::regex_match (parameter, parameterMatch, numericParameter)) {
		signature [index].insert (parameterMatch [1].str ());
	    } else {
		// Another conditional signature can use this slot for an out/inout
		// parameter. Retain a sentinel so a numeric input in a different
		// branch cannot turn its writable argument into a constructor value.
		signature [index].insert ("");
	    }

	    index++;
	    cursor = comma + 1;
	}

	auto& stored = parameterTypes [name] [parameters.empty () ? 0 : index];
	for (const auto& [parameterIndex, types] : signature) {
	    stored [parameterIndex].insert (types.begin (), types.end ());
	}
    }

    // second pass: wrap the matching call-site arguments
    for (const auto& [name, signatures] : parameterTypes) {
	size_t pos = 0;

	while ((pos = source.find (name, pos)) != std::string::npos) {
	    const size_t afterName = pos + name.size ();

	    // must be a word boundary on both sides
	    if ((pos > 0 && isWordChar (source [pos - 1])) ||
		(afterName < source.size () && isWordChar (source [afterName]))) {
		pos = afterName;
		continue;
	    }

	    // must be followed by an argument list
	    size_t open = afterName;

	    while (open < source.size () && isSpace (source [open])) {
		open++;
	    }

	    if (open >= source.size () || source [open] != '(') {
		pos = afterName;
		continue;
	    }

	    // split the top-level arguments
	    std::vector<std::pair<size_t, size_t>> arguments;
	    size_t argumentStart = open + 1;
	    size_t close = open;
	    int depth = 0;

	    for (; close < source.size (); close++) {
		const char c = source [close];

		if (c == '(') {
		    depth++;
		} else if (c == ')') {
		    if (--depth == 0) {
			arguments.emplace_back (argumentStart, close);
			break;
		    }
		} else if (c == ',' && depth == 1) {
		    arguments.emplace_back (argumentStart, close);
		    argumentStart = close + 1;
		}
	    }

	    if (close >= source.size ()) {
		break; // unbalanced parenthesis, nothing sensible to do
	    }

	    // skip definitions ('{' after the parameter list) and prototypes (the
	    // "arguments" are parameter declarations rather than expressions)
	    size_t after = close + 1;

	    while (after < source.size () && isSpace (source [after])) {
		after++;
	    }

	    const std::string firstArgument =
		arguments.empty () ? "" : source.substr (arguments [0].first, arguments [0].second - arguments [0].first);

	    if ((after < source.size () && source [after] == '{') ||
		std::regex_match (firstArgument, parameterDeclaration)) {
		pos = afterName;
		continue;
	    }

	    const auto signature = signatures.find (arguments.size ());
	    if (signature == signatures.end ()) {
		pos = close + 1;
		continue;
	    }
	    const ParameterTypes& typesByIndex = signature->second;

	    // Wrap right-to-left so the recorded offsets stay valid.
	    for (auto entry = typesByIndex.rbegin (); entry != typesByIndex.rend (); ++entry) {
		const size_t index = entry->first;
		const std::set<std::string>& types = entry->second;
		if (index >= arguments.size () || types.contains ("")) {
		    continue;
		}

		std::string targetType;
		if (types.size () == 1) {
		    targetType = *types.begin ();
		} else if (types.contains ("int")) {
		    // Preserve the older int-parameter compatibility for combo variants that expose
		    // both an int and a float signature in the un-preprocessed source.
		    targetType = "int";
		} else {
		    continue;
		}

		const auto& [argumentBegin, argumentEnd] = arguments [index];
		const std::string argument = source.substr (argumentBegin, argumentEnd - argumentBegin);
		const std::regex alreadyConverted ("^\\s*" + targetType + "\\s*\\(.*\\)\\s*$");
		if (std::regex_match (argument, alreadyConverted)) {
		    continue;
		}

		source.insert (argumentEnd, ")");
		source.insert (argumentBegin, targetType + "(");
	    }

	    pos = afterName;
	}
    }

    return source;
}

std::string ShaderUnit::applyNumericInitializerCompatibility (std::string source) const {
    static const std::regex declaration (
	R"(\b((?:[biu]?vec[234]|float[234]|int[234]|float|int|uint|bool))\s+[A-Za-z_][A-Za-z0-9_]*\s*=\s*)");
    size_t offset = 0;
    bool changed = false;

    while (offset < source.size ()) {
	std::smatch match;
	if (!std::regex_search (source.cbegin () + offset, source.cend (), match, declaration)) {
	    break;
	}

	const std::string type = match [1].str ();
	const size_t initializerBegin = offset + match.position () + match.length ();
	const size_t semicolon = source.find (';', initializerBegin);
	const size_t newline = source.find ('\n', initializerBegin);
	if (semicolon == std::string::npos || (newline != std::string::npos && newline < semicolon)) {
	    offset = initializerBegin;
	    continue;
	}

	int depth = 0;
	bool topLevelComma = false;
	size_t modulo = std::string::npos;
	for (size_t i = initializerBegin; i < semicolon; i++) {
	    if (source [i] == '(' || source [i] == '[') {
		depth++;
	    } else if (source [i] == ')' || source [i] == ']') {
		depth--;
	    } else if (depth == 0 && source [i] == ',') {
		topLevelComma = true;
	    } else if (depth == 0 && source [i] == '%') {
		modulo = i;
	    }
	}

	if (topLevelComma) {
	    offset = semicolon + 1;
	    continue;
	}

	std::string initializer = source.substr (initializerBegin, semicolon - initializerBegin);
	if ((type == "int" || type == "uint") && modulo != std::string::npos) {
	    const size_t relativeModulo = modulo - initializerBegin;
	    std::string left = initializer.substr (0, relativeModulo);
	    const std::string right = initializer.substr (relativeModulo + 1);
	    if (type == "uint") {
		static const std::regex unsignedAddition (
		    R"((\b([A-Za-z_][A-Za-z0-9_]*)\s*\+\s*)([0-9]+)(?![A-Za-z0-9_]))");
		std::smatch addition;
		if (std::regex_search (left, addition, unsignedAddition)) {
		    const std::regex unsignedDeclaration ("\\buint\\s+" + addition [2].str () + "\\b");
		    if (std::regex_search (source.cbegin (), source.cbegin () + initializerBegin,
					   unsignedDeclaration)) {
			left.replace (addition.position (), addition.length (),
				      addition [1].str () + addition [3].str () + "u");
		    }
		}
	    }
	    initializer = type + "(" + left + ") % " + type + "(" + right + ")";
	}

	const std::regex alreadyConverted ("^\\s*" + type + "\\s*\\(.*\\)\\s*$");
	if (!std::regex_match (initializer, alreadyConverted)) {
	    initializer = type + "(" + initializer + ")";
	}

	source.replace (initializerBegin, semicolon - initializerBegin, initializer);
	offset = initializerBegin + initializer.size () + 1;
	changed = true;
    }

    // Material properties are floats even when authors use them as integer loop bounds.
    // Comparing in float space matches HLSL promotion while keeping the induction variable int.
    static const std::regex integerLoop (
	R"((for\s*\(\s*int\s+([A-Za-z_][A-Za-z0-9_]*)\s*=\s*[^;]+;\s*)([A-Za-z_][A-Za-z0-9_]*)(\s*(?:<=|>=|<|>)\s*)([^;]+)(;))");
    std::smatch loop;
    offset = 0;
    while (std::regex_search (source.cbegin () + offset, source.cend (), loop, integerLoop)) {
	const size_t position = offset + loop.position ();
	if (loop [2].str () != loop [3].str ()) {
	    offset = position + loop.length ();
	    continue;
	}
	const std::string replacement = loop [1].str () + "float(" + loop [3].str () + ")" + loop [4].str ()
	    + "float(" + loop [5].str () + ")" + loop [6].str ();
	source.replace (position, loop.length (), replacement);
	offset = position + replacement.size ();
	changed = true;
    }

    if (changed) {
	sLog.out ("Applied numeric initializer compatibility in ", this->m_file);
    }
    return source;
}

std::string ShaderUnit::applyVectorBuiltinCompatibility (std::string source) const {
    const std::string original = source;
    source = reorderLiteralMax (std::move (source));

    // These are the recurring HLSL idioms found by the corpus validator. Keep the rules
    // syntax-directed: guessing types from identifier names across combo branches can corrupt
    // otherwise valid stock shaders that reuse local names with different types.
    static const std::regex vectorPow (
	R"(pow\s*\(((?:[A-Za-z_][A-Za-z0-9_]*\.(?:rgb|xyz)|color\s*/\s*luma))\s*,\s*([^,;\n\)]+)\))");
    static const std::regex vectorMax (
	R"(max\s*\((abs\s*\(\s*[A-Za-z_][A-Za-z0-9_]*\s*\)\s*-\s*[A-Za-z_][A-Za-z0-9_]*\.xy\s*\+\s*[A-Za-z_][A-Za-z0-9_]*)\s*,\s*(0(?:\.0+)?)\s*\))");
    static const std::regex scalarVectorMix (
	R"(mix\s*\(\s*(luma)\s*,\s*(color)\s*,)");
    static const std::regex wideNarrowMix (
	R"(mix\s*\(\s*(albedo)\s*,\s*(newAlbedo)\s*,)");
    static const std::regex narrowStrengthCompound (
	R"(\bstrength\s*\*=\s*(500\s*/\s*g_Texture0Resolution)\s*;)");
    static const std::regex dotMatrixSmoothstep (
	R"(smoothstep\s*\(\s*dist\s*,\s*dist\s*\+\s*smoothing\s*,\s*thresh\s*\))");
    static const std::regex dotMatrixAccumulation (
	R"(fragLV\s*\+=\s*(smoothstep\s*\(\s*dist\s*,\s*dist\s*\+\s*smoothing\s*,\s*vec2\s*\(\s*thresh\s*\)\s*\)))");
    static const std::regex vectorDeltaMax (
	R"(max\s*\(\s*delta\s*,\s*(0(?:\.0+)?)\s*\))");
    static const std::regex albedoGammaPow (
	R"(pow\s*\(\s*albedo\s*,\s*startGamma\s*\))");
    static const std::regex inverseAlbedoGammaPow (
	R"(pow\s*\(\s*albedo\s*,\s*1\.0\s*/\s*startGamma\s*\))");
    static const std::regex chromaticTimer (
	R"((u_[rgb]Offset\s*\*\s*)timer\b(?!\.))");
    static const std::regex stereoChannelClip (
	R"(barLeft\s*\*=\s*isLeftChannel\s*;\s*barRight\s*\*=\s*isRightChannel\s*;)");
    static const std::regex integerBarStep (
	R"((bar(?:Left|Right)\s*\*=\s*)step\s*\(([^;\n]+)\)\s*;)");
    static const std::regex integerInvertedBarStep (
	R"((bar(?:Left|Right)\s*\*=\s*)(1\.0\s*-\s*step\s*\([^;\n]+\))\s*;)");

    source = std::regex_replace (source, vectorPow, "pow($1, vec3($2))");
    source = std::regex_replace (source, vectorMax, "max($1, vec2($2))");
    source = std::regex_replace (source, scalarVectorMix, "mix(vec3($1), $2,");
    source = std::regex_replace (source, wideNarrowMix, "mix($1.rgb, $2,");
    source = std::regex_replace (source, narrowStrengthCompound, "strength *= vec2($1);");
    source = std::regex_replace (source, dotMatrixSmoothstep, "smoothstep(dist, dist + smoothing, vec2(thresh))");
    source = std::regex_replace (source, dotMatrixAccumulation, "fragLV += float($1)");
    source = std::regex_replace (source, vectorDeltaMax, "max(delta, vec2($1))");
    source = std::regex_replace (source, albedoGammaPow, "pow(albedo, vec4(startGamma))");
    source = std::regex_replace (source, inverseAlbedoGammaPow, "pow(albedo, vec4(1.0 / startGamma))");
    source = std::regex_replace (source, chromaticTimer, "$1timer.xy");
    source = std::regex_replace (
	source, stereoChannelClip,
	"#if ANTIALIAS == 1\n"
	"barLeft *= float(isLeftChannel); barRight *= float(isRightChannel);\n"
	"#else\n"
	"barLeft *= int(isLeftChannel); barRight *= int(isRightChannel);\n"
	"#endif");
    source = std::regex_replace (source, integerBarStep, "$1int(step($2));");
    source = std::regex_replace (source, integerInvertedBarStep, "$1int($2);");

    if (source != original) {
	sLog.out ("Applied vector built-in compatibility in ", this->m_file);
    }
    return source;
}

std::string ShaderUnit::applyBooleanArithmeticCompatibility (std::string source) const {
    static const std::regex comparisonProduct (
	R"(return\s+\(([^;\n]*(?:<=|>=|==|!=|<|>)[^;\n]*)\)\s*\*\s*\(([^;\n]*(?:<=|>=|==|!=|<|>)[^;\n]*)\)\s*;)");
    static const std::regex audioCircleMask (
	R"((\bbar\s*\*=\s*)(shapeCoord\.x\s*>\s*0\.0\s*&&\s*shapeCoord\.x\s*\*\s*sign\s*\(\s*endAngle\s*-\s*startAngle\s*\)\s*<\s*1\.0)\s*;)");
    std::string patched = std::regex_replace (source, comparisonProduct, "return float(($1) && ($2));");
    // Simple Audio Bars switches bar between int and float with ANTIALIAS.
    // A numeric 0/1 mask preserves both types without truncating its amplitude.
    patched = std::regex_replace (patched, audioCircleMask, "$1int($2);");
    if (patched != source) {
	sLog.out ("Applied boolean arithmetic compatibility in ", this->m_file);
    }
    return patched;
}

std::string ShaderUnit::applyFragmentTexCoordCompatibility (std::string source) const {
    if (this->m_type != GLSLContext::UnitType_Fragment) {
	return source;
    }

    const std::string original = source;
    static const std::regex wideTexCoordDecl (R"(\bvarying\s+vec[34]\s+v_TexCoord\s*;)");
    static const std::string declarationPrefix = R"(\bvarying\s+(?:(?:lowp|mediump|highp)\s+)?)";
    static const std::regex narrowTexCoordDecl (declarationPrefix + R"(vec2\s+(v_TexCoord[0-9]*)\s*;)");
    std::set<std::string> narrowNames;
    for (std::sregex_iterator it (source.begin (), source.end (), narrowTexCoordDecl), end; it != end; ++it) {
	narrowNames.insert ((*it)[1].str ());
    }
    for (const auto& name : narrowNames) {
	std::smatch linkedDeclaration;
	if (m_link == nullptr
	    || !std::regex_search (m_link->m_preprocessed, linkedDeclaration,
		std::regex (declarationPrefix + "(vec[34])\\s+" + name + "\\s*;"))
	    || std::regex_search (m_link->m_preprocessed,
		std::regex (declarationPrefix + "vec2\\s+" + name + "\\s*;"))) {
	    continue;
	}
	// Widen before parsing: glslang otherwise folds vec2.wz to .yx and loses
	// animated higher components. Keep bare vec2 reads and matching combo branches.
	const std::regex anyDeclaration ("(" + declarationPrefix + ")(vec[234])\\s+" + name + "\\s*;");
	const std::regex bareUse ("\\b" + name + "\\b(?!\\s*\\.)");
	const std::regex shieldedNarrow ("(" + declarationPrefix + ")vec2\\s+V_TEXCOORD_DECL\\s*;");
	source = std::regex_replace (source, anyDeclaration, "$1$2 V_TEXCOORD_DECL;");
	source = std::regex_replace (source, bareUse, name + ".xy");
	source = std::regex_replace (source, shieldedNarrow, "$1" + linkedDeclaration[1].str () + " " + name + ";");
	source = std::regex_replace (source, std::regex ("V_TEXCOORD_DECL"), name);
    }
    if (std::regex_search (source, wideTexCoordDecl)) {
	static const std::regex texCoordBeforeVec2 (R"(\bv_TexCoord\b(\s*[-+*/]\s*(?:CAST2|vec2)\s*\())");
	static const std::regex vec2BeforeTexCoord (R"(((?:CAST2|vec2)\s*\([^)]+\)\s*[-+*/]\s*)\bv_TexCoord\b(?!\s*\.))");
	static const std::regex sampledTexCoord (
	    R"(((?:texSample2D|texSample2DLod)\s*\([^,]+,\s*)\bv_TexCoord\b(?!\s*\.))"
	);
	// HLSL allows implicit truncation on assignment (vec2 x = v_TexCoord), GLSL does not
	static const std::regex vec2AssignTexCoord (R"((\bvec2\s+[A-Za-z_][A-Za-z0-9_]*\s*=\s*)v_TexCoord\s*;)");
	static const std::regex vec2InitializerTexCoord (
	    R"((\b(?:vec2|float2)\s+[A-Za-z_][A-Za-z0-9_]*\s*=\s*[^;\n]*?)\bv_TexCoord\b(?!\s*\.))");

	source = std::regex_replace (source, texCoordBeforeVec2, "v_TexCoord.xy$1");
	source = std::regex_replace (source, vec2BeforeTexCoord, "$1v_TexCoord.xy");
	source = std::regex_replace (source, sampledTexCoord, "$1v_TexCoord.xy");
	source = std::regex_replace (source, vec2AssignTexCoord, "$1v_TexCoord.xy;");
	source = std::regex_replace (source, vec2InitializerTexCoord, "$1v_TexCoord.xy");
    }

    if (source != original) {
	sLog.out ("Applied fragment TexCoord vec2 compatibility in ", this->m_file);
    }

    return source;
}

std::string ShaderUnit::applyNonConstantInitializerCompatibility (std::string source) const {
    const std::string original = source;
    static const std::regex uniformInitializer (
	R"(\bconst(\s+(?:(?:[biu]?vec|float|int|uint)[234]?|bool|mat[234](?:x[234])?)\s+[A-Za-z_][A-Za-z0-9_]*(?:\s*\[[^\]]*\])?\s*=\s*[^;\n]*\b[ug]_[A-Za-z_][A-Za-z0-9_]*[^;\n]*;))"
    );
    static const std::regex authoredTemporaryInitializer (
	R"(\bconst(\s+(?:(?:[biu]?vec|float|int|uint)[234]?|bool|mat[234](?:x[234])?)\s+__[A-Za-z_][A-Za-z0-9_]*\s*=\s*[^;\n]*;))");

    source = std::regex_replace (source, uniformInitializer, "$1");
    source = std::regex_replace (source, authoredTemporaryInitializer, "$1");
    if (source != original) {
	sLog.out ("Applied uniform initializer compatibility in ", this->m_file);
    }
    return source;
}

std::string ShaderUnit::applyDuplicateMacroCompatibility (std::string source) const {
    static const std::regex definePattern (R"(^\s*#\s*define\s+([A-Za-z_][A-Za-z0-9_]*))");
    std::set<std::string> defined;
    std::istringstream input (source);
    std::ostringstream output;
    std::string line;
    bool changed = false;

    while (std::getline (input, line)) {
	std::smatch match;
	if (std::regex_search (line, match, definePattern) && !defined.insert (match[1].str ()).second) {
	    output << "#undef " << match[1].str () << '\n';
	    changed = true;
	}
	output << line << '\n';
    }

    if (changed) {
	sLog.out ("Applied duplicate macro compatibility in ", this->m_file);
    }
    return changed ? output.str () : source;
}

void ShaderUnit::preprocessReservedIdentifiers () {
    // The legacy blend effect uses `input` as a local RGBA value. Wallpaper
    // Engine accepts it, but GLSL reserves input/output even as variable names.
    // Keep material metadata intact and rename declarations, macros and uses
    // together before uniform bindings are collected.
    static std::mutex cacheMutex;
    static std::unordered_map<std::string, std::string> cache;
    static size_t cacheBytes = 0;
    {
        const std::lock_guard lock (cacheMutex);
        if (const auto found = cache.find (m_preprocessed); found != cache.end ()) {
            m_preprocessed = found->second;
            return;
        }
    }

    const std::string code = maskShaderComments (m_preprocessed);
    static const std::regex reserved (R"(\b(input|output)\b)");
    std::string result;
    size_t copied = 0;
    for (std::sregex_iterator it (code.begin (), code.end (), reserved), end; it != end; ++it) {
	const size_t position = it->position ();
	result.append (m_preprocessed, copied, position - copied);
	result.append ("_lweReserved_").append (it->str ());
	copied = position + it->length ();
    }
    result.append (m_preprocessed, copied);

    const size_t entryBytes = m_preprocessed.size () + result.size () + sizeof (decltype (cache)::value_type);
    if (entryBytes <= SHADER_TEXT_CACHE_BUDGET_BYTES) {
        const std::lock_guard lock (cacheMutex);
        if (cacheBytes + entryBytes > SHADER_TEXT_CACHE_BUDGET_BYTES) {
            cache.clear ();
            cacheBytes = 0;
        }
        if (cache.emplace (m_preprocessed, result).second) cacheBytes += entryBytes;
    }
    m_preprocessed = std::move (result);
}

std::string ShaderUnit::applyHeaderMacroCompatibility (std::string source) const {
    static const std::regex authoredLog10 (R"(\b(?:float|vec[234])\s+log10\s*\()");
    std::smatch match;
    if (std::regex_search (source, match, authoredLog10)) {
	source.insert (match.position (), "#undef log10\n");
	sLog.out ("Applied log10 header macro compatibility in ", this->m_file);
    }
    return source;
}

std::string ShaderUnit::applyWritableInputs (std::string source) const {
    const std::string code = maskShaderComments (source);
    // HLSL shader inputs are writable copies. GLSL inputs are read-only;
    // shadow only the inputs assigned by the authored shader body.
    static const std::regex writePattern (R"(\b([A-Za-z_]\w*)(?:\.[xyzwrgba]+)?\s*[+\-*/]?=[^=])");
    std::set<std::string> written;
    for (std::sregex_iterator it (code.begin (), code.end (), writePattern), end; it != end; ++it) {
	written.insert ((*it)[1].str ());
    }
    if (written.empty ()) {
	return source;
    }

    static const std::regex mainPattern (R"(\bvoid\s+main\s*\([^)]*\)\s*\{)");
    std::vector<std::pair<size_t, size_t>> mains;
    for (std::sregex_iterator it (code.begin (), code.end (), mainPattern), end; it != end; ++it) {
	mains.emplace_back (it->position (), it->position () + it->length ());
    }
    // Insert from the end so offsets remain valid when a shader provides
    // several main() definitions selected by preprocessor conditions.
    for (auto entry = mains.rbegin (); entry != mains.rend (); ++entry) {
	const auto [mainPosition, bodyPosition] = *entry;
	std::set<std::string> mainWritten = written;
	size_t bodyEnd = bodyPosition;
	int depth = 1;
	while (bodyEnd < code.size () && depth > 0) {
	    if (code[bodyEnd] == '{') {
		depth++;
	    } else if (code[bodyEnd] == '}') {
		depth--;
	    }
	    bodyEnd++;
	}
	// Preserve explicit local copies within this entry point, including
	// locals whose type differs from the unused interpolated input.
	static const std::regex localDeclaration (
	    R"(\b(?:bool|int|uint|float[234]?|double|[biud]?vec[234]|d?mat[234](?:x[234])?)\s+(\w+)\s*(?:=|;|,))"
	);
	for (std::sregex_iterator it (code.begin () + bodyPosition, code.begin () + bodyEnd, localDeclaration), end;
	     it != end; ++it) {
	    mainWritten.erase ((*it)[1].str ());
	}
	static const std::regex varyingDecl (R"(^\s*varying\s+(\w+)\s+(\w+)\s*;)");
	static const std::regex attributeDecl (R"(^\s*attribute\s+(\w+)\s+(\w+)\s*;)");
	const auto& inputDecl = m_type == GLSLContext::UnitType_Fragment ? varyingDecl : attributeDecl;
	std::vector<std::string> conditions;
	std::string aliases;
	size_t position = 0;
	while (position < mainPosition) {
	    const size_t newline = code.find ('\n', position);
	    const size_t end = std::min (newline, mainPosition);
	    const std::string line = code.substr (position, end - position);
	    const auto directive = preprocessorDirective (line);
	    std::smatch declaration;
	    if (directive == "if" || directive == "ifdef" || directive == "ifndef") {
		conditions.push_back (line + '\n');
	    } else if ((directive == "else" || directive == "elif") && !conditions.empty ()) {
		conditions.back () += line + '\n';
	    } else if (directive == "endif" && !conditions.empty ()) {
		conditions.pop_back ();
	    } else if (
		std::regex_search (line, declaration, inputDecl) && mainWritten.contains (declaration[2].str ())
	    ) {
		// Preserve the real preprocessor conditions instead of guessing their
		// truth values. Cached source then works for every combo variant,
		// including different input widths under #if / #elif / #else.
		for (const auto& condition : conditions) {
		    aliases += condition;
		}
		const std::string name = declaration[2].str ();
		aliases += "    " + declaration[1].str () + " " + name + " = " + name + ";\n";
		for (size_t i = 0; i < conditions.size (); ++i) {
		    aliases += "#endif\n";
		}
	    }
	    position = end + 1;
	}
	if (!aliases.empty ()) {
	    source.insert (bodyPosition, "\n" + aliases);
	}
    }
    return source;
}

std::string ShaderUnit::applyFloatTernaryCompatibility (std::string source) const {
    // HLSL allows float/int as ternary conditions; GLSL 330 requires bool.
    // Match a bare identifier (no operators before it) used as a ternary condition in:
    //   assignment:    = ident ?
    //   return:        return ident ?
    //   argument:      ( ident ?
    //   comma-sep:     , ident ?
    // and wrap it with bool() so GLSL accepts it.  bool(bool_val) is a no-op, so
    // wrapping an already-bool identifier is harmless.
    static const std::regex floatTernaryRe (
        R"(((?:=\s*|return\s+|,\s*|\(\s*))([A-Za-z_][A-Za-z0-9_]*)\s*\?)"
    );
    const std::string patched = std::regex_replace (source, floatTernaryRe, "$1bool($2) ?");
    if (patched != source) {
	sLog.out ("Applied float-ternary bool() cast in ", this->m_file);
    }
    return patched;
}

void ShaderUnit::parseComboConfiguration (const std::string& content, const int defaultValue) {
    JSON data;
    try {
	data = WallpaperEngine::Data::JSON::parseCompatible (content, this->m_file + " combo metadata");
    } catch (const std::exception& e) {
	sLog.error ("Cannot parse combo metadata in shader ", this->m_file, ": ", e.what ());
	return;
    }
    const auto comboEntry = data.find ("combo");
    if (comboEntry == data.end () || !comboEntry->is_string () || !validComboName (comboEntry->get<std::string> ())) {
	sLog.error ("Ignoring invalid combo name in shader ", this->m_file, ": ", content);
	return;
    }
    const auto combo = comboEntry->get<std::string> ();
    // ignore type as it seems to be used only on the editor
    // const auto type = data.find ("type");
    const auto defvalue = data.find ("default");

    // "require" describes availability in the editor, not runtime overrides.
    // The native [COMBO] reader retains combo/default only. A saved child
    // option must not enable a parent option the author has disabled.

    // check the combos
    const auto entry = this->m_combos.find (combo);
    const auto entryOverride = this->m_overrideCombos.find (combo);

    // add the combo to the found list
    this->m_usedCombos.emplace (combo, true);

    // if the combo was not found in the predefined values this means that the default value in the JSON data can be
    // used so only define the ones that are not already defined
    if (entry == this->m_combos.end () && entryOverride == this->m_overrideCombos.end ()) {
	// if no combo is defined just load the default settings
	if (defvalue == data.end ()) {
	    // A combo without an authored default uses the caller's disabled value.
	    this->m_discoveredCombos.emplace (combo, defaultValue);
	} else if (defvalue->is_number_float ()) {
	    sLog.exception ("float combos are not supported in shader ", this->m_file, ". ", combo);
	} else if (defvalue->is_number_integer ()) {
	    this->m_discoveredCombos.emplace (combo, defvalue->get<int> ());
	} else if (defvalue->is_string ()) {
	    sLog.exception ("string combos are not supported in shader ", this->m_file, ". ", combo);
	} else {
	    sLog.exception ("cannot parse combo information ", combo, ". unknown type for ", defvalue->dump ());
	}
    }
}

void ShaderUnit::parseParameterConfiguration (
    const std::string& type, const std::string& name, const std::string& content
) {
    JSON data;
    try {
	data = WallpaperEngine::Data::JSON::parseCompatible (
	    content, this->m_file + " parameter metadata for " + name
	);
    } catch (const std::exception& e) {
	sLog.error ("Cannot parse parameter metadata for ", name, " in shader ", this->m_file, ": ", e.what ());
	return;
    }
    const auto material = data.optional ("material");
    const auto defvalue = data.optional ("default");
    // auto range = data.find ("range");
    const auto combo = data.find ("combo");

    // CPass binds texture resolutions from the actual textures at draw time.
    static const std::regex textureResolution (R"(g_Texture[0-9]Resolution)");
    const auto annotationType = data.find ("type");
    if (type == "vec4" && annotationType != data.end () && *annotationType == "resolution"
        && std::regex_match (name, textureResolution) && data.find ("material") == data.end ()
        && data.find ("default") == data.end () && combo == data.end ()) {
        return;
    }

    // this is not a real parameter
    auto constant = this->m_constants.end ();

    if (material.has_value ()) {
	constant = this->m_constants.find (*material);
    }

    if (constant == this->m_constants.end () && !defvalue.has_value ()) {
	if (type != "sampler2D" && type != "sampler3D" && type != "sampler2DBackBuffer" && type != "sampler2DComparison") {
	    sLog.exception ("Cannot parse parameter data for ", name, " in shader ", this->m_file);
	}
    }

    Variables::ShaderVariable* parameter = nullptr;

    // Some workshop shader defaults use commas between vector components.
    // Normalize metadata here without changing scene vector parsing.
    std::string vectorDefault;
    if ((type == "vec2" || type == "vec3" || type == "vec4") && defvalue->is_string ()) {
	static const std::regex commaSeparator (R"(\s*,\s*)");
	vectorDefault = std::regex_replace (defvalue->get<std::string> (), commaSeparator, " ");
    }

    // Workshop metadata can use a scalar default for a vector (for example
    // Deformer Simulation's vec2 position defaults to 0). Expand that scalar
    // across the components, while preserving authored vector strings.
    if (type == "vec4") {
	parameter = new Variables::ShaderVariableVector4 (
	    defvalue->is_number () ? glm::vec4 (defvalue->get<float> ())
				  : VectorBuilder::parse<glm::vec4> (vectorDefault)
	);
    } else if (type == "vec3") {
	parameter = new Variables::ShaderVariableVector3 (
	    defvalue->is_number () ? glm::vec3 (defvalue->get<float> ())
				  : VectorBuilder::parse<glm::vec3> (vectorDefault)
	);
    } else if (type == "vec2") {
	parameter = new Variables::ShaderVariableVector2 (
	    defvalue->is_number () ? glm::vec2 (defvalue->get<float> ())
				  : VectorBuilder::parse<glm::vec2> (vectorDefault)
	);
    } else if (type == "float") {
	if (defvalue->is_string ()) {
	    parameter = new Variables::ShaderVariableFloat (std::stoi (defvalue->get<std::string> ()));
	} else {
	    parameter = new Variables::ShaderVariableFloat (defvalue->get<float> ());
	}
    } else if (type == "int") {
	if (defvalue->is_string ()) {
	    parameter = new Variables::ShaderVariableInteger (std::stoi (defvalue->get<std::string> ()));
	} else {
	    parameter = new Variables::ShaderVariableInteger (defvalue->get<int> ());
	}
    } else if (type == "sampler2D" || type == "sampler3D" || type == "sampler2DComparison" || type == "sampler2DBackBuffer") {
	// samplers can have special requirements, check what sampler we're working with and create definitions
	// if needed
	const auto textureName = data.find ("default");
	// TODO: CREATE TEXTURE WITH THE GIVEN COLOR
	// extract the texture number from the name
	const char value = name.at (std::string ("g_Texture").length ());
	const auto requireany = data.find ("requireany");
	const auto require = data.find ("require");
	const auto components = data.find ("components");
	// now convert it to integer
	// TODO: BETTER CONVERSION HERE
	size_t index = value - '0';
	// TODO: SUPPORT USER TEXTURES!!
	const auto formatCombo = data.find ("formatcombo");
	if (formatCombo != data.end () && formatCombo->is_boolean () && formatCombo->get<bool> ()) {
	    this->m_textureFormatRequests.insert (index);
	}

	// Packed material textures record their authored channels in texture flag
	// bits 20..23. CPass exposes that nibble as TEX<n>COMPONENTS; match the
	// sampler's ordered component metadata to the corresponding feature combos.
	// Explicit material/override combos retain precedence over these discovered
	// values through the normal combo chain.
	if (components != data.end () && components->is_array ()) {
	    const std::string componentMaskName = "TEX" + std::to_string (index) + "COMPONENTS";
	    int componentMask = 0;
	    if (const auto it = this->m_combos.find (componentMaskName); it != this->m_combos.end ()) {
		componentMask = it->second;
	    }

	    size_t componentIndex = 0;
	    for (const auto& component : *components) {
		if (componentIndex >= 4) {
		    break;
		}

		const auto componentCombo = component.find ("combo");
		if ((componentMask & (1 << componentIndex)) != 0 && componentCombo != component.end ()
		    && componentCombo->is_string ()) {
		    const std::string comboName = componentCombo->get<std::string> ();
		    this->m_discoveredCombos.emplace (comboName, 1);
		    this->m_usedCombos.emplace (comboName, true);
		}
		componentIndex++;
	    }
	}

	if (combo != data.end ()) {
	    // TODO: CLEANUP HOW THIS IS DETERMINED FIRST
	    // if the texture exists (and is not null), add to the combo
	    const auto textureSlotUsed
		= this->m_passTextures.contains (index) || this->m_overrideTextures.contains (index);
	    bool isRequired = false;
	    int comboValue = 1;

	    if (textureSlotUsed) {
		// nothing extra to do, the texture exists, the combo must be set
		// these tend to not have default value
		isRequired = true;
	    } else if (require != data.end ()) {
		// this is required based on certain conditions
		if (requireany != data.end () && requireany->get<bool> ()) {
		    // any of the values set are valid, check for them
		    for (const auto& item : require->items ()) {
			const std::string& macro = item.key ();
			const auto it = this->m_combos.find (macro);

			// if any of the values matched, this option is required
			if (it == this->m_combos.end () || this->m_overrideCombos.contains (macro)
			    || it->second != item.value ()) {
			    isRequired = true;
			    break;
			}
		    }
		} else {
		    isRequired = true;

		    // all values must match for it to be required
		    for (const auto& item : require->items ()) {
			const std::string& macro = item.key ();
			const auto it = this->m_combos.find (macro);

			// these can not exist and that'd be fine, we just care about the values
			if ((it != this->m_combos.end () || this->m_overrideCombos.contains (macro))
			    && it->second == item.value ()) {
			    isRequired = false;
			    break;
			}
		    }
		}
	    }

	    if (isRequired && !textureSlotUsed) {
		if (!defvalue.has_value ()) {
		    isRequired = false;
		} else {
		    // is the combo registered already?
		    // if not, add it with the default value
		    // there's already a combo providing this value, so it doesn't need to be added
		    if (this->m_combos.contains (*combo) || this->m_overrideCombos.contains (*combo)) {
			isRequired = false;
			// otherwise a default value must be used
		    } else if (defvalue->is_string ()) {
			comboValue = std::stoi (defvalue->get<std::string> ().c_str ());
		    } else if (defvalue->is_number ()) {
			comboValue = *defvalue;
		    } else {
			sLog.exception (
			    "Cannot determine default value for combo ", combo->get<std::string> (),
			    " because it's not specified by the shader and is not given a default value: ", this->m_file
			);
		    }
		}
	    }

	    if (isRequired) {
		// add the new combo to the list
		this->m_discoveredCombos.emplace (*combo, comboValue);
		// textures linked to combos need to be tracked too
		this->m_usedCombos.emplace (*combo, true);
	    }
	}

	if (textureName != data.end ()) {
	    this->m_defaultTextures.emplace (index, *textureName);
	}

	// samplers are not saved, we can ignore them for now
	return;
    } else {
	sLog.error ("Unknown parameter type: ", type, " for ", name, " in shader ", this->m_file);
	return;
    }

    if (material.has_value () && parameter != nullptr) {
	parameter->setIdentifierName (*material);
	parameter->setName (name);

	this->m_parameters.push_back (parameter);
    }
}

void ShaderUnit::stripOrphanedEndifs () {
    // Workshop shaders (authored for HLSL's lenient preprocessor) sometimes emit one extra
    // #endif that has no matching #if.  GLSL's preprocessor rejects these.  Walk the shader
    // line-by-line, track depth, and blank-out any #endif whose depth would underflow.
    int depth = 0;
    size_t pos = 0;

    while (pos < this->m_preprocessed.size ()) {
        const size_t lineEnd = this->m_preprocessed.find ('\n', pos);
        const size_t end = (lineEnd == std::string::npos) ? this->m_preprocessed.size () : lineEnd;

        // Trim leading whitespace for the directive check only
        size_t tok = pos;
        while (tok < end && (this->m_preprocessed [tok] == ' ' || this->m_preprocessed [tok] == '\t')) {
            tok++;
        }

        const size_t remaining = end - tok;
        if (remaining >= 3 && this->m_preprocessed.substr (tok, 3) == "#if") {
            depth++;
        } else if (remaining >= 6 && this->m_preprocessed.substr (tok, 6) == "#endif") {
            if (depth == 0) {
                // orphaned #endif — comment it out
                this->m_preprocessed [tok] = '/';
                this->m_preprocessed [tok + 1] = '/';
            } else {
                depth--;
            }
        }

        pos = (lineEnd == std::string::npos) ? this->m_preprocessed.size () : lineEnd + 1;
    }
}

const ComboMap& ShaderUnit::getCombos () const { return this->m_combos; }

const ComboMap& ShaderUnit::getDiscoveredCombos () const { return this->m_discoveredCombos; }

void ShaderUnit::linkToUnit (const ShaderUnit* unit) {
    this->m_link = unit;
    if (unit == nullptr || this->m_type != GLSLContext::UnitType_Fragment) return;

    // D3D keeps vertex and pixel constants separate. GLSL links equally named
    // uniforms, even when their material controls differ. Foliage sway uses
    // g_Strength for vertex displacement (default 100) and UV sway (default .3).
    // Give conflicting fragment controls their own binding before translation.
    std::map<std::string, std::string> renamed;
    for (auto* parameter : this->m_parameters) {
	for (const auto* linked : unit->getParameters ()) {
	    if (parameter->getName () == linked->getName ()
		&& parameter->getIdentifierName () != linked->getIdentifierName ()) {
		const std::string original = parameter->getName ();
		const std::string replacement = "_lweFragment_" + original;
		renamed.emplace (original, replacement);
		parameter->setName (replacement);
		break;
	    }
	}
    }
    if (renamed.empty ()) return;

    // Replace complete identifiers only; leave comments and material metadata
    // intact. Applying the same map to declarations, macros and uses also keeps
    // inactive combo branches valid without evaluating the preprocessor here.
    const std::string code = maskShaderComments (this->m_preprocessed);
    static const std::regex identifier (R"(\b[A-Za-z_][A-Za-z_0-9]*\b)");
    std::string result;
    size_t copied = 0;
    for (auto it = std::sregex_iterator (code.begin (), code.end (), identifier);
	 it != std::sregex_iterator (); ++it) {
	const auto replacement = renamed.find (it->str ());
	if (replacement == renamed.end ()) continue;
	const size_t position = it->position ();
	result.append (this->m_preprocessed, copied, position - copied);
	result.append (replacement->second);
	copied = position + it->length ();
    }
    result.append (this->m_preprocessed, copied);
    this->m_preprocessed = std::move (result);
}

const ShaderUnit* ShaderUnit::getLinkedUnit () const { return this->m_link; }

const std::string& ShaderUnit::compile () {
    if (!this->m_final.empty ()) {
	return this->m_final;
    }

    this->m_final = SHADER_HEADER (this->m_file);

    if (this->m_type == GLSLContext::UnitType_Fragment) {
	this->m_final += FRAGMENT_SHADER_DEFINES;
    } else {
	this->m_final += VERTEX_SHADER_DEFINES;
    }

    std::map<std::string, bool> addedCombos;

    for (const auto& [name, value] : this->m_overrideCombos) {
	std::string uppercase;
	std::ranges::transform (name, std::back_inserter (uppercase), ::toupper);

	if (!addedCombos.contains (uppercase)) {
	    this->m_final += comboDefinition (uppercase, value);
	    addedCombos.emplace (uppercase, true);
	}
    }

    // now add all the combos to the source
    for (const auto& [name, value] : this->m_combos) {
	std::string uppercase;
	std::ranges::transform (name, std::back_inserter (uppercase), ::toupper);

	if (!addedCombos.contains (uppercase)) {
	    this->m_final += comboDefinition (uppercase, value);
	    addedCombos.emplace (uppercase, true);
	}
    }

    for (const auto& [name, value] : this->m_discoveredCombos) {
	std::string uppercase;
	std::ranges::transform (name, std::back_inserter (uppercase), ::toupper);

	if (!addedCombos.contains (uppercase)) {
	    this->m_final += comboDefinition (uppercase, value);
	    addedCombos.emplace (uppercase, true);
	}
    }

    if (this->m_link != nullptr) {
	for (const auto& [name, value] : this->m_link->getCombos ()) {
	    std::string uppercase;
	    std::ranges::transform (name, std::back_inserter (uppercase), ::toupper);

	    if (!addedCombos.contains (uppercase)) {
		this->m_final += comboDefinition (uppercase, value);
		addedCombos.emplace (uppercase, true);
	    }
	}

	for (const auto& [name, value] : this->m_link->getDiscoveredCombos ()) {
	    std::string uppercase;
	    std::ranges::transform (name, std::back_inserter (uppercase), ::toupper);

	    if (!addedCombos.contains (uppercase)) {
		this->m_final += comboDefinition (uppercase, value);
		addedCombos.emplace (uppercase, true);
	    }
	}
    }

    // this should be the rest of the shader
    // Compatibility normally depends only on the unit type and linked sources.
    // Macro expansion also depends on the generated header, included below in
    // those cache keys. Shared shaders and switch-backs skip the regex passes.
    // Guarded by a mutex so a future worker-thread caller stays safe.
    static std::mutex sCompatCacheMutex;
    static std::unordered_map<std::string, std::string> sCompatCache;
    static size_t sCompatCacheBytes = 0;
    static const std::string sNoLink;

    // A native constant macro may contain a trailing semicolon. Wrapping its
    // unexpanded use in a numeric constructor places that semicolon inside the
    // expression. Expand these shaders with their actual combo values first.
    const bool adaptReflectionProjection = m_type == GLSLContext::UnitType_Fragment
	&& this->m_combos.contains ("LWE_LEGACY_REFLECTION_UV");
    const bool expandMacros = adaptReflectionProjection || hasStatementMacro (this->m_preprocessed);

    const std::string& linkedSource = this->m_link != nullptr ? this->m_link->m_preprocessed : sNoLink;
    std::string compatKey;
    compatKey.reserve (this->m_preprocessed.size () + linkedSource.size () + 64);
    compatKey.push_back (this->m_type == GLSLContext::UnitType_Vertex ? 'v' : 'f');
    compatKey.append (std::to_string (this->m_preprocessed.size ())).push_back (':');
    compatKey.append (this->m_preprocessed);
    compatKey.push_back ('\x1F');
    compatKey.append (std::to_string (linkedSource.size ())).push_back (':');
    compatKey.append (linkedSource);
    if (adaptReflectionProjection) compatKey.append ("\x1Dlegacy-reflection");
    if (expandMacros) {
	compatKey.push_back ('\x1E');
	compatKey.append (this->m_final);
    }

    {
	const std::lock_guard<std::mutex> lock (sCompatCacheMutex);
	const auto it = sCompatCache.find (compatKey);
	if (it != sCompatCache.end ()) {
	    if (expandMacros) this->m_final.clear ();
	    this->m_final += it->second;
	    // cache hits still produce a distinct fully-composed unit (different combos
	    // in the header), so dump those too
	    this->dumpFinalSource ();
	    return this->m_final;
	}
    }

    std::string compatResult;
    if (auto cached = ShaderDiskCache::get ().load ("compat", compatKey)) {
        Debug::RenderHealth::count ("shader.disk_compat_hit");
        compatResult = std::move (cached->first);
        if (expandMacros) this->m_final.clear ();
    } else {
        Debug::RenderHealth::count ("shader.disk_compat_miss");
	// Native shader loading seeds revision 69 before evaluating source conditionals,
	// independently of scene/project versions. Authored source definitions may
	// replace it; the duplicate-macro pass below preserves that behavior in GLSL.
	compatResult = "#undef SHADERVERSION\n#define SHADERVERSION 69\n" + this->m_preprocessed;
	normalizeAudioSampleConditions (compatResult);
	if (m_type == GLSLContext::UnitType_Vertex
	    && compatResult.find ("in_ParticleTrailLength") != std::string::npos
	    && compatResult.find ("trailRightStart") != std::string::npos) {
	    // The stock rope's non-GS fallback subtracts a scalar after multiplying
	    // by the width. Native genericropeparticle.geom emits position +/- right;
	    // center the fallback on the same path so child star heads stay attached.
	    static const std::regex offsetRope (
		R"(\bposition\s*\+=\s*right\s*\*\s*uvs\.x\s*\*\s*2\.0\s*-\s*1\.0\s*;)"
	    );
	    compatResult = std::regex_replace (compatResult, offsetRope, "position += right * (uvs.x * 2.0 - 1.0);");
	    // Native screen ropes face from each segment midpoint toward the eye.
	    // Preserve uniform facing for fixed/upright draws.
	    static const std::regex screenRope (
		R"((^|\n)[ \t]*(vec3\s+eyeDirection\s*=\s*mul\s*\(\s*g_OrientationForward\s*,\s*CAST3X3\s*\(\s*g_ModelMatrixInverse\s*\)\s*\)\s*;))"
	    );
	    compatResult = std::regex_replace (compatResult, screenRope,
		"$1#if LWE_ROPE_SCREEN_FACING\n"
		"vec3 eyeModel = mul(vec4(g_EyePosition, 1.0), g_ModelMatrixInverse).xyz;\n"
		"vec3 eyeDirection = normalize((endPosition + startPosition) * 0.5 - eyeModel);\n"
		"#else\n$2\n#endif\n");
	}
	compatResult = this->applyLinkedVaryingCompatibility (std::move (compatResult));
	compatResult = this->applyMissingFragmentVaryingCompatibility (std::move (compatResult));
	compatResult = this->applyFloatTernaryCompatibility (std::move (compatResult));
	compatResult = this->applyBooleanArithmeticCompatibility (std::move (compatResult));
	compatResult = this->applyWritableInputs (std::move (compatResult));
	compatResult = this->applyFragmentTexCoordCompatibility (std::move (compatResult));
	compatResult = this->applyVectorBuiltinCompatibility (std::move (compatResult));
	if (expandMacros) {
	    compatResult = this->applyHeaderMacroCompatibility (this->m_final + compatResult);
	    compatResult = this->applyDuplicateMacroCompatibility (std::move (compatResult));
	    this->m_final.clear ();
	    compatResult = GLSLContext::get ().preprocess (this->m_type, compatResult);
	}
	if (adaptReflectionProjection) adaptLegacyReflectionProjection (compatResult);
	flattenPackedAudioSpectrumIndices (compatResult);
	narrowRgbProducts (compatResult);
	compatResult = this->applyNumericParameterCallCompatibility (std::move (compatResult));
	compatResult = this->applyNumericInitializerCompatibility (std::move (compatResult));
	compatResult = this->applyNonConstantInitializerCompatibility (std::move (compatResult));
	compatResult = this->applyHeaderMacroCompatibility (std::move (compatResult));
	compatResult = this->applyDuplicateMacroCompatibility (std::move (compatResult));

        ShaderDiskCache::get ().store ("compat", compatKey, { compatResult, "" });
    }
    this->m_final += compatResult;

    {
	const std::lock_guard<std::mutex> lock (sCompatCacheMutex);
	const size_t entryBytes = compatKey.size () + compatResult.size ();
	if (entryBytes > SHADER_TEXT_CACHE_BUDGET_BYTES) {
	    this->dumpFinalSource ();
	    return this->m_final;
	}
	if (sCompatCacheBytes + entryBytes > SHADER_TEXT_CACHE_BUDGET_BYTES) {
	    sCompatCache.clear ();
	    sCompatCacheBytes = 0;
	}
	if (sCompatCache.emplace (compatKey, std::move (compatResult)).second) {
	    sCompatCacheBytes += entryBytes;
	}
    }

    this->dumpFinalSource ();

    // the pass itself handles shader compilation, the unit doesn't have enough information for this step
    return this->m_final;
}

ShaderUnit::~ShaderUnit () {
    for (auto* parameter : this->m_parameters) {
	delete parameter;
    }
}

void ShaderUnit::dumpFinalSource () const {
    // debug aid (WPE_DUMP_SHADERS=<dir>): dump the fully composed source for offline
    // inspection, e.g. batch-compiling every emitted unit with glslang
    Debug::dumpText (
        "WPE_DUMP_SHADERS", this->m_file, this->m_type == GLSLContext::UnitType_Vertex ? ".vert" : ".frag",
        this->m_final);
}

const std::vector<Variables::ShaderVariable*>& ShaderUnit::getParameters () const { return this->m_parameters; }
const TextureMap& ShaderUnit::getTextures () const { return this->m_defaultTextures; }

bool ShaderUnit::requestsTextureFormat (const size_t index) const {
    return this->m_textureFormatRequests.contains (index);
}
