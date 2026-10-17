#include "CImage.h"

#include "CRenderable.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <iterator>
#include <optional>
#include <sstream>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/rotate_vector.hpp>
#undef GLM_ENABLE_EXPERIMENTAL

#include "WallpaperEngine/Data/Model/DynamicValue.h"
#include "WallpaperEngine/Data/Model/Material.h"
#include "WallpaperEngine/Data/Model/Object.h"
#include "WallpaperEngine/Data/Model/UserSetting.h"
#include "WallpaperEngine/Data/Parsers/MaterialParser.h"
#include "WallpaperEngine/Data/Utils/BinaryReader.h"
#include "WallpaperEngine/Data/Utils/MemoryStream.h"
#include "WallpaperEngine/Logging/Log.h"

using namespace WallpaperEngine;
using namespace WallpaperEngine::Render::Objects;
using namespace WallpaperEngine::Render::Objects::Effects;
using namespace WallpaperEngine::Data::Parsers;
using namespace WallpaperEngine::Data::Builders;
using namespace WallpaperEngine::Data::Utils;

namespace {
glm::vec2 rotateVec2 (const glm::vec2& value, float angle) {
    const float cosAngle = std::cos (angle);
    const float sinAngle = std::sin (angle);
    return { value.x * cosAngle - value.y * sinAngle, value.x * sinAngle + value.y * cosAngle };
}

bool isMagentaNeonTint (const glm::vec3& color) { return color.r > 0.55f && color.g < 0.25f && color.b > 0.45f; }

bool usesSourceUvMask (const std::string& shader, const int textureIndex) {
	const auto separator = shader.find_last_of ('/');
	const std::string effect = separator == std::string::npos ? shader : shader.substr (separator + 1);
	if (effect == "shake") {
	    // Texture 1 is a vector field; only texture 3 is an opacity mask.
	    return textureIndex == 3;
	}
	if (effect == "waterwaves") {
	    return textureIndex == 1 || textureIndex == 2;
	}
	if (effect == "pulse") {
	    return textureIndex == 2;
	}
	// Waterflow's texture 1 stores displacement vectors. Reprojecting the pixels
	// without transforming those vectors changes their direction on puppet meshes.
	return textureIndex == 1
	    && (effect == "tint" || effect == "iris" || effect == "foliagesway" || effect == "caustics");
}

float normalizeImageAlpha (const float alpha) {
    // Some Workshop scenes store image opacity as an 8-bit value instead of 0..1.
    if (alpha > 1.0f && alpha <= 255.0f && std::floor (alpha) == alpha) {
	return alpha / 255.0f;
    }

    return std::clamp (alpha, 0.0f, 1.0f);
}

float bezierEase (const float t, const float x1, const float x2) {
    const auto bx = [x1, x2] (const float s) {
	const float inv = 1.0f - s;
	return 3.0f * inv * inv * s * x1 + 3.0f * inv * s * s * x2 + s * s * s;
    };

    float s = t;
    for (int i = 0; i < 6; i++) {
	const float inv = 1.0f - s;
	const float dx = 3.0f * inv * inv * x1 + 6.0f * inv * s * (x2 - x1) + 3.0f * s * s * (1.0f - x2);
	if (std::abs (dx) < 1e-6f) {
	    break;
	}
	s = std::clamp (s - (bx (s) - t) / dx, 0.0f, 1.0f);
    }
    return 3.0f * (1.0f - s) * s * s + s * s * s;
}

float evalAnimationChannel (const AnimationChannel& channel, const float frame, const float fallback) {
    if (channel.keys.empty ()) {
	return fallback;
    }
    if (channel.keys.size () == 1 || frame <= channel.keys.front ().frame) {
	return channel.keys.front ().value;
    }
    if (frame >= channel.keys.back ().frame) {
	return channel.keys.back ().value;
    }

    for (size_t i = 0; i + 1 < channel.keys.size (); i++) {
	const auto& k0 = channel.keys[i];
	const auto& k1 = channel.keys[i + 1];
	if (frame < k0.frame || frame > k1.frame) {
	    continue;
	}
	const float span = std::max (k1.frame - k0.frame, 1e-6f);
	const float t = (frame - k0.frame) / span;
	const float x1 = std::clamp (k0.frontX * 0.5f, 0.0f, 1.0f);
	const float x2 = std::clamp (1.0f + k1.backX * 0.5f, 0.0f, 1.0f);
	return k0.value + (k1.value - k0.value) * bezierEase (t, x1, x2);
    }
    return fallback;
}

std::optional<glm::vec3> findMagentaCompositeTint (const Image& image, const std::vector<int>& skippedEffectIds) {
    for (const auto& effect : image.effects) {
	if (std::find (skippedEffectIds.begin (), skippedEffectIds.end (), static_cast<int> (effect->id))
	    != skippedEffectIds.end ()) {
	    continue;
	}
	if (!effect->visible->value->getBool ()) {
	    continue;
	}

	for (const auto& passOverride : effect->passOverrides) {
	    const auto compositeCombo = passOverride->combos.find ("COMPOSITE");
	    if (compositeCombo == passOverride->combos.end () || compositeCombo->second != 2) {
		continue;
	    }

	    const auto compositeColor = passOverride->constants.find ("compositecolor");
	    if (compositeColor == passOverride->constants.end () || compositeColor->second == nullptr
		|| compositeColor->second->value == nullptr) {
		continue;
	    }

	    const auto tint = compositeColor->second->value->getVec3 ();
	    if (isMagentaNeonTint (tint)) {
		return tint;
	    }
	}
    }

    return std::nullopt;
}

} // namespace

CImage::ResolvedTransform CImage::localTransform (const Object& object) {
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
    } else {
	scale = object.groupScale->value->getVec3 ();
	angle = object.groupAngles->value->getVec3 ().z;
    }

    return { origin, scale, angle };
}

CImage::ResolvedTransform CImage::resolveTransform (const Object& object) const {
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
	current = &parentObject->getObject ();
	chain[count++] = current;
    }

    // Accumulate top-down: the root's local transform is already its resolved
    // transform, then fold each child onto its already-resolved parent.
    ResolvedTransform resolved = localTransform (*chain[count - 1]);
    for (int i = count - 2; i >= 0; --i) {
	ResolvedTransform local = localTransform (*chain[i]);
	bool attached = false;

	if (chain[i]->attachment.has_value () && chain[i]->parent.has_value ()) {
	    const CObject* parentRender = this->getScene ().getObject (*chain[i]->parent);
	    if (parentRender != nullptr && parentRender->is<CImage> ()) {
		const auto* parentImage = parentRender->as<CImage> ();
		const auto attachmentMatrix = parentImage->getPuppetAttachmentMatrix (*chain[i]->attachment);
		if (attachmentMatrix.has_value ()) {
		    const glm::vec3 attachmentOrigin = glm::vec3 ((*attachmentMatrix)[3]);
		    const float attachmentAngle = std::atan2 ((*attachmentMatrix)[0][1], (*attachmentMatrix)[0][0]);

		    const glm::vec2 attachmentOffset = rotateVec2 (
			{ attachmentOrigin.x * resolved.scale.x, attachmentOrigin.y * resolved.scale.y }, resolved.angle
		    );
		    const glm::vec2 childOffset = rotateVec2 (
			{ local.origin.x * resolved.scale.x, local.origin.y * resolved.scale.y },
			resolved.angle + attachmentAngle
		    );

		    local.origin.x = resolved.origin.x + attachmentOffset.x + childOffset.x;
		    local.origin.y = resolved.origin.y + attachmentOffset.y + childOffset.y;
		    local.origin.z = resolved.origin.z + (attachmentOrigin.z + local.origin.z) * resolved.scale.z;
		    local.scale *= resolved.scale;
		    local.angle += resolved.angle + attachmentAngle;
		    resolved = local;
		    attached = true;
		}
	    }
	}

	if (!attached) {
	    const glm::vec2 offset
		= rotateVec2 ({ local.origin.x * resolved.scale.x, local.origin.y * resolved.scale.y }, resolved.angle);
	    local.origin.x = resolved.origin.x + offset.x;
	    local.origin.y = resolved.origin.y + offset.y;
	    local.origin.z = resolved.origin.z + local.origin.z * resolved.scale.z;
	    resolved = { local.origin, local.scale * resolved.scale, local.angle + resolved.angle };
	}
    }

    return resolved;
}

