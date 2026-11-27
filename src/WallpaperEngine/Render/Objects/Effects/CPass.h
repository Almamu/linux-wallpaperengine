#pragma once

#include <functional>
#include <array>
#include <cstdint>
#include <glm/gtc/type_ptr.hpp>
#include <set>
#include <utility>

#include "../../TextureProvider.h"
#include "EffectFBOBindings.h"
#include "WallpaperEngine/Data/Model/Material.h"
#include "WallpaperEngine/Render/CFBO.h"
#include "WallpaperEngine/Render/FBOProvider.h"
#include "WallpaperEngine/Render/Helpers/ContextAware.h"
#include "WallpaperEngine/Render/Shaders/Shader.h"
#include "WallpaperEngine/Render/Shaders/ShaderProgramCache.h"

namespace WallpaperEngine::Render::Objects {
class CRenderable;
}

namespace WallpaperEngine::Render::Objects::Effects {
using namespace WallpaperEngine::Render;
using namespace WallpaperEngine::Render::Shaders::Variables;
using namespace WallpaperEngine::Data::Model;

class CPass final : public Helpers::ContextAware {
public:
    CPass (
	CRenderable& renderable, std::shared_ptr<const FBOProvider> fboProvider, const MaterialPass& pass,
	std::optional<std::reference_wrapper<const ImageEffectPassOverride>> override,
	std::optional<std::reference_wrapper<const TextureMap>> binds,
	std::optional<std::reference_wrapper<std::string>> target, ComboMap runtimeCombos = {},
	bool deferShaderSetup = false, std::shared_ptr<const EffectFBOBindings> effectFBOBindings = nullptr
    );
    ~CPass ();

    /** Build an initially hidden effect only when it first enters the active chain. */
    void initialize ();
    void render ();
    /** Refresh initialized user-image materials, including currently hidden passes. */
    bool updateUserTextures ();
    void updateFogShader ();
    /** Bind the current standard albedo provider/frame for an alpha-cutout shadow draw. */
    bool bindAlbedoTexture ();
    /** Cold authored dependencies needed when an initialized selection is cleared. */
    void collectUserTextureFallbacks (std::set<std::string>& names) const;

    /** Copy commands remap their explicit source together with effect binds. */
    void setCopySource (const std::string& source);
    void setDestination (std::shared_ptr<const CFBO> drawTo);
    void setInput (std::shared_ptr<const TextureProvider> input);
    void setPreviousInput (std::shared_ptr<const TextureProvider> input);
    void setTexCoord (GLuint texcoord);
    void setPosition (GLuint position);
    void setModelViewProjectionMatrix (const glm::mat4* projection, bool reverseDepth = false);
    void setModelViewProjectionMatrixInverse (const glm::mat4* projection);
    void setModelMatrix (const glm::mat4* model);
    void setViewProjectionMatrix (const glm::mat4* viewProjection);
    void setEffectTextureProjectionMatrix (const glm::mat4* matrix, const glm::mat4* inverse);
    void setBlendingMode (BlendingMode blendingmode);
    [[nodiscard]] BlendingMode getBlendingMode () const;
    void setDepthtestMode (DepthtestMode depthtestmode);
    [[nodiscard]] DepthtestMode getDepthtestMode () const;
    void setDepthwriteMode (DepthwriteMode depthwritemode);
    [[nodiscard]] DepthwriteMode getDepthwriteMode () const;
    /** Selects the RGB/alpha blend equations; defaults to ordinary additive blending. */
    void setBlendEquation (GLenum color, GLenum alpha);
    [[nodiscard]] std::shared_ptr<const CFBO> resolveFBO (const std::string& name) const;

    [[nodiscard]] std::shared_ptr<const FBOProvider> getFBOProvider () const;
    [[nodiscard]] const CRenderable& getRenderable () const;
    [[nodiscard]] const MaterialPass& getPass () const;
    [[nodiscard]] const ImageEffectPassOverride& getOverride () const;
    [[nodiscard]] const TextureMap& getBinds () const;
    [[nodiscard]] std::optional<std::reference_wrapper<std::string>> getTarget () const;
    [[nodiscard]] Render::Shaders::Shader* getShader () const;
    [[nodiscard]] GLuint getProgramID () const;
    [[nodiscard]] uint64_t getShaderRevision () const;
    void setRuntimeCombos (const ComboMap& combos);

    // Custom geometry rendering support (for particles, etc.)
    using GeometryCallback = std::function<void ()>;
    void
    setGeometryCallback (GeometryCallback setupAttribs, GeometryCallback drawGeometry, GeometryCallback cleanupAttribs);

