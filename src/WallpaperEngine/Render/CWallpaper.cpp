#include "CWallpaper.h"
#include "WallpaperEngine/Logging/Log.h"
#include "WallpaperEngine/Render/Wallpapers/CScene.h"
#include "WallpaperEngine/Render/Wallpapers/CVideo.h"
#include "WallpaperEngine/Render/Wallpapers/CWeb.h"

#include "WallpaperEngine/Data/Model/Project.h"
#include "WallpaperEngine/Data/Model/Wallpaper.h"

#include <algorithm>

using namespace WallpaperEngine::Render;

CWallpaper::CWallpaper (
    const Wallpaper& wallpaperData, RenderContext& context, AudioContext& audioContext,
    const WallpaperState::TextureUVsScaling& scalingMode, const uint32_t& clampMode
) :
    ContextAware (context), FBOProvider (nullptr), m_wallpaperData (wallpaperData), m_audioContext (audioContext),
    m_state (scalingMode, clampMode),
    m_contrast (std::clamp (context.getApp ().getContext ().settings.render.contrast, 0.0f, 4.0f)),
    m_saturation (std::clamp (context.getApp ().getContext ().settings.render.saturation, 0.0f, 4.0f)) {
    // NOTE: m_vaoBuffer is created lazily in render(): VAOs are not shared between GL
    // contexts, so it cannot be created here when building on the async switch worker

    this->setupShaders ();

    constexpr GLfloat texCoords[] = { 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f, 1.0f };

    // inverted positions so the final texture is rendered properly
    constexpr GLfloat position[] = { -1.0f, 1.0f,  0.0f, 1.0,  1.0f, 0.0f, -1.0f, -1.0f, 0.0f,
				     -1.0f, -1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f,  -1.0f, 0.0f };

    glGenBuffers (1, &this->m_texCoordBuffer);
    glBindBuffer (GL_ARRAY_BUFFER, this->m_texCoordBuffer);
    glBufferData (GL_ARRAY_BUFFER, sizeof (texCoords), texCoords, GL_STATIC_DRAW);

    glGenBuffers (1, &this->m_positionBuffer);
    glBindBuffer (GL_ARRAY_BUFFER, this->m_positionBuffer);
    glBufferData (GL_ARRAY_BUFFER, sizeof (position), position, GL_STATIC_DRAW);
}

CWallpaper::~CWallpaper () {
    // destroy shader programs
    GLuint attachedShaders[2];
    GLsizei attachedCount = 0;

    // destroy shaders (we only attach 2 to each program)
    glGetAttachedShaders (this->m_shader, 2, &attachedCount, attachedShaders);

    for (auto i = 0; i < attachedCount; i++) {
	glDeleteShader (attachedShaders[i]);
    }

    glDeleteProgram (this->m_shader);

    // destroy used buffers
    glDeleteBuffers (1, &this->m_texCoordBuffer);
    glDeleteBuffers (1, &this->m_positionBuffer);
    glDeleteVertexArrays (1, &this->m_vaoBuffer);
}

const AssetLocator& CWallpaper::getAssetLocator () const { return *this->m_wallpaperData.project.assetLocator; }

const Wallpaper& CWallpaper::getWallpaperData () const { return this->m_wallpaperData; }

GLuint CWallpaper::getWallpaperFramebuffer () const { return this->m_sceneFBO->getFramebuffer (); }

GLuint CWallpaper::getWallpaperTexture () const { return this->m_sceneFBO->getTextureID (0); }