CImage::CImage (Wallpapers::CScene& scene, const Image& image) :
    CObject (scene, image), CRenderable (scene, image, *image.model->material), ScriptableObject (scene, image),
    m_sceneSpacePosition (GL_NONE), m_copySpacePosition (GL_NONE), m_passSpacePosition (GL_NONE),
    m_texcoordCopy (GL_NONE), m_texcoordPass (GL_NONE), m_modelViewProjectionScreen (),
    m_modelViewProjectionPass (glm::mat4 (1.0)), m_modelViewProjectionCopy (), m_modelViewProjectionScreenInverse (),
    m_modelViewProjectionPassInverse (glm::inverse (m_modelViewProjectionPass)), m_modelViewProjectionCopyInverse (),
    m_modelMatrix (), m_viewProjectionMatrix (), m_image (image), m_pos (), m_initialized (false) {
    this->m_animatedAlpha = normalizeImageAlpha (image.alpha->value->getFloat ());
    this->m_color4Cache = image.color->value->getVec4 ();
    this->m_color4Cache.a *= this->m_animatedAlpha;
    // register any properties in use on this object
    this->registerProperty ("origin", *image.origin->value);
    this->registerProperty ("scale", *image.scale->value);
    this->registerProperty ("angles", *image.angles->value);
    this->registerProperty ("visible", *image.visible->value);
    this->registerProperty ("alpha", *image.alpha->value);
    this->registerProperty ("color", *image.color->value);
    this->registerProperty ("parallaxDepth", *image.parallaxDepth->value);

    for (const auto& layer : image.animationLayers) {
	const std::string prefix = "animationLayer" + std::to_string (layer->id) + "_";
	this->registerProperty (prefix + "rate", *layer->rate->value);
	this->registerProperty (prefix + "visible", *layer->visible->value);
	this->registerProperty (prefix + "blend", *layer->blend->value);
	this->registerProperty (prefix + "animation", *layer->animation->value);
    }

    this->registerMaterialProperties ("material", *image.model->material);

    for (const auto& effect : image.effects) {
	const std::string effectPrefix = "effect" + std::to_string (effect->id);
	this->registerProperty (effectPrefix + "_visible", *effect->visible->value);
	for (size_t overrideIndex = 0; overrideIndex < effect->passOverrides.size (); overrideIndex++) {
	    const auto& passOverride = *effect->passOverrides[overrideIndex];
	    for (const auto& [name, setting] : passOverride.constants) {
		if (setting != nullptr && setting->value != nullptr) {
		    this->registerProperty (
			effectPrefix + "_override" + std::to_string (overrideIndex) + "_" + name, *setting->value
		    );
		}
	    }
	}
	for (size_t passIndex = 0; passIndex < effect->effect->passes.size (); passIndex++) {
	    const auto& pass = *effect->effect->passes[passIndex];
	    if (pass.material.has_value ()) {
		this->registerMaterialProperties (effectPrefix + "_pass" + std::to_string (passIndex), **pass.material);
	    }
	}
    }

    // get scene width and height to calculate positions
    auto scene_width = static_cast<float> (scene.getWidth ());
    auto scene_height = static_cast<float> (scene.getHeight ());

    const auto transform = this->resolveTransform (this->getImage ());
    glm::vec3 origin = transform.origin;
    glm::vec2 size = this->getSize ();
    glm::vec3 scale = transform.scale;

    // Composition layers render their authored child subtree into a private full-frame
    // target. Their material samples _rt_FullFrameBuffer, so shadow that name locally.
    if (this->isCompositionLayer () && scene.hasAuthoredChildren (image.id)) {
	const glm::vec2 compositionSize { static_cast<float> (scene.getWidth ()),
					  static_cast<float> (scene.getHeight ()) };
	auto composition = scene.create (
	    "_rt_compositionLayer_" + std::to_string (image.id), TextureFormat_ARGB8888, TextureFlags_ClampUVs, 1.0f,
	    compositionSize, compositionSize
	);
	this->m_compositionFBO = composition;
	this->alias ("_rt_FullFrameBuffer", composition);
    }

    this->detectTexture ();

    // detect texture (if any)
    if (this->m_texture == nullptr) {
	if (this->m_image.model->solidlayer && size.x == 0.0f && size.y == 0.0f) {
	    size.x = scene_width;
	    size.y = scene_height;
	}
	// if (this->m_image->isSolid ()) // layer receives cursor events:
	// https://docs.wallpaperengine.io/en/scene/scenescript/reference/event/cursor.html same applies to effects
	// TODO: create a dummy texture of correct size, fbo constructors should be enough, but this should be properly
	// handled
	this->m_texture = std::make_shared<CFBO> (
	    "", TextureFormat_ARGB8888, TextureFlags_NoFlags, 1, size.x, size.y, size.x, size.y
	);
    }

    // If the wallpaper doesn't specify a size, fall back to the texture or model dimensions
    if ((size.x == 0.0f || size.y == 0.0f) && this->m_texture != nullptr) {
	size.x = static_cast<float> (this->m_texture->getRealWidth ());
	size.y = static_cast<float> (this->m_texture->getRealHeight ());
    } else if (
	(size.x == 0.0f || size.y == 0.0f) && this->getImage ().model->width.has_value ()
	&& this->getImage ().model->height.has_value ()
    ) {
	size.x = static_cast<float> (this->getImage ().model->width.value ());
	size.y = static_cast<float> (this->getImage ().model->height.value ());
    }

    // fullscreen layers should use the whole projection's size
    // TODO: WHAT SHOULD AUTOSIZE DO?
    if (this->getImage ().model->fullscreen) {
	size = { scene_width, scene_height };
	origin = { scene_width / 2, scene_height / 2, 0 };

	// TODO: CHANGE ALIGNMENT TOO?
    }

    // Load puppets before allocating their render targets so autosize affects the
    // canvas, projections and effect FBOs from the first frame.
    this->m_hasPuppetMesh = this->loadPuppetMesh (size);
    this->m_size = size;

    glm::vec2 scaledSize = size * glm::vec2 (scale);

    // calculate the center and shift from there
    this->m_pos.x = origin.x - (scaledSize.x / 2);
    this->m_pos.w = origin.y + (scaledSize.y / 2);
    this->m_pos.z = origin.x + (scaledSize.x / 2);
    this->m_pos.y = origin.y - (scaledSize.y / 2);

    if (this->getImage ().alignment.find ("top") != std::string::npos) {
	this->m_pos.y -= scaledSize.y / 2;
	this->m_pos.w -= scaledSize.y / 2;
    } else if (this->getImage ().alignment.find ("bottom") != std::string::npos) {
	this->m_pos.y += scaledSize.y / 2;
	this->m_pos.w += scaledSize.y / 2;
    }

    if (this->getImage ().alignment.find ("left") != std::string::npos) {
	this->m_pos.x += scaledSize.x / 2;
	this->m_pos.z += scaledSize.x / 2;
    } else if (this->getImage ().alignment.find ("right") != std::string::npos) {
	this->m_pos.x -= scaledSize.x / 2;
	this->m_pos.z -= scaledSize.x / 2;
    }

    // wallpaper engine
    this->m_pos.x -= scene_width / 2;
    this->m_pos.y = scene_height / 2 - this->m_pos.y;
    this->m_pos.z -= scene_width / 2;
    this->m_pos.w = scene_height / 2 - this->m_pos.w;

    // register both FBOs into the scene
    std::ostringstream nameA, nameB;

    // TODO: determine when _rt_imageLayerComposite and _rt_imageLayerAlbedo is used
    nameA << "_rt_imageLayerComposite_" << this->getImage ().id << "_a";
    nameB << "_rt_imageLayerComposite_" << this->getImage ().id << "_b";

    this->m_currentMainFBO = this->m_mainFBO = scene.create (
	nameA.str (), TextureFormat_ARGB8888, this->m_texture->getFlags (), 1, { size.x, size.y }, { size.x, size.y }
    );
    this->m_currentSubFBO = this->m_subFBO = scene.create (
	nameB.str (), TextureFormat_ARGB8888, this->m_texture->getFlags (), 1, { size.x, size.y }, { size.x, size.y }
    );

    // build a list of vertices, these might need some change later (or maybe invert the camera)
    GLfloat sceneSpacePosition[] = { this->m_pos.x, this->m_pos.y, 0.0f, this->m_pos.x, this->m_pos.w, 0.0f,
				     this->m_pos.z, this->m_pos.y, 0.0f, this->m_pos.z, this->m_pos.y, 0.0f,
				     this->m_pos.x, this->m_pos.w, 0.0f, this->m_pos.z, this->m_pos.w, 0.0f };

    float width = 1.0f;
    float height = 1.0f;

    if (this->getTexture ()->isAnimated ()) {
	// animated images use different coordinates as they're essentially a texture atlas
	width = static_cast<float> (this->getTexture ()->getRealWidth ())
	    / static_cast<float> (this->getTexture ()->getTextureWidth (0));
	height = static_cast<float> (this->getTexture ()->getRealHeight ())
	    / static_cast<float> (this->getTexture ()->getTextureHeight (0));
    }
    // calculate the correct texCoord limits for the texture based on the texture screen size and real size
    else if (
	this->getTexture () != nullptr
	&& (this->getTexture ()->getTextureWidth (0) != this->getTexture ()->getRealWidth ()
	    || this->getTexture ()->getTextureHeight (0) != this->getTexture ()->getRealHeight ())
    ) {
	// Account for padding in non-power-of-two textures: clamp UVs to the real content
	width = static_cast<float> (this->getTexture ()->getRealWidth ())
	    / static_cast<float> (this->getTexture ()->getTextureWidth (0));
	height = static_cast<float> (this->getTexture ()->getRealHeight ())
	    / static_cast<float> (this->getTexture ()->getTextureHeight (0));
    }

    // TODO: RECALCULATE THESE POSITIONS FOR PASSTHROUGH SO THEY TAKE THE RIGHT PART OF THE TEXTURE
    float x = 0.0f;
    float y = 0.0f;

    if (this->getTexture ()->isAnimated ()) {
	// animations should be copied completely
	x = 0.0f;
	y = 0.0f;
	width = 1.0f;
	height = 1.0f;
    }

    GLfloat realWidth = size.x;
    GLfloat realHeight = size.y;
    GLfloat realX = 0.0;
    GLfloat realY = 0.0;

    if (this->getImage ().model->passthrough) {
	// Passthrough shaders fill the destination FBO from texcoords and sample the scene using positions.
	// Keep the destination quad full-screen in local FBO space, but pass scene-space positions through.
	x = 0.0f;
	y = 0.0f;
	width = 1.0f;
	height = 1.0f;
	realX = this->m_pos.x;
	realY = this->m_pos.w;
	realWidth = this->m_pos.z;
	realHeight = this->m_pos.y;

	if (this->getImage ().model->fullscreen) {
	    realX = -1.0;
	    realY = -1.0;
	    realWidth = 1.0;
	    realHeight = 1.0;
	}
    }

    GLfloat texcoordCopy[] = { x, height, x, y, width, height, width, height, x, y, width, y };

    GLfloat copySpacePosition[] = { realX,     realHeight, 0.0f, realX, realY, 0.0f, realWidth, realHeight, 0.0f,
				    realWidth, realHeight, 0.0f, realX, realY, 0.0f, realWidth, realY,      0.0f };

    GLfloat texcoordPass[] = { 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f };

    GLfloat passSpacePosition[]
	= { -1.0, 1.0, 0.0f, -1.0, -1.0, 0.0f, 1.0, 1.0, 0.0f, 1.0, 1.0, 0.0f, -1.0, -1.0, 0.0f, 1.0, -1.0, 0.0f };

    // bind vertex list to the openGL buffers
    glGenBuffers (1, &this->m_sceneSpacePosition);
    glBindBuffer (GL_ARRAY_BUFFER, this->m_sceneSpacePosition);
    glBufferData (GL_ARRAY_BUFFER, sizeof (sceneSpacePosition), sceneSpacePosition, GL_STATIC_DRAW);

    glGenBuffers (1, &this->m_copySpacePosition);
    glBindBuffer (GL_ARRAY_BUFFER, this->m_copySpacePosition);
    glBufferData (GL_ARRAY_BUFFER, sizeof (copySpacePosition), copySpacePosition, GL_STATIC_DRAW);

    // bind pass' vertex list to the openGL buffers
    glGenBuffers (1, &this->m_passSpacePosition);
    glBindBuffer (GL_ARRAY_BUFFER, this->m_passSpacePosition);
    glBufferData (GL_ARRAY_BUFFER, sizeof (passSpacePosition), passSpacePosition, GL_STATIC_DRAW);

    glGenBuffers (1, &this->m_texcoordCopy);
    glBindBuffer (GL_ARRAY_BUFFER, this->m_texcoordCopy);
    glBufferData (GL_ARRAY_BUFFER, sizeof (texcoordCopy), texcoordCopy, GL_STATIC_DRAW);

    glGenBuffers (1, &this->m_texcoordPass);
    glBindBuffer (GL_ARRAY_BUFFER, this->m_texcoordPass);
    glBufferData (GL_ARRAY_BUFFER, sizeof (texcoordPass), texcoordPass, GL_STATIC_DRAW);

    // compute the center of the image in scene space for rotation
    this->m_sceneCenter
	= glm::vec3 ((this->m_pos.x + this->m_pos.z) / 2.0f, (this->m_pos.y + this->m_pos.w) / 2.0f, 0.0f);

    const auto& initialCamera = this->getScene ().getCamera ();
    this->m_modelViewProjectionScreen = initialCamera.isOrthogonal ()
	? initialCamera.getProjection () * initialCamera.getLookAt ()
	: initialCamera.getScreenProjection ();

    if (this->getImage ().model->passthrough) {
	this->m_modelViewProjectionCopy = this->m_modelViewProjectionScreen;
    } else {
	this->m_modelViewProjectionCopy = glm::ortho<float> (0.0, size.x, 0.0, size.y);
    }
    this->m_modelViewProjectionCopyInverse = glm::inverse (this->m_modelViewProjectionCopy);
    this->m_modelMatrix = glm::ortho<float> (0.0, size.x, 0.0, size.y);
    this->m_viewProjectionMatrix = glm::mat4 (1.0);

    // ensure the input texture is marked as used
    // this makes video playback start if it's not already
    this->m_texture->incrementUsageCount ();
}

