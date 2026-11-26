#pragma once

#include "WallpaperEngine/Render/CObject.h"
#include "WallpaperEngine/Scripting/ScriptableObject.h"

#include <array>

namespace WallpaperEngine::Render::Objects {
using namespace WallpaperEngine::Scripting;

/**
 * Light source in a 3D scene. Draws nothing itself; CScene reads its
 * world-space state every frame to feed the lighting uniforms. Deriving from
 * ScriptableObject keeps script-driven origins/angles ticking (e.g. a sun
 * position script).
 */
class CLight final : public ScriptableObject {
public:
    CLight (Wallpapers::CScene& scene, const Light& light);
    ~CLight () override;

    /** Resolve after layer composites exist; retain video and dynamic providers. */
    void initializeCookie ();
    [[nodiscard]] const std::shared_ptr<const TextureProvider>& getCookie () const { return m_cookie; }
    [[nodiscard]] glm::mat4 getCookieProjection (bool reverseDepth = false) const;

    [[nodiscard]] const Light& getLight () const;

    /** world-space position (point lights) */
    [[nodiscard]] glm::vec3 getWorldPosition () const;
    /** world-space direction the light points at (directional lights); +X at zero angles */
    [[nodiscard]] glm::vec3 getWorldDirection () const;
    /** Native spot lighting retains authored and inherited scale in its forward axis. */
    [[nodiscard]] glm::vec3 getWorldForwardAxis () const;
    /** world-space second endpoint of a tube light */
    [[nodiscard]] glm::vec3 getTubeEndPosition () const;
    /** light color premultiplied by intensity, zeroed while the light is invisible */
    [[nodiscard]] glm::vec3 getPremultipliedColor () const;

    /** Native spot uniform packing: cos(inner degrees), cos(outer degrees). */
    [[nodiscard]] static glm::vec2 calculateSpotConeCosines (float innerDegrees, float outerDegrees);
    /** Shared spot/cookie projection retaining authored roll and scale-independent axes. */
    [[nodiscard]] static glm::mat4 calculateSpotProjection (
        const glm::mat4& world, float outerDegrees, float radius, bool orthographic, bool reverseDepth
    );
    /** Conventional light-space matrix used to construct the volumetric cone. */
    [[nodiscard]] static glm::mat4 calculateSpotShadowViewProjection (
	const glm::vec3& origin, const glm::vec3& direction, float outerDegrees, float radius
    );
    /** Native square cascade using the current camera and full authored light basis. */
    [[nodiscard]] glm::mat4 getDirectionalShadowProjection (
        const glm::vec3& cameraEye, const glm::mat4& cameraView, int cascade, int shadowResolution
    ) const;
    [[nodiscard]] static glm::mat4 calculateDirectionalShadowProjection (
        const glm::vec3& cameraEye, const glm::mat4& cameraView, const glm::mat4& lightWorld,
        const glm::vec3& cascadeDistances, int cascade, int shadowResolution, bool orthographic = false
    );
    /** Stable orthographic light-space matrix enclosing a nested camera-frustum cascade. */
    [[nodiscard]] static glm::mat4 calculateDirectionalShadowViewProjection (
	const glm::vec3& cameraEye, const glm::vec3& cameraCenter, const glm::vec3& cameraUp,
	float fieldOfViewDegrees, float aspectRatio, float zoom, float nearDistance, float farDistance,
	const glm::vec3& lightDirection, int shadowResolution, bool reverseDepth = false
    );
    /** Six +X/-X/+Y/-Y/+Z/-Z view-projections used by the native 2x3 point atlas block. */
    [[nodiscard]] static std::array<glm::mat4, 6>
    calculatePointShadowViewProjections (const glm::vec3& origin, float radius, float sourceSize = 0.0f,
	bool orthographic = false, bool reverseDepth = false);
    /** Compact perspective coefficients consumed by CalculateProjectedCoordsPoint. */
    [[nodiscard]] static glm::vec4 calculatePointShadowProjectionInfo (float radius, float sourceSize = 0.0f,
	bool orthographic = false, bool reverseDepth = false);
    /** Transform a tube's authored local control point into its world-space endpoint. */
    [[nodiscard]] static glm::vec3
    calculateTubeEndPosition (const glm::mat4& worldMatrix, const glm::vec3& controlPoint);

private:
    [[nodiscard]] glm::mat4 resolveLightMatrix () const;
    const Light& m_light;
    std::shared_ptr<const TextureProvider> m_cookie;
};
} // namespace WallpaperEngine::Render::Objects
