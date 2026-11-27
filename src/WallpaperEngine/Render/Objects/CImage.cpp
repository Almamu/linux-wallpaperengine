#include "CImage.h"

#include "CRenderable.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <limits>
#include <optional>
#include <ranges>
#include <sstream>
#include <string_view>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/rotate_vector.hpp>
#undef GLM_ENABLE_EXPERIMENTAL

#include "WallpaperEngine/Data/Model/DynamicValue.h"
#include "WallpaperEngine/Data/Model/Material.h"
#include "WallpaperEngine/Data/Model/MdlMorph.h"
#include "WallpaperEngine/Data/Model/Object.h"
#include "WallpaperEngine/Data/Model/UserSetting.h"
#include "WallpaperEngine/Data/Parsers/MaterialParser.h"
#include "WallpaperEngine/Data/Parsers/MdlAnimationParser.h"
#include "WallpaperEngine/Data/Utils/BinaryReader.h"
#include "WallpaperEngine/Data/Utils/MemoryStream.h"
#include "WallpaperEngine/Logging/Log.h"
#include "WallpaperEngine/Render/CTexture.h"
#include "WallpaperEngine/Render/Utils/NormalMatrix.h"
#include "WallpaperEngine/Scripting/ScriptEngine.h"

using namespace WallpaperEngine;
using namespace WallpaperEngine::Render::Objects;
using namespace WallpaperEngine::Render::Objects::Effects;
using namespace WallpaperEngine::Data::Model;
using namespace WallpaperEngine::Data::Parsers;
using namespace WallpaperEngine::Data::Builders;
using namespace WallpaperEngine::Data::Utils;

namespace {
const glm::vec4 neutralCompositeColor (1.0f);

bool usesLegacyAnimationUvs (const Render::TextureProvider& texture) {
    const auto* compiled = dynamic_cast<const Render::CTexture*> (&texture);
    if (compiled == nullptr || !compiled->isAnimated ()) return false;
    const auto version = compiled->getHeader ().animatedVersion;
    return version == Data::Assets::AnimatedVersion_TEXS0001 || version == Data::Assets::AnimatedVersion_TEXS0002;
}

glm::vec2 rotateVec2 (const glm::vec2& value, float angle) {
    const float cosAngle = std::cos (angle);
    const float sinAngle = std::sin (angle);
    return { value.x * cosAngle - value.y * sinAngle, value.x * sinAngle + value.y * cosAngle };
}

// rotate a scene-space vector by unflipped scene angles (z, then y, then x applied to the
// vector) — reduces to rotateVec2 when only z is set, which keeps z-only parent chains
// byte-identical to the previous behaviour
glm::vec3 rotateVec3 (const glm::vec3& value, const glm::vec3& angles) {
    if (angles.x == 0.0f && angles.y == 0.0f) {
	const glm::vec2 rotated = rotateVec2 ({ value.x, value.y }, angles.z);
	return { rotated.x, rotated.y, value.z };
    }

    glm::mat4 rotation = glm::mat4 (1.0f);
    rotation = glm::rotate (rotation, angles.z, glm::vec3 (0.0f, 0.0f, 1.0f));
    rotation = glm::rotate (rotation, angles.y, glm::vec3 (0.0f, 1.0f, 0.0f));
    rotation = glm::rotate (rotation, angles.x, glm::vec3 (1.0f, 0.0f, 0.0f));
    return { rotation * glm::vec4 (value, 0.0f) };
}

bool isMagentaNeonTint (const glm::vec3& color) { return color.r > 0.55f && color.g < 0.25f && color.b > 0.45f; }

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

}

CImage::ResolvedTransform CImage::localTransform (const Object& object, double time) {
    // Keyframed transform properties are sampled as a unit. Wallpaper Engine's
    // generic property parser allows animation on origin, scale, and angles.
    const glm::vec3 origin = object.origin->evaluateVec3 (time);
    glm::vec3 scale = glm::vec3 (1.0f);
    glm::vec3 angles = glm::vec3 (0.0f);

    if (object.is<Image> ()) {
	const auto* image = object.as<Image> ();
	scale = image->scale->evaluateVec3 (time);
	angles = image->angles->evaluateVec3 (time);
    } else if (object.is<Text> ()) {
	const auto* text = object.as<Text> ();
	scale = text->scale->evaluateVec3 (time);
	angles = object.groupAngles->evaluateVec3 (time);
    } else {
	scale = object.groupScale->evaluateVec3 (time);
	angles = object.groupAngles->evaluateVec3 (time);
    }

    return { origin, scale, angles };
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
    const double time = this->getScene ().getElapsedTime ();
    const auto compose = [] (const ResolvedTransform& parent, const ResolvedTransform& local) {
	const glm::vec3 offset = rotateVec3 (
	    { local.origin.x * parent.scale.x, local.origin.y * parent.scale.y, local.origin.z * parent.scale.z },
	    parent.angles
	);
	return ResolvedTransform {
	    .origin = parent.origin + offset,
	    .scale = local.scale * parent.scale,
	    .angles = local.angles + parent.angles,
	};
    };

    ResolvedTransform resolved = localTransform (*chain[count - 1], time);
    for (int i = count - 2; i >= 0; --i) {
	const Object& child = *chain[i];
	if (child.attachment.has_value () && child.parent.has_value ()) {
	    const auto* parentObject = this->getScene ().getObject (child.parent.value ());
	    const auto* parentImage = dynamic_cast<const CImage*> (parentObject);
	    if (parentImage != nullptr) {
		const auto attachment = parentImage->puppetAttachmentTransform (*child.attachment);
		if (attachment.has_value ()) {
		    resolved = compose (resolved, *attachment);
		}
	    }
	}

	resolved = compose (resolved, localTransform (child, time));
    }

    return resolved;
}

