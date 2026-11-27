#pragma once

#include "WallpaperEngine/Render/Camera.h"
#include "WallpaperEngine/Data/JSON.h"

#include "WallpaperEngine/Render/CWallpaper.h"
#include "WallpaperEngine/Render/Objects/ModelData.h"
#include "WallpaperEngine/Scripting/ScriptEngine.h"

#include <array>
#include <chrono>
#include <functional>
#include <limits>
#include <set>
#include <optional>
#include <random>

namespace WallpaperEngine::Render {
enum class RenderSortClass {
    Opaque = 0,
    Translucent = 1,
    Additive = 2,
};

class Camera;
class CObject;
class CTexture;
}

namespace WallpaperEngine::Render::Objects {
class CLight;
class CImage;
namespace Effects {
class CPass;
}
}

namespace WallpaperEngine::Render::Wallpapers {
using namespace WallpaperEngine::Data::Model;

class HdrBloom;
class VolumetricLights;

class CScene final : public CWallpaper {
public:
    CScene (
	const Wallpaper& wallpaper, RenderContext& context, AudioContext& audioContext,
	const WallpaperState::TextureUVsScaling& scalingMode, const uint32_t& clampMode,
	const std::string& screenName = "default", std::function<void (bool)> loadingCheckpoint = {}
    );

    ~CScene () override;

    uint32_t createModelData (
	std::vector<Scripting::ModelDataShape> shapes,
	std::optional<std::pair<glm::vec3, glm::vec3>> bounds = std::nullopt
    );
    void destroyModelData (uint32_t token);
    std::shared_ptr<Objects::ModelData> findModelData (uint32_t token);

    /** Cooperative preparation runs on the render thread. Only enclosing, fully owned
     * construction boundaries may allow cancellation to unwind the scene. */
    void loadingCheckpoint (bool canCancel = false) const;
    [[nodiscard]] bool isLoading () const { return static_cast<bool> (m_loadingCheckpoint); }
    [[nodiscard]] bool isConstructing () const { return m_constructing; }
    [[nodiscard]] double getRenderTime () const;
    /** The captured driver timestamp shared by every object in the current scene frame. */
    [[nodiscard]] double getFrameRenderTime () const {
        return this->m_previousSceneTime.value_or (this->m_loadingRenderTime);
    }
    [[nodiscard]] double getTextureAnimationSceneTime () const;
    [[nodiscard]] std::chrono::steady_clock::time_point getTimerTime () const;
    void prepareFirstFrame ();
    void holdLoading ();
    void finishLoading ();


    [[nodiscard]] Scripting::ScriptEngine& getScriptEngine () const;
    [[nodiscard]] Camera& getCamera () const;

    /** Persistent pose pushed by thisScene.setCameraTransforms(), used when no
     *  visible camera layer or active camera path owns the rendered camera. */
    void setScriptCameraTransform (const CameraTransform& transform);

    [[nodiscard]] const Scene& getScene () const;
    [[nodiscard]] bool isHdr () const { return m_hdrEnabled; }
    [[nodiscard]] uint32_t getTextureReduction () const { return m_textureReduction; }
    [[nodiscard]] TextureFormat getColorFormat () const {
        return m_hdrEnabled ? TextureFormat_RGBA16161616f : TextureFormat_ARGB8888;
    }
    [[nodiscard]] GLuint getWallpaperFramebuffer () const override;
    [[nodiscard]] GLuint getWallpaperTexture () const override;
    /** Authored textures resolve against this scene's assets and user properties. */
    [[nodiscard]] std::shared_ptr<const TextureProvider> resolveTexture (const std::string& name) const;
    void markTextureUsed (const TextureProvider& texture) const;
    [[nodiscard]] std::shared_ptr<const TextureProvider>
    resolveUserTexture (const std::string& name, uint32_t flags, float aspect) const;
    void registerUserTexturePass (Objects::Effects::CPass* pass);
    void unregisterUserTexturePass (Objects::Effects::CPass* pass);
    [[nodiscard]] bool isLightCookieAlias (const TextureProvider* texture) const {
	return texture == m_lightCookieAlias.get ();
    }
    [[nodiscard]] const std::shared_ptr<const TextureProvider>& getLightCookie () const { return m_lightCookie; }
    [[nodiscard]] GLuint getLightCookieSampler ();

