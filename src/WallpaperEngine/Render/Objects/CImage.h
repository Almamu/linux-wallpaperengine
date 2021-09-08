#pragma once

#include "WallpaperEngine/Core/Objects/CImage.h"

#include "WallpaperEngine/Render/Objects/Effects/CMaterial.h"
#include "WallpaperEngine/Render/Objects/CEffect.h"
#include "WallpaperEngine/Render/CObject.h"
#include "WallpaperEngine/Render/CScene.h"

#include "WallpaperEngine/Render/Shaders/Compiler.h"

#include "WallpaperEngine/Assets/CTexture.h"

#include <glm/vec3.hpp>

using namespace WallpaperEngine;
using namespace WallpaperEngine::Assets;

namespace WallpaperEngine::Render::Objects::Effects
{
    class CMaterial;
}

namespace WallpaperEngine::Render::Objects
{
    class CEffect;

    class CImage : public CObject
    {
    public:
        CImage (CScene* scene, Core::Objects::CImage* image);

        void render () override;

        const Core::Objects::CImage* getImage () const;
        const std::vector<CEffect*>& getEffects () const;

        const GLfloat* getVertex () const;
        const GLuint* getVertexBuffer () const;
        const GLuint* getPassVertexBuffer () const;
        const GLuint* getTexCoordBuffer () const;
        const GLuint* getPassTexCoordBuffer () const;
        const CTexture* getTexture () const;

    protected:
        static const std::string Type;

    private:
        CTexture* m_texture;
        GLfloat m_vertexList [6 * 3];
        GLfloat m_passesVertexList [6 * 3];
        GLfloat m_texCoordList [6 * 2];
        GLfloat m_passTexCoordList [6 * 2];
        GLuint m_vertexBuffer;
        GLuint m_passesVertexBuffer;
        GLuint m_texCoordBuffer;
        GLuint m_passTexCoordBuffer;
        uint16_t m_vertexIndices [6];


        Core::Objects::CImage* m_image;

        std::vector<CEffect*> m_effects;
        Effects::CMaterial* m_material;
    };
}
