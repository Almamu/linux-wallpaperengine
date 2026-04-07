#include "WallpaperEngine/Logging/Log.h"

#include "argparse/argparse.hpp"
#include "frontends/context.h"
#include "frontends/project.h"
#include "frontends/render.h"
#include "glad/glad.h"

#include <GLFW/glfw3.h>
#include <csignal>
#include <cstring>
#include <filesystem>

std::string assetsPath;
std::string steamPath;
std::string wallpaper;
std::map<std::string, std::string> properties;
int framebufferWidth = 1280;
int framebufferHeight = 720;

void glfw_framebuffer_size_callback (GLFWwindow* window, const int width, const int height) {
	framebufferWidth = width;
	framebufferHeight = height;

	glViewport (0, 0, width, height);
}

bool keepRunning = true;

void signalhandler (const int signal) { keepRunning = false; }

void* glfw_gl_proc_address_impl (void* user_parameter, const char* proc_name) {
	return reinterpret_cast<void*> (glfwGetProcAddress (proc_name));
}

float glfw_get_time (void* user_parameter) { return glfwGetTime (); }

wp_gl_proc_address glfw_gl_proc_address = { .user_parameter = nullptr, .get_proc_address = glfw_gl_proc_address_impl };
wp_time_counter glfw_time_counter = { .user_parameter = nullptr, .get_time = glfw_get_time };

void parseArgs (const int argc, char* argv[]) {
	argparse::ArgumentParser program ("linux-wallpaperengine-dev-viewer", "0.0", argparse::default_arguments::help);

	program.add_argument ("-a", "--assets")
		.help ("Path to the WallpaperEngine's assets")
		.action ([] (const std::string& value) -> void { assetsPath = value; });

	program.add_argument ("-s", "--steam-dir")
		.help ("Path to your steam installation")
		.action ([] (const std::string& value) -> void { steamPath = value; });

	program.add_argument ("wallpaper").help ("Wallpaper to display").action ([] (const std::string& value) -> void {
		wallpaper = value;
	});

	program.add_argument ("-p", "--property")
		.help ("Sets a property of the background")
		.action ([] (const std::string& value) -> void {
			const std::string::size_type equals = value.find ('=');

			if (equals == std::string::npos) {
				properties.emplace (value, "1");
			} else {
				properties.emplace (value.substr (0, equals), value.substr (equals + 1));
			}
		});

	program.parse_known_args (argc, argv);
}