CImage::~CImage () {
    this->m_texture->decrementUsageCount ();

    for (const auto& maskPass : this->m_puppetMaskPasses) {
	maskPass.source->decrementUsageCount ();
    }
    this->m_puppetMaskPasses.clear ();
    for (const auto& prePass : this->m_puppetPrePasses) {
	if (prePass.retainedTexture != nullptr) {
	    prePass.retainedTexture->decrementUsageCount ();
	}
    }
    this->m_puppetPrePasses.clear ();

    // delete passes first as they depend on the image's data
    for (auto* pass : this->m_passes) {
	delete pass;
    }

    this->m_passes.clear ();

    // free any gl resources
    glDeleteBuffers (1, &this->m_sceneSpacePosition);
    glDeleteBuffers (1, &this->m_copySpacePosition);
    glDeleteBuffers (1, &this->m_passSpacePosition);
    glDeleteBuffers (1, &this->m_texcoordCopy);
    glDeleteBuffers (1, &this->m_texcoordPass);
    if (this->m_puppetSpacePosition != GL_NONE) {
	glDeleteBuffers (1, &this->m_puppetSpacePosition);
    }
    if (this->m_puppetTexCoord != GL_NONE) {
	glDeleteBuffers (1, &this->m_puppetTexCoord);
    }
    if (this->m_puppetIndices != GL_NONE) {
	glDeleteBuffers (1, &this->m_puppetIndices);
    }
}

bool CImage::loadPuppetMesh (glm::vec2& size) {
    if (!this->getImage ().model->puppet.has_value ()) {
	return false;
    }

    try {
	const auto stream = this->getScene ().getScene ().project.assetLocator->read (*this->getImage ().model->puppet);
	std::vector<char> data { std::istreambuf_iterator<char> (*stream), std::istreambuf_iterator<char> () };

	std::string error;
	auto parsed = PuppetModel::parse (data, error);
	if (!parsed.has_value ()) {
	    sLog.error ("Could not parse puppet ", *this->getImage ().model->puppet, ": ", error);
	    return false;
	}
	this->m_puppetModel = std::move (parsed);
	const auto& model = *this->m_puppetModel;

	const size_t vertexCount = model.positions.size ();
	this->m_puppetRawPositions.clear ();
	this->m_puppetRawPositions.reserve (vertexCount * 3);
	std::vector<GLfloat> texcoords;
	texcoords.reserve (vertexCount * 2);
	for (size_t i = 0; i < vertexCount; i++) {
	    this->m_puppetRawPositions.push_back (model.positions[i].x);
	    this->m_puppetRawPositions.push_back (model.positions[i].y);
	    this->m_puppetRawPositions.push_back (model.positions[i].z);
	    texcoords.push_back (model.uvs[i].x);
	    texcoords.push_back (model.uvs[i].y);
	}

	glGenBuffers (1, &this->m_puppetTexCoord);
	glBindBuffer (GL_ARRAY_BUFFER, this->m_puppetTexCoord);
	glBufferData (GL_ARRAY_BUFFER, texcoords.size () * sizeof (GLfloat), texcoords.data (), GL_STATIC_DRAW);

	// Puppet vertices are converted from Wallpaper Engine's image-local Y-down
	// coordinates into our Y-up render space. That reflection reverses triangle
	// winding. Rewind each triangle so materials using normal back-face culling
	// remain visible (most tail puppets use nocull, which hid this bug).
	std::vector<GLushort> rewoundIndices = model.indices;
	for (size_t i = 0; i + 2 < rewoundIndices.size (); i += 3) {
	    std::swap (rewoundIndices[i + 1], rewoundIndices[i + 2]);
	}

	glGenBuffers (1, &this->m_puppetIndices);
	glBindBuffer (GL_ARRAY_BUFFER, this->m_puppetIndices);
	glBufferData (
	    GL_ARRAY_BUFFER, rewoundIndices.size () * sizeof (GLushort), rewoundIndices.data (), GL_STATIC_DRAW
	);

	this->m_puppetIndexCount = static_cast<GLsizei> (rewoundIndices.size ());

	for (const auto& layer : this->getImage ().animationLayers) {
	    const auto clipId = static_cast<uint32_t> (layer->animation->value->getInt ());
	    const auto* clip = model.findClip (clipId);
	    if (clip == nullptr) {
		sLog.out ("Puppet ", *this->getImage ().model->puppet, ": no clip with id ", clipId, ", layer ignored");
		continue;
	    }
	    this->m_puppetLayers.push_back (PuppetLayerBinding { .clip = clip, .layer = layer.get () });
	}

	if (this->getImage ().model->autosize && !this->getImage ().model->fullscreen) {
	    size = this->computePuppetCanvasSize (size);
	}
	this->updatePuppetPositionBuffer (size);

	sLog.out (
	    "Loaded puppet ", *this->getImage ().model->puppet, " vertices=", vertexCount,
	    " indices=", this->m_puppetIndexCount, " bones=", model.bones.size (), " clips=", model.clips.size (),
	    " layers=", this->m_puppetLayers.size ()
	);

	return true;
    } catch (const std::exception& ex) {
	sLog.error ("Could not load puppet mesh ", *this->getImage ().model->puppet, ": ", ex.what ());
	return false;
    }
}

