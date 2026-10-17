#include "CModel.h"

#include "WallpaperEngine/Data/Builders/UserSettingBuilder.h"
#include "WallpaperEngine/Data/Model/Material.h"
#include "WallpaperEngine/Data/Model/Project.h"
#include "WallpaperEngine/Logging/Log.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <string_view>

using namespace WallpaperEngine;
using namespace WallpaperEngine::Render::Objects;

extern float g_Time;

CModel::CModel (Wallpapers::CScene& scene, const ModelObject& model) :
    CObject (scene, model), CRenderable (scene, model, *model.material), ScriptableObject (scene, model),
    m_model (model) {
    this->registerProperty ("origin", *model.origin->value);
    this->registerProperty ("scale", *model.scale->value);
    this->registerProperty ("angles", *model.angles->value);
    this->registerProperty ("visible", *model.visible->value);
    this->registerProperty ("alpha", *model.alpha->value);
    this->registerProperty ("color", *model.color->value);

    for (const auto& layer : model.animationLayers) {
	const std::string prefix = "animationLayer" + std::to_string (layer->id) + "_";
	this->registerProperty (prefix + "rate", *layer->rate->value);
	this->registerProperty (prefix + "visible", *layer->visible->value);
	this->registerProperty (prefix + "blend", *layer->blend->value);
	this->registerProperty (prefix + "animation", *layer->animation->value);
    }

    this->registerMaterialProperties ("material", *model.material);
    for (size_t materialIndex = 0; materialIndex < model.extraMaterials.size (); materialIndex++) {
	this->registerMaterialProperties (
	    "extraMaterial" + std::to_string (materialIndex), *model.extraMaterials[materialIndex]
	);
    }

    this->detectTexture ();
}

CModel::~CModel () {
    for (auto& submesh : m_submeshes) {
	delete submesh.pass;
	if (submesh.vbo != GL_NONE) {
	    glDeleteBuffers (1, &submesh.vbo);
	}
	if (submesh.ebo != GL_NONE) {
	    glDeleteBuffers (1, &submesh.ebo);
	}
	if (submesh.vao != GL_NONE) {
	    glDeleteVertexArrays (1, &submesh.vao);
	}
    }
}

void CModel::setup () {
    if (m_initialized) {
	return;
    }

    CRenderable::setup ();

    if (!this->loadMesh ()) {
	sLog.error ("Model object ", this->getId (), " has no usable mesh - skipping");
	return;
    }

    for (auto& submesh : m_submeshes) {
	this->setupPass (submesh);
    }

    this->updateMatrices ();
    m_initialized = true;
}

