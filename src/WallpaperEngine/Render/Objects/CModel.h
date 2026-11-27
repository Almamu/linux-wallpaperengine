#pragma once

#include "CRenderable.h"
#include "ModelBounds.h"
#include "ModelData.h"
#include "WallpaperEngine/Data/Model/DynamicValue.h"
#include "WallpaperEngine/Render/Objects/Effects/CPass.h"
#include "WallpaperEngine/Scripting/ScriptableObject.h"

#include <glm/mat3x3.hpp>
#include <glm/mat4x3.hpp>
#include <array>
#include <limits>
#include <map>
#include <vector>

namespace WallpaperEngine::Render::Objects {
using namespace WallpaperEngine::Scripting;

/**
 * 3D model renderer for perspective scenes. Draws static and GPU-skinned MDLV
 * meshes through the regular material/pass pipeline (generic4 and friends)
 * with depth state taken from the material.
 */
class CModel final : public CRenderable, public ScriptableObject {
public:
    CModel (Wallpapers::CScene& scene, const Model3D& model);
    ~CModel () override;

    void setup () override;
    /** Refresh pose and transforms once, after the scene scripts and camera update. */
    void prepareRender ();
    void render () override;
    /** Submit one material class when a perspective scene sorts model passes. */
    void render (RenderSortClass renderClass);
    [[nodiscard]] const std::vector<RenderSortClass>& getRenderSortClasses () const;
    /** Draw opaque/cutout model depth into the scene shadow atlas. */
    void renderShadow (const glm::mat4& lightViewProjection);
    void renderReflection (const glm::mat4& view, const glm::vec3& eye);

    [[nodiscard]] const Model3D& getModel () const;
    struct AnimationLayer {
	const ImageAnimationLayer* settings = nullptr;
	const MdlAnimationClip* clip = nullptr;
	PropertyAnimation timeline = {};
	std::string key;
	float blend = 1.0f;
	bool visible = true;
	bool single = false;
	bool blendIn = true;
	bool blendOut = true;
	float blendTime = 0.5f;
	struct BlendWrite {
	    double time;
	    float value;
	};
	std::optional<BlendWrite> blendWrite;
	[[nodiscard]] bool hasBlendCurve () const;
	[[nodiscard]] float evaluateBlend (double time) const;
	void setBlend (float value, double time);
    };
    struct SingleAnimationConfig {
	bool blendIn = true;
	bool blendOut = true;
	float blendTime = 0.5f;
    };
    void prepareAnimationEvents () override;
    [[nodiscard]] AnimationLayer* findAnimationLayer (const std::string& key);
    [[nodiscard]] AnimationLayer* findAnimatedBlend (const DynamicValue& property);
    void applyScriptBlend (AnimationLayer& layer, float value, double time);
    [[nodiscard]] AnimationLayer* getAnimationLayer (const std::string& name);
    [[nodiscard]] AnimationLayer* getAnimationLayer (size_t index);
    [[nodiscard]] size_t getAnimationLayerCount () const;
    AnimationLayer* playSingleAnimation (const std::string& name, const SingleAnimationConfig& config);
    void animationControlsChanged (PropertyAnimation& animation);
    [[nodiscard]] std::optional<glm::mat4> getAttachmentTransform (const std::string& name) const override;
    [[nodiscard]] std::optional<size_t> getAttachmentIndex (const std::string& name) const override;
    [[nodiscard]] std::optional<std::string> getAttachmentName (size_t index) const override;
    [[nodiscard]] std::optional<size_t> getBoneIndex (const std::string& name) const override;
    [[nodiscard]] std::optional<glm::mat4> getBoneTransform (size_t index) const override;

    [[nodiscard]] const float& getBrightness () const override;
    [[nodiscard]] const float& getUserAlpha () const override;
    [[nodiscard]] const float& getAlpha () const override;
    [[nodiscard]] const glm::vec3& getColor () const override;
    [[nodiscard]] const glm::vec4& getColor4 () const override;
    [[nodiscard]] const glm::vec3& getCompositeColor () const override;

    /**
     * Submit opaque/alpha-to-coverage passes first, preserving authored order
     * across the shared translucent/additive group.
     */
    [[nodiscard]] static std::vector<size_t> calculatePassRenderPermutation (const std::vector<BlendingMode>& modes);

private:
    struct ShadowCaster {
	Effects::CPass* pass = nullptr;
	int leavesUVMode = -1;
    };