void CWallpaper::setupShaders () {
    // This runs during construction, so failures must release their GL objects here.
    struct ShaderResources {
	GLuint vertex = 0;
	GLuint fragment = 0;
	GLuint program = 0;
	~ShaderResources () {
	    if (program != 0) glDeleteProgram (program);
	    if (fragment != 0) glDeleteShader (fragment);
	    if (vertex != 0) glDeleteShader (vertex);
	}
    } resources;

    // reserve shaders in OpenGL
    const GLuint vertexShaderID = glCreateShader (GL_VERTEX_SHADER);
    resources.vertex = vertexShaderID;

    // give shader's source code to OpenGL to be compiled
    const char* sourcePointer = "#version 330\n"
				"precision highp float;\n"
				"in vec3 a_Position;\n"
				"in vec2 a_TexCoord;\n"
				"out vec2 v_TexCoord;\n"
				"out vec2 v_ScreenPos;\n"
				"void main () {\n"
				"gl_Position = vec4 (a_Position, 1.0);\n"
				"v_TexCoord = a_TexCoord;\n"
				// normalized 0..1 position across the output, used by transitions
				"v_ScreenPos = a_Position.xy * 0.5 + 0.5;\n"
				"}";

    glShaderSource (vertexShaderID, 1, &sourcePointer, nullptr);
    glCompileShader (vertexShaderID);

    GLint result = GL_FALSE;
    int infoLogLength = 0;

    // ensure the vertex shader was correctly compiled
    glGetShaderiv (vertexShaderID, GL_COMPILE_STATUS, &result);
    glGetShaderiv (vertexShaderID, GL_INFO_LOG_LENGTH, &infoLogLength);

    if (infoLogLength > 0) {
	const auto logBuffer = new char[infoLogLength + 1];
	// ensure logBuffer ends with a \0
	memset (logBuffer, 0, infoLogLength + 1);
	// get information about the error
	glGetShaderInfoLog (vertexShaderID, infoLogLength, nullptr, logBuffer);
	const std::string message = logBuffer;
	// free the buffer
	delete[] logBuffer;
	if (result != GL_TRUE) sLog.exception ("Wallpaper vertex shader compilation failed: ", message);
	if (!message.empty ()) sLog.error (message);
    }
    if (result != GL_TRUE) sLog.exception ("Wallpaper vertex shader compilation failed without a driver log");

    // reserve shaders in OpenGL
    const GLuint fragmentShaderID = glCreateShader (GL_FRAGMENT_SHADER);
    resources.fragment = fragmentShaderID;

    // give shader's source code to OpenGL to be compiled
    // transitions compute a per-pixel reveal alpha; blending is only enabled while
    // a wallpaper switch is animating, so the alpha is ignored the rest of the time
    // every transition except fade reveals with a hard step() edge - no feathering
    sourcePointer = "#version 330\n"
		    "precision highp float;\n"
		    "uniform sampler2D g_Texture0;\n"
		    "uniform int g_TransitionMode;\n"
		    "uniform float g_TransitionProgress;\n"
		    "uniform vec2 g_TransitionCenter;\n"
		    "uniform float g_SceneFadeAlpha;\n"
		    "uniform float g_Contrast;\n"
		    "uniform float g_Saturation;\n"
		    "uniform vec4 g_ColorCorrection;\n"
                    "uniform float g_HDRSourceScale;\n"
                    "uniform bool g_ScRGBPresentation;\n"
                    "uniform float g_HDRHeadroom;\n"
		    "uniform sampler3D g_ColorLut;\n"
		    "uniform float g_ColorLutStrength;\n"
		    "uniform float g_Fps;\n"
		    "uniform vec3 g_FpsOutput;\n"
		    "uniform float g_LoadTimes[7];\n"
		    "uniform int g_LoadDetails;\n"
		    "in vec2 v_TexCoord;\n"
		    "in vec2 v_ScreenPos;\n"
		    "out vec4 out_FragColor;\n"
		    "vec3 sRGBToLinear (vec3 c) { return mix (c / 12.92, pow (max ((c + 0.055) / 1.055, 0.0), vec3 (2.4)), step (vec3 (0.04045), c)); }\n"
		    "float hash21 (vec2 v) {\n"
		    "return fract (sin (dot (v, vec2 (12.9898, 78.233))) * 43758.5453);\n"
		    "}\n"
		    "vec2 hash22 (vec2 v) {\n"
		    "return fract (sin (vec2 (dot (v, vec2 (127.1, 311.7)), dot (v, vec2 (269.5, 183.3)))) * 43758.5453);\n"
		    "}\n"
		    // smooth value noise, used for the organic reveal shapes
		    "float vnoise (vec2 v) {\n"
		    "vec2 i = floor (v);\n"
		    "vec2 u = fract (v);\n"
		    "u = u * u * (3.0 - 2.0 * u);\n"
		    "return mix (mix (hash21 (i), hash21 (i + vec2 (1.0, 0.0)), u.x),\n"
		    "mix (hash21 (i + vec2 (0.0, 1.0)), hash21 (i + vec2 (1.0, 1.0)), u.x), u.y);\n"
		    "}\n"
		    "float transitionAlpha () {\n"
		    "float p = g_TransitionProgress;\n"
		    "if (g_TransitionMode == 1) return p;\n" // fade: uniform crossfade
		    "if (g_TransitionMode == 2) return step (v_ScreenPos.x, p);\n"
		    "if (g_TransitionMode == 3) return step (1.0 - v_ScreenPos.x, p);\n"
		    "if (g_TransitionMode == 4) return step (v_ScreenPos.y, p);\n"
		    "if (g_TransitionMode == 5) return step (1.0 - v_ScreenPos.y, p);\n"
		    "if (g_TransitionMode == 6) {\n" // disc growing from a per-switch origin
		    // farthest corner from the origin determines the radius that covers the screen
		    "float maxRadius = length (max (g_TransitionCenter, vec2 (1.0) - g_TransitionCenter));\n"
		    "return step (distance (v_ScreenPos, g_TransitionCenter), p * maxRadius);\n"
		    "}\n"
		    "if (g_TransitionMode == 7) {\n" // vertical stripes sweeping in alternating directions
		    "float stripe = floor (v_ScreenPos.x * 12.0);\n"
		    "float y = mod (stripe, 2.0) < 1.0 ? v_ScreenPos.y : 1.0 - v_ScreenPos.y;\n"
		    "return step (y, p);\n"
		    "}\n"
		    "if (g_TransitionMode == 8) {\n" // pixelate: grid cells dissolve in pseudo-random order
		    "vec2 cell = floor (v_ScreenPos * vec2 (48.0, 27.0));\n"
		    "float order = fract (sin (dot (cell, vec2 (12.9898, 78.233))) * 43758.5453);\n"
		    "return step (order, p);\n"
		    "}\n"
		    "if (g_TransitionMode == 9) {\n" // honeycomb: hexagonal cells pop in, ordered by a hash
		    "vec2 uv = v_ScreenPos * vec2 (16.0, 9.0);\n"
		    "vec2 r = vec2 (1.0, 1.7320508);\n"
		    "vec2 a = mod (uv, r) - r * 0.5;\n"
		    "vec2 b = mod (uv - r * 0.5, r) - r * 0.5;\n"
		    "vec2 offset = dot (a, a) < dot (b, b) ? a : b;\n"
		    "vec2 id = uv - offset;\n"
		    "float order = fract (sin (dot (id, vec2 (12.9898, 78.233))) * 43758.5453);\n"
		    // cells grow from their center: inner distance delays the reveal slightly
		    "float inner = length (offset) * 0.35;\n"
		    "return step (order + inner, p * 1.35);\n"
		    "}\n"
		    "if (g_TransitionMode == 10) return step ((v_ScreenPos.x + v_ScreenPos.y) * 0.5, p);\n" // diagonal wipe
		    "if (g_TransitionMode == 11) {\n" // clock: radial sweep around the center
		    "vec2 d = v_ScreenPos - vec2 (0.5);\n"
		    "return step (atan (d.x, -d.y) / 6.28318530718 + 0.5, p);\n"
		    "}\n"
		    "if (g_TransitionMode == 12) {\n" // iris: diamond growing from a per-switch origin
		    "vec2 d = abs (v_ScreenPos - g_TransitionCenter);\n"
		    "float maxDist = max (g_TransitionCenter.x, 1.0 - g_TransitionCenter.x)\n"
		    "+ max (g_TransitionCenter.y, 1.0 - g_TransitionCenter.y);\n"
		    "return step (d.x + d.y, p * maxDist);\n"
		    "}\n"
		    "if (g_TransitionMode == 13) {\n" // checkerboard: alternating cells wipe in two waves
		    "vec2 cell = floor (v_ScreenPos * vec2 (8.0, 4.0));\n"
		    "float parity = mod (cell.x + cell.y, 2.0);\n"
		    "return step ((fract (v_ScreenPos.x * 8.0) + parity) * 0.5, p);\n"
		    "}\n"
		    "if (g_TransitionMode == 14) return step (fract (v_ScreenPos.y * 12.0), p);\n" // horizontal blinds
		    "if (g_TransitionMode == 15) return step (abs (v_ScreenPos.x - 0.5) * 2.0, p);\n" // split from center outward
		    "if (g_TransitionMode == 16) {\n" // voronoi: irregular shards pop in random order
		    "vec2 uv = v_ScreenPos * vec2 (12.0, 6.75);\n"
		    "vec2 base = floor (uv);\n"
		    "float best = 1e9;\n"
		    "vec2 bestId = vec2 (0.0);\n"
		    "for (int yo = -1; yo <= 1; yo++) {\n"
		    "for (int xo = -1; xo <= 1; xo++) {\n"
		    "vec2 cell = base + vec2 (float (xo), float (yo));\n"
		    "vec2 delta = uv - cell - hash22 (cell);\n"
		    "float dist = dot (delta, delta);\n"
		    "if (dist < best) { best = dist; bestId = cell; }\n"
		    "}\n"
		    "}\n"
		    "return step (hash21 (bestId), p);\n"
		    "}\n"
		    "if (g_TransitionMode == 17) {\n" // noise: organic blobs dissolve in, perlin style
		    "float n = vnoise (v_ScreenPos * vec2 (10.0, 5.6)) * 0.6\n"
		    "+ vnoise (v_ScreenPos * vec2 (23.0, 13.0)) * 0.3\n"
		    "+ vnoise (v_ScreenPos * vec2 (47.0, 26.0)) * 0.1;\n"
		    // stretch the midtone-heavy fbm range so the dissolve spans the full animation
		    "return step (clamp ((n - 0.15) * 1.4, 0.0, 0.995), p);\n"
		    "}\n"
		    "if (g_TransitionMode == 18) {\n" // dots: polka dots grow along a diagonal curtain
		    "float sweep = (v_ScreenPos.x + v_ScreenPos.y) * 0.5;\n"
		    "float radius = clamp ((p * 1.6 - sweep) / 0.6, 0.0, 1.0) * 0.75;\n"
		    "return step (distance (fract (v_ScreenPos * vec2 (20.0, 11.25)), vec2 (0.5)), radius);\n"
		    "}\n"
		    "if (g_TransitionMode == 19) {\n" // ink splash: wobbly-edged blob from a per-switch origin
		    "float maxRadius = length (max (g_TransitionCenter, vec2 (1.0) - g_TransitionCenter));\n"
		    "float wobble = vnoise (v_ScreenPos * 9.0) - 0.5;\n"
		    "return step (distance (v_ScreenPos, g_TransitionCenter) * (1.0 + wobble * 0.7), p * maxRadius * 1.4);\n"
		    "}\n"
		    "return 1.0;\n"
		    "}\n"
		    "// The HUD is part of the existing final composite: no extra window, texture or draw pass.\n"
		    // 3x5 bitmaps: digits, F, P, S (as 5), space, '.', then L A D R J T E X U B I C W and an S unlike 5
		    "const int GLYPHS[29] = int[29] (31599, 29850, 29671, 31207, 18925, 31183, 31695, 18727, 31727, 31215,\n"
		    "    4815, 4843, 31183, 0, 8192, 29257, 23530, 15211, 23275, 11044, 9367, 29391, 23213, 31597, 15083,\n"
		    "    29847, 25166, 24429, 14478);\n"
		    "void fpsOverlay () {\n"
		    "    if (g_Fps < 0.0) return;\n"
		    "    vec2 pixel = vec2 (v_ScreenPos.x, 1.0 - v_ScreenPos.y) * g_FpsOutput.xy / g_FpsOutput.z;\n"
		    "    vec2 panel = pixel - vec2 (12.0, 52.0);\n"
		    "    if (any (lessThan (panel, vec2 (0.0))) || any (greaterThanEqual (panel, vec2 (136.0, 31.0)))) return;\n"
		    "    out_FragColor = vec4 (out_FragColor.rgb * 0.2, 1.0);\n"
		    "    ivec2 p = ivec2 (floor ((panel - vec2 (8.0)) / 3.0));\n"
		    "    if (any (lessThan (p, ivec2 (0))) || p.y >= 5 || p.x >= 40 || p.x % 4 == 3) return;\n"
		    "    int value = int (clamp (g_Fps, 0.0, 9999.9) * 10.0 + 0.5);\n"
		    "    int chars[10] = int[10] (10, 11, 12, 13,\n"
		    "        value >= 10000 ? value / 10000 : 13,\n"
		    "        value >= 1000 ? value / 1000 % 10 : 13,\n"
		    "        value >= 100 ? value / 100 % 10 : 13,\n"
		    "        value / 10 % 10, 14, value % 10);\n"
		    "    if ((GLYPHS[chars[p.x / 4]] & (1 << (p.y * 3 + p.x % 4))) != 0)\n"
		    "        out_FragColor = vec4 (g_ScRGBPresentation ? sRGBToLinear (vec3 (0.65, 1.0, 0.65)) : vec3 (0.65, 1.0, 0.65), 1.0);\n"
		    "}\n"
		    // Beside the FPS panel: "LOAD  1.23 S", plus one row per phase with details on.
		    "void loadOverlay () {\n"
		    "    if (g_LoadTimes[0] < 0.0) return;\n"
		    "    int rows = g_LoadDetails != 0 ? 7 : 1;\n"
		    "    vec2 pixel = vec2 (v_ScreenPos.x, 1.0 - v_ScreenPos.y) * g_FpsOutput.xy / g_FpsOutput.z;\n"
		    "    vec2 panel = pixel - vec2 (156.0, 52.0);\n"
		    "    if (any (lessThan (panel, vec2 (0.0))) || any (greaterThanEqual (panel, vec2 (169.0, 13.0 + 18.0 * float (rows))))) return;\n"
		    "    out_FragColor = vec4 (out_FragColor.rgb * 0.2, 1.0);\n"
		    "    ivec2 p = ivec2 (floor ((panel - vec2 (8.0)) / 3.0));\n"
		    "    if (any (lessThan (p, ivec2 (0))) || p.x >= 52 || p.x % 4 == 3 || p.y % 6 == 5 || p.y / 6 >= rows) return;\n"
		    // LOAD, PROJ, TEX, UPLD, BUILD, PACE, WAIT; the letter O reuses the zero glyph
		    "    const int labels[35] = int[35] (15, 0, 16, 17, 13, 11, 18, 0, 19, 13, 20, 21, 22, 13, 13,\n"
		    "        23, 11, 15, 17, 13, 24, 23, 25, 15, 17, 11, 16, 26, 21, 13, 27, 16, 25, 20, 13);\n"
		    "    int row = p.y / 6, column = p.x / 4;\n"
		    "    int value = int (clamp (g_LoadTimes[row], 0.0, 99.99) * 100.0 + 0.5);\n"
		    "    int c = column < 5 ? labels[row * 5 + column] : column == 5 ? 13\n"
		    "        : column == 6 ? (value >= 1000 ? value / 1000 : 13) : column == 7 ? value / 100 % 10\n"
		    "        : column == 8 ? 14 : column == 9 ? value / 10 % 10 : column == 10 ? value % 10 : column == 11 ? 13 : 28;\n"
		    "    if ((GLYPHS[c] & (1 << (p.y % 6 * 3 + p.x % 4))) != 0)\n"
		    "        out_FragColor = vec4 (g_ScRGBPresentation ? sRGBToLinear (vec3 (0.65, 1.0, 0.65)) : vec3 (0.65, 1.0, 0.65), 1.0);\n"
		    "}\n"
		    "vec3 nativeColorCorrection (vec3 color, vec4 params) {\n"
		    "    if (params == vec4 (1.0, 1.0, 1.0, 0.0)) return color;\n"
		    "    color = mix (vec3 (0.5), color, params.y);\n"
		    "    float value = max (color.r, max (color.g, color.b));\n"
		    "    float chroma = value - min (color.r, min (color.g, color.b));\n"
		    "    float hue = 0.0;\n"
		    "    if (chroma > 0.0) {\n"
		    "        float denominator = 6.0 * chroma + 1e-10;\n"
		    "        if (value == color.r) hue = (color.g - color.b) / denominator + (color.g < color.b ? 1.0 : 0.0);\n"
		    "        else if (value == color.g) hue = (color.b - color.r) / denominator + 1.0 / 3.0;\n"
		    "        else hue = (color.r - color.g) / denominator + 2.0 / 3.0;\n"
		    "    }\n"
		    "    float saturation = chroma / (value + 1e-10) * params.z;\n"
		    "    vec3 phase = fract (vec3 (hue + params.w) + vec3 (1.0, 2.0 / 3.0, 1.0 / 3.0));\n"
		    "    vec3 pureColor = clamp (abs (phase * 6.0 - vec3 (3.0)) - vec3 (1.0), vec3 (0.0), vec3 (1.0));\n"
		    "    return value * params.x * mix (vec3 (1.0), pureColor, saturation);\n"
		    "}\n"
		    "void main () {\n"
		    "vec3 sceneColor = texture (g_Texture0, v_TexCoord).rgb;\n"
                    "if (g_HDRSourceScale > 0.0) sceneColor *= g_HDRSourceScale;\n"
		    "sceneColor = nativeColorCorrection (sceneColor, g_ColorCorrection);\n"
		    "if (g_ColorLutStrength > 0.0) sceneColor = mix (sceneColor, texture (g_ColorLut, sceneColor).rgb, g_ColorLutStrength);\n"
		    // materials/util/fade.json uses the fade shader's default tint (0.315,
		    // 0.135, 0.1125), multiplied by 0.7 in fade.frag. Scheme-color overrides
		    // can replace this once Linux-side desktop scheme integration exists.
                    "if (g_HDRSourceScale > 0.0) sceneColor = clamp (sceneColor, vec3 (0.0), vec3 (2.0 * g_HDRHeadroom));\n"
		    "vec3 fadeColor = vec3 (0.2205, 0.0945, 0.07875);\n"
		    "sceneColor = mix (sceneColor, fadeColor, g_SceneFadeAlpha);\n"
		    // Perform output grading in the universal scene/video/web composite.
		    // Rec.709 luminance keeps saturation=0 perceptually useful; both defaults
		    // are exact no-ops, so existing output remains unchanged.
		    "float luminance = dot (sceneColor, vec3 (0.2126, 0.7152, 0.0722));\n"
		    "sceneColor = mix (vec3 (luminance), sceneColor, g_Saturation);\n"
		    "sceneColor = (sceneColor - vec3 (0.5)) * g_Contrast + vec3 (0.5);\n"
                    // Convert each wallpaper before transition blending. Ordinary SDR
                    // bytes retain their existing path when the destination is SDR.
                    "if (g_ScRGBPresentation && g_HDRSourceScale == 0.0) {\n"
                    "    sceneColor = sRGBToLinear (sceneColor);\n"
                    "} else if (!g_ScRGBPresentation && g_HDRSourceScale > 0.0) {\n"
                    // Secondary 8-bit captures cannot transport HDR light. This is
                    // the standard scRGB -> sRGB encoding, not a Windows tone map.
                    "    sceneColor = mix (sceneColor * 12.92, 1.055 * pow (max (sceneColor, 0.0), vec3 (1.0 / 2.4)) - 0.055, step (vec3 (0.0031308), sceneColor));\n"
                    "}\n"
		    "out_FragColor = vec4 (sceneColor, transitionAlpha ());\n"
		    "fpsOverlay ();\n"
		    "loadOverlay ();\n"
		    "}";

    glShaderSource (fragmentShaderID, 1, &sourcePointer, nullptr);
    glCompileShader (fragmentShaderID);

    result = GL_FALSE;
    infoLogLength = 0;

    // ensure the fragment shader was correctly compiled
    glGetShaderiv (fragmentShaderID, GL_COMPILE_STATUS, &result);
    glGetShaderiv (fragmentShaderID, GL_INFO_LOG_LENGTH, &infoLogLength);

    if (infoLogLength > 0) {
	const auto logBuffer = new char[infoLogLength + 1];
	// ensure logBuffer ends with a \0
	memset (logBuffer, 0, infoLogLength + 1);
	// get information about the error
	glGetShaderInfoLog (fragmentShaderID, infoLogLength, nullptr, logBuffer);
	const std::string message = logBuffer;
	// free the buffer
	delete[] logBuffer;
	if (result != GL_TRUE) sLog.exception ("Wallpaper fragment shader compilation failed: ", message);
	if (!message.empty ()) sLog.error (message);
    }
    if (result != GL_TRUE) sLog.exception ("Wallpaper fragment shader compilation failed without a driver log");

    // create the final program
    this->m_shader = glCreateProgram ();
    resources.program = this->m_shader;
    // link the shaders together
    glAttachShader (this->m_shader, vertexShaderID);
    glAttachShader (this->m_shader, fragmentShaderID);
    glLinkProgram (this->m_shader);
    // check that the shader was properly linked
    result = GL_FALSE;
    infoLogLength = 0;

    glGetProgramiv (this->m_shader, GL_LINK_STATUS, &result);
    glGetProgramiv (this->m_shader, GL_INFO_LOG_LENGTH, &infoLogLength);

    if (infoLogLength > 0) {
	const auto logBuffer = new char[infoLogLength + 1];
	// ensure logBuffer ends with a \0
	memset (logBuffer, 0, infoLogLength + 1);
	// get information about the error
	glGetProgramInfoLog (this->m_shader, infoLogLength, nullptr, logBuffer);
	const std::string message = logBuffer;
	// free the buffer
	delete[] logBuffer;
	if (result != GL_TRUE) sLog.exception ("Wallpaper shader program linking failed: ", message);
	if (!message.empty ()) sLog.error (message);
    }
    if (result != GL_TRUE) sLog.exception ("Wallpaper shader program linking failed without a driver log");

    // After linking, detach the shaders; the scoped resources delete them on return.
    glDetachShader (this->m_shader, vertexShaderID);
    glDetachShader (this->m_shader, fragmentShaderID);

    // get textures
    this->g_Texture0 = glGetUniformLocation (this->m_shader, "g_Texture0");
    this->g_TransitionMode = glGetUniformLocation (this->m_shader, "g_TransitionMode");
    this->g_TransitionProgress = glGetUniformLocation (this->m_shader, "g_TransitionProgress");
    this->g_TransitionCenter = glGetUniformLocation (this->m_shader, "g_TransitionCenter");
    this->g_SceneFadeAlpha = glGetUniformLocation (this->m_shader, "g_SceneFadeAlpha");
    this->g_Contrast = glGetUniformLocation (this->m_shader, "g_Contrast");
    this->g_Saturation = glGetUniformLocation (this->m_shader, "g_Saturation");
    this->g_ColorCorrection = glGetUniformLocation (this->m_shader, "g_ColorCorrection");
    this->g_HDRSourceScale = glGetUniformLocation (this->m_shader, "g_HDRSourceScale");
    this->g_ScRGBPresentation = glGetUniformLocation (this->m_shader, "g_ScRGBPresentation");
    this->g_HDRHeadroom = glGetUniformLocation (this->m_shader, "g_HDRHeadroom");
    this->g_ColorLut = glGetUniformLocation (this->m_shader, "g_ColorLut");
    this->g_ColorLutStrength = glGetUniformLocation (this->m_shader, "g_ColorLutStrength");
    this->g_Fps = glGetUniformLocation (this->m_shader, "g_Fps");
    this->g_FpsOutput = glGetUniformLocation (this->m_shader, "g_FpsOutput");
    this->g_LoadTimes = glGetUniformLocation (this->m_shader, "g_LoadTimes");
    this->g_LoadDetails = glGetUniformLocation (this->m_shader, "g_LoadDetails");
    this->a_Position = glGetAttribLocation (this->m_shader, "a_Position");
    this->a_TexCoord = glGetAttribLocation (this->m_shader, "a_TexCoord");
    resources.program = 0;
}

