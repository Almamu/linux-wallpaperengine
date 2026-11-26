#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <cctype>
#include <cstdlib>

#include "WallpaperEngine/Assets/AssetLocator.h"
#include "WallpaperEngine/Data/Model/UserSetting.h"
#include "WallpaperEngine/Render/Shaders/GLSLContext.h"
#include "WallpaperEngine/Render/Shaders/Shader.h"
#include "WallpaperEngine/Render/Shaders/ShaderUnit.h"
#include "WallpaperEngine/Render/Shaders/Variables/ShaderVariable.h"

using WallpaperEngine::Assets::AssetLocator;
using WallpaperEngine::Data::Model::ComboMap;
using WallpaperEngine::Data::Model::ShaderConstantMap;
using WallpaperEngine::Data::Model::TextureMap;
using WallpaperEngine::FileSystem::Container;
using WallpaperEngine::Render::Shaders::GLSLContext;
using WallpaperEngine::Render::Shaders::Shader;
using WallpaperEngine::Render::Shaders::ShaderUnit;

namespace {
constexpr auto nativeCoverageRemap =
    "out_FragColor.a=(clamp((out_FragColor.a-0.5)/max(fwidth(out_FragColor.a),0.0001)+0.5,0.0,1.0));";

std::string preprocessFragmentMain (const AssetLocator& assets, const std::string& name, const ComboMap& combos) {
    const ShaderConstantMap constants;
    const TextureMap textures;
    const ComboMap overrides;
    Shader shader (assets, name, combos, overrides, textures, textures, constants);
    auto source = GLSLContext::get ().preprocess (GLSLContext::UnitType_Fragment, shader.fragment ());
    std::erase_if (source, [] (unsigned char character) { return std::isspace (character); });
    // Ignore the shared clip() helpers emitted before the fragment entry point.
    const auto main = source.find ("voidmain(");
    REQUIRE (main != std::string::npos);
    return source.substr (main);
}

std::unique_ptr<AssetLocator> shaderAssets (const std::string& header, const std::string& nestedHeader = "") {
    auto container = std::make_unique<Container> ();
    container->getVFS ().add ("shaders/test_ordering.h", header);
    if (!nestedHeader.empty ()) {
	container->getVFS ().add ("shaders/test_nested.h", nestedHeader);
    }
    return std::make_unique<AssetLocator> (std::move (container));
}

std::string
compileFragment (const std::string& header, const std::string& source, const std::string& nestedHeader = "") {
    const auto assets = shaderAssets (header, nestedHeader);
    const ShaderConstantMap constants;
    const TextureMap textures;
    const ComboMap combos;
    ShaderUnit unit (
	GLSLContext::UnitType_Fragment, "test_ordering.frag", source, *assets, constants, textures, textures, combos,
	combos
    );
    return unit.compile ();
}

std::pair<std::string, std::string>
compileLinked (const std::string& vertexSource, const std::string& fragmentSource, const ComboMap& combos) {
    const auto assets = shaderAssets ("");
    const ShaderConstantMap constants;
    const TextureMap textures;
    ShaderUnit vertex (
	GLSLContext::UnitType_Vertex, "test_linked.vert", vertexSource, *assets, constants, textures, textures, combos,
	combos
    );
    ShaderUnit fragment (
	GLSLContext::UnitType_Fragment, "test_linked.frag", fragmentSource, *assets, constants, textures, textures,
	combos, combos
    );
    vertex.linkToUnit (&fragment);
    fragment.linkToUnit (&vertex);
    return { vertex.compile (), fragment.compile () };
}
} // namespace

TEST_CASE ("native depth buffer sampling compiles without a comparison sampler", "[shader][backbuffer]") {
    const auto sources = compileLinked (
        "void main() { gl_Position = vec4(0.0); }",
        "uniform sampler2DBackBuffer g_Texture1; // {\"hidden\":true,\"default\":\"_rt_volumetricsBack\"}\n"
        "uniform sampler2D g_Texture3;\n"
        "void main() { gl_FragColor = texSample2DBackBuffer(g_Texture1, vec2(.25), vec2(32))"
        " + texLoad2D(g_Texture3, vec2(.25), vec2(32)); }", {}
    );
    const auto translated = GLSLContext::get ().toGlsl (sources.first, sources.second);
    REQUIRE_FALSE (translated.second.empty ());
    CHECK_THAT (translated.second, Catch::Matchers::ContainsSubstring ("texelFetch"));
}

TEST_CASE ("native varying declarations ignore component suffixes", "[shader][varying-suffix]") {
    for (const std::string suffix : { "", ".xy", ".rgba" }) {
	for (const bool vertexSuffix : { false, true }) {
	    CAPTURE (suffix, vertexSuffix);
	    const auto sources = compileLinked (
		"varying vec4 v_Size" + (vertexSuffix ? suffix : "") + ";\n"
		"void main() { v_Size = vec4(0.25, 0.5, 0.75, 1.0); gl_Position = vec4(0.0); }\n",
		"// varying vec4 v_Comment.xy;\n"
		"varying vec4 v_Size" + (vertexSuffix ? "" : suffix) + ";\n"
		"void main() { gl_FragColor = v_Size; }\n", {}
	    );
	    const auto translated = GLSLContext::get ().toGlsl (sources.first, sources.second);
	    REQUIRE_FALSE (translated.first.empty ());
	    REQUIRE_FALSE (translated.second.empty ());
	    CHECK_THAT (sources.second, Catch::Matchers::ContainsSubstring ("// varying vec4 v_Comment.xy;"));
	}
    }
}

TEST_CASE ("varying suffix normalization preserves conditional types and expression swizzles", "[shader][varying-suffix]") {
    const std::string declarations =
        "#if SMALL\nvarying vec2 v_Size.xy;\n#else\nvarying vec4 v_Size.rgba;\n#endif\n"
        "/* varying vec4 v_Comment.xy; */\nvarying vec4 v_Offset . xy;\n";
    for (const int small : { 0, 1 }) {
	CAPTURE (small);
	const auto sources = compileLinked (
	    declarations + "void main() {\n#if SMALL\nv_Size = vec2(0.25, 0.5);\n"
	    "#else\nv_Size = vec4(0.25, 0.5, 0.75, 1.0);\n#endif\n"
	    "v_Offset = vec4(0.0, 0.0, 0.75, 1.0); gl_Position = vec4(0.0); }\n",
	    declarations + "void main() { gl_FragColor = vec4(v_Size.xy, v_Offset.zw); }\n",
	    ComboMap { { "SMALL", small } }
	);
	const auto translated = GLSLContext::get ().toGlsl (sources.first, sources.second);
	REQUIRE_FALSE (translated.first.empty ());
	REQUIRE_FALSE (translated.second.empty ());
	CHECK_THAT (sources.second, Catch::Matchers::ContainsSubstring ("vec4(v_Size.xy, v_Offset.zw)"));
	CHECK_THAT (sources.second, Catch::Matchers::ContainsSubstring ("/* varying vec4 v_Comment.xy; */"));
    }
}

