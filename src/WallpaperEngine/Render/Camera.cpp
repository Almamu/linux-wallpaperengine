#include <algorithm>
#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "Camera.h"
#include "WallpaperEngine/Logging/Log.h"

using namespace WallpaperEngine;
using namespace WallpaperEngine::Render;

Camera::Camera (Wallpapers::CScene& scene, const SceneData::Camera& camera) :
    m_width (0), m_height (0), m_camera (camera), m_scene (scene) {
    // get the lookat position
    // TODO: ENSURE THIS IS ONLY USED WHEN NOT DOING AN ORTOGRAPHIC CAMERA AS IT THROWS OFF POINTS
    this->m_lookat = glm::lookAt (this->getEye (), this->getCenter (), this->getUp ());
}

Camera::~Camera () = default;

const glm::vec3& Camera::getCenter () const {
    return this->m_hasScriptedView ? this->m_scriptedCenter : this->m_camera.configuration.center;
}

const glm::vec3& Camera::getEye () const {
    return this->m_hasScriptedView ? this->m_scriptedEye : this->m_camera.configuration.eye;
}

const glm::vec3& Camera::getUp () const {
    return this->m_hasScriptedView ? this->m_scriptedUp : this->m_camera.configuration.up;
}

const glm::mat4& Camera::getProjection () const { return this->m_projection; }

const glm::mat4& Camera::getScreenProjection () const { return this->m_screenProjection; }

const glm::mat4& Camera::getLookAt () const { return this->m_lookat; }

bool Camera::isOrthogonal () const { return this->m_isOrthogonal; }

Wallpapers::CScene& Camera::getScene () const { return this->m_scene; }

float Camera::getWidth () const { return this->m_width; }

float Camera::getHeight () const { return this->m_height; }

float Camera::getFov () const {
    return this->m_scriptedFov > 0.0f ? this->m_scriptedFov : this->m_camera.projection.fov->value->getFloat ();
}

float Camera::getOverrideFov () const { return this->m_camera.projection.overrideFov->value->getFloat (); }

float Camera::getNearZ () const { return this->m_camera.projection.nearz->value->getFloat (); }

float Camera::getFarZ () const { return this->m_camera.projection.farz->value->getFloat (); }

void Camera::setOrthogonalProjection (const float width, const float height) {
    this->m_width = width;
    this->m_height = height;

    float nearz = this->m_camera.projection.nearz->value->getFloat ();
    float farz = this->m_camera.projection.farz->value->getFloat ();

    // Orthographic Workshop scenes commonly place their 2D layers at z=0.
    // Keep that plane inside the clip volume, even when the authored near/far
    // range starts or ends at zero. Exact clip-plane vertices can be dropped
    // by some drivers after projection and view matrices are combined.
    const float depthMargin = std::max (1.0f, std::abs (farz - nearz) * 0.001f);
    nearz = std::min (nearz, 0.0f) - depthMargin;
    farz = std::max (farz, 0.0f) + depthMargin;

    this->m_projection = glm::ortho<float> (-width / 2.0, width / 2.0, -height / 2.0, height / 2.0, nearz, farz);
    this->m_projection = glm::translate (this->m_projection, this->getEye ());
    this->m_screenProjection = this->m_projection;
    this->m_isOrthogonal = true;
}

void Camera::setPerspectiveProjection (const float width, const float height) {
    const float safeWidth = std::max (width, 1.0f);
    const float safeHeight = std::max (height, 1.0f);
    this->m_width = safeWidth;
    this->m_height = safeHeight;

    const float nearz = std::max (this->getNearZ (), 0.01f);
    const float farz = std::max (this->getFarZ (), nearz + 1.0f);

    this->m_projection = glm::scale (glm::mat4 (1.0f), glm::vec3 (1.0f, -1.0f, 1.0f))
	* glm::perspective (glm::radians (this->getFov ()), safeWidth / safeHeight, nearz, farz);

    const float halfRange = std::max (farz, 1000.0f);
    this->m_screenProjection = glm::ortho<float> (
	-safeWidth / 2.0f, safeWidth / 2.0f, -safeHeight / 2.0f, safeHeight / 2.0f, -halfRange, halfRange
    );
    this->m_lookat = glm::lookAt (this->getEye (), this->getCenter (), this->getUp ());
    this->m_isOrthogonal = false;
}

void Camera::setScriptedView (const glm::vec3& eye, const glm::vec3& center) {
    const glm::vec3 delta = center - eye;
    if (!std::isfinite (eye.x + eye.y + eye.z + center.x + center.y + center.z) || glm::dot (delta, delta) < 1e-12f) {
	return;
    }

    this->m_hasScriptedView = true;
    this->m_scriptedEye = eye;
    this->m_scriptedCenter = center;

    if (!this->m_isOrthogonal) {
	this->m_lookat = glm::lookAt (this->getEye (), this->getCenter (), this->getUp ());
    }
}

void Camera::setScriptedView (const glm::vec3& eye, const glm::vec3& center, const glm::vec3& up, const float fov) {
    const glm::vec3 delta = center - eye;
    if (!std::isfinite (eye.x + eye.y + eye.z + center.x + center.y + center.z + up.x + up.y + up.z)
	|| glm::dot (delta, delta) < 1e-12f || glm::dot (up, up) < 1e-12f) {
	return;
    }

    this->m_hasScriptedView = true;
    this->m_scriptedEye = eye;
    this->m_scriptedCenter = center;
    this->m_scriptedUp = glm::normalize (up);
    this->m_scriptedFov = std::isfinite (fov) && fov > 1.0f && fov < 179.0f ? fov : 0.0f;

    if (!this->m_isOrthogonal) {
	this->setPerspectiveProjection (this->m_width, this->m_height);
    }
}

void Camera::clearScriptedView () {
    if (!this->m_hasScriptedView) {
	return;
    }
    this->m_hasScriptedView = false;
    this->m_scriptedFov = 0.0f;
    if (!this->m_isOrthogonal) {
	this->setPerspectiveProjection (this->m_width, this->m_height);
    }
}