    [[nodiscard]] int getWidth () const override;
    [[nodiscard]] int getHeight () const override;
    [[nodiscard]] const glm::vec2& getOutputSize () const;
    /** Static admitted AUTO bounds use a constructor snapshot, never output/camera extents. */
    [[nodiscard]] const std::optional<glm::ivec2>& getAutoCollisionBoundsExtent () const {
        return m_autoCollisionBoundsExtent;
    }
    [[nodiscard]] static std::optional<glm::vec2> prepareStaticAutoCollisionBounds (const Scene& scene);
    /** Previous completed scene, before bloom; allocated only when a material requests it. */
    [[nodiscard]] std::shared_ptr<CFBO> getMipMappedFramebuffer ();
    [[nodiscard]] std::shared_ptr<const CFBO> getLegacyReflectionTarget () const { return m_legacyReflectionTarget; }
    [[nodiscard]] bool supportsLegacyReflection () const;
    [[nodiscard]] bool isRenderingLegacyReflection () const { return m_renderingLegacyReflection; }
    void registerLegacyReflectionPass (Objects::Effects::CPass* pass, bool active);
    void unregisterLegacyReflectionPass (Objects::Effects::CPass* pass);
    /** Keep the producer drawing hidden outputs sampled by material/shader inputs. */
    void retainRenderTarget (const std::shared_ptr<const CFBO>& target);
    [[nodiscard]] bool isRenderTargetRetained (const std::shared_ptr<const CFBO>& target) const {
	return m_sampledRenderTargets.contains (target);
    }
    [[nodiscard]] glm::ivec2 getFramebufferSize () const override;
    [[nodiscard]] WallpaperState::TextureUVs getPresentationUVs () const override;

    // Scene-owned projected clock shared by script runtime, shaders and particle noise.
    // Native rollover resets this clock; newly loaded wallpapers start at zero.
    [[nodiscard]] float getTime () const;
    /** Scene-relative timestamp for subtracting animation intervals before narrowing. */
    [[nodiscard]] double getElapsedTime () const { return this->m_sceneElapsedTime; }
    [[nodiscard]] uint32_t getSceneFrame () const { return this->m_sceneFrame; }
    [[nodiscard]] float getDeltaTime () const;
    [[nodiscard]] float getPreviousDeltaTime () const { return m_previousSceneDeltaTime; }
    [[nodiscard]] float getFps () const;

    /**
     * Wallpaper Engine conventions for camera parallax that are not encoded in wallpaper
     * assets; everything else (amount, delay, influence, per-object depth, locktransforms)
     * comes from scene.json.
     */
    /** Official camera-delay response recovered from wallpaper64.exe. A positive authored
     * delay maps to alpha = (1 - delay / limit) * rate * dt, clamped to [0,1]; zero snaps. */
    static constexpr float PARALLAX_DELAY_LIMIT = 3.0f;
    static constexpr float PARALLAX_DELAY_RATE = 10.0f;
    /** Fraction of each axis' extent a unit-depth layer travels over a full mouse swing
     * (applied per-axis: width for x, height for y) */
    static constexpr float PARALLAX_TRANSLATION_SPAN = 0.5f;

    /** fade.json is fully opaque at a path boundary and clears over 0.5 seconds. */
    static constexpr float CAMERA_FADE_DURATION = 0.5f;