TEST_CASE ("linked translation keys distinguish separators inside source comments", "[shader][cache-key]") {
    const std::string vertex = "#version 330\nvoid main() { gl_Position = vec4(0.0); }\n// vertex";
    const std::string fragment = "\n#version 330\nout vec4 color;\nvoid main() { color = vec4(1.0); }\n";
    const std::string extra = "// extra comment";
    const std::string firstFragment = extra + '\x1f' + fragment;
    const std::string secondVertex = vertex + '\x1f' + extra;
    // These pairs collided when a single separator joined the two strings.
    const auto first = GLSLContext::get ().toGlsl (vertex, firstFragment);
    const auto second = GLSLContext::get ().toGlsl (secondVertex, fragment);
    REQUIRE_FALSE (first.first.empty ());
    REQUIRE_FALSE (second.first.empty ());
    CHECK (first.first.ends_with (vertex + "\n#endif"));
    CHECK (second.first.ends_with (secondVertex + "\n#endif"));
    CHECK (second.second.ends_with (fragment + "\n#endif"));
}

TEST_CASE ("native clip discards fragments with a negative component", "[shader][clip]") {
    for (const std::string type : { "float", "vec2", "vec3", "vec4" }) {
        CAPTURE (type);
        const auto sources = compileLinked (
            "void main() { gl_Position = vec4(0.0); }",
            "uniform " + type + " value;\nvoid main() { clip(value); gl_FragColor = vec4(1.0); }", {}
        );
        const auto translated = GLSLContext::get ().toGlsl (sources.first, sources.second);
        REQUIRE_FALSE (translated.second.empty ());
        CHECK_THAT (translated.second, Catch::Matchers::ContainsSubstring ("discard"));
        CHECK_THAT (translated.second, Catch::Matchers::ContainsSubstring (
            type == "float" ? "< 0.0" : "lessThan"));
    }
}

TEST_CASE ("stock generic4 writes coverage alpha without a shader cutoff", "[.][shader][generic4-a2c]") {
    // The production compatibility rule recognizes the original fragment by its full contents.
    const auto* root = std::getenv ("LWE_TEST_STOCK_ASSETS");
    if (!root) SKIP ("Set LWE_TEST_STOCK_ASSETS to the Wallpaper Engine assets directory from Steam build 23967692");

    auto container = std::make_unique<Container> ();
    container->mount (root, "/");
    const AssetLocator assets (std::move (container));

    for (const int coverage : { 0, 1 }) {
        CAPTURE (coverage);
        const auto fragment = preprocessFragmentMain (assets, "generic4", { { "ALPHATOCOVERAGE", coverage } });
        CHECK_THAT (fragment, Catch::Matchers::ContainsSubstring ("out_FragColor=albedo;"));
        CHECK ((fragment.find (nativeCoverageRemap) != std::string::npos) == (coverage == 1));
        CHECK_THAT (fragment, !Catch::Matchers::ContainsSubstring ("if(out_FragColor.a<0.5)discard;"));
    }
}

TEST_CASE ("custom generic4 and other shaders retain authored coverage and alpha-test cutoffs", "[shader][generic4-a2c]") {
    const std::string source = R"(
uniform vec4 g_TestAlbedo;
void main() {
    vec4 albedo = g_TestAlbedo;
#if ALPHATEST
    if (albedo.a < 0.25) discard;
#endif
	gl_FragColor = albedo;
#if ALPHATOCOVERAGE
	gl_FragColor.a = saturate((gl_FragColor.a - 0.5) / max(fwidth(gl_FragColor.a), 0.0001) + 0.5);
#if GLSL
	if (gl_FragColor.a < 0.5) discard;
#endif
#endif
}
)";
    auto container = std::make_unique<Container> ();
    for (const std::string name : { "generic4", "custom-a2c" }) {
        container->getVFS ().add ("shaders/" + name + ".vert", "void main() { gl_Position = vec4(0.0); }");
        container->getVFS ().add ("shaders/" + name + ".frag", source);
    }
    const AssetLocator assets (std::move (container));

    for (const std::string name : { "generic4", "custom-a2c" }) {
        for (const int alphaTest : { 0, 1 }) {
            CAPTURE (name, alphaTest);
            const ComboMap combos { { "ALPHATOCOVERAGE", 1 }, { "ALPHATEST", alphaTest } };
            const auto fragment = preprocessFragmentMain (assets, name, combos);
            CHECK_THAT (fragment, Catch::Matchers::ContainsSubstring (nativeCoverageRemap));
            CHECK_THAT (fragment, Catch::Matchers::ContainsSubstring ("if(out_FragColor.a<0.5)discard;"));
            CHECK ((fragment.find ("if(albedo.a<0.25)discard;") != std::string::npos) == (alphaTest == 1));
        }
    }
}

TEST_CASE ("statement macros expand before numeric shader conversions", "[shader][statement-macro]") {
    for (const std::string expression : { "mask", "MASK_ALIAS", "GET_MASK()" }) {
	std::string firstVariant;
	for (const int masked : { 0, 1, 0 }) {
	    CAPTURE (expression, masked);
	    const std::string source =
		"#if MASK\nfloat mask = 0.25;\n#else\n#define mask 1.0;\n#endif\n"
		"#define MASK_ALIAS mask\n#define GET_MASK() MASK_ALIAS\n"
		"#define SET_RED() color.r = 0.2;\n"
		"#define SCALE 1.0\n#define SCALE 0.75\n"
		"void main() {\nvec4 color = vec4(0.0);\n"
		"float opacity = SCALE * " + expression + ";\n"
		"float narrowed = vec2(0.2, 0.8);\n"
		"#if MASK\nvec3 widened = mask;\n#else\nvec3 widened = vec3(1.0);\nmask\n#endif\n"
		"SET_RED()\ngl_FragColor = vec4(opacity, widened.x, color.r + narrowed, 1.0);\n}\n";
	    const auto compiled = compileLinked (
		"void main() { gl_Position = vec4(0.0); }\n", source, ComboMap { { "MASK", masked } }
	    );
	    const auto translated = GLSLContext::get ().toGlsl (compiled.first, compiled.second);
	    REQUIRE_FALSE (translated.first.empty ());
	    REQUIRE_FALSE (translated.second.empty ());
	    if (firstVariant.empty ()) firstVariant = translated.second;
	    else if (masked) CHECK (translated.second != firstVariant);
	    else CHECK (translated.second == firstVariant);
	}
    }
}

TEST_CASE ("failed macro preprocessing never caches an incomplete shader", "[shader][statement-macro-error]") {
    const auto assets = shaderAssets ("");
    const ShaderConstantMap constants;
    const TextureMap textures;
    const ComboMap combos;
    ShaderUnit unit (
	GLSLContext::UnitType_Fragment, "macro_error.frag",
	"#define TERMINATED 1.0;\n#error deliberate\nvoid main() { gl_FragColor = vec4(1.0); }\n",
	*assets, constants, textures, textures, combos, combos
    );
    CHECK_THROWS (unit.compile ());
    CHECK_THROWS (unit.compile ());
}

