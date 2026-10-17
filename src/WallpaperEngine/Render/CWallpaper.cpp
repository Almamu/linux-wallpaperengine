#include "CWallpaper.h"
#include "WallpaperEngine/Logging/Log.h"
#include "WallpaperEngine/Render/Wallpapers/CScene.h"
#include "WallpaperEngine/Render/Wallpapers/CVideo.h"
#include "WallpaperEngine/Render/Wallpapers/CWeb.h"

#include "WallpaperEngine/Data/Model/Project.h"
#include "WallpaperEngine/Data/Model/Wallpaper.h"

using namespace WallpaperEngine::Render;

CWallpaper::CWallpaper (
    const Wallpaper& wallpaperData, RenderContext& context, AudioContext& audioContext,
    const WallpaperState::TextureUVsScaling& scalingMode, const uint32_t& clampMode, const glm::vec2& uvOffset,
    const PostProcessSettings& postProcess
) :
    ContextAware (context), FBOProvider (nullptr), m_wallpaperData (wallpaperData), m_audioContext (audioContext),
    m_state (scalingMode, clampMode, uvOffset),
    m_fillState (WallpaperState::TextureUVsScaling::ZoomFillUVs, clampMode, uvOffset),
    m_postProcess (postProcess) {
    // generate the VAO to stop opengl from complaining
    glGenVertexArrays (1, &this->m_vaoBuffer);
    glBindVertexArray (this->m_vaoBuffer);

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
    // reserve shaders in OpenGL
    const GLuint vertexShaderID = glCreateShader (GL_VERTEX_SHADER);

    // give shader's source code to OpenGL to be compiled
    const char* sourcePointer = "#version 330\n"
				"precision highp float;\n"
				"in vec3 a_Position;\n"
				"in vec2 a_TexCoord;\n"
				"out vec2 v_TexCoord;\n"
				"void main () {\n"
				"gl_Position = vec4 (a_Position, 1.0);\n"
				"v_TexCoord = a_TexCoord;\n"
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
	// throw an exception about the issue
	const std::string message = logBuffer;
	// free the buffer
	delete[] logBuffer;
	// throw an exception
	sLog.exception (message);
    }

    // reserve shaders in OpenGL
    const GLuint fragmentShaderID = glCreateShader (GL_FRAGMENT_SHADER);

    // give shader's source code to OpenGL to be compiled
    sourcePointer = "#version 330\n"
		    "precision highp float;\n"
		    "uniform sampler2D g_Texture0;\n"
		    "uniform float u_Saturation;\n"
		    "uniform float u_Contrast;\n"
		    "uniform vec3 u_BorderColour;\n"
		    "uniform int u_FitBlurEnabled;\n"
		    "uniform vec4 u_FitUVBounds;\n"
		    "uniform vec4 u_FillUVBounds;\n"
		    "uniform vec2 u_FitBlurStep;\n"
		    "in vec2 v_TexCoord;\n"
		    "out vec4 out_FragColor;\n"
		    "vec3 adjustColour (vec3 pixel) {\n"
		    "float lum = dot (pixel, vec3 (0.2126, 0.7152, 0.0722));\n"
		    "vec3 colour = mix (vec3 (lum), pixel, u_Saturation);\n"
		    "return (colour - 0.5) * u_Contrast + 0.5;\n"
		    "}\n"
		    // A small weighted 3x3 blur keeps the fitted wallpaper fill on the GPU.
		    "vec3 blurredFill (vec2 uv) {\n"
		    "vec2 d = u_FitBlurStep;\n"
		    "vec3 colour = texture (g_Texture0, uv).rgb * 4.0;\n"
		    "colour += (texture (g_Texture0, uv + vec2 (d.x, 0.0)).rgb +\n"
		    "texture (g_Texture0, uv - vec2 (d.x, 0.0)).rgb +\n"
		    "texture (g_Texture0, uv + vec2 (0.0, d.y)).rgb +\n"
		    "texture (g_Texture0, uv - vec2 (0.0, d.y)).rgb) * 2.0;\n"
		    "colour += texture (g_Texture0, uv + d).rgb +\n"
		    "texture (g_Texture0, uv - d).rgb +\n"
		    "texture (g_Texture0, uv + vec2 (d.x, -d.y)).rgb +\n"
		    "texture (g_Texture0, uv + vec2 (-d.x, d.y)).rgb;\n"
		    "return colour / 16.0;\n"
		    "}\n"
		    "void main () {\n"
		    "if (u_FitBlurEnabled != 0) {\n"
		    "vec2 fitStart = u_FitUVBounds.xy;\n"
		    "vec2 fitEnd = u_FitUVBounds.zw;\n"
		    // Fit UVs span the output; coordinates outside the texture are the empty bars.
		    "if (any (lessThan (v_TexCoord, vec2 (0.0))) || any (greaterThan (v_TexCoord, vec2 (1.0)))) {\n"
		    "vec2 screenUV = (v_TexCoord - fitStart) / (fitEnd - fitStart);\n"
		    "vec2 fillUV = mix (u_FillUVBounds.xy, u_FillUVBounds.zw, screenUV);\n"
		    "out_FragColor = vec4 (adjustColour (blurredFill (fillUV)), 1.0);\n"
		    "return;\n"
		    "}\n"
		    "}\n"
		    "vec4 tex = texture (g_Texture0, v_TexCoord);\n"
		    "if (tex.a < 0.01) { out_FragColor = vec4 (u_BorderColour, 1.0); return; }\n"
		    "out_FragColor = vec4 (adjustColour (tex.rgb), tex.a);\n"
		    "}";

    glShaderSource (fragmentShaderID, 1, &sourcePointer, nullptr);
    glCompileShader (fragmentShaderID);

    result = GL_FALSE;
    infoLogLength = 0;

    // ensure the vertex shader was correctly compiled
    glGetShaderiv (fragmentShaderID, GL_COMPILE_STATUS, &result);
    glGetShaderiv (fragmentShaderID, GL_INFO_LOG_LENGTH, &infoLogLength);

    if (infoLogLength > 0) {
	const auto logBuffer = new char[infoLogLength + 1];
	// ensure logBuffer ends with a \0
	memset (logBuffer, 0, infoLogLength + 1);
	// get information about the error
	glGetShaderInfoLog (fragmentShaderID, infoLogLength, nullptr, logBuffer);
	// throw an exception about the issue
	const std::string message = logBuffer;
	// free the buffer
	delete[] logBuffer;
	// throw an exception
	sLog.exception (message);
    }

    // create the final program
    this->m_shader = glCreateProgram ();
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
	// throw an exception about the issue
	const std::string message = logBuffer;
	// free the buffer
	delete[] logBuffer;
	// throw an exception
	sLog.exception (message);
    }

    // after being liked shaders can be dettached and deleted
    glDetachShader (this->m_shader, vertexShaderID);
    glDetachShader (this->m_shader, fragmentShaderID);

    glDeleteShader (vertexShaderID);
    glDeleteShader (fragmentShaderID);

    // get textures
    this->g_Texture0 = glGetUniformLocation (this->m_shader, "g_Texture0");
    this->a_Position = glGetAttribLocation (this->m_shader, "a_Position");
    this->a_TexCoord = glGetAttribLocation (this->m_shader, "a_TexCoord");
    this->u_FitBlurEnabled = glGetUniformLocation (this->m_shader, "u_FitBlurEnabled");
    this->u_FitUVBounds = glGetUniformLocation (this->m_shader, "u_FitUVBounds");
    this->u_FillUVBounds = glGetUniformLocation (this->m_shader, "u_FillUVBounds");
    this->u_FitBlurStep = glGetUniformLocation (this->m_shader, "u_FitBlurStep");
    glUseProgram (this->m_shader);
    glUniform1f (glGetUniformLocation (this->m_shader, "u_Saturation"), this->m_postProcess.saturation);
    glUniform1f (glGetUniformLocation (this->m_shader, "u_Contrast"), this->m_postProcess.contrast);
    glUniform3f (
	glGetUniformLocation (this->m_shader, "u_BorderColour"), this->m_postProcess.borderColour.r,
	this->m_postProcess.borderColour.g, this->m_postProcess.borderColour.b
    );
}