bool CModel::loadMesh () {
    const auto stream = this->getScene ().getScene ().project.assetLocator->read (m_model.modelFile);
    const std::vector<char> data { std::istreambuf_iterator<char> (*stream), std::istreambuf_iterator<char> () };

    if (data.size () < 32 || std::memcmp (data.data (), "MDLV", 4) != 0) {
	sLog.error ("Not an MDLV model: ", m_model.modelFile);
	return false;
    }

    const size_t magicEnd = std::string_view (data.data (), data.size ()).find ('\0');
    if (magicEnd == std::string_view::npos) {
	sLog.error ("Unterminated MDLV magic in ", m_model.modelFile);
	return false;
    }

    uint32_t mdlvVersion = 0;
    if (magicEnd >= 8) {
	for (size_t digit = 4; digit < 8; digit++) {
	    const char ch = data[digit];
	    if (ch < '0' || ch > '9') {
		mdlvVersion = 0;
		break;
	    }
	    mdlvVersion = mdlvVersion * 10 + static_cast<uint32_t> (ch - '0');
	}
    }

    size_t offset = magicEnd + 1 + 2 * sizeof (uint32_t);
    if (offset + sizeof (uint32_t) > data.size ()) {
	sLog.error ("Truncated MDLV header in ", m_model.modelFile);
	return false;
    }

    uint32_t submeshCount = 0;
    std::memcpy (&submeshCount, data.data () + offset, sizeof (submeshCount));
    offset += sizeof (submeshCount);
    if (submeshCount == 0 || submeshCount > 16) {
	sLog.error ("Unexpected submesh count ", submeshCount, " in ", m_model.modelFile);
	return false;
    }

    const auto readU32 = [&data, &offset] (uint32_t& out) -> bool {
	if (offset + sizeof (uint32_t) > data.size ()) {
	    return false;
	}
	std::memcpy (&out, data.data () + offset, sizeof (out));
	offset += sizeof (uint32_t);
	return true;
    };

    GLint previousVAO = 0;
    glGetIntegerv (GL_VERTEX_ARRAY_BINDING, &previousVAO);

    for (uint32_t index = 0; index < submeshCount; index++) {
	if (offset >= data.size ()) {
	    sLog.error ("Truncated submesh record ", index, " in ", m_model.modelFile);
	    break;
	}

	const auto* nameEnd = static_cast<const char*> (std::memchr (data.data () + offset, 0, data.size () - offset));
	if (nameEnd == nullptr) {
	    sLog.error ("Unterminated submesh material name in ", m_model.modelFile);
	    break;
	}
	offset = static_cast<size_t> (nameEnd - data.data ()) + 1;

	if (offset + sizeof (uint32_t) + 6 * sizeof (float) > data.size ()) {
	    sLog.error ("Truncated submesh metadata in ", m_model.modelFile);
	    break;
	}
	offset += sizeof (uint32_t) + 6 * sizeof (float);

	uint32_t vertexTag = 0;
	if (!readU32 (vertexTag)) {
	    sLog.error ("Bad vertex tag in submesh ", index, " of ", m_model.modelFile);
	    break;
	}

	size_t vertexStride = 48;
	GLuint uvOffset = 40;
	if (vertexTag == 0x0180000fu) {
	    vertexStride = 80;
	    uvOffset = 72;
	} else if (vertexTag == 0u && mdlvVersion != 0 && mdlvVersion < 16) {
	    vertexStride = 52;
	    uvOffset = 44;
	} else if (vertexTag != 15u) {
	    sLog.error ("Unsupported MDLV vertex layout tag ", vertexTag, " in ", m_model.modelFile);
	    break;
	}

	uint32_t vertexBytes = 0;
	if (!readU32 (vertexBytes) || vertexBytes == 0 || vertexBytes % vertexStride != 0
	    || offset + vertexBytes > data.size ()) {
	    sLog.error ("Bad vertex block in submesh ", index, " of ", m_model.modelFile);
	    break;
	}
	const size_t verticesOffset = offset;
	offset += vertexBytes;

	uint32_t indexBytes = 0;
	if (!readU32 (indexBytes) || indexBytes == 0 || indexBytes % (sizeof (uint16_t) * 3) != 0
	    || offset + indexBytes > data.size ()) {
	    sLog.error ("Bad index block in submesh ", index, " of ", m_model.modelFile);
	    break;
	}
	const size_t indicesOffset = offset;
	offset += indexBytes;
	// MDLV0023 stores six reserved bytes after each index block. They are part
	// of the submesh record and must be skipped before reading the next one.
	if (mdlvVersion >= 23) {
	    constexpr size_t trailerSize = 6;
	    if (offset + trailerSize > data.size ()) {
		sLog.error ("Truncated MDLV0023 submesh trailer in ", m_model.modelFile);
		break;
	    }
	    offset += trailerSize;
	}

	const size_t vertexCount = vertexBytes / vertexStride;
	const auto indexCount = static_cast<GLsizei> (indexBytes / sizeof (uint16_t));
	const auto* indices = reinterpret_cast<const uint16_t*> (data.data () + indicesOffset);
	bool valid = true;
	for (GLsizei i = 0; i < indexCount; i++) {
	    if (indices[i] >= vertexCount) {
		valid = false;
		break;
	    }
	}
	if (!valid) {
	    sLog.error ("Mesh index out of range in submesh ", index, " of ", m_model.modelFile);
	    continue;
	}

	Submesh submesh {};
	submesh.indexCount = indexCount;
	submesh.stride = static_cast<GLsizei> (vertexStride);
	submesh.uvOffset = uvOffset;
	submesh.skinned = vertexTag == 0x0180000fu;
	if (submesh.skinned) {
	    submesh.bindVertices.assign (data.data () + verticesOffset, data.data () + verticesOffset + vertexBytes);
	}

	if (index == 0) {
	    submesh.material = m_model.material.get ();
	} else if (index - 1 < m_model.extraMaterials.size ()) {
	    submesh.material = m_model.extraMaterials[index - 1].get ();
	}

	if (submesh.material == nullptr || submesh.material->passes.empty ()) {
	    sLog.error ("Submesh ", index, " of ", m_model.modelFile, " has no material pass");
	    continue;
	}

	glGenVertexArrays (1, &submesh.vao);
	glGenBuffers (1, &submesh.vbo);
	glGenBuffers (1, &submesh.ebo);

	glBindVertexArray (submesh.vao);
	glBindBuffer (GL_ARRAY_BUFFER, submesh.vbo);
	glBufferData (GL_ARRAY_BUFFER, vertexBytes, data.data () + verticesOffset, GL_STATIC_DRAW);
	glBindBuffer (GL_ELEMENT_ARRAY_BUFFER, submesh.ebo);
	glBufferData (GL_ELEMENT_ARRAY_BUFFER, indexBytes, data.data () + indicesOffset, GL_STATIC_DRAW);

	m_submeshes.push_back (std::move (submesh));
    }

    glBindVertexArray (static_cast<GLuint> (previousVAO));
    this->setupAnimationLayers (data);
    return !m_submeshes.empty ();
}

