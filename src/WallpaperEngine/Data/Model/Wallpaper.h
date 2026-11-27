#pragma once

#include <memory>
#include <vector>

#include <glm/glm.hpp>
#include <utility>

#include "Object.h"
#include "Material.h"
#include "CameraPath.h"
#include "Types.h"
#include "WallpaperEngine/Data/JSON.h"
#include "WallpaperEngine/Data/Utils/TypeCaster.h"

namespace WallpaperEngine::Data::Model {
using namespace WallpaperEngine::Data::Utils;

struct WallpaperData {
    std::string filename;
    Project& project;
};

class Wallpaper : public TypeCaster, public WallpaperData {
public:
    explicit Wallpaper (WallpaperData data) noexcept : TypeCaster (), WallpaperData (std::move (data)) { };
    ~Wallpaper () override = default;
};

class Video final : public Wallpaper {
public:
    explicit Video (WallpaperData data) noexcept : Wallpaper (std::move (data)) { }

    ~Video () override = default;
};

class Web final : public Wallpaper {
public:
    explicit Web (WallpaperData data) noexcept : Wallpaper (std::move (data)) { }

    ~Web () override = default;

    // Web callbacks receive authored descriptors with the current property values.
    JSON::JSON propertyDefinitions;
};

struct SceneData {
    /** Authored format version; legacy scenes omit it. */
    int version = 0;
    /** Authored HDR bloom mode; text overbrightness is disabled in ordinary LDR scenes. */
    bool hdr = false;
    /** Native MSAA admission is any authored, non-null top-level model layer. */
    bool hasModelLayer = false;
    /** Conservative static AUTO admission also excludes general/property scripts. */
    bool hasPropertyScripts = false;
    /** Automatically order blended 3D models from back to front. */
    bool transparentSorting;
    /** Preserve an explicitly authored layer order instead of automatic depth sorting. */
    bool customSortOrder;
    /** Clear the scene color each frame; disabling this preserves previous color, never depth. */
    UserSettingUniquePtr clearEnabled;
    struct {
	UserSettingUniquePtr ambient;
	UserSettingUniquePtr skylight;
	UserSettingUniquePtr clear;
    } colors;
    /** Scene force properties; the kinematic-chain solver is not implemented yet. */
    struct {
	UserSettingUniquePtr gravityDirection;
	UserSettingUniquePtr gravityStrength;
	UserSettingUniquePtr windEnabled;
	UserSettingUniquePtr windDirection;
	UserSettingUniquePtr windStrength;
    } physics;
    /** Distance- and world-height-based fog applied by materials whose FOG combo is enabled. */
    struct Fog {
	struct Layer {
	    UserSettingUniquePtr enabled;
	    UserSettingUniquePtr color;
	    UserSettingUniquePtr start;
	    UserSettingUniquePtr end;
	    UserSettingUniquePtr startDensity;
	    UserSettingUniquePtr endDensity;
	};
	Layer distance;
	Layer height;
    } fog;
    /**
     * Camera configuration
     */
    struct Camera {
	/** Enable fade effect */
	UserSettingUniquePtr fade;
	/** Used by the software to allow the users to preview the background or not? */
	bool preview;
	/** Parsed path queues from the legacy top-level list and camera objects. */
	std::vector<CameraPathSource> paths;
	/** Camera layer ids in authored order. Their live world transforms select and
	 *  position the runtime camera after SceneScript updates. */
	std::vector<int> objectIds;
	/** Projection settings authored on each camera layer. These must follow the
	 *  same visible layer as the transform; an inactive camera must not change
	 *  the scene framing. */
	struct ObjectProjection {
	    int id;
	    UserSettingUniquePtr fov;
	    UserSettingUniquePtr zoom;
	};
	std::vector<ObjectProjection> objectProjections;

	/**
	 * Bloom effect configuration
	 */
	struct {
	    /** If bloom is enabled or not */
	    UserSettingUniquePtr enabled;
	    /** Bloom's strength to pass onto the shader */
	    UserSettingUniquePtr strength;
	    /** Bloom's threshold to pass onto the shader */
	    UserSettingUniquePtr threshold;
	    /** Bloom's color tint to pass onto the shader */
	    UserSettingUniquePtr tint;
	    struct {
		UserSettingUniquePtr strength;
		UserSettingUniquePtr threshold;
		UserSettingUniquePtr feather;
		UserSettingUniquePtr scatter;
		UserSettingUniquePtr iterations;
	    } hdr;
	} bloom;
	/**
	 * Parallax effect configuration
	 */
	struct {
	    UserSettingUniquePtr enabled;
	    UserSettingUniquePtr amount;
	    UserSettingUniquePtr delay;
	    UserSettingUniquePtr mouseInfluence;
	} parallax;

	/**
	 * Shake effect configuration
	 */
	struct {
	    UserSettingUniquePtr enabled;
	    UserSettingUniquePtr amplitude;
	    UserSettingUniquePtr roughness;
	    UserSettingUniquePtr speed;
	} shake;

	/**
	 * Position configuration
	 */
	struct {
	    glm::vec3 center;
	    glm::vec3 eye;
	    glm::vec3 up;
	} configuration;

	/**
	 * Projection information
	 */
	struct {
	    int width;
	    int height;
	    bool isAuto;
	    /** True for 3D scenes (orthogonalprojection null/absent): perspective camera */
	    bool isPerspective;
	    UserSettingUniquePtr nearz;
	    UserSettingUniquePtr farz;
	    UserSettingUniquePtr fov;
	    UserSettingUniquePtr perspectiveOverrideFov;
	    UserSettingUniquePtr zoom;
	} projection;
    } camera;

    /** Explicit scene-load material dependencies, independent of authored layers. */
    std::vector<MaterialUniquePtr> precachedMaterials;
    ObjectList objects;
};

class Scene final : public Wallpaper, public SceneData {
public:
    explicit Scene (WallpaperData data, SceneData sceneData) noexcept :
	Wallpaper (std::move (data)), SceneData (std::move (sceneData)) { }

    ~Scene () override = default;
};
} // namespace WallpaperEngine::Data::Model