CImage::CImage (Wallpapers::CScene& scene, const Image& image) :
    CObject (scene, image), CRenderable (scene, image, *image.model->material), ScriptableObject (scene, image),
    m_sceneSpacePosition (GL_NONE), m_copySpacePosition (GL_NONE), m_passSpacePosition (GL_NONE),
    m_texcoordCopy (GL_NONE), m_texcoordPass (GL_NONE), m_modelViewProjectionScreen (),
    m_modelViewProjectionPass (glm::mat4 (1.0)), m_modelViewProjectionCopy (), m_modelViewProjectionScreenInverse (),
    m_modelViewProjectionPassInverse (glm::inverse (m_modelViewProjectionPass)), m_modelViewProjectionCopyInverse (),
    m_modelMatrix (), m_viewProjectionMatrix (), m_image (image), m_resolvedAlpha (image.alpha->value->getFloat ()),
    m_pos (), m_initialized (false) {
    // register any properties in use on this object
    this->registerProperty ("origin", *image.origin);
    this->registerProperty ("scale", *image.scale);
    this->registerProperty ("angles", *image.angles);
    this->registerProperty ("visible", *image.visible);
    this->registerProperty ("ledsource", *image.ledSource);
    this->registerProperty ("alpha", *image.alpha);
    this->registerProperty ("color", *image.color);
    this->registerProperty ("parallaxDepth", *image.parallaxDepth);
    this->registerProperty ("nointerpolation", *image.noInterpolation);
    this->registerProperty ("perspective", *image.perspective);

    // get scene width and height to calculate positions
    auto scene_width = static_cast<float> (scene.getWidth ());
    auto scene_height = static_cast<float> (scene.getHeight ());

    const auto transform = this->resolveTransform (this->getImage ());
    glm::vec3 origin = transform.origin;
    glm::vec2 size = this->getSize ();
    glm::vec3 scale = transform.scale;
    const uint32_t interpolationFlags
	= image.noInterpolation->value->getBool () ? TextureFlags_NoInterpolation : TextureFlags_NoFlags;

    // Native composition layers have two authored forms. A layer with children
    // owns an isolated subtree surface; a childless layer is a full-frame stack
    // effect and must keep sampling the shared _rt_FullFrameBuffer. Unconditionally
    // shadowing that target leaves childless water/ripple layers with an empty input.
    if (this->isCompositionLayer () && scene.hasAuthoredChildren (image.id)) {
	const glm::vec2 compositionSize = scene.getOutputSize ();
	this->m_compositionFBO = this->create (
	    "_rt_compositionLayer_" + std::to_string (image.id), scene.getColorFormat (),
	    TextureFlags_ClampUVs | interpolationFlags, 1.0f, compositionSize, compositionSize, true, false, 1, scene.getCamera ().usesReverseDepth ()
	);
	scene.publish (this->m_compositionFBO->getName (), this->m_compositionFBO);
	// Every pass created for this image receives an FBOProvider parented to the
	// image, so this alias also redirects an authored texture slot named
	// _rt_FullFrameBuffer, not only CRenderable's primary input.
	this->alias ("_rt_FullFrameBuffer", this->m_compositionFBO);
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
	    "", TextureFormat_ARGB8888, TextureFlags_ClampUVs, 1, size.x, size.y, size.x, size.y
	);
    }

    // If the wallpaper doesn't specify a size, fall back to the texture or model dimensions
    if ((size.x == 0.0f || size.y == 0.0f) && this->m_texture != nullptr) {
	const auto* sourceTarget = dynamic_cast<const CFBO*> (this->m_texture.get ());
	if (sourceTarget != nullptr && (sourceTarget->getName () == "_rt_FullFrameBuffer"
	    || sourceTarget->getName ().starts_with ("_rt_compositionLayer_"))) {
	    // Scene storage follows output pixels; an implicit layer quad still spans
	    // the authored canvas, independently of that storage resolution.
	    size = { scene_width, scene_height };
	} else {
	    size.x = static_cast<float> (this->m_texture->getRealWidth ());
	    size.y = static_cast<float> (this->m_texture->getRealHeight ());
	}
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
    this->m_size = size;
    // Effects use source-image pixels even when the layer stretches that image
    // to a different geometric size. Using the quad size here shrinks overlays
    // and misplaces pixel offsets (for example, notes on Ocarina's staff).
    // Procedural solids and local passthrough targets retain their authored extent.
    m_effectSize = size;
    if (!image.model->fullscreen && !image.model->passthrough && !isCompositionLayer ()
	&& !m_material.passes.empty ()) {
	const auto source = m_material.passes.front ()->textures.find (0);
	if (source != m_material.passes.front ()->textures.end ()
	    && !source->second.starts_with ("_rt_") && !source->second.starts_with ("_alias_")
	    && !(image.model->solidlayer && source->second == "util/white")) {
	    m_effectSize = { m_texture->getRealWidth (), m_texture->getRealHeight () };
	}
    }
    if (image.model->fullscreen || this->m_compositionFBO != nullptr) {
	m_effectSize = scene.getOutputSize ();
    } else {
	// Native Half quality reduces local image effects even for single-mip sources.
	// Geometry, authored source UVs and scene-sized render targets stay unchanged.
	m_effectSize /= static_cast<float> (scene.getTextureReduction ());
	if (scene.getTextureReduction () > 1) {
	    // Native divides integer source extents before target/render-scale rounding.
	    m_effectSize = glm::floor (m_effectSize);
	}
    }

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

    this->m_currentMainFBO = this->m_mainFBO = this->create (
	nameA.str (), scene.getColorFormat (), this->m_texture->getFlags () | interpolationFlags, 1, m_effectSize, m_effectSize
    );
    this->m_currentSubFBO = this->m_subFBO = this->create (
	nameB.str (), scene.getColorFormat (), this->m_texture->getFlags () | interpolationFlags, 1, m_effectSize, m_effectSize
    );
    scene.publish (this->m_mainFBO->getName (), this->m_mainFBO);
    scene.publish (this->m_subFBO->getName (), this->m_subFBO);

    // build a list of vertices, these might need some change later (or maybe invert the camera)
    GLfloat sceneSpacePosition[] = { this->m_pos.x, this->m_pos.y, 0.0f, this->m_pos.x, this->m_pos.w, 0.0f,
				     this->m_pos.z, this->m_pos.y, 0.0f, this->m_pos.z, this->m_pos.y, 0.0f,
				     this->m_pos.x, this->m_pos.w, 0.0f, this->m_pos.z, this->m_pos.w, 0.0f };

    float width = 1.0f;
    float height = 1.0f;

    if (this->getTexture ()->isAnimated ()) {
	// Legacy quads use TEXI image/storage dimensions. Page dimensions belong
	// only to the affine frame transform; using them here crops the frame twice.
	width = static_cast<float> (this->getTexture ()->getRealWidth ())
	    / this->getTexture ()->getResolution ()->x;
	height = static_cast<float> (this->getTexture ()->getRealHeight ())
	    / this->getTexture ()->getResolution ()->y;
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

    if (this->getTexture ()->isAnimated () && !usesLegacyAnimationUvs (*this->getTexture ())) {
	// TEXS3 uses the complete frame basis; older sheets retain the authored input UV crop.
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
    glGenBuffers (1, &this->m_effectSpacePosition);
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

    this->m_hasPuppetMesh = this->loadPuppetMesh (size);

    // compute the center of the image in scene space for rotation
    this->m_sceneCenter
	= glm::vec3 ((this->m_pos.x + this->m_pos.z) / 2.0f, (this->m_pos.y + this->m_pos.w) / 2.0f, 0.0f);

    this->m_modelViewProjectionScreen
	= this->getScene ().getCamera ().getRasterProjection () * this->getScene ().getCamera ().getLookAt ();

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

    // delete passes first as they depend on the image's data
    for (auto* pass : this->m_passes) {
	delete pass;
    }

    this->m_passes.clear ();
    delete this->m_puppetAlbedoCopyPass;
    delete this->m_puppetOverlayPass;
    for (auto* pass : this->m_puppetClippingPasses) {
	delete pass;
    }
    this->m_puppetClippingPasses.clear ();

    // free any gl resources
    glDeleteBuffers (1, &this->m_sceneSpacePosition);
    glDeleteBuffers (1, &this->m_copySpacePosition);
    glDeleteBuffers (1, &this->m_effectSpacePosition);
    glDeleteBuffers (1, &this->m_passSpacePosition);
    glDeleteBuffers (1, &this->m_texcoordCopy);
    glDeleteBuffers (1, &this->m_texcoordPass);
    if (this->m_puppetVertexColor != GL_NONE) {
	glDeleteBuffers (1, &this->m_puppetVertexColor);
    }
    if (this->m_puppetSpacePosition != GL_NONE) {
	glDeleteBuffers (1, &this->m_puppetSpacePosition);
    }
    if (this->m_puppetPrelightingPosition != GL_NONE) {
	glDeleteBuffers (1, &this->m_puppetPrelightingPosition);
    }
    if (this->m_puppetTexCoordFirstPass != GL_NONE && this->m_puppetTexCoordFirstPass != this->m_puppetTexCoord) {
	glDeleteBuffers (1, &this->m_puppetTexCoordFirstPass);
    }
    if (this->m_puppetTexCoord != GL_NONE) {
	glDeleteBuffers (1, &this->m_puppetTexCoord);
    }
    if (this->m_puppetIndices != GL_NONE) {
	glDeleteBuffers (1, &this->m_puppetIndices);
    }
    for (GLuint* buffer : { &this->m_puppetOverlayPosition, &this->m_puppetOverlayTexCoord,
			    &this->m_puppetOverlayBlendIndices, &this->m_puppetOverlayIndices }) {
	if (*buffer != GL_NONE) {
	    glDeleteBuffers (1, buffer);
	}
    }
}

bool CImage::loadPuppetMesh (const glm::vec2& size) {
    if (!this->getImage ().model->puppet.has_value () || !this->getImage ().model->puppetMesh.has_value ()) {
	return false;
    }

    try {
	const auto& mesh = *this->getImage ().model->puppetMesh;
	if (mesh.submeshes.empty ()) {
	    throw std::runtime_error ("puppet model has no submeshes");
	}

	const auto& body = mesh.submeshes.front ();
	if (body.strideBytes == 0 || body.vertices.size () * sizeof (float) % body.strideBytes != 0
	    || body.blendIndicesOffset == MdlMesh::AttributeAbsent
	    || body.blendWeightsOffset == MdlMesh::AttributeAbsent || body.uvOffset == MdlMesh::AttributeAbsent) {
	    throw std::runtime_error ("puppet body does not use a supported skinned vertex layout");
	}

	const size_t vertexCount = body.vertices.size () * sizeof (float) / body.strideBytes;
	std::vector<GLfloat> texcoords;
	std::vector<GLushort> indices;

	this->m_puppetRawPositions.clear ();
	this->m_puppetRawPositions.reserve (vertexCount * 3);
	this->m_puppetBlendIndices.clear ();
	this->m_puppetBlendIndices.reserve (vertexCount * 4);
	this->m_puppetBlendWeights.clear ();
	this->m_puppetBlendWeights.reserve (vertexCount * 4);
	texcoords.reserve (vertexCount * 2);
	indices.reserve (body.indices.size ());

	for (size_t index = 0; index < vertexCount; index++) {
	    const auto* vertex
		= reinterpret_cast<const char*> (body.vertices.data ()) + static_cast<ptrdiff_t> (index * body.strideBytes);
	    glm::vec3 position;
	    std::memcpy (&position.x, vertex, sizeof (float));
	    std::memcpy (&position.y, vertex + sizeof (float), sizeof (float));
	    std::memcpy (&position.z, vertex + sizeof (float) * 2, sizeof (float));
	    if (!std::isfinite (position.x) || !std::isfinite (position.y) || !std::isfinite (position.z)) {
		throw std::runtime_error ("puppet vertex contains a non-finite position");
	    }
	    this->m_puppetRawPositions.insert (
		this->m_puppetRawPositions.end (), { position.x, position.y, position.z }
	    );
	    for (int component = 0; component < 4; component++) {
		uint32_t value;
		std::memcpy (
		    &value, vertex + body.blendIndicesOffset + component * sizeof (uint32_t), sizeof (value)
		);
		this->m_puppetBlendIndices.push_back (value);
	    }
	    for (int component = 0; component < 4; component++) {
		float weight;
		std::memcpy (&weight, vertex + body.blendWeightsOffset + component * sizeof (float), sizeof (weight));
		if (!std::isfinite (weight) || weight < 0.0f) {
		    throw std::runtime_error ("puppet vertex contains an invalid blend weight");
		}
		this->m_puppetBlendWeights.push_back (weight);
	    }
	    float u;
	    float v;
	    std::memcpy (&u, vertex + body.uvOffset, sizeof (u));
	    std::memcpy (&v, vertex + body.uvOffset + sizeof (float), sizeof (v));
	    if (!std::isfinite (u) || !std::isfinite (v)) {
		throw std::runtime_error ("puppet vertex contains a non-finite texture coordinate");
	    }
	    texcoords.push_back (u);
	    texcoords.push_back (v);
	}

	for (const auto value : body.indices) {
	    if (value >= vertexCount || value > std::numeric_limits<GLushort>::max ()) {
		sLog.error ("Invalid puppet mesh index ", value, " in ", *this->getImage ().model->puppet);
		return false;
	    }
	    indices.push_back (static_cast<GLushort> (value));
	}

	// MDLV vertices are already assembled even when their texture UVs point into a parts
	// atlas. The shared animation parser supplies MDLS bones, MDAT attachments, and MDLA clips.
	try {
	    this->m_puppetAnimation = MdlAnimationParser::load (
		this->getScene ().getScene ().project, *this->getImage ().model->puppet
	    );
	    const auto bindPose = MdlAnimationEvaluator::evaluate (this->m_puppetAnimation, {});
	    this->m_puppetWorldBones = bindPose.worldBones;
	    this->m_puppetPublishedSceneBones.clear ();
	    sLog.out (
		"Loaded puppet animation data ", *this->getImage ().model->puppet,
		" bones=", this->m_puppetAnimation.bones.size (),
		" attachments=", this->m_puppetAnimation.attachments.size (),
		" clips=", this->m_puppetAnimation.animations.size ()
	    );
	} catch (const std::exception& ex) {
	    sLog.error ("Could not load puppet animation data ", *this->getImage ().model->puppet, ": ", ex.what ());
	    this->m_puppetAnimation = {};
	    this->m_puppetWorldBones.clear ();
	    this->m_puppetPublishedSceneBones.clear ();
	}

	this->m_puppetDrawRanges = { MdlDrawRange { .indexCount = static_cast<uint32_t> (indices.size ()) } };
	this->loadPuppetOverlay (mesh, size);
	this->updatePuppetPositionBuffer (size);

	glGenBuffers (1, &this->m_puppetTexCoord);
	glBindBuffer (GL_ARRAY_BUFFER, this->m_puppetTexCoord);
	glBufferData (GL_ARRAY_BUFFER, texcoords.size () * sizeof (GLfloat), texcoords.data (), GL_STATIC_DRAW);

	// variant compensating for power-of-two padding, used when sampling the source texture directly
	glm::vec2 uvScale (1.0f);
	if (!this->getTexture ()->isAnimated ()
	    && (this->getTexture ()->getTextureWidth (0) != this->getTexture ()->getRealWidth ()
		|| this->getTexture ()->getTextureHeight (0) != this->getTexture ()->getRealHeight ())) {
	    uvScale.x = static_cast<float> (this->getTexture ()->getRealWidth ())
		/ static_cast<float> (this->getTexture ()->getTextureWidth (0));
	    uvScale.y = static_cast<float> (this->getTexture ()->getRealHeight ())
		/ static_cast<float> (this->getTexture ()->getTextureHeight (0));
	}

	if (uvScale == glm::vec2 (1.0f)) {
	    this->m_puppetTexCoordFirstPass = this->m_puppetTexCoord;
	} else {
	    std::vector<GLfloat> scaledTexcoords = texcoords;
	    for (size_t index = 0; index + 1 < scaledTexcoords.size (); index += 2) {
		scaledTexcoords[index] *= uvScale.x;
		scaledTexcoords[index + 1] *= uvScale.y;
	    }

	    glGenBuffers (1, &this->m_puppetTexCoordFirstPass);
	    glBindBuffer (GL_ARRAY_BUFFER, this->m_puppetTexCoordFirstPass);
	    glBufferData (
		GL_ARRAY_BUFFER, scaledTexcoords.size () * sizeof (GLfloat), scaledTexcoords.data (), GL_STATIC_DRAW
	    );
	}

	glGenBuffers (1, &this->m_puppetIndices);
	glBindBuffer (GL_ELEMENT_ARRAY_BUFFER, this->m_puppetIndices);
	glBufferData (GL_ELEMENT_ARRAY_BUFFER, indices.size () * sizeof (GLushort), indices.data (), GL_STATIC_DRAW);

	this->m_puppetIndexCount = static_cast<GLsizei> (indices.size ());
	sLog.out (
	    "Loaded puppet mesh ", *this->getImage ().model->puppet, " version=", mesh.version, " format=0x", std::hex,
	    body.vertexTag, std::dec, " stride=", body.strideBytes, " vertices=", vertexCount,
	    " indices=", this->m_puppetIndexCount
	);

	return true;
    } catch (const std::exception& ex) {
	sLog.error ("Could not load puppet mesh ", *this->getImage ().model->puppet, ": ", ex.what ());
	return false;
    }
}

void CImage::loadPuppetOverlay (const MdlMesh& mesh, const glm::vec2& size) {
    // The overlay lives in a second submesh whose vertices carry a vec4 texcoord (the
    // channelmap atlas uv in xy) instead of the body's skin weights. The container parser
    // reads the whole file; the scanner above only ever finds the first mesh block.
    const MdlSubmesh* overlay = nullptr;
    for (const auto& submesh : mesh.submeshes) {
	if (submesh.texCoordVec4Offset != MdlMesh::AttributeAbsent && !submesh.indices.empty ()) {
	    overlay = &submesh;
	    break;
	}
    }

    if (overlay == nullptr) {
	return;
    }

    const size_t stride = overlay->strideBytes / sizeof (float);
    const size_t vertexCount = stride > 0 ? overlay->vertices.size () / stride : 0;
    if (vertexCount == 0) {
	return;
    }

    std::vector<GLfloat> texcoords;
    std::vector<GLuint> blendIndices;
    std::vector<GLushort> indices;
    std::vector<GLfloat> positions;
    positions.reserve (vertexCount * 3);
    texcoords.reserve (vertexCount * 4);
    blendIndices.reserve (vertexCount * 4);
    indices.reserve (overlay->indices.size ());

    // attribute offsets are in bytes, the vertices are stored as floats
    const size_t uvElement = overlay->texCoordVec4Offset / sizeof (float);
    const size_t blendElement = overlay->blendIndicesOffset != MdlMesh::AttributeAbsent
	? overlay->blendIndicesOffset / sizeof (float)
	: 0;

    for (size_t vertex = 0; vertex < vertexCount; vertex++) {
	const float* base = overlay->vertices.data () + vertex * stride;
	// Channel quads are authored in top-left-origin image pixels. They are texture
	// composition geometry, not skinned body geometry: flip Y into the local FBO's
	// bottom-left coordinate system and keep them entirely in albedo space.
	positions.push_back (base[0]);
	positions.push_back (size.y - base[1]);
	positions.push_back (base[2]);

	for (int component = 0; component < 4; component++) {
	    texcoords.push_back (base[uvElement + component]);
	}

	// the shader picks a g_BlendMap element with these, they are not skin bones
	for (int component = 0; component < 4; component++) {
	    uint32_t value = 0;
	    if (overlay->blendIndicesOffset != MdlMesh::AttributeAbsent) {
		std::memcpy (&value, base + blendElement + component, sizeof (value));
	    }
	    blendIndices.push_back (value);
	}
    }

    for (const auto index : overlay->indices) {
	if (index >= vertexCount) {
	    sLog.error ("Invalid puppet overlay index ", index, " in ", *this->getImage ().model->puppet);
	    return;
	}
	indices.push_back (static_cast<GLushort> (index));
    }

    glGenBuffers (1, &this->m_puppetOverlayPosition);
    glBindBuffer (GL_ARRAY_BUFFER, this->m_puppetOverlayPosition);
    glBufferData (GL_ARRAY_BUFFER, positions.size () * sizeof (GLfloat), positions.data (), GL_STATIC_DRAW);

    glGenBuffers (1, &this->m_puppetOverlayTexCoord);
    glBindBuffer (GL_ARRAY_BUFFER, this->m_puppetOverlayTexCoord);
    glBufferData (GL_ARRAY_BUFFER, texcoords.size () * sizeof (GLfloat), texcoords.data (), GL_STATIC_DRAW);

    glGenBuffers (1, &this->m_puppetOverlayBlendIndices);
    glBindBuffer (GL_ARRAY_BUFFER, this->m_puppetOverlayBlendIndices);
    glBufferData (GL_ARRAY_BUFFER, blendIndices.size () * sizeof (GLuint), blendIndices.data (), GL_STATIC_DRAW);

    glGenBuffers (1, &this->m_puppetOverlayIndices);
    glBindBuffer (GL_ELEMENT_ARRAY_BUFFER, this->m_puppetOverlayIndices);
    glBufferData (GL_ELEMENT_ARRAY_BUFFER, indices.size () * sizeof (GLushort), indices.data (), GL_STATIC_DRAW);

    this->m_puppetOverlayIndexCount = static_cast<GLsizei> (indices.size ());
    this->m_puppetOverlayMaterial = overlay->materialPath;
    sLog.out (
	"Loaded puppet channelmap overlay ", overlay->materialPath, " vertices=", vertexCount,
	" indices=", this->m_puppetOverlayIndexCount
    );
}

const MdlAnimationClip* CImage::findPuppetAnimationClip (const uint32_t id) const {
    const auto found = std::find_if (
	this->m_puppetAnimation.animations.begin (), this->m_puppetAnimation.animations.end (),
	[id] (const MdlAnimationClip& animation) { return animation.id == id; }
    );
    return found != this->m_puppetAnimation.animations.end () ? &*found : nullptr;
}

void CImage::selectPuppetAnimations (const double sceneTime) {
    this->m_puppetActiveLayers.clear ();

    if (this->m_puppetAnimation.animations.empty ()) {
	return;
    }

    // Only scene-assigned clips contribute. An embedded clip can be an unused
    // editor animation; missing or empty animationlayers must keep the rest pose.
    // Keep authored zero-weight layers for stable ordering and playback state.

    for (const auto& layer : this->getImage ().animationLayers) {
	const bool visible = layer->visible->value->getBool ();
	const bool blendIn = layer->blendIn->value->getBool ();
	const bool blendOut = layer->blendOut->value->getBool ();
	const float blendTime = std::max (layer->blendTime->value->getFloat (), 0.0f);
	const float rate = layer->rate->value->getFloat ();
	const auto animationId = static_cast<uint32_t> (layer->animation->value->getInt ());
	const MdlAnimationClip* animation = this->findPuppetAnimationClip (animationId);
	auto& playback = this->m_puppetLayerPlayback[layer->id];
	float deltaTime = 0.0f;

	if (!playback.initialized) {
	    playback.initialized = true;
	    playback.animation = animationId;
	    playback.visible = visible;
	    playback.time = static_cast<float> (sceneTime) * rate;
	    if (animation != nullptr && animation->fps > 0.0f && animation->frameCount > 0) {
		const float duration = animation->frameCount / animation->fps;
		if (animation->mode == "single") {
		    // This initial scene-relative playhead represents elapsed playback, not a seek.
		    playback.finished = playback.playing && !playback.stopped && playback.time >= duration;
		    playback.time = std::clamp (playback.time, 0.0f, duration);
		} else {
		    const float period = duration * (animation->mode == "mirror" ? 2.0f : 1.0f);
		    playback.time = std::fmod (playback.time, period);
		    if (playback.time < 0.0f) playback.time += period;
		    if (animation->mode == "mirror" && playback.time > duration) {
			playback.time = period - playback.time;
			playback.reverse = true;
		    }
		}
	    }
	    playback.lastSceneTime = sceneTime;
	    playback.visibilityWeight = visible && blendIn ? 0.0f : visible ? 1.0f : 0.0f;
	} else {
	    deltaTime = static_cast<float> (sceneTime - playback.lastSceneTime);
	    playback.lastSceneTime = sceneTime;

	    // A timeline seek/reset should seek deterministic scene-driven layers too.
	    if (deltaTime < 0.0f) {
		playback.time = static_cast<float> (sceneTime) * rate;
		playback.reverse = playback.finished = false;
		deltaTime = 0.0f;
	    } else if (playback.playing && !playback.stopped && !playback.finished
		&& animation != nullptr && animation->fps > 0.0f) {
		const float duration = animation->frameCount / animation->fps;
		if (duration > 0.0f && !(animation->mode == "single" && playback.time >= duration)) {
		    // Native stores the folded playhead; getFrame reads this value directly.
		    playback.time += deltaTime * rate * (playback.reverse ? -1.0f : 1.0f);
		    if (animation->mode == "single") {
			if (playback.time >= duration) {
			    playback.time = duration;
			    playback.finished = true;
			}
		    } else if (animation->mode == "mirror") {
			if (!playback.reverse && playback.time >= duration) {
			    playback.time = duration - std::fmod (playback.time, duration);
			    playback.reverse = true;
			} else if (playback.reverse && playback.time <= 0.0f) {
			    playback.time = -std::fmod (playback.time, duration);
			    playback.reverse = false;
			}
		    } else {
			if (playback.time < 0.0f) playback.time = std::fmod (playback.time + duration, duration);
			if (playback.time >= duration) playback.time = std::fmod (playback.time, duration);
		    }
		}
	    }

	    if (playback.animation != animationId) {
		playback.animation = animationId;
		playback.time = 0.0f;
		playback.stopped = false;
		playback.reverse = playback.finished = false;
	    }

	    if (visible != playback.visible) {
		playback.visible = visible;
		if ((visible && !blendIn) || (!visible && !blendOut)) {
		    playback.visibilityWeight = visible ? 1.0f : 0.0f;
		}
	    }
	}

	if (visible) {
	    if (!blendIn || blendTime <= 0.0f) {
		playback.visibilityWeight = 1.0f;
	    } else {
		playback.visibilityWeight
		    = std::min (1.0f, playback.visibilityWeight + std::max (deltaTime, 0.0f) / blendTime);
	    }
	} else if (!blendOut || blendTime <= 0.0f) {
	    playback.visibilityWeight = 0.0f;
	} else {
	    playback.visibilityWeight
		= std::max (0.0f, playback.visibilityWeight - std::max (deltaTime, 0.0f) / blendTime);
	}

	const float weight = std::clamp (layer->blend->value->getFloat (), 0.0f, 1.0f) * playback.visibilityWeight;
	if (animation != nullptr) {
	    this->m_puppetActiveLayers.push_back (
		{
		    .animation = animation,
		    .time = playback.stopped ? 0.0f : playback.time,
		    .weight = weight,
		    .additive = layer->additive->value->getBool (),
		    .frame = (playback.stopped ? 0.0f : playback.time) * animation->fps,
		    .enabled = visible || playback.visibilityWeight > 0.0f,
		}
	    );
	}
    }
}

void CImage::updatePuppetPositionBuffer (const glm::vec2& size, const bool stepRope) {
    if (this->m_puppetRawPositions.empty ()) {
	return;
    }

    const double sceneTime = this->getScene ().getElapsedTime ();
    this->selectPuppetAnimations (sceneTime);

    // Spring bones react to the layer's scene motion (scripted origin, parallax). Native steps
    // them in y-up scene space, so flip the fork's y-down quad back to keep rotation handedness.
    // ponytail: layer rotation is left out of this matrix; add it when a rotated spring puppet shows up.
    const glm::vec2 parallax = this->getImage ().model->fullscreen ? glm::vec2 (0.0f) : this->resolveParallaxOffset ();
    const glm::vec2 center = glm::vec2 (this->m_pos.x + this->m_pos.z, this->m_pos.y + this->m_pos.w) * 0.5f + parallax;
    MdlPhysicsStep physics {
	.state = this->m_puppetPhysics,
	.model = glm::scale (
	    glm::translate (glm::mat4 (1.0f), glm::vec3 (center.x, -center.y, 0.0f)),
	    glm::vec3 ((this->m_pos.z - this->m_pos.x) / size.x, (this->m_pos.y - this->m_pos.w) / size.y, 1.0f)
	),
	// Re-skinning at an unchanged scene time must not advance the springs again.
	.deltaTime = static_cast<float> (std::max (sceneTime - this->m_puppetPhysicsTime, 0.0)),
    };
    this->m_puppetPhysicsTime = sceneTime;
    const auto& scene = this->getScene ().getScene ();
    const bool hasSpringBones = std::ranges::any_of (
        this->m_puppetAnimation.bones, [] (const auto& bone) { return bone.spring.has_value (); }
    );
    if (stepRope || (hasSpringBones && !this->m_puppetAnimation.ikGroups.empty ())) {
        physics.previewRope = !stepRope;
        if (hasSpringBones) physics.ropeFallbackModel = physics.model;
        // Native predicts authored scene-space points through the aligned layer Model.
        // Camera projection, fullscreen geometry and parallax only affect drawing.
        physics.model = this->resolveLayerModelMatrix ();
	physics.rope = MdlRopeStep {
	    .gravity = scene.physics.gravityDirection->evaluateVec3 (sceneTime)
		* scene.physics.gravityStrength->evaluateFloat (sceneTime),
	    // Native seeds and predicts immediately, including layers created late in a scene.
	    .deltaTime = this->getScene ().getDeltaTime (),
	};
	if (scene.physics.windEnabled->value->getBool ()) {
	    physics.rope->windDirection = scene.physics.windDirection->evaluateVec3 (sceneTime);
	    physics.rope->windStrength = scene.physics.windStrength->evaluateFloat (sceneTime);
	    physics.rope->windTime = this->getScene ().getTime ();
	    physics.rope->windWidth = static_cast<float> (this->getScene ().getWidth ());
	    // Admitted rope models already use native authored world X.
	    physics.rope->windOriginX = 0.0f;
	}
    }


    // Vertex positions are stored in the assembled rest pose, so each bone applies
    // animatedWorld * inverseBindWorld. The same evaluator now drives 3D models too.
    // Setup previews preserve dependency history. Admitted mixed graphs leave all
    // physics history untouched; standalone springs keep their existing setup path.
    const bool previewDependencies = !stepRope && this->m_puppetAnimation.dependencyCount != 0;
    std::vector<glm::mat4> setupWorkingWorldBones;
    std::vector<float> setupDependencyWeights;
    if (previewDependencies) {
        setupWorkingWorldBones.swap (physics.state.workingWorldBones);
        setupDependencyWeights.swap (physics.state.dependencyWeights);
    }
    auto pose = MdlAnimationEvaluator::evaluate (this->m_puppetAnimation, this->m_puppetActiveLayers, &physics);
    if (previewDependencies) {
        physics.state.workingWorldBones.swap (setupWorkingWorldBones);
        physics.state.dependencyWeights.swap (setupDependencyWeights);
    }
    this->m_puppetWorldBones = std::move (pose.worldBones);
    this->m_puppetPublishedSceneBones = std::move (pose.publishedWorldBones);
    // The public native buffer is sampled during evaluation, including the layer Model.
    // A later script property write must not retroactively transform that cached pose.
    for (auto& bone : this->m_puppetPublishedSceneBones) bone = physics.model * bone;
    this->m_puppetSkinBones = std::move (pose.skinBones);

    const auto& mesh = *this->getImage ().model->puppetMesh;
    if (updatePuppetDrawOrder (mesh, this->m_puppetAnimation.defaultDepths, pose.depths, this->m_puppetRangeOrder)) {
	const auto plan = buildPuppetClippingPlan (mesh, 0, &this->m_puppetRangeOrder);
	this->m_puppetDrawRanges.clear ();
	for (const auto& draw : plan.draws) {
	    this->m_puppetDrawRanges.push_back (MdlDrawRange { .firstIndex = draw.firstIndex, .indexCount = draw.indexCount });
	}
	if (this->m_hasPuppetClipping) this->updatePuppetClippingCommands (plan);
    }

    // ponytail: only g_BlendMap row zero, which covers BLENDROWCOUNT 1 (every channelmap
    // material in the corpus). More rows would need a vec4 array uniform instead.
    this->m_puppetBlendMap = glm::vec4 (0.0f);
    for (size_t track = 0; track < pose.blendWeights.size () && track < 4; track++) {
	this->m_puppetBlendMap[static_cast<glm::length_t> (track)] = pose.blendWeights[track];
    }

    const auto& body = mesh.submeshes.front ();
    const auto morphTargets = preparePuppetMorphTargets (
	body, pose.puppetMorphWeights, pose.puppetMorphMask, this->m_puppetWorldBones
    );

    const bool ropeAdmitted = !this->m_puppetPublishedSceneBones.empty ();
    const bool mapRopeToScene = ropeAdmitted && !this->getImage ().model->fullscreen;
    glm::mat4 puppetToDraw (1.0f);
    if (mapRopeToScene) {
        glm::mat4 sceneBasis (1.0f);
        if (!scene.camera.projection.isPerspective) {
            sceneBasis = glm::scale (glm::translate (sceneBasis, glm::vec3 (
                -this->getScene ().getWidth () * 0.5f, this->getScene ().getHeight () * 0.5f, 0.0f
            )), glm::vec3 (1.0f, -1.0f, 1.0f));
            sceneBasis = glm::translate (sceneBasis, glm::vec3 (parallax.x, -parallax.y, 0.0f));
        }
        // The ordinary quad bakes a flattened parent transform into its vertices.
        // Compensate in its existing draw basis to retain the full affine puppet pose.
        puppetToDraw = glm::inverse (this->m_sceneModelMatrix) * sceneBasis * physics.model;
    }

    const bool morphColor = !body.morphTargets.empty () && (body.flags & 0x1000) != 0
	&& (body.vertexTag & 0x10001) == 0x10000;
    this->m_puppetColors.clear ();
    if (morphColor) this->m_puppetColors.reserve (this->m_puppetRawPositions.size () / 3);
    this->m_puppetPositions.clear ();
    this->m_puppetPositions.reserve (this->m_puppetRawPositions.size ());
    for (size_t index = 0; index + 2 < this->m_puppetRawPositions.size (); index += 3) {
	glm::vec3 position (
	    this->m_puppetRawPositions[index], this->m_puppetRawPositions[index + 1],
	    this->m_puppetRawPositions[index + 2]
	);

	float vertexAlpha = 1.0f;
	float maskAlpha = 1.0f;
	if (!morphTargets.empty ()) {
	    glm::vec3 unmorphedSkinned (0.0f);
	    const size_t vertex = index / 3;
	    if ((body.flags & 0x2000) != 0) {
		// Modifiers use raw weighted skinning, before any target changes position.
		for (size_t influence = 0; influence < 4; ++influence) {
		    const float weight = this->m_puppetBlendWeights[vertex * 4 + influence];
		    const uint32_t bone = this->m_puppetBlendIndices[vertex * 4 + influence];
		    if (bone < this->m_puppetSkinBones.size ())
			unmorphedSkinned += weight * glm::vec3 (this->m_puppetSkinBones[bone] * glm::vec4 (position, 1));
		}
	    }
	    const float morphIndex = body.vertices[vertex * body.strideBytes / sizeof (float) + 3];
	    const auto morph = evaluatePuppetMorph (body, morphTargets, morphIndex, unmorphedSkinned, &maskAlpha);
	    position += glm::vec3 (morph);
	    vertexAlpha = morph.w;
	}

	if (morphColor) this->m_puppetColors.push_back ({ glm::vec4 (1, 1, 1, vertexAlpha), maskAlpha });
	if (!this->m_puppetSkinBones.empty ()) {
	    const size_t vertex = index / 3;
	    const glm::vec4 rest (position, 1.0f);
	    glm::vec4 posed (0.0f);
	    float totalWeight = 0.0f;

	    for (size_t influence = 0; influence < 4; influence++) {
		const float weight = this->m_puppetBlendWeights[vertex * 4 + influence];
		const uint32_t bone = this->m_puppetBlendIndices[vertex * 4 + influence];

		if (weight <= 0.0f || bone >= this->m_puppetSkinBones.size ()) {
		    continue;
		}

		posed += weight * (this->m_puppetSkinBones[bone] * rest);
		totalWeight += weight;
	    }

	    if (totalWeight > 0.0f) {
		position = glm::vec3 (posed) / totalWeight;
	    }
	}

	// map the posed model-space position into the current draw basis. Stock image
	// skinning keeps XYZ; tilted rope forces can create model-space Z displacement.
        if (mapRopeToScene) {
            position = glm::vec3 (puppetToDraw * glm::vec4 (position, 1.0f));
        } else {
            const float u = 0.5f + position.x / size.x;
            const float v = 0.5f - position.y / size.y;
            position = glm::vec3 (
                this->m_pos.x + u * (this->m_pos.z - this->m_pos.x),
                this->m_pos.w + v * (this->m_pos.y - this->m_pos.w), ropeAdmitted ? position.z : 0.0f
            );
        }
        this->m_puppetPositions.push_back (position.x);
        this->m_puppetPositions.push_back (position.y);
        this->m_puppetPositions.push_back (position.z);
    }

    if (morphColor) {
	if (this->m_puppetVertexColor == GL_NONE) glGenBuffers (1, &this->m_puppetVertexColor);
	glBindBuffer (GL_ARRAY_BUFFER, this->m_puppetVertexColor);
	const size_t bytes = this->m_puppetColors.size () * sizeof (PuppetVertexColor);
	if (bytes != this->m_puppetColorBufferBytes) {
	    glBufferData (GL_ARRAY_BUFFER, bytes, this->m_puppetColors.data (), GL_DYNAMIC_DRAW);
	    this->m_puppetColorBufferBytes = bytes;
	} else if (bytes > 0) {
	    glBufferSubData (GL_ARRAY_BUFFER, 0, bytes, this->m_puppetColors.data ());
	}
    }

    if (this->m_puppetSpacePosition == GL_NONE) {
	glGenBuffers (1, &this->m_puppetSpacePosition);
    }
    glBindBuffer (GL_ARRAY_BUFFER, this->m_puppetSpacePosition);
    const size_t positionBytes = this->m_puppetPositions.size () * sizeof (GLfloat);
    if (positionBytes != this->m_puppetPositionBufferBytes) {
	glBufferData (GL_ARRAY_BUFFER, positionBytes, this->m_puppetPositions.data (), GL_DYNAMIC_DRAW);
	this->m_puppetPositionBufferBytes = positionBytes;
    } else if (positionBytes > 0) {
	glBufferSubData (GL_ARRAY_BUFFER, 0, positionBytes, this->m_puppetPositions.data ());
    }
}

void CImage::setupPuppetAlbedoPasses () {
    if (this->m_puppetOverlayIndexCount == 0 || this->m_puppetOverlayMaterial.empty ()) {
	return;
    }

    if (this->m_material.passes.empty ()) {
	sLog.error ("Puppet channelmap has no base texture for object ", this->getId ());
	return;
    }
    const auto baseTexture = this->m_material.passes.front ()->textures.find (0);
    if (baseTexture == this->m_material.passes.front ()->textures.end ()) {
	sLog.error ("Puppet channelmap has no base texture for object ", this->getId ());
	return;
    }

    try {
	// Wallpaper Engine creates this exact target, copies the layer's base image
	// into it with fullscreenlayer, and only then draws puppettexturechannels.
	const std::string albedoName = "_rt_imageLayerAlbedo_" + std::to_string (this->getId ());
	auto copyMaterial = MaterialParser::load (
	    this->getScene ().getScene ().project, "materials/util/fullscreenlayer.json"
	);
	auto overlayMaterial
	    = MaterialParser::load (this->getScene ().getScene ().project, this->m_puppetOverlayMaterial);
	if (copyMaterial->passes.empty () || overlayMaterial->passes.empty ()) {
	    throw std::runtime_error ("puppet albedo material has no passes");
	}

	const auto* stableCopyMaterial
	    = this->m_materials.compatibilityMaterials.emplace_back (std::move (copyMaterial)).get ();
	const auto* stableOverlayMaterial
	    = this->m_materials.compatibilityMaterials.emplace_back (std::move (overlayMaterial)).get ();

	this->m_puppetAlbedoFBO = this->create (
	    albedoName, this->getScene ().getColorFormat (), this->getTexture ()->getFlags (), 1.0f, this->m_size, this->m_size
	);

	auto copyOverride = std::make_unique<ImageEffectPassOverride> (ImageEffectPassOverride {
	    .id = -1,
	    .combos = this->getTexture ()->isAnimated () ? ComboMap { { "SPRITESHEET", 1 } } : ComboMap {},
	    .constants = {},
	    .textures = { { 0, baseTexture->second } },
	});
	const auto& stableCopyOverride = *this->m_materials.compatibilityOverrides.emplace_back (
	    std::move (copyOverride)
	);

	auto inputOverride = std::make_unique<ImageEffectPassOverride> (ImageEffectPassOverride {
	    .id = -1,
	    .combos = {},
	    .constants = {},
	    .textures = { { 0, albedoName } },
	});
	this->m_puppetAlbedoInputOverride
	    = this->m_materials.compatibilityOverrides.emplace_back (std::move (inputOverride)).get ();

	this->m_puppetAlbedoCopyPass = new CPass (
	    *this, std::make_shared<FBOProvider> (this), **stableCopyMaterial->passes.begin (), stableCopyOverride,
	    std::nullopt, std::nullopt
	);
	this->m_puppetAlbedoCopyPass->setBlendingMode (BlendingMode_Normal);
	this->m_puppetAlbedoCopyPass->setDestination (this->m_puppetAlbedoFBO);
	this->m_puppetAlbedoCopyPass->setInput (this->getTexture ());
	this->m_puppetAlbedoCopyPass->setPosition (this->getPassSpacePosition ());
	this->m_puppetAlbedoCopyPass->setTexCoord (this->getTexCoordCopy ());
	this->m_puppetAlbedoCopyPass->setModelViewProjectionMatrix (&this->m_modelViewProjectionPass);
	this->m_puppetAlbedoCopyPass->setModelViewProjectionMatrixInverse (&this->m_modelViewProjectionPassInverse);
	this->m_puppetAlbedoCopyPass->setModelMatrix (&this->m_modelMatrix);
	this->m_puppetAlbedoCopyPass->setViewProjectionMatrix (&this->m_viewProjectionMatrix);

	this->m_puppetOverlayPass = new CPass (
	    *this, std::make_shared<FBOProvider> (this), **stableOverlayMaterial->passes.begin (), std::nullopt,
	    std::nullopt, std::nullopt
	);

	// The channel material supplies the channel atlas in texture 0 and the
	// original base image in texture 1. Its DOUBLEBUFFERED path mixes those
	// pixels using g_BlendMap, directly into the local albedo target.
	this->m_puppetOverlayPass->setDestination (this->m_puppetAlbedoFBO);
	this->m_puppetOverlayPass->setInput (this->getTexture ());
	this->m_puppetOverlayPass->setModelViewProjectionMatrix (&this->m_modelViewProjectionCopy, usesReverseProjection (&m_modelViewProjectionCopy));
	this->m_puppetOverlayPass->setModelViewProjectionMatrixInverse (&this->m_modelViewProjectionCopyInverse);
	this->m_puppetOverlayPass->setModelMatrix (&this->m_modelMatrix);
	this->m_puppetOverlayPass->setViewProjectionMatrix (&this->m_viewProjectionMatrix);
	this->m_puppetOverlayPass->addUniform ("g_BlendMap", &this->m_puppetBlendMap, 1);

	CPass* pass = this->m_puppetOverlayPass;
	pass->setGeometryCallback (
	    [this, pass] () {
		const GLint position = glGetAttribLocation (pass->getProgramID (), "a_Position");
		const GLint texCoord = glGetAttribLocation (pass->getProgramID (), "a_TexCoordVec4");
		const GLint blendIndices = glGetAttribLocation (pass->getProgramID (), "a_BlendIndices");

		if (position >= 0) {
		    glEnableVertexAttribArray (position);
		    glBindBuffer (GL_ARRAY_BUFFER, this->m_puppetOverlayPosition);
		    glVertexAttribPointer (position, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
		}

		if (texCoord >= 0) {
		    glEnableVertexAttribArray (texCoord);
		    glBindBuffer (GL_ARRAY_BUFFER, this->m_puppetOverlayTexCoord);
		    glVertexAttribPointer (texCoord, 4, GL_FLOAT, GL_FALSE, 0, nullptr);
		}

		if (blendIndices >= 0) {
		    glEnableVertexAttribArray (blendIndices);
		    glBindBuffer (GL_ARRAY_BUFFER, this->m_puppetOverlayBlendIndices);
		    // an integer attribute must not go through the float path or it reads as 0.0
		    glVertexAttribIPointer (blendIndices, 4, GL_UNSIGNED_INT, 0, nullptr);
		}
	    },
	    [this] () {
		const GLboolean cullFaceEnabled = glIsEnabled (GL_CULL_FACE);
		glDisable (GL_CULL_FACE);
		glBindBuffer (GL_ELEMENT_ARRAY_BUFFER, this->m_puppetOverlayIndices);
		glDrawElements (GL_TRIANGLES, this->m_puppetOverlayIndexCount, GL_UNSIGNED_SHORT, nullptr);
		if (cullFaceEnabled) {
		    glEnable (GL_CULL_FACE);
		}
	    },
	    [pass] () {
		for (const auto* name : { "a_Position", "a_TexCoordVec4", "a_BlendIndices" }) {
		    const GLint attribute = glGetAttribLocation (pass->getProgramID (), name);
		    if (attribute >= 0) {
			glDisableVertexAttribArray (attribute);
		    }
		}
	    }
	);

	sLog.out (
	    "Loaded puppet albedo compositor ", this->m_puppetOverlayMaterial,
	    " target=", this->m_puppetAlbedoFBO->getName ()
	);
    } catch (const std::exception& ex) {
	sLog.error ("Could not set up puppet albedo composition ", this->m_puppetOverlayMaterial, ": ", ex.what ());
	delete this->m_puppetAlbedoCopyPass;
	delete this->m_puppetOverlayPass;
	this->m_puppetAlbedoCopyPass = nullptr;
	this->m_puppetOverlayPass = nullptr;
	this->m_puppetAlbedoFBO = nullptr;
	this->m_puppetAlbedoInputOverride = nullptr;
    }
}

void CImage::renderPuppetAlbedo () {
    if (this->m_puppetAlbedoCopyPass == nullptr || this->m_puppetOverlayPass == nullptr) {
	return;
    }

    glColorMask (true, true, true, true);
    this->m_puppetAlbedoCopyPass->render ();
    this->m_puppetOverlayPass->render ();
}

std::optional<CImage::ResolvedTransform> CImage::puppetAttachmentTransform (const std::string& name) const {
    const auto model = this->getAttachmentTransform (name);
    if (!model.has_value ()) {
	return std::nullopt;
    }

    // Attachment matrices and scene object origins use the same authoring-space axes.
    // The scene render path performs its Y flip later, when it builds screen geometry;
    // flipping here as well would send every attached child in the opposite direction.
    glm::vec3 scale {
	glm::length (glm::vec3 ((*model)[0])),
	glm::length (glm::vec3 ((*model)[1])),
	glm::length (glm::vec3 ((*model)[2])),
    };
    glm::mat3 rotation (1.0f);
    for (int column = 0; column < 3; column++) {
	if (scale[column] > 1e-8f) {
	    rotation[column] = glm::vec3 ((*model)[column]) / scale[column];
	}
    }
    if (glm::determinant (rotation) < 0.0f) {
	scale.x = -scale.x;
	rotation[0] = -rotation[0];
    }
    const glm::vec3 angles = glm::eulerAngles (glm::normalize (glm::quat_cast (rotation)));

    return ResolvedTransform {
	.origin = { (*model)[3][0], (*model)[3][1], (*model)[3][2] },
	.scale = scale,
	.angles = angles,
    };
}

void CImage::setupPuppetGeometryCallback (Effects::CPass* pass, bool samplesSourceTexture) const {
    this->setupPuppetRangeGeometryCallback (
	pass, samplesSourceTexture, {}, &this->m_puppetDrawRanges
    );
}

void CImage::setupPuppetRangeGeometryCallback (
    Effects::CPass* pass, const bool samplesSourceTexture, const std::vector<MdlDrawRange>& ranges,
    const std::vector<MdlDrawRange>* dynamicRanges
) const {
    // when the final pass samples the source texture directly (no effects) the UVs need the
    // same padding compensation the copy texcoords get; FBO content always spans the full range
    const GLuint texCoordBuffer = samplesSourceTexture ? this->m_puppetTexCoordFirstPass : this->m_puppetTexCoord;

    pass->setGeometryCallback (
	[this, pass, texCoordBuffer] () {
	    const GLint position = glGetAttribLocation (pass->getProgramID (), "a_Position");
	    const GLint positionC1 = glGetAttribLocation (pass->getProgramID (), "a_PositionC1");
	    const GLint texCoord = glGetAttribLocation (pass->getProgramID (), "a_TexCoord");

	    if (position >= 0) {
		glEnableVertexAttribArray (position);
		glBindBuffer (GL_ARRAY_BUFFER, this->m_puppetSpacePosition);
		glVertexAttribPointer (position, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
	    }
	    if (positionC1 >= 0 && this->m_puppetPrelightingPosition != GL_NONE) {
		glEnableVertexAttribArray (positionC1);
		glBindBuffer (GL_ARRAY_BUFFER, this->m_puppetPrelightingPosition);
		glVertexAttribPointer (positionC1, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
	    }

	    const auto& combos = pass->getShader ()->getCombos ();
	    const auto prelighting = combos.find ("PRELIGHTING");
	    const auto& overrides = pass->getOverride ().combos;
	    const auto prelightingOverride = overrides.find ("PRELIGHTING");
	    const bool isPrelighting = prelightingOverride != overrides.end () ? prelightingOverride->second != 0
		: prelighting != combos.end () && prelighting->second != 0;
	    const auto& shader = pass->getPass ().shader;
	    const GLint color = glGetAttribLocation (pass->getProgramID (), "a_Color");
	    if (this->hasPuppetMorphAlpha () && color >= 0 && !isPrelighting
		&& (shader == "genericimage3" || shader == "genericimage4" || shader == "clippingmaskimage4")) {
		glEnableVertexAttribArray (color);
		glBindBuffer (GL_ARRAY_BUFFER, this->m_puppetVertexColor);
		const bool mask = shader == "clippingmaskimage4";
		glVertexAttribPointer (color, mask ? 1 : 4, GL_FLOAT, GL_FALSE, sizeof (PuppetVertexColor),
		    mask ? reinterpret_cast<const void*> (offsetof (PuppetVertexColor, maskAlpha)) : nullptr);
	    }

	    if (texCoord >= 0) {
		glEnableVertexAttribArray (texCoord);
		glBindBuffer (GL_ARRAY_BUFFER, texCoordBuffer);
		glVertexAttribPointer (texCoord, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
	    }
	},
	[this, ranges, dynamicRanges] () {
	    // the Y-flip into scene space inverts triangle winding, so materials with
	    // cullmode "normal" would cull the whole mesh (e.g. the eye layer in 3558034522)
	    const GLboolean cullFaceEnabled = glIsEnabled (GL_CULL_FACE);
	    glDisable (GL_CULL_FACE);
	    glBindBuffer (GL_ELEMENT_ARRAY_BUFFER, this->m_puppetIndices);
	    for (const auto& range : dynamicRanges ? *dynamicRanges : ranges) {
		if (range.submeshIndex != 0 || range.indexCount == 0) {
		    continue;
		}
		const auto byteOffset = static_cast<uintptr_t> (range.firstIndex) * sizeof (GLushort);
		glDrawElements (
		    GL_TRIANGLES, static_cast<GLsizei> (range.indexCount), GL_UNSIGNED_SHORT,
		    reinterpret_cast<const void*> (byteOffset)
		);
	    }
	    if (cullFaceEnabled) {
		glEnable (GL_CULL_FACE);
	    }
	},
	[pass] () {
	    const GLint color = glGetAttribLocation (pass->getProgramID (), "a_Color");
	    if (color >= 0) glDisableVertexAttribArray (color);
	    const GLint position = glGetAttribLocation (pass->getProgramID (), "a_Position");
	    const GLint positionC1 = glGetAttribLocation (pass->getProgramID (), "a_PositionC1");
	    const GLint texCoord = glGetAttribLocation (pass->getProgramID (), "a_TexCoord");

	    if (position >= 0) {
		glDisableVertexAttribArray (position);
	    }
	    if (positionC1 >= 0) glDisableVertexAttribArray (positionC1);

	    if (texCoord >= 0) {
		glDisableVertexAttribArray (texCoord);
	    }
	}
    );
}

bool CImage::setupPuppetClippingPasses (
    Effects::CPass& finalPass, const std::shared_ptr<const TextureProvider>& input,
    const std::shared_ptr<const CFBO>& destination, const glm::mat4* projection,
    const glm::mat4* inverseProjection, const bool samplesSourceTexture
) {
    const auto& model = *this->getImage ().model;
    if (!model.puppetMesh.has_value () || model.puppetMesh->clippingDescriptors.empty ()) {
	return false;
    }

    try {
	const auto& mesh = *model.puppetMesh;
	const auto plan = buildPuppetClippingPlan (mesh, 0, &this->m_puppetRangeOrder);
	if (plan.unsupportedComposition) {
	    sLog.error (
		"Puppet clipping for ", *model.puppet,
		" has unsupported clipping composition ordering; rendering the un-clipped body for now"
	    );
	    return false;
	}
	for (const auto& mask : plan.masks) {
	    const auto& descriptor = mesh.clippingDescriptors[mask.descriptorIndex];
	    if ((descriptor.flags & ~1u) != 0) {
		sLog.error (
		    "Puppet clipping for ", *model.puppet, " uses unsupported descriptor flags 0x", std::hex,
		    descriptor.flags, std::dec, "; rendering the un-clipped body for now"
		);
		return false;
	    }
	    for (const uint32_t rangeIndex : mask.sourceRanges) {
		if (mesh.drawRanges[rangeIndex].submeshIndex != 0) {
		    sLog.error (
			"Puppet clipping source references unsupported submesh ",
			mesh.drawRanges[rangeIndex].submeshIndex, " in ", *model.puppet
		    );
		    return false;
		}
	    }
	}
	for (const auto& draw : plan.draws) {
	    if (draw.submeshIndex != 0) {
		sLog.error ("Puppet clipping draw references unsupported submesh ", draw.submeshIndex, " in ", *model.puppet);
		return false;
	    }
	}

	// CLIPPINGTARGET and CLIPPINGUVS are implemented by genericimage4. Puppet
	// effect chains deliberately end in effectpassthrough_4 when descriptors exist.
	if (finalPass.getPass ().shader != "genericimage4") {
	    sLog.error (
		"Puppet clipping final shader ", finalPass.getPass ().shader,
		" has no genericimage4 clipping path in ", *model.puppet
	    );
	    return false;
	}

	const auto sceneSize = this->getScene ().getOutputSize ();
	const auto maskSize = plan.requiresComposition ? glm::max (glm::uvec2 (sceneSize) / 2u, glm::uvec2 (2))
	    : glm::uvec2 (sceneSize);
	const std::string maskName = "_rt_puppetClipping_" + std::to_string (this->getId ());
	this->m_puppetClippingFBO = this->create (
	    maskName, plan.requiresComposition ? TextureFormat_R8 : TextureFormat_ARGB8888,
	    TextureFlags_ClampUVs, 1.0f, maskSize, maskSize
	);
	if (plan.requiresComposition) {
	    this->m_puppetClippingIntermediateFBO = this->create (
		maskName + "_intermediate", TextureFormat_R8, TextureFlags_ClampUVs, 1.0f, maskSize, maskSize
	    );
	}

	this->m_materials.compatibilityMaterials.emplace_back (
	    MaterialParser::load (this->getScene ().getScene ().project, "materials/util/clippingmaskimage4.json")
	);
	const auto& maskMaterial = *this->m_materials.compatibilityMaterials.back ();
	const auto provider = std::make_shared<FBOProvider> (this);
	this->m_puppetMaskPasses.reserve (plan.masks.size ());
	this->m_puppetClippingSourceRanges.resize (plan.masks.size ());

	const auto configurePass = [this, &input, &destination, projection, inverseProjection] (Effects::CPass& pass) {
	    pass.setDestination (destination);
	    pass.setInput (input);
	    pass.setModelViewProjectionMatrix (projection, usesReverseProjection (projection));
	    pass.setModelViewProjectionMatrixInverse (inverseProjection);
	    pass.setModelMatrix (&this->m_modelMatrix);
	    pass.setViewProjectionMatrix (&this->m_viewProjectionMatrix);
	    pass.setEffectTextureProjectionMatrix (
		&this->m_effectTextureProjectionMatrix, &this->m_effectTextureProjectionMatrixInverse
	    );
	    pass.setModelMatrix (&this->m_sceneModelMatrix);
	    pass.setViewProjectionMatrix (&this->m_sceneViewProjectionMatrix);
	    pass.addUniform ("g_NormalModelMatrix", &this->m_sceneNormalModelMatrix);
	};

	for (const auto& mask : plan.masks) {
	    const auto& descriptor = mesh.clippingDescriptors[mask.descriptorIndex];
	    auto maskOverride = std::make_unique<ImageEffectPassOverride> (ImageEffectPassOverride {
		.id = -1,
		.combos = {},
		.constants = {},
		.textures = { { 1, descriptor.maskAsset } },
	    });
	    const auto& stableOverride = *this->m_materials.compatibilityOverrides.emplace_back (
		std::move (maskOverride)
	    );

	    auto* pass = new CPass (
		*this, provider, **maskMaterial.passes.begin (), stableOverride, std::nullopt, std::nullopt
	    );
	    this->m_puppetClippingPasses.push_back (pass);
	    this->m_puppetMaskPasses.push_back (pass);
	    pass->setDestination (this->m_puppetClippingFBO);
	    pass->setInput (input);
	    pass->setModelViewProjectionMatrix (projection, usesReverseProjection (projection));
	    pass->setModelViewProjectionMatrixInverse (inverseProjection);
	    pass->setModelMatrix (&this->m_modelMatrix);
	    pass->setViewProjectionMatrix (&this->m_viewProjectionMatrix);
	    pass->setEffectTextureProjectionMatrix (
		&this->m_effectTextureProjectionMatrix, &this->m_effectTextureProjectionMatrixInverse
	    );
	    pass->setBlendEquation (GL_MAX, GL_FUNC_ADD);
	    this->m_puppetClippingRenderVars.emplace_back (std::make_unique<glm::vec4> (0.0f));
	    pass->addUniform ("g_RenderVar0", this->m_puppetClippingRenderVars.back ().get ());

	    this->setupPuppetRangeGeometryCallback (
		pass, samplesSourceTexture, {}, &this->m_puppetClippingSourceRanges[mask.descriptorIndex]
	    );
	}

	if (plan.requiresComposition) {
	    this->m_materials.compatibilityMaterials.emplace_back (
		MaterialParser::load (this->getScene ().getScene ().project, "materials/util/flattexture.json")
	    );
	    auto* compose = new CPass (
		*this, provider, **this->m_materials.compatibilityMaterials.back ()->passes.begin (),
		std::nullopt, std::nullopt, std::nullopt
	    );
	    this->m_puppetClippingPasses.push_back (compose);
	    this->m_puppetMaskComposePass = compose;
	    compose->setDestination (this->m_puppetClippingFBO);
	    compose->setInput (this->m_puppetClippingIntermediateFBO);
	    compose->setPosition (this->getPassSpacePosition ());
	    compose->setTexCoord (this->getTexCoordPass ());
	    static const glm::mat4 identity (1.0f);
	    static const float alpha = 1.0f;
	    compose->setModelViewProjectionMatrix (&identity);
	    compose->setModelViewProjectionMatrixInverse (&identity);
	    compose->setModelMatrix (&identity);
	    compose->setViewProjectionMatrix (&identity);
	    compose->addUniform ("g_Alpha", &alpha);
	    compose->setBlendEquation (GL_FUNC_ADD, GL_FUNC_ADD);
	    compose->setGeometryCallback ({}, [] () {
		// R8 sampling gives alpha=1, so this is the native red-mask product.
		glBlendFuncSeparate (GL_DST_COLOR, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE);
		glDrawArrays (GL_TRIANGLES, 0, 6);
	    }, {});
	}

	for (const bool clipped : { false, true }) {
	    std::optional<std::reference_wrapper<const TextureMap>> binds = finalPass.getBinds ();
	    ComboMap runtimeCombos;
	    if (clipped) {
		auto targetBinds = std::make_unique<TextureMap> (finalPass.getBinds ());
		targetBinds->insert_or_assign (8, maskName);
		binds = *this->m_puppetClippingBinds.emplace_back (std::move (targetBinds));
		runtimeCombos = {
		    { "CLIPPINGUVS", 1 },
		    { "CLIPPINGTARGET", 1 },
		};
	    }

	    auto* pass = new CPass (
		*this, provider, finalPass.getPass (), finalPass.getOverride (), binds, std::nullopt,
		std::move (runtimeCombos)
	    );
	    this->m_puppetClippingPasses.push_back (pass);
	    configurePass (*pass);
	    if (&finalPass == m_fogPass) pass->addUniform ("g_Color4", &neutralCompositeColor);
	    pass->setBlendingMode (finalPass.getBlendingMode ());
	    pass->setDepthtestMode (finalPass.getDepthtestMode ());
	    pass->setDepthwriteMode (finalPass.getDepthwriteMode ());
	    this->setupPuppetRangeGeometryCallback (
		pass, samplesSourceTexture, {}, &this->m_puppetClippingDrawRange
	    );
	    if (clipped) this->m_puppetClippedPass = pass;
	    else this->m_puppetPlainPass = pass;
	}
	this->updatePuppetClippingCommands (plan);

	sLog.out (
	    "Loaded puppet clipping ", *model.puppet, " masks=", plan.masks.size (), " draws=", plan.draws.size ()
	);
	return !this->m_puppetClippingCommands.empty ();
    } catch (const std::exception& ex) {
	sLog.error ("Could not set up puppet clipping for ", *model.puppet, ": ", ex.what ());
	this->m_puppetClippingCommands.clear ();
	return false;
    }
}

void CImage::updatePuppetClippingCommands (const PuppetClippingPlan& plan) {
    this->m_puppetClippingCommands.clear ();
    const auto& mesh = *this->getImage ().model->puppetMesh;
    if (plan.unsupportedComposition) {
	for (const auto& draw : plan.draws) this->m_puppetClippingCommands.push_back (PuppetClippingRenderCommand {
	    .pass = this->m_puppetPlainPass,
	    .range = { .firstIndex = draw.firstIndex, .indexCount = draw.indexCount },
	});
	return;
    }
    for (size_t index = 0; index < plan.masks.size (); index++) {
	auto& sources = this->m_puppetClippingSourceRanges[index];
	sources.clear ();
	for (const uint32_t source : plan.masks[index].sourceRanges) sources.push_back (mesh.drawRanges[source]);
    }
    size_t activeMask = std::numeric_limits<size_t>::max ();
    for (const auto& draw : plan.draws) {
	const bool clipped = !draw.masks.empty ();
	if (clipped && activeMask != draw.masks.front ()) {
	    activeMask = draw.masks.front ();
	    const auto& ancestors = plan.masks[activeMask].ancestors;
	    for (size_t factor = 0; factor <= ancestors.size (); factor++) {
		const size_t mask = factor < ancestors.size () ? ancestors[factor] : activeMask;
		this->m_puppetClippingCommands.push_back (PuppetClippingRenderCommand {
		    .buildsMask = true, .intermediateMask = factor != 0, .pass = this->m_puppetMaskPasses[mask],
		});
		if (factor != 0) this->m_puppetClippingCommands.push_back (PuppetClippingRenderCommand {
		    .pass = this->m_puppetMaskComposePass,
		});
	    }
	}
	this->m_puppetClippingCommands.push_back (PuppetClippingRenderCommand {
	    .pass = clipped ? this->m_puppetClippedPass : this->m_puppetPlainPass,
	    .range = { .firstIndex = draw.firstIndex, .indexCount = draw.indexCount },
	    .blending = clipped ? ((mesh.clippingDescriptors[plan.masks[draw.masks.front ()].descriptorIndex].flags & 1)
		? BlendingMode_Additive : BlendingMode_Translucent) : BlendingMode_Unknown,
	});
    }
}

void CImage::renderPuppetClipping () {
    GLfloat clearColor[4];
    glGetFloatv (GL_COLOR_CLEAR_VALUE, clearColor);

    for (const auto& command : this->m_puppetClippingCommands) {
	if (command.buildsMask) {
	    const GLboolean scissorEnabled = glIsEnabled (GL_SCISSOR_TEST);
	    glDisable (GL_SCISSOR_TEST);
	    glColorMask (true, true, true, true);
	    const auto& target = command.intermediateMask ? this->m_puppetClippingIntermediateFBO
		: this->m_puppetClippingFBO;
	    command.pass->setDestination (target);
	    glBindFramebuffer (GL_FRAMEBUFFER, target->getFramebuffer ());
	    glViewport (0, 0, target->getRealWidth (), target->getRealHeight ());
	    glClearColor (0.0f, 0.0f, 0.0f, 0.0f);
	    glClear (GL_COLOR_BUFFER_BIT);
	    if (scissorEnabled) {
		glEnable (GL_SCISSOR_TEST);
	    }
	} else if (command.pass == this->m_puppetMaskComposePass) {
	    glColorMask (true, true, true, true);
	} else {
	    this->m_puppetClippingDrawRange.front () = command.range;
	    if (command.blending != BlendingMode_Unknown) command.pass->setBlendingMode (command.blending);
	    // Scene alpha is owned by the wallpaper compositor, but an isolated
	    // composition surface needs child alpha so its parent can blend the group.
	    glColorMask (
		true, true, true, this->getScene ().isRenderingToComposition () ? GL_TRUE : GL_FALSE
	    );
	}
	command.pass->render ();
    }

    glClearColor (clearColor[0], clearColor[1], clearColor[2], clearColor[3]);
}

void CImage::setup () {
    // do not double-init stuff, that's bad!
    if (this->m_initialized) {
	return;
    }

    // TODO: SUPPORT PASSTHROUGH (IT'S A SHADER)
    if (this->m_image.model->passthrough && !this->isCompositionLayer ()) {
	// passthrough images without effects are bad, do not draw them
	if (this->m_image.effects.empty ()) {
	    return;
	}

	// Some have attempted to declare effects with visible set to false.
	bool allEffectsInvisible = true;
	for (const auto& cur : this->m_image.effects) {
	    if (cur->visible->value->getBool () || cur->visible->value->getScriptSource ().has_value ()
		|| cur->visible->property != nullptr || !cur->effect->functions.empty ()) {
		allEffectsInvisible = false;
		break;
	    }
	}

	if (allEffectsInvisible) {
	    return;
	}
    }

    const auto& debug = this->getScene ().getContext ().getApp ().getContext ().settings.render.debug;

    const auto& scene = this->getScene ().getScene ();
    const auto fogCanEnable = [] (const SceneData::Fog::Layer& layer) {
	return layer.enabled->value->getBool () || layer.enabled->value->getScriptSource ().has_value ()
	    || layer.enabled->property != nullptr;
    };
    const bool fogPresentation = scene.version > 2 && scene.camera.projection.isPerspective && shouldRenderFinalPass (true)
	&& (fogCanEnable (scene.fog.distance) || fogCanEnable (scene.fog.height));

    // Build the binary-compatible channelmap prepass before the base material:
    // the base pass receives _rt_imageLayerAlbedo_<id> instead of the source
    // texture when a channelmap submesh is present.
    this->setupPuppetAlbedoPasses ();
    const std::optional<std::reference_wrapper<const ImageEffectPassOverride>> baseOverride
	= this->m_puppetAlbedoInputOverride != nullptr
	? std::optional<std::reference_wrapper<const ImageEffectPassOverride>> (*this->m_puppetAlbedoInputOverride)
	: std::nullopt;

    // copy pass to the composite layer
    for (const auto& cur : this->getImage ().model->material->passes) {
	const auto comboEnabled = [&cur] (const char* name) {
	    const auto combo = cur->combos.find (name);
	    return combo != cur->combos.end () && combo->second != 0;
	};
	const bool prelighting = this->m_passes.empty ()
	    && (cur->shader == "genericimage4" || cur->shader == "genericimage3" || cur->shader == "genericimage2")
	    && (comboEnabled ("LIGHTING") || comboEnabled ("REFLECTION"))
	    && (!this->m_hasPuppetMesh || !this->getScene ().getScene ().camera.projection.isPerspective)
	    && !this->m_image.model->passthrough && !debug.baseOnly
	    && (!this->m_image.effects.empty () || this->m_image.colorBlendMode->value->getInt () > 0
		|| this->m_image.model->material->passes.size () > 1);
	ComboMap baseCombos;
	if (prelighting) baseCombos.emplace ("PRELIGHTING", 1);
	// Childless composition layers still publish the cropped background RGB
	// for clipping consumers. With background copying disabled, the stock
	// clear-alpha shader keeps that RGB while effects start with zero coverage.
	if (this->isCompositionLayer () && this->m_compositionFBO == nullptr
	    && !this->m_image.copyBackground) {
	    baseCombos.emplace ("CLEARALPHA", 1);
	}
	auto* pass = new CPass (
	    *this, std::make_shared<FBOProvider> (this), *cur, baseOverride, std::nullopt, std::nullopt,
	    baseCombos
	);
	this->m_passes.push_back (pass);
	if (prelighting) {
	    this->m_prelightingPass = pass;
	}
    }

    // Native applies image fog in a generated scene-final pass. Scene versions
    // through 2 use the older passthrough shader, whose fragment has no fog.
    // Resolve FOG after shader metadata defaults, just like the native material.
    const auto fragmentComboEnabled = [] (const CPass& pass, std::string_view name) {
	const auto& shader = *pass.getShader ();
	for (const auto* combos : { &pass.getOverride ().combos, &shader.getCombos (),
	    &shader.getFragment ().getDiscoveredCombos (), &shader.getVertex ().getDiscoveredCombos () }) {
	    const auto combo = std::ranges::find_if (*combos, [name] (const auto& entry) {
		return std::ranges::equal (entry.first, name, [] (unsigned char authored, char canonical) {
		    return std::toupper (authored) == canonical;
		});
	    });
	    if (combo != combos->end ()) return combo->second != 0;
	}
	return false;
    };
    const bool finalFog = fogPresentation && !m_passes.empty () && fragmentComboEnabled (*m_passes.front (), "FOG");
    ComboMap finalFogCombos;

    if (finalFog) {
	finalFogCombos.emplace ("FOG_COMPUTED", 1);
	if (m_passes.front ()->getBlendingMode () == BlendingMode_AlphaToCoverage) {
	    // The final pass inherits GPU coverage from the source. Its shader also
	    // needs the discard path for single-sample scene targets.
	    finalFogCombos.emplace ("ALPHATOCOVERAGE", 1);
	}
	for (auto* source : m_passes) {
	    ComboMap sourceCombos;
	    // The generated pass owns fog; the base passes still feed exported textures.
	    if (fragmentComboEnabled (*source, "FOG_COMPUTED")) sourceCombos.emplace ("FOG_COMPUTED", 0);

	    const auto& shader = source->getPass ().shader;
	    if (source == m_passes.front () && m_prelightingPass == nullptr && !m_image.model->passthrough
		&& (shader == "genericimage4" || shader == "genericimage3" || shader == "genericimage2")
		&& (fragmentComboEnabled (*source, "LIGHTING") || fragmentComboEnabled (*source, "REFLECTION"))) {
		m_prelightingPass = source;
		sourceCombos.emplace ("PRELIGHTING", 1);
		if (m_hasPuppetMesh && shader == "genericimage4") sourceCombos.emplace ("PRELIGHTINGDUALVERTEX", 1);
	    }

	    if (sourceCombos.contains ("PRELIGHTINGDUALVERTEX")) {
		// Native 140266fa0 adds an immutable raster stream while the main
		// position is skinned. Our main stream already contains the CPU pose.
		std::vector<GLfloat> rasterPositions = m_puppetRawPositions;
		const auto& auxiliary = m_image.model->puppetMesh->submeshes.front ().auxiliaryPositions;
		if (auxiliary.size () * 3 == rasterPositions.size ()) {
		    rasterPositions.clear ();
		    for (const auto& position : auxiliary)
			rasterPositions.insert (rasterPositions.end (), { position.x, position.y, position.z });
		}
		glGenBuffers (1, &m_puppetPrelightingPosition);
		glBindBuffer (GL_ARRAY_BUFFER, m_puppetPrelightingPosition);
		glBufferData (GL_ARRAY_BUFFER, rasterPositions.size () * sizeof (GLfloat), rasterPositions.data (), GL_STATIC_DRAW);
	    }
	    source->setRuntimeCombos (sourceCombos);
	}
    }
    if (m_prelightingPass != nullptr) {
	// Keep world-space illumination separate from the offscreen image MVP,
	// including a formerly direct puppet now followed by scene fog.
	m_prelightingPass->addUniform ("g_AltModelMatrix", &m_prelightingModelMatrix);
	m_prelightingPass->addUniform ("g_AltNormalModelMatrix", &m_sceneNormalModelMatrix);
	m_prelightingPass->addUniform ("g_AltViewProjectionMatrix", &m_sceneViewProjectionMatrix);
    }

    // prepare the passes list
    if (!debug.baseOnly && !this->getImage ().effects.empty ()) {
	// generate the effects used by this material
	for (const auto& cur : this->m_image.effects) {
	    if (std::find (debug.skipEffects.begin (), debug.skipEffects.end (), static_cast<int> (cur->id))
		!= debug.skipEffects.end ()) {
		continue;
	    }

	    const bool dynamicVisibility = cur->visible->value->getScriptSource ().has_value ()
		|| cur->visible->property != nullptr;
	    if (dynamicVisibility) {
		this->getScene ().getScriptEngine ().queueScript (
		    "visible_fx" + std::to_string (cur->id) + "_" + std::to_string (getId ()),
		    *cur->visible->value, *this
		);
	    }
	    const bool retainedHidden = !cur->effect->functions.empty ();
	    if (!cur->visible->value->getBool () && !dynamicVisibility && !retainedHidden) {
		continue;
	    }
	    const size_t firstEffectPass = m_passes.size ();
	    const bool deferShaderSetup = !cur->visible->value->getBool ();

	    // Register any script-driven shader constants in this effect's pass overrides so their per-frame
	    // scripts actually run. The script source is parsed onto the constant's DynamicValue, but unlike
	    // object-level properties these effect constants are never queued — so e.g. a "rainbow" colour
	    // cycle (a tint colour driven by a JS update()) stays frozen at its static fallback. Queue them
	    // here so tick() advances the value and CPass uploads the live result.
	    {
		int passIndex = 0;
		for (const auto& passOverride : cur->passOverrides) {
		    const auto scope = "effect_" + std::to_string (cur->id) + "_pass"
			+ std::to_string (passIndex) + "_";
		    for (const auto& [constantName, setting] : passOverride->constants) {
			if (setting->value != nullptr && (setting->value->getScriptSource ().has_value ()
			    || setting->animation != nullptr)) {
			    // Linked channels name their parent within this material pass.
			    registerProperty (scope + constantName, *setting, scope);
			    if (setting->animation != nullptr && !setting->animation->name.empty ())
				registerAnimation (setting->animation->name, *setting->animation);
			}
		    }
		    ++passIndex;
		}
	    }

	    const auto fboProvider = std::make_shared<FBOProvider> (this);

	    // create all the fbos for this effect
	    for (const auto& fbo : cur->effect->fbos) {
		auto target = fboProvider->create (*fbo, m_effectSize, this->m_image.model->fullscreen);
		if (this->m_image.model->fullscreen || this->m_compositionFBO != nullptr) {
		    this->m_sceneEffectTargets.push_back ({ std::move (target), fbo.get () });
		}
	    }

	    registerEffectFunctions (*cur, fboProvider);
	    std::vector<std::string> bufferNames;
	    for (const auto& fbo : cur->effect->fbos) bufferNames.push_back (fbo->name);
	    const auto effectBindings = std::make_shared<Effects::EffectFBOBindings> (bufferNames);

	    auto curEffect = cur->effect->passes.begin ();
	    auto endEffect = cur->effect->passes.end ();
	    auto curOverride = cur->passOverrides.begin ();
	    auto endOverride = cur->passOverrides.end ();

	    for (; curEffect != endEffect; ++curEffect) {
		const auto override = curOverride != endOverride
		    ? **curOverride
		    : std::optional<std::reference_wrapper<const ImageEffectPassOverride>> (std::nullopt);
		// Override slots follow authored passes, including commands and disabled passes.
		if (curOverride != endOverride) ++curOverride;
		if (!(*curEffect)->enabled) continue;
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

		    if ((*curEffect)->command == Command_Swap) {
			m_swapCommands.push_back ({ m_passes.size (), effectBindings,
			    *(*curEffect)->source, *(*curEffect)->target, cur->visible.get () });
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
			*this, fboProvider, config, std::nullopt, std::nullopt, (*curEffect)->target.value (),
			{}, deferShaderSetup, effectBindings
		    ));
		    m_passes.back ()->setCopySource (*(*curEffect)->source);
		    m_copyPasses.insert (m_passes.back ());
		} else {
		    for (auto& pass : (*curEffect)->material.value ()->passes) {
			const auto target = (*curEffect)->target.has_value ()
			    ? *(*curEffect)->target
			    : std::optional<std::reference_wrapper<std::string>> (std::nullopt);

			this->m_passes.push_back (
			    new CPass (*this, fboProvider, *pass, override, (*curEffect)->binds, target, {}, deferShaderSetup, effectBindings)
			);
			this->getScene ().loadingCheckpoint ();
		    }
		}
	    }
	    if (dynamicVisibility || retainedHidden) {
		for (size_t index = firstEffectPass; index < m_passes.size (); ++index) {
		    m_passVisibility.emplace (m_passes[index], cur->visible.get ());
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
        const bool hasClipping = this->getImage ().model->puppetMesh.has_value ()
            && !this->getImage ().model->puppetMesh->clippingDescriptors.empty ();
	this->m_materials.colorBlending.material
	    = MaterialParser::load (this->getScene ().getScene ().project,
                hasClipping || finalFog ? "materials/util/effectpassthrough_4.json" : "materials/util/effectpassthrough.json");
	this->m_materials.colorBlending.override = std::make_unique<ImageEffectPassOverride> (ImageEffectPassOverride {
            .id = -1,
            .combos = finalFogCombos,
            .constants = {},
            .textures = {},
        });
	this->m_materials.colorBlending.override->combos.emplace ("BLENDMODE", this->m_image.colorBlendMode->value->getInt ());

	this->m_passes.push_back (new CPass (
	    *this, std::make_shared<FBOProvider> (this), **this->m_materials.colorBlending.material->passes.begin (),
	    *this->m_materials.colorBlending.override, std::nullopt, std::nullopt
	));
	// The base material already applied the layer tint and opacity. Applying
	// them again here suppresses faint layers such as Godzilla's 13% shadow.
	this->m_passes.back ()->addUniform ("g_Color4", &neutralCompositeColor);
	if (finalFog) this->m_fogPass = this->m_passes.back ();
    }

    // puppet-warped images with effects get a dedicated composite pass: effect shaders can
    // manipulate a_Position in image space (e.g. pulse, foliagesway), so hijacking the last
    // effect pass with scene-space puppet geometry mangles the mesh (gojo in 3100265648);
    // a passthrough pass is position-neutral and safe to warp
    // A stable final pass also lets scripted effects switch without moving scene
    // geometry or depth/blend state onto a different effect shader.
    // The color-blending pass already provides stable final geometry. It must
    // sample the background in scene coordinates, not fill a local image target
    // with a miniature copy of the entire scene.
    if ((debug.baseOnly || this->m_image.colorBlendMode->value->getInt () <= 0)
        && (finalFog || this->isCompositionLayer ()
	    || ((this->m_hasPuppetMesh || !m_passVisibility.empty ()) && this->m_passes.size () > 1))) {
	const bool hasClipping = this->getImage ().model->puppetMesh.has_value ()
	    && !this->getImage ().model->puppetMesh->clippingDescriptors.empty ();
	this->m_materials.compatibilityMaterials.emplace_back (
	    MaterialParser::load (
		this->getScene ().getScene ().project,
		hasClipping || finalFog ? "materials/util/effectpassthrough_4.json" : "materials/util/effectpassthrough.json"
	    )
	);
	this->m_materials.compatibilityOverrides.emplace_back (
	    std::make_unique<ImageEffectPassOverride> (ImageEffectPassOverride {
		.id = -1,
		.combos = finalFogCombos,
		.constants = {},
		.textures = {},
	    })
	);

	this->m_passes.push_back (new CPass (
	    *this, std::make_shared<FBOProvider> (this),
	    **this->m_materials.compatibilityMaterials.back ()->passes.begin (),
	    *this->m_materials.compatibilityOverrides.back (), std::nullopt, std::nullopt
	));
	// The source already applied the image tint and opacity.
	this->m_passes.back ()->addUniform ("g_Color4", &neutralCompositeColor);
	if (finalFog) this->m_fogPass = this->m_passes.back ();
    }

    // If there is more than one pass, the first pass renders only to an intermediate FBO while the
    // last pass draws the layer geometry into the scene. Move explicitly enabled depth state from
    // the base material to that final composite pass. Keeping an effect's usual depthtest=disabled
    // there lets a depth-tested background layer overwrite 3D models in front of it (Ventura 4K,
    // Workshop 3298178668). Leave all-disabled 2D effect chains untouched: forcing their state
    // breaks full-resolution blur results followed by another effect (Legendaries of Hoenn,
    // Workshop 3101147701).
    if (this->m_passes.size () > 1) {
	const auto first = this->m_passes.begin ();
	const auto last = std::find_if (m_passes.rbegin (), m_passes.rend (),
	    [this] (auto* pass) { return !m_copyPasses.contains (pass); });

	(*last)->setBlendingMode ((*first)->getBlendingMode ());
	(*first)->setBlendingMode (BlendingMode_Normal);
	if ((*first)->getDepthtestMode () == DepthtestMode_Enabled) {
	    (*last)->setDepthtestMode (DepthtestMode_Enabled);
	    (*first)->setDepthtestMode (DepthtestMode_Disabled);
	}
	if ((*first)->getDepthwriteMode () == DepthwriteMode_Enabled) {
	    (*last)->setDepthwriteMode (DepthwriteMode_Enabled);
	    (*first)->setDepthwriteMode (DepthwriteMode_Disabled);
	}
    }

    CRenderable::setup ();

    this->updateEffectVisibility ();
    this->m_initialized = true;
}

void CImage::updateEffectVisibility () {
    const auto enabled = [&] (Effects::CPass* pass) {
	const auto entry = m_passVisibility.find (pass);
	return entry == m_passVisibility.end ()
	    || (entry->second->value->getBool () && !m_failedEffects.contains (entry->second));
    };
    size_t active = 0;
    bool changed = false;
    for (auto* pass : m_passes) {
	if (!enabled (pass)) continue;
	changed |= active >= m_activePasses.size () || m_activePasses[active] != pass;
	++active;
    }
    if (!changed && active == m_activePasses.size ()) return;
    // Disabled optional shaders must not prevent their owning image from loading.
    // On first activation, prepare the whole effect before routing any of its
    // passes. If compilation fails, bypass that entire effect without retry spam.
    for (auto* pass : m_passes) {
	if (!enabled (pass)) continue;
	try {
	    pass->initialize ();
	    this->getScene ().loadingCheckpoint ();
	} catch (const std::exception& error) {
	    const auto entry = m_passVisibility.find (pass);
	    if (entry == m_passVisibility.end ()) throw;
	    m_failedEffects.insert (entry->second);
	    sLog.error ("Disabling effect on object ", getId (), " after shader setup failed: ", error.what ());
	}
    }
    m_activePasses.clear ();
    for (auto* pass : m_passes) {
	if (enabled (pass)) m_activePasses.push_back (pass);
    }
    setupPasses ();
}

bool CImage::usesReverseProjection (const glm::mat4* projection) const {
    return getScene ().getCamera ().usesReverseDepth ()
	&& (projection == &m_modelViewProjectionScreen || projection == &m_effectSceneProjection
	    || (projection == &m_modelViewProjectionCopy && getImage ().model->passthrough));
}

void CImage::setupPasses () {
    m_currentMainFBO = m_mainFBO;
    m_currentSubFBO = m_subFBO;
    m_finalPassRouting.reset ();
    m_finalPassDrawsToScene = false;
    // do a pass on everything and setup proper inputs and values
    std::shared_ptr<const CFBO> drawTo = this->m_currentMainFBO;
    std::shared_ptr<const TextureProvider> asInput
	= this->m_puppetAlbedoFBO != nullptr ? this->m_puppetAlbedoFBO : this->getTexture ();
    GLuint texcoord = this->m_puppetAlbedoFBO != nullptr ? this->getTexCoordPass () : this->getTexCoordCopy ();

    auto cur = this->m_activePasses.begin ();
    auto end = this->m_activePasses.end ();
    const auto lastMaterial = std::find_if (m_activePasses.rbegin (), m_activePasses.rend (),
	[this] (auto* pass) { return !m_copyPasses.contains (pass); });
    bool first = true;
    bool inTargetEffectSequence = false;
    std::shared_ptr<const TextureProvider> effectInput = nullptr;

    for (; cur != end; ++cur) {
	Effects::CPass* pass = *cur;
	if (m_copyPasses.contains (pass)) {
	    pass->setDestination (pass->resolveFBO (pass->getTarget ()->get ()));
	    pass->setInput (asInput);
	    pass->setPosition (getPassSpacePosition ());
	    pass->setTexCoord (getTexCoordPass ());
	    pass->setModelViewProjectionMatrix (&m_modelViewProjectionPass);
	    continue;
	}
	std::shared_ptr<const CFBO> prevDrawTo = drawTo;
	bool writesToTarget = false;
	const bool isFirstPass = first;
	GLuint spacePosition = (isFirstPass) ? this->getCopySpacePosition () : this->getPassSpacePosition ();
	const glm::mat4* projection
	    = (isFirstPass) ? &this->m_modelViewProjectionCopy : &this->m_modelViewProjectionPass;
	const glm::mat4* inverseProjection
	    = (isFirstPass) ? &this->m_modelViewProjectionCopyInverse : &this->m_modelViewProjectionPassInverse;
	first = false;

	pass->setModelMatrix (&this->m_modelMatrix);
	if (pass == m_prelightingPass && m_puppetPrelightingPosition != GL_NONE) {
	    projection = &m_puppetPrelightingProjection;
	    inverseProjection = &m_puppetPrelightingProjectionInverse;
	    setupPuppetGeometryCallback (pass, m_puppetAlbedoFBO == nullptr);
	}
	pass->setViewProjectionMatrix (pass == this->m_prelightingPass
	    ? &this->m_sceneViewProjectionMatrix : &this->m_viewProjectionMatrix);
	pass->setEffectTextureProjectionMatrix (
	    &this->m_effectTextureProjectionMatrix, &this->m_effectTextureProjectionMatrixInverse
	);
	if (!isFirstPass) {
	    // Intermediate quads draw in NDC, but background reads follow the layer's scene placement.
	    pass->addUniform ("g_EffectModelViewProjectionMatrix", &this->m_framebufferReadProjection);
	}

	writesToTarget = this->configurePassTarget (pass, drawTo, asInput, effectInput, inTargetEffectSequence);
	if (!writesToTarget && this->shouldRenderFinalPass (lastMaterial != m_activePasses.rend () && pass == *lastMaterial)) {
	    // Keep the offscreen route as well as the scene route. A layer may become
	    // visible after scripts initialize, or hide while another layer samples it.
	    this->m_finalPassRouting = FinalPassRouting {
		.pass = pass,
		.offscreenTarget = drawTo,
		.input = asInput,
		.offscreenPosition = spacePosition,
		.offscreenProjection = projection,
		.offscreenProjectionInverse = inverseProjection,
		.samplesSourceTexture = isFirstPass && this->m_puppetAlbedoFBO == nullptr,
		.sceneBlending = this->m_material.passes.front ()->blending,
	    };
	}

	pass->setDestination (drawTo);
	pass->setInput (asInput);
	pass->setPreviousInput (inTargetEffectSequence ? effectInput : nullptr);
	pass->setPosition (spacePosition);
	pass->setTexCoord (texcoord);
	pass->setModelViewProjectionMatrix (projection, usesReverseProjection (projection));
	pass->setModelViewProjectionMatrixInverse (inverseProjection);
	if (this->m_finalPassRouting.has_value () && this->m_finalPassRouting->pass == pass) {
	    this->updateFinalPassVisibility (true);
	}

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
    if (!isLastPass) {
	return false;
    }

    const auto& debug = this->getScene ().getContext ().getApp ().getContext ().settings.render.debug;
    return !(debug.noSolidFinal && this->getImage ().model->solidlayer);
}

void CImage::updateFinalPassVisibility (const bool force) {
    if (!this->m_finalPassRouting.has_value ()) {
	return;
    }
    const bool emptyGeometry = this->m_image.sizeSpecified && !this->m_image.model->fullscreen
	&& (this->m_image.size.x == 0.0f || this->m_image.size.y == 0.0f);
    const auto& debug = this->getScene ().getContext ().getApp ().getContext ().settings.render.debug;
    // Native retains empty effects but suppresses their layer's scene draw,
    // even when another effect follows. Hiding the empty effect restores it.
    const bool emptyEffect = !debug.baseOnly && std::ranges::any_of (m_image.effects, [&debug] (const auto& effect) {
	return effect->visible->value->getBool ()
	    && std::ranges::find (debug.skipEffects, effect->id) == debug.skipEffects.end ()
	    && std::ranges::none_of (effect->effect->passes, [] (const auto& pass) { return pass->enabled; });
    });
    const bool visible = !emptyGeometry && !emptyEffect && this->getImage ().visible->value->getBool ()
	&& this->isVisibleThroughParents ();
    if (!force && visible == this->m_finalPassDrawsToScene) {
	return;
    }

    const auto& route = *this->m_finalPassRouting;
    Effects::CPass* pass = route.pass;
    this->m_finalPassDrawsToScene = visible;
    // A hidden layer publishes RGBA, including transparent pixels. Blending
    // against an earlier ping-pong result resurrects keyed-out backgrounds.
    // Native linear dodge applies addition only once. Its generated shader
    // already includes the background; the base additive blend would add it again.
    const bool additiveColorBlend = this->m_materials.colorBlending.material != nullptr
        && this->m_image.colorBlendMode->value->getInt () == 31
        && route.sceneBlending == BlendingMode_Additive;
    pass->setBlendingMode (visible && !additiveColorBlend ? route.sceneBlending : BlendingMode_Normal);
    if (!visible) {
	pass->setDestination (route.offscreenTarget);
	pass->setPosition (route.offscreenPosition);
	pass->setModelViewProjectionMatrix (route.offscreenProjection, usesReverseProjection (route.offscreenProjection));
	pass->setModelViewProjectionMatrixInverse (route.offscreenProjectionInverse);
	pass->addUniform ("g_EffectModelViewProjectionMatrix",
	    route.offscreenPosition == this->getPassSpacePosition ()
		? &this->m_framebufferReadProjection : route.offscreenProjection);
	pass->setModelMatrix (&this->m_modelMatrix);
	pass->setViewProjectionMatrix (pass == this->m_prelightingPass
	    ? &this->m_sceneViewProjectionMatrix : &this->m_viewProjectionMatrix);
	static const glm::mat3 identityNormal (1.0f);
	pass->addUniform ("g_NormalModelMatrix", &identityNormal);
	pass->setGeometryCallback ({}, {}, {});
	return;
    }

    const auto destination = this->getScene ().getFBO ();
    const glm::mat4* projection;
    const glm::mat4* inverseProjection;
    if (this->getImage ().model->passthrough && this->getImage ().model->fullscreen) {
	// Full-frame post-processing uses the untransformed NDC quad.
	pass->setPosition (this->getPassSpacePosition ());
	projection = &this->m_modelViewProjectionPass;
	inverseProjection = &this->m_modelViewProjectionPassInverse;
    } else {
	pass->setPosition (this->getSceneSpacePosition ());
	projection = &this->m_modelViewProjectionScreen;
	inverseProjection = &this->m_modelViewProjectionScreenInverse;
	// Lit image shaders consume model/view matrices separately from the MVP.
	pass->setModelMatrix (&this->m_sceneModelMatrix);
	pass->setViewProjectionMatrix (&this->m_sceneViewProjectionMatrix);
	pass->addUniform ("g_NormalModelMatrix", &this->m_sceneNormalModelMatrix);
    }
    pass->setDestination (destination);
    pass->setModelViewProjectionMatrix (projection, usesReverseProjection (projection));
    pass->setModelViewProjectionMatrixInverse (inverseProjection);

    if (!route.samplesSourceTexture && !this->m_hasPuppetMesh && !this->getImage ().model->passthrough) {
	// Vertex effects deform local image coordinates before layer scale/rotation.
	// Scene-space vertices bake those transforms too early and reverse Y offsets.
	pass->setPosition (this->m_effectSpacePosition);
	pass->setModelMatrix (&this->m_effectSceneModel);
	pass->setModelViewProjectionMatrix (&this->m_effectSceneProjection, usesReverseProjection (&m_effectSceneProjection));
	pass->setModelViewProjectionMatrixInverse (&this->m_effectSceneProjectionInverse);
	projection = &this->m_effectSceneProjection;
    }

    // Scene draws already carry the layer transform in their vertex/MVP pair.
    pass->addUniform ("g_EffectModelViewProjectionMatrix", projection);

    // Hidden composite textures use the undeformed image quad. Build puppet
    // scene geometry once, and restore it whenever this layer becomes visible.
    if (this->m_hasPuppetMesh) {
	if (!this->m_puppetFinalPassConfigured) {
	    this->m_hasPuppetClipping = this->setupPuppetClippingPasses (
		*pass, route.input, destination, projection, inverseProjection, route.samplesSourceTexture
	    );
	    this->m_puppetFinalPassConfigured = true;
	}
	for (auto* clippingPass : m_puppetClippingPasses)
	    if (clippingPass != this->m_puppetMaskComposePass) clippingPass->setInput (route.input);
	if (!this->m_hasPuppetClipping) {
	    this->setupPuppetGeometryCallback (pass, route.samplesSourceTexture);
	}
    }
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

void CImage::updateSceneTargets () const {
    const bool sceneSized = this->m_image.model->fullscreen || this->m_compositionFBO != nullptr;
    if (!sceneSized && this->m_puppetClippingFBO == nullptr && this->m_compositionFBO == nullptr) {
	return;
    }
    const auto& outputSize = this->getScene ().getOutputSize ();
    const auto targetSize = FBOProvider::calculateTargetSize (outputSize, this->getRenderScale ());
    if (this->m_compositionFBO != nullptr) {
	this->m_compositionFBO->resize (targetSize.x, targetSize.y);
    }
    if (this->m_puppetClippingFBO != nullptr) {
	const auto maskSize = this->m_puppetClippingIntermediateFBO
	    ? glm::max (targetSize / 2u, glm::uvec2 (2)) : targetSize;
	this->m_puppetClippingFBO->resize (maskSize.x, maskSize.y);
	if (this->m_puppetClippingIntermediateFBO)
	    this->m_puppetClippingIntermediateFBO->resize (maskSize.x, maskSize.y);
    }
    if (sceneSized) {
	this->m_mainFBO->resize (targetSize.x, targetSize.y);
	this->m_subFBO->resize (targetSize.x, targetSize.y);
	for (const auto& [target, definition] : this->m_sceneEffectTargets) {
	    const auto effectExtent = FBOProvider::calculateEffectTargetExtent (
		*definition, outputSize, this->getRenderScale ()
	    );
	    // Shared fullscreen storage retains the first allocation's target scale.
	    const auto effectSize = glm::max (
		glm::uvec2 (glm::vec2 (effectExtent) / target->getScale ()), glm::uvec2 (2)
	    );
	    target->resize (effectSize.x, effectSize.y);
	}
    }
}

void CImage::prepareRenderEffects () {
    if (this->m_initialized) {
	updateEffectVisibility ();
    }
}

bool CImage::isVolumetricBarrier () const {
    // Native image gate 1401ea2d0 admits retained/LED sources even when hidden.
    return CObject::isVolumetricBarrier () || this->m_image.ledSource->value->getBool ()
	|| this->getScene ().isRenderTargetRetained (this->m_mainFBO)
	|| this->getScene ().isRenderTargetRetained (this->m_subFBO);
}

void CImage::render () {
    // do not try to render something that did not initialize successfully
    if (!this->m_initialized) {
	return;
    }

    updateEffectVisibility ();
    // Own and inherited visibility gate only the scene draw. Hidden image layers
    // still update their composite textures for model faces and effect inputs;
    // the dress in 3761619125 samples sources inside a hidden parent group.
    this->updateFinalPassVisibility ();
    size_t renderPassCount = this->m_passes.size ();
    if (!this->m_finalPassDrawsToScene && !this->m_hasPuppetMesh && this->m_compositionFBO == nullptr
	&& !this->m_image.ledSource->value->getBool ()
	&& !this->getScene ().isRenderTargetRetained (this->m_mainFBO)
	&& !this->getScene ().isRenderTargetRetained (this->m_subFBO)) {
	// Keep the complete prefix feeding persistent targets and swap commands.
	// Only the unused presentation tail can be dropped: a hidden fluid/ripple
	// simulation must still see the same inputs and advance every frame.
	renderPassCount = 0;
	for (size_t index = 0; index < this->m_passes.size (); ++index) {
	    if (this->m_passes[index]->getTarget ().has_value ()) {
		renderPassCount = index + 1;
	    }
	}
	for (const auto& command : this->m_swapCommands) {
	    renderPassCount = std::max (renderPassCount, command.beforePass);
	}
	if (renderPassCount == 0 && this->m_swapCommands.empty ()) {
	    return;
	}
    }
    this->updateSceneTargets ();

    // Image opacity can be keyframed independently of its texture animation.
    // Keep the evaluated value in stable member storage because CPass uniforms
    // retain a pointer returned by getAlpha()/getUserAlpha(). This is especially
    // important for authored opening overlays that fade away to reveal the live
    // scene below them.
    this->m_resolvedAlpha = this->m_image.alpha->evaluateFloat (this->getScene ().getElapsedTime ());

    glColorMask (true, true, true, true);

    // Compose animated channel-map quads into the base albedo before any effect
    // pass or puppet deformation consumes it.
    this->renderPuppetAlbedo ();

    // Always update screen transform (handles rotation + parallax dynamically);
    // fullscreen/autosize/locked layers are excluded from parallax inside
    this->updateScreenSpacePosition ();
    this->updateEffectSceneMatrix ();

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

    auto cur = this->m_activePasses.begin ();
    const auto end = this->m_activePasses.end ();
    // Native exports the effected image before its final scene blend and fog.
    // Consumers apply their own fog; reusable textures must retain their color.
    const bool skipScenePresentation = !m_finalPassDrawsToScene && m_finalPassRouting.has_value ()
	&& (m_materials.colorBlending.material != nullptr || m_finalPassRouting->pass == m_fogPass);
    auto command = m_swapCommands.begin ();
    for (size_t index = 0; index <= renderPassCount; ++index) {
	for (; command != m_swapCommands.end () && command->beforePass == index; ++command) {
	    if (!m_failedEffects.contains (command->visible)) command->execute ();
	}
	if (index == renderPassCount) {
	    break;
	}
	if (cur == end || *cur != m_passes[index]) continue;
	auto* pass = *cur++;
	if (skipScenePresentation && pass == m_finalPassRouting->pass) continue;
	glColorMask (true, true, true, true);
	if (m_finalPassRouting && pass == m_finalPassRouting->pass && this->m_finalPassDrawsToScene) {
	    if (this->m_hasPuppetClipping) {
		this->renderPuppetClipping ();
		continue;
	    }
	    // Preserve alpha only on the real scene target. A child image rendered
	    // into a composition layer must contribute its authored alpha.
	    glColorMask (
		true, true, true, this->getScene ().isRenderingToComposition () ? GL_TRUE : GL_FALSE
	    );
	}

	if (pass == m_passes.front () && m_fogPass != nullptr
	    && (m_puppetPrelightingPosition != GL_NONE || pass->getPass ().blending == BlendingMode_AlphaToCoverage)) {
	    // Exporting the final effect can fill pixels outside the sparse rest
	    // mesh or cutout. Start the source texture fresh each frame.
	    m_mainFBO->clear (glm::vec4 (0.0f));
	}
	pass->render ();
    }

    // Export the completed effect chain through the named _a texture, before
    // scene fog even when the source is visible. An even pass count ends on _b;
    // otherwise consumers see the penultimate result instead of the last effect.
    // Preserve the working-buffer routes: visible layers and explicit effect
    // bindings may also depend on them. No additional texture allocation is needed.
    if (renderPassCount == m_passes.size () && m_finalPassRouting.has_value ()
	&& (!m_finalPassDrawsToScene || m_finalPassRouting->pass == m_fogPass)) {
	const auto result = skipScenePresentation || m_finalPassRouting->pass == m_fogPass
	    ? std::static_pointer_cast<const CFBO> (m_finalPassRouting->input)
	    : m_finalPassRouting->offscreenTarget;
	if (result != m_mainFBO) {
	    GLint readFramebuffer, drawFramebuffer;
	    glGetIntegerv (GL_READ_FRAMEBUFFER_BINDING, &readFramebuffer);
	    glGetIntegerv (GL_DRAW_FRAMEBUFFER_BINDING, &drawFramebuffer);
	    const GLboolean scissorEnabled = glIsEnabled (GL_SCISSOR_TEST);
	    glDisable (GL_SCISSOR_TEST);
	    glBindFramebuffer (GL_READ_FRAMEBUFFER, result->getFramebuffer ());
	    glBindFramebuffer (GL_DRAW_FRAMEBUFFER, m_mainFBO->getFramebuffer ());
	    glBlitFramebuffer (0, 0, result->getRealWidth (), result->getRealHeight (),
		0, 0, m_mainFBO->getRealWidth (), m_mainFBO->getRealHeight (), GL_COLOR_BUFFER_BIT, GL_NEAREST);
	    glBindFramebuffer (GL_READ_FRAMEBUFFER, readFramebuffer);
	    glBindFramebuffer (GL_DRAW_FRAMEBUFFER, drawFramebuffer);
	    if (scissorEnabled) glEnable (GL_SCISSOR_TEST);
	}
    }

    // Restore alpha writes: leaving the mask disabled leaks it into the next frame's scene clear (the
    // clear silently stops writing alpha) and into any FBO created afterwards (an in-process wallpaper
    // rebuild "clears" its new framebuffers to uninitialized VRAM). The scene buffer's alpha then sticks
    // at whatever the allocation contained, and every alpha-blended writeback composites against it.
    glColorMask (true, true, true, true);

#if !NDEBUG
    glPopDebugGroup ();
#endif /* DEBUG */
}

const float& CImage::getBrightness () const { return this->m_image.brightness->value->getFloat (); }

const float& CImage::getUserAlpha () const { return this->m_resolvedAlpha; }

const float& CImage::getAlpha () const { return this->m_resolvedAlpha; }

const glm::vec3& CImage::getColor () const { return this->m_image.color->value->getVec3 (); }

const glm::vec4& CImage::getColor4 () const {
    const auto& color = *this->m_image.color->value;
    this->m_resolvedColor4 = color.getVec4 ();
    // SceneScript image colors are RGB. General vector conversion fills a
    // missing fourth component with zero, but that must not hide the image.
    // Explicit RGBA colors retain their alpha; layer opacity is applied by CPass.
    if (color.getType () == DynamicValue::UnderlyingType::Vec3) {
	this->m_resolvedColor4.a = 1.0f;
    }
    if (this->getScene ().isHdr ()) {
        const float brightness = this->m_image.brightness->evaluateFloat (this->getScene ().getElapsedTime ());
        this->m_resolvedColor4 *= glm::vec4 (brightness, brightness, brightness, 1.0f);
    }
    return this->m_resolvedColor4;
}

const glm::vec3& CImage::getCompositeColor () const { return this->m_image.color->value->getVec3 (); }

size_t CImage::getPuppetAnimationLayerCount () const { return this->m_image.animationLayers.size (); }

std::optional<size_t> CImage::findPuppetAnimationLayer (const int index) const {
    if (index < 0 || static_cast<size_t> (index) >= this->m_image.animationLayers.size ()) {
	return std::nullopt;
    }
    return static_cast<size_t> (index);
}

std::optional<size_t> CImage::findPuppetAnimationLayer (const std::string& name) const {
    for (size_t index = 0; index < this->m_image.animationLayers.size (); index++) {
	if (this->m_image.animationLayers[index]->name == name) {
	    return index;
	}
    }
    return std::nullopt;
}

float CImage::getPuppetAnimationLayerFrame (const size_t index) const {
    if (index >= this->m_image.animationLayers.size ()) return 0.0f;
    const auto& layer = this->m_image.animationLayers[index];
    const auto found = this->m_puppetLayerPlayback.find (layer->id);
    if (found == this->m_puppetLayerPlayback.end ()) return 0.0f;
    const auto* animation = this->findPuppetAnimationClip (found->second.animation);
    return animation != nullptr && animation->fps > 0.0f ? found->second.time / (1.0f / animation->fps) : 0.0f;
}

void CImage::setPuppetAnimationLayerFrame (const size_t index, const float frame) {
    if (index >= this->m_image.animationLayers.size () || !std::isfinite (frame)) return;
    const auto& layer = this->m_image.animationLayers[index];
    if (!this->m_puppetLayerPlayback[layer->id].initialized) {
	this->selectPuppetAnimations (this->getScene ().getElapsedTime ());
    }
    auto& playback = this->m_puppetLayerPlayback[layer->id];
    const auto* animation = this->findPuppetAnimationClip (playback.animation);
    if (animation == nullptr || !(animation->fps > 0.0f)) return;
    playback.time = frame * (1.0f / animation->fps);
    playback.stopped = false;
}

void CImage::playPuppetAnimationLayer (const std::optional<size_t> index) {
    const auto play = [this] (const size_t layerIndex) {
	if (layerIndex >= this->m_image.animationLayers.size ()) {
	    return;
	}
	auto& playback = this->m_puppetLayerPlayback[this->m_image.animationLayers[layerIndex]->id];
	if (playback.finished) playback.time = 0.0f;
	playback.finished = false;
	playback.playing = true;
	playback.stopped = false;
    };

    if (index.has_value ()) {
	play (*index);
    } else {
	for (size_t layerIndex = 0; layerIndex < this->m_image.animationLayers.size (); layerIndex++) {
	    play (layerIndex);
	}
    }
}

void CImage::pausePuppetAnimationLayer (const std::optional<size_t> index) {
    const auto pause = [this] (const size_t layerIndex) {
	if (layerIndex < this->m_image.animationLayers.size ()) {
	    this->m_puppetLayerPlayback[this->m_image.animationLayers[layerIndex]->id].playing = false;
	}
    };

    if (index.has_value ()) {
	pause (*index);
    } else {
	for (size_t layerIndex = 0; layerIndex < this->m_image.animationLayers.size (); layerIndex++) {
	    pause (layerIndex);
	}
    }
}

void CImage::stopPuppetAnimationLayer (const std::optional<size_t> index) {
    const auto stop = [this] (const size_t layerIndex) {
	if (layerIndex >= this->m_image.animationLayers.size ()) {
	    return;
	}
	auto& playback = this->m_puppetLayerPlayback[this->m_image.animationLayers[layerIndex]->id];
	playback.playing = false;
	playback.stopped = true;
	playback.reverse = playback.finished = false;
	playback.time = 0.0f;
    };

    if (index.has_value ()) {
	stop (*index);
    } else {
	for (size_t layerIndex = 0; layerIndex < this->m_image.animationLayers.size (); layerIndex++) {
	    stop (layerIndex);
	}
    }
}

bool CImage::isPuppetAnimationLayerPlaying (const std::optional<size_t> index) const {
    const auto isPlaying = [this] (const size_t layerIndex) {
	if (layerIndex >= this->m_image.animationLayers.size ()) {
	    return false;
	}
	const int layerId = this->m_image.animationLayers[layerIndex]->id;
	const auto playback = this->m_puppetLayerPlayback.find (layerId);
	return playback == this->m_puppetLayerPlayback.end ()
	    || (playback->second.playing && !playback->second.stopped && !playback->second.finished);
    };

    if (index.has_value ()) {
	return isPlaying (*index);
    }
    for (size_t layerIndex = 0; layerIndex < this->m_image.animationLayers.size (); layerIndex++) {
	if (isPlaying (layerIndex)) {
	    return true;
	}
    }
    return false;
}

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
    // 3D scenes: image quads use local coordinates with the authored alignment;
    // origin/angles/scale (and the parent chain) are applied by the world matrix instead
    // of being baked into the vertices, and there is no screen-space y-flip
    if (this->getScene ().getScene ().camera.projection.isPerspective) {
	this->m_pos.x = -size.x / 2.0f;
	this->m_pos.z = size.x / 2.0f;
	this->m_pos.y = -size.y / 2.0f;
	this->m_pos.w = size.y / 2.0f;
	if (this->m_image.alignment.find ("left") != std::string::npos) {
	    this->m_pos.x += size.x / 2.0f;
	    this->m_pos.z += size.x / 2.0f;
	} else if (this->m_image.alignment.find ("right") != std::string::npos) {
	    this->m_pos.x -= size.x / 2.0f;
	    this->m_pos.z -= size.x / 2.0f;
	}
	if (this->m_image.alignment.find ("top") != std::string::npos) {
	    this->m_pos.y -= size.y / 2.0f;
	    this->m_pos.w -= size.y / 2.0f;
	} else if (this->m_image.alignment.find ("bottom") != std::string::npos) {
	    this->m_pos.y += size.y / 2.0f;
	    this->m_pos.w += size.y / 2.0f;
	}
	return;
    }

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
    const std::array<GLfloat, 18> sceneSpacePosition = {
	this->m_pos.x, this->m_pos.y, 0.0f, this->m_pos.x, this->m_pos.w, 0.0f, this->m_pos.z, this->m_pos.y, 0.0f,
	this->m_pos.z, this->m_pos.y, 0.0f, this->m_pos.x, this->m_pos.w, 0.0f, this->m_pos.z, this->m_pos.w, 0.0f
    };

    float width = 1.0f;
    float height = 1.0f;
    if (this->getTexture () != nullptr && usesLegacyAnimationUvs (*this->getTexture ())) {
	width = static_cast<float> (this->getTexture ()->getRealWidth ())
	    / this->getTexture ()->getResolution ()->x;
	height = static_cast<float> (this->getTexture ()->getRealHeight ())
	    / this->getTexture ()->getResolution ()->y;
    } else if (this->getTexture () != nullptr && !this->getTexture ()->isAnimated ()
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

    const std::array<GLfloat, 12> texcoordCopy = { x, height, x, y, width, height, width, height, x, y, width, y };
    const std::array<GLfloat, 18> copySpacePosition
	= { realX,     realHeight, 0.0f, realX, realY, 0.0f, realWidth, realHeight, 0.0f,
	    realWidth, realHeight, 0.0f, realX, realY, 0.0f, realWidth, realY,      0.0f };

    const auto uploadIfChanged = [this] (GLuint buffer, const auto& values, auto& cached) {
	if (this->m_geometryBufferCacheValid && values == cached) {
	    return;
	}

	glBindBuffer (GL_ARRAY_BUFFER, buffer);
	glBufferData (GL_ARRAY_BUFFER, sizeof (values), values.data (), GL_DYNAMIC_DRAW);
	cached = values;
    };

    uploadIfChanged (this->m_sceneSpacePosition, sceneSpacePosition, this->m_cachedSceneSpacePosition);
    const glm::vec2 half = size * 0.5f;
    const std::array<GLfloat, 18> effectSpacePosition = {
	-half.x, -half.y, 0, -half.x, half.y, 0, half.x, -half.y, 0,
	half.x, -half.y, 0, -half.x, half.y, 0, half.x, half.y, 0
    };
    uploadIfChanged (this->m_effectSpacePosition, effectSpacePosition, this->m_cachedEffectSpacePosition);
    uploadIfChanged (this->m_copySpacePosition, copySpacePosition, this->m_cachedCopySpacePosition);
    uploadIfChanged (this->m_texcoordCopy, texcoordCopy, this->m_cachedTexcoordCopy);
    this->m_geometryBufferCacheValid = true;

    this->m_sceneCenter
	= glm::vec3 ((this->m_pos.x + this->m_pos.z) / 2.0f, (this->m_pos.y + this->m_pos.w) / 2.0f, 0.0f);
    this->m_modelViewProjectionCopy = this->getImage ().model->passthrough
	? this->m_modelViewProjectionScreen
	: glm::ortho<float> (0.0, size.x, 0.0, size.y);
    this->m_modelViewProjectionCopyInverse = glm::inverse (this->m_modelViewProjectionCopy);
    this->m_modelMatrix = glm::ortho<float> (0.0, size.x, 0.0, size.y);
    if (m_puppetPrelightingPosition != GL_NONE) {
	// Rest/auxiliary XYZ is centered and Y-up; image targets use 0..size.
	m_puppetPrelightingProjection = glm::scale (
	    glm::translate (m_modelViewProjectionCopy, glm::vec3 (size * 0.5f, 0.0f)), glm::vec3 (1, -1, 1)
	);
	m_puppetPrelightingProjectionInverse = glm::inverse (m_puppetPrelightingProjection);
    }
}

CImage::ResolvedTransform CImage::updateGeometryBuffers () {
    auto sceneWidth = static_cast<float> (this->getScene ().getWidth ());
    auto sceneHeight = static_cast<float> (this->getScene ().getHeight ());
    const auto transform = this->resolveTransform (this->getImage ());
    glm::vec3 origin = transform.origin;
    const glm::vec3 scale = transform.scale;
    const glm::vec2 size = this->resolveGeometrySize (sceneWidth, sceneHeight, origin);
    this->m_size = size;
    this->updateScenePosition (origin, size, scale, sceneWidth, sceneHeight);
    this->uploadGeometryBuffers (size);

    return transform;
}

void CImage::updateEffectSceneMatrix () {
    if (this->m_size.x == 0.0f || this->m_size.y == 0.0f) return;
    this->m_effectSceneModel = glm::translate (this->m_sceneModelMatrix, this->m_sceneCenter);
    this->m_effectSceneModel = glm::scale (this->m_effectSceneModel, glm::vec3 (
	(this->m_pos.z - this->m_pos.x) / this->m_size.x,
	(this->m_pos.w - this->m_pos.y) / this->m_size.y, 1.0f));
    this->m_effectSceneProjection = this->m_sceneViewProjectionMatrix * this->m_effectSceneModel;
    this->m_effectSceneProjectionInverse = glm::inverse (this->m_effectSceneProjection);
    this->m_framebufferReadProjection = glm::scale (
	this->m_effectSceneProjection, glm::vec3 (this->m_size * glm::vec2 (0.5f, -0.5f), 1.0f)
    );
}

void CImage::updateScreenSpacePosition () {
    const ResolvedTransform transform = this->updateGeometryBuffers ();

    // 3D scenes: the world matrix carries the full transform chain and the camera provides
    // a real perspective view; 2D-only concerns (scene-center rotation, mouse parallax)
    // do not apply
    if (this->getScene ().getScene ().camera.projection.isPerspective) {
	const glm::mat4 world = this->resolveWorldMatrix ();
	const glm::mat4 viewProjection
	    = this->getScene ().getCamera ().getRasterProjection () * this->getScene ().getCamera ().getLookAt ();
	const PerspectiveSceneMatrices matrices = calculatePerspectiveSceneMatrices (world, viewProjection);

	this->m_sceneModelMatrix = matrices.model;
	this->m_sceneViewProjectionMatrix = matrices.viewProjection;
	this->m_sceneNormalModelMatrix = matrices.normalModel;
	this->m_modelViewProjectionScreen = matrices.modelViewProjection;
	this->m_modelViewProjectionScreenInverse = glm::inverse (this->m_modelViewProjectionScreen);
	const bool prelightingDrawsToScene = this->m_finalPassDrawsToScene && this->m_finalPassRouting.has_value ()
	    && this->m_finalPassRouting->pass == this->m_prelightingPass;
	this->m_prelightingModelMatrix = this->m_sceneModelMatrix;
	if (this->m_prelightingPass != nullptr && !prelightingDrawsToScene && m_puppetPrelightingPosition == GL_NONE) {
	    // The first effect input is drawn in 0..size texture coordinates.
	    // Lighting and reflections still need the quad's centered, aligned
	    // coordinates transformed through the real 3D layer hierarchy.
	    this->m_prelightingModelMatrix = glm::translate (
		this->m_sceneModelMatrix, glm::vec3 (this->m_pos.x, this->m_pos.w, 0.0f)
	    );
	    this->m_prelightingModelMatrix = glm::scale (
		this->m_prelightingModelMatrix,
		glm::vec3 ((this->m_pos.z - this->m_pos.x) / this->m_size.x,
		    (this->m_pos.y - this->m_pos.w) / this->m_size.y, 1.0f)
	    );
	}

	if (this->getImage ().model->passthrough) {
	    this->m_modelViewProjectionCopy = this->m_modelViewProjectionScreen;
	    this->m_modelViewProjectionCopyInverse = this->m_modelViewProjectionScreenInverse;
	}

	if (this->m_hasPuppetMesh) this->updatePuppetPositionBuffer (this->m_size, true);
	return;
    }

    // Build rotation from angles (already in radians from scene.json — see CParticle.cpp:2119)
    // Negate X and Z rotations to account for Y-flipped coordinate system (CParticle.cpp:2120)
    // all three axes are resolved through the parent chain by resolveTransform (PR #479)
    const glm::vec3 angles = transform.angles;
    glm::mat4 rotModel = glm::mat4 (1.0f);
    if (glm::dot (angles, angles) > 0.0f) {
	rotModel = glm::translate (rotModel, this->m_sceneCenter);
	rotModel = glm::rotate (rotModel, -angles.z, glm::vec3 (0.0f, 0.0f, 1.0f));
	rotModel = glm::rotate (rotModel, angles.y, glm::vec3 (0.0f, 1.0f, 0.0f));
	rotModel = glm::rotate (rotModel, angles.x, glm::vec3 (-1.0f, 0.0f, 0.0f));
	rotModel = glm::translate (rotModel, -this->m_sceneCenter);

	// same rotation as rotModel above, without the scene-center translate: a direction-only
	// local->world matrix for effect shaders (e.g. depthparallax) that rotate the parallax
	// input into the layer's own axes via g_EffectTextureProjectionMatrixInverse
	this->m_effectTextureProjectionMatrix = glm::mat4 (1.0f);
	this->m_effectTextureProjectionMatrix
	    = glm::rotate (this->m_effectTextureProjectionMatrix, -angles.z, glm::vec3 (0.0f, 0.0f, 1.0f));
	this->m_effectTextureProjectionMatrix
	    = glm::rotate (this->m_effectTextureProjectionMatrix, angles.y, glm::vec3 (0.0f, 1.0f, 0.0f));
	this->m_effectTextureProjectionMatrix
	    = glm::rotate (this->m_effectTextureProjectionMatrix, angles.x, glm::vec3 (-1.0f, 0.0f, 0.0f));
	// pure rotation matrix: inverse == transpose, cheaper and numerically exact
	this->m_effectTextureProjectionMatrixInverse = glm::transpose (this->m_effectTextureProjectionMatrix);
    } else {
	this->m_effectTextureProjectionMatrix = glm::mat4 (1.0f);
	this->m_effectTextureProjectionMatrixInverse = glm::mat4 (1.0f);
    }

    const auto& camera = this->getScene ().getCamera ();
    this->m_sceneViewProjectionMatrix = camera.getProjection () * camera.getLookAt ();
    if (this->m_image.perspective->value->getBool () && !this->m_image.model->fullscreen) {
	// Native image draws use the same virtual camera as mixed-scene models.
	// The quad is already centered and Y-flipped; retain its existing transforms.
	const float fov = glm::radians (glm::clamp (camera.getFov (), 0.1f, 179.9f));
	const float distance = 1.0f / (std::tan (fov * 0.5f) * camera.getProjection ()[1][1]);
	glm::mat4 view = camera.getLookAt ();
	view[3][2] = -distance;
	const auto& output = this->getScene ().getOutputSize ();
	this->m_sceneViewProjectionMatrix = glm::perspective (fov, output.x / output.y, 5.0f,
	    glm::max (15000.0f, distance + 1000.0f)) * view;
    }
    glm::mat4 model (1.0f);

    // The 2D geometry buffer stores authoring-space X/Y only. Preserve the
    // resolved Z origin in the model transform so tilted image layers remain at
    // their authored depth instead of being forced onto the camera plane.
    model = glm::translate (model, { 0.0f, 0.0f, transform.origin.z });

    // Apply parallax displacement if enabled — folded into the matrix before the rotation,
    // so the offset stays in scene space instead of being rotated with the object (PR #479)
    // fullscreen layers always cover the projection, so parallax never moves them.
    // locktransforms does NOT exclude a layer: it's an editor-UI lock, and wallpapers like
    // 3135984503 author every layer locked with real parallax depths; layers that shouldn't
    // move simply have parallaxDepth 0 (e.g. the background in 2665939987)
    if (!this->getImage ().model->fullscreen) {
	model = glm::translate (model, glm::vec3 (this->resolveParallaxOffset (), 0.0f));
    }

    this->m_sceneModelMatrix = model * rotModel;
    // Orthographic geometry and lights reflect authored Y into render space.
    // Reflect the image tangent basis too, so normal maps keep their authored
    // light direction instead of exchanging illumination from above and below.
    this->m_sceneNormalModelMatrix = glm::mat3 (rotModel)
	* glm::mat3 (glm::scale (glm::mat4 (1.0f), glm::vec3 (1.0f, -1.0f, 1.0f)));
    if (this->m_prelightingPass != nullptr) {
	this->m_prelightingModelMatrix = this->m_sceneModelMatrix;
	const bool drawsToScene = this->m_finalPassDrawsToScene && this->m_finalPassRouting.has_value ()
	    && this->m_finalPassRouting->pass == this->m_prelightingPass;
	if (!drawsToScene && m_puppetPrelightingPosition == GL_NONE) {
	    // Match each copy-space corner (0..size) to its authored scene-space
	    // corner before applying rotation, depth and parallax.
	    this->m_prelightingModelMatrix = glm::translate (
		this->m_prelightingModelMatrix, glm::vec3 (this->m_pos.x, this->m_pos.w, 0.0f)
	    );
	    this->m_prelightingModelMatrix = glm::scale (
		this->m_prelightingModelMatrix,
		glm::vec3 ((this->m_pos.z - this->m_pos.x) / this->m_size.x,
		    (this->m_pos.y - this->m_pos.w) / this->m_size.y, 1.0f)
	    );
	}
    }
    const glm::mat4 mvp = this->m_sceneViewProjectionMatrix * this->m_sceneModelMatrix;

    this->m_modelViewProjectionScreen = mvp;
    this->m_modelViewProjectionScreenInverse = glm::inverse (mvp);
    if (this->getImage ().model->passthrough) {
	this->m_modelViewProjectionCopy = this->m_modelViewProjectionScreen;
	this->m_modelViewProjectionCopyInverse = this->m_modelViewProjectionScreenInverse;
    }
    // Rope vertices compensate against the completed draw model; setup still samples
    // without stepping, and each render update evaluates the puppet exactly once.
    if (this->m_hasPuppetMesh) this->updatePuppetPositionBuffer (this->m_size, true);
}

auto CImage::calculatePerspectiveSceneMatrices (const glm::mat4& world, const glm::mat4& viewProjection)
    -> PerspectiveSceneMatrices {
    return PerspectiveSceneMatrices {
	.model = world,
	.viewProjection = viewProjection,
	.modelViewProjection = viewProjection * world,
	.normalModel = Render::Utils::calculateNormalModelMatrix (world),
    };
}

const Image& CImage::getImage () const { return this->m_image; }

bool CImage::isCompositionLayer () const {
    return this->m_image.model != nullptr
	&& (this->m_image.model->filename == "models/util/composelayer.json"
	    || this->m_image.model->filename == "models/util/composelayer_depthtest.json");
}

bool CImage::copiesCompositionBackground () const { return this->m_image.copyBackground; }

std::shared_ptr<const CFBO> CImage::getCompositionFBO () const {
    // Composition children draw before the parent image's render() call.
    this->updateSceneTargets ();
    return this->m_compositionFBO;
}

glm::mat4 CImage::resolveLayerModelMatrix () const {
    const glm::vec2 half = this->getSize () * 0.5f;
    glm::vec3 offset (0.0f);
    if (m_image.alignment.find ("left") != std::string::npos) offset.x = half.x;
    else if (m_image.alignment.find ("right") != std::string::npos) offset.x = -half.x;
    if (m_image.alignment.find ("top") != std::string::npos) offset.y = -half.y;
    else if (m_image.alignment.find ("bottom") != std::string::npos) offset.y = half.y;
    return glm::translate (this->resolveWorldMatrix (), offset);
}

glm::vec2 CImage::getSize () const {
    if (this->m_image.sizeSpecified && !this->m_image.model->fullscreen
	&& (this->m_image.size.x == 0.0f || this->m_image.size.y == 0.0f)) {
	return this->m_image.size;
    }
    if (this->m_size.x > 0.0f && this->m_size.y > 0.0f) {
	return this->m_size;
    }
    if (this->m_texture == nullptr) {
	return this->getImage ().size;
    }

    return { this->m_texture->getRealWidth (), this->m_texture->getRealHeight () };
}

std::optional<glm::mat4> CImage::getAttachmentTransform (const std::string& name) const {
    return MdlAnimationEvaluator::attachmentTransform (this->m_puppetAnimation, this->m_puppetWorldBones, name);
}

std::optional<size_t> CImage::getAttachmentIndex (const std::string& name) const {
    size_t index = 0;
    for (const auto& attachmentName : this->m_puppetAnimation.attachments | std::views::keys) {
	if (attachmentName == name) {
	    return index;
	}
	index++;
    }
    return std::nullopt;
}

std::optional<std::string> CImage::getAttachmentName (const size_t requestedIndex) const {
    size_t index = 0;
    for (const auto& attachmentName : this->m_puppetAnimation.attachments | std::views::keys) {
	if (index++ == requestedIndex) {
	    return attachmentName;
	}
    }
    return std::nullopt;
}

std::optional<size_t> CImage::getBoneIndex (const std::string& name) const {
    if (name.empty ()) return std::nullopt;
    for (size_t index = 0; index < this->m_puppetAnimation.bones.size (); ++index) {
	if (this->m_puppetAnimation.bones[index].name == name) return index;
    }
    return std::nullopt;
}

std::optional<glm::mat4> CImage::getBoneTransform (const size_t index) const {
    if (!this->m_puppetPublishedSceneBones.empty ()) {
        if (index >= this->m_puppetPublishedSceneBones.size ()) return std::nullopt;
        return this->m_puppetPublishedSceneBones[index];
    }
    if (index >= this->m_puppetWorldBones.size ()) return std::nullopt;
    return this->resolveWorldMatrix () * this->m_puppetWorldBones[index];
}

glm::mat3 CImage::calculateOrthographicAttachmentTextureMatrix (
    const glm::mat4& attachmentWorld, const glm::mat4& receiverWorld,
    const glm::mat4& sourceViewProjection, const glm::mat4& receiverViewProjection,
    const float cameraZ, const glm::vec2& size, const std::optional<glm::vec3> receiverEye,
    const float receiverClipDepth
) {
    const glm::mat4 sourceClip = sourceViewProjection * attachmentWorld;
    const glm::mat4 unproject = glm::inverse (receiverViewProjection);
    const glm::mat4 receiverInverse = glm::inverse (receiverWorld);
    const auto project = [&] (const float x) {
        const glm::vec4 clip = sourceClip * glm::vec4 (x, 0, 0, 1);
        const glm::vec4 screen = unproject * glm::vec4 (glm::vec2 (clip) / clip.w, receiverEye ? receiverClipDepth : 0, 1);
        const glm::vec3 screenWorld = glm::vec3 (screen) / screen.w;
        // Perspective receivers cast from their eye; ordinary orthographic
        // receivers retain the parallel +Z ray.
        const glm::vec3 rayOrigin = receiverEye.value_or (glm::vec3 (screenWorld.x, screenWorld.y, cameraZ - 2000.0f));
        const glm::vec3 rayDirection = receiverEye ? glm::normalize (screenWorld - *receiverEye) : glm::vec3 (0, 0, 1);
        const glm::vec3 origin = glm::vec3 (receiverInverse * glm::vec4 (rayOrigin, 1));
        const glm::vec3 direction = glm::vec3 (receiverInverse * glm::vec4 (rayDirection, 0));
        // Native ignores the unit-triangle hit flag: extrapolate outside the
        // image and behind the ray. Parallel rays retain zero barycentrics.
        const glm::vec2 hit = std::abs (direction.z) <= std::numeric_limits<float>::epsilon ()
            ? glm::vec2 (0) : glm::vec2 (origin - direction * (origin.z / direction.z));
        return glm::vec2 (.5f) + (hit - .5f) / size * glm::vec2 (1, -1);
    };
    const glm::vec2 origin = project (0);
    glm::vec2 axis = project (100) - origin;
    const float length = glm::length (axis);
    axis = length <= std::numeric_limits<float>::epsilon () ? glm::vec2 (0) : axis / length;
    return glm::mat3 (glm::vec3 (axis, 0), glm::vec3 (axis.y, -axis.x, 0), glm::vec3 (origin, 1));
}

std::optional<glm::mat3> CImage::transformAttachmentToTexture (
    const CObject& source, const glm::mat4& attachmentWorld
) const {
    const auto& camera = this->getScene ().getCamera ();
    const Object& object = source.getObject ();
    const bool virtualReceiver = this->m_image.perspective->value->getBool ();
    bool virtualSource = false;
    if (object.is<Image> ()) {
        const Image& image = *object.as<Image> ();
        if (image.model->fullscreen) return std::nullopt;
        virtualSource = image.perspective->value->getBool ();
    } else if (object.is<Model3D> ()) {
        virtualSource = object.as<Model3D> ()->perspective->value->getBool ();
    } else {
        return std::nullopt;
    }
    // Native fullscreen receivers project through their authored transform,
    // using their logical texture dimensions rather than the fullscreen draw extent.
    const glm::vec2 size = this->m_image.model->fullscreen
        ? glm::vec2 (this->m_texture->getRealWidth (), this->m_texture->getRealHeight ()) : this->getSize ();
    const glm::mat4 receiverWorld = this->m_image.model->fullscreen
        ? this->resolveWorldMatrix () : this->resolveLayerModelMatrix ();
    if (!camera.isOrthogonal ()) {
        const glm::mat4 viewProjection = camera.getProjection () * camera.getLookAt ();
        const glm::vec3 receiverEye = virtualReceiver
            ? glm::vec3 (glm::inverse (camera.getLookAt ())[3]) : camera.getEye ();
        // Native depth .5 / 1 maps to conventional GL clip depth 0 / 1.
        return calculateOrthographicAttachmentTextureMatrix (
            attachmentWorld, receiverWorld, viewProjection, viewProjection,
            camera.getEye ().z, size, receiverEye, virtualReceiver ? 1.0f : 0.0f);
    }
    const glm::mat4 sceneToRender = glm::translate (glm::mat4 (1),
        glm::vec3 (-camera.getWidth () * .5f, camera.getHeight () * .5f, 0))
        * glm::scale (glm::mat4 (1), glm::vec3 (1, -1, 1));
    const glm::mat4 viewProjection = camera.getProjection () * camera.getLookAt () * sceneToRender;
    glm::mat4 virtualView (1), virtualViewProjection (1);
    if (virtualSource || virtualReceiver) {
        // Image and model virtual layers use the same render camera.
        const float fov = glm::radians (glm::clamp (camera.getFov (), 0.1f, 179.9f));
        const float distance = 1.0f / (std::tan (fov * 0.5f) * camera.getProjection ()[1][1]);
        glm::mat4 view = camera.getLookAt ();
        view[3][2] = -distance;
        const auto& output = this->getScene ().getOutputSize ();
        virtualView = view * sceneToRender;
        virtualViewProjection = glm::perspective (fov, output.x / output.y, 5.0f,
            glm::max (15000.0f, distance + 1000.0f)) * virtualView;
    }
    // Render offsets inherit the root layer. Convert their Y-flipped space
    // back to authored coordinates before composing the camera transforms.
    const glm::vec2 sourceOffset = source.resolveParallaxOffset () * glm::vec2 (1, -1);
    const glm::vec2 receiverOffset = this->resolveParallaxOffset () * glm::vec2 (1, -1);
    std::optional<glm::vec3> receiverEye;
    if (virtualReceiver) {
        receiverEye = glm::vec3 (glm::inverse (glm::translate (virtualView, glm::vec3 (receiverOffset, 0)))[3]);
    }
    return calculateOrthographicAttachmentTextureMatrix (
        attachmentWorld, receiverWorld,
        glm::translate (virtualSource ? virtualViewProjection : viewProjection, glm::vec3 (sourceOffset, 0)),
        glm::translate (virtualReceiver ? virtualViewProjection : viewProjection, glm::vec3 (receiverOffset, 0)),
        camera.getEye ().z, size, receiverEye);
}

static glm::mat4 cursorPlaneTransform (const glm::mat4& world) {
    // A 2D image can have zero Z scale while retaining a valid XY hit plane.
    // Complete its basis from the two in-plane axes before taking an inverse.
    glm::mat4 plane = world;
    if (glm::determinant (plane) == 0.0f) {
	const glm::vec3 normal = glm::cross (glm::vec3 (plane[0]), glm::vec3 (plane[1]));
	const float length = glm::length (normal);
	if (std::isfinite (length) && length > 0.0f) plane[2] = glm::vec4 (normal / length, 0.0f);
    }
    return plane;
}

std::optional<glm::vec3> CImage::intersectCursorPlane (
    const glm::mat4& world, const glm::mat4& viewProjection,
    const glm::vec2& normalizedPosition, const bool projectionYFlipped
) {
    glm::vec2 ndc = normalizedPosition * 2.0f - 1.0f;
    if (projectionYFlipped) ndc.y = -ndc.y;
    const glm::mat4 inverse = glm::inverse (viewProjection * cursorPlaneTransform (world));
    const glm::vec4 near4 = inverse * glm::vec4 (ndc, -1.0f, 1.0f);
    const glm::vec4 far4 = inverse * glm::vec4 (ndc, 1.0f, 1.0f);
    const glm::vec3 near = glm::vec3 (near4) / near4.w;
    const glm::vec3 ray = glm::vec3 (far4) / far4.w - near;
    if (!std::isfinite (ray.z) || std::abs (ray.z) <= glm::length (ray) * 1e-6f) {
	return std::nullopt;
    }
    const float distance = -near.z / ray.z;
    if (!std::isfinite (distance) || distance < 0.0f || distance > 1.0f) return std::nullopt;
    const glm::vec3 local = near + distance * ray;
    if (!std::isfinite (local.x) || !std::isfinite (local.y)) return std::nullopt;
    return glm::vec3 (local.x, local.y, 0.0f);
}

std::optional<glm::vec3> CImage::cursorLocalPosition (const glm::vec3& worldPosition) const {
    // Native dispatches to hidden images, including children of hidden parents.
    // Visibility controls propagation blocking, not the layer's hit plane.

    const glm::mat4 world = cursorPlaneTransform (this->resolveWorldMatrix ());
    const float determinant = glm::determinant (world);
    // Small world-space UI plates are valid: Ocarina's note buttons use scales
    // of only a few thousandths. An absolute determinant threshold rejects them.
    if (!std::isfinite (determinant) || determinant == 0.0f) {
	return std::nullopt;
    }

    const auto& camera = getScene ().getCamera ();
    const auto& parallax = getScene ().getScene ().camera.parallax;
    glm::vec3 hitPosition = worldPosition;
    if (camera.isOrthogonal () && !this->getImage ().model->fullscreen
	&& parallax.enabled->value->getBool ()
	&& !getContext ().getApp ().getContext ().settings.mouse.disableparallax) {
	// Native input uses the queried image's own origin and depth, even when
	// its rendered position inherits a different root-layer parallax offset.
	const glm::vec2 depth = getImage ().parallaxDepth->value->getVec2 ();
	const glm::vec2 offset = calculateParallaxOffset (
	    glm::vec2 (getObject ().origin->evaluateVec3 (getScene ().getElapsedTime ())), depth,
	    { getScene ().getWidth (), getScene ().getHeight () }, *getScene ().getParallaxDisplacement (),
	    parallax.amount->value->getFloat ()
	);
	// Undo the scene-space translation before applying the parent/local inverse.
	hitPosition -= glm::vec3 (offset.x, -offset.y, 0.0f);
    }
    std::optional<glm::vec3> position;
    if (camera.isOrthogonal () && getImage ().perspective->value->getBool () && !getImage ().model->fullscreen) {
	// The native perspective image flag selects the virtual camera ray even
	// in an orthographic scene. Keep its input offset in authored coordinates.
	const float fov = glm::radians (glm::clamp (camera.getFov (), 0.1f, 179.9f));
	const float distance = 1.0f / (std::tan (fov * 0.5f) * camera.getProjection ()[1][1]);
	glm::mat4 view = camera.getLookAt ();
	view[3][2] = -distance;
	const auto& output = getScene ().getOutputSize ();
	const glm::mat4 projection = glm::perspective (fov, output.x / output.y, 5.0f,
	    glm::max (15000.0f, distance + 1000.0f));
	const glm::mat4 sceneToRender = glm::translate (glm::mat4 (1.0f),
	    glm::vec3 (-camera.getWidth () * 0.5f, camera.getHeight () * 0.5f, 0.0f))
	    * glm::scale (glm::mat4 (1.0f), glm::vec3 (1.0f, -1.0f, 1.0f));
	const glm::mat4 hitWorld = glm::translate (sceneToRender, worldPosition - hitPosition) * world;
	position = intersectCursorPlane (hitWorld, projection * view,
	    *getScene ().getMousePositionNormalized (), true);
    } else {
	position = camera.isOrthogonal ()
	    ? std::optional<glm::vec3> (glm::vec3 (glm::inverse (world) * glm::vec4 (hitPosition, 1.0f)))
	    : intersectCursorPlane (world, camera.getProjection () * camera.getLookAt (),
		*getScene ().getMousePositionNormalized (), camera.isYFlipped ());
    }
    if (!position) return std::nullopt;
    const glm::vec3 local = *position;
    const glm::vec2 size = this->getSize ();

    float left = -size.x * 0.5f;
    float right = size.x * 0.5f;
    float bottom = -size.y * 0.5f;
    float top = size.y * 0.5f;
    if (this->m_image.alignment.find ("left") != std::string::npos) {
	left = 0.0f;
	right = size.x;
    } else if (this->m_image.alignment.find ("right") != std::string::npos) {
	left = -size.x;
	right = 0.0f;
    }
    if (this->m_image.alignment.find ("top") != std::string::npos) {
	bottom = -size.y;
	top = 0.0f;
    } else if (this->m_image.alignment.find ("bottom") != std::string::npos) {
	bottom = 0.0f;
	top = size.y;
    }

    if (local.x < left || local.x > right || local.y < bottom || local.y > top) {
	return std::nullopt;
    }
    return local;
}

std::optional<std::string> CImage::cursorHitBox (const glm::vec3& worldPosition) const {
    if (!m_hasPuppetMesh || m_puppetAnimation.hitBoxes.empty ()) return std::nullopt;
    const glm::mat4 world = cursorPlaneTransform (resolveLayerModelMatrix ());
    const float determinant = glm::determinant (world);
    if (!std::isfinite (determinant) || determinant == 0.0f) return std::nullopt;
    const auto& camera = getScene ().getCamera ();
    glm::vec3 origin;
    glm::vec3 direction;
    if (camera.isOrthogonal ()) {
	glm::vec3 position = worldPosition;
	if (!getImage ().model->fullscreen) {
	    const glm::vec2 offset = resolveParallaxOffset ();
	    position -= glm::vec3 (offset.x, -offset.y, 0.0f);
	}
	const glm::mat4 inverse = glm::inverse (world);
	origin = glm::vec3 (inverse * glm::vec4 (position, 1.0f));
	direction = glm::vec3 (inverse * glm::vec4 (0.0f, 0.0f, 1.0f, 0.0f));
    } else {
	glm::vec2 ndc = *getScene ().getMousePositionNormalized () * 2.0f - 1.0f;
	if (camera.isYFlipped ()) ndc.y = -ndc.y;
	const glm::mat4 inverse = glm::inverse (camera.getProjection () * camera.getLookAt () * world);
	const glm::vec4 near = inverse * glm::vec4 (ndc, -1.0f, 1.0f);
	const glm::vec4 far = inverse * glm::vec4 (ndc, 1.0f, 1.0f);
	origin = glm::vec3 (near) / near.w;
	direction = glm::vec3 (far) / far.w - origin;
    }
    return MdlAnimationEvaluator::hitBox (m_puppetAnimation, m_puppetWorldBones, origin, direction);
}

GLuint CImage::getSceneSpacePosition () const { return this->m_sceneSpacePosition; }

GLuint CImage::getCopySpacePosition () const { return this->m_copySpacePosition; }

GLuint CImage::getPassSpacePosition () const { return this->m_passSpacePosition; }

GLuint CImage::getTexCoordCopy () const { return this->m_texcoordCopy; }

GLuint CImage::getTexCoordPass () const { return this->m_texcoordPass; }