void CWallpaper::setDestinationFramebuffer (GLuint framebuffer) { this->m_destFramebuffer = framebuffer; }

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

void CWallpaper::render (
    const glm::ivec4& viewport, const bool vflip, const glm::ivec2& globalPosition, const glm::ivec2& logicalSize
) {
    // Get current frame counter from the driver to avoid redundant scene renders
    const uint32_t currentFrame = this->getContext ().getDriver ().getFrameCounter ();
    const bool needsSceneRender = (currentFrame != this->m_lastRenderedFrame);
    const glm::ivec4 sceneViewport = this->m_spanInfo.has_value ()
	? glm::ivec4 { 0, 0, this->m_spanInfo->totalBounds.z, this->m_spanInfo->totalBounds.w }
	: viewport;

#if !NDEBUG
    glPushDebugGroup (GL_DEBUG_SOURCE_APPLICATION, 0, -1, "Rendering scene");
#endif /* !NDEBUG */
    if (needsSceneRender) {
	this->renderFrame (sceneViewport);
	this->m_lastRenderedFrame = currentFrame;
    }
#if !NDEBUG
    glPopDebugGroup ();
    glPushDebugGroup (GL_DEBUG_SOURCE_APPLICATION, 0, -1, "Rendering scene to output");
#endif /* !NDEBUG */

	float ustart, uend, vstart, vend;
    float fillUstart = 0.0f, fillUend = 1.0f, fillVstart = 0.0f, fillVend = 1.0f;
    const bool blurredFit =
	this->m_state.getTextureUVsScaling () == WallpaperState::TextureUVsScaling::ZoomFitUVs;

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
	auto [baseUstart, baseUend, baseVstart, baseVend] = this->m_state.getTextureUVs ();
	float baseFillUstart = 0.0f, baseFillUend = 1.0f;
	float baseFillVstart = 0.0f, baseFillVend = 1.0f;
	if (blurredFit) {
	    this->m_fillState.updateState (span.totalBounds, vflip, this->getWidth (), this->getHeight ());
	    auto fillUVs = this->m_fillState.getTextureUVs ();
	    baseFillUstart = fillUVs.ustart;
	    baseFillUend = fillUVs.uend;
	    baseFillVstart = fillUVs.vstart;
	    baseFillVend = fillUVs.vend;
	}

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
	fillUstart = baseFillUstart + relLeft * (baseFillUend - baseFillUstart);
	fillUend = baseFillUstart + relRight * (baseFillUend - baseFillUstart);
	fillVstart = baseFillVstart + relTop * (baseFillVend - baseFillVstart);
	fillVend = baseFillVstart + relBottom * (baseFillVend - baseFillVstart);

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
	auto uvs = this->m_state.getTextureUVs ();
	ustart = uvs.ustart;
	uend = uvs.uend;
	vstart = uvs.vstart;
	vend = uvs.vend;
	if (blurredFit) {
	    this->m_fillState.updateState (viewport, vflip, this->getWidth (), this->getHeight ());
	    auto fillUVs = this->m_fillState.getTextureUVs ();
	    fillUstart = fillUVs.ustart;
	    fillUend = fillUVs.uend;
	    fillVstart = fillUVs.vstart;
	    fillVend = fillUVs.vend;
	}
    }

    const GLfloat texCoords[] = {
	ustart, vstart, uend, vstart, ustart, vend, ustart, vend, uend, vstart, uend, vend,
    };

    glViewport (viewport.x, viewport.y, viewport.z, viewport.w);

    glBindFramebuffer (GL_FRAMEBUFFER, this->m_destFramebuffer);

    glBindVertexArray (this->m_vaoBuffer);

    glDisable (GL_BLEND);
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
    glUniform1i (this->u_FitBlurEnabled, blurredFit ? 1 : 0);
    if (blurredFit) {
	glUniform4f (this->u_FitUVBounds, ustart, vstart, uend, vend);
	glUniform4f (this->u_FillUVBounds, fillUstart, fillVstart, fillUend, fillVend);
	// Keep the blur radius constant in output pixels at every monitor resolution.
	const float blurRadius = 18.0f;
	const float viewportWidth = static_cast<float> (viewport.z > 0 ? viewport.z : 1);
	const float viewportHeight = static_cast<float> (viewport.w > 0 ? viewport.w : 1);
	glUniform2f (
	    this->u_FitBlurStep,
	    (fillUend - fillUstart) * blurRadius / viewportWidth,
	    (fillVend - fillVstart) * blurRadius / viewportHeight
	);
    }
    // write the framebuffer as is to the screen
    glBindBuffer (GL_ARRAY_BUFFER, this->m_texCoordBuffer);
    glDrawArrays (GL_TRIANGLES, 0, 6);

