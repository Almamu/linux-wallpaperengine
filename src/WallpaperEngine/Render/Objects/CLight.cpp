#include "CLight.h"
#include "WallpaperEngine/Data/Model/DynamicValue.h"
#include "WallpaperEngine/Render/CTexture.h"
#include "WallpaperEngine/Render/Depth.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <limits>
#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>

using namespace WallpaperEngine;
using namespace WallpaperEngine::Render::Objects;

CLight::CLight (Wallpapers::CScene& scene, const Light& light) :
    CObject (scene, light), ScriptableObject (scene, light), m_light (light) {
    this->registerProperty ("color", *light.color);
    this->registerProperty ("intensity", *light.intensity);
    this->registerProperty ("radius", *light.radius);
    this->registerProperty ("lightsourcesize", *light.sourceSize);
    this->registerProperty ("exponent", *light.exponent);
    this->registerProperty ("innercone", *light.innerCone);
    this->registerProperty ("outercone", *light.outerCone);
    this->registerProperty ("controlpoint", *light.controlPoint);
    this->registerProperty ("castvolumetrics", *light.castVolumetrics);
    this->registerProperty ("density", *light.density);
    this->registerProperty ("volumetricsexponent", *light.volumetricsExponent);
}

const Light& CLight::getLight () const { return this->m_light; }

CLight::~CLight () {
    if (m_cookie) m_cookie->decrementUsageCount ();
}

void CLight::initializeCookie () {
    if (m_cookie || m_light.type != LightData::Type_Spot || !m_light.useCookie) return;
    const auto& name = m_light.cookie;
    if (!name.empty ()) {
	if (name.starts_with ("_rt_") || name.starts_with ("_alias_")) {
	    m_cookie = getScene ().find (name);
	    if (const auto target = std::dynamic_pointer_cast<const CFBO> (m_cookie)) {
		getScene ().retainRenderTarget (target);
	    }
	} else {
	    try { m_cookie = getScene ().resolveTexture (name); }
	    catch (const std::runtime_error& error) {
		sLog.error ("Cannot load light cookie ", name, ": ", error.what ());
	    }
	}
    }
    if (!name.empty () && !m_cookie) {
	// Native keeps a missing named texture distinct from an empty selection:
	// a nearest-filtered 6x6 black/yellow checker, not the flashlight default.
	auto texture = std::make_unique<Texture> ();
	texture->format = TextureFormat_ARGB8888;
	texture->flags = TextureFlags_NoInterpolation;
	texture->width = texture->height = texture->textureWidth = texture->textureHeight = 6;
	texture->imageCount = 1;
	auto mip = std::make_shared<Mipmap> ();
	mip->width = mip->height = 6;
	mip->uncompressedSize = 6 * 6 * 4;
	mip->uncompressedData = std::make_unique<char[]> (mip->uncompressedSize);
	for (int y = 0; y < 6; ++y) for (int x = 0; x < 6; ++x) {
	    auto* pixel = mip->uncompressedData.get () + (y * 6 + x) * 4;
	    pixel[0] = pixel[1] = (x + y) % 2 ? static_cast<char> (255) : 0;
	    pixel[2] = 0;
	    pixel[3] = static_cast<char> (255);
	}
	texture->images[0].push_back (std::move (mip));
	m_cookie = std::make_shared<CTexture> (getScene ().getContext (), std::move (texture), getScene ().getTextureReduction ());
    }
    if (!m_cookie) m_cookie = getScene ().resolveTexture ("cookie/flashlight1");
    m_cookie->incrementUsageCount ();
}

glm::mat4 CLight::getCookieProjection (const bool reverseDepth) const {
    return calculateSpotProjection (resolveLightMatrix (), m_light.outerCone->value->getFloat (),
        m_light.radius->value->getFloat (), getScene ().getCamera ().isOrthogonal (), reverseDepth);
}