void CModel::setupAnimationLayers (const std::vector<char>& modelData) {
    if (m_model.animationLayers.empty ()) {
	return;
    }

    std::string error;
    auto parsed = PuppetModel::parse (modelData, error);
    if (!parsed.has_value () || !parsed->hasAnimation ()) {
	sLog.out ("Skinned model ", m_model.modelFile, " has no usable skeleton/animation: ", error);
	return;
    }
    m_puppetModel = std::move (*parsed);

    for (const auto& layer : m_model.animationLayers) {
	const auto animationId = static_cast<uint32_t> (layer->animation->value->getInt ());
	const auto* clip = m_puppetModel->findClip (animationId);
	if (clip == nullptr) {
	    sLog.out ("Model ", m_model.modelFile, " has no animation clip ", animationId, "; layer ignored");
	    continue;
	}
	m_animationLayers.push_back ({ .clip = clip, .layer = layer.get () });
    }

    sLog.out (
	"Loaded skinned model ", m_model.modelFile, " bones=", m_puppetModel->bones.size (),
	" clips=", m_puppetModel->clips.size (), " activeLayers=", m_animationLayers.size ()
    );
}

std::optional<Scripting::ScriptableObject::AnimationLayerProperties>
CModel::findAnimationLayer (const std::string& name) const {
    for (const auto& binding : this->m_animationLayers) {
	if (binding.clip == nullptr || binding.layer == nullptr || binding.clip->name != name
	    || binding.layer->rate == nullptr || binding.layer->rate->value == nullptr
	    || binding.layer->visible == nullptr || binding.layer->visible->value == nullptr) {
	    continue;
	}
	return Scripting::ScriptableObject::AnimationLayerProperties { .rate = binding.layer->rate->value.get (),
								       .visible
								       = binding.layer->visible->value.get () };
    }
    return std::nullopt;
}