    [[nodiscard]] static float calculateParallaxSmoothingAlpha (float delay, float deltaTime);
    [[nodiscard]] static glm::vec2 calculateShaderParallaxPosition (const glm::vec2& displacement);
    /** Recover the elapsed time for this scene rather than the application loop. Wayland outputs
     * can present independently, so a slower monitor may skip several global render iterations. */
    [[nodiscard]] static float
    calculateSceneDeltaTime (double currentTime, std::optional<double> previousSceneTime);
    /** Pack the authoring start/end values into common_fog.h's
     * {start, range, startDensity, densityRange} uniform layout. */
    [[nodiscard]] static glm::vec4
    calculateFogParams (float start, float end, float startDensity, float endDensity);
    [[nodiscard]] static float calculateCameraFadeAlpha (float elapsedTime, float duration);
    [[nodiscard]] static size_t chooseCameraPathIndex (
	std::optional<size_t> current, size_t count, const std::string& queueMode,
	std::vector<size_t>& remaining, const std::function<uint32_t ()>& nextRandom
    );

    /** Playhead of the shot currently on screen. `state` carries the runtime
     * CameraPathFlags bits (reversed/finished) the authored flags do not. */
    struct CameraPathPlayback {
	float elapsed = 0.0f;
	uint32_t state = 0;
    };
    [[nodiscard]] static CameraPathPlayback
    advanceCameraPath (const CameraPath& path, CameraPathPlayback current, float deltaTime);

    struct TransparentSortKey {
	/** Deferred objects follow the stable first traversal. */
	bool sortable = false;
	/** Local origin projected onto the camera's backward axis. */
	float cameraDepth = 0.0f;
	bool sortLast = false;
	/** Native reuses the previously sorted deferred list for equal depths. */
	size_t previousOrder = std::numeric_limits<size_t>::max ();
    };

    /** Return the stable first traversal followed by deferred objects ordered
     * back-to-front, with flagged fullscreen images last. */
    [[nodiscard]] static std::vector<size_t>
    calculateTransparentSortPermutation (const std::vector<TransparentSortKey>& keys);

    const glm::vec2* getMousePosition () const;
    const glm::vec2* getMousePositionLast () const;
    const glm::vec2* getMousePositionNormalized () const;
    const glm::vec2* getParallaxDisplacement () const;
    const glm::vec2* getParallaxPosition () const;

    [[nodiscard]] const std::vector<CObject*>& getObjectsByRenderOrder () const;
    [[nodiscard]] const std::vector<CObject*>& getObjectsByCursorOrder () const;
    [[nodiscard]] const CObject* getObject (int id) const;

    /** Composition layers with explicit children isolate that subtree. Childless
     * layers instead operate on the already-rendered full-frame stack. */
    [[nodiscard]] static bool hasAuthoredChildren (const SceneData& scene, int parentId);
    [[nodiscard]] bool hasAuthoredChildren (int parentId) const;

    /** Destination selected while a native composition layer renders its child subtree. */
    [[nodiscard]] std::shared_ptr<const CFBO> getActiveRenderTarget () const;
    /** Redirect an ordinary scene destination into the active composition target. */
    [[nodiscard]] std::shared_ptr<const CFBO>
    resolveRenderTarget (const std::shared_ptr<const CFBO>& requested) const;
    [[nodiscard]] bool isRenderingToComposition () const;
    /** Non-copying composition subtrees union blended child alpha, including nested groups. */
    [[nodiscard]] bool usesCompositionAlphaMax () const;