void CWallpaper::setDestinationFramebuffer (GLuint framebuffer) { this->m_destFramebuffer = framebuffer; }

void CWallpaper::setTransition (const TransitionMode mode, const float progress, const glm::vec2 center) {
    this->m_transitionMode = mode;
    this->m_transitionProgress = progress;
    this->m_transitionCenter = center;
}

void CWallpaper::setSpanInfo (const SpanInfo& spanInfo) { this->m_spanInfo = spanInfo; }

const CWallpaper::SpanInfo* CWallpaper::getSpanInfo () const {
    return this->m_spanInfo.has_value () ? &this->m_spanInfo.value () : nullptr;
}

void CWallpaper::updateUVs (const glm::ivec4& viewport, const bool vflip) {
    // update UVs if something has changed, otherwise use old values
    if (this->m_state.hasChanged (viewport, vflip, this->getWidth (), this->getHeight ())) {
	// Update wallpaper state
	this->m_state.updateState (viewport, vflip, this->getWidth (), this->getHeight ());
    }
}

void CWallpaper::renderWithColorSpace (
    const glm::ivec4& viewport, const bool vflip, const glm::ivec2& globalPosition,
    const glm::ivec2& logicalSize, const bool scRGBPresentation, const float hdrHeadroom
) {
    // Presentation belongs to this draw. Secondary SDR captures use render()
    // directly, and must never inherit the primary output's encoding.
    struct RestorePresentation {
        bool& encoding;
        float& headroom;
        const bool oldEncoding;
        const float oldHeadroom;
        ~RestorePresentation () { encoding = oldEncoding; headroom = oldHeadroom; }
    } restore { this->m_scRGBPresentation, this->m_hdrHeadroom,
        this->m_scRGBPresentation, this->m_hdrHeadroom };
    this->m_scRGBPresentation = scRGBPresentation;
    this->m_hdrHeadroom = hdrHeadroom;
    this->render (viewport, vflip, globalPosition, logicalSize);
}