    struct SubmeshBuffers {
	GLuint vertexBuffer = GL_NONE;
	GLuint indexBuffer = GL_NONE;
	GLsizei indexCount = 0;
	GLsizei vertexCount = 0;
	GLenum indexType = GL_UNSIGNED_INT;
	GLuint shadowVao = GL_NONE;
	uint32_t shadowVertexTag = 0;
	ShadowCaster shadowCaster;
        std::vector<float> morphVertices;
    };

    struct ShadowProgram {
	std::shared_ptr<const Shaders::ShaderProgramCache::SharedProgram> interface;
	std::shared_ptr<const Shaders::ShaderProgramCache::SharedProgram> program;
	GLint lightViewProjection = -1;
	GLint model = -1;
	GLint bones = -1;
	GLint albedo = -1;
	GLint alphaToCoverage = -1;
	GLint time = -1;
	std::array<GLint, 11> foliageUniforms {};
    };

    void setupGeometryCallback (Effects::CPass* pass, size_t submeshIndex);
    void registerMaterialProperties (size_t index, const Material& material, bool deferInitialization = false);
    void unregisterMaterialProperties (size_t index, const Material& material);
    void setupMaterialPasses (const std::vector<const Material*>& materials);
    void syncModelDataMaterials ();
    void setupShadowProgram (int leavesUVMode = -1);
    void renderPasses (std::optional<RenderSortClass> renderClass);
    void updateAnimationPose (bool stepPhysics = false) const;
    void updateMorphBuffers ();
    void syncAnimationLayers () const;
    [[nodiscard]] glm::mat4 resolveModelMatrix () const;
    void updateMatrices ();

    const Model3D& m_model;
    std::shared_ptr<ModelData> m_modelData;
    std::vector<MaterialUniquePtr> m_runtimeMaterials;
    std::vector<const Material*> m_materials;
    std::vector<uint64_t> m_materialShapeIds;
    uint64_t m_structureRevision = UINT64_MAX;
    uint64_t m_materialRevision = 0;
    /** Legacy SceneScript compatibility: published model layers read thisLayer.size. */
    Data::Model::DynamicValue m_size;
    // Stable addresses for the timelines registered with SceneScript. Controllers
    // retain keys, so removal never leaves JavaScript holding a dangling pointer.
    std::vector<std::unique_ptr<AnimationLayer>> m_animationLayers;
    uint64_t m_nextAnimationLayer = 0;

    std::vector<SubmeshBuffers> m_submeshes = {};
    mutable std::vector<glm::mat4> m_worldBones = {};
    mutable std::vector<glm::mat4> m_skinBones = {};
    /** Packed affine matrices uploaded to the shader's mat4x3 g_Bones array. */
    mutable std::vector<glm::mat4x3> m_gpuSkinBones = {};
    mutable double m_poseTime = std::numeric_limits<double>::quiet_NaN ();
    mutable Data::Model::MdlPhysicsState m_physicsState;
    mutable std::optional<uint32_t> m_physicsFrame;
    mutable std::optional<Data::Model::MdlAnimationBounds> m_animationBounds;
    bool m_skinningEnabled = false;
    bool m_morphingEnabled = false;
    bool m_morphActive = false;
    mutable bool m_morphDirty = false;
    mutable std::vector<Data::Model::MdlModelMorphState> m_modelMorphs;

    glm::mat4 m_modelMatrix = glm::mat4 (1.0f);
    glm::mat4 m_viewProjectionMatrix = glm::mat4 (1.0f);
    glm::mat4 m_modelViewProjectionMatrix = glm::mat4 (1.0f);
    glm::mat4 m_modelViewProjectionMatrixInverse = glm::mat4 (1.0f);
    glm::mat3 m_normalMatrix = glm::mat3 (1.0f);
    glm::vec3 m_eyePosition = glm::vec3 (0.0f);

    std::vector<Effects::CPass*> m_passes = {};
    std::vector<RenderSortClass> m_renderSortClasses = {};
    // -1 is ordinary model depth; foliage variants retain their authored leaf UV mode.
    std::map<int, ShadowProgram> m_shadowPrograms;
    std::optional<Data::Model::MdlAnimationBounds> m_renderBounds;
    ModelBounds m_shadowBounds;
    ModelBounds::Box m_posedShadowBounds;
    bool m_frameVisible = false;
    bool m_mainViewVisible = true;
    bool m_initialized = false;

    /** model objects carry no per-object tint/alpha; materials do that via constants */
    float m_brightness = 1.0f;
    float m_alpha = 1.0f;
    glm::vec3 m_color = glm::vec3 (1.0f);
    glm::vec4 m_color4 = glm::vec4 (1.0f);
};
} // namespace WallpaperEngine::Render::Objects
