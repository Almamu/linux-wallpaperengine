#include "CObject.h"

#include <cmath>
#include <utility>

#include <glm/gtc/matrix_transform.hpp>
#include "WallpaperEngine/Data/Model/Object.h"
#include "WallpaperEngine/Logging/Log.h"
#include "WallpaperEngine/Render/Wallpapers/CScene.h"

using namespace WallpaperEngine;
using namespace WallpaperEngine::Render;
using namespace WallpaperEngine::Render::Wallpapers;

CObject::CObject (Wallpapers::CScene& scene, const Object& object) :
    Helpers::ContextAware (scene), m_scene (scene), m_object (object) { }

void CObject::setup () { }
void CObject::render () { }

Wallpapers::CScene& CObject::getScene () const { return this->m_scene; }

const AssetLocator& CObject::getAssetLocator () const { return this->getScene ().getAssetLocator (); }

int CObject::getId () const { return this->m_object.id; }

const Object& CObject::getObject () const { return this->m_object; }

glm::mat4 CObject::localModelMatrix (const Object& object) {
    glm::vec3 origin = object.origin->value->getVec3 ();
    glm::vec3 scale = glm::vec3 (1.0f);
    float angle = 0.0f;

    if (object.is<Image> ()) {
	const auto* image = object.as<Image> ();
	scale = image->scale->value->getVec3 ();
	angle = image->angles->value->getVec3 ().z;
    } else if (object.is<Text> ()) {
	const auto* text = object.as<Text> ();
	scale = text->scale->value->getVec3 ();
	angle = object.groupAngles->value->getVec3 ().z;
    } else {
	scale = object.groupScale->value->getVec3 ();
	angle = object.groupAngles->value->getVec3 ().z;
    }

    glm::mat4 mat = glm::translate (glm::mat4 (1.0f), origin);
    if (angle != 0.0f) {
	mat = glm::rotate (mat, angle, glm::vec3 (0.0f, 0.0f, 1.0f));
    }
    return glm::scale (mat, scale);
}

glm::mat4 CObject::resolveModelMatrix () const {
    return this->resolveModelMatrix (this->getObject ());
}

glm::mat4 CObject::resolveModelMatrix (const Object& object) const {
    return resolveModelMatrix (object, [this] (int id) -> const Object* {
	const auto* parentObject = this->getScene ().getObject (id);
	return parentObject != nullptr ? &parentObject->getObject () : nullptr;
    });
}

glm::mat4 CObject::resolveModelMatrix (
    const Object& object, const std::function<const Object*(int)>& getParent
) {
    constexpr int kMaxParentDepth = 32;

    const Object* chain[kMaxParentDepth + 1];
    int count = 0;
    const Object* current = &object;
    chain[count++] = current;

    while (current->parent.has_value ()) {
	if (count > kMaxParentDepth) {
	    sLog.error ("Parent transform chain is too deep; possible cycle at object id=", current->id);
	    break;
	}
	const auto* parent = getParent (current->parent.value ());
	if (parent == nullptr) {
	    break;
	}
	for (int i = 0; i < count; ++i) {
	    if (chain[i] == parent) {
		sLog.error ("Parent transform cycle at object id=", parent->id);
		return localModelMatrix (object);
	    }
	}
	current = parent;
	chain[count++] = current;
    }

    glm::mat4 world = localModelMatrix (*chain[count - 1]);
    for (int i = count - 2; i >= 0; --i) {
	world = world * localModelMatrix (*chain[i]);
    }

    return world;
}
