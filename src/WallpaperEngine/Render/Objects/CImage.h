#pragma once

#include "WallpaperEngine/Render/CObject.h"
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

class CImage final : public CObject, public CFBOProvider {
    friend CObject;

  public:
    CImage (Wallpapers::CScene& scene, const Image& image);

    void setup ();
    void render () override;

    [[nodiscard]] const Image& getImage () const;
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
    void setupPasses ();

    void updateScreenSpacePosition ();

  private:
    std::shared_ptr<const ITexture> m_texture = nullptr;
    GLuint m_sceneSpacePosition;
    GLuint m_copySpacePosition;
    GLuint m_passSpacePosition;
    GLuint m_texcoordCopy;
    GLuint m_texcoordPass;

    glm::mat4 m_modelViewProjectionScreen = {};
    glm::mat4 m_modelViewProjectionPass = {};
    glm::mat4 m_modelViewProjectionCopy = {};
    glm::mat4 m_modelViewProjectionScreenInverse = {};
    glm::mat4 m_modelViewProjectionPassInverse = {};
    glm::mat4 m_modelViewProjectionCopyInverse = {};

    glm::mat4 m_modelMatrix = {};
    glm::mat4 m_viewProjectionMatrix = {};

    std::shared_ptr<const CFBO> m_mainFBO = nullptr;
    std::shared_ptr<const CFBO> m_subFBO = nullptr;
    std::shared_ptr<const CFBO> m_currentMainFBO = nullptr;
    std::shared_ptr<const CFBO> m_currentSubFBO = nullptr;

    const Image& m_image;

    std::vector<CEffect*> m_effects = {};
    Effects::CMaterial* m_material = nullptr;
    Effects::CMaterial* m_colorBlendMaterial = nullptr;
    std::vector<Effects::CPass*> m_passes = {};
    std::vector<MaterialPass> m_virtualPassess = {};

    glm::vec4 m_pos = {};

    double m_animationTime = 0.0;

    bool m_initialized = false;

    struct {
        struct {
            MaterialUniquePtr material;
            ImageEffectPassOverrideUniquePtr override;
        } colorBlending;
    } m_materials;
};
} // namespace WallpaperEngine::Render::Objects