    /**
     * Per-frame light state for 3D scenes, laid out to match the LightingV1
     * uniform contract (see ShaderUnit::generateLightingV1). Counts are fixed
     * at load time so the vectors' storage stays valid for uniform pointers.
     */
    struct SceneLights {
	int directionalCount = 0;
	int directionalShadowCount = 0;
	int pointCount = 0;
	int pointShadowCount = 0;
	int spotCount = 0;
	int spotShadowCount = 0;
	int tubeCount = 0;
	/** Matrix features: three per shadowed directional light and one per shadowed spot. */
	int shadowFeatureCount = 0;
	/** Total shadow views, including six faces per shadowed point light. */
	int shadowViewCount = 0;
	uint32_t directionalShadowMask = 0;
	uint32_t pointShadowMask = 0;
	uint32_t spotShadowMask = 0;
	/** xyz = world-space direction towards the light, w unused */
	std::vector<glm::vec4> directionalDirections = {};
	/** rgb = color premultiplied by intensity, w unused */
	std::vector<glm::vec4> directionalColors = {};
	/** Three atlas feature indices per directional light, or -1 when unshadowed. */
	std::vector<glm::ivec3> directionalShadowFeatures = {};
	std::vector<float> directionalShadowEnabled = {};
	/** xyz = world-space position, w = falloff exponent */
	std::vector<glm::vec4> pointOrigins = {};
	/** rgb = color premultiplied by intensity, w = radius */
	std::vector<glm::vec4> pointColors = {};
	/** Fixed four-light interface used by older generic model shaders. */
	std::array<glm::vec3, 4> legacyPositions = {};
	std::array<glm::vec4, 4> legacyColors = {};
	/** Four PBR light colors packed into three vec4s; the fourth RGB occupies w. */
	std::array<glm::vec4, 3> legacyPremultipliedColors = {};
	void updateLegacyPointLights ();
	/** Native compact projection and 2x3 atlas block data, indexed like point arrays. */
	std::vector<glm::vec4> pointShadowProjections = {};
	std::vector<glm::vec4> pointShadowTransforms = {};
	std::vector<float> pointShadowEnabled = {};
	std::vector<std::array<glm::mat4, 6>> pointShadowMatrices = {};
	std::vector<std::array<glm::ivec4, 6>> pointShadowViewports = {};
	/** xyz = world-space position, w = cos(inner cone angle) */
	std::vector<glm::vec4> spotOrigins = {};
	/** xyz = the spotlight's scaled forward axis, w = cos(outer cone angle) */
	std::vector<glm::vec4> spotDirections = {};
	/** rgb = color premultiplied by intensity, w = radius */
	std::vector<glm::vec4> spotColors = {};
	/** x = falloff exponent, yzw unused */
	std::vector<glm::vec4> spotExponents = {};
	uint32_t spotCookieMask = 0;
	std::vector<glm::mat4> spotCookieMatrices = {};
	/** Atlas feature index per spotlight, or -1 when the light does not cast shadows. */
	std::vector<int> spotShadowFeatures = {};
	std::vector<float> spotShadowEnabled = {};
	/** Unified feature arrays uploaded under Wallpaper Engine's g_LFeature contract. */
	std::vector<glm::mat4> shadowMatrices = {};
	std::vector<glm::vec4> shadowTransforms = {};
	std::vector<float> shadowFeatureEnabled = {};
	/** Pixel viewports inside the depth atlas; not uploaded to material shaders. */
	std::vector<glm::ivec4> shadowViewports = {};
	/** xyz = first world-space endpoint, w = falloff exponent */
	std::vector<glm::vec4> tubeOriginsA = {};
	/** xyz = second world-space endpoint, w unused */
	std::vector<glm::vec4> tubeOriginsB = {};
	/** rgb = color premultiplied by intensity, w = radius */
	std::vector<glm::vec4> tubeColors = {};
    };

    [[nodiscard]] const SceneLights& getLights () const;
    /** Per-type shader slot in authored order. Runtime lights have no slot. */
    [[nodiscard]] std::optional<int> getAuthoredLightSlot (const int id) const {
	const auto slot = this->m_authoredLightSlots.find (id);
	if (slot == this->m_authoredLightSlots.end ()) return std::nullopt;
	return slot->second;
    }
    void applyLightCombos (ComboMap& combos) const;

    struct SceneFog {
	bool distanceEnabled = false;
	bool heightEnabled = false;
	glm::vec4 distanceParams = {};
	glm::vec4 heightParams = {};
    };

    [[nodiscard]] const SceneFog& getFog () const;

