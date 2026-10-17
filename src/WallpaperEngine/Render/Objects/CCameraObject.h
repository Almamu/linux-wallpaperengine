#pragma once

#include "WallpaperEngine/Data/Model/Object.h"
#include "WallpaperEngine/Render/CObject.h"
#include "WallpaperEngine/Render/Wallpapers/CScene.h"
#include "WallpaperEngine/Scripting/ScriptableObject.h"

namespace WallpaperEngine::Render::Objects {

/** A scene camera asset: it runs its bound properties but draws no geometry. */
class CCameraObject final : public Scripting::ScriptableObject {
public:
    CCameraObject (Wallpapers::CScene& scene, const Data::Model::CameraObject& camera) :
	CObject (scene, camera), Scripting::ScriptableObject (scene, camera), m_camera (camera) {
	this->registerProperty ("scale", *camera.groupScale->value);
	this->registerProperty ("angles", *camera.groupAngles->value);
	this->registerProperty ("visible", *camera.groupVisible->value);
	this->registerProperty ("fov", *camera.fov->value);
	this->registerProperty ("zoom", *camera.zoom->value);
    }

    [[nodiscard]] const Data::Model::CameraObject& getCameraObject () const { return m_camera; }

private:
    const Data::Model::CameraObject& m_camera;
};

} // namespace WallpaperEngine::Render::Objects