glm::mat4 CLight::calculateSpotProjection (
    const glm::mat4& world, const float outerDegrees, const float radius,
    const bool orthographic, const bool reverseDepth
) {
    const auto normalize = [reverseDepth] (const glm::vec3& axis) {
        if (!reverseDepth) return glm::normalize (axis);
        // Native14025d420 uses this inverse-square-root approximation once,
        // even for unit axes; exact normalization changes spot shadow depth.
        const float squaredLength = glm::dot (axis, axis);
        float inverse = std::bit_cast<float> (0x5f375a86u - (std::bit_cast<uint32_t> (squaredLength) >> 1));
        inverse *= 1.5f - squaredLength * 0.5f * inverse * inverse;
        return axis * inverse;
    };
    const glm::vec3 right = normalize (glm::vec3 (world[2]));
    const glm::vec3 up = normalize (glm::vec3 (world[1]));
    const glm::vec3 back = -normalize (glm::vec3 (world[0]));
    const glm::vec3 origin (world[3]);
    const glm::mat4 view (
	glm::vec4 (right.x, up.x, back.x, 0), glm::vec4 (right.y, up.y, back.y, 0),
	glm::vec4 (right.z, up.z, back.z, 0),
	glm::vec4 (-glm::dot (right, origin), -glm::dot (up, origin), -glm::dot (back, origin), 1)
    );
    const float nearPlane = orthographic ? 1.0f : 0.05f;
    const float farPlane = std::max (radius, nearPlane + 0.01f);
    const float fieldOfView = glm::radians (2.0f * outerDegrees);
    return (reverseDepth ? reversePerspective (fieldOfView, 1.0f, nearPlane, farPlane)
        : glm::perspective (fieldOfView, 1.0f, nearPlane, farPlane)) * view;
}

glm::mat4 CLight::resolveLightMatrix () const {
    glm::mat4 world = this->resolveWorldMatrix ();
    const auto& camera = this->getScene ().getCamera ();
    if (camera.isOrthogonal ()) {
	// Match image/model coordinates: authored top-left, Y-down to centered, Y-up.
	const glm::mat4 sceneToRender = glm::translate (
	    glm::mat4 (1.0f), glm::vec3 (-camera.getWidth () * 0.5f, camera.getHeight () * 0.5f, 0.0f)
	) * glm::scale (glm::mat4 (1.0f), glm::vec3 (1.0f, -1.0f, 1.0f));
	world = sceneToRender * world;
    }
    return world;
}

glm::vec3 CLight::getWorldPosition () const {
    return glm::vec3 (this->resolveLightMatrix ()[3]);
}

glm::vec3 CLight::getWorldDirection () const {
    // base forward is +X: matches the sun tracking script in workshop scene 3589454154,
    // which steers the light with a yaw of (-atan2(z, x) - 180°) towards the origin
    return glm::normalize (this->getWorldForwardAxis ());
}

glm::vec3 CLight::getWorldForwardAxis () const {
    return glm::vec3 (this->resolveLightMatrix ()[0]);
}

glm::vec3 CLight::getTubeEndPosition () const {
    return calculateTubeEndPosition (
	this->resolveLightMatrix (), this->m_light.controlPoint->value->getVec3 ()
    );
}

glm::vec3 CLight::getPremultipliedColor () const {
    if (!this->m_light.groupVisible->value->getBool () || !this->isVisibleThroughParents ()) {
	return glm::vec3 (0.0f);
    }

    const double time = getScene ().getElapsedTime ();
    return m_light.color->evaluateVec3 (time) * m_light.intensity->evaluateFloat (time);
}

glm::vec2 CLight::calculateSpotConeCosines (const float innerDegrees, const float outerDegrees) {
    constexpr float degreesToRadians = 0.01745329251994329577f;
    return { std::cos (innerDegrees * degreesToRadians), std::cos (outerDegrees * degreesToRadians) };
}

glm::mat4 CLight::calculateSpotShadowViewProjection (
    const glm::vec3& origin, const glm::vec3& direction, const float outerDegrees, const float radius
) {
    const float directionLength = glm::length (direction);
    const glm::vec3 forward
	= directionLength > 1e-6f ? direction / directionLength : glm::vec3 (1.0f, 0.0f, 0.0f);
    // lookAt becomes singular when forward and up are parallel. Switch axes for
    // near-vertical authored lights while retaining a stable orientation otherwise.
    const glm::vec3 up = std::abs (glm::dot (forward, glm::vec3 (0.0f, 1.0f, 0.0f))) > 0.99f
	? glm::vec3 (0.0f, 0.0f, 1.0f)
	: glm::vec3 (0.0f, 1.0f, 0.0f);
    const float farPlane = std::max (std::abs (radius), 1.01f);
    const float nearPlane = std::clamp (farPlane * 0.001f, 0.01f, farPlane * 0.1f);
    const float fieldOfView = std::clamp (std::abs (outerDegrees) * 2.0f, 1.0f, 179.0f);

    return glm::perspective (glm::radians (fieldOfView), 1.0f, nearPlane, farPlane)
	* glm::lookAt (origin, origin + forward, up);
}