int main (const int argc, char* argv[]) {
	sLog.addOutput (new std::ostream (std::cout.rdbuf ()));
	sLog.addError (new std::ostream (std::cerr.rdbuf ()));

	std::signal (SIGINT, signalhandler);
	std::signal (SIGTERM, signalhandler);
	std::signal (SIGKILL, signalhandler);

	// parse any args
	parseArgs (argc, argv);

	// create window and start glfw
	if (glfwInit () == GLFW_FALSE) {
		sLog.exception ("Cannot initialize GLFW");
	}

	// set some window hints (opengl version to be used mainly)
	glfwWindowHint (GLFW_SAMPLES, 4);
	glfwWindowHint (GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint (GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint (GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint (GLFW_RESIZABLE, GLFW_TRUE);
	glfwWindowHint (GLFW_DECORATED, GLFW_FALSE);
	glfwWindowHint (GLFW_FLOATING, GLFW_TRUE);

	// create the window
	GLFWwindow* glfwWindow
		= glfwCreateWindow (framebufferWidth, framebufferHeight, "Wallpaper Engine", nullptr, nullptr);

	if (glfwWindow == nullptr) {
		sLog.exception ("Cannot create window");
	}

	glfwSetFramebufferSizeCallback (glfwWindow, glfw_framebuffer_size_callback);
	glfwMakeContextCurrent (glfwWindow);

	// prepare configuration and contexts
	wp_configuration* configuration = wp_config_create ();

	if (!steamPath.empty ()) {
		if (wp_config_set_steam_dir (configuration, steamPath.c_str ()) == false) {
			sLog.exception ("Cannot set Steam directory to ", steamPath);
		}
	}

	if (!assetsPath.empty ()) {
		if (wp_config_set_assets_dir (configuration, assetsPath.c_str ()) == false) {
			sLog.exception ("Cannot set assets directory to ", assetsPath);
		}
	}

	for (const auto& [key, value] : properties) {
		wp_config_set_property (configuration, key.c_str (), value.c_str ());
	}

	// same fps limit as rendering
	wp_config_set_web_fps_limit (configuration, 30);

	// config set, create context
	wp_context* context = wp_context_create (configuration);

	wp_context_set_gl_proc_address (context, &glfw_gl_proc_address);
	wp_context_set_time_counter (context, &glfw_time_counter);

	// try to load as a path, if not, try as id, otherwise show an error
	wp_project* project = wp_project_load_folder (context, nullptr, wallpaper.c_str ());

	if (project == nullptr) {
		project = wp_project_load_id (context, nullptr, strtol (wallpaper.c_str (), nullptr, 10));
	}

	if (project == nullptr) {
		sLog.exception ("Cannot load background ", wallpaper);
	}

	const int width = wp_project_get_width (project);
	const int height = wp_project_get_height (project);

	GLuint framebuffer;
	GLuint texture;
	GLuint texCoordBuffer;
	GLuint positionBuffer;
	GLuint shader;
	GLuint vaoBuffer;
	// setup vao and required buffers
	constexpr GLfloat texCoords[] = { 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f, 1.0f };
	constexpr GLfloat position[] = { -1.0f, 1.0f,  0.0f, 1.0f, 1.0f, 0.0f, -1.0f, -1.0f, 0.0f,
		                             -1.0f, -1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f,  -1.0f, 0.0f };

	glGenVertexArrays (1, &vaoBuffer);
	glBindVertexArray (vaoBuffer);

	glGenBuffers (1, &texCoordBuffer);
	glBindBuffer (GL_ARRAY_BUFFER, texCoordBuffer);
	glBufferData (GL_ARRAY_BUFFER, sizeof (texCoords), texCoords, GL_STATIC_DRAW);

	glGenBuffers (1, &positionBuffer);
	glBindBuffer (GL_ARRAY_BUFFER, positionBuffer);
	glBufferData (GL_ARRAY_BUFFER, sizeof (position), position, GL_STATIC_DRAW);
	// setup framebuffer to use as output

	glGenTextures (1, &texture);
	glGenFramebuffers (1, &framebuffer);
	glBindFramebuffer (GL_FRAMEBUFFER, framebuffer);

	glObjectLabel (GL_FRAMEBUFFER, framebuffer, -1, "Output framebuffer");
	glObjectLabel (GL_TEXTURE, texture, -1, "Output texture");

	glBindTexture (GL_TEXTURE_2D, texture);
	glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
	glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	constexpr GLenum drawBuffers[] = { GL_COLOR_ATTACHMENT0 };
	glFramebufferTexture2D (GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
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
	shader = glCreateProgram ();
	// link the shaders together
	glAttachShader (shader, vertexShaderID);
	glAttachShader (shader, fragmentShaderID);
	glLinkProgram (shader);
	// check that the shader was properly linked
	result = GL_FALSE;
	infoLogLength = 0;

	glGetProgramiv (shader, GL_LINK_STATUS, &result);
	glGetProgramiv (shader, GL_INFO_LOG_LENGTH, &infoLogLength);

	if (infoLogLength > 0) {
		const auto logBuffer = new char[infoLogLength + 1];
		// ensure logBuffer ends with a \0
		memset (logBuffer, 0, infoLogLength + 1);
		// get information about the error
		glGetProgramInfoLog (shader, infoLogLength, nullptr, logBuffer);
		// throw an exception about the issue
		const std::string message = logBuffer;
		// free the buffer
		delete[] logBuffer;
		// throw an exception
		sLog.exception (message);
	}

	// after being liked shaders can be dettached and deleted
	glDetachShader (shader, vertexShaderID);
	glDetachShader (shader, fragmentShaderID);

	glDeleteShader (vertexShaderID);
	glDeleteShader (fragmentShaderID);

	// get textures
	const GLint g_Texture0 = glGetUniformLocation (shader, "g_Texture0");
	const GLint a_Position = glGetAttribLocation (shader, "a_Position");
	const GLint a_TexCoord = glGetAttribLocation (shader, "a_TexCoord");

	wp_project_set_output_framebuffer (project, framebuffer);

	// maximum 30 fps as it's more than enough
	constexpr double minimumTime = 1.0f / 30;

	while (keepRunning) {
		const double startTime = glfwGetTime ();

		wp_render_update_time (context);
		// this renders into the framebuffer
		wp_render_frame (project);

		glViewport (0, 0, framebufferWidth, framebufferHeight);

		glBindFramebuffer (GL_FRAMEBUFFER, 0);

		glBindVertexArray (vaoBuffer);

		glDisable (GL_BLEND);
		glDisable (GL_DEPTH_TEST);
		glDisable (GL_CULL_FACE);
		// do not use any shader
		glUseProgram (shader);
		// activate scene texture
		glActiveTexture (GL_TEXTURE0);
		glBindTexture (GL_TEXTURE_2D, texture);
		// set uniforms and attribs
		glEnableVertexAttribArray (a_TexCoord);
		glBindBuffer (GL_ARRAY_BUFFER, texCoordBuffer);
		glVertexAttribPointer (a_TexCoord, 2, GL_FLOAT, GL_FALSE, 0, nullptr);

		glEnableVertexAttribArray (a_Position);
		glBindBuffer (GL_ARRAY_BUFFER, positionBuffer);
		glVertexAttribPointer (a_Position, 3, GL_FLOAT, GL_FALSE, 0, nullptr);

		glUniform1i (g_Texture0, 0);
		// write the framebuffer as is to the screen
		glBindBuffer (GL_ARRAY_BUFFER, texCoordBuffer);
		glDrawArrays (GL_TRIANGLES, 0, 6);

		// present the rendered frame
		glfwSwapBuffers (glfwWindow);
		glfwPollEvents ();

		if (glfwGetKey (glfwWindow, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
			keepRunning = false;
		}

		if (glfwWindowShouldClose (glfwWindow)) {
			keepRunning = false;
		}

		// basic fps limit, we might want to change this at some point
		if (const double diffTime = glfwGetTime () - startTime; diffTime < minimumTime) {
			usleep (static_cast<unsigned int> ((minimumTime - diffTime) * CLOCKS_PER_SEC));
		}
	}

	// remove signal handlers before exiting the app
	std::signal (SIGINT, SIG_DFL);
	std::signal (SIGTERM, SIG_DFL);
	std::signal (SIGKILL, SIG_DFL);

	// cleanup wallpapers loaded
	wp_project_destroy (project);
	wp_context_destroy (context);
	wp_config_destroy (configuration);

	// cleanup opengl resources
	glDeleteProgram (shader);
	glDeleteFramebuffers (1, &framebuffer);
	glDeleteTextures (1, &texture);
	glDeleteBuffers (1, &positionBuffer);
	glDeleteBuffers (1, &texCoordBuffer);
	glDeleteVertexArrays (1, &vaoBuffer);

	// cleanup glfw resources
	glfwDestroyWindow (glfwWindow);
	glfwTerminate ();

	return 0;
}