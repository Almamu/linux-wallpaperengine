#include "CObject.h"

#include <cmath>
#include <utility>

#include "WallpaperEngine/Data/Model/Object.h"
#include "WallpaperEngine/Logging/Log.h"
#include "WallpaperEngine/Render/Wallpapers/CScene.h"

using namespace WallpaperEngine;
using namespace WallpaperEngine::Render;
using namespace WallpaperEngine::Render::Wallpapers;

namespace {
glm::vec2 rotateVec2 (const glm::vec2& value, float angle) {
    const float cosAngle = std::cos (angle);
    const float sinAngle = std::sin (angle);
    return { value.x * cosAngle - value.y * sinAngle, value.x * sinAngle + value.y * cosAngle };
}
} // namespace

CObject::CObject (Wallpapers::CScene& scene, const Object& object) :
    Helpers::ContextAware (scene), m_scene (scene), m_object (object) { }

void CObject::setup () { }
void CObject::render () { }

Wallpapers::CScene& CObject::getScene () const { return this->m_scene; }

const AssetLocator& CObject::getAssetLocator () const { return this->getScene ().getAssetLocator (); }

int CObject::getId () const { return this->m_object.id; }

const Object& CObject::getObject () const { return this->m_object; }

CObject::ResolvedTransform CObject::localTransform (const Object& object) {
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

    return { origin, scale, angle };
}

CObject::ResolvedTransform CObject::resolveTransform () const {
    return this->resolveTransform (this->getObject ());
}

CObject::ResolvedTransform CObject::resolveTransform (const Object& object) const {
    constexpr int kMaxParentDepth = 32;

    // Walk up the parent chain leaf-first, bounded by kMaxParentDepth to guard
    // against cycles. chain[0] is the requested object; the last entry is the root.
    const Object* chain[kMaxParentDepth + 1];
    int count = 0;
    const Object* current = &object;
    chain[count++] = current;

    while (current->parent.has_value ()) {
	if (count > kMaxParentDepth) {
	    sLog.error ("Parent transform chain is too deep; possible cycle at object id=", current->id);
	    break;
	}
	const auto* parentObject = this->getScene ().getObject (current->parent.value ());
	if (parentObject == nullptr) {
	    break;
	}
	const Object* parent = &parentObject->getObject ();
	for (int i = 0; i < count; ++i) {
	    if (chain[i] == parent) {
		sLog.error ("Parent transform cycle at object id=", parent->id);
		return localTransform (object);
	    }
	}
	current = parent;
	chain[count++] = current;
    }

    // Accumulate top-down: the root's local transform is already its resolved
    // transform, then fold each child onto its already-resolved parent.
    ResolvedTransform resolved = localTransform (*chain[count - 1]);
    for (int i = count - 2; i >= 0; --i) {
	ResolvedTransform local = localTransform (*chain[i]);
	const glm::vec2 offset
	    = rotateVec2 ({ local.origin.x * resolved.scale.x, local.origin.y * resolved.scale.y }, resolved.angle);
	local.origin.x = resolved.origin.x + offset.x;
	local.origin.y = resolved.origin.y + offset.y;
	local.origin.z = resolved.origin.z + local.origin.z * resolved.scale.z;
	resolved = { local.origin, local.scale * resolved.scale, local.angle + resolved.angle };
    }

    return resolved;
}