    // Runtime layer API — backs thisScene.createLayer()/getLayerIndex()/sortLayer() in the
    // scripting engine. Audio visualizers (and other generative scripts) spawn their bar layers
    // at init() time via these; without them the controlling script throws and the placeholder
    // template renders as a static block.
    //
    // createLayer accepts an image, model, particle or sound asset path, or a serialized layer
    // configuration. Asset paths also resolve in the calling script's workshop scope.
    // destroyLayer removes a layer at the next frame boundary, after the current script returns.
    Render::CObject* createLayer (const std::string& modelPath, const std::string& workshopId);
    Render::CObject* createLayer (Data::JSON::JSON configuration, const std::string& workshopId);
    bool destroyLayer (CObject* layer);
    // Index of a layer within the scriptable-layer subset of the render order (matches getLayer()/
    // getLayerCount()), or -1 if not present.
    [[nodiscard]] int getScriptableLayerIndex (const CObject* layer) const;
    // Move a layer so it sits at the given scriptable-layer index in the render order (z-order).
    void moveLayerToScriptableIndex (CObject* layer, int index);

protected:
    void renderFrame (const glm::ivec4& viewport) override;
    void onFirstFrameSubmitted () override;
    void updateMouse (const glm::ivec4& viewport);
    [[nodiscard]] float getSceneFadeAlpha () const override;

    friend class CWallpaper;

private:
    const char* validateModelDataMaterial (const Scripting::ModelDataShape& shape, bool allowUnknownSignature = false) const;
    struct VideoActivity {
	std::weak_ptr<const CTexture> texture;
	uint32_t lastUsedFrame = 0;
    };
    mutable std::map<const TextureProvider*, VideoActivity> m_videoActivity;
    uint32_t m_sceneFrame = 0;
    void updateVideoActivity ();
    mutable std::map<std::string, std::shared_ptr<const CTexture>> m_userTextures;
    std::vector<Objects::Effects::CPass*> m_userTexturePasses;
    std::set<std::weak_ptr<const CFBO>, std::owner_less<>> m_sampledRenderTargets;
    /** Release raw-owned render objects, including during constructor unwinding. */
    void destroyObjects () noexcept;
    void destroyQueuedLayers ();
    Render::CObject* createObject (const Object& object);
    Render::CObject* dispatchObjectType (const Object& object);
    void addObjectToRenderOrder (const Object& object);
    void updateShadowAtlasLayout ();
    void updateLightState ();
    void renderShadowAtlas ();
    void registerFogScripts ();
    void updateFogState ();
    void initializeBloom ();
    void updateBloomState ();
    bool updateCameraObject ();
    void updateCameraPath (float deltaTime);
    [[nodiscard]] const CameraPathSource* findActiveCameraPathSource () const;
    struct FrameRenderEntry {
	CObject* object;
    };
    [[nodiscard]] std::vector<FrameRenderEntry> buildFrameRenderOrder ();