    // Public uniform setters for external callers (pointer-based, updated per-frame)
    void addUniform (const std::string& name, const float* value, int count = 1);
    void addUniform (const std::string& name, const glm::vec3* value);
    void addUniform (const std::string& name, const glm::vec4* value, int count = 1);
    void addUniform (const std::string& name, const glm::mat3* value);
    void addUniform (const std::string& name, const glm::mat4* value, int count = 1);
    void addUniform (const std::string& name, const glm::mat4x3* value, int count = 1);
    [[nodiscard]] bool samplesLegacyReflection () const;
    [[nodiscard]] bool samplesRenderTarget (const CFBO& target) const;

private:
    bool m_reverseDepth = false;
    [[nodiscard]] std::shared_ptr<const TextureProvider> resolveUserTexture (int index);
    void setupUserTexture (int index, const std::string& entry);
    struct TextureChainEntry {
	std::shared_ptr<const TextureProvider> texture;
	std::shared_ptr<TextureChainEntry> next;
	bool legacyReflection = false;
    };
    [[nodiscard]] std::shared_ptr<const TextureProvider> resolveChainEntry (const TextureChainEntry& entry) const;
    bool m_hasLegacyReflectionBinding = false;
    bool m_usesLegacyReflectionProjection = false;
    int m_legacyReflectionSamplers = 0;

    /**
     * Pins (increment) or releases (decrement) every texture this pass references.
     * Video textures only decode while something holds a usage count on them and
     * CImage only pins its main texture, so effect/pass inputs must be pinned here
     */
    void adjustTextureUsageCounts (bool increment) const;

    enum UniformType {
	Float = 0,
	Matrix3 = 1,
	Matrix4 = 2,
	Matrix4x3 = 3,
	Integer = 4,
	Vector2 = 5,
	Vector3 = 6,
	Vector4 = 7,
	Double = 8
    };

    class UniformEntry {
    public:
	UniformEntry (const GLint id, std::string name, UniformType type, const void* value, int count) :
	    id (id), name (std::move (name)), type (type), value (value), count (count) { }
	UniformEntry (
	    const GLint id, std::string name, UniformType type, std::shared_ptr<const void> ownedValue, int count
	) :
	    id (id), name (std::move (name)), type (type), value (ownedValue.get ()), count (count),
	    ownedValue (std::move (ownedValue)) { }

	const GLint id;
	std::string name;
	UniformType type;
	const void* value;
	int count;
	/** Keeps values copied into this entry alive; external pointer-backed values leave this empty. */
	std::shared_ptr<const void> ownedValue = nullptr;
    };

    class ReferenceUniformEntry {
    public:
	ReferenceUniformEntry (const GLint id, std::string name, UniformType type, const void** value) :
	    id (id), name (std::move (name)), type (type), value (value) { }

	const GLint id;
	std::string name;
	UniformType type;
	const void** value;
    };

    class AttribEntry {
    public:
	AttribEntry (const GLint id, std::string name, GLint type, GLint elements, const GLuint* value) :
	    id (id), name (std::move (name)), type (type), elements (elements), value (value) { }

	const GLint id;
	std::string name;
	GLint type;
	GLint elements;
	const GLuint* value;
    };

    struct TextureAnimationState {
	uint32_t currentTexture = 0;
	glm::vec2 translation = { 0.0f, 0.0f };
	glm::vec4 rotation = { 0.0f, 0.0f, 0.0f, 0.0f };
    };

    void setupShaders ();
    void releaseShaders ();
    void rebuildShaders ();
    [[nodiscard]] const std::string& userTextureValue (const std::string& entry) const;
    [[nodiscard]] TextureMap userTextureValues () const;
    [[nodiscard]] glm::ivec2 fogCombos () const;
    void setupProgramSharing ();
    void leaveProgramSharing ();
    [[nodiscard]] GLuint privateProgram ();
    void setupShaderVariables ();
    void setupUniforms ();
    void setupTextureUniforms ();
    void setupAttributes ();
    void addAttribute (const std::string& name, GLint type, GLint elements, const GLuint* value);
    void addUniform (ShaderVariable* value);
    void addUniform (const ShaderVariable* value, const DynamicValue* setting);
    void addUniform (const ShaderVariable* value, const UserSetting& setting);
    void addUniform (const std::string& name, int value);
    void addUniform (const std::string& name, double value);
    void addUniform (const std::string& name, float value);
    void addUniform (const std::string& name, glm::vec2 value);
    void addUniform (const std::string& name, glm::vec3 value);
    void addUniform (const std::string& name, glm::vec4 value);
    void addUniform (const std::string& name, const glm::mat3& value);
    void addUniform (const std::string& name, glm::mat4 value);
    void addUniform (const std::string& name, const int* value, int count = 1);
    void addUniform (const std::string& name, const double* value, int count = 1);
    void addUniform (const std::string& name, const glm::vec2* value);
    void addUniform (const std::string& name, const int** value);
    void addUniform (const std::string& name, const double** value);
    void addUniform (const std::string& name, const float** value);
    void addUniform (const std::string& name, const glm::vec2** value);
    void addUniform (const std::string& name, const glm::vec3** value);
    void addUniform (const std::string& name, const glm::vec4** value);
    void addUniform (const std::string& name, const glm::mat3** value);
    void addUniform (const std::string& name, const glm::mat4** value);
    template <typename T> void addUniform (const std::string& name, UniformType type, T value);
    template <typename T> void addUniform (const std::string& name, UniformType type, T* value, int count = 1);
    template <typename T> void addUniform (const std::string& name, UniformType type, T** value);