TEST_CASE ("shader text scans distinguish changed source at the same filename", "[shader][scan-cache]") {
    const auto assets = shaderAssets ("");
    const ShaderConstantMap constants;
    const TextureMap textures;
    const ComboMap combos;
    // Reuse both changed and unchanged source, including a negative macro scan.
    for (const bool terminated : { false, true, false, true }) {
        CAPTURE (terminated);
        const std::string source =
            "// input output and #define IGNORED 1.0; stay comments\n"
            "/*\n#define ALSO_IGNORED 1.0;\n*/\n"
            "uniform float output; // {\"material\":\"input\",\"default\":0.5}\n"
            "#define VALUE 0.25" + std::string (terminated ? ";\n" : "\n") +
            "void main() { float input = VALUE; gl_FragColor = vec4(input * output); }\n";
        ShaderUnit unit (
            GLSLContext::UnitType_Fragment, "scan_cache.frag", source, *assets, constants, textures,
            textures, combos, combos
        );
        const auto& parameters = unit.getParameters ();
        REQUIRE (parameters.size () == 1);
        CHECK (parameters[0]->getName () == "_lweReserved_output");
        CHECK (parameters[0]->getIdentifierName () == "input");
        CHECK (parameters[0]->getFloat () == 0.5f);
        const auto fragment = unit.compile ();
        if (terminated) {
            CHECK (fragment.find ("#define VALUE") == std::string::npos);
        } else {
            CHECK_THAT (fragment, Catch::Matchers::ContainsSubstring ("#define VALUE 0.25\n"));
            CHECK_THAT (fragment, Catch::Matchers::ContainsSubstring ("#define ALSO_IGNORED 1.0;"));
            CHECK_THAT (fragment, Catch::Matchers::ContainsSubstring ("\"material\":\"input\""));
        }
        const auto translated = GLSLContext::get ().toGlsl (
            "#version 330\nvoid main() { gl_Position = vec4(0.0); }\n", fragment
        );
        CHECK_FALSE (translated.second.empty ());
    }
}

TEST_CASE ("reserved identifier scans preserve tokens and quoted metadata on reuse", "[shader][scan-cache]") {
    const auto assets = shaderAssets ("");
    const ShaderConstantMap constants;
    const TextureMap textures;
    const ComboMap combos;
    for (const std::string name : { "input", "inputValue", "output", "output_2", "input" }) {
        CAPTURE (name);
        const bool reserved = name == "input" || name == "output";
        const std::string source = "uniform float " + name +
            "; // {\"material\":\"output\",\"default\":0.75}\n"
            "#if 0\n\"input \\\" output\"\n'input'\n#endif\n"
            "void main() { gl_FragColor = vec4(" + name + "); }\n";
        ShaderUnit unit (
            GLSLContext::UnitType_Fragment, "reserved_cache.frag", source, *assets, constants,
            textures, textures, combos, combos
        );
        REQUIRE (unit.getParameters ().size () == 1);
        CHECK (unit.getParameters ()[0]->getName () == (reserved ? "_lweReserved_" + name : name));
        CHECK (unit.getParameters ()[0]->getIdentifierName () == "output");
        const auto fragment = unit.compile ();
        CHECK_THAT (fragment, Catch::Matchers::ContainsSubstring ("\"input \\\" output\""));
        CHECK_THAT (fragment, Catch::Matchers::ContainsSubstring ("'input'"));
    }
}

TEST_CASE ("audio bar circle masks preserve integer and antialiased amplitudes", "[shader][audio-bar-mask]") {
    for (const int antialias : { 0, 1 }) {
	DYNAMIC_SECTION ("antialias " << antialias) {
	    const auto sources = compileLinked (
		"void main() { gl_Position = vec4(0.0); }",
		"uniform vec2 shapeCoord;\nuniform float startAngle;\nuniform float endAngle;\n"
		"void main() {\n#if ANTIALIAS\nfloat bar = 0.375;\n#else\nint bar = 1;\n#endif\n"
		// Simple Audio Bars uses a boolean semicircle mask with both numeric types.
		"bar *= shapeCoord.x > 0.0 && shapeCoord.x * sign(endAngle - startAngle) < 1.0;\n"
		"gl_FragColor = vec4(bar);\n}", ComboMap { { "ANTIALIAS", antialias } }
	    );
	    const auto translated = GLSLContext::get ().toGlsl (sources.first, sources.second);
	    REQUIRE_FALSE (translated.second.empty ());
	    CHECK_THAT (translated.second, Catch::Matchers::ContainsSubstring ("&&"));
	    if (antialias) CHECK_THAT (translated.second, Catch::Matchers::ContainsSubstring ("0.375"));
	}
    }
}

TEST_CASE ("shader parameter declarations accept aligned whitespace", "[shader][parameter][regression]") {
    const auto assets = shaderAssets ("");
    const ShaderConstantMap constants;
    const TextureMap textures;
    const ComboMap combos;
    for (const std::string declaration : { "uniform vec3 u_Color", "uniform vec3  u_Color",
					  "uniform vec3\tu_Color", "uniform\tvec3\tu_Color",
					  "\tuniform \tvec3 \t u_Color \t ", "uniform highp vec3 u_Color" }) {
	DYNAMIC_SECTION (declaration) {
	    // Misty Sea aligns its six color declarations with two spaces after vec3.
	    const std::string source
		= declaration + "; // {\"material\":\"color\",\"default\":\"0.25 0.5 0.75\"}\n"
		  "void main() { gl_FragColor = vec4(u_Color, 1.0); }\n";
	    ShaderUnit unit (
		GLSLContext::UnitType_Fragment, "aligned_parameters.frag", source, *assets, constants, textures,
		textures, combos, combos
	    );
	    const auto& parameters = unit.getParameters ();
	    REQUIRE (parameters.size () == 1);
	    CHECK (parameters[0]->getName () == "u_Color");
	    CHECK (parameters[0]->getIdentifierName () == "color");
	    CHECK (parameters[0]->getVec3 () == glm::vec3 (0.25f, 0.5f, 0.75f));
	    const auto translated = GLSLContext::get ().toGlsl (
		"#version 330\nvoid main() { gl_Position = vec4(0.0); }\n", unit.compile ()
	    );
	    CHECK_FALSE (translated.second.empty ());
	}
    }
}

TEST_CASE ("vector shader defaults accept numbers without dropping the layer", "[shader][parameter][regression]") {
    const auto assets = shaderAssets ("");
    const ShaderConstantMap constants;
    const TextureMap textures;
    const ComboMap combos;
    for (const std::string value : { "0", "0.375", "-2" }) {
	CAPTURE (value);
	// Deformer Simulation on 3761619125 declares a numeric vec2 default
	// inside an inactive option. Metadata is still read before preprocessing.
	const std::string source
	    = "#if 0\nuniform vec2 u_Pos; // {\"material\":\"pos\",\"default\":" + value + "}\n#endif\n"
	      "uniform vec3 u_Color; // {\"material\":\"color\",\"default\":" + value + "}\n"
	      "uniform vec4 u_Depth; // {\"material\":\"depth\",\"default\":" + value + "}\n"
	      "uniform vec4 u_Components; // {\"material\":\"components\",\"default\":\"0.5 1 2 3\"}\n"
	      "void main() { gl_FragColor = vec4(u_Color, 1.0) * u_Depth * u_Components; }\n";
	ShaderUnit unit (
	    GLSLContext::UnitType_Fragment, "vector_defaults.frag", source, *assets, constants, textures, textures,
	    combos, combos
	);
	const auto& parameters = unit.getParameters ();
	REQUIRE (parameters.size () == 4);
	CHECK (parameters[0]->getIdentifierName () == "pos");
	CHECK (parameters[0]->getVec2 () == glm::vec2 (std::stof (value)));
	CHECK (parameters[1]->getVec3 () == glm::vec3 (std::stof (value)));
	CHECK (parameters[2]->getVec4 () == glm::vec4 (std::stof (value)));
	CHECK (parameters[3]->getVec4 () == glm::vec4 (0.5f, 1.0f, 2.0f, 3.0f));
	const auto translated = GLSLContext::get ().toGlsl (
	    "#version 330\nvoid main() { gl_Position = vec4(0.0); }\n", unit.compile ()
	);
	CHECK_FALSE (translated.second.empty ());
    }
}