glm::vec2 CImage::computePuppetCanvasSize (const glm::vec2& size) const {
    if (!this->m_puppetModel.has_value ()) {
	return size;
    }

    float maxX = 0.0f;
    float maxY = 0.0f;
    const auto& model = *this->m_puppetModel;

    // Include the bind pose even when a puppet has no usable animation clips.
    for (const auto& position : model.positions) {
	maxX = std::max (maxX, std::abs (position.x));
	maxY = std::max (maxY, std::abs (position.y));
    }

    std::vector<glm::mat4> skin;
    std::vector<glm::vec3> positions;
    std::vector<PuppetModel::ActiveLayer> active (1);
    for (const auto& binding : this->m_puppetLayers) {
	if (binding.clip == nullptr || binding.clip->frameCount == 0) {
	    continue;
	}
	if (!binding.layer->visible->value->getBool ()) {
	    continue;
	}
	// Additive clips store offsets from their rest pose, matching the live renderer.
	active[0] = PuppetModel::ActiveLayer {
	    .clip = binding.clip,
	    .rate = 1.0f,
	    .blend = binding.layer->blend->value->getFloat (),
	    .additive = binding.layer->additive,
	};
	const size_t sampleSteps = static_cast<size_t> (binding.clip->frameCount) * 2;
	const double fps = std::max (static_cast<double> (binding.clip->fps), 1.0);
	for (size_t sample = 0; sample <= sampleSteps; ++sample) {
	    // Half-frame samples catch most between-keyframe movement without a costly
	    // per-pixel or per-render-frame bounds calculation.
	    const double frame = static_cast<double> (binding.clip->frameCount) * sample / sampleSteps;
	    model.evaluateSkinning (active, frame / fps, skin);
	    model.skinPositions (skin, positions);
	    for (const auto& position : positions) {
		maxX = std::max (maxX, std::abs (position.x));
		maxY = std::max (maxY, std::abs (position.y));
	    }
	}
    }

    // Keep one pixel around sampled extrema for interpolation between samples.
    glm::vec2 canvasSize {
	std::max (size.x, std::ceil (maxX + 1.0f) * 2.0f),
	std::max (size.y, std::ceil (maxY + 1.0f) * 2.0f),
    };

    // Puppet keyframes can move far beyond the screen. Keep off-screen travel
    // clipped to the GPU's texture limit instead of downscaling the whole layer.
    GLint maxTextureSize = 0;
    glGetIntegerv (GL_MAX_TEXTURE_SIZE, &maxTextureSize);
    if (maxTextureSize > 0) {
	const float limit = static_cast<float> (maxTextureSize);
	canvasSize = glm::min (canvasSize, glm::vec2 (limit));
    }

    return canvasSize;
}

void CImage::updatePuppetPositionBuffer (const glm::vec2& size) {
    this->uploadPuppetPositions (this->m_puppetRawPositions, size);
}

void CImage::uploadPuppetPositions (const std::vector<GLfloat>& raw, const glm::vec2& size) {
    if (raw.empty ()) {
	return;
    }

    std::vector<GLfloat> positions;
    positions.reserve (raw.size ());
    for (size_t index = 0; index + 2 < raw.size (); index += 3) {
	const float localX = size.x / 2.0f + raw[index];
	const float localY = size.y / 2.0f - raw[index + 1];
	if (this->m_puppetScreenSpace) {
	    const float u = localX / size.x;
	    const float v = localY / size.y;
	    positions.push_back (this->m_pos.x + u * (this->m_pos.z - this->m_pos.x));
	    positions.push_back (this->m_pos.w + v * (this->m_pos.y - this->m_pos.w));
	} else {
	    positions.push_back (localX);
	    positions.push_back (localY);
	}
	// Puppet mesh Z values are local to the model. The local image target and the
	// orthographic scene projection use a [-1, 1] clip-depth range, so carrying
	// those values through clips much of a 2D puppet. Scene-level depth is carried
	// by the image object; preserve local Z only for direct perspective output.
	const bool orthographic = this->getScene ().getCamera ().isOrthogonal ();
	positions.push_back (this->m_puppetScreenSpace && !orthographic ? raw[index + 2] : 0.0f);
    }

    if (this->m_puppetSpacePosition == GL_NONE) {
	glGenBuffers (1, &this->m_puppetSpacePosition);
    }
    glBindBuffer (GL_ARRAY_BUFFER, this->m_puppetSpacePosition);
    glBufferData (GL_ARRAY_BUFFER, positions.size () * sizeof (GLfloat), positions.data (), GL_DYNAMIC_DRAW);
}

void CImage::updatePuppetAnimation () {
    if (!this->m_hasPuppetMesh || !this->m_puppetModel.has_value () || this->m_puppetLayers.empty ()
	|| !this->m_puppetModel->hasAnimation ()) {
	return;
    }

    this->m_puppetActiveScratch.clear ();
    for (const auto& binding : this->m_puppetLayers) {
	if (!binding.layer->visible->value->getBool ()) {
	    continue;
	}
	this->m_puppetActiveScratch.push_back (
	    PuppetModel::ActiveLayer {
		.clip = binding.clip,
		.rate = binding.layer->rate->value->getFloat (),
		.blend = binding.layer->blend->value->getFloat (),
		.additive = binding.layer->additive,
	    }
	);
    }

    this->m_puppetModel->evaluateSkinning (
	this->m_puppetActiveScratch, static_cast<double> (this->getScene ().getTime ()), this->m_puppetSkinMatrices
    );
    this->m_puppetModel->skinPositions (this->m_puppetSkinMatrices, this->m_puppetSkinnedPositions);

    this->m_puppetSkinnedFlat.resize (this->m_puppetSkinnedPositions.size () * 3);
    for (size_t i = 0; i < this->m_puppetSkinnedPositions.size (); i++) {
	this->m_puppetSkinnedFlat[i * 3 + 0] = this->m_puppetSkinnedPositions[i].x;
	this->m_puppetSkinnedFlat[i * 3 + 1] = this->m_puppetSkinnedPositions[i].y;
	this->m_puppetSkinnedFlat[i * 3 + 2] = this->m_puppetSkinnedPositions[i].z;
    }

    this->uploadPuppetPositions (this->m_puppetSkinnedFlat, this->m_size);
}