glm::mat4 CLight::getDirectionalShadowProjection (
    const glm::vec3& cameraEye, const glm::mat4& cameraView, const int cascade, const int shadowResolution
) const {
    return calculateDirectionalShadowProjection (
        cameraEye, cameraView, resolveLightMatrix (), m_light.cascadeDistances, cascade, shadowResolution,
        getScene ().getCamera ().isOrthogonal ()
    );
}

glm::mat4 CLight::calculateDirectionalShadowProjection (
    const glm::vec3& cameraEye, const glm::mat4& cameraView, const glm::mat4& lightWorld,
    const glm::vec3& cascadeDistances, const int cascade, const int shadowResolution, const bool orthographic
) {
    const float width = cascadeDistances[cascade];
    const float depth = cascade < 2 ? 4.0f * cascadeDistances.y
        : std::max (1.5f * cascadeDistances.z, 4.0f * cascadeDistances.y);
    const glm::vec3 lightX = glm::vec3 (glm::normalize (lightWorld[0]));
    const glm::vec3 forward (-cameraView[0][2], -cameraView[1][2], -cameraView[2][2]);
    glm::vec3 center = cameraEye + (forward - 0.5f * glm::dot (forward, lightX) * lightX) * (width * 0.5f);
    if (orthographic) center.z = 0.0f;
    const glm::vec3 authoredY (lightWorld[1]), authoredZ (lightWorld[2]);
    const float texel = width / static_cast<float> (shadowResolution);
    // Both native remainders use the original center and unnormalized authored axes.
    const float offsetY = std::fmod (glm::dot (authoredY, center), texel);
    const float offsetZ = std::fmod (glm::dot (authoredZ, center), texel);
    center -= authoredY * offsetY;
    center -= authoredZ * offsetZ;

    const auto normalize = [] (const glm::vec3& axis) {
        const float squaredLength = glm::dot (axis, axis);
        float inverse = std::bit_cast<float> (0x5f375a86u - (std::bit_cast<uint32_t> (squaredLength) >> 1));
        inverse *= 1.5f - squaredLength * 0.5f * inverse * inverse;
        return axis * inverse;
    };
    const glm::vec3 right = normalize (authoredZ), up = normalize (authoredY);
    const glm::vec3 back = normalize (-glm::vec3 (lightWorld[0]));
    const glm::mat4 view (
        glm::vec4 (right.x, up.x, back.x, 0), glm::vec4 (right.y, up.y, back.y, 0),
        glm::vec4 (right.z, up.z, back.z, 0),
        glm::vec4 (-glm::dot (right, center), -glm::dot (up, center), -glm::dot (back, center), 1)
    );
    glm::mat4 projection (1.0f);
    projection[0][0] = projection[1][1] = 2.0f / width;
    projection[2][2] = 1.0f / depth;
    projection[3][2] = 0.5f;
    return projection * view;
}