TEST_CASE ("shader vector defaults accept comma separated components", "[shader][parameter][regression]") {
    const auto assets = shaderAssets ("");
    const ShaderConstantMap constants;
    const TextureMap textures;
    const ComboMap combos;
    // Rain Drop on Glass (3755078205) uses comma-separated color defaults.
    // Metadata must load even when the effect supplies its own color values.
    ShaderUnit unit (
	GLSLContext::UnitType_Fragment, "rain_defaults.frag",
	"uniform vec2 u_Pos; // {\"material\":\"pos\",\"default\":\"0.10, 0.11\"}\n"
	"uniform vec3 u_Color; // {\"material\":\"color\",\"default\":\"0.10,0.11,0.12\"}\n"
	"uniform vec4 u_Depth; // {\"material\":\"depth\",\"default\":\"0.10 , 0.11 ,0.12, 0.13\"}\n"
	"void main() { gl_FragColor = vec4(u_Pos, u_Color.x, 1.0) * u_Depth; }\n",
	*assets, constants, textures, textures, combos, combos
    );
    const auto& parameters = unit.getParameters ();
    REQUIRE (parameters.size () == 3);
    CHECK (parameters[0]->getVec2 () == glm::vec2 (0.10f, 0.11f));
    CHECK (parameters[1]->getVec3 () == glm::vec3 (0.10f, 0.11f, 0.12f));
    CHECK (parameters[2]->getVec4 () == glm::vec4 (0.10f, 0.11f, 0.12f, 0.13f));
    const auto translated = GLSLContext::get ().toGlsl (
	"#version 330\nvoid main() { gl_Position = vec4(0.0); }\n", unit.compile ()
    );
    CHECK_FALSE (translated.second.empty ());
}

TEST_CASE ("legacy shaders can use GLSL reserved input and output identifiers", "[shader][regression]") {
    const auto fragment = compileFragment (
	"", "// input and output in comments stay intact\n"
	"uniform float output; // {\"material\":\"input\",\"default\":0.5}\n"
	"#define COLOR(input) ((input) * output)\n"
	"vec4 blend(vec4 input) { return COLOR(input); }\n"
	"void main() { vec4 input = vec4(1.0); gl_FragColor = blend(input); }\n"
    );
    CHECK (fragment.find ("// input and output in comments stay intact") != std::string::npos);
    CHECK (fragment.find ("\"material\":\"input\"") != std::string::npos);
    const auto translated = GLSLContext::get ().toGlsl (
	"#version 330\nvoid main() { gl_Position = vec4(0.0); }\n", fragment
    );
    CHECK_FALSE (translated.second.empty ());
}

TEST_CASE ("continued shader lines are joined before metadata and GLSL parsing", "[shader][continuation]") {
    for (const std::string newline : { "\n", "\r\n" }) {
	const std::string continued = "\\" + newline;
	const auto fragment = compileFragment (
	    "#define INCLUDED_VALUE (1 " + continued + "+ 2)\n",
	    "#include \"test_ordering.h\"\n"
	    "// continued comment " + continued + "#include \"does_not_exist.h\"\n"
	    "#define COMBINED to" + continued + "ken\n"
	    "#if INCLUDED_VALUE != 3\n#error broken macro continuation\n#endif\n"
	    "uniform vec2 u_Pos; // {\"material\":\"pos\", " + continued + "\"default\":\"0 0\"}\n"
	    "void main() {\nfloat token = 1.0 " + continued + "+ 2.0;\n"
	    "gl_FragColor = vec4(COMBINED + u_Pos.x);\n}\n"
	);
	CHECK (fragment.find ("#define COMBINED token") != std::string::npos);
	CHECK (fragment.find ("\\\n") == std::string::npos);
	CHECK (fragment.find ("\\\r\n") == std::string::npos);
	const auto translated = GLSLContext::get ().toGlsl (
	    "#version 330\nvoid main() { gl_Position = vec4(0.0); }\n", fragment
	);
	CHECK_FALSE (translated.second.empty ());
    }
}

TEST_CASE ("writable fragment inputs preserve conditional types across cached variants", "[shader][varying]") {
    const std::string declarations
	= "#if SHAPE == 1\nvarying vec4 v_TexCoord;\n"
	  "#elif SHAPE == 2\nvarying vec3 v_TexCoord;\n"
	  "#else\nvarying vec2 v_TexCoord;\n#endif\n"
	  "varying vec2 v_Depth;\nvarying float v_ReadOnly;\n";
    for (const int shape : { 0, 1, 2, 0 }) {
	const auto [vertex, fragment] = compileLinked (
	    declarations
		+ "void main() {\n#if SHAPE == 1\nv_TexCoord = vec4(0.5);\n"
		  "#elif SHAPE == 2\nv_TexCoord = vec3(0.5);\n#else\nv_TexCoord = vec2(0.5);\n#endif\n"
		  "v_Depth = vec2(0.25); v_ReadOnly = 1.0; gl_Position = vec4(0.0);\n}\n",
	    declarations
		+ "void main() {\nv_Depth += vec2(0.125); v_TexCoord.xy *= 0.5;\n"
		  "if (v_ReadOnly == 1.0) gl_FragColor = vec4(v_Depth, v_TexCoord.xy);\n}\n",
	    { { "SHAPE", shape } }
	);
	CHECK (fragment.find ("float v_ReadOnly = v_ReadOnly;") == std::string::npos);
	const auto translated = GLSLContext::get ().toGlsl (vertex, fragment);
	CHECK_FALSE (translated.first.empty ());
	CHECK_FALSE (translated.second.empty ());
    }
}

TEST_CASE ("authored local copies of fragment inputs are left intact", "[shader][varying]") {
    // Lens Flare Sun (Last Train, 2488626583) declares unused vec4 inputs
    // then shadows them with scalar locals. Chromatic Aberration also makes
    // its own copies; injecting another declaration would break both effects.
    const auto [vertex, fragment] = compileLinked (
	"varying vec4 timer;\nvarying vec4 rValue;\n"
	"void main() { timer = vec4(0.0); rValue = vec4(0.0); gl_Position = vec4(0.0); }\n",
	"varying vec4 timer;\nvarying vec4 rValue;\n"
	"void main() { float timer = 0.25; vec4 rValue = vec4(0.5);\n"
	"timer += 0.25; rValue.xy *= timer; gl_FragColor = rValue; }\n", {}
    );
    const auto translated = GLSLContext::get ().toGlsl (vertex, fragment);
    CHECK_FALSE (translated.first.empty ());
    CHECK_FALSE (translated.second.empty ());
}

TEST_CASE ("writable vertex inputs preserve their incoming values and conditional types", "[shader][attribute]") {
    for (const int wide : { 0, 1, 0 }) {
	const auto [vertex, fragment] = compileLinked (
	    "#if WIDE\nattribute vec4 a_TexCoord;\n#else\nattribute vec2 a_TexCoord;\n#endif\n"
	    "attribute vec3 a_Position;\nvarying vec2 v_TexCoord;\n"
	    "void main() { a_TexCoord *= 0.5; a_TexCoord += 0.25;\n"
	    "v_TexCoord = a_TexCoord.xy; gl_Position = vec4(a_Position, 1.0); }\n",
	    "varying vec2 v_TexCoord;\nvoid main() { gl_FragColor = vec4(v_TexCoord, 0.0, 1.0); }\n",
	    { { "WIDE", wide } }
	);
	const auto translated = GLSLContext::get ().toGlsl (vertex, fragment);
	CHECK_FALSE (translated.first.empty ());
	CHECK_FALSE (translated.second.empty ());
	CHECK (vertex.find ("a_Position = a_Position;") == std::string::npos);
    }
}

