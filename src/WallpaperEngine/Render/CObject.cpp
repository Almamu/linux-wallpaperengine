#include "CObject.h"

#include <algorithm>
#include <glm/gtc/matrix_transform.hpp>
#include <utility>

#include "WallpaperEngine/Data/Model/DynamicValue.h"
#include "WallpaperEngine/Data/Model/Object.h"

using namespace WallpaperEngine;
using namespace WallpaperEngine::Render;
using namespace WallpaperEngine::Render::Wallpapers;

namespace {
RenderSortClass classifyMaterial (const Material& material) {
    RenderSortClass result = RenderSortClass::Opaque;
    for (const auto& pass : material.passes) {
	if (pass->blending == BlendingMode_Additive) {
	    return RenderSortClass::Additive;
	}
	if (pass->blending == BlendingMode_Translucent) {
	    result = RenderSortClass::Translucent;
	}
    }
    return result;
}

glm::mat4 localMatrix (const Object& object, const double time) {
    const glm::vec3 origin = object.origin->evaluateVec3 (time);

    glm::vec3 scale = glm::vec3 (1.0f);
    glm::vec3 angles = glm::vec3 (0.0f);

    if (object.is<Image> ()) {
	const auto* image = object.as<Image> ();
	scale = image->scale->evaluateVec3 (time);
	angles = image->angles->evaluateVec3 (time);
    } else if (object.is<Particle> ()) {
	const auto* particle = object.as<Particle> ();
	scale = particle->scale->evaluateVec3 (time);
	angles = particle->angles->evaluateVec3 (time);
    } else if (object.is<Text> ()) {
	scale = object.as<Text> ()->scale->evaluateVec3 (time);
	angles = object.groupAngles->evaluateVec3 (time);
    } else {
	scale = object.groupScale->evaluateVec3 (time);
	angles = object.groupAngles->evaluateVec3 (time);
    }

    glm::mat4 local = glm::translate (glm::mat4 (1.0f), origin);
    // same rotation order and radian units as rotateVec3 in CImage.cpp
    local = glm::rotate (local, angles.z, glm::vec3 (0.0f, 0.0f, 1.0f));
    local = glm::rotate (local, angles.y, glm::vec3 (0.0f, 1.0f, 0.0f));
    local = glm::rotate (local, angles.x, glm::vec3 (1.0f, 0.0f, 0.0f));
    return glm::scale (local, scale);
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

void CObject::registerEffectFunctions (const ImageEffect& effect, std::shared_ptr<FBOProvider> provider) {
    if (!effect.effect->functions.empty ()) m_effectFunctionProviders.insert_or_assign (&effect, std::move (provider));
}

void CObject::clearEffectFunctions () { m_effectFunctionProviders.clear (); }

void CObject::executeMaterialFunction (const ImageEffect& effect, const std::string& name) const {
    const auto provider = m_effectFunctionProviders.find (&effect);
    const auto function = effect.effect->functions.find (name);
    if (provider == m_effectFunctionProviders.end () || function == effect.effect->functions.end ()) return;
    // Wallpaper Engine 2.8.42 uses the resolved target count, then clears the
    // first physical FBOs instead of dereferencing those resolved target indices.
    const auto count = std::min (function->second.size (), effect.effect->fbos.size ());
    for (size_t index = 0; index < count; ++index) {
	const auto& definition = *effect.effect->fbos[index];
	if (const auto target = provider->second->find (definition.name); target != nullptr) target->clear (definition.clear);
    }
}

std::optional<glm::mat4> CObject::getAttachmentTransform (const std::string&) const { return std::nullopt; }

std::optional<size_t> CObject::getAttachmentIndex (const std::string&) const { return std::nullopt; }

std::optional<std::string> CObject::getAttachmentName (const size_t) const { return std::nullopt; }

std::optional<size_t> CObject::getBoneIndex (const std::string&) const { return std::nullopt; }

std::optional<glm::mat4> CObject::getBoneTransform (const size_t) const { return std::nullopt; }

glm::vec2 CObject::calculateParallaxOffset (
    const glm::vec2& rootOrigin, const glm::vec2& depth, const glm::vec2& canvasSize, const glm::vec2& displacement,
    const float amount
) {
    // The native renderer subtracts the influenced cursor in canvas coordinates
    // from the root layer's origin, not from the canvas center. Omitting the
    // origin term makes off-center backgrounds expose clear color (Gojo).
    const glm::vec2 cursor = canvasSize * (glm::vec2 (0.5f) + displacement * CScene::PARALLAX_TRANSLATION_SPAN);
    return (rootOrigin - cursor) * depth * amount * glm::vec2 (1.0f, -1.0f);
}

glm::vec2 CObject::resolveParallaxOffset () const {
    const auto& parallax = this->m_scene.getScene ().camera.parallax;
    if (!parallax.enabled->value->getBool ()
	|| this->getContext ().getApp ().getContext ().settings.mouse.disableparallax) {
	return glm::vec2 (0.0f);
    }
    constexpr int kMaxParentDepth = 32;
    const Object* current = &this->m_object;

    // Wallpaper Engine applies camera parallax while rendering a root layer and then
    // renders the complete child subtree under that translated transform. Consequently,
    // both origin and depth belong to the root-most layer. A child-authored depth
    // does not override its parent (3367988661 pins a date/clock subtree this way).
    for (int depth = 0; current->parent.has_value () && depth <= kMaxParentDepth; depth++) {
	const auto* parentObject = this->m_scene.getObject (current->parent.value ());
	if (parentObject == nullptr) {
	    break;
	}
	current = &parentObject->getObject ();
    }

    glm::vec2 depth (1.0f);

    // Images initialize the live property to the native default, so later
    // script assignments also apply when the field was absent from the scene.
    if (current->is<Image> ()) {
	depth = current->as<Image> ()->parallaxDepth->value->getVec2 ();
    } else if (current->is<Shape> ()) {
	depth = current->as<Shape> ()->parallaxDepth->value->getVec2 ();
    } else if (current->authoredParallaxDepth.has_value ()) {
	// Read the live typed property so user settings and SceneScript changes keep working.
	if (current->is<Particle> ()) {
	    depth = current->as<Particle> ()->parallaxDepth->value->getVec2 ();
	} else if (current->is<Text> ()) {
	    depth = current->as<Text> ()->parallaxDepth->value->getVec2 ();
	} else {
	    depth = *current->authoredParallaxDepth;
	}
    }

    // The official Layer constructor initializes parallaxDepth to 1,1. Older workshop
    // scenes often omit the field and depend on that default (Gojo 3100265648 and the
    // puppet/window layer in 3487328036). Explicit 0,0 is the authored pin value.
    return calculateParallaxOffset (
	glm::vec2 (current->origin->evaluateVec3 (this->m_scene.getElapsedTime ())), depth,
	{ this->m_scene.getWidth (), this->m_scene.getHeight () }, *this->m_scene.getParallaxDisplacement (),
	parallax.amount->value->getFloat ()
    );
}

bool CObject::isVisibleThroughParents () const {
    constexpr int kMaxParentDepth = 32;
    const Object* current = &this->m_object;

    for (int depth = 0; current->parent.has_value () && depth <= kMaxParentDepth; depth++) {
	const auto* parentObject = this->m_scene.getObject (current->parent.value ());

	if (parentObject == nullptr) {
	    break;
	}

	const Object& parent = parentObject->getObject ();
	// image/text parents render from their own visible value; plain group containers
	// only carry the group fallback
	const auto& visible = parent.is<Image> () ? parent.as<Image> ()->visible
	    : parent.is<Text> ()                  ? parent.as<Text> ()->visible
						  : parent.groupVisible;

	if (visible != nullptr && visible->value != nullptr && !visible->value->getBool ()) {
	    return false;
	}

	current = &parent;
    }

    return true;
}

bool CObject::isVolumetricBarrier () const {
    // Native camera gate 1400a36d0 always returns false; ordinary objects use
    // their own live visibility followed by the same test on each ancestor.
    if (this->m_object.cameraLayer) return false;

    constexpr int kMaxParentDepth = 32;
    const Object* current = &this->m_object;
    for (int depth = 0; depth <= kMaxParentDepth; ++depth) {
	const auto& visible = current->is<Image> () ? current->as<Image> ()->visible
	    : current->is<Particle> ()             ? current->as<Particle> ()->visible
	    : current->is<Text> ()                 ? current->as<Text> ()->visible
						   : current->groupVisible;
	if (visible != nullptr && visible->value != nullptr && !visible->value->getBool ()) return false;
	if (!current->parent.has_value ()) break;
	const auto* parent = this->m_scene.getObject (*current->parent);
	if (parent == nullptr) break;
	current = &parent->getObject ();
    }
    return true;
}

glm::mat4 CObject::resolveWorldMatrix () const {
    constexpr int kMaxParentDepth = 32;

    // walk leaf-first like resolveParallaxOffset, bounded to guard against cycles
    const Object* chain[kMaxParentDepth + 1];
    int count = 0;
    const Object* current = &this->m_object;
    chain[count++] = current;

    while (current->parent.has_value () && count <= kMaxParentDepth) {
	const auto* parentObject = this->m_scene.getObject (current->parent.value ());

	if (parentObject == nullptr) {
	    break;
	}

	current = &parentObject->getObject ();
	chain[count++] = current;
    }

    const double time = this->m_scene.getElapsedTime ();
    glm::mat4 world = glm::mat4 (1.0f);

    for (int i = count - 1; i >= 0; --i) {
	world = world * localMatrix (*chain[i], time);

	// Attachments live on the parent and are inserted between that parent's local
	// transform and the attached child's local transform.
	if (i > 0 && chain[i - 1]->attachment.has_value ()) {
	    const CObject* parent = this->m_scene.getObject (chain[i]->id);
	    if (parent != nullptr) {
		const auto attachment = parent->getAttachmentTransform (*chain[i - 1]->attachment);
		if (attachment.has_value ()) {
		    world *= *attachment;
		}
	    }
	}
    }

    return world;
}

RenderSortClass CObject::getRenderSortClass () const {
    const Object& object = this->getObject ();
    if (object.is<Particle> ()) {
	const auto& material = object.as<Particle> ()->material;
	if (material != nullptr && material->material != nullptr) {
	    return classifyMaterial (*material->material);
	}
    }
    if (object.is<Model3D> ()) {
	RenderSortClass result = RenderSortClass::Opaque;
	for (const auto& material : object.as<Model3D> ()->materials) {
	    const RenderSortClass current = classifyMaterial (*material);
	    if (current == RenderSortClass::Additive) {
		return current;
	    }
	    if (current == RenderSortClass::Translucent) {
		result = current;
	    }
	}
	return result;
    }
    return RenderSortClass::Opaque;
}
