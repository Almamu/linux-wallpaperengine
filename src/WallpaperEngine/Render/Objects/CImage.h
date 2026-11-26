#pragma once

#include "Effects/EffectSwapCommand.h"

#include "CRenderable.h"
#include "WallpaperEngine/Data/Model/MdlAnimation.h"
#include "WallpaperEngine/Render/CObject.h"
#include "WallpaperEngine/Render/Objects/Effects/CPass.h"
#include "WallpaperEngine/Render/Objects/PuppetClippingPlan.h"
#include "WallpaperEngine/Render/Wallpapers/CScene.h"

#include "WallpaperEngine/Render/Shaders/Shader.h"

#include "../TextureProvider.h"
#include "WallpaperEngine/Scripting/ScriptableObject.h"

#include <array>
#include <glm/vec3.hpp>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
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
    struct PerspectiveSceneMatrices {
	glm::mat4 model;
	glm::mat4 viewProjection;
	glm::mat4 modelViewProjection;
	glm::mat3 normalModel;
    };

    CImage (Wallpapers::CScene& scene, const Image& image);
    ~CImage () override;

    void setup () override;
    void render () override;
    [[nodiscard]] bool isVolumetricBarrier () const override;
    void prepareRenderEffects ();

    [[nodiscard]] const Image& getImage () const;
    [[nodiscard]] bool isCompositionLayer () const;
    [[nodiscard]] bool copiesCompositionBackground () const;
    [[nodiscard]] std::shared_ptr<const CFBO> getCompositionFBO () const;
    [[nodiscard]] glm::vec2 getSize () const;
    [[nodiscard]] glm::mat4 resolveLayerModelMatrix () const override;
    [[nodiscard]] std::optional<glm::mat4> getAttachmentTransform (const std::string& name) const override;
    [[nodiscard]] std::optional<size_t> getAttachmentIndex (const std::string& name) const override;
    [[nodiscard]] std::optional<std::string> getAttachmentName (size_t index) const override;
    [[nodiscard]] std::optional<size_t> getBoneIndex (const std::string& name) const override;
    [[nodiscard]] std::optional<glm::mat4> getBoneTransform (size_t index) const override;

    /** Return cursor coordinates local to this image when the world-space point
     *  intersects its authored quad; std::nullopt means the point is outside. */
    [[nodiscard]] std::optional<glm::vec3> cursorLocalPosition (const glm::vec3& worldPosition) const;
    [[nodiscard]] std::optional<std::string> cursorHitBox (const glm::vec3& worldPosition) const;
    /** Intersect a perspective cursor ray with a transformed image's local Z=0 plane. */
    [[nodiscard]] static std::optional<glm::vec3> intersectCursorPlane (
	const glm::mat4& world, const glm::mat4& viewProjection,
	const glm::vec2& normalizedPosition, bool projectionYFlipped
    );

    /** Projects model/puppet attachments onto nonfullscreen image receivers. */
    [[nodiscard]] std::optional<glm::mat3> transformAttachmentToTexture (
        const CObject& source, const glm::mat4& attachmentWorld
    ) const;
    [[nodiscard]] static glm::mat3 calculateOrthographicAttachmentTextureMatrix (
        const glm::mat4& attachmentWorld, const glm::mat4& receiverWorld,
        const glm::mat4& sourceViewProjection, const glm::mat4& receiverViewProjection,
        float cameraZ, const glm::vec2& size, std::optional<glm::vec3> receiverEye = std::nullopt,
        float receiverClipDepth = 1.0f
    );

    /** Split a perspective image transform into the matrices consumed by genericimage3/4.
     *  Their LIGHTING path uses model and view-projection separately instead of the MVP. */
    [[nodiscard]] static PerspectiveSceneMatrices
    calculatePerspectiveSceneMatrices (const glm::mat4& world, const glm::mat4& viewProjection);

    [[nodiscard]] bool hasPuppetMorphAlpha () const { return m_puppetVertexColor != GL_NONE; }
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

    [[nodiscard]] size_t getPuppetAnimationLayerCount () const;
    [[nodiscard]] std::optional<size_t> findPuppetAnimationLayer (int index) const;
    [[nodiscard]] std::optional<size_t> findPuppetAnimationLayer (const std::string& name) const;
    [[nodiscard]] float getPuppetAnimationLayerFrame (size_t index) const;
    void setPuppetAnimationLayerFrame (size_t index, float frame);
    void playPuppetAnimationLayer (std::optional<size_t> index = std::nullopt);
    void pausePuppetAnimationLayer (std::optional<size_t> index = std::nullopt);
    void stopPuppetAnimationLayer (std::optional<size_t> index = std::nullopt);
    [[nodiscard]] bool isPuppetAnimationLayerPlaying (std::optional<size_t> index = std::nullopt) const;

    /**
     * Performs a ping-pong on the available framebuffers to be able to continue rendering things to them
     *
     * @param drawTo The framebuffer to use
     * @param asInput The last texture used as output (if needed)
     */
    void pinpongFramebuffer (std::shared_ptr<const CFBO>* drawTo, std::shared_ptr<const TextureProvider>* asInput);