TEST_CASE ("authored local copies of vertex inputs are left intact", "[shader][attribute]") {
    const auto [vertex, fragment] = compileLinked (
	"attribute vec2 a_TexCoord;\nvarying vec2 v_TexCoord;\n"
	"void main() { vec2 a_TexCoord = a_TexCoord; a_TexCoord *= 0.5;\n"
	"v_TexCoord = a_TexCoord; gl_Position = vec4(0.0); }\n",
	"varying vec2 v_TexCoord;\nvoid main() { gl_FragColor = vec4(v_TexCoord, 0.0, 1.0); }\n", {}
    );
    const auto translated = GLSLContext::get ().toGlsl (vertex, fragment);
    CHECK_FALSE (translated.first.empty ());
    CHECK_FALSE (translated.second.empty ());
}

TEST_CASE ("each conditional fragment entry point gets writable inputs", "[shader][varying]") {
    for (const int mode : { 0, 1, 2 }) {
	const auto [vertex, fragment] = compileLinked (
	    "varying vec2 v_Depth;\nvoid main() { v_Depth = vec2(0.5); gl_Position = vec4(0.0); }\n",
	    "varying vec2 v_Depth;\n"
	    "#if MODE == 0\nvoid main() { float v_Depth = 0.5; gl_FragColor = vec4(v_Depth); }\n"
	    "#elif MODE == 1\nvoid main() { /* } vec2 v_Depth; */\n"
	    "// vec2 v_Depth = v_Depth;\nv_Depth += vec2(0.5); gl_FragColor = vec4(v_Depth, 0.0, 1.0); }\n"
	    "#else\nvoid main() { v_Depth *= 0.5; gl_FragColor = vec4(v_Depth, 0.0, 1.0); }\n#endif\n",
	    { { "MODE", mode } }
	);
	const auto translated = GLSLContext::get ().toGlsl (vertex, fragment);
	CHECK_FALSE (translated.first.empty ());
	CHECK_FALSE (translated.second.empty ());
    }
}

TEST_CASE ("malformed includes fail at their own line", "[shader][include][regression]") {
    for (const std::string directive : { "#include", "#include test_ordering.h", "#include \"\"",
					 "#include \"test_ordering.h", "#include \"test_ordering.h\" junk" }) {
	CAPTURE (directive);
	CHECK_THROWS_WITH (
	    compileFragment (
		"#define INCLUDED_SCALE 2.0\n", directive + "\n#include \"test_ordering.h\"\nvoid main() {}\n"
	    ),
	    Catch::Matchers::ContainsSubstring ("Malformed #include")
	);
	CHECK_THROWS_WITH (
	    compileFragment (directive, "#include \"test_ordering.h\"\nvoid main() {}\n"),
	    Catch::Matchers::ContainsSubstring ("Malformed #include")
	);
    }
}

TEST_CASE ("commented includes are ignored and spaced includes are expanded", "[shader][include][regression]") {
    const auto fragment = compileFragment (
	"#define LIVE_INCLUDE 2.0\n# include \"test_nested.h\"",
	"// #include \"missing.h\"\n/*\n#include \"missing.h\"\n*/\n"
	"# include \"test_ordering.h\" /* a comment continues\n#include \"missing.h\"\n*/\n"
	"void main() { gl_FragColor = vec4(LIVE_INCLUDE * NESTED_INCLUDE); }\n",
	"#define NESTED_INCLUDE 3.0\n"
    );
    CHECK (fragment.find ("#define LIVE_INCLUDE 2.0") != std::string::npos);
    CHECK (fragment.find ("#define NESTED_INCLUDE 3.0") != std::string::npos);
    CHECK (fragment.find ("tried including file missing.h") == std::string::npos);
    const auto translated
	= GLSLContext::get ().toGlsl ("#version 330\nvoid main() { gl_Position = vec4(0.0); }\n", fragment);
    CHECK_FALSE (translated.first.empty ());
    CHECK_FALSE (translated.second.empty ());
}

TEST_CASE ("combo editor requirements do not enable other shader options", "[shader][combo][regression]") {
    // The reference metadata reader stores only combo/default for [COMBO].
    // Deformer Simulation leaves these child settings on while SHOW_LOCATOR
    // is off; forcing the parent on replaces the image with an editor grid.
    const std::string vertexSource
	= "// [COMBO] {\"combo\":\"SHOW_LOCATOR\",\"default\":0}\n"
	  "// [COMBO] {\"combo\":\"DEFORMER_LOCATOR\",\"default\":0,\"require\":{\"SHOW_LOCATOR\":1}}\n"
	  "// [COMBO] {\"combo\":\"OVERLAY_LOCATOR\",\"default\":1,\"require\":{\"SHOW_LOCATOR\":1,\"DEFORMER_LOCATOR\":1}}\n"
	  "void main() { gl_Position = vec4(float(SHOW_LOCATOR)); }\n";
    const std::string fragmentSource
	= "// [COMBO] {\"combo\":\"LIGHTING\",\"default\":1}\n"
	  "// [COMBO] {\"combo\":\"RIMLIGHTING\",\"default\":0,\"require\":{\"LIGHTING\":1}}\n"
	  "void main() { gl_FragColor = vec4(float(SHOW_LOCATOR + LIGHTING)); }\n";
    for (const ComboMap combos : {
	     ComboMap { { "DEFORMER_LOCATOR", 1 }, { "RIMLIGHTING", 1 }, { "LIGHTING", 0 } },
	     ComboMap { { "DEFORMER_LOCATOR", 1 }, { "SHOW_LOCATOR", 0 }, { "LIGHTING", 0 } },
	     ComboMap { { "SHOW_LOCATOR", 1 }, { "RIMLIGHTING", 1 }, { "LIGHTING", 1 } },
	 }) {
	const auto [vertex, fragment] = compileLinked (vertexSource, fragmentSource, combos);
	const int locator = combos.contains ("SHOW_LOCATOR") ? combos.at ("SHOW_LOCATOR") : 0;
	for (const auto& source : { vertex, fragment }) {
	    CHECK (source.find ("#define SHOW_LOCATOR " + std::to_string (locator) + "\n") != std::string::npos);
	    CHECK (source.find ("#define LIGHTING " + std::to_string (combos.at ("LIGHTING")) + "\n")
		   != std::string::npos);
	}
	const auto translated = GLSLContext::get ().toGlsl (vertex, fragment);
	CHECK_FALSE (translated.first.empty ());
	CHECK_FALSE (translated.second.empty ());
    }
}