void CWallpaper::render (
    const glm::ivec4& viewport, const bool vflip, const glm::ivec2& globalPosition, const glm::ivec2& logicalSize,
    const glm::ivec2& presentationSize
) {
    // Get current frame counter from the driver to avoid redundant scene renders
    const uint32_t currentFrame = this->getContext ().getDriver ().getFrameCounter ();
    // Both sides of a transition advance normally; shared/span scenes still update
    // only once per driver frame.
    const bool needsSceneRender = !this->m_firstFramePrepared
	&& (this->m_lastRenderedFrame == UINT32_MAX || currentFrame != this->m_lastRenderedFrame);
    const glm::ivec4 sceneViewport = this->m_spanInfo.has_value ()
	? glm::ivec4 { 0, 0, this->m_spanInfo->totalBounds.z, this->m_spanInfo->totalBounds.w }
	: viewport;
    // Input and camera framing must agree from the first frame after a resize.
    this->updateUVs (sceneViewport, vflip);

#if !NDEBUG
    glPushDebugGroup (GL_DEBUG_SOURCE_APPLICATION, 0, -1, "Rendering scene");
#endif /* !NDEBUG */
    // A prepared scene's first draw is already complete, including feedback/history.
    // Present it once rather than advancing that state in an extra hidden frame.
    if (this->m_firstFramePrepared) {
	this->m_firstFramePrepared = false;
	this->m_lastRenderedFrame = currentFrame;
    }
    if (needsSceneRender) {
	this->renderFrame (sceneViewport);
	this->m_lastRenderedFrame = currentFrame;
    }
#if !NDEBUG
    glPopDebugGroup ();
    glPushDebugGroup (GL_DEBUG_SOURCE_APPLICATION, 0, -1, "Rendering scene to output");
#endif /* !NDEBUG */

    float ustart, uend, vstart, vend;

    if (this->m_spanInfo.has_value ()) {
	// Span mode: treat bounding box as virtual viewport, scale wallpaper using
	// the normal scaling rules (fill/fit/stretch/default), then slice per monitor.
	const auto& span = this->m_spanInfo.value ();
	const float spanW = static_cast<float> (span.totalBounds.z);
	const float spanH = static_cast<float> (span.totalBounds.w);
	const float spanX = static_cast<float> (span.totalBounds.x);
	const float spanY = static_cast<float> (span.totalBounds.y);

	// Compute base UVs for the wallpaper scaled to the bounding box
	this->updateUVs (span.totalBounds, vflip);
	auto [baseUstart, baseUend, baseVstart, baseVend] = this->getPresentationUVs ();

	// This viewport's relative position within the bounding box [0..1]
	// Use logicalSize (same coordinate space as globalPosition and totalBounds)
	const float relLeft = (static_cast<float> (globalPosition.x) - spanX) / spanW;
	const float relRight = (static_cast<float> (globalPosition.x + logicalSize.x) - spanX) / spanW;
	const float relTop = (static_cast<float> (globalPosition.y) - spanY) / spanH;
	const float relBottom = (static_cast<float> (globalPosition.y + logicalSize.y) - spanY) / spanH;

	// Interpolate within the base UVs to get this viewport's slice
	const float baseURange = baseUend - baseUstart;
	const float baseVRange = baseVend - baseVstart;

	ustart = baseUstart + relLeft * baseURange;
	uend = baseUstart + relRight * baseURange;
	vstart = baseVstart + relTop * baseVRange;
	vend = baseVstart + relBottom * baseVRange;

	// Log span debug info only on first few frames
	if (this->m_lastRenderedFrame < 5) {
	    sLog.debug (
		"SPAN DEBUG: viewport=", viewport.z, "x", viewport.w, " globalPos=(", globalPosition.x, ",",
		globalPosition.y, ")", " span=(", span.totalBounds.x, ",", span.totalBounds.y, ",", span.totalBounds.z,
		",", span.totalBounds.w, ")", " rel=[", relLeft, ",", relRight, "]x[", relTop, ",", relBottom, "]",
		" baseUV=[", baseUstart, ",", baseUend, "]x[", baseVstart, ",", baseVend, "]", " finalUV=[", ustart,
		",", uend, "]x[", vstart, ",", vend, "]"
	    );
	}
    } else {
	// Normal mode: compute UVs based on viewport dimensions and wallpaper resolution
	updateUVs (viewport, vflip);
	auto uvs = this->getPresentationUVs ();
	ustart = uvs.ustart;
	uend = uvs.uend;
	vstart = uvs.vstart;
	vend = uvs.vend;
    }

    const GLfloat texCoords[] = {
	ustart, vstart, uend, vstart, ustart, vend, ustart, vend, uend, vstart, uend, vend,
    };

    // Offscreen consumers retain the displayed crop/camera while reducing only
    // the final presentation pass, after the scene has rendered at its usual size.
    if (presentationSize.x > 0 && presentationSize.y > 0)
	glViewport (0, 0, presentationSize.x, presentationSize.y);
    else
	glViewport (viewport.x, viewport.y, viewport.z, viewport.w);

    glBindFramebuffer (GL_FRAMEBUFFER, this->m_destFramebuffer);

    // created lazily on the render thread: VAOs are not shared between GL contexts,
    // so an async-built wallpaper cannot create it on the worker's context
    if (this->m_vaoBuffer == GL_NONE) {
	glGenVertexArrays (1, &this->m_vaoBuffer);
    }

    glBindVertexArray (this->m_vaoBuffer);

    // while a switch is animating, the composite shader emits a per-pixel reveal alpha
    // that blends this wallpaper over the outgoing one drawn just before it
    const bool transitionActive
	= this->m_transitionMode != TransitionMode_None && this->m_transitionProgress < 1.0f;

    if (transitionActive) {
	glEnable (GL_BLEND);
	glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    } else {
	glDisable (GL_BLEND);
    }
    glDisable (GL_DEPTH_TEST);
    glDisable (GL_CULL_FACE);
    // do not use any shader
    glUseProgram (this->m_shader);
    // activate scene texture
    glActiveTexture (GL_TEXTURE0);
    glBindTexture (GL_TEXTURE_2D, this->getWallpaperTexture ());
    // set uniforms and attribs
    glEnableVertexAttribArray (this->a_TexCoord);
    glBindBuffer (GL_ARRAY_BUFFER, this->m_texCoordBuffer);
    glBufferData (GL_ARRAY_BUFFER, sizeof (texCoords), texCoords, GL_STATIC_DRAW);
    glVertexAttribPointer (this->a_TexCoord, 2, GL_FLOAT, GL_FALSE, 0, nullptr);

    glEnableVertexAttribArray (this->a_Position);
    glBindBuffer (GL_ARRAY_BUFFER, this->m_positionBuffer);
    glVertexAttribPointer (this->a_Position, 3, GL_FLOAT, GL_FALSE, 0, nullptr);

    glUniform1i (this->g_Texture0, 0);
    glUniform1i (this->g_TransitionMode, transitionActive ? this->m_transitionMode : TransitionMode_None);
    glUniform1f (this->g_TransitionProgress, this->m_transitionProgress);
    glUniform2f (this->g_TransitionCenter, this->m_transitionCenter.x, this->m_transitionCenter.y);
    glUniform1f (this->g_SceneFadeAlpha, this->getSceneFadeAlpha ());
    glUniform1f (this->g_Contrast, this->m_contrast);
    glUniform1f (this->g_Saturation, this->m_saturation);
    const auto correction = this->getColorCorrection ();
    glUniform4f (this->g_ColorCorrection, correction.x, correction.y, correction.z, correction.w);
    glUniform1f (this->g_HDRSourceScale, this->getHDRSourceScale ());
    glUniform1i (this->g_ScRGBPresentation, this->m_scRGBPresentation);
    glUniform1f (this->g_HDRHeadroom, this->m_hdrHeadroom);
    const auto [lut, strength] = this->getColorLut ();
    glActiveTexture (GL_TEXTURE1);
    glBindTexture (GL_TEXTURE_3D, lut);
    glActiveTexture (GL_TEXTURE0);
    glUniform1i (this->g_ColorLut, 1);
    glUniform1f (this->g_ColorLutStrength, strength);
    glUniform1f (this->g_Fps, this->m_fps);
    const float outputScale = logicalSize.x > 0 ? static_cast<float> (viewport.z) / logicalSize.x : 1.0f;
    glUniform3f (this->g_FpsOutput, viewport.z, viewport.w, outputScale);
    glUniform1fv (this->g_LoadTimes, static_cast<GLsizei> (this->m_loadTimes.size ()), this->m_loadTimes.data ());
    glUniform1i (this->g_LoadDetails, this->m_loadDetails);
    // write the framebuffer as is to the screen
    glBindBuffer (GL_ARRAY_BUFFER, this->m_texCoordBuffer);
    glDrawArrays (GL_TRIANGLES, 0, 6);

#if !NDEBUG
    glPopDebugGroup ();
#endif /* !NDEBUG */
}