    std::unique_ptr<Scripting::ScriptEngine> m_scriptEngine;
    std::unique_ptr<Camera> m_camera;
    glm::vec2 m_outputSize = { 1, 1 };
    std::optional<glm::ivec2> m_autoCollisionBoundsExtent;
    std::shared_ptr<CFBO> m_mipMappedFramebuffer;
    std::shared_ptr<CFBO> m_legacyReflectionTarget;
    std::vector<Objects::Effects::CPass*> m_legacyReflectionPasses;
    std::vector<Objects::Effects::CPass*> m_legacyReflectionShaderPasses;
    bool m_renderingLegacyReflection = false;
    void renderLegacyReflection (const glm::vec3& clearColor);
    void updateOutputSize (const glm::ivec4& viewport);
    bool m_hdrEnabled = false;
    /** Selected once before object construction, independent of later output target resizes. */
    uint32_t m_textureReduction = 1;
    /** Keep explicit material dependencies alive for later runtime-created layers. */
    std::vector<std::shared_ptr<const TextureProvider>> m_precachedTextures;
    /** Keep the native-selected material shader resources alive until this scene unloads. */
    std::vector<std::shared_ptr<const Shaders::ShaderProgramCache::SharedProgram>> m_precachedPrograms;
    std::unique_ptr<HdrBloom> m_hdrBloom;
    ObjectUniquePtr m_bloomObjectData;
    CObject* m_bloomObject = nullptr;
    bool m_bloomSetupAttempted = false;
    // Keeps runtime-created layer data alive while its renderer refers to it.
    std::vector<ObjectUniquePtr> m_runtimeLayerData = {};
    Objects::ModelDataStore m_modelData;
    std::set<int> m_layersToDestroy;
    std::set<int> m_objectsInCreation = {};
    std::map<int, CObject*> m_objects = {};
    std::vector<CObject*> m_objectsByRenderOrder = {};
    std::map<CObject*, size_t> m_deferredRenderOrder;
    /** Authored stacking order, independent of dependency-driven render scheduling. */
    std::vector<CObject*> m_objectsByCursorOrder = {};
    std::vector<Objects::CLight*> m_lightObjects = {};
    /** Per-type uniform indices assigned when the authored shader masks are built. */
    std::map<int, int> m_authoredLightSlots;
    std::vector<const Objects::CLight*> m_shadowLightOrder;
    SceneLights m_lights = {};
    std::unique_ptr<VolumetricLights> m_volumetricLights;
    SceneFog m_fog = {};
    std::vector<DynamicValue*> m_scriptedValues = {};
    glm::vec2 m_mousePosition = {};
    glm::vec2 m_mousePositionLast = {};
    glm::vec2 m_mousePositionNormalized = {};
    glm::vec2 m_parallaxDisplacement = {};
    /** Parallax position fed to shaders via g_ParallaxPosition, 0.5,0.5 = centered */
    glm::vec2 m_parallaxPosition = { 0.5f, 0.5f };
    bool m_constructing = true;
    std::function<void (bool)> m_loadingCheckpoint;
    float m_loadingRenderTime = 0.0f;
    std::chrono::steady_clock::time_point m_loadingTimerTime;
    std::optional<double> m_previousSceneTime = std::nullopt;
    std::optional<double> m_textureAnimationStart;
    float m_sceneDeltaTime = 0.0f;
    float m_previousSceneDeltaTime = 0.0f;
    double m_sceneElapsedTime = 0.0;
    double m_sceneTime = 0.0;
    const CameraPathSource* m_activeCameraPathSource = nullptr;
    struct CameraPathSourceState {
	std::optional<size_t> index;
	std::vector<size_t> remaining;
	CameraPathPlayback playback;
    };
    /** Nonselected sources retain independent queues; hidden sources park. */
    std::map<const CameraPathSource*, CameraPathSourceState> m_cameraPathSourceStates;
    std::optional<size_t> m_activeCameraPathIndex = std::nullopt;
    std::vector<size_t> m_remainingCameraPaths;
    float m_cameraPathElapsed = 0.0f;
    /** Runtime playback bits for the active shot (reversed/finished). */
    uint32_t m_cameraPathState = 0;
    std::optional<CameraTransform> m_scriptCameraTransform = std::nullopt;
    std::mt19937 m_cameraPathRandom { std::random_device {} () };
    std::shared_ptr<const CFBO> _rt_4FrameBuffer = nullptr;
    std::shared_ptr<const CFBO> _rt_8FrameBuffer = nullptr;
    std::shared_ptr<const CFBO> _rt_Bloom = nullptr;
    std::shared_ptr<const CFBO> _rt_shadowAtlas = nullptr;
    GLint m_shadowAtlasMaxExtent = 0;
    std::shared_ptr<const CFBO> m_lightCookieAlias;
    std::shared_ptr<const TextureProvider> m_lightCookie;
    GLuint m_lightCookieSampler = GL_NONE;
    std::shared_ptr<const CFBO> m_compositionRenderTarget = nullptr;
    bool m_compositionAlphaMax = false;

    static constexpr int SHADOW_FEATURE_SIZE = 256;
    static constexpr int SHADOW_ATLAS_MAX_WIDTH = 8192;
};
} // namespace WallpaperEngine::Render::Wallpaper