glm::mat4 CLight::calculateDirectionalShadowViewProjection (
    const glm::vec3& cameraEye, const glm::vec3& cameraCenter, const glm::vec3& cameraUp,
    const float fieldOfViewDegrees, const float aspectRatio, const float zoom, const float nearDistance,
    const float farDistance, const glm::vec3& lightDirection, const int shadowResolution, const bool reverseDepth
) {
    const auto normalizedOr = [] (const glm::vec3& value, const glm::vec3& fallback) {
	const float length = glm::length (value);
	return length > 1e-6f ? value / length : fallback;
    };

    const glm::vec3 cameraForward
	= normalizedOr (cameraCenter - cameraEye, glm::vec3 (0.0f, 0.0f, -1.0f));
    glm::vec3 cameraRight = glm::cross (cameraForward, cameraUp);
    if (glm::length (cameraRight) <= 1e-6f) {
	const glm::vec3 alternateUp = std::abs (cameraForward.y) > 0.99f ? glm::vec3 (0.0f, 0.0f, 1.0f)
									 : glm::vec3 (0.0f, 1.0f, 0.0f);
	cameraRight = glm::cross (cameraForward, alternateUp);
    }
    cameraRight = glm::normalize (cameraRight);
    const glm::vec3 correctedCameraUp = glm::normalize (glm::cross (cameraRight, cameraForward));

    const float cascadeNear = std::max (nearDistance, 0.0001f);
    const float cascadeFar = std::max (farDistance, cascadeNear + 0.0001f);
    const float safeZoom = std::max (zoom, 0.0001f);
    const float tanHalfFov
	= std::tan (glm::radians (std::clamp (fieldOfViewDegrees, 1.0f, 179.0f)) * 0.5f) / safeZoom;
    const float safeAspect = std::max (std::abs (aspectRatio), 0.0001f);

    std::array<glm::vec3, 8> corners {};
    int corner = 0;
    for (const float distance : { cascadeNear, cascadeFar }) {
	const glm::vec3 planeCenter = cameraEye + cameraForward * distance;
	const float halfHeight = tanHalfFov * distance;
	const float halfWidth = halfHeight * safeAspect;
	for (const float y : { -1.0f, 1.0f }) {
	    for (const float x : { -1.0f, 1.0f }) {
		corners[corner++]
		    = planeCenter + cameraRight * (x * halfWidth) + correctedCameraUp * (y * halfHeight);
	    }
	}
    }

    glm::vec3 cascadeCenter (0.0f);
    for (const glm::vec3& point : corners) {
	cascadeCenter += point;
    }
    cascadeCenter /= static_cast<float> (corners.size ());

    float radius = 0.0f;
    for (const glm::vec3& point : corners) {
	radius = std::max (radius, glm::length (point - cascadeCenter));
    }
    radius = std::max (radius, 0.001f);

    const glm::vec3 rayDirection = normalizedOr (lightDirection, glm::vec3 (1.0f, 0.0f, 0.0f));
    const glm::vec3 lightUp = std::abs (glm::dot (rayDirection, glm::vec3 (0.0f, 1.0f, 0.0f))) > 0.99f
	? glm::vec3 (0.0f, 0.0f, 1.0f)
	: glm::vec3 (0.0f, 1.0f, 0.0f);
    const glm::vec3 lightEye = cascadeCenter - rayDirection * radius * 2.0f;
    const glm::mat4 lightView = glm::lookAt (lightEye, cascadeCenter, lightUp);

    glm::vec3 minimum (std::numeric_limits<float>::max ());
    glm::vec3 maximum (std::numeric_limits<float>::lowest ());
    for (const glm::vec3& point : corners) {
	const glm::vec3 lightPoint = glm::vec3 (lightView * glm::vec4 (point, 1.0f));
	minimum = glm::min (minimum, lightPoint);
	maximum = glm::max (maximum, lightPoint);
    }

    float halfWidth = std::max ((maximum.x - minimum.x) * 0.5f, 0.0001f);
    float halfHeight = std::max ((maximum.y - minimum.y) * 0.5f, 0.0001f);
    float centerX = (minimum.x + maximum.x) * 0.5f;
    float centerY = (minimum.y + maximum.y) * 0.5f;
    const float resolution = static_cast<float> (std::max (shadowResolution, 1));
    const float texelX = halfWidth * 2.0f / resolution;
    const float texelY = halfHeight * 2.0f / resolution;
    centerX = std::round (centerX / texelX) * texelX;
    centerY = std::round (centerY / texelY) * texelY;
    // A one-texel margin keeps snapped bounds from clipping an extreme corner.
    halfWidth += texelX;
    halfHeight += texelY;

    const float lightNear = std::max (0.0001f, -maximum.z - radius);
    const float lightFar = std::max (lightNear + 0.0001f, -minimum.z + radius);
    glm::mat4 lightProjection = glm::ortho (
	centerX - halfWidth, centerX + halfWidth, centerY - halfHeight, centerY + halfHeight, lightNear,
	lightFar
    );
    if (reverseDepth) {
        // Retain cascade fitting/texel snapping; form reverse Z directly.
        lightProjection[2][2] = 1.0f / (lightFar - lightNear);
        lightProjection[3][2] = lightFar / (lightFar - lightNear);
    }
    // Snap a fixed world-space origin to the shadow texel grid. Snapping only
    // the light-space bounds is insufficient because lightView follows the
    // camera and would otherwise reintroduce sub-texel projection movement.
    const glm::mat4 unsnapped = lightProjection * lightView;
    const glm::vec4 shadowOrigin = unsnapped * glm::vec4 (0.0f, 0.0f, 0.0f, 1.0f) * (resolution * 0.5f);
    const glm::vec2 roundingOffset
	= (glm::round (glm::vec2 (shadowOrigin)) - glm::vec2 (shadowOrigin)) * (2.0f / resolution);
    lightProjection[3][0] += roundingOffset.x;
    lightProjection[3][1] += roundingOffset.y;
    return lightProjection * lightView;
}