void CWallpaper::setPause (bool newState) { }

float CWallpaper::getSceneFadeAlpha () const { return 0.0f; }

glm::vec4 CWallpaper::getColorCorrection () const { return { 1.0f, 1.0f, 1.0f, 0.0f }; }

std::pair<GLuint, float> CWallpaper::getColorLut () const { return {}; }

void CWallpaper::setupFramebuffers (bool sceneDepthBuffer, glm::vec2 size, uint32_t samples, TextureFormat format, bool reverseDepth) {
    this->setBackbufferFormat (format);
    if (size.x <= 0 || size.y <= 0) {
	size = { this->getWidth (), this->getHeight () };
    }
    const uint32_t clamp = this->m_state.getClampingMode ();

    // create framebuffer for the scene
    this->m_sceneFBO = this->create (
	"_rt_FullFrameBuffer", format, clamp, 1.0, size, size,
	sceneDepthBuffer, false, samples, reverseDepth
    );

}

AudioContext& CWallpaper::getAudioContext () const { return this->m_audioContext; }

const WallpaperState& CWallpaper::getState () const { return this->m_state; }

std::shared_ptr<const CFBO> CWallpaper::findFBO (const std::string& name) const {
    const auto fbo = this->find (name);

    if (fbo == nullptr) {
	sLog.exception ("Cannot find FBO ", name);
    }

    return fbo;
}