    void setupRenderFramebuffer (const std::shared_ptr<const CFBO>& drawTo) const;
    void setupRenderTexture ();
    [[nodiscard]] std::shared_ptr<const TextureProvider> resolveTexture0 ();
    [[nodiscard]] TextureAnimationState
    resolveTextureAnimationState (const std::shared_ptr<const TextureProvider>& texture) const;
    void bindTextureUnit (int index, const std::shared_ptr<const TextureProvider>& texture, uint32_t frame);
    void bindTextureOverrides (uint32_t currentTexture, std::shared_ptr<const TextureProvider>& texture0);
    void setupRenderUniforms ();
    void refreshUniformBindings ();
    void setupRenderReferenceUniforms ();
    void setupRenderAttributes () const;
    void renderGeometry () const;
    void cleanupRenderSetup ();

    std::shared_ptr<const TextureProvider> resolveTexture (
	std::shared_ptr<const TextureProvider> expected, int index,
	std::shared_ptr<const TextureProvider> previous = nullptr
    );

    CRenderable& m_renderable;
    std::shared_ptr<const FBOProvider> m_fboProvider;
    const MaterialPass& m_pass;
    const TextureMap& m_binds;
    const ImageEffectPassOverride& m_override;
    const TextureMap m_userTextureEntries;
    TextureMap m_userTextureValues;
    std::map<int, std::shared_ptr<const TextureProvider>> m_userTextureProviders;
    std::map<int, int> m_userTextureFormats;
    ComboMap m_runtimeCombos = {};
    std::optional<std::reference_wrapper<std::string>> m_target;
    std::shared_ptr<const EffectFBOBindings> m_effectFBOBindings;
    std::optional<std::string> m_copySource;
    std::map<int, glm::vec4> m_effectTextureResolutions;
    std::map<int, std::shared_ptr<const CFBO>> m_fbos = {};
    std::map<std::string, int> m_combos = {};
    std::vector<std::unique_ptr<AttribEntry>> m_attribs = {};
    std::map<std::string, std::unique_ptr<UniformEntry>> m_uniforms = {};
    std::map<std::string, std::unique_ptr<ReferenceUniformEntry>> m_referenceUniforms = {};
    // Stable draw lists avoid traversing string-keyed trees on every model draw.
    struct DrawUniform {
	const UniformEntry* entry;
	size_t bytes;
    };
    std::vector<DrawUniform> m_drawUniforms;
    bool m_cacheUniformValues = false;
    std::vector<const ReferenceUniformEntry*> m_drawReferenceUniforms;
    std::array<bool, 32> m_activeSamplers = {};
    bool m_uniformBindingsDirty = true;
    struct ExternalUniform {
	UniformType type;
	const void* value;
	int count;
    };
    /** Owner bindings (bones, particle basis, etc.) survive scene shader changes,
     *  including uniforms absent from the initially compiled variant. */
    std::map<std::string, ExternalUniform> m_externalUniforms;
    /** Shader uniforms supplied by authored pass/effect constants. These take precedence over
     *  generic object uniforms with the same GLSL name. */
    std::set<std::string> m_constantUniforms = {};
    struct AnimatedUniform {
	const UserSetting* setting;
	std::unique_ptr<DynamicValue> sampled;
    };
    std::map<std::string, AnimatedUniform> m_animatedUniforms;
    BlendingMode m_blendingmode = BlendingMode_Normal;
    DepthtestMode m_depthtestmode = DepthtestMode_Disabled;
    DepthwriteMode m_depthwritemode = DepthwriteMode_Disabled;
    GLenum m_colorBlendEquation = GL_FUNC_ADD;
    GLenum m_alphaBlendEquation = GL_FUNC_ADD;
    /**
     * Default matrix the pointers below start at. Owners (CImage, CModel, ...) point these at their own
     * matrices before rendering; passes whose owner never does (e.g. plain copy passes) must still read
     * a valid matrix. Leaving them uninitialized made the render depend on heap garbage: it worked on a
     * fresh launch but an in-process wallpaper rebuild (live property change / bg swap) recycled heap
     * memory into new passes and the same scene rendered wrong.
     */
    static const glm::mat4 s_defaultMatrix;
    const glm::mat4* m_modelViewProjectionMatrix = &s_defaultMatrix;
    const glm::mat4* m_modelViewProjectionMatrixInverse = &s_defaultMatrix;
    const glm::mat4* m_modelMatrix = &s_defaultMatrix;
    glm::mat4 m_layerModelMatrix = glm::mat4 (1.0f);
    bool m_usesLayerModelMatrix = false;
    const glm::mat4* m_viewProjectionMatrix = &s_defaultMatrix;
    const glm::mat4* m_effectTextureProjectionMatrix = &s_defaultMatrix;
    const glm::mat4* m_effectTextureProjectionMatrixInverse = &s_defaultMatrix;

