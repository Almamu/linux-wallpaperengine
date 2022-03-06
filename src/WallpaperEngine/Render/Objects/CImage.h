#pragma once

#include "WallpaperEngine/Core/Objects/CImage.h"

#include "WallpaperEngine/Render/Objects/Effects/CMaterial.h"
#include "WallpaperEngine/Render/Objects/CEffect.h"
#include "WallpaperEngine/Render/CObject.h"
#include "WallpaperEngine/Render/CScene.h"

#include "WallpaperEngine/Render/Shaders/Compiler.h"

#include "WallpaperEngine/Assets/ITexture.h"

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
        friend CObject;
    public:
        CImage (CScene* scene, Core::Objects::CImage* image);

        void setup ();
        void render () override;

        const Core::Objects::CImage* getImage () const;
        const std::vector<CEffect*>& getEffects () const;
        const glm::vec2 getSize() const;

        const GLfloat* getVertex () const;
        const GLuint* getSceneSpacePosition () const;
        const GLuint* getCopySpacePosition () const;
        const GLuint* getPassSpacePosition () const;
        const GLuint* getTexCoordCopy () const;
        const GLuint* getTexCoordPass () const;
        ITexture* getTexture () const;
        const double getAnimationTime () const;

        /**
         * Performs a ping-pong on the available framebuffers to be able to continue rendering things to them
         *
         * @param drawTo The framebuffer to use
         * @param asInput The last texture used as output (if needed)
         */
        void pinpongFramebuffer (CFBO** drawTo, ITexture** asInput);

    protected:
        static const std::string Type;

        void simpleRender ();
        void complexRender ();
    private:
        ITexture* m_texture;
        GLuint m_sceneSpacePosition;
        GLuint m_copySpacePosition;
        GLuint m_passSpacePosition;
        GLuint m_texcoordCopy;
        GLuint m_texcoordPass;

        glm::mat4 m_modelViewProjectionScreen;
        glm::mat4 m_modelViewProjectionPass;

        CFBO* m_mainFBO;
        CFBO* m_subFBO;
        CFBO* m_currentMainFBO;
        CFBO* m_currentSubFBO;

        Core::Objects::CImage* m_image;

        std::vector<CEffect*> m_effects;
        Effects::CMaterial* m_material;
        Effects::CMaterial* m_copyMaterial;

        double m_animationTime;
    };
}