std::shared_ptr<const CFBO> CWallpaper::getFBO () const { return this->m_sceneFBO; }

glm::ivec2 CWallpaper::getFramebufferSize () const {
    return calculateTargetSize ({ this->getWidth (), this->getHeight () }, this->getRenderScale ());
}

WallpaperState::TextureUVs CWallpaper::getPresentationUVs () const { return this->m_state.getTextureUVs (); }

std::unique_ptr<CWallpaper> CWallpaper::fromWallpaper (
    const Wallpaper& wallpaper, RenderContext& context, AudioContext& audioContext,
    WebBrowser::WebBrowserContext* browserContext, const WallpaperState::TextureUVsScaling& scalingMode,
    const uint32_t& clampMode, const std::string& screenName
) {
    if (wallpaper.is<Scene> ()) {
	return std::make_unique<WallpaperEngine::Render::Wallpapers::CScene> (
	    wallpaper, context, audioContext, scalingMode, clampMode, screenName
	);
    }

    if (wallpaper.is<Video> ()) {
	return std::make_unique<WallpaperEngine::Render::Wallpapers::CVideo> (
	    wallpaper, context, audioContext, scalingMode, clampMode
	);
    }

    if (wallpaper.is<Web> ()) {
	return std::make_unique<WallpaperEngine::Render::Wallpapers::CWeb> (
	    wallpaper, context, audioContext, *browserContext, scalingMode, clampMode
	);
    }

    sLog.exception ("Unsupported wallpaper type");
}
