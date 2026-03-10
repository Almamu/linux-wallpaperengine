#include "CWallpaper.h"
#include "WallpaperEngine/Logging/Log.h"
#include "WallpaperEngine/Render/Wallpapers/CScene.h"
#include "WallpaperEngine/Render/Wallpapers/CVideo.h"
#include "WallpaperEngine/Render/Wallpapers/CWeb.h"

#include "WallpaperEngine/Data/Model/Project.h"
#include "WallpaperEngine/Data/Model/Wallpaper.h"

using namespace WallpaperEngine::Render;

CWallpaper::CWallpaper (
	const Wallpaper& wallpaperData, RenderContext& context, AudioContext& audioContext, wp_mouse_input* mouseInput
) :
	ContextAware (context), FBOProvider (nullptr), m_wallpaperData (wallpaperData), m_audioContext (audioContext),
	m_mouseInput (mouseInput) {
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
					"in vec2 v_TexCoord;\n"
					"out vec4 out_FragColor;\n"
					"void main () {\n"
					"out_FragColor = texture (g_Texture0, v_TexCoord);\n"
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
}

void CWallpaper::setDestinationFramebuffer (GLuint framebuffer) const { this->m_destFramebuffer = framebuffer; }

void CWallpaper::render () {
#if !NDEBUG
	if (GLAD_GL_VERSION_4_3) {
		glPushDebugGroup (GL_DEBUG_SOURCE_APPLICATION, 0, -1, "Rendering scene");
	}
#endif /* !NDEBUG */
	this->renderFrame ();
#if !NDEBUG
	if (GLAD_GL_VERSION_4_3) {
		glPopDebugGroup ();
	}
#endif /* !NDEBUG */
}

void CWallpaper::setPause (bool newState) { }

void CWallpaper::setupFramebuffers () {
	const uint32_t width = this->getWidth ();
	const uint32_t height = this->getHeight ();

	// create framebuffer for the scene
	this->m_sceneFBO = this->create (
		"_rt_FullFrameBuffer", TextureFormat_ARGB8888, TextureFlags_NoFlags, 1.0, { width, height }, { width, height }
	);

	this->alias ("_rt_MipMappedFrameBuffer", "_rt_FullFrameBuffer");
}

AudioContext& CWallpaper::getAudioContext () const { return this->m_audioContext; }

std::shared_ptr<const CFBO> CWallpaper::findFBO (const std::string& name) const {
	const auto fbo = this->find (name);

	if (fbo == nullptr) {
		sLog.exception ("Cannot find FBO ", name);
	}

	return fbo;
}

std::shared_ptr<const CFBO> CWallpaper::getFBO () const { return this->m_sceneFBO; }

glm::dvec2 CWallpaper::getLiveMousePosition () const {
	if (this->m_mouseInput == nullptr) {
		return { 0, 0 };
	}

	return { this->m_mouseInput->get_x (this->m_mouseInput->user_parameter),
		     this->m_mouseInput->get_y (this->m_mouseInput->user_parameter) };
}

wp_mouse_input* CWallpaper::getMouseInputHandler () const { return this->m_mouseInput; }

std::unique_ptr<CWallpaper> CWallpaper::fromWallpaper (
	const Wallpaper& wallpaper, RenderContext& context, AudioContext& audioContext, wp_mouse_input* mouseInput
) {
	if (wallpaper.is<Scene> ()) {
		return std::make_unique<WallpaperEngine::Render::Wallpapers::CScene> (
			wallpaper, context, audioContext, mouseInput
		);
	}

	if (wallpaper.is<Video> ()) {
		return std::make_unique<WallpaperEngine::Render::Wallpapers::CVideo> (wallpaper, context, audioContext);
	}

	if (wallpaper.is<Web> ()) {
		return std::make_unique<WallpaperEngine::Render::Wallpapers::CWeb> (
			wallpaper, context, audioContext, mouseInput
		);
	}

	sLog.exception ("Unsupported wallpaper type");
}