void CModel::updateSkinning () {
    if (!m_puppetModel.has_value () || m_animationLayers.empty ()) {
	return;
    }

    m_activeAnimationLayers.clear ();
    for (const auto& binding : m_animationLayers) {
	if (!binding.layer->visible->value->getBool ()) {
	    continue;
	}
	m_activeAnimationLayers.push_back (
	    {
		.clip = binding.clip,
		.rate = binding.layer->rate->value->getFloat (),
		.blend = binding.layer->blend->value->getFloat (),
		.additive = binding.layer->additive,
	    }
	);
    }
    if (m_activeAnimationLayers.empty ()) {
	for (auto& submesh : m_submeshes) {
	    if (!submesh.skinned || submesh.bindVertices.empty ()) {
		continue;
	    }
	    glBindBuffer (GL_ARRAY_BUFFER, submesh.vbo);
	    glBufferSubData (
		GL_ARRAY_BUFFER, 0, static_cast<GLsizeiptr> (submesh.bindVertices.size ()), submesh.bindVertices.data ()
	    );
	}
	return;
    }

    m_puppetModel->evaluateSkinning (
	m_activeAnimationLayers, static_cast<double> (this->getScene ().getTime ()), m_skinMatrices
    );
    for (auto& submesh : m_submeshes) {
	if (!submesh.skinned || submesh.bindVertices.empty ()) {
	    continue;
	}
	constexpr size_t stride = 80;
	constexpr size_t normalOffset = 12;
	constexpr size_t tangentOffset = 24;
	constexpr size_t indicesOffset = 40;
	constexpr size_t weightsOffset = 56;
	m_skinnedVertexScratch = submesh.bindVertices;
	const size_t vertexCount = m_skinnedVertexScratch.size () / stride;
	for (size_t vertex = 0; vertex < vertexCount; vertex++) {
	    char* v = m_skinnedVertexScratch.data () + vertex * stride;
	    float position[3], normal[3], tangent[4], weights[4];
	    uint32_t indices[4];
	    std::memcpy (position, v, sizeof (position));
	    std::memcpy (normal, v + normalOffset, sizeof (normal));
	    std::memcpy (tangent, v + tangentOffset, sizeof (tangent));
	    std::memcpy (indices, v + indicesOffset, sizeof (indices));
	    std::memcpy (weights, v + weightsOffset, sizeof (weights));

	    glm::vec4 skinnedPosition (0.0f);
	    glm::vec3 skinnedNormal (0.0f);
	    glm::vec3 skinnedTangent (0.0f);
	    float totalWeight = 0.0f;
	    for (int influence = 0; influence < 4; influence++) {
		const float weight = weights[influence];
		const uint32_t bone = indices[influence];
		if (weight <= 0.0f || bone >= m_skinMatrices.size ()) {
		    continue;
		}
		const auto& skin = m_skinMatrices[bone];
		skinnedPosition += skin * glm::vec4 (position[0], position[1], position[2], 1.0f) * weight;
		skinnedNormal += glm::mat3 (skin) * glm::vec3 (normal[0], normal[1], normal[2]) * weight;
		skinnedTangent += glm::mat3 (skin) * glm::vec3 (tangent[0], tangent[1], tangent[2]) * weight;
		totalWeight += weight;
	    }
	    if (totalWeight <= 1e-6f) {
		continue;
	    }
	    const float inverseWeight = 1.0f / totalWeight;
	    skinnedPosition *= inverseWeight;
	    if (glm::length (skinnedNormal) > 1e-6f) {
		skinnedNormal = glm::normalize (skinnedNormal);
	    } else {
		skinnedNormal = glm::vec3 (normal[0], normal[1], normal[2]);
	    }
	    if (glm::length (skinnedTangent) > 1e-6f) {
		skinnedTangent = glm::normalize (skinnedTangent);
	    } else {
		skinnedTangent = glm::vec3 (tangent[0], tangent[1], tangent[2]);
	    }
	    const glm::vec3 outPosition (skinnedPosition);
	    std::memcpy (v, glm::value_ptr (outPosition), sizeof (position));
	    std::memcpy (v + normalOffset, glm::value_ptr (skinnedNormal), sizeof (normal));
	    std::memcpy (v + tangentOffset, glm::value_ptr (skinnedTangent), sizeof (tangent) - sizeof (float));
	}
	glBindBuffer (GL_ARRAY_BUFFER, submesh.vbo);
	glBufferSubData (
	    GL_ARRAY_BUFFER, 0, static_cast<GLsizeiptr> (m_skinnedVertexScratch.size ()), m_skinnedVertexScratch.data ()
	);
    }
}