    /**
     * Contains the final map of textures to be used
     */
    std::map<int, std::shared_ptr<TextureChainEntry>> m_textures = {};
    /** Stable pointer-backed {1/w, 1/h, w, h} uniforms for every resolved texture slot. */
    std::map<int, glm::vec4> m_textureTexels = {};
    std::vector<std::pair<int, GLint>> m_cookieSamplerBindings;
    /** Whether this pass currently holds usage counts on m_textures, see adjustTextureUsageCounts */
    mutable bool m_texturesUsageCounted = false;

    /**
     * Owns the parsed/preprocessed shader sources for this pass. Complex scenes can
     * create hundreds of passes, so leaking these strings on every wallpaper switch
     * quickly retains hundreds of megabytes.
     */
    std::unique_ptr<Render::Shaders::Shader> m_shader = nullptr;
    bool m_shaderInitialized = false;
    uint64_t m_shaderRevision = 0;
    glm::ivec2 m_fogCombos = {};

    std::shared_ptr<const CFBO> m_drawTo = nullptr;
    std::shared_ptr<const TextureProvider> m_input = nullptr;
    std::shared_ptr<const TextureProvider> m_previousInput = nullptr;
    glm::vec4 m_texture0Resolution = {};
    std::array<float, 10> m_textureMipMapCounts = {};
    glm::vec3 m_screen = { 1, 1, 1 };
    /** Wallpaper-owned projected g_Time value retained for the uniform pointer. */
    float m_sceneTime = 0.0f;
    /** Wallpaper Engine's g_Frametime global. Refresh this from the scene-local clock before
     * every draw so feedback effects advance correctly on independently paced outputs. */
    float m_frameTime = 0.0f;
    glm::vec2 m_sceneTexelSize = { 1, 1 };
    glm::vec2 m_sceneTexelSizeHalf = { 0.5f, 0.5f };
    /**
     * Effective object tint uploaded to shaders that use the newer g_Color4 convention.
     * Those shaders fold opacity into the tint alpha instead of declaring g_UserAlpha,
     * so keep a stable, pointer-backed value and refresh it before every draw.
     */
    glm::vec4 m_effectiveColor4 = {};
    int m_singleSampleCoverageHeight = 0;

    // Location queries only; identical passes share this GL object. Draws use
    // m_sharedProgram, or m_privateProgram once this pass cannot share.
    GLuint m_programID = GL_NONE;
    std::shared_ptr<const Shaders::ShaderProgramCache::SharedProgram> m_interfaceProgram;
    GLuint m_privateProgram = GL_NONE;
    std::shared_ptr<Shaders::ShaderProgramCache::SharingGroup> m_programSharingGroup;
    std::shared_ptr<const Shaders::ShaderProgramCache::SharedProgram> m_sharedProgram;
    bool m_programSharingChecked = false;

    // shader variables used temporary
    GLint g_Texture0Rotation = -1;
    GLint g_Texture0Translation = -1;
    GLuint a_TexCoord = GL_NONE;
    GLuint a_Position = GL_NONE;
    GLuint m_vao = GL_NONE;

    // Custom geometry callbacks (for particles, etc.)
    GeometryCallback m_setupAttribsCallback;
    GeometryCallback m_drawGeometryCallback;
    GeometryCallback m_cleanupAttribsCallback;
};
} // namespace WallpaperEngine::Render::Objects::Effects