protected:
    void setupPasses ();
    void updateEffectVisibility ();

    void updateScreenSpacePosition ();

    struct ResolvedTransform {
	glm::vec3 origin;
	glm::vec3 scale;
	// raw scene angles in radians, accumulated through the parent chain;
	// the render path negates x/z to account for the y-flipped coordinate system
	glm::vec3 angles;
    };

    [[nodiscard]] ResolvedTransform resolveTransform (const WallpaperEngine::Data::Model::Object& object) const;

    /**
     * Computes the object's own transform (origin/scale/angle) without walking the
     * parent chain. Used as the per-node step of resolveTransform.
     */
    [[nodiscard]] static ResolvedTransform
    localTransform (const WallpaperEngine::Data::Model::Object& object, double time);

private:
    [[nodiscard]] bool usesReverseProjection (const glm::mat4* projection) const;
    struct PuppetLayerPlayback {
	bool initialized = false;
	bool playing = true;
	bool stopped = false;
	bool finished = false;
	bool reverse = false;
	bool visible = false;
	uint32_t animation = 0;
	float time = 0.0f;
	double lastSceneTime = 0.0;
	float visibilityWeight = 0.0f;
    };

    bool loadPuppetMesh (const glm::vec2& size);
    void loadPuppetOverlay (const WallpaperEngine::Data::Model::MdlMesh& mesh, const glm::vec2& size);
    void setupPuppetAlbedoPasses ();
    void renderPuppetAlbedo ();
    [[nodiscard]] const WallpaperEngine::Data::Model::MdlAnimationClip* findPuppetAnimationClip (uint32_t id) const;
    void selectPuppetAnimations (double sceneTime);
    void updatePuppetPositionBuffer (const glm::vec2& size, bool stepRope = false);
    void updatePuppetClippingCommands (const PuppetClippingPlan& plan);
    [[nodiscard]] std::optional<ResolvedTransform> puppetAttachmentTransform (const std::string& name) const;
    void setupPuppetGeometryCallback (Effects::CPass* pass, bool samplesSourceTexture) const;
    void setupPuppetRangeGeometryCallback (
	Effects::CPass* pass, bool samplesSourceTexture,
	const std::vector<WallpaperEngine::Data::Model::MdlDrawRange>& ranges,
	const std::vector<WallpaperEngine::Data::Model::MdlDrawRange>* dynamicRanges = nullptr
    ) const;
    bool setupPuppetClippingPasses (
	Effects::CPass& finalPass, const std::shared_ptr<const TextureProvider>& input,
	const std::shared_ptr<const CFBO>& destination, const glm::mat4* projection,
	const glm::mat4* inverseProjection, bool samplesSourceTexture
    );
    void renderPuppetClipping ();
    void updateSceneTargets () const;
    ResolvedTransform updateGeometryBuffers ();
    [[nodiscard]] glm::vec2 resolveGeometrySize (float sceneWidth, float sceneHeight, glm::vec3& origin) const;
    void updateScenePosition (
	const glm::vec3& origin, const glm::vec2& size, const glm::vec3& scale, float sceneWidth, float sceneHeight
    );
    void uploadGeometryBuffers (const glm::vec2& size);
    void updateEffectSceneMatrix ();
    [[nodiscard]] bool shouldRenderFinalPass (bool isLastPass) const;
    void updateFinalPassVisibility (bool force = false);
    bool configurePassTarget (
	Effects::CPass* pass, std::shared_ptr<const CFBO>& drawTo,
	const std::shared_ptr<const TextureProvider>& asInput, std::shared_ptr<const TextureProvider>& effectInput,
	bool& inTargetEffectSequence
    );

    GLuint m_sceneSpacePosition;
    GLuint m_effectSpacePosition = GL_NONE;
    GLuint m_copySpacePosition;
    GLuint m_passSpacePosition;
    GLuint m_texcoordCopy;
    GLuint m_texcoordPass;
    GLuint m_puppetSpacePosition = GL_NONE;
    GLuint m_puppetPrelightingPosition = GL_NONE;
    size_t m_puppetPositionBufferBytes = 0;
    GLuint m_puppetVertexColor = GL_NONE;
    size_t m_puppetColorBufferBytes = 0;
    struct PuppetVertexColor {
	glm::vec4 color;
	float maskAlpha;
    };
    std::vector<PuppetVertexColor> m_puppetColors;
    GLuint m_puppetTexCoord = GL_NONE;
    GLuint m_puppetTexCoordFirstPass = GL_NONE;
    GLuint m_puppetIndices = GL_NONE;
    GLsizei m_puppetIndexCount = 0;
    std::vector<uint32_t> m_puppetRangeOrder;
    std::vector<WallpaperEngine::Data::Model::MdlDrawRange> m_puppetDrawRanges;
    bool m_hasPuppetMesh = false;
    std::vector<GLfloat> m_puppetRawPositions = {};
    std::vector<uint32_t> m_puppetBlendIndices = {};
    std::vector<GLfloat> m_puppetBlendWeights = {};
    WallpaperEngine::Data::Model::MdlAnimationData m_puppetAnimation = {};
    WallpaperEngine::Data::Model::MdlPhysicsState m_puppetPhysics = {};
    double m_puppetPhysicsTime = 0.0;
    std::vector<glm::mat4> m_puppetWorldBones = {};
    /** Sampled scene-space API snapshots before each owner's own group solve. */
    std::vector<glm::mat4> m_puppetPublishedSceneBones;
    std::vector<glm::mat4> m_puppetSkinBones = {};
    std::vector<GLfloat> m_puppetPositions = {};
    std::vector<WallpaperEngine::Data::Model::MdlActiveAnimation> m_puppetActiveLayers = {};
    std::unordered_map<int, PuppetLayerPlayback> m_puppetLayerPlayback = {};

    /**
     * Channelmap overlay: a second puppet submesh of static quads composited into
     * _rt_imageLayerAlbedo_<id> before effects and body deformation. This mirrors
     * Wallpaper Engine's puppettexturechannels path for authored eye blinks.
     */
    GLuint m_puppetOverlayPosition = GL_NONE;
    GLuint m_puppetOverlayTexCoord = GL_NONE;
    GLuint m_puppetOverlayBlendIndices = GL_NONE;
    GLuint m_puppetOverlayIndices = GL_NONE;
    GLsizei m_puppetOverlayIndexCount = 0;
    std::string m_puppetOverlayMaterial = {};
    /** g_BlendMap row zero; the shader indexes it with a_BlendIndices.x. */
    glm::vec4 m_puppetBlendMap = glm::vec4 (0.0f);
    Effects::CPass* m_puppetAlbedoCopyPass = nullptr;
    Effects::CPass* m_puppetOverlayPass = nullptr;
    std::shared_ptr<const CFBO> m_puppetAlbedoFBO = nullptr;
    const ImageEffectPassOverride* m_puppetAlbedoInputOverride = nullptr;

    struct PuppetClippingRenderCommand {
	bool buildsMask = false;
	bool intermediateMask = false;
	Effects::CPass* pass = nullptr;
	WallpaperEngine::Data::Model::MdlDrawRange range;
	WallpaperEngine::Data::Model::BlendingMode blending = WallpaperEngine::Data::Model::BlendingMode_Unknown;
    };
    std::vector<Effects::CPass*> m_puppetClippingPasses = {};
    std::vector<PuppetClippingRenderCommand> m_puppetClippingCommands = {};
    std::vector<Effects::CPass*> m_puppetMaskPasses;
    std::vector<std::vector<WallpaperEngine::Data::Model::MdlDrawRange>> m_puppetClippingSourceRanges;
    Effects::CPass* m_puppetMaskComposePass = nullptr;
    Effects::CPass* m_puppetPlainPass = nullptr;
    Effects::CPass* m_puppetClippedPass = nullptr;
    std::vector<WallpaperEngine::Data::Model::MdlDrawRange> m_puppetClippingDrawRange { WallpaperEngine::Data::Model::MdlDrawRange {} };
    std::vector<std::unique_ptr<TextureMap>> m_puppetClippingBinds = {};
    std::vector<std::unique_ptr<glm::vec4>> m_puppetClippingRenderVars = {};
    std::shared_ptr<CFBO> m_puppetClippingFBO = nullptr;
    std::shared_ptr<CFBO> m_puppetClippingIntermediateFBO = nullptr;
    bool m_hasPuppetClipping = false;

    glm::mat4 m_modelViewProjectionScreen = {};
    glm::mat4 m_modelViewProjectionPass = {};
    glm::mat4 m_modelViewProjectionCopy = {};
    glm::mat4 m_modelViewProjectionScreenInverse = {};
    glm::mat4 m_effectSceneModel = glm::mat4 (1.0f);
    glm::mat4 m_effectSceneProjection = glm::mat4 (1.0f);
    glm::mat4 m_effectSceneProjectionInverse = glm::mat4 (1.0f);
    glm::mat4 m_framebufferReadProjection = glm::mat4 (1.0f);
    glm::mat4 m_modelViewProjectionPassInverse = {};
    glm::mat4 m_modelViewProjectionCopyInverse = {};
    glm::mat4 m_puppetPrelightingProjection = glm::mat4 (1.0f);
    glm::mat4 m_puppetPrelightingProjectionInverse = glm::mat4 (1.0f);

    glm::mat4 m_modelMatrix = {};
    glm::mat4 m_viewProjectionMatrix = {};
    glm::mat4 m_sceneModelMatrix = glm::mat4 (1.0f);
    glm::mat4 m_sceneViewProjectionMatrix = glm::mat4 (1.0f);
    glm::mat3 m_sceneNormalModelMatrix = glm::mat3 (1.0f);
    Effects::CPass* m_prelightingPass = nullptr;
    /** Presentation-only fog; hidden image composites export its input. */
    Effects::CPass* m_fogPass = nullptr;
    glm::mat4 m_prelightingModelMatrix = glm::mat4 (1.0f);

    /** rotation-only local->world matrix (and its inverse) fed to effect passes as
     * g_EffectTextureProjectionMatrix so depthparallax-style shaders can rotate the
     * parallax input into the layer's own local axes; identity when angles are zero */
    glm::mat4 m_effectTextureProjectionMatrix = glm::mat4 (1.0f);
    glm::mat4 m_effectTextureProjectionMatrixInverse = glm::mat4 (1.0f);

    std::shared_ptr<CFBO> m_mainFBO = nullptr;
    std::shared_ptr<CFBO> m_subFBO = nullptr;
    std::shared_ptr<const CFBO> m_currentMainFBO = nullptr;
    std::shared_ptr<const CFBO> m_currentSubFBO = nullptr;
    /** Isolated child surface used by models/util/composelayer.json. */
    std::shared_ptr<CFBO> m_compositionFBO = nullptr;
    struct SceneEffectTarget {
	std::shared_ptr<CFBO> target;
	const FBO* definition;
    };
    std::vector<SceneEffectTarget> m_sceneEffectTargets = {};

    const Image& m_image;
    // Stable storage is required because effect-pass uniforms keep pointers to
    // getAlpha()/getUserAlpha() for the lifetime of the image.
    float m_resolvedAlpha = 1.0f;
    mutable glm::vec4 m_resolvedColor4 { 1.0f };

    std::vector<Effects::CPass*> m_passes = {};
    std::unordered_set<Effects::CPass*> m_copyPasses;
    std::vector<Effects::EffectSwapCommand> m_swapCommands;
    std::vector<Effects::CPass*> m_activePasses = {};
    std::unordered_map<Effects::CPass*, const UserSetting*> m_passVisibility;
    std::unordered_set<const UserSetting*> m_failedEffects;
    std::vector<MaterialPassUniquePtr> m_virtualPassess = {};

    glm::vec4 m_pos = {};
    glm::vec3 m_sceneCenter = {};
    glm::vec2 m_size = {};
    glm::vec2 m_effectSize = {};

    std::array<GLfloat, 18> m_cachedSceneSpacePosition = {};
    std::array<GLfloat, 18> m_cachedEffectSpacePosition = {};
    std::array<GLfloat, 18> m_cachedCopySpacePosition = {};
    std::array<GLfloat, 12> m_cachedTexcoordCopy = {};
    bool m_geometryBufferCacheValid = false;

    bool m_initialized = false;
    struct FinalPassRouting {
	Effects::CPass* pass;
	std::shared_ptr<const CFBO> offscreenTarget;
	std::shared_ptr<const TextureProvider> input;
	GLuint offscreenPosition;
	const glm::mat4* offscreenProjection;
	const glm::mat4* offscreenProjectionInverse;
	bool samplesSourceTexture;
	BlendingMode sceneBlending;
    };
    std::optional<FinalPassRouting> m_finalPassRouting;
    bool m_puppetFinalPassConfigured = false;
    /** Current routing of the implicit final pass; visibility may change after setup. */
    bool m_finalPassDrawsToScene = false;

    struct {
	struct {
	    MaterialUniquePtr material;
	    ImageEffectPassOverrideUniquePtr override;
	} colorBlending;
	std::vector<MaterialUniquePtr> compatibilityMaterials = {};
	std::vector<ImageEffectPassOverrideUniquePtr> compatibilityOverrides = {};
    } m_materials;
};
} // namespace WallpaperEngine::Render::Objects