TEST_CASE ("invalid combo names do not corrupt shader definitions", "[shader][combo][regression]") {
    for (const std::string metadata : { "{}", "{\"combo\":null}", "{\"combo\":false}", "{\"combo\":\"\"}",
					"{\"combo\":\"1INVALID\"}", "{\"combo\":\"HAS SPACE\"}" }) {
	CAPTURE (metadata);
	const auto fragment = compileFragment (
	    "",
	    "// [COMBO] " + metadata
		+ "\n// [COMBO] {\"combo\":\"VALID_DEFAULT\"}\n"
		  "void main() { gl_FragColor = vec4(float(VALID_DEFAULT)); }\n"
	);
	CHECK (fragment.find ("#define VALID_DEFAULT 0") != std::string::npos);
	const auto translated
	    = GLSLContext::get ().toGlsl ("#version 330\nvoid main() { gl_Position = vec4(0.0); }\n", fragment);
	CHECK_FALSE (translated.first.empty ());
	CHECK_FALSE (translated.second.empty ());
    }
    const auto [vertex, fragment] = compileLinked (
	"void main() { gl_Position = vec4(float(VALID)); }\n", "void main() { gl_FragColor = vec4(float(VALID)); }\n",
	{ { "", 1 }, { "HAS SPACE", 1 }, { "1INVALID", 1 }, { "VALID", 1 } }
    );
    CHECK (vertex.find ("#define VALID 1") != std::string::npos);
    CHECK (fragment.find ("#define VALID 1") != std::string::npos);
    CHECK (fragment.find ("#define  1") == std::string::npos);
    CHECK (fragment.find ("#define HAS SPACE") == std::string::npos);
    const auto translated = GLSLContext::get ().toGlsl (vertex, fragment);
    CHECK_FALSE (translated.first.empty ());
    CHECK_FALSE (translated.second.empty ());
}

TEST_CASE (
    "include macros reach authored helpers without moving header functions before uniforms", "[shader][include]"
) {
    const std::string fragment = compileFragment (
	"#define INCLUDED_SCALE 2.0\n"
	"#include \"test_nested.h\"\n"
	"vec4 sampleSource(vec2 uv) {\n"
	"    return texSample2D(g_Texture0, uv) * INCLUDED_SCALE * NESTED_SCALE;\n"
	"}\n",
	"#include \"test_ordering.h\"\n"
	"float authoredHelper() {\n"
	"    return INCLUDED_SCALE * NESTED_SCALE;\n"
	"}\n"
	"uniform sampler2D g_Texture0;\n"
	"void main() {\n"
	"    gl_FragColor = sampleSource(vec2(0.0)) * authoredHelper();\n"
	"}\n",
	"#define NESTED_SCALE 3.0\n"
    );

    const size_t define = fragment.find ("#define INCLUDED_SCALE");
    const size_t nestedDefine = fragment.find ("#define NESTED_SCALE");
    const size_t authoredHelper = fragment.find ("float authoredHelper");
    const size_t uniform = fragment.find ("uniform sampler2D g_Texture0");
    const size_t includedHelper = fragment.find ("vec4 sampleSource");

    REQUIRE (define != std::string::npos);
    REQUIRE (nestedDefine != std::string::npos);
    REQUIRE (authoredHelper != std::string::npos);
    REQUIRE (uniform != std::string::npos);
    REQUIRE (includedHelper != std::string::npos);
    CHECK (define < authoredHelper);
    CHECK (nestedDefine < authoredHelper);
    CHECK (define < includedHelper);
    CHECK (nestedDefine < includedHelper);
    CHECK (uniform < includedHelper);

    const auto translated = GLSLContext::get ().toGlsl (
	"#version 330\n"
	"void main() {\n"
	"    gl_Position = vec4(0.0);\n"
	"}\n",
	fragment
    );
    CHECK_FALSE (translated.first.empty ());
    CHECK_FALSE (translated.second.empty ());
}

TEST_CASE ("conditional include macros keep their preprocessor scope", "[shader][include]") {
    const std::string fragment = compileFragment (
	"#if FEATURE_ENABLED\n"
	"#define CONDITIONAL_SCALE 2.0\n"
	"#endif\n"
	"vec4 sampleSource(vec2 uv) {\n"
	"    return texSample2D(g_Texture0, uv);\n"
	"}\n",
	"#include \"test_ordering.h\"\n"
	"uniform sampler2D g_Texture0;\n"
	"void main() {\n"
	"    gl_FragColor = sampleSource(vec2(0.0));\n"
	"}\n"
    );

    const size_t conditional = fragment.find ("#if FEATURE_ENABLED");
    const size_t define = fragment.find ("#define CONDITIONAL_SCALE");
    const size_t endif = fragment.find ("#endif", define);

    REQUIRE (conditional != std::string::npos);
    REQUIRE (define != std::string::npos);
    REQUIRE (endif != std::string::npos);
    CHECK (conditional < define);
    CHECK (define < endif);
}

TEST_CASE ("conditionally included headers do not contribute early macros", "[shader][include]") {
    const std::string fragment = compileFragment (
	"#define ROOT_CONDITIONAL_SCALE 2.0\n"
	"vec4 sampleSource(vec2 uv) {\n"
	"    return texSample2D(g_Texture0, uv);\n"
	"}\n",
	"#if FEATURE_ENABLED\n"
	"#include \"test_ordering.h\"\n"
	"#endif\n"
	"uniform sampler2D g_Texture0;\n"
	"void main() {\n"
	"    gl_FragColor = sampleSource(vec2(0.0));\n"
	"}\n"
    );

    const size_t rootEndif = fragment.find ("#endif");
    const size_t define = fragment.find ("#define ROOT_CONDITIONAL_SCALE");
    const size_t uniform = fragment.find ("uniform sampler2D g_Texture0");
    const size_t includedHelper = fragment.find ("vec4 sampleSource");

    REQUIRE (rootEndif != std::string::npos);
    REQUIRE (define != std::string::npos);
    REQUIRE (uniform != std::string::npos);
    REQUIRE (includedHelper != std::string::npos);
    CHECK (rootEndif < define);
    CHECK (uniform < includedHelper);
}

TEST_CASE ("numbered TexCoord inputs retain animated higher components", "[shader][texcoord-width]") {
    for (const std::string qualifier : { "", "mediump ", "highp " }) {
	for (const std::string name : { "v_TexCoord", "v_TexCoord1" }) {
	    INFO ("qualifier=" << qualifier << " name=" << name);
	    const auto sources = compileLinked (
		"uniform float g_Time;\nvarying " + qualifier + "vec4 " + name + ";\n"
		"void main() { " + name + " = vec4(.25, .5, .75, g_Time); gl_Position = vec4(0); }\n",
		"varying " + qualifier + "vec2 " + name + ";\n"
		"void main() { vec2 uv = " + name + "; gl_FragColor = vec4(" + name + ".wz, "
		"vec2(0.0) + " + name + ".zw) + vec4(uv, 0, 0); }\n", {}
	    );
	    const auto translated = GLSLContext::get ().toGlsl (sources.first, sources.second);
	    REQUIRE_FALSE (translated.second.empty ());
	    // Inspect emitted executable GLSL, excluding the appended original-source diagnostic.
	    const std::string executable = translated.second.substr (0, translated.second.find ("#if 0"));
	    CHECK_THAT (executable, Catch::Matchers::ContainsSubstring (name + ".wz"));
	    CHECK_THAT (executable, Catch::Matchers::ContainsSubstring (name + ".zw"));
	    CHECK_THAT (executable, Catch::Matchers::ContainsSubstring (name + ".xy"));
	}
    }
}