void CModel::setupPass (Submesh& submesh) {
    const auto& firstPass = **submesh.material->passes.begin ();

    submesh.fboProvider = std::make_shared<FBOProvider> (this);
    submesh.passOverride = std::make_unique<ImageEffectPassOverride> ();

    // Until the scene-light stage is available, never select Wallpaper Engine's
    // dynamic LightingV1 branch: our current generated module is a zero-light stub,
    // which turns otherwise valid 3D model materials black/invisible. Render the
    // authored albedo/material path as the global fallback instead.
    submesh.passOverride->combos["LIGHTING"] = 0;

    submesh.pass
	= new Effects::CPass (*this, submesh.fboProvider, firstPass, *submesh.passOverride, std::nullopt, std::nullopt);

    submesh.pass->setDestination (this->getScene ().getFBO ());
    submesh.pass->setInput (this->getTexture ());
    submesh.pass->setModelViewProjectionMatrix (&m_mvpMatrix);
    submesh.pass->setModelViewProjectionMatrixInverse (&m_mvpMatrixInverse);
    submesh.pass->setModelMatrix (&m_modelMatrix);
    submesh.pass->setViewProjectionMatrix (&m_viewProjectionMatrix);
    submesh.pass->addUniform ("g_NormalModelMatrix", &m_normalMatrix);
    submesh.pass->addUniform ("g_EyePosition", &m_eyePosition);

    const GLuint program = submesh.pass->getProgramID ();
    const GLsizei stride = submesh.stride;

    GLint previousVAO = 0;
    glGetIntegerv (GL_VERTEX_ARRAY_BINDING, &previousVAO);
    glBindVertexArray (submesh.vao);
    glBindBuffer (GL_ARRAY_BUFFER, submesh.vbo);
    glBindBuffer (GL_ELEMENT_ARRAY_BUFFER, submesh.ebo);

    const GLint locPosition = glGetAttribLocation (program, "a_Position");
    const GLint locNormal = glGetAttribLocation (program, "a_Normal");
    const GLint locTangent = glGetAttribLocation (program, "a_Tangent4");
    const GLint locTexCoord = glGetAttribLocation (program, "a_TexCoord");

    if (locPosition >= 0) {
	glEnableVertexAttribArray (locPosition);
	glVertexAttribPointer (locPosition, 3, GL_FLOAT, GL_FALSE, stride, nullptr);
    }
    if (locNormal >= 0) {
	glEnableVertexAttribArray (locNormal);
	glVertexAttribPointer (locNormal, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*> (12));
    }
    if (locTangent >= 0) {
	glEnableVertexAttribArray (locTangent);
	glVertexAttribPointer (locTangent, 4, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*> (24));
    }
    if (locTexCoord >= 0) {
	glEnableVertexAttribArray (locTexCoord);
	glVertexAttribPointer (
	    locTexCoord, 2, GL_FLOAT, GL_FALSE, stride,
	    reinterpret_cast<void*> (static_cast<uintptr_t> (submesh.uvOffset))
	);
    }

    glBindVertexArray (static_cast<GLuint> (previousVAO));

    Submesh* current = &submesh;
    submesh.pass->setGeometryCallback (
	[current] () {
	    glGetIntegerv (GL_VERTEX_ARRAY_BINDING, &current->prevVAO);
	    glBindVertexArray (current->vao);
	    glFrontFace (GL_CW);
	},
	[current] () { glDrawElements (GL_TRIANGLES, current->indexCount, GL_UNSIGNED_SHORT, nullptr); },
	[current] () {
	    glFrontFace (GL_CCW);
	    glBindVertexArray (static_cast<GLuint> (current->prevVAO));
	}
    );
}

namespace {
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
	s -= (bx (s) - t) / dx;
	s = std::clamp (s, 0.0f, 1.0f);
    }

    const float inv = 1.0f - s;
    return 3.0f * inv * s * s + s * s * s;
}

float evalChannel (const AnimationChannel& channel, const float frame, const float fallback) {
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
}

glm::vec3 CModel::effectiveAngles () const {
    const glm::vec3 authored = m_model.angles->value->getVec3 ();
    const auto* animation = m_model.anglesAnimation.get ();
    if (animation == nullptr || animation->maxFrame <= 0.0f) {
	return authored;
    }

    const float frame = std::fmod (this->getScene ().getTime () * PropertyAnimation::FPS, animation->maxFrame);
    return {
	evalChannel (animation->channels[0], frame, authored.x),
	evalChannel (animation->channels[1], frame, authored.y),
	evalChannel (animation->channels[2], frame, authored.z),
    };
}