void CImage::setupPuppetGeometryCallback (Effects::CPass* pass) const {
    pass->setGeometryCallback (
	[this, pass] () {
	    const GLint position = glGetAttribLocation (pass->getProgramID (), "a_Position");
	    const GLint texCoord = glGetAttribLocation (pass->getProgramID (), "a_TexCoord");

	    if (position >= 0) {
		glEnableVertexAttribArray (position);
		glBindBuffer (GL_ARRAY_BUFFER, this->m_puppetSpacePosition);
		glVertexAttribPointer (position, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
	    }

	    if (texCoord >= 0) {
		glEnableVertexAttribArray (texCoord);
		glBindBuffer (GL_ARRAY_BUFFER, this->m_puppetTexCoord);
		glVertexAttribPointer (texCoord, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
	    }
	},
	[this] () {
	    GLint currentFramebuffer = 0;
	    glGetIntegerv (GL_DRAW_FRAMEBUFFER_BINDING, &currentFramebuffer);
	    if (currentFramebuffer != static_cast<GLint> (this->getScene ().getFBO ()->getFramebuffer ())) {
		GLfloat previousClearColor[4] = {};
		glGetFloatv (GL_COLOR_CLEAR_VALUE, previousClearColor);
		glClearColor (0.0f, 0.0f, 0.0f, 0.0f);
		glClear (GL_COLOR_BUFFER_BIT);
		glClearColor (
		    previousClearColor[0], previousClearColor[1], previousClearColor[2], previousClearColor[3]
		);
	    }
	    glBindBuffer (GL_ELEMENT_ARRAY_BUFFER, this->m_puppetIndices);
	    glDrawElements (GL_TRIANGLES, this->m_puppetIndexCount, GL_UNSIGNED_SHORT, nullptr);
	},
	[pass] () {
	    const GLint position = glGetAttribLocation (pass->getProgramID (), "a_Position");
	    const GLint texCoord = glGetAttribLocation (pass->getProgramID (), "a_TexCoord");

	    if (position >= 0) {
		glDisableVertexAttribArray (position);
	    }

	    if (texCoord >= 0) {
		glDisableVertexAttribArray (texCoord);
	    }
	}
    );
}

void CImage::setupPuppetMaskPass (Effects::CPass* effectPass, const int textureIndex) {
    // Reproject source-UV maps through the live puppet mesh before screen-space effects sample them.
    auto source = effectPass->getTexture (textureIndex);
    if (source == nullptr || std::dynamic_pointer_cast<const CFBO> (source) != nullptr) {
	return;
    }

    std::shared_ptr<const CFBO> warpedMask;
    if (const auto existing = this->m_puppetMaskPassBySource.find (source.get ());
	existing != this->m_puppetMaskPassBySource.end ()) {
	warpedMask = this->m_puppetMaskPasses[existing->second].target;
    } else {
	if (this->m_materials.puppetMaskMaterial == nullptr) {
	    this->m_materials.puppetMaskMaterial
		= MaterialParser::load (this->getScene ().getScene ().project, "materials/util/effectpassthrough.json");
	}

	const uint32_t width = std::max (1u, static_cast<uint32_t> (this->m_size.x));
	const uint32_t height = std::max (1u, static_cast<uint32_t> (this->m_size.y));
	auto target = std::make_shared<CFBO> (
	    "_rt_puppetMask_" + std::to_string (this->getImage ().id) + "_"
		+ std::to_string (this->m_puppetMaskPasses.size ()),
	    TextureFormat_ARGB8888, TextureFlags_ClampUVs, 1.0f, width, height, width, height
	);
	auto pass = std::make_unique<Effects::CPass> (
	    *this, std::make_shared<FBOProvider> (nullptr), **this->m_materials.puppetMaskMaterial->passes.begin (),
	    std::nullopt, std::nullopt, std::nullopt
	);
	pass->setBlendingMode (BlendingMode_Normal);
	pass->setDestination (target);
	pass->setInput (source);
	pass->setPosition (this->m_puppetSpacePosition);
	pass->setTexCoord (this->getTexCoordCopy ());
	pass->setModelViewProjectionMatrix (&this->m_modelViewProjectionCopy);
	pass->setModelViewProjectionMatrixInverse (&this->m_modelViewProjectionCopyInverse);
	pass->setModelMatrix (&this->m_modelMatrix);
	pass->setViewProjectionMatrix (&this->m_viewProjectionMatrix);
	pass->addUniform ("g_Color4", &this->m_whiteMaskColor);
	this->setupPuppetGeometryCallback (pass.get ());

	const size_t index = this->m_puppetMaskPasses.size ();
	source->incrementUsageCount ();
	this->m_puppetMaskPasses.push_back (
	    PuppetMaskPass { .source = source, .target = target, .pass = std::move (pass) }
	);
	this->m_puppetMaskPassBySource.emplace (source.get (), index);
	warpedMask = std::move (target);
    }

    effectPass->setTextureOverride (textureIndex, std::move (warpedMask));
}

bool CImage::setupPuppetOpacityPass (Effects::CPass* pass) {
    auto mask = pass->getTexture (1);
    if (mask == nullptr || this->m_prePuppetTexture == nullptr) {
	return false;
    }

    // Opacity masks share the source image's UVs. Apply them before the puppet
    // mesh maps the image into its animated canvas so the mask follows each pixel.
    const auto input = this->m_prePuppetTexture;
    const uint32_t width = std::max (1u, input->getRealWidth ());
    const uint32_t height = std::max (1u, input->getRealHeight ());
    auto target = std::make_shared<CFBO> (
	    "_rt_puppetOpacity_" + std::to_string (this->getImage ().id) + "_"
		+ std::to_string (this->m_puppetPrePasses.size ()),
	TextureFormat_ARGB8888, TextureFlags_ClampUVs, 1.0f, width, height, width, height
    );

    pass->setInput (input);
    pass->setDestination (target);
    pass->setBlendingMode (BlendingMode_Normal);
    pass->setPosition (this->getPassSpacePosition ());
    pass->setTexCoord (input.get () == this->getTexture ().get () ? this->getTexCoordCopy ()
									 : this->getTexCoordPass ());
    pass->setModelViewProjectionMatrix (&this->m_modelViewProjectionPass);
    pass->setModelViewProjectionMatrixInverse (&this->m_modelViewProjectionPassInverse);
    pass->setModelMatrix (&this->m_modelMatrix);
    pass->setViewProjectionMatrix (&this->m_viewProjectionMatrix);

    mask->incrementUsageCount ();
    this->m_puppetPrePasses.push_back (
	PuppetPrePass { .retainedTexture = std::move (mask), .pass = std::unique_ptr<Effects::CPass> (pass) }
    );
    this->m_prePuppetTexture = std::move (target);
    return true;
}

bool CImage::setupPuppetSourceEffectPass (Effects::CPass* pass) {
    if (this->m_prePuppetTexture == nullptr) {
	return false;
    }

    const auto input = this->m_prePuppetTexture;
    const uint32_t width = std::max (1u, input->getRealWidth ());
    const uint32_t height = std::max (1u, input->getRealHeight ());
    auto target = std::make_shared<CFBO> (
	"_rt_puppetPreEffect_" + std::to_string (this->getImage ().id) + "_"
	    + std::to_string (this->m_puppetPrePasses.size ()),
	TextureFormat_ARGB8888, TextureFlags_ClampUVs, 1.0f, width, height, width, height
    );

    // UV-space displacement must happen before skinning so it follows the puppet's bones.
    pass->setInput (input);
    pass->setPreviousInput (input);
    pass->setDestination (target);
    pass->setBlendingMode (BlendingMode_Normal);
    pass->setPosition (this->getPassSpacePosition ());
    pass->setTexCoord (input.get () == this->getTexture ().get () ? this->getTexCoordCopy ()
									 : this->getTexCoordPass ());
    pass->setModelViewProjectionMatrix (&this->m_modelViewProjectionPass);
    pass->setModelViewProjectionMatrixInverse (&this->m_modelViewProjectionPassInverse);
    pass->setModelMatrix (&this->m_modelMatrix);
    pass->setViewProjectionMatrix (&this->m_viewProjectionMatrix);

    this->m_puppetPrePasses.push_back (
	PuppetPrePass { .retainedTexture = nullptr, .pass = std::unique_ptr<Effects::CPass> (pass) }
    );
    this->m_prePuppetTexture = std::move (target);
    return true;
}

void CImage::setup () {
    // do not double-init stuff, that's bad!
    if (this->m_initialized) {
	return;
    }

    // TODO: CHECK ORDER OF THINGS, 2419444134'S ID 27 DEPENDS ON 104'S COMPOSITE_A WHEN OUR LAST RENDER IS ON
    // COMPOSITE_B
    // TODO: SUPPORT PASSTHROUGH (IT'S A SHADER)
    if (this->m_image.model->passthrough) {
	// passthrough images without effects are bad, do not draw them
	if (this->m_image.effects.empty ()) {
	    return;
	}

	// Some have attempted to declare effects with visible set to false.
	bool allEffectsInvisible = true;
	for (const auto& cur : this->m_image.effects) {
	    if (cur->visible->value->getBool ()) {
		allEffectsInvisible = false;
		break;
	    }
	}

	if (allEffectsInvisible) {
	    return;
	}
    }

    const auto& debug = this->getScene ().getContext ().getApp ().getContext ().settings.render.debug;

    this->m_prePuppetTexture = this->getTexture ();

    // copy pass to the composite layer
    for (const auto& cur : this->getImage ().model->material->passes) {
	this->m_passes.push_back (
	    new CPass (*this, std::make_shared<FBOProvider> (this), *cur, std::nullopt, std::nullopt, std::nullopt)
	);
    }

    // prepare the passes list
    if (!debug.baseOnly && !this->getImage ().effects.empty ()) {
	// generate the effects used by this material
	for (const auto& cur : this->m_image.effects) {
	    if (std::find (debug.skipEffects.begin (), debug.skipEffects.end (), static_cast<int> (cur->id))
		!= debug.skipEffects.end ()) {
		continue;
	    }

	    // do not add non-visible effects, this might need some adjustements tho as some effects might not be
	    // visible but affect the output of the image...
	    if (!cur->visible->value->getBool ()) {
		continue;
	    }

	    const auto fboProvider = std::make_shared<FBOProvider> (this);

	    // create all the fbos for this effect
	    for (const auto& fbo : cur->effect->fbos) {
		fboProvider->create (*fbo, this->m_texture->getFlags (), this->getSize ());
	    }

	    // TODO: MAKE USE OF ZIP OPERATOR IN BOOST? WAY OVERKILL JUST FOR THIS...

	    auto curEffect = cur->effect->passes.begin ();
	    auto endEffect = cur->effect->passes.end ();
	    auto curOverride = cur->passOverrides.begin ();
	    auto endOverride = cur->passOverrides.end ();

	    for (; curEffect != endEffect; ++curEffect) {
		if (!(*curEffect)->material.has_value ()) {
		    if (!(*curEffect)->command.has_value ()) {
			sLog.error ("Pass without material and command not supported");
			continue;
		    }

		    if (!(*curEffect)->source.has_value ()) {
			sLog.error ("Pass without material and source not supported");
			continue;
		    }

		    if (!(*curEffect)->target.has_value ()) {
			sLog.error ("Pass without material and target not supported");
			continue;
		    }

		    if ((*curEffect)->command != Command_Copy) {
			sLog.error ("Only copy command is supported for pass without material");
			continue;
		    }

		    auto virtualPass
			= std::make_unique<MaterialPass> (MaterialPass { .blending = BlendingMode_Normal,
									 .cullmode = CullingMode_Disable,
									 .depthtest = DepthtestMode_Disabled,
									 .depthwrite = DepthwriteMode_Disabled,
									 .shader = "commands/copy",
									 .textures = { { 0, *(*curEffect)->source } },
									 .combos = {},
									 .constants = {} });

		    const auto& config = *this->m_virtualPassess.emplace_back (std::move (virtualPass));

		    // build a pass for a copy shader
		    this->m_passes.push_back (new CPass (
			*this, fboProvider, config, std::nullopt, std::nullopt, (*curEffect)->target.value ()
		    ));
		} else {
		    for (auto& pass : (*curEffect)->material.value ()->passes) {
			const auto override = curOverride != endOverride
			    ? **curOverride
			    : std::optional<std::reference_wrapper<const ImageEffectPassOverride>> (std::nullopt);
			const auto target = (*curEffect)->target.has_value ()
			    ? *(*curEffect)->target
			    : std::optional<std::reference_wrapper<std::string>> (std::nullopt);

			auto* effectPass = new CPass (*this, fboProvider, *pass, override, (*curEffect)->binds, target);
			const std::string& shader
			    = override.has_value () && override->get ().shaderOverride.has_value ()
			    ? *override->get ().shaderOverride
			    : pass->shader;
			if (this->m_hasPuppetMesh && shader == "effects/waterwaves"
			    && !(*curEffect)->target.has_value () && this->setupPuppetSourceEffectPass (effectPass)) {
			    continue;
			}
			if (this->m_hasPuppetMesh && shader == "effects/opacity"
			    && this->setupPuppetOpacityPass (effectPass)) {
			    continue;
			}
			if (this->m_hasPuppetMesh) {
			    // Reproject every effect mask from image UVs onto the animated mesh.
			    for (const int textureIndex : { 1, 2, 3 }) {
				if (usesSourceUvMask (shader, textureIndex)) {
				    this->setupPuppetMaskPass (effectPass, textureIndex);
				}
			    }
			}
			this->m_passes.push_back (effectPass);
		    }

		    if (curOverride != endOverride) {
			++curOverride;
		    }
		}
	    }
	}
    }

    if (!debug.baseOnly) {
	const auto magentaCompositeTint = findMagentaCompositeTint (this->m_image, debug.skipEffects);
	if (magentaCompositeTint.has_value ()) {
	    auto tintOverride = std::make_unique<ImageEffectPassOverride> (ImageEffectPassOverride {
		.id = -1,
		.combos = {
		    { "BLENDMODE", 30 },
		},
		.constants = {},
		.textures = {},
	    });
	    tintOverride->constants.emplace ("color", UserSettingBuilder::fromValue (magentaCompositeTint.value ()));
	    tintOverride->constants.emplace ("alpha", UserSettingBuilder::fromValue (1.0f));

	    this->m_materials.compatibilityMaterials.emplace_back (
		MaterialParser::load (this->getScene ().getScene ().project, "materials/effects/tint.json")
	    );
	    this->m_materials.compatibilityOverrides.emplace_back (std::move (tintOverride));

	    this->m_passes.push_back (new CPass (
		*this, std::make_shared<FBOProvider> (this),
		**this->m_materials.compatibilityMaterials.back ()->passes.begin (),
		*this->m_materials.compatibilityOverrides.back (), std::nullopt, std::nullopt
	    ));
	}
    }

    // extra render pass if there's any blending to be done
    if (!debug.baseOnly && this->m_image.colorBlendMode->value->getInt () > 0) {
	this->m_materials.colorBlending.material
	    = MaterialParser::load (this->getScene ().getScene ().project, "materials/util/effectpassthrough.json");
	this->m_materials.colorBlending.override = std::make_unique<ImageEffectPassOverride> (ImageEffectPassOverride {
            .id = -1,
            .combos = {
                {"BLENDMODE", this->m_image.colorBlendMode->value->getInt()},
            },
            .constants = {},
            .textures = {},
        });

	this->m_passes.push_back (new CPass (
	    *this, std::make_shared<FBOProvider> (this), **this->m_materials.colorBlending.material->passes.begin (),
	    *this->m_materials.colorBlending.override, std::nullopt, std::nullopt
	));
    }

    // if there's more than one pass the blendmode has to be moved from the beginning to the end
    if (this->m_passes.size () > 1) {
	const auto first = this->m_passes.begin ();
	const auto last = this->m_passes.rbegin ();

	(*last)->setBlendingMode ((*first)->getBlendingMode ());
	(*first)->setBlendingMode (BlendingMode_Normal);
    }

    CRenderable::setup ();

    this->setupPasses ();
    this->m_initialized = true;
}

void CImage::setupPasses () {
    // do a pass on everything and setup proper inputs and values
    std::shared_ptr<const CFBO> drawTo = this->m_currentMainFBO;
    std::shared_ptr<const TextureProvider> asInput
	= this->m_prePuppetTexture != nullptr ? this->m_prePuppetTexture : this->getTexture ();
    GLuint texcoord = this->getTexCoordCopy ();

    auto cur = this->m_passes.begin ();
    auto end = this->m_passes.end ();
    bool first = true;
    bool inTargetEffectSequence = false;
    std::shared_ptr<const TextureProvider> effectInput = nullptr;

    for (; cur != end; ++cur) {
	// TODO: PROPERLY CHECK EFFECT'S VISIBILITY AND TAKE IT INTO ACCOUNT
	// TODO: THIS REQUIRES ON-THE-FLY EVALUATION OF EFFECTS VISIBILITY TO FIGURE OUT
	// TODO: WHICH ONE IS THE LAST + A FEW OTHER THINGS
	Effects::CPass* pass = *cur;
	std::shared_ptr<const CFBO> prevDrawTo = drawTo;
	bool writesToTarget = false;
	const bool isFirstPass = first;
	const bool usePuppetGeometry = this->m_hasPuppetMesh;
	GLuint spacePosition = (isFirstPass)
	    ? (usePuppetGeometry ? this->m_puppetSpacePosition : this->getCopySpacePosition ())
	    : this->getPassSpacePosition ();
	const glm::mat4* projection
	    = (isFirstPass) ? &this->m_modelViewProjectionCopy : &this->m_modelViewProjectionPass;
	const glm::mat4* inverseProjection
	    = (isFirstPass) ? &this->m_modelViewProjectionCopyInverse : &this->m_modelViewProjectionPassInverse;
	first = false;

	if (isFirstPass && usePuppetGeometry) {
	    pass->setBlendingMode (BlendingMode_Translucent);
	    this->setupPuppetGeometryCallback (pass);
	}

	pass->setModelMatrix (&this->m_modelMatrix);
	pass->setViewProjectionMatrix (&this->m_viewProjectionMatrix);

	writesToTarget = this->configurePassTarget (pass, drawTo, asInput, effectInput, inTargetEffectSequence);
	// determine if it's the last element in the list as this is a screen-copy-like process
	// TODO: PROPERLY CHECK IF THIS IS ALL THAT'S NEEDED
	if (!writesToTarget && this->shouldRenderFinalPass (std::next (cur) == end)) {
	    // TODO: PROPERLY CHECK EFFECT'S VISIBILITY AND TAKE IT INTO ACCOUNT
	    spacePosition = this->getSceneSpacePosition ();
	    drawTo = this->getScene ().getFBO ();
	    projection = &this->m_modelViewProjectionScreen;
	    inverseProjection = &this->m_modelViewProjectionScreenInverse;

	    if (isFirstPass && usePuppetGeometry && !this->m_puppetScreenSpace) {
		this->m_puppetScreenSpace = true;
		this->updatePuppetPositionBuffer (this->m_size);
	    }
	}

	pass->setDestination (drawTo);
	pass->setInput (asInput);
	pass->setPreviousInput (inTargetEffectSequence ? effectInput : nullptr);
	pass->setPosition (spacePosition);
	pass->setTexCoord (texcoord);
	pass->setModelViewProjectionMatrix (projection);
	pass->setModelViewProjectionMatrixInverse (inverseProjection);

	texcoord = this->getTexCoordPass ();

	if (writesToTarget) {
	    asInput = drawTo;
	    drawTo = prevDrawTo;
	} else {
	    drawTo = prevDrawTo;
	    this->pinpongFramebuffer (&drawTo, &asInput);
	    inTargetEffectSequence = false;
	    effectInput = nullptr;
	}
    }
}

bool CImage::shouldRenderFinalPass (bool isLastPass) const {
    if (!isLastPass || !this->getImage ().visible->value->getBool ()) {
	return false;
    }

    const auto& debug = this->getScene ().getContext ().getApp ().getContext ().settings.render.debug;
    return !(debug.noSolidFinal && this->getImage ().model->solidlayer);
}

bool CImage::configurePassTarget (
    Effects::CPass* pass, std::shared_ptr<const CFBO>& drawTo, const std::shared_ptr<const TextureProvider>& asInput,
    std::shared_ptr<const TextureProvider>& effectInput, bool& inTargetEffectSequence
) {
    if (!pass->getTarget ().has_value ()) {
	return false;
    }

    const std::string target = pass->getTarget ().value ();
    std::shared_ptr<const CFBO> resolved = pass->getFBOProvider ()->find (target);
    if (resolved == nullptr) {
	resolved = this->getScene ().findFBO (target);
    }
    if (resolved == nullptr) {
	sLog.error (
	    "Pass target FBO '", target, "' could not be resolved for object ", pass->getRenderable ().getId (),
	    " shader=", pass->getPass ().shader
	);
	return false;
    }

    if (!inTargetEffectSequence) {
	effectInput = asInput;
	inTargetEffectSequence = true;
    }
    drawTo = resolved;
    return true;
}

void CImage::pinpongFramebuffer (std::shared_ptr<const CFBO>* drawTo, std::shared_ptr<const TextureProvider>* asInput) {
    // temporarily store FBOs used
    std::shared_ptr<const CFBO> currentMainFBO = this->m_currentMainFBO;
    std::shared_ptr<const CFBO> currentSubFBO = this->m_currentSubFBO;

    if (drawTo != nullptr) {
	*drawTo = currentSubFBO;
    }
    if (asInput != nullptr) {
	*asInput = currentMainFBO;
    }

    // swap the FBOs
    this->m_currentMainFBO = currentSubFBO;
    this->m_currentSubFBO = currentMainFBO;
}

void CImage::render () {
    // do not try to render something that did not initialize successfully
    if (!this->m_initialized) {
	return;
    }

    if (!this->getImage ().visible->value->getBool ()) {
	return;
    }

    this->updateAlphaAnimation ();
    this->m_color4Cache = this->m_image.color->value->getVec4 ();
    this->m_color4Cache.a *= this->getAlpha ();

    glColorMask (true, true, true, true);

    // Always update screen transform (handles rotation + parallax dynamically)
    this->updateScreenSpacePosition ();

    // Puppet meshes are skinned from the authored animation layers every frame.
    this->updatePuppetAnimation ();

#if !NDEBUG
    std::string str = "Image ";

    if (this->getScene ().getScene ().camera.bloom.enabled->value->getBool () && this->getId () == -1) {
	str += "bloom";
    } else {
	str += this->getImage ().name + " (" + std::to_string (this->getId ()) + ", "
	    + this->getImage ().model->material->filename + ")";
    }

    glPushDebugGroup (GL_DEBUG_SOURCE_APPLICATION, 0, -1, str.c_str ());
#endif /* DEBUG */

    // Apply source-space effects before skinning, then update auxiliary maps with
    // the same skinned positions as the image pass.
    for (const auto& prePass : this->m_puppetPrePasses) {
	prePass.pass->render ();
    }
    for (const auto& maskPass : this->m_puppetMaskPasses) {
	maskPass.pass->render ();
    }

    auto cur = this->m_passes.begin ();

    for (const auto end = this->m_passes.end (); cur != end; ++cur) {
	if (std::next (cur) == end) {
	    glColorMask (true, true, true, this->getScene ().isRenderingToComposition () ? GL_TRUE : GL_FALSE);
	}

	(*cur)->render ();
    }

#if !NDEBUG
    glPopDebugGroup ();
#endif /* DEBUG */
}

void CImage::updateAlphaAnimation () {
    if (!this->m_image.alphaAnimation) {
	return;
    }

    const auto& animation = *this->m_image.alphaAnimation;
    const float frame = std::clamp (this->getScene ().getTime () * PropertyAnimation::FPS, 0.0f, animation.maxFrame);
    const float authoredAlpha = normalizeImageAlpha (this->m_image.alpha->value->getFloat ());
    this->m_animatedAlpha = normalizeImageAlpha (evalAnimationChannel (animation.channels[0], frame, authoredAlpha));
}

const float& CImage::getBrightness () const { return this->m_image.brightness->value->getFloat (); }

const float& CImage::getUserAlpha () const {
	this->m_normalizedAlpha = normalizeImageAlpha (
	    this->m_image.alphaAnimation ? this->m_animatedAlpha : this->m_image.alpha->value->getFloat ()
	);
	return this->m_normalizedAlpha;
}

const float& CImage::getAlpha () const {
	return this->getUserAlpha ();
}

const glm::vec3& CImage::getColor () const { return this->m_image.color->value->getVec3 (); }

const glm::vec4& CImage::getColor4 () const { return this->m_color4Cache; }

const glm::vec3& CImage::getCompositeColor () const { return this->m_image.color->value->getVec3 (); }

glm::vec2 CImage::resolveGeometrySize (float sceneWidth, float sceneHeight, glm::vec3& origin) const {
    glm::vec2 size = this->getSize ();

    if ((size.x == 0.0f || size.y == 0.0f) && this->m_texture != nullptr) {
	size.x = static_cast<float> (this->m_texture->getRealWidth ());
	size.y = static_cast<float> (this->m_texture->getRealHeight ());
    } else if (
	(size.x == 0.0f || size.y == 0.0f) && this->getImage ().model->width.has_value ()
	&& this->getImage ().model->height.has_value ()
    ) {
	size.x = static_cast<float> (this->getImage ().model->width.value ());
	size.y = static_cast<float> (this->getImage ().model->height.value ());
    }

    if (this->getImage ().model->fullscreen) {
	size = { sceneWidth, sceneHeight };
	origin = { sceneWidth / 2.0f, sceneHeight / 2.0f, 0.0f };
    }

    return size;
}

void CImage::updateScenePosition (
    const glm::vec3& origin, const glm::vec2& size, const glm::vec3& scale, float sceneWidth, float sceneHeight
) {
    const glm::vec2 scaledSize = size * glm::vec2 (scale);
    this->m_pos.x = origin.x - (scaledSize.x / 2.0f);
    this->m_pos.w = origin.y + (scaledSize.y / 2.0f);
    this->m_pos.z = origin.x + (scaledSize.x / 2.0f);
    this->m_pos.y = origin.y - (scaledSize.y / 2.0f);

    if (this->getImage ().alignment.find ("top") != std::string::npos) {
	this->m_pos.y -= scaledSize.y / 2.0f;
	this->m_pos.w -= scaledSize.y / 2.0f;
    } else if (this->getImage ().alignment.find ("bottom") != std::string::npos) {
	this->m_pos.y += scaledSize.y / 2.0f;
	this->m_pos.w += scaledSize.y / 2.0f;
    }

    if (this->getImage ().alignment.find ("left") != std::string::npos) {
	this->m_pos.x += scaledSize.x / 2.0f;
	this->m_pos.z += scaledSize.x / 2.0f;
    } else if (this->getImage ().alignment.find ("right") != std::string::npos) {
	this->m_pos.x -= scaledSize.x / 2.0f;
	this->m_pos.z -= scaledSize.x / 2.0f;
    }

    this->m_pos.x -= sceneWidth / 2.0f;
    this->m_pos.y = sceneHeight / 2.0f - this->m_pos.y;
    this->m_pos.z -= sceneWidth / 2.0f;
    this->m_pos.w = sceneHeight / 2.0f - this->m_pos.w;
}

void CImage::uploadGeometryBuffers (const glm::vec2& size) {
    GLfloat sceneSpacePosition[] = { this->m_pos.x, this->m_pos.y, 0.0f, this->m_pos.x, this->m_pos.w, 0.0f,
				     this->m_pos.z, this->m_pos.y, 0.0f, this->m_pos.z, this->m_pos.y, 0.0f,
				     this->m_pos.x, this->m_pos.w, 0.0f, this->m_pos.z, this->m_pos.w, 0.0f };

    float width = 1.0f;
    float height = 1.0f;
    if (this->getTexture () != nullptr && !this->getTexture ()->isAnimated ()
	&& (this->getTexture ()->getTextureWidth (0) != this->getTexture ()->getRealWidth ()
	    || this->getTexture ()->getTextureHeight (0) != this->getTexture ()->getRealHeight ())) {
	width = static_cast<float> (this->getTexture ()->getRealWidth ())
	    / static_cast<float> (this->getTexture ()->getTextureWidth (0));
	height = static_cast<float> (this->getTexture ()->getRealHeight ())
	    / static_cast<float> (this->getTexture ()->getTextureHeight (0));
    }

    float x = 0.0f;
    float y = 0.0f;
    GLfloat realWidth = size.x;
    GLfloat realHeight = size.y;
    GLfloat realX = 0.0f;
    GLfloat realY = 0.0f;

    if (this->getImage ().model->passthrough) {
	width = 1.0f;
	height = 1.0f;
	realX = this->m_pos.x;
	realY = this->m_pos.w;
	realWidth = this->m_pos.z;
	realHeight = this->m_pos.y;

	if (this->getImage ().model->fullscreen) {
	    realX = -1.0f;
	    realY = -1.0f;
	    realWidth = 1.0f;
	    realHeight = 1.0f;
	}
    }

    GLfloat texcoordCopy[] = { x, height, x, y, width, height, width, height, x, y, width, y };
    GLfloat copySpacePosition[] = { realX,     realHeight, 0.0f, realX, realY, 0.0f, realWidth, realHeight, 0.0f,
				    realWidth, realHeight, 0.0f, realX, realY, 0.0f, realWidth, realY,      0.0f };

    glBindBuffer (GL_ARRAY_BUFFER, this->m_sceneSpacePosition);
    glBufferData (GL_ARRAY_BUFFER, sizeof (sceneSpacePosition), sceneSpacePosition, GL_DYNAMIC_DRAW);
    glBindBuffer (GL_ARRAY_BUFFER, this->m_copySpacePosition);
    glBufferData (GL_ARRAY_BUFFER, sizeof (copySpacePosition), copySpacePosition, GL_DYNAMIC_DRAW);
    glBindBuffer (GL_ARRAY_BUFFER, this->m_texcoordCopy);
    glBufferData (GL_ARRAY_BUFFER, sizeof (texcoordCopy), texcoordCopy, GL_DYNAMIC_DRAW);

    this->m_sceneCenter
	= glm::vec3 ((this->m_pos.x + this->m_pos.z) / 2.0f, (this->m_pos.y + this->m_pos.w) / 2.0f, 0.0f);
    this->m_modelViewProjectionCopy = this->getImage ().model->passthrough
	? this->m_modelViewProjectionScreen
	: glm::ortho<float> (0.0, size.x, 0.0, size.y);
    this->m_modelViewProjectionCopyInverse = glm::inverse (this->m_modelViewProjectionCopy);
    this->m_modelMatrix = glm::ortho<float> (0.0, size.x, 0.0, size.y);
}

CImage::ResolvedTransform CImage::updateGeometryBuffers () {
    auto sceneWidth = static_cast<float> (this->getScene ().getWidth ());
    auto sceneHeight = static_cast<float> (this->getScene ().getHeight ());
    const auto transform = this->resolveTransform (this->getImage ());
    glm::vec3 origin = transform.origin;
    const glm::vec3 scale = transform.scale;
    const glm::vec2 size = this->resolveGeometrySize (sceneWidth, sceneHeight, origin);
    const glm::vec2 previousSize = this->m_size;
    this->m_size = size;
    if (this->m_hasPuppetMesh && size != previousSize) {
	this->updatePuppetPositionBuffer (size);
    }

    this->updateScenePosition (origin, size, scale, sceneWidth, sceneHeight);
    this->uploadGeometryBuffers (size);
    return transform;
}

void CImage::updateScreenSpacePosition () {
    const ResolvedTransform transform = this->updateGeometryBuffers ();
    const glm::vec3 angles = this->getImage ().angles->value->getVec3 ();

    // Angles are in radians. Negate X and Z to account for the Y-flipped coordinate system.
    const float angle = transform.angle;
    glm::mat4 rotModel = glm::mat4 (1.0f);
    if (angle != 0.0f || angles.x != 0.0f || angles.y != 0.0f) {
	rotModel = glm::translate (rotModel, this->m_sceneCenter);
	rotModel = glm::rotate (rotModel, -angle, glm::vec3 (0.0f, 0.0f, 1.0f));
	rotModel = glm::rotate (rotModel, angles.y, glm::vec3 (0.0f, 1.0f, 0.0f));
	rotModel = glm::rotate (rotModel, -angles.x, glm::vec3 (1.0f, 0.0f, 0.0f));
	rotModel = glm::translate (rotModel, -this->m_sceneCenter);
    }

    const auto& camera = this->getScene ().getCamera ();
    glm::mat4 mvp
	= camera.isOrthogonal () ? camera.getProjection () * camera.getLookAt () : camera.getScreenProjection ();

    // Apply parallax displacement if enabled
    if (this->getScene ().getScene ().camera.parallax.enabled
	&& this->getScene ().getContext ().getApp ().getContext ().settings.mouse.enabled
	&& !this->getScene ().getContext ().getApp ().getContext ().settings.mouse.disableparallax) {
	const double parallaxAmount = this->getScene ().getScene ().camera.parallax.amount->value->getFloat ();
	const glm::vec2 depth = this->getImage ().parallaxDepth->value->getVec2 ();
	const glm::vec2* displacement = this->getScene ().getParallaxDisplacement ();
	const float x = (depth.x + parallaxAmount) * displacement->x * this->m_size.x;
	const float y = (depth.y + parallaxAmount) * displacement->y * this->m_size.y;
	mvp = glm::translate (mvp, { x, y, 0.0f });
    }

    // Rotate the image first so parallax stays aligned with the screen axes.
    mvp *= rotModel;
    this->m_modelViewProjectionScreen = mvp;
    this->m_modelViewProjectionScreenInverse = glm::inverse (mvp);
    if (this->getImage ().model->passthrough) {
	this->m_modelViewProjectionCopy = this->m_modelViewProjectionScreen;
	this->m_modelViewProjectionCopyInverse = this->m_modelViewProjectionScreenInverse;
    }
}

const Image& CImage::getImage () const { return this->m_image; }

bool CImage::isCompositionLayer () const {
    return this->m_image.model != nullptr && this->m_image.model->filename == "models/util/composelayer.json";
}

bool CImage::copiesCompositionBackground () const { return this->m_image.copyBackground; }

std::shared_ptr<const CFBO> CImage::getCompositionFBO () const { return this->m_compositionFBO; }

std::optional<ScriptableObject::AnimationLayerProperties> CImage::findAnimationLayer (const std::string& name) const {
    for (const auto& binding : this->m_puppetLayers) {
	if (binding.clip == nullptr || binding.layer == nullptr || binding.clip->name != name
	    || binding.layer->rate == nullptr || binding.layer->rate->value == nullptr
	    || binding.layer->visible == nullptr || binding.layer->visible->value == nullptr) {
	    continue;
	}
	return ScriptableObject::AnimationLayerProperties { .rate = binding.layer->rate->value.get (),
							    .visible = binding.layer->visible->value.get () };
    }
    return std::nullopt;
}

std::optional<glm::mat4> CImage::getPuppetAttachmentMatrix (const std::string& name) const {
    if (!this->m_puppetModel.has_value () || this->m_puppetModel->bones.empty ()) {
	return std::nullopt;
    }

    const auto* attachment = this->m_puppetModel->findAttachment (name);
    if (attachment == nullptr) {
	return std::nullopt;
    }

    std::vector<PuppetModel::ActiveLayer> active;
    active.reserve (this->m_puppetLayers.size ());
    for (const auto& binding : this->m_puppetLayers) {
	if (!binding.layer->visible->value->getBool ()) {
	    continue;
	}
	active.push_back (
	    PuppetModel::ActiveLayer {
		.clip = binding.clip,
		.rate = binding.layer->rate->value->getFloat (),
		.blend = binding.layer->blend->value->getFloat (),
		.additive = binding.layer->additive,
	    }
	);
    }

    std::vector<glm::mat4> world;
    this->m_puppetModel->evaluateWorldPose (active, static_cast<double> (this->getScene ().getTime ()), world);
    if (attachment->bone >= world.size ()) {
	return std::nullopt;
    }
    return world[attachment->bone] * attachment->local;
}

glm::vec2 CImage::getSize () const {
    if (this->m_size.x > 0.0f && this->m_size.y > 0.0f) {
	// Effect targets must match the resolved canvas, including puppet autosize.
	return this->m_size;
    }
    const glm::vec2 authored = this->getImage ().size;
    if (authored.x > 0.0f && authored.y > 0.0f) {
	return authored;
    }
    if (this->m_texture != nullptr) {
	return { this->m_texture->getRealWidth (), this->m_texture->getRealHeight () };
    }
    return authored;
}

GLuint CImage::getSceneSpacePosition () const { return this->m_sceneSpacePosition; }

GLuint CImage::getCopySpacePosition () const { return this->m_copySpacePosition; }

GLuint CImage::getPassSpacePosition () const { return this->m_passSpacePosition; }

GLuint CImage::getTexCoordCopy () const { return this->m_texcoordCopy; }

GLuint CImage::getTexCoordPass () const { return this->m_texcoordPass; }