#if !NDEBUG
    glPopDebugGroup ();
#endif /* !NDEBUG */
}

void CWallpaper::setPause (bool newState) { }

void CWallpaper::setupFramebuffers () {
    const uint32_t width = this->getWidth ();
    const uint32_t height = this->getHeight ();
    const uint32_t clamp = this->m_state.getClampingMode ();

    // create framebuffer for the scene
    this->m_sceneFBO = this->create (
	"_rt_FullFrameBuffer", TextureFormat_ARGB8888, clamp, 1.0, { width, height }, { width, height }
    );

    this->alias ("_rt_MipMappedFrameBuffer", "_rt_FullFrameBuffer");
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

std::unique_ptr<CWallpaper> CWallpaper::fromWallpaper (
    const Wallpaper& wallpaper, RenderContext& context, AudioContext& audioContext,
    WebBrowser::WebBrowserContext* browserContext, const WallpaperState::TextureUVsScaling& scalingMode,
    const uint32_t& clampMode, const glm::vec2& uvOffset, const PostProcessSettings& postProcess
) {
    if (wallpaper.is<Scene> ()) {
	return std::make_unique<WallpaperEngine::Render::Wallpapers::CScene> (
	    wallpaper, context, audioContext, scalingMode, clampMode, uvOffset, postProcess
	);
    }

    if (wallpaper.is<Video> ()) {
	return std::make_unique<WallpaperEngine::Render::Wallpapers::CVideo> (
	    wallpaper, context, audioContext, scalingMode, clampMode, uvOffset, postProcess
	);
    }

    if (wallpaper.is<Web> ()) {
	return std::make_unique<WallpaperEngine::Render::Wallpapers::CWeb> (
	    wallpaper, context, audioContext, *browserContext, scalingMode, clampMode, uvOffset, postProcess
	);
    }

    sLog.exception ("Unsupported wallpaper type");
}