TEST_CASE ("matching conditional TexCoord widths stay synchronized", "[shader][compat]") {
    const std::string vertexSource =
	"uniform float g_Test;\n"
	"#if LIGHTMAP\n"
	"attribute vec4 a_TexCoordVec4;\n"
	"varying vec4 v_TexCoord;\n"
	"#else\n"
	"attribute vec2 a_TexCoord;\n"
	"varying vec2 v_TexCoord;\n"
	"#endif\n"
	"void main() {\n"
	"#if LIGHTMAP\n"
	"    v_TexCoord = a_TexCoordVec4;\n"
	"#else\n"
	"    v_TexCoord = a_TexCoord;\n"
	"#endif\n"
	"    gl_Position = vec4(0.0);\n"
	"}\n";
    const std::string fragmentSource =
	"uniform float g_Test;\n"
	"#if LIGHTMAP\n"
	"varying vec4 v_TexCoord;\n"
	"#else\n"
	"varying vec2 v_TexCoord;\n"
	"#endif\n"
	"void main() { out_FragColor = vec4(v_TexCoord.xy, 0.0, 1.0); }\n";
    const ComboMap combos { { "LIGHTMAP", 0 } };

    const auto [vertex, fragment] = compileLinked (vertexSource, fragmentSource, combos);
    CHECK (vertex.find ("varying vec2 v_TexCoord;") != std::string::npos);
    CHECK (fragment.find ("varying vec2 v_TexCoord;") != std::string::npos);

    const auto translated = GLSLContext::get ().toGlsl (vertex, fragment);
    REQUIRE_FALSE (translated.first.empty ());
    REQUIRE_FALSE (translated.second.empty ());
    CHECK (translated.first.find ("out vec2 v_TexCoord;") != std::string::npos);
    CHECK (translated.second.find ("in vec2 v_TexCoord;") != std::string::npos);
}

TEST_CASE ("min max preserve native vector scalar overloads", "[shader][minmax]") {
    struct NumericType {
        const char* vectorPrefix;
        const char* scalar;
    };
    for (const NumericType type : { NumericType { "vec", "0.5" }, NumericType { "ivec", "-2" },
                                    NumericType { "uvec", "2u" } }) {
        for (const int width : { 2, 3, 4 }) {
            const std::string vectorType = std::string (type.vectorPrefix) + std::to_string (width);
            const std::string scalar = type.scalar;
            CAPTURE (vectorType);
            const std::string fragment = compileFragment (
                "", "uniform " + vectorType + " value;\nvoid main() {\n"
                    + vectorType + " a = min(value, " + scalar + ");\n"
                    + vectorType + " b = max(value, " + scalar + ");\n"
                    + vectorType + " c = max(a, b);\n"
                    + vectorType + " d = min(b, a);\n"
                    + "gl_FragColor = vec4(float(c.x + d.x));\n}\n"
            );
            const auto translated = GLSLContext::get ().toGlsl (
                "#version 330\nvoid main() { gl_Position = vec4(0.0); }\n", fragment
            );
            CHECK_FALSE (translated.first.empty ());
            CHECK_FALSE (translated.second.empty ());
        }
    }
}

TEST_CASE ("min max retain constant expressions and fluid vector clamps", "[shader][minmax]") {
    const std::string fragment = compileFragment (
        "",
        "const int low = min(-3, -2);\n"
        "const int high = max(-3, -2);\n"
        "const uint ulow = min(4294967293u, 4294967294u);\n"
        "const uint uhigh = max(4294967293u, 4294967294u);\n"
        // Native built-ins retain integer precision and compile-time evaluation.
        "float signedCheck[low == -3 && high == -2 ? 1 : -1];\n"
        "float unsignedCheck[ulow == 4294967293u && uhigh == 4294967294u ? 1 : -1];\n"
        "const float mixed = max(1, 0.5);\n"
        "float mixedCheck[mixed == 1.0 ? 1 : -1];\n"
        "const vec2 constantVector = max(vec2(-2.0, 2.0), 1);\n"
        "float vectorCheck[constantVector == vec2(1.0, 2.0) ? 1 : -1];\n"
        "uniform vec2 velocity;\n"
        "void main() {\n"
        // The stock vorticity shader clamps velocity in this order.
        "vec2 clamped = min(max(velocity, -1000.0), 1000.0);\n"
        "vec2 implicit = min(max(velocity, -1000), 1000);\n"
        "gl_FragColor = vec4(clamped, implicit);\n"
        "}\n"
    );
    const auto translated = GLSLContext::get ().toGlsl (
        "#version 330\nvoid main() { gl_Position = vec4(0.0); }\n", fragment
    );
    CHECK_FALSE (translated.first.empty ());
    CHECK_FALSE (translated.second.empty ());
}

TEST_CASE ("literal-first max keeps bloom vectors and nested calls", "[shader][minmax]") {
    const std::string fragment = compileFragment (
        "", "uniform sampler2D image;\n"
        "// max(0, not a call\n/* max(0, also not a call */\n"
        "void main() {\n"
        "vec4 bloom = max(0, (texture(image, vec2(0.5)) - 1.0) / 2.0 + 1.0);\n"
        "vec4 nested = max(0, max(-1.0, bloom));\n"
        "gl_FragColor = max(1e-3, nested);\n}\n"
    );
    const auto translated = GLSLContext::get ().toGlsl (
        "#version 330\nvoid main() { gl_Position = vec4(0.0); }\n", fragment
    );
    CHECK_FALSE (translated.first.empty ());
    CHECK_FALSE (translated.second.empty ());
}

TEST_CASE ("max compatibility leaves authored macros and function ownership intact", "[shader][minmax]") {
    const std::string macro = compileFragment (
        "", "#define max(a,b) ((a) - (b))\n"
        "const int result = max(3, 2);\nfloat check[result == 1 ? 1 : -1];\n"
        "void main() { gl_FragColor = vec4(float(result)); }\n"
    );
    const auto translated = GLSLContext::get ().toGlsl (
        "#version 330\nvoid main() { gl_Position = vec4(0.0); }\n", macro
    );
    CHECK_FALSE (translated.second.empty ());
    const std::string comments = compileFragment (
        "", "// max(0, this is not code\n/* max(0, nor is this */\n"
        "float maximum(float x) { return x; }\n"
        "float max(float a, float b) { return a - b; }\n"
        "void main() { gl_FragColor = vec4(max(3.0, maximum(2.0))); }\n"
    );
    CHECK_FALSE (GLSLContext::get ().toGlsl (
        "#version 330\nvoid main() { gl_Position = vec4(0.0); }\n", comments
    ).second.empty ());
}

TEST_CASE ("conditional function signatures preserve output arguments", "[shader][out-parameter]") {
    // Auto Sway versions share a function name and argument count but place
    // writable outputs at different positions. Never cast those l-values.
    for (const int mode : { 0, 1 }) {
        const auto [vertex, fragment] = compileLinked (
            "void main() { gl_Position = vec4(0.0); }\n",
            "#if MODE\nvoid calculate(float amount, out vec2 result) { result = vec2(amount); }\n"
            "#else\nvoid calculate(out vec2 result, float amount) { result = vec2(amount); }\n#endif\n"
            "void main() { vec2 result;\n#if MODE\ncalculate(0.5, result);\n"
            "#else\ncalculate(result, 0.5);\n#endif\ngl_FragColor = vec4(result, 0.0, 1.0); }\n",
            { { "MODE", mode } }
        );
        const auto translated = GLSLContext::get ().toGlsl (vertex, fragment);
        CHECK_FALSE (translated.first.empty ());
        CHECK_FALSE (translated.second.empty ());
    }
}

