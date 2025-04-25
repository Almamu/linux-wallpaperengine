#pragma once

#include "WallpaperEngine/Core/Objects/CImage.h"

#include "WallpaperEngine/Render/CObject.h"
#include "WallpaperEngine/Render/Objects/CEffect.h"
#include "WallpaperEngine/Render/Objects/Effects/CMaterial.h"
#include "WallpaperEngine/Render/Objects/Effects/CPass.h"
#include "WallpaperEngine/Render/Wallpapers/CScene.h"

#include "WallpaperEngine/Render/Shaders/CShader.h"

#include "WallpaperEngine/Assets/ITexture.h"

#include <glm/vec3.hpp>

using namespace WallpaperEngine;
using namespace WallpaperEngine::Assets;

namespace WallpaperEngine::Render::Objects::Effects {
class CMaterial;
class CPass;
} // namespace WallpaperEngine::Render::Objects::Effects

namespace WallpaperEngine::Render::Objects {
class CEffect;

class CImage final : public CObject {
    friend CObject;

  public:
    CImage (Wallpapers::CScene* scene, const Core::Objects::CImage* image);

    void setup ();
    void render () override;

    [[nodiscard]] const Core::Objects::CImage* getImage () const;
    [[nodiscard]] const std::vector<CEffect*>& getEffects () const;
    [[nodiscard]] const Effects::CMaterial* getMaterial () const;
    [[nodiscard]] glm::vec2 getSize () const;

    [[nodiscard]] GLuint getSceneSpacePosition () const;
    [[nodiscard]] GLuint getCopySpacePosition () const;
    [[nodiscard]] GLuint getPassSpacePosition () const;
    [[nodiscard]] GLuint getTexCoordCopy () const;
    [[nodiscard]] GLuint getTexCoordPass () const;
    [[nodiscard]] std::shared_ptr<const ITexture> getTexture () const;
    [[nodiscard]] double getAnimationTime () const;

    /**
     * Performs a ping-pong on the available framebuffers to be able to continue rendering things to them
     *
     * @param drawTo The framebuffer to use
     * @param asInput The last texture used as output (if needed)
     */
    void pinpongFramebuffer (std::shared_ptr<const CFBO>* drawTo, std::shared_ptr<const ITexture>* asInput);

  protected:
    static const std::string Type;

    void setupPasses ();

    void updateScreenSpacePosition ();

  private:
    std::shared_ptr<const ITexture> m_texture;
    GLuint m_sceneSpacePosition;
    GLuint m_copySpacePosition;
    GLuint m_passSpacePosition;
    GLuint m_texcoordCopy;
    GLuint m_texcoordPass;

    glm::mat4 m_modelViewProjectionScreen;
    glm::mat4 m_modelViewProjectionPass;
    glm::mat4 m_modelViewProjectionCopy;
    glm::mat4 m_modelViewProjectionScreenInverse;
    glm::mat4 m_modelViewProjectionPassInverse;
    glm::mat4 m_modelViewProjectionCopyInverse;

    glm::mat4 m_modelMatrix;
    glm::mat4 m_viewProjectionMatrix;

    std::shared_ptr<const CFBO> m_mainFBO;
    std::shared_ptr<const CFBO> m_subFBO;
    std::shared_ptr<const CFBO> m_currentMainFBO;
    std::shared_ptr<const CFBO> m_currentSubFBO;

    const Core::Objects::CImage* m_image;

    std::vector<CEffect*> m_effects;
    Effects::CMaterial* m_material;
    Effects::CMaterial* m_colorBlendMaterial;
    std::vector<Effects::CPass*> m_passes;

    glm::vec4 m_pos;

    double m_animationTime;

    bool m_initialized;
};
} // namespace WallpaperEngine::Render::Objects