void CModel::updateMatrices () {
    const auto& camera = this->getScene ().getCamera ();
    const float sceneWidth = camera.getWidth ();
    const float sceneHeight = camera.getHeight ();

    // Resolve the full authored parent chain for models, just as image layers
    // do. Model properties can be script-driven, so read their current values
    // every frame rather than caching the resulting matrix at setup time.
    constexpr size_t kMaxParentDepth = 32;
    std::vector<const Object*> chain { &m_model };
    const Object* current = &m_model;
    while (current->parent.has_value () && chain.size () < kMaxParentDepth) {
	const CObject* parentRender = this->getScene ().getObject (*current->parent);
	if (parentRender == nullptr) {
	    break;
	}
	current = &parentRender->getObject ();
	chain.push_back (current);
    }

    const bool orthogonal = camera.isOrthogonal ();
    const auto nodeMatrix = [this, orthogonal, sceneWidth, sceneHeight] (const Object& object, bool root) {
	glm::vec3 origin = object.origin->value->getVec3 ();
	glm::vec3 angles = object.groupAngles->value->getVec3 ();
	glm::vec3 scale = object.groupScale->value->getVec3 ();

	if (object.is<Image> ()) {
	    const auto* image = object.as<Image> ();
	    angles = image->angles->value->getVec3 ();
	    scale = image->scale->value->getVec3 ();
	} else if (object.is<ModelObject> ()) {
	    const auto* model = object.as<ModelObject> ();
	    angles = model->angles->value->getVec3 ();
	    scale = model->scale->value->getVec3 ();
	    const CObject* renderObject = this->getScene ().getObject (object.id);
	    if (const auto* modelRender = dynamic_cast<const CModel*> (renderObject); modelRender != nullptr) {
		angles = modelRender->effectiveAngles ();
	    }
	} else if (object.is<Particle> ()) {
	    const auto* particle = object.as<Particle> ();
	    angles = particle->angles->value->getVec3 ();
	    scale = particle->scale->value->getVec3 ();
	} else if (object.is<Text> ()) {
	    scale = object.as<Text> ()->scale->value->getVec3 ();
	}

	if (orthogonal) {
	    // The root converts authored top-left scene coordinates to the camera's
	    // centered, y-up space. Descendant origins are local offsets in that space.
	    origin.x -= root ? sceneWidth / 2.0f : 0.0f;
	    origin.y = (root ? sceneHeight / 2.0f : 0.0f) - origin.y;
	}

	glm::mat4 matrix = glm::translate (glm::mat4 (1.0f), origin);
	matrix = glm::rotate (matrix, orthogonal ? -angles.z : angles.z, glm::vec3 (0, 0, 1));
	matrix = glm::rotate (matrix, angles.y, glm::vec3 (0, 1, 0));
	matrix = glm::rotate (matrix, orthogonal ? -angles.x : angles.x, glm::vec3 (1, 0, 0));
	matrix = glm::scale (matrix, scale);
	if (orthogonal) {
	    matrix = glm::scale (matrix, glm::vec3 (1.0f, -1.0f, 1.0f));
	}
	return matrix;
    };

    // Fold transforms from the root down so animated parents affect every
    // nested model, including skyboxes and other multi-part scene assets.
    m_modelMatrix = glm::mat4 (1.0f);
    for (auto it = chain.rbegin (); it != chain.rend (); ++it) {
	m_modelMatrix *= nodeMatrix (**it, it == chain.rbegin ());
    }

    if (!orthogonal) {
	m_viewProjectionMatrix = camera.getProjection () * camera.getLookAt ();
	m_eyePosition = camera.getEye ();
    } else if (m_model.perspective) {
	const float sceneFov = glm::radians (camera.getFov ());
	const float overrideFov = camera.getOverrideFov ();
	const float projectionFov = overrideFov > 0.0f ? glm::radians (overrideFov) : sceneFov;
	const float eyeZ = (sceneHeight * 0.5f) / std::tan (projectionFov * 0.5f);
	const float nearz = std::max (camera.getNearZ (), 1.0f);
	const float farz = std::max (camera.getFarZ (), eyeZ + 10.0f * sceneHeight);

	const glm::mat4 projection = glm::perspective (projectionFov, sceneWidth / sceneHeight, nearz, farz);
	const glm::mat4 view
	    = glm::lookAt (glm::vec3 (0.0f, 0.0f, eyeZ), glm::vec3 (0.0f), glm::vec3 (0.0f, 1.0f, 0.0f));
	m_viewProjectionMatrix = projection * view;
	m_eyePosition = glm::vec3 (0.0f, 0.0f, eyeZ);
    } else {
	m_viewProjectionMatrix = camera.getProjection () * camera.getLookAt ();
	m_eyePosition = glm::vec3 (0.0f, 0.0f, 1000.0f);
    }

    m_mvpMatrix = m_viewProjectionMatrix * m_modelMatrix;
    m_mvpMatrixInverse = glm::inverse (m_mvpMatrix);
    m_normalMatrix = glm::inverseTranspose (glm::mat3 (m_modelMatrix));
}

void CModel::render () {
    if (!m_initialized || !m_model.visible->value->getBool ()) {
	return;
    }

    this->updateMatrices ();
    this->updateSkinning ();
    for (const auto& submesh : m_submeshes) {
	submesh.pass->render ();
    }
}

const float& CModel::getBrightness () const { return m_brightness; }
const float& CModel::getUserAlpha () const { return m_model.alpha->value->getFloat (); }
const float& CModel::getAlpha () const { return m_model.alpha->value->getFloat (); }
const glm::vec3& CModel::getColor () const { return m_model.color->value->getVec3 (); }
const glm::vec4& CModel::getColor4 () const {
    m_color4 = { this->getColor (), this->getAlpha () };
    return m_color4;
}
const glm::vec3& CModel::getCompositeColor () const { return this->getColor (); }