std::array<glm::mat4, 6>
CLight::calculatePointShadowViewProjections (
    const glm::vec3& origin, const float radius, const float sourceSize, const bool orthographic, const bool reverseDepth
) {
    // Native excludes geometry within the point source from its shadow map.
    const float nearPlane = orthographic ? 1.0f : std::max (sourceSize, 0.05f);
    const float farPlane = std::max (radius, nearPlane + 0.01f);
    // LIGHTS_SHADOW_MAPPING_QUALITY is 2: native uses a 94 degree face.
    glm::mat4 projection = reverseDepth
        ? reversePerspective (glm::radians (94.0f), 1.0f, nearPlane, farPlane)
        : glm::perspective (glm::half_pi<float> (), 1.0f, nearPlane, farPlane);
    // Native140190c80 biases caster clip Z before constructing all six faces.
    // The compact sampling coefficients below remain unbiased.
    if (reverseDepth) projection[3][2] -= 0.00333f;

    // This is the face order hard-coded by CalculateProjectedCoordsPoint:
    // +X/-X on row 0, +Y/-Y on row 1, and +Z/-Z on row 2.
    static constexpr std::array<glm::vec3, 6> directions = {
	glm::vec3 (1.0f, 0.0f, 0.0f), glm::vec3 (-1.0f, 0.0f, 0.0f),
	glm::vec3 (0.0f, 1.0f, 0.0f), glm::vec3 (0.0f, -1.0f, 0.0f),
	glm::vec3 (0.0f, 0.0f, 1.0f), glm::vec3 (0.0f, 0.0f, -1.0f),
    };
    static constexpr std::array<glm::vec3, 6> up = {
	glm::vec3 (0.0f, 1.0f, 0.0f), glm::vec3 (0.0f, 1.0f, 0.0f),
	glm::vec3 (0.0f, 0.0f, 1.0f), glm::vec3 (0.0f, 0.0f, -1.0f),
	glm::vec3 (0.0f, 1.0f, 0.0f), glm::vec3 (0.0f, 1.0f, 0.0f),
    };

    std::array<glm::mat4, 6> result {};
    for (size_t face = 0; face < result.size (); face++) {
	// Form the basis before translating: origin + direction can round back to origin.
	glm::mat4 view = glm::lookAt (glm::vec3 (0.0f), directions[face], up[face]);
	view[3] = view * glm::vec4 (-origin, 1.0f);
	result[face] = projection * view;
    }
    return result;
}

glm::vec4 CLight::calculatePointShadowProjectionInfo (
    const float radius, const float sourceSize, const bool orthographic, const bool reverseDepth
) {
    const float nearPlane = orthographic ? 1.0f : std::max (sourceSize, 0.05f);
    const float farPlane = std::max (radius, nearPlane + 0.01f);
    const glm::mat4 projection = reverseDepth
        ? reversePerspective (glm::radians (94.0f), 1.0f, nearPlane, farPlane)
        : glm::perspective (glm::half_pi<float> (), 1.0f, nearPlane, farPlane);
    return glm::vec4 (projection[2][2], projection[3][2], projection[2][3], projection[3][3]);
}

glm::vec3
CLight::calculateTubeEndPosition (const glm::mat4& worldMatrix, const glm::vec3& controlPoint) {
    return glm::vec3 (worldMatrix * glm::vec4 (controlPoint, 1.0f));
}
