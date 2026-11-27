#pragma once

#include "WallpaperEngine/Render/Wallpapers/CScene.h"
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "WallpaperEngine/Data/Model/Wallpaper.h"

namespace WallpaperEngine::Render::Wallpapers {
class CScene;
}

namespace WallpaperEngine::Render {
using namespace WallpaperEngine::Data::Model;

class Camera {
public:
    Camera (Wallpapers::CScene& scene, const SceneData::Camera& camera);
    ~Camera ();

    void setOrthogonalProjection (const float width, const float height);
    void setPerspectiveProjection (const float width, const float height, const bool flipY);
    /** Output framing changes the projection, not authored canvas coordinates. */
    void setViewportScale (glm::vec2 scale);
    [[nodiscard]] const glm::vec2& getViewportScale () const;

    [[nodiscard]] const glm::vec3& getCenter () const;
    [[nodiscard]] const glm::vec3& getEye () const;
    [[nodiscard]] const glm::vec3& getUp () const;
    [[nodiscard]] const glm::mat4& getProjection () const;
    [[nodiscard]] const glm::mat4& getRasterProjection () const;
    [[nodiscard]] bool usesReverseDepth () const;
    void setReverseDepth (bool enabled);
    [[nodiscard]] const glm::mat4& getLookAt () const;
    [[nodiscard]] Wallpapers::CScene& getScene () const;
    [[nodiscard]] bool isOrthogonal () const;
    /** True when the perspective projection renders Y-mirrored to compensate the
     *  output's vertical flip at present time (see Output::renderVFlip) */
    [[nodiscard]] bool isYFlipped () const;
    [[nodiscard]] float getWidth () const;
    [[nodiscard]] float getHeight () const;
    [[nodiscard]] float getFov () const;
    [[nodiscard]] float getDefaultFov (double sceneTime) const;
    [[nodiscard]] float getNearZ () const;
    [[nodiscard]] float getFarZ () const;
    [[nodiscard]] float getZoom () const;
    [[nodiscard]] CameraTransform getDefaultTransform () const;
    /** SceneScript exchanges the sampled pose before render-only shake. */
    [[nodiscard]] const CameraTransform& getUnshakenTransform () const;
    /** Replace the resting pose used when no camera path is active. */
    void setDefaultTransform (const CameraTransform& transform, bool apply);
    /** Convert a camera layer's world matrix into the eye/forward/up contract. */
    [[nodiscard]] static CameraTransform objectTransform (
	const glm::mat4& world, float fov, float zoom
    );
    /** Apply a sampled camera path transform and rebuild view/projection matrices. */
    void setTransform (const CameraTransform& transform);
    /** Restore the authored resting pose. */
    void resetTransform ();
    /** Convert a normalized scene position (0..1, bottom-left origin) to the
     *  Wallpaper Engine authoring plane at world Z=0. */
    [[nodiscard]] glm::vec3 screenToWorld (const glm::vec2& normalizedPosition) const;

    /** Pure projection helper exposed for coordinate regression tests. */
    [[nodiscard]] static glm::vec3 projectScreenPosition (
	const glm::vec2& normalizedPosition, bool orthogonal, float width, float height, const glm::mat4& projection,
	const glm::mat4& view, bool projectionYFlipped
    );

private:
    float m_width;
    float m_height;
    glm::vec2 m_viewportScale = { 1.0f, 1.0f };
    bool m_isOrthogonal = false;
    bool m_isYFlipped = false;
    glm::mat4 m_projection = {};
    glm::mat4 m_rasterProjection = {};
    bool m_reverseDepth = false;
    glm::mat4 m_lookat = {};
    CameraTransform m_defaultTransform = {};
    CameraTransform m_transform = {};
    glm::vec3 m_renderEye = {};
    glm::vec3 m_renderCenter = {};
    /** The editor viewport is canceled out in legacy 2D scenes. A runtime camera
     *  layer or path instead moves the orthographic view and must not be canceled. */
    bool m_hasRuntimeTransform = false;
    const SceneData::Camera& m_camera;
    Wallpapers::CScene& m_scene;

    void updateMatrices ();
};
} // namespace WallpaperEngine::Render
