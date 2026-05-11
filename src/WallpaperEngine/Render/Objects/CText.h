#pragma once

#include "WallpaperEngine/Data/Model/Object.h"
#include "WallpaperEngine/Render/CObject.h"

#include <GL/glew.h>
#include <glm/glm.hpp>
#include <string>
#include <vector>

namespace WallpaperEngine::Render::Objects {
class CText final : public CObject {
public:
    CText (Wallpapers::CScene& scene, const Text& text);
    ~CText () override;

    void render () override;

    [[nodiscard]] const Text& getText () const;

private:
    struct Vertex {
	float x;
	float y;
	float z;
	float u;
	float v;
    };

    struct ResolvedTransform {
	glm::vec3 origin;
	glm::vec3 scale;
	float angle;
    };

    void setup ();
    void updateBuffer ();
    GLuint compileShader (GLenum type, const char* source);
    bool updateTexture (const std::string& text, float pointSize, const std::string& alignment);
    std::vector<unsigned char> loadFontData ();
    [[nodiscard]] std::string resolveRenderText () const;
    void adjustOriginForKnownLayout (glm::vec3& origin) const;
    void fitScaleToMaxWidth (glm::vec3& scale) const;
    void uploadTextQuad (const glm::vec3& origin, const glm::vec3& scale, float angle, const std::string& alignment);
    static std::vector<std::string> splitLines (const std::string& text);
    [[nodiscard]] ResolvedTransform resolveTransform (const WallpaperEngine::Data::Model::Object& object, int depth = 0) const;

    const Text& m_text;
    GLuint m_program = 0;
    GLuint m_vao = 0;
    GLuint m_vbo = 0;
    GLuint m_texture = 0;
    GLint m_positionLocation = -1;
    GLint m_texCoordLocation = -1;
    GLint m_mvpLocation = -1;
    GLint m_colorLocation = -1;
    GLint m_textureLocation = -1;
    std::vector<Vertex> m_vertices = {};
    std::string m_lastText;
    std::string m_lastAlignment;
    std::string m_lastFont;
    std::string m_cachedFontKey;
    std::vector<unsigned char> m_cachedFontData = {};
    float m_lastPointSize = 0.0f;
    bool m_textureReady = false;
    glm::vec3 m_lastScale = {};
    glm::vec3 m_lastOrigin = {};
    glm::vec3 m_lastAngles = {};
    int m_textureWidth = 0;
    int m_textureHeight = 0;
};
} // namespace WallpaperEngine::Render::Objects
