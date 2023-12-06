// This code is a modification of the original project that can be found at
// https://github.com/if1live/cef-gl-example

#ifndef GLCORE_HPP
#  define GLCORE_HPP

#  include <GL/glew.h>

// *****************************************************************************
//! \brief Helper functions for compiling OpenGL shaders.
// *****************************************************************************
class GLCore
{
public:

    static void checkError(const char* filename, const uint32_t line, const char* expression);
    static GLuint compileShaderFromCode(GLenum shader_type, const char *src);
    static GLuint compileShaderFromFile(GLenum shader_type, const char *filepath);
    static GLuint createShaderProgram(const char *vert, const char *frag);
    static GLuint createShaderProgram(GLuint vert, GLuint frag);
    static bool deleteShader(GLuint shader);
    static bool deleteProgram(GLuint program);
};

#  ifdef CHECK_OPENGL
#    define GLCHECK(expr) expr; GLCore::checkError(__FILE__, __LINE__, #expr);
#  else
#    define GLCHECK(expr) expr;
#  endif

#endif
