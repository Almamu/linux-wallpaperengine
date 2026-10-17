#pragma once

#include "CRenderable.h"
#include "PuppetModel.h"
#include "WallpaperEngine/Render/CObject.h"
#include "WallpaperEngine/Render/Objects/Effects/CPass.h"
#include "WallpaperEngine/Render/Wallpapers/CScene.h"

#include "WallpaperEngine/Render/Shaders/Shader.h"

#include "../TextureProvider.h"
#include "WallpaperEngine/Scripting/ScriptableObject.h"

#include <glm/vec3.hpp>
#include <map>
#include <memory>
#include <optional>
#include <vector>

using namespace WallpaperEngine;
using namespace WallpaperEngine::Render;
using namespace WallpaperEngine::Scripting;
namespace WallpaperEngine::Render::Objects::Effects {
class CMaterial;
class CPass;
} // namespace WallpaperEngine::Render::Objects::Effects

namespace WallpaperEngine::Render::Objects {
class CImage final : public CRenderable, public ScriptableObject {
    friend CObject;

public:
    CImage (Wallpapers::CScene& scene, const Image& image);
    ~CImage () override;

    void setup () override;
    void render () override;

    [[nodiscard]] const Image& getImage () const;
    [[nodiscard]] glm::vec2 getSize () const;

    [[nodiscard]] GLuint getSceneSpacePosition () const;
    [[nodiscard]] GLuint getCopySpacePosition () const;
    [[nodiscard]] GLuint getPassSpacePosition () const;
    [[nodiscard]] GLuint getTexCoordCopy () const;
    [[nodiscard]] GLuint getTexCoordPass () const;

    [[nodiscard]] const float& getBrightness () const override;
    [[nodiscard]] const float& getUserAlpha () const override;
    [[nodiscard]] const float& getAlpha () const override;
    [[nodiscard]] const glm::vec3& getColor () const override;
    [[nodiscard]] const glm::vec4& getColor4 () const override;
    [[nodiscard]] const glm::vec3& getCompositeColor () const override;

    /**
     * Performs a ping-pong on the available framebuffers to be able to continue rendering things to them
     *
     * @param drawTo The framebuffer to use
     * @param asInput The last texture used as output (if needed)
     */
    void pinpongFramebuffer (std::shared_ptr<const CFBO>* drawTo, std::shared_ptr<const TextureProvider>* asInput);

    [[nodiscard]] bool isCompositionLayer () const;
    [[nodiscard]] bool copiesCompositionBackground () const;
    [[nodiscard]] std::shared_ptr<const CFBO> getCompositionFBO () const;
    [[nodiscard]] std::optional<glm::mat4> getPuppetAttachmentMatrix (const std::string& name) const;

    [[nodiscard]] std::optional<ScriptableObject::AnimationLayerProperties>
    findAnimationLayer (const std::string& name) const override;

protected:
    void setupPasses ();

    void updateScreenSpacePosition ();

    struct ResolvedTransform {
	glm::vec3 origin;
	glm::vec3 scale;
	float angle;
    };

    [[nodiscard]] ResolvedTransform resolveTransform (const WallpaperEngine::Data::Model::Object& object) const;

