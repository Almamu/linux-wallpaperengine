#include "Output.h"

#include "WallpaperEngine/Logging/Log.h"

#include <linux-wallpaperengine/render.h>
#include <memory.h>

using namespace WallpaperEngine::Desktop;

Output::Output (wp_project* wallpaper, const glm::vec4 viewport) :
	m_wallpaper (wallpaper), m_viewport (viewport), m_outputFramebuffer (GL_NONE), m_previousWidth (1),
	m_previousHeight (1) { }

Output::~Output () { this->clearFramebuffer (); }

void Output::render () {
	if (this->m_wallpaper == nullptr) {
		return;
	}

	const int width = wp_project_get_width (this->m_wallpaper);
	const int height = wp_project_get_height (this->m_wallpaper);

	if (width != this->m_previousWidth || height != this->m_previousHeight) {
		this->clearFramebuffer ();
		this->m_previousWidth = width;
		this->m_previousHeight = height;
		this->setupFramebuffer ();
	}

	// render to our framebuffer
	wp_render_frame (this->m_wallpaper);
	// now render to the destionation
	glViewport (this->m_viewport.x, this->m_viewport.y, this->m_viewport.z, this->m_viewport.w);

	glBindFramebuffer (GL_FRAMEBUFFER, this->m_outputFramebuffer);

	glBindVertexArray (this->m_vaoBuffer);

	glDisable (GL_BLEND);
	glDisable (GL_DEPTH_TEST);
	glDisable (GL_CULL_FACE);
	// do not use any shader
	glUseProgram (this->m_shader);
	// activate scene texture
	glActiveTexture (GL_TEXTURE0);
	glBindTexture (GL_TEXTURE_2D, this->m_texture);
	// set uniforms and attribs
	glEnableVertexAttribArray (this->m_texCoord);
	glBindBuffer (GL_ARRAY_BUFFER, this->m_texCoordBuffer);
	glVertexAttribPointer (this->m_texCoord, 2, GL_FLOAT, GL_FALSE, 0, nullptr);

	glEnableVertexAttribArray (this->m_position);
	glBindBuffer (GL_ARRAY_BUFFER, this->m_positionBuffer);
	glVertexAttribPointer (this->m_position, 3, GL_FLOAT, GL_FALSE, 0, nullptr);

	glUniform1i (this->m_texture0, 0);
	// write the framebuffer as is to the screen
	glBindBuffer (GL_ARRAY_BUFFER, this->m_texCoordBuffer);
	glDrawArrays (GL_TRIANGLES, 0, 6);
}

void Output::setWallpaper (wp_project* wallpaper) {
	this->clearFramebuffer ();

	this->m_wallpaper = wallpaper;

	if (this->m_wallpaper != nullptr) {
		this->setupFramebuffer ();
	}
}

void Output::setViewport (const glm::vec4 viewport) { this->m_viewport = viewport; }

void Output::setFramebuffer (const GLuint framebuffer) { this->m_outputFramebuffer = framebuffer; }

wp_project* Output::getWallpaper () const { return this->m_wallpaper; }

glm::vec4 Output::getViewport () const { return this->m_viewport; }

GLuint Output::getFramebuffer () const { return this->m_framebuffer; }

void Output::setupFramebuffer () {
	// TODO: CLEAN THIS UP, THIS CODE IS REPEATED IN MULTIPLE PLACES
	// setup vao and required buffers
	constexpr GLfloat texCoords[] = { 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f, 1.0f };
	constexpr GLfloat position[] = { -1.0f, 1.0f,  0.0f, 1.0f, 1.0f, 0.0f, -1.0f, -1.0f, 0.0f,
		                             -1.0f, -1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f,  -1.0f, 0.0f };

	glGenVertexArrays (1, &this->m_vaoBuffer);
	glBindVertexArray (this->m_vaoBuffer);

	glGenBuffers (1, &this->m_texCoordBuffer);
	glBindBuffer (GL_ARRAY_BUFFER, this->m_texCoordBuffer);
	glBufferData (GL_ARRAY_BUFFER, sizeof (texCoords), texCoords, GL_STATIC_DRAW);

	glGenBuffers (1, &this->m_positionBuffer);
	glBindBuffer (GL_ARRAY_BUFFER, this->m_positionBuffer);
	glBufferData (GL_ARRAY_BUFFER, sizeof (position), position, GL_STATIC_DRAW);
	// setup framebuffer to use as output

	glGenTextures (1, &this->m_texture);
	glGenFramebuffers (1, &this->m_framebuffer);
	glBindFramebuffer (GL_FRAMEBUFFER, this->m_framebuffer);

	glObjectLabel (GL_FRAMEBUFFER, this->m_framebuffer, -1, "Output framebuffer");
	glObjectLabel (GL_TEXTURE, this->m_texture, -1, "Output texture");

	glBindTexture (GL_TEXTURE_2D, this->m_texture);
	glTexImage2D (
		GL_TEXTURE_2D, 0, GL_RGBA8, this->m_previousWidth, this->m_previousHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr
	);
	glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	constexpr GLenum drawBuffers[] = { GL_COLOR_ATTACHMENT0 };
	glFramebufferTexture2D (GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, this->m_texture, 0);
	glDrawBuffers (1, drawBuffers);

	if (glCheckFramebufferStatus (GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
		sLog.exception ("Framebuffer setup failed");
	}

	glClear (GL_COLOR_BUFFER_BIT);

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
	this->m_texture0 = glGetUniformLocation (this->m_shader, "g_Texture0");
	this->m_position = glGetAttribLocation (this->m_shader, "a_Position");
	this->m_texCoord = glGetAttribLocation (this->m_shader, "a_TexCoord");

	wp_project_set_output_framebuffer (this->m_wallpaper, this->m_framebuffer);
}

void Output::clearFramebuffer () {
	// clean up opengl resources
	if (this->m_shader != GL_NONE) {
		glDeleteProgram (this->m_shader);
		this->m_shader = GL_NONE;
	}

	if (this->m_framebuffer != GL_NONE) {
		glDeleteFramebuffers (1, &this->m_framebuffer);
		this->m_framebuffer = GL_NONE;
	}

	if (this->m_texture != GL_NONE) {
		glDeleteTextures (1, &this->m_texture);
		this->m_texture = GL_NONE;
	}

	if (this->m_positionBuffer != GL_NONE) {
		glDeleteBuffers (1, &this->m_positionBuffer);
		this->m_positionBuffer = GL_NONE;
	}

	if (this->m_texCoordBuffer != GL_NONE) {
		glDeleteBuffers (1, &this->m_texCoordBuffer);
		this->m_texCoordBuffer = GL_NONE;
	}

	if (this->m_vaoBuffer != GL_NONE) {
		glDeleteVertexArrays (1, &this->m_vaoBuffer);
		this->m_vaoBuffer = GL_NONE;
	}
}