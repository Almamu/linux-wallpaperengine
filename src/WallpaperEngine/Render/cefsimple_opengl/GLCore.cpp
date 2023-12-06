// This code is a modification of the original project that can be found at
// https://github.com/if1live/cef-gl-example

#include "GLCore.hpp"
#include <fstream>
#include <iostream>

void GLCore::checkError(const char* filename, const uint32_t line, const char* expression)
{
    GLenum id;
    const char* error;

    while ((id = glGetError()) != GL_NO_ERROR)
    {
        switch (id)
        {
        case GL_INVALID_OPERATION:
            error = "GL_INVALID_OPERATION";
            break;
        case GL_INVALID_ENUM:
            error = "GL_INVALID_ENUM";
            break;
        case GL_INVALID_VALUE:
            error = "GL_INVALID_VALUE";
            break;
        case GL_OUT_OF_MEMORY:
            error = "GL_OUT_OF_MEMORY";
            break;
        case GL_INVALID_FRAMEBUFFER_OPERATION:
            error = "GL_INVALID_FRAMEBUFFER_OPERATION";
            break;
        default:
            error = "UNKNOWN";
            break;
        }

        // Do not use directly LOG macros because it will catch this
        // filename and its line instead of the faulty file/line which
        // produced the OpenGL error.
        std::cerr << "GLERR: " << filename << " " << line << ": Failed executing "
                  << expression << ". Reason was " << error << std::endl;
    }
}

GLuint GLCore::compileShaderFromCode(GLenum shader_type, const char *src)
{
    GLuint shader = glCreateShader(shader_type);
    glShaderSource(shader, 1, &src, NULL);
    glCompileShader(shader);

    GLint status;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if (status == GL_TRUE) {
        return shader;
    }

    // shader compile fail!
    fprintf(stderr, "SHADER COMPILE ERROR\n");

    GLint info_len = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &info_len);
    if (info_len > 1) {
        char *info_log = (char*)malloc(sizeof(char) * info_len);
        glGetShaderInfoLog(shader, info_len, NULL, info_log);
        fprintf(stderr, "Error compiling shader: \n%s\n", info_log);
        free(info_log);
    }
    glDeleteShader(shader);
    return 0;
}

GLuint GLCore::compileShaderFromFile(GLenum shader_type, const char *filepath)
{
    std::ifstream ifs(filepath);
    std::string shader_str((std::istreambuf_iterator<char>(ifs)),
                           (std::istreambuf_iterator<char>()));

    const char *src = shader_str.data();
    return compileShaderFromCode(shader_type, src);
}

bool GLCore::deleteShader(GLuint shader)
{
    glDeleteShader(shader);
    return true;
}

GLuint GLCore::createShaderProgram(const char *vert, const char *frag)
{
    GLuint vertShader = compileShaderFromFile(GL_VERTEX_SHADER, vert);
    GLuint fragShader = compileShaderFromFile(GL_FRAGMENT_SHADER, frag);
    if (vertShader == 0 || fragShader == 0) {
        return 0;
    }
    return createShaderProgram(vertShader, fragShader);
}

GLuint GLCore::createShaderProgram(GLuint vert, GLuint frag)
{
    GLuint program = glCreateProgram();

    glAttachShader(program, vert);
    glAttachShader(program, frag);
    glLinkProgram(program);
    glDetachShader(program, vert);
    glDetachShader(program, frag);

    GLint linked;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (linked) {
        return program;
    }

    // fail...
    GLint info_len = 0;
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &info_len);
    if (info_len > 1) {
        char *info_log = (char*)malloc(sizeof(char) * info_len);
        glGetProgramInfoLog(program, info_len, NULL, info_log);
        fprintf(stderr, "Error linking program: \n%s\n", info_log);
        free(info_log);
    }

    glDeleteProgram(program);
    return 0;
}

bool GLCore::deleteProgram(GLuint program)
{
    glDeleteProgram(program);
    return true;
}