    /**
     * Computes the object's own transform (origin/scale/angle) without walking the
     * parent chain. Used as the per-node step of resolveTransform.
     */
    [[nodiscard]] static ResolvedTransform localTransform (const WallpaperEngine::Data::Model::Object& object);

private:
    bool loadPuppetMesh (glm::vec2& size);
    [[nodiscard]] glm::vec2 computePuppetCanvasSize (const glm::vec2& size) const;
    void updatePuppetPositionBuffer (const glm::vec2& size);
    void setupPuppetGeometryCallback (Effects::CPass* pass) const;
    bool setupPuppetSourceEffectPass (Effects::CPass* pass);
    bool setupPuppetOpacityPass (Effects::CPass* pass);
    void setupPuppetMaskPass (Effects::CPass* effectPass, int textureIndex);
    void uploadPuppetPositions (const std::vector<GLfloat>& raw, const glm::vec2& size);
    void updatePuppetAnimation ();
    void updateAlphaAnimation ();
    ResolvedTransform updateGeometryBuffers ();
    [[nodiscard]] glm::vec2 resolveGeometrySize (float sceneWidth, float sceneHeight, glm::vec3& origin) const;
    void updateScenePosition (
	const glm::vec3& origin, const glm::vec2& size, const glm::vec3& scale, float sceneWidth, float sceneHeight
    );
    void uploadGeometryBuffers (const glm::vec2& size);
    [[nodiscard]] bool shouldRenderFinalPass (bool isLastPass) const;
    bool configurePassTarget (
	Effects::CPass* pass, std::shared_ptr<const CFBO>& drawTo,
	const std::shared_ptr<const TextureProvider>& asInput, std::shared_ptr<const TextureProvider>& effectInput,
	bool& inTargetEffectSequence
    );

    GLuint m_sceneSpacePosition;
    GLuint m_copySpacePosition;
    GLuint m_passSpacePosition;
    GLuint m_texcoordCopy;
    GLuint m_texcoordPass;
    GLuint m_puppetSpacePosition = GL_NONE;
    GLuint m_puppetTexCoord = GL_NONE;
    GLuint m_puppetIndices = GL_NONE;
    GLsizei m_puppetIndexCount = 0;
    bool m_hasPuppetMesh = false;
    /** Effectless puppet: the first pass can become the screen pass, so puppet verts
     *  may need to be remapped from texture-local space into the image quad. */
    bool m_puppetScreenSpace = false;
    std::vector<GLfloat> m_puppetRawPositions = {};
    mutable glm::vec4 m_color4Cache = glm::vec4 (1.0f);

    std::optional<PuppetModel> m_puppetModel = std::nullopt;
    struct PuppetLayerBinding {
	const PuppetModel::Clip* clip;
	const ImageAnimationLayer* layer;
    };
    std::vector<PuppetLayerBinding> m_puppetLayers = {};
    std::vector<PuppetModel::ActiveLayer> m_puppetActiveScratch = {};
    std::vector<glm::mat4> m_puppetSkinMatrices = {};
    std::vector<glm::vec3> m_puppetSkinnedPositions = {};
    std::vector<GLfloat> m_puppetSkinnedFlat = {};
    struct PuppetMaskPass {
	std::shared_ptr<const TextureProvider> source;
	std::shared_ptr<CFBO> target;
	std::unique_ptr<Effects::CPass> pass;
    };
    struct PuppetPrePass {
        std::shared_ptr<const TextureProvider> retainedTexture;
	std::unique_ptr<Effects::CPass> pass;
    };
    std::map<const TextureProvider*, size_t> m_puppetMaskPassBySource = {};
    std::vector<PuppetMaskPass> m_puppetMaskPasses = {};
    std::vector<PuppetPrePass> m_puppetPrePasses = {};
    std::shared_ptr<const TextureProvider> m_prePuppetTexture = nullptr;
    glm::vec4 m_whiteMaskColor = glm::vec4 (1.0f);

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

    std::vector<Effects::CPass*> m_passes = {};
    std::shared_ptr<const CFBO> m_compositionFBO = nullptr;
    std::vector<MaterialPassUniquePtr> m_virtualPassess = {};

    glm::vec4 m_pos = {};
    glm::vec3 m_sceneCenter = {};
    glm::vec2 m_size = {};
    float m_animatedAlpha = 1.0f;
    mutable float m_normalizedAlpha = 1.0f;

    bool m_initialized = false;

    struct {
	struct {
	    MaterialUniquePtr material;
	    ImageEffectPassOverrideUniquePtr override;
	} colorBlending;
	MaterialUniquePtr puppetMaskMaterial;
	std::vector<MaterialUniquePtr> compatibilityMaterials = {};
	std::vector<ImageEffectPassOverrideUniquePtr> compatibilityOverrides = {};
    } m_materials;
};
} // namespace WallpaperEngine::Render::Objects