TEST_CASE ("cursor ripple input and decay use elapsed time with bounded float history", "[shader][cursor-ripple]") {
    const std::string apply =
        "uniform float g_Frametime;\nvarying vec2 v_PointDelta;\nvoid main() {\n"
        "vec4 albedo = vec4(0.0);\n"
        "float timeAmt = min(1.0 / 30.0, g_Frametime) / 0.02;\n"
        "float pointerMoveAmt = v_PointDelta.x;\n"
        "vec4 colorAdd = vec4(timeAmt * pointerMoveAmt);\n"
        "gl_FragColor = albedo + colorAdd;\n}\n";
    const std::string simulate =
        "uniform float g_Frametime;\nuniform float g_RippleDecay;\nvoid main() {\n"
        "vec4 albedo = vec4(0.0);\nvec4 force = vec4(0.5);\nfloat decay = 1.5;\n"
        "float drop = max(1.001 / 255.0, decay / 255.0 * (g_Frametime / 0.02) * g_RippleDecay);\n"
        "force -= drop;\nalbedo = force;\ngl_FragColor = albedo;\n}\n";
    for (const auto& [name, source] : std::vector<std::pair<std::string, std::string>> {
        { "effects/cursorripple_apply_force", apply }, { "effects/cursorripple_simulate_force", simulate }
    }) {
        CAPTURE (name);
        auto container = std::make_unique<Container> ();
        container->getVFS ().add ("shaders/" + name + ".frag", source);
        container->getVFS ().add ("shaders/custom.frag", source);
        AssetLocator assets (std::move (container));
        const auto normalized = assets.fragmentShader (name);
        CHECK (normalized != source);
        CHECK (assets.fragmentShader ("custom") == source);
        CHECK_THAT (normalized, Catch::Matchers::ContainsSubstring ("saturate("));
        if (name.ends_with ("apply_force")) {
            CHECK_THAT (normalized, Catch::Matchers::ContainsSubstring ("30.0 * max(g_Frametime, 0.0001)"));
        } else {
            CHECK_THAT (normalized, Catch::Matchers::ContainsSubstring ("g_Frametime * 30.0"));
        }
        const auto linked = compileLinked (
            "varying vec2 v_PointDelta;\nvoid main() { v_PointDelta = vec2(1); gl_Position = vec4(0); }",
            normalized, {}
        );
        CHECK_FALSE (GLSLContext::get ().toGlsl (linked.first, linked.second).second.empty ());
    }
    // A customized shader at a stock path must retain its own equations.
    auto container = std::make_unique<Container> ();
    const std::string custom = "void main() { gl_FragColor = vec4(0.25); }";
    container->getVFS ().add ("shaders/effects/cursorripple_simulate_force.frag", custom);
    AssetLocator assets (std::move (container));
    CHECK (assets.fragmentShader ("effects/cursorripple_simulate_force") == custom);
}

TEST_CASE ("native volume samplers retain defaults and compile RGB and RGBA reads", "[shader][volume]") {
    const auto sources = compileLinked (
	"void main() { gl_Position = vec4(0.0); }",
	"uniform sampler3D g_Texture1; // {\"default\":\"lut/neutral\"}\n"
	"uniform sampler3D g_Texture2; // {\"hidden\":true}\n"
	"void main() {\n"
	"vec3 lutColor = texSample3D(g_Texture1, vec3(0.25,0.5,0.75));\n"
	"gl_FragColor = vec4(lutColor,1) + texSample3D(g_Texture2, vec3(0.5));\n}\n", {}
    );
    const auto translated = GLSLContext::get ().toGlsl (sources.first, sources.second);
    REQUIRE_FALSE (translated.second.empty ());
    CHECK_THAT (translated.second, Catch::Matchers::ContainsSubstring ("sampler3D"));
}

TEST_CASE ("CPU puppet mask alpha precedes coverage multiplication and inversion", "[shader][puppet-mask-alpha]") {
    const auto assets = shaderAssets ("");
    const ShaderConstantMap constants;
    const TextureMap textures;
    const ComboMap overrides;
    const std::string vertexSource = "attribute vec3 a_Position;\nvoid main() { gl_Position = vec4(a_Position, 1.0); }";
    const std::string fragmentSource = "void main() { vec4 color = vec4(.5);\ncolor.r *= color.a;\n"
        "color.r = 1.0 - color.r;\ngl_FragColor = color; }";
    for (const bool maskShader : { false, true }) {
        for (const int enabled : { 0, 1 }) {
            const ComboMap combos { { "PUPPET_MORPH_ALPHA", enabled } };
            const std::string name = maskShader ? "clippingmaskimage4" : "custom-mask";
            ShaderUnit vertex (GLSLContext::UnitType_Vertex, name, vertexSource, *assets,
                constants, textures, textures, combos, overrides);
            ShaderUnit fragment (GLSLContext::UnitType_Fragment, name, fragmentSource, *assets,
                constants, textures, textures, combos, overrides);
            vertex.linkToUnit (&fragment);
            fragment.linkToUnit (&vertex);
            const auto& compiledVertex = vertex.compile ();
            const auto& compiledFragment = fragment.compile ();
            const bool injected = maskShader && enabled;
            CHECK ((compiledVertex.find ("v_PuppetMaskAlpha = a_Color.x") != std::string::npos) == injected);
            CHECK ((compiledFragment.find ("color.a *= v_PuppetMaskAlpha") != std::string::npos) == injected);
            if (injected) {
                CHECK (compiledFragment.find ("color.a *= v_PuppetMaskAlpha") < compiledFragment.find ("color.r *= color.a"));
                CHECK (compiledFragment.find ("color.r *= color.a") < compiledFragment.find ("color.r = 1.0 - color.r"));
            }
            const auto translated = GLSLContext::get ().toGlsl (compiledVertex, compiledFragment);
            REQUIRE_FALSE (translated.first.empty ());
            REQUIRE_FALSE (translated.second.empty ());
        }
    }
}

TEST_CASE ("perspective screen ropes use the native segment midpoint facing", "[shader][rope-facing]") {
    const std::string source = R"(
uniform mat4 g_ModelMatrixInverse;
uniform vec3 g_OrientationForward;
uniform vec3 g_EyePosition;
attribute vec4 a_PositionVec4;
attribute vec4 a_TexCoordVec4;
#define in_ParticleTrailLength a_TexCoordVec4.w
void main() {
    vec3 startPosition = a_PositionVec4.xyz;
    vec3 endPosition = a_TexCoordVec4.xyz;
    vec3 eyeDirection = mul(g_OrientationForward, CAST3X3(g_ModelMatrixInverse));
    vec3 trailRightStart = normalize(cross(eyeDirection, endPosition - startPosition));
    gl_Position = vec4(startPosition + trailRightStart, 1.0);
}
)";
    for (const int facing : { 0, 1, 0 }) {
        CAPTURE (facing);
        const auto shaders = compileLinked (source, "void main() { gl_FragColor = vec4(1.0); }",
            { { "LWE_ROPE_SCREEN_FACING", facing } });
        const auto preprocessed = GLSLContext::get ().preprocess (GLSLContext::UnitType_Vertex, shaders.first);
        CHECK ((preprocessed.find ("eyeModel") != std::string::npos) == (facing != 0));
        const auto translated = GLSLContext::get ().toGlsl (shaders.first, shaders.second);
        REQUIRE_FALSE (translated.first.empty ());
        REQUIRE_FALSE (translated.second.empty ());
    }
}
