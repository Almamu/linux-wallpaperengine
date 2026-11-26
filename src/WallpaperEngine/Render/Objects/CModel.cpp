#include "CModel.h"
#include "ModelAnimationVolume.h"
#include "WallpaperEngine/Data/Model/MdlAnimationBounds.h"
#include "WallpaperEngine/Render/Utils/NormalMatrix.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <numeric>
#include <ranges>
#include <sstream>
#include <string_view>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "WallpaperEngine/Data/Model/MdlAnimation.h"
#include "WallpaperEngine/Data/Model/MdlMorph.h"
#include "WallpaperEngine/Logging/Log.h"
#include "WallpaperEngine/Data/Parsers/MaterialParser.h"
#include "WallpaperEngine/Scripting/ScriptEngine.h"

using namespace WallpaperEngine;
using namespace WallpaperEngine::Render::Objects;
using WallpaperEngine::Render::RenderSortClass;

namespace {
constexpr std::array<std::pair<const char*, const char*>, 11> foliageShadowUniforms {{
    { "g_SpeedLeaves", "foliagespeedleaves" }, { "g_SpeedBase", "foliagespeedbase" },
    { "g_StrengthLeaves", "strengthleaves" }, { "g_StrengthBase", "strengthbase" },
    { "g_Phase", "foliagephase" }, { "g_FoliageScale", "foliagescale" },
    { "g_Direction", "scrolldirection" }, { "g_CutOff", "foliagecutoff" },
    { "g_TreeHeight", "foliageheight" }, { "g_TreeRadius", "foliageradius" },
    { "g_FoliageUVBounds", "foliageuvbounds" }
}};

int foliageLeavesUVMode (const Effects::CPass& pass) {
    const auto& shader = *pass.getShader ();
    // A project-local foliage4 can replace the stock vertex shader.
    if (!std::ranges::all_of (foliageShadowUniforms, [&shader] (const auto& uniform) {
	const auto* parameter = shader.findParameter (uniform.second).vertex;
	return parameter != nullptr && parameter->getName () == uniform.first;
    })) return -1;

    const auto& vertex = shader.getVertex ();
    const auto& fragment = shader.getFragment ();
    for (const auto* combos : { &pass.getOverride ().combos, &vertex.getCombos (), &vertex.getDiscoveredCombos (),
	&fragment.getCombos (), &fragment.getDiscoveredCombos () }) {
	const auto mode = std::ranges::find_if (*combos, [] (const auto& combo) {
	    return std::ranges::equal (combo.first, std::string_view ("LEAVESUVMODE"),
		[] (unsigned char letter, char uppercase) { return std::toupper (letter) == uppercase; });
	});
	if (mode != combos->end ()) {
	    // Other values use the stock helper's unweighted leaf path.
	    return mode->second >= 1 && mode->second <= 4 ? mode->second : 0;
	}
    }
    return 0;
}

glm::vec4 foliageShadowValue (const Effects::CPass& pass, const char* name, const double time) {
    const UserSetting* setting = nullptr;
    if (const auto found = pass.getOverride ().constants.find (name); found != pass.getOverride ().constants.end ()) {
	setting = found->second.get ();
    } else if (const auto found = pass.getPass ().constants.find (name); found != pass.getPass ().constants.end ()) {
	setting = found->second.get ();
    }
    const auto parameter = pass.getShader ()->findParameter (name);
    glm::vec4 value = setting ? setting->value->getVec4 () : parameter.vertex->getVec4 ();
    if (setting && setting->animation) {
	for (int channel = 0; channel < 4; ++channel) {
	    value[channel] = setting->animation->relative
		? value[channel] + setting->animation->evaluateChannel (channel, time, 0.0f)
		: setting->animation->evaluateChannel (channel, time, value[channel]);
	}
    }
    return value;
}

bool supportsModelMorphShader (const std::string& shader) {
    // Stock generic3/4 and model_vertex_v1 consumers apply the same local
    // position/normal deltas before skinning and any shader-specific motion.
    return shader == "generic3" || shader == "generic4" || shader == "chroma4"
        || shader == "foliage4" || shader == "fur4";
}

RenderSortClass passRenderClass (const BlendingMode mode) {
    switch (mode) {
	case BlendingMode_Translucent:
	    return RenderSortClass::Translucent;
	case BlendingMode_Additive:
	    return RenderSortClass::Additive;
	case BlendingMode_Normal:
	case BlendingMode_AlphaToCoverage:
	case BlendingMode_Unknown:
	default:
	    return RenderSortClass::Opaque;
    }
}
} // namespace

ModelData::ModelData (std::vector<Scripting::ModelDataShape> input) {
    glm::vec3 minimum (std::numeric_limits<float>::infinity ());
    glm::vec3 maximum (-std::numeric_limits<float>::infinity ());
    bool hasPositions = false;
    for (auto& source : input) {
	Shape shape;
	shape.id = m_nextShapeId++;
	shape.input = std::move (source);
	shape.geometryMaterial = shape.input.material;
	shape.geometryVertexFormat = shape.input.vertexFormat;
	shape.drawCount = static_cast<GLsizei> (shape.input.indexWidth == 0
	    ? shape.input.vertices.size () * sizeof (float) / shape.input.strideBytes
	    : shape.input.indices.size () / shape.input.indexWidth);
	updateLayout (shape);
	const auto& layout = shape.layout;
	if (layout.positionOffset != MdlMesh::AttributeAbsent) {
	    for (size_t vertex = 0; vertex < shape.input.vertices.size ();
		 vertex += layout.strideBytes / sizeof (float)) {
		const float* position = shape.input.vertices.data () + vertex + layout.positionOffset / sizeof (float);
		minimum = glm::min (minimum, glm::vec3 (position[0], position[1], position[2]));
		maximum = glm::max (maximum, glm::vec3 (position[0], position[1], position[2]));
		hasPositions = true;
	    }
	}
	if (shapes.empty ()) {
	    mesh = layout;
	}
	MdlSubmesh submesh;
	submesh.vertexTag = layout.vertexTag;
	submesh.strideBytes = layout.strideBytes;
	submesh.materialPath = shape.input.material;
	mesh.submeshes.push_back (std::move (submesh));
	shapes.push_back (std::move (shape));
    }
    mesh.hasBoundingBox = hasPositions;
    if (hasPositions) {
	mesh.boundingBoxMin = minimum;
	mesh.boundingBoxMax = maximum;
    }
}

void ModelData::updateLayout (Shape& shape) {
    auto& layout = shape.layout;
    layout.vertexTag = shape.input.vertexFormat;
    layout.strideBytes = shape.input.strideBytes;
    uint32_t offset = 0;
    const auto attribute = [&] (uint32_t flag, uint32_t components) {
	if ((layout.vertexTag & flag) == 0) {
	    return MdlMesh::AttributeAbsent;
	}
	const auto start = offset;
	offset += components * sizeof (float);
	return start;
    };
    layout.positionOffset = attribute (1, 3);
    layout.normalOffset = attribute (2, 3);
    layout.tangentOffset = attribute (4, 4);
    layout.uvOffset = attribute (8, 2);
    shape.colorOffset = attribute (0x8000, 4);
}

void ModelData::upload () {
    GLint arrayBuffer = 0;
    glGetIntegerv (GL_ARRAY_BUFFER_BINDING, &arrayBuffer);
    for (auto& shape : shapes) {
	glGenBuffers (1, &shape.vertexBuffer);
	glBindBuffer (GL_ARRAY_BUFFER, shape.vertexBuffer);
	glBufferData (
	    GL_ARRAY_BUFFER, shape.input.vertices.size () * sizeof (float), shape.input.vertices.data (),
	    shape.input.vertexDynamic ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW
	);
	if (shape.input.indexWidth != 0) {
	    glGenBuffers (1, &shape.indexBuffer);
	    glBindBuffer (GL_ARRAY_BUFFER, shape.indexBuffer);
	    glBufferData (
		GL_ARRAY_BUFFER, shape.input.indices.size (), shape.input.indices.data (),
		shape.input.indexDynamic ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW
	    );
	}
    }
    glBindBuffer (GL_ARRAY_BUFFER, static_cast<GLuint> (arrayBuffer));
}

const char* ModelData::applyData (const std::vector<Scripting::ModelDataShape>& updates) {
    if (updates.size () > shapes.size ()) {
	return "Cannot add shapes in IModelData.update";
    }
    for (size_t index = 0; index < updates.size (); ++index) {
	const auto& update = updates[index];
	auto& shape = shapes[index];
	const auto& original = shape.input;
	if (update.deleteShape || update.deleteIndices) {
	    return "Cannot delete shape or buffers in IModelData.update";
	}
	if (!update.material.empty () && update.material != original.material) {
	    return "Material cannot be changed in IModelData.update";
	}
	if (update.vertexFormat != 0 && update.vertexFormat != original.vertexFormat) {
	    return "Vertex format cannot be changed in IModelData.update";
	}
	const size_t indexCount = update.indexCount;
	const size_t originalIndexCount = original.indexWidth == 0 ? 0 : original.indices.size () / original.indexWidth;
	if ((update.hasVertices && (!original.vertexDynamic || update.vertexFloatCount > original.vertices.size ()))
	    || (update.indexWidth != 0 && (!original.indexDynamic || indexCount > originalIndexCount))) {
	    return "Model data not created as dynamic";
	}
	auto uploadBytes = [] (GLuint buffer, size_t bytes, const void* data) {
	    GLint previous = 0;
	    glGetIntegerv (GL_ARRAY_BUFFER_BINDING, &previous);
	    glBindBuffer (GL_ARRAY_BUFFER, buffer);
	    glBufferSubData (GL_ARRAY_BUFFER, 0, bytes, data);
	    glBindBuffer (GL_ARRAY_BUFFER, static_cast<GLuint> (previous));
	};
	if (update.hasVertices) {
	    // Native maps the full allocation and copies that size, including a short
	    // view's backing tail. Parsing already copied and bounds-checked those bytes.
	    uploadBytes (shape.vertexBuffer, original.vertices.size () * sizeof (float), update.vertices.data ());
	    std::copy_n (update.vertices.begin (), original.vertices.size (), shape.input.vertices.begin ());
	    const size_t count = original.indexWidth == 0 ? update.vertexFloatCount / original.strideBytes : indexCount;
	    if (count > 0) {
		shape.drawCount = static_cast<GLsizei> (count);
	    }
	}
	if (update.indexWidth != 0) {
	    if (update.indexWidth != original.indexWidth) {
		return "Incorrect index buffer type";
	    }
	    uploadBytes (shape.indexBuffer, original.indices.size (), update.indices.data ());
	    std::copy_n (update.indices.begin (), original.indices.size (), shape.input.indices.begin ());
	    shape.drawCount = static_cast<GLsizei> (indexCount);
	}
    }
    return nullptr;
}

const char* ModelData::replaceData (std::vector<Scripting::ModelDataShape> replacements) {
    // Native validates only a supplied material on recreation. Omitted material
    // retains the path and bypasses that gate. Scene-owned resources validate
    // supplied materials; resources without a validator keep their known history.
    for (size_t index = 0; index < replacements.size (); ++index) {
	const auto& replacement = replacements[index];
	if (replacement.deleteShape) {
	    if (index >= shapes.size ()) return "Cannot delete a missing model shape";
	    continue;
	}
	const bool appended = index >= shapes.size ();
	const Shape empty;
	const auto& original = appended ? empty.input : shapes[index].input;
	if (appended && (replacement.material.empty () || !validateMaterial)) {
	    return "Appending model shapes requires a supplied material and scene validation";
	}
	const bool layoutChanged = replacement.vertexFormat != 0 && replacement.vertexFormat != original.vertexFormat;
	const size_t originalIndexCount = original.indexWidth == 0 ? 0 : original.indices.size () / original.indexWidth;
	const bool recreate = appended || layoutChanged || (replacement.deleteIndices && original.indexWidth != 0)
	    || (replacement.hasVertices && (!original.vertexDynamic || replacement.vertexFloatCount > original.vertices.size ()))
	    || (replacement.indexWidth != 0 && (!original.indexDynamic || replacement.indexCount > originalIndexCount));
	const bool suppliedMaterial = !replacement.material.empty ();
	if (recreate && suppliedMaterial && !validateMaterial && (original.material != shapes[index].geometryMaterial
	    || (replacement.vertexFormat & shapes[index].geometryVertexFormat) != shapes[index].geometryVertexFormat)) {
	    return "IModelData.replaceData requires compiled shader validation for changed material or missing original semantics";
	}
	if ((layoutChanged && (replacement.vertexFormat & ~0x800fu) != 0)
	    || (!layoutChanged && replacement.vertexFormat != 0 && replacement.strideBytes != original.strideBytes)
	    || (!recreate && original.indexWidth == 0 && replacement.indexWidth != 0)
	    || (recreate && (!replacement.hasVertices
		|| replacement.vertexFormat == 0 || replacement.strideBytes == 0
		|| (suppliedMaterial && !validateMaterial && replacement.material != original.material)
		|| replacement.vertexFloatCount > replacement.vertices.size ()
		|| (replacement.indexWidth != 0 && (replacement.indexWidth != 2 && replacement.indexWidth != 4))
		|| (replacement.indexWidth != 0 && replacement.indexCount > replacement.indices.size () / replacement.indexWidth)))) {
	    return "IModelData.replaceData currently requires unchanged material, retained semantics and complete recreated shapes";
	}
	if (!recreate && ((replacement.hasVertices && replacement.vertices.size () != original.vertices.size ())
	    || (replacement.indexWidth != 0 && replacement.indexWidth == original.indexWidth
		&& replacement.indices.size () != original.indices.size ()))) {
	    return "Model replacement backing copy does not match the original allocation";
	}
	uint32_t packedStride = 0;
	if (layoutChanged) {
	    for (const auto [flag, components] : std::array<std::pair<uint32_t, uint32_t>, 5> {
		     { { 1, 3 }, { 2, 3 }, { 4, 4 }, { 8, 2 }, { 0x8000, 4 } } }) {
		if ((replacement.vertexFormat & flag) != 0) {
		    packedStride += components * sizeof (float);
		}
	    }
	}
	if (recreate && ((layoutChanged && replacement.strideBytes != packedStride)
	    || (replacement.vertexFloatCount * sizeof (float)) % replacement.strideBytes != 0)) {
	    return "Inconsistent vertex buffer size";
	}
    }
    GLint previous = 0;
    glGetIntegerv (GL_ARRAY_BUFFER_BINDING, &previous);
    for (size_t index = 0; index < replacements.size (); ++index) {
	auto& replacement = replacements[index];
	if (replacement.deleteShape) continue;
	const bool appended = index >= shapes.size ();
	if (appended) {
	    shapes.emplace_back ();
	    auto& added = shapes.back ();
	    added.id = m_nextShapeId++;
	    added.geometryMaterial = replacement.material;
	    added.geometryVertexFormat = replacement.vertexFormat;
	    glGenBuffers (1, &added.vertexBuffer);
	    mesh.submeshes.emplace_back ();
	    mesh.submeshes.back ().materialPath = replacement.material;
	    ++structureRevision;
	}
	auto& shape = shapes[index];
	const bool layoutChanged = replacement.vertexFormat != 0 && replacement.vertexFormat != shape.input.vertexFormat;
	const size_t originalIndexCount = shape.input.indexWidth == 0 ? 0 : shape.input.indices.size () / shape.input.indexWidth;
	const bool recreate = appended || layoutChanged || (replacement.deleteIndices && shape.input.indexWidth != 0)
	    || (replacement.hasVertices && (!shape.input.vertexDynamic || replacement.vertexFloatCount > shape.input.vertices.size ()))
	    || (replacement.indexWidth != 0 && (!shape.input.indexDynamic || replacement.indexCount > originalIndexCount));
	if (!recreate) {
	    if (!replacement.material.empty () && replacement.material != shape.input.material) {
		shape.input.material = std::move (replacement.material);
		++materialRevision;
	    }
	    // Native maps only supplied buffers and retains original flags. Its vertex
	    // write/count change survives a subsequently rejected index buffer type.
	    if (replacement.hasVertices) {
		glBindBuffer (GL_ARRAY_BUFFER, shape.vertexBuffer);
		glBufferSubData (GL_ARRAY_BUFFER, 0, replacement.vertices.size () * sizeof (float), replacement.vertices.data ());
		const size_t count = shape.input.indexWidth == 0
		    ? replacement.vertexFloatCount / shape.input.strideBytes : replacement.indexCount;
		if (count > 0) {
		    shape.drawCount = static_cast<GLsizei> (count);
		}
		shape.input.vertices = std::move (replacement.vertices);
	    }
	    if (replacement.indexWidth != 0) {
		if (replacement.indexWidth != shape.input.indexWidth) {
		    glBindBuffer (GL_ARRAY_BUFFER, static_cast<GLuint> (previous));
		    return "Incorrect index buffer type";
		}
		glBindBuffer (GL_ARRAY_BUFFER, shape.indexBuffer);
		glBufferSubData (GL_ARRAY_BUFFER, 0, replacement.indices.size (), replacement.indices.data ());
		shape.drawCount = static_cast<GLsizei> (replacement.indexCount);
		shape.input.indices = std::move (replacement.indices);
	    }
	    continue;
	}
	// Parsing may have captured dynamic capacities before a property getter
	// changed the original flags. Recreated buffers use only their logical views.
	const bool suppliedMaterial = !replacement.material.empty ();
	if (!suppliedMaterial) replacement.material = shape.input.material;
	if (replacement.material != shape.input.material) ++materialRevision;
	replacement.vertices.resize (replacement.vertexFloatCount);
	replacement.indices.resize (replacement.indexCount * replacement.indexWidth);
	shape.input = std::move (replacement);
	if (layoutChanged) {
	    updateLayout (shape);
	    mesh.submeshes[index].vertexTag = shape.layout.vertexTag;
	    mesh.submeshes[index].strideBytes = shape.layout.strideBytes;
	    if (index == 0) {
		mesh.vertexTag = shape.layout.vertexTag;
		mesh.strideBytes = shape.layout.strideBytes;
		mesh.positionOffset = shape.layout.positionOffset;
		mesh.normalOffset = shape.layout.normalOffset;
		mesh.tangentOffset = shape.layout.tangentOffset;
		mesh.uvOffset = shape.layout.uvOffset;
	    }
	}
	// Keep the names referenced by every live color/shadow VAO while replacing
	// their storage. Static buffers, capacity growth or layout changes recreate both complete buffers.
	glBindBuffer (GL_ARRAY_BUFFER, shape.vertexBuffer);
	glBufferData (GL_ARRAY_BUFFER, shape.input.vertices.size () * sizeof (float), shape.input.vertices.data (),
	    shape.input.vertexDynamic ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW);
	if (shape.input.indexWidth != 0) {
	    if (shape.indexBuffer == GL_NONE) {
		glGenBuffers (1, &shape.indexBuffer);
	    }
	    glBindBuffer (GL_ARRAY_BUFFER, shape.indexBuffer);
	    glBufferData (GL_ARRAY_BUFFER, shape.input.indices.size (), shape.input.indices.data (),
		shape.input.indexDynamic ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW);
	} else if (shape.indexBuffer != GL_NONE) {
	    glDeleteBuffers (1, &shape.indexBuffer);
	    shape.indexBuffer = GL_NONE;
	}
	shape.drawCount = static_cast<GLsizei> (shape.input.indexWidth == 0
	    ? shape.input.vertices.size () * sizeof (float) / shape.input.strideBytes
	    : shape.input.indices.size () / shape.input.indexWidth);
	// Native publishes complete geometry/material before the compiled signature
	// gate; its failure stops later shapes while retaining this changed prefix.
	if (suppliedMaterial && validateMaterial) {
	    if (const char* error = validateMaterial (shape.input)) {
		glBindBuffer (GL_ARRAY_BUFFER, static_cast<GLuint> (previous));
		return error;
	    }
	}
    }
    // Native null markers refer to the original input indices. Compact only
    // after the entire replacement traversal succeeds; errors retain the prefix.
    size_t removed = 0;
    for (size_t index = 0; index < replacements.size (); ++index) {
        if (!replacements[index].deleteShape) continue;
        const auto current = index - removed++;
        if (static_cast<GLuint> (previous) == shapes[current].vertexBuffer
            || static_cast<GLuint> (previous) == shapes[current].indexBuffer) previous = 0;
        glDeleteBuffers (1, &shapes[current].vertexBuffer);
        glDeleteBuffers (1, &shapes[current].indexBuffer);
        shapes.erase (shapes.begin () + static_cast<std::ptrdiff_t> (current));
        mesh.submeshes.erase (mesh.submeshes.begin () + static_cast<std::ptrdiff_t> (current));
        ++structureRevision;
        ++materialRevision;
    }
    if (removed != 0 && !shapes.empty ()) {
        const auto& first = shapes.front ().layout;
        mesh.vertexTag = first.vertexTag;
        mesh.strideBytes = first.strideBytes;
        mesh.positionOffset = first.positionOffset;
        mesh.normalOffset = first.normalOffset;
        mesh.tangentOffset = first.tangentOffset;
        mesh.uvOffset = first.uvOffset;
    }
    glBindBuffer (GL_ARRAY_BUFFER, static_cast<GLuint> (previous));
    return nullptr;
}

ModelData::~ModelData () {
    for (const auto& shape : shapes) {
	glDeleteBuffers (1, &shape.vertexBuffer);
	glDeleteBuffers (1, &shape.indexBuffer);
    }
}

CModel::CModel (Wallpapers::CScene& scene, const Model3D& model) :
    CObject (scene, model), CRenderable (scene, model, **model.materials.begin ()), ScriptableObject (scene, model),
    m_model (model), m_modelData (scene.findModelData (model.modelDataToken)),
    m_size (model.mesh.hasBoundingBox ? model.mesh.boundingBoxMax - model.mesh.boundingBoxMin : glm::vec3 (0.0f)) {
    // Some older Workshop scenes bind the editor's 2D pivot script to a 3D model.
    // Expose its authored bounds as a vector-like size so those published scripts can
    // keep calculating their pivot even though current IModelLayer docs omit the field.
    this->registerProperty ("size", this->m_size);
    this->registerProperty ("castshadow", *model.castShadow->value);
    this->registerProperty ("perspective", *model.perspective->value);
    if (m_modelData) {
        m_materialRevision = m_modelData->materialRevision;
        m_runtimeMaterials.resize (model.materials.size ());
        for (size_t index = 0; index < model.materials.size (); ++index) {
            m_materialShapeIds.push_back (m_modelData->shapes[index].id);
            m_materials.push_back (model.materials[index].get ());
        }
    }
    for (size_t index = 0; index < model.materials.size (); ++index) {
        registerMaterialProperties (m_modelData ? m_materialShapeIds[index] : index, *model.materials[index]);
    }
    const auto addLayer = [&] (const ImageAnimationLayer* settings, const MdlAnimationClip* clip) {
	auto layer = std::make_unique<AnimationLayer> ();
	layer->settings = settings;
	layer->clip = clip;
	layer->key = "__model_animation_" + std::to_string (m_nextAnimationLayer++);
	layer->timeline.name = settings != nullptr ? settings->name : clip->name;
	if (clip != nullptr) {
	    layer->timeline.fps = clip->fps;
	    layer->timeline.length = static_cast<float> (clip->frameCount);
	    layer->timeline.mode = clip->mode.empty () ? "loop" : clip->mode;
	    layer->timeline.events = clip->events;
	}
	if (settings != nullptr) layer->timeline.rate = settings->rate->value->getFloat ();
	registerAnimation (layer->key, layer->timeline);
	m_animationLayers.push_back (std::move (layer));
    };
    for (const auto& settings : model.animationLayers) {
	const auto found = std::find_if (model.animationData.animations.begin (), model.animationData.animations.end (),
	    [&] (const MdlAnimationClip& clip) {
		return clip.id == static_cast<uint32_t> (settings->animation->value->getInt ());
	    });
	addLayer (settings.get (), found == model.animationData.animations.end () ? nullptr : &*found);
	const auto prefix = "animation_" + std::to_string (settings->id) + "_";
	for (const auto& [name, value] : std::initializer_list<std::pair<const char*, const UserSetting*>> {
	    { "rate", settings->rate.get () }, { "blend", settings->blend.get () },
	    { "visible", settings->visible.get () }, { "animation", settings->animation.get () },
	    { "additive", settings->additive.get () }, { "blendin", settings->blendIn.get () },
	    { "blendout", settings->blendOut.get () }, { "blendtime", settings->blendTime.get () } }) {
	    registerProperty (prefix + name, *value);
	    if (value->animation != nullptr) registerAnimation (prefix + name, *value->animation);
	}
    }
}

CModel::~CModel () {
    for (size_t index = 0; index < m_runtimeMaterials.size (); ++index) {
        if (m_runtimeMaterials[index]) unregisterMaterialProperties (m_materialShapeIds[index], *m_runtimeMaterials[index]);
    }

    for (const auto& pass : this->m_passes) {
	delete pass;
    }

    for (const auto& submesh : this->m_submeshes) {
	if (!m_modelData) {
	    glDeleteBuffers (1, &submesh.vertexBuffer);
	    glDeleteBuffers (1, &submesh.indexBuffer);
	}
	if (submesh.shadowVao != GL_NONE) glDeleteVertexArrays (1, &submesh.shadowVao);
    }
}

void CModel::setup () {
    if (this->m_initialized) {
	return;
    }

    this->detectTexture ();

    if (this->m_texture == nullptr) {
	// keep the pass pipeline happy; the shader's albedo defaults handle the rest
	this->m_texture = std::make_shared<CFBO> ("", TextureFormat_ARGB8888, TextureFlags_NoFlags, 1, 1, 1, 1, 1);
    }

    CRenderable::setup ();

    if (m_modelData) {
        // Initial material init hooks can change the resource before setup.
        // Build from the current shapes through the same owned staging path.
        m_initialized = true;
        --m_materialRevision;
        syncModelDataMaterials ();
        if (getScene ().getLights ().shadowViewCount > 0) {
            setupShadowProgram ();
            m_shadowBounds = ModelBounds (m_modelData->mesh, 0);
        }
        return;
    }

    this->m_skinningEnabled = this->m_model.mesh.skinned && !this->m_model.animationData.bones.empty ();
    this->m_morphingEnabled = !m_modelData && !m_model.animationData.bones.empty ()
        && std::ranges::any_of (m_model.mesh.submeshes, [] (const auto& body) { return !body.morphTargets.empty (); });

    if (!this->m_model.animationData.bones.empty ()) {
	this->updateAnimationPose ();
	sLog.out (
	    "Loaded 3D model animation data ", this->m_model.filename,
	    " bones=", this->m_model.animationData.bones.size (),
	    " attachments=", this->m_model.animationData.attachments.size (),
	    " clips=", this->m_model.animationData.animations.size (),
	    " skinning=", this->m_skinningEnabled ? "gpu" : "none"
	);
    }

    for (size_t submeshIndex = 0; submeshIndex < this->m_model.mesh.submeshes.size (); submeshIndex++) {
	const auto& submesh = this->m_model.mesh.submeshes[submeshIndex];
	SubmeshBuffers buffers {};

	if (m_modelData) {
	    const auto& shape = m_modelData->shapes[submeshIndex];
	    buffers.vertexBuffer = shape.vertexBuffer;
	    buffers.indexBuffer = shape.indexBuffer;
	    buffers.vertexCount
		= static_cast<GLsizei> (shape.input.vertices.size () * sizeof (float) / shape.input.strideBytes);
	    buffers.indexType = shape.input.indexWidth == 2 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT;
	    buffers.indexCount = shape.input.indexWidth == 0
		? 0
		: static_cast<GLsizei> (shape.input.indices.size () / shape.input.indexWidth);
	} else {
	    glGenBuffers (1, &buffers.vertexBuffer);
	    glBindBuffer (GL_ARRAY_BUFFER, buffers.vertexBuffer);
	    glBufferData (
		GL_ARRAY_BUFFER, submesh.vertices.size () * sizeof (float), submesh.vertices.data (), GL_STATIC_DRAW
	    );

	    glGenBuffers (1, &buffers.indexBuffer);
	    glBindBuffer (GL_ELEMENT_ARRAY_BUFFER, buffers.indexBuffer);
	    glBufferData (
		GL_ELEMENT_ARRAY_BUFFER, submesh.indices.size () * sizeof (uint32_t), submesh.indices.data (),
		GL_STATIC_DRAW
	    );

	    // a container can pair vertex layouts (puppet models add a channelmap overlay next to
	    // the skinned body); the attribute setup below is built for the mesh-level tag, so a
	    // submesh that disagrees would read its vertices through the wrong stride
	    buffers.indexCount = submesh.vertexTag == this->m_model.mesh.vertexTag
		? static_cast<GLsizei> (submesh.indices.size ())
		: 0;
	}
        this->m_submeshes.push_back (std::move (buffers));
    }
    std::vector<const Material*> materials;
    for (const auto& material : m_model.materials) materials.push_back (material.get ());
    setupMaterialPasses (materials);

    if (this->getScene ().getLights ().shadowViewCount > 0) {
	this->setupShadowProgram ();
	this->m_shadowBounds = ModelBounds (this->m_model.mesh, this->m_skinningEnabled ? this->m_gpuSkinBones.size () : 0);
    }

    this->m_initialized = true;
}

void CModel::setupMaterialPasses (const std::vector<const Material*>& materials) {
    m_renderSortClasses.clear ();
    for (size_t submeshIndex = 0; submeshIndex < materials.size (); ++submeshIndex) {
	const auto& material = materials[submeshIndex];
	const auto opaquePass = std::find_if (material->passes.begin (), material->passes.end (), [] (const auto& pass) {
	    return pass->blending == BlendingMode_Normal
		|| (pass->blending == BlendingMode_AlphaToCoverage
		    && (pass->shader == "generic4" || pass->shader == "foliage4"));
	});
	auto& caster = m_submeshes[submeshIndex].shadowCaster;
	caster = {};

	for (const auto& cur : materials[submeshIndex]->passes) {
	    ComboMap runtimeCombos;
            if (m_morphingEnabled && supportsModelMorphShader (cur->shader)) {
                // Position/normal morphs are already applied to the shared color/shadow VBO.
                runtimeCombos.insert_or_assign ("MORPHING", 0);
            }
	    if (this->m_skinningEnabled) {
		runtimeCombos.insert_or_assign ("SKINNING", 1);
		runtimeCombos.insert_or_assign ("BONECOUNT", static_cast<int> (this->m_gpuSkinBones.size ()));
	    }

	    auto pass = std::make_unique<Effects::CPass> (
		*this, std::make_shared<FBOProvider> (this), *cur, std::nullopt, std::nullopt, std::nullopt,
		std::move (runtimeCombos), true
	    );
	    pass->initialize ();

	    pass->setDestination (this->getScene ().getFBO ());
	    pass->setInput (this->m_texture);
	    pass->setModelViewProjectionMatrix (&this->m_modelViewProjectionMatrix, getScene ().getCamera ().usesReverseDepth ());
	    pass->setModelViewProjectionMatrixInverse (&this->m_modelViewProjectionMatrixInverse);
	    pass->setModelMatrix (&this->m_modelMatrix);
	    pass->setViewProjectionMatrix (&this->m_viewProjectionMatrix);
	    pass->addUniform ("g_NormalModelMatrix", &this->m_normalMatrix);
	    pass->addUniform ("g_EyePosition", &this->m_eyePosition);
	    if (this->m_skinningEnabled) {
		pass->addUniform (
		    "g_Bones", this->m_gpuSkinBones.data (), static_cast<int> (this->m_gpuSkinBones.size ())
		);
	    }
	    this->setupGeometryCallback (pass.get (), submeshIndex);
	    if (opaquePass != material->passes.end () && cur.get () == opaquePass->get ()) {
		caster.pass = pass.get ();
		if (cur->shader == "foliage4" && getScene ().getLights ().shadowViewCount > 0) {
		    caster.leavesUVMode = foliageLeavesUVMode (*pass);
		    if (caster.leavesUVMode >= 0) setupShadowProgram (caster.leavesUVMode);
		}
	    }

	    this->m_passes.push_back (pass.release ());
	    this->getScene ().loadingCheckpoint ();
	}
    }
    std::vector<BlendingMode> passModes;
    passModes.reserve (this->m_passes.size ());
    for (const auto* pass : this->m_passes) {
	passModes.push_back (pass->getBlendingMode ());
    }

    const std::vector<size_t> permutation = calculatePassRenderPermutation (passModes);
    std::vector<Effects::CPass*> orderedPasses;
    orderedPasses.reserve (this->m_passes.size ());
    for (const size_t index : permutation) {
	orderedPasses.push_back (this->m_passes[index]);
    }
    this->m_passes = std::move (orderedPasses);
    for (const auto* pass : this->m_passes) {
	const RenderSortClass renderClass = passRenderClass (pass->getBlendingMode ());
	if (std::ranges::find (this->m_renderSortClasses, renderClass) == this->m_renderSortClasses.end ()) {
	    this->m_renderSortClasses.push_back (renderClass);
	}
    }

}

void CModel::registerMaterialProperties (size_t index, const Material& material, bool deferInitialization) {
    for (size_t passIndex = 0; passIndex < material.passes.size (); ++passIndex) {
        const auto scope = "material_" + std::to_string (index) + "_pass_" + std::to_string (passIndex) + "_";
        for (const auto& [name, setting] : material.passes[passIndex]->constants) {
            if (setting->animation != nullptr || setting->value->getScriptSource ().has_value ()) {
                registerProperty (scope + name, *setting, scope, deferInitialization);
            }
        }
    }
}

void CModel::unregisterMaterialProperties (size_t index, const Material& material) {
    for (size_t passIndex = 0; passIndex < material.passes.size (); ++passIndex) {
        const auto scope = "material_" + std::to_string (index) + "_pass_" + std::to_string (passIndex) + "_";
        for (const auto& [name, setting] : material.passes[passIndex]->constants) {
            if (setting->animation != nullptr || setting->value->getScriptSource ().has_value ()) {
                unregisterProperty (scope + name);
            }
        }
    }
}

void CModel::syncModelDataMaterials () {
    if (!m_modelData || !m_initialized || m_materialRevision == m_modelData->materialRevision
        || getScene ().getScriptEngine ().getRunningModule () != nullptr) return;
    // Synchronize after hooks, including hidden consumers, before shadow/sort.
    // New material init hooks may publish another revision for the next frame.
    m_materialRevision = m_modelData->materialRevision;
    const auto structureRevision = m_modelData->structureRevision;
    const bool structureChanged = m_structureRevision != structureRevision;
    std::vector<uint64_t> shapeIds;
    std::vector<size_t> previousIndices;
    std::vector<MaterialUniquePtr> replacements (m_modelData->shapes.size ());
    std::vector<const Material*> materials;
    try {
        for (size_t index = 0; index < m_modelData->shapes.size (); ++index) {
            const auto& shape = m_modelData->shapes[index];
            const auto found = std::ranges::find (m_materialShapeIds, shape.id);
            const auto previous = static_cast<size_t> (found - m_materialShapeIds.begin ());
            const auto* current = found == m_materialShapeIds.end () ? nullptr : m_materials[previous];
            if (!current || shape.input.material != current->filename) {
                replacements[index] = Data::Parsers::MaterialParser::load (getScene ().getScene ().project, shape.input.material, true);
                if (replacements[index]->passes.empty ()) throw std::runtime_error ("Model material has no passes");
            }
            shapeIds.push_back (shape.id);
            previousIndices.push_back (previous);
            materials.push_back (replacements[index] ? replacements[index].get () : current);
        }
    } catch (const std::exception& error) {
        sLog.error ("Model material replacement failed: ", error.what ());
        return;
    }
    if (!structureChanged && std::ranges::none_of (replacements, [] (const auto& material) { return material != nullptr; })) return;
    const auto previousTexture = m_texture;
    const auto previousAnimationTime = m_animationTime;
    auto previousClasses = std::move (m_renderSortClasses);
    auto previousPasses = std::move (m_passes);
    std::vector<SubmeshBuffers> previousSubmeshes;
    std::vector<ShadowCaster> previousShadow;
    if (structureChanged) {
        previousSubmeshes = std::move (m_submeshes);
        m_submeshes.resize (materials.size ());
        for (size_t index = 0; index < materials.size (); ++index) {
            auto& buffers = m_submeshes[index];
            const auto& shape = m_modelData->shapes[index];
            buffers.vertexBuffer = shape.vertexBuffer;
            buffers.indexBuffer = shape.indexBuffer;
            buffers.vertexCount = static_cast<GLsizei> (shape.input.vertices.size () * sizeof (float) / shape.input.strideBytes);
            buffers.indexType = shape.input.indexWidth == 2 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT;
            buffers.indexCount = shape.input.indexWidth == 0 ? 0 : static_cast<GLsizei> (shape.input.indices.size () / shape.input.indexWidth);
        }
    } else {
        for (const auto& buffers : m_submeshes)
            previousShadow.push_back (buffers.shadowCaster);
    }
    try {
        if (materials.empty () || m_materials.empty () || materials[0] != m_materials[0]) {
            m_texture.reset ();
            if (!materials.empty ()) detectTexture (*materials[0]);
            if (!m_texture) m_texture = std::make_shared<CFBO> ("", TextureFormat_ARGB8888, TextureFlags_NoFlags, 1, 1, 1, 1, 1);
            m_animationTime = 0.0;
            for (const auto& frame : m_texture->getFrames ()) m_animationTime += frame->frametime;
        }
        setupMaterialPasses (materials);
    } catch (const std::exception& error) {
        for (auto* pass : m_passes) delete pass;
        m_passes = std::move (previousPasses);
        m_renderSortClasses = std::move (previousClasses);
        if (structureChanged) {
            m_submeshes = std::move (previousSubmeshes);
        } else {
            for (size_t index = 0; index < m_submeshes.size (); ++index) {
                m_submeshes[index].shadowCaster = previousShadow[index];
            }
        }
        m_texture = previousTexture;
        m_animationTime = previousAnimationTime;
        sLog.error ("Model material replacement failed: ", error.what ());
        return;
    }
    // Stable resource identities preserve survivor property keys through index
    // compaction. Passes retire before any referenced material owner is released.
    for (auto* pass : previousPasses) delete pass;
    for (const auto& buffers : previousSubmeshes) {
        if (buffers.shadowVao != GL_NONE) glDeleteVertexArrays (1, &buffers.shadowVao);
    }
    for (size_t previous = 0; previous < m_materialShapeIds.size (); ++previous) {
        const auto found = std::ranges::find (shapeIds, m_materialShapeIds[previous]);
        if (found == shapeIds.end () || materials[found - shapeIds.begin ()] != m_materials[previous]) {
            unregisterMaterialProperties (m_materialShapeIds[previous], *m_materials[previous]);
        }
    }
    std::vector<MaterialUniquePtr> owners (materials.size ());
    std::vector<size_t> registrations;
    for (size_t index = 0; index < materials.size (); ++index) {
        if (replacements[index]) {
            owners[index] = std::move (replacements[index]);
            registrations.push_back (index);
        } else {
            owners[index] = std::move (m_runtimeMaterials[previousIndices[index]]);
        }
    }
    m_runtimeMaterials = std::move (owners);
    m_materials = std::move (materials);
    m_materialShapeIds = std::move (shapeIds);
    m_structureRevision = structureRevision;
    for (const auto index : registrations) registerMaterialProperties (m_materialShapeIds[index], *m_materials[index], true);
    getScene ().getScriptEngine ().initializeQueuedScripts (this);
}

std::vector<size_t> CModel::calculatePassRenderPermutation (const std::vector<BlendingMode>& modes) {
    std::vector<size_t> permutation (modes.size ());
    std::iota (permutation.begin (), permutation.end (), 0);
    // Native model sorting puts opaque/coverage surfaces first, but treats
    // translucent and additive surfaces as peers. The glass over Sly's roof
    // lamps must therefore remain after the earlier additive lamp surface.
    std::stable_sort (permutation.begin (), permutation.end (), [&modes] (const size_t left, const size_t right) {
	return (passRenderClass (modes[left]) != RenderSortClass::Opaque)
	    < (passRenderClass (modes[right]) != RenderSortClass::Opaque);
    });
    return permutation;
}

void CModel::setupShadowProgram (const int leavesUVMode) {
    if (m_shadowPrograms.contains (leavesUVMode)) return;
    std::ostringstream vertex;
    vertex << "#version 330 core\n"
	      "layout(location = 0) in vec3 a_Position;\n"
	      "layout(location = 3) in vec2 a_TexCoord;\n"
	      "out vec2 v_TexCoord;\n";
    if (this->m_skinningEnabled) {
	vertex << "layout(location = 1) in uvec4 a_BlendIndices;\n"
		  "layout(location = 2) in vec4 a_BlendWeights;\n"
		  "uniform mat4x3 u_Bones[" << this->m_gpuSkinBones.size () << "];\n";
    }
    if (leavesUVMode >= 0) {
	vertex << "#define LEAVESUVMODE " << leavesUVMode << "\n"
		  "#define CAST4 vec4\n"
		  "#define saturate(value) clamp(value, 0.0, 1.0)\n"
	       << getAssetLocator ().includeShader ("common_foliage.h") << "\n"
		  "uniform float g_Time;\n";
	for (size_t index = 0; index < foliageShadowUniforms.size (); ++index) {
	    vertex << (index == foliageShadowUniforms.size () - 1 ? "uniform vec2 " : "uniform float ")
		   << foliageShadowUniforms[index].first << ";\n";
	}
    }
    vertex << "uniform mat4 u_LightViewProjection;\n"
	      "uniform mat4 u_Model;\n"
	      "void main() {\n"
	      "    v_TexCoord = a_TexCoord;\n"
	      "    vec3 localPosition = a_Position;\n";
    if (this->m_skinningEnabled) {
	vertex << "    mat4x3 skin = u_Bones[a_BlendIndices.x] * a_BlendWeights.x\n"
		  "        + u_Bones[a_BlendIndices.y] * a_BlendWeights.y\n"
		  "        + u_Bones[a_BlendIndices.z] * a_BlendWeights.z\n"
		  "        + u_Bones[a_BlendIndices.w] * a_BlendWeights.w;\n"
		  "    localPosition = skin * vec4(localPosition, 1.0);\n";
    }
    vertex << "    vec4 worldPosition = u_Model * vec4(localPosition, 1.0);\n";
    if (leavesUVMode >= 0) {
	// The stock caster deforms world space after skinning, retaining the
	// original local position for the height/radius weights.
	vertex << "    worldPosition.xyz += CalcFoliageAnimation(worldPosition.xyz, a_Position, a_TexCoord,\n"
		  "        g_Direction, g_Time, g_SpeedLeaves, g_SpeedBase, g_StrengthLeaves, g_StrengthBase,\n"
		  "        g_Phase, g_FoliageScale, g_CutOff, g_TreeHeight, g_TreeRadius, g_FoliageUVBounds);\n";
    }
    vertex << "    gl_Position = u_LightViewProjection * worldPosition;\n}\n";

    static const std::string fragment = "#version 330 core\n"
	"uniform bool u_AlphaToCoverage;\n"
	"uniform sampler2D u_Albedo;\n"
	"in vec2 v_TexCoord;\n"
	"void main() {\n"
	"    if (u_AlphaToCoverage) {\n"
	"        float alpha = texture(u_Albedo, v_TexCoord).a;\n"
	"        float coverage = (alpha - 0.5) / max(fwidth(alpha), 0.0001) + 0.5;\n"
	"        if (coverage < 0.5) discard;\n"
	"    }\n"
	"}\n";
    std::shared_ptr<Shaders::ShaderProgramCache::SharingGroup> group;

    const auto interface = this->getScene ().getContext ().getShaderProgramCache ().interfaceProgram (
	vertex.str (), fragment, group
    );
    ShadowProgram program;
    program.interface = interface;
    program.program = Shaders::ShaderProgramCache::shareProgram (interface->id, group, "model-shadow");
    if (!program.program) program.program = interface;
    program.lightViewProjection = glGetUniformLocation (interface->id, "u_LightViewProjection");
    program.model = glGetUniformLocation (interface->id, "u_Model");
    program.bones = glGetUniformLocation (interface->id, "u_Bones");
    program.albedo = glGetUniformLocation (interface->id, "u_Albedo");
    program.alphaToCoverage = glGetUniformLocation (interface->id, "u_AlphaToCoverage");
    program.time = glGetUniformLocation (interface->id, "g_Time");
    for (size_t index = 0; index < foliageShadowUniforms.size (); ++index) {
	program.foliageUniforms[index] = glGetUniformLocation (interface->id, foliageShadowUniforms[index].first);
    }
    m_shadowPrograms.emplace (leavesUVMode, std::move (program));
}

void CModel::setupGeometryCallback (Effects::CPass* pass, size_t submeshIndex) {
    pass->setGeometryCallback (
	[this, pass, submeshIndex, revision = uint64_t (0), vertexTag = uint32_t (0),
	 locations = std::array<GLint, 10> { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 }] () mutable {
	    if (m_modelData && m_structureRevision != m_modelData->structureRevision) return;
	    const auto& mesh = m_modelData ? m_modelData->shapes[submeshIndex].layout : m_model.mesh;
	    if (revision == pass->getShaderRevision () && vertexTag == mesh.vertexTag) {
		return;
	    }
	    revision = pass->getShaderRevision ();
	    vertexTag = mesh.vertexTag;
	    const auto stride = static_cast<GLsizei> (mesh.strideBytes);
	    // Every retained pass refreshes after a shader reload or shared layout change.
	    for (const auto location : locations) {
		if (location >= 0) {
		    glDisableVertexAttribArray (location);
		}
	    }
	    const GLint position = glGetAttribLocation (pass->getProgramID (), "a_Position");
        const GLint positionVec4 = m_modelData ? glGetAttribLocation (pass->getProgramID (), "a_PositionVec4") : -1;
	    const GLint normal = glGetAttribLocation (pass->getProgramID (), "a_Normal");
	    const GLint tangent = glGetAttribLocation (pass->getProgramID (), "a_Tangent4");
	    const GLint texCoord = glGetAttribLocation (pass->getProgramID (), "a_TexCoord");
        const GLint texCoordVec3 = m_modelData ? glGetAttribLocation (pass->getProgramID (), "a_TexCoordVec3") : -1;
        const GLint texCoordVec4 = m_modelData ? glGetAttribLocation (pass->getProgramID (), "a_TexCoordVec4") : -1;
	    const GLint blendIndices = glGetAttribLocation (pass->getProgramID (), "a_BlendIndices");
	    const GLint blendWeights = glGetAttribLocation (pass->getProgramID (), "a_BlendWeights");

	    const GLint color = glGetAttribLocation (pass->getProgramID (), "a_Color");
	    locations = { position, normal, tangent, texCoord, blendIndices, blendWeights, color, positionVec4, texCoordVec3, texCoordVec4 };
	    glBindBuffer (GL_ELEMENT_ARRAY_BUFFER, m_modelData ? m_modelData->shapes[submeshIndex].indexBuffer
		: this->m_submeshes[submeshIndex].indexBuffer);
	    glBindBuffer (GL_ARRAY_BUFFER, this->m_submeshes[submeshIndex].vertexBuffer);

	    // Script resources store POSITION3/UV2; GL supplies missing components.
	    for (const auto location : { position, positionVec4 }) {
		if (location >= 0 && mesh.positionOffset != MdlMesh::AttributeAbsent) {
		    glEnableVertexAttribArray (location);
		    glVertexAttribPointer (
			location, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*> (mesh.positionOffset)
		    );
		}
	    }

	    // a shader can ask for an attribute the vertex tag left out; leaving the array
	    // disabled feeds the generic default instead of reinterpreting another attribute
	    if (normal >= 0 && mesh.normalOffset != MdlMesh::AttributeAbsent) {
		glEnableVertexAttribArray (normal);
		glVertexAttribPointer (
		    normal, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*> (mesh.normalOffset)
		);
	    }

	    if (tangent >= 0 && mesh.tangentOffset != MdlMesh::AttributeAbsent) {
		glEnableVertexAttribArray (tangent);
		glVertexAttribPointer (
		    tangent, 4, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*> (mesh.tangentOffset)
		);
	    }

	    for (const auto location : { texCoord, texCoordVec3, texCoordVec4 }) {
		if (location >= 0 && mesh.uvOffset != MdlMesh::AttributeAbsent) {
		    glEnableVertexAttribArray (location);
		    glVertexAttribPointer (
			location, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*> (mesh.uvOffset)
		    );
		}
	    }

	    if (color >= 0 && m_modelData
		&& m_modelData->shapes[submeshIndex].colorOffset != MdlMesh::AttributeAbsent) {
		glEnableVertexAttribArray (color);
		glVertexAttribPointer (
		    color, 4, GL_FLOAT, GL_FALSE, stride,
		    reinterpret_cast<void*> (m_modelData->shapes[submeshIndex].colorOffset)
		);
	    }

	    if (this->m_skinningEnabled && blendIndices >= 0) {
		glEnableVertexAttribArray (blendIndices);
		glVertexAttribIPointer (
		    blendIndices, 4, GL_UNSIGNED_INT, stride, reinterpret_cast<void*> (mesh.blendIndicesOffset)
		);
	    }

	    if (this->m_skinningEnabled && blendWeights >= 0) {
		glEnableVertexAttribArray (blendWeights);
		glVertexAttribPointer (
		    blendWeights, 4, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*> (mesh.blendWeightsOffset)
		);
	    }
	},
	[this, submeshIndex] () {
	    if (m_modelData && m_structureRevision != m_modelData->structureRevision) return;
	    // Compensate only the renderer's output/orthographic Y conversions.
	    // Authored negative scales can intentionally turn a skybox inside out.
	    const auto& camera = this->getScene ().getCamera ();
	    const bool windingReversed = camera.isYFlipped () != camera.isOrthogonal ();
	    glFrontFace (windingReversed ? GL_CW : GL_CCW);
	    const auto& buffers = m_submeshes[submeshIndex];
	    const auto* shape = m_modelData ? &m_modelData->shapes[submeshIndex] : nullptr;
	    glBindBuffer (GL_ELEMENT_ARRAY_BUFFER, shape ? shape->indexBuffer : buffers.indexBuffer);
	    if (shape && shape->input.indexWidth == 0) {
		glDrawArrays (GL_TRIANGLES, 0, shape->drawCount);
	    } else {
		glDrawElements (GL_TRIANGLES, shape ? shape->drawCount : buffers.indexCount,
		    shape ? (shape->input.indexWidth == 2 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT) : buffers.indexType, nullptr);
	    }
	    glFrontFace (GL_CCW);
	},
	[] () { }
    );
}

void CModel::syncAnimationLayers () const {
    const double time = getScene ().getElapsedTime ();
    for (const auto& layer : m_animationLayers) {
	if (layer->settings == nullptr) continue;
	const auto& settings = *layer->settings;
	const auto id = static_cast<uint32_t> (settings.animation->value->getInt ());
	if (layer->clip == nullptr || layer->clip->id != id) {
	    const auto& clips = m_model.animationData.animations;
	    const auto found = std::find_if (clips.begin (), clips.end (),
		[id] (const MdlAnimationClip& clip) { return clip.id == id; });
	    layer->clip = found == clips.end () ? nullptr : &*found;
	    if (layer->clip != nullptr) {
		layer->timeline.fps = layer->clip->fps;
		layer->timeline.length = static_cast<float> (layer->clip->frameCount);
		layer->timeline.mode = layer->clip->mode.empty () ? "loop" : layer->clip->mode;
		layer->timeline.events = layer->clip->events;
		layer->timeline.setFrame (0.0f, time);
	    } else {
		layer->timeline.events.clear ();
	    }
	    m_poseTime = std::numeric_limits<double>::quiet_NaN ();
	}
	const float rate = settings.rate->value->getFloat ();
	if (rate != layer->timeline.rate) layer->timeline.setRate (rate, time);
    }
}

void CModel::prepareAnimationEvents () {
    syncAnimationLayers ();
    const double time = getScene ().getElapsedTime ();
    // Keep a completed clip until its last event interval has been dispatched.
    // Retire only here, never from a callback while ScriptEngine iterates events.
    std::erase_if (m_animationLayers, [&] (const auto& layer) {
	if (!layer->single || layer->timeline.elapsedFrame (time) < layer->timeline.length
	    || layer->timeline.previousEventFrame < layer->timeline.length) return false;
	unregisterAnimation (layer->key);
	return true;
    });
}

CModel::AnimationLayer* CModel::findAnimationLayer (const std::string& key) {
    for (const auto& layer : m_animationLayers) if (layer->key == key) return layer.get ();
    return nullptr;
}

CModel::AnimationLayer* CModel::getAnimationLayer (const std::string& name) {
    for (const auto& layer : m_animationLayers) if (layer->timeline.name == name) return layer.get ();
    return nullptr;
}

CModel::AnimationLayer* CModel::getAnimationLayer (const size_t index) {
    return index < m_animationLayers.size () ? m_animationLayers[index].get () : nullptr;
}

size_t CModel::getAnimationLayerCount () const { return m_animationLayers.size (); }

CModel::AnimationLayer* CModel::playSingleAnimation (const std::string& name, const SingleAnimationConfig& config) {
    const auto& clips = m_model.animationData.animations;
    const auto found = std::find_if (clips.begin (), clips.end (),
	[&] (const MdlAnimationClip& clip) { return clip.name == name; });
    if (found == clips.end () || found->fps <= 0.0f || found->frameCount == 0) return nullptr;
    auto layer = std::make_unique<AnimationLayer> ();
    layer->clip = &*found;
    layer->single = true;
    layer->blendIn = config.blendIn;
    // Native one-shots of loop clips keep their authored return pose until
    // retirement; fading them early exposes a paused idle before its reset event.
    layer->blendOut = config.blendOut && found->mode == "single";
    layer->blendTime = std::max (config.blendTime, 0.0f);
    layer->key = "__model_animation_" + std::to_string (m_nextAnimationLayer++);
    layer->timeline.fps = found->fps;
    layer->timeline.length = static_cast<float> (found->frameCount);
    layer->timeline.mode = "single";
    layer->timeline.name = found->name;
    layer->timeline.events = found->events;
    layer->timeline.setFrame (0.0f, getScene ().getElapsedTime ());
    registerAnimation (layer->key, layer->timeline);
    auto* result = layer.get ();
    // Native one-shot layers blend over the base animation, before additive detail.
    const auto position = std::find_if (m_animationLayers.begin (), m_animationLayers.end (), [] (const auto& other) {
	return other->settings != nullptr && other->settings->additive->value->getBool ();
    });
    m_animationLayers.insert (position, std::move (layer));
    m_poseTime = std::numeric_limits<double>::quiet_NaN ();
    return result;
}

void CModel::animationControlsChanged (PropertyAnimation& animation) {
    for (const auto& layer : m_animationLayers) {
	if (&layer->timeline == &animation && layer->settings != nullptr
	    && layer->settings->rate->value->getFloat () != animation.rate) {
	    layer->settings->rate->value->update (animation.rate, DynamicValue::UpdateSource::User);
	}
    }
    m_poseTime = std::numeric_limits<double>::quiet_NaN ();
}

void CModel::updateAnimationPose (const bool stepPhysics) const {
    const auto& animationData = this->m_model.animationData;
    if (animationData.bones.empty ()) {
	return;
    }

    std::vector<MdlActiveAnimation> activeAnimations;
    const double sceneTime = this->getScene ().getElapsedTime ();
    syncAnimationLayers ();
    const bool hasPhysics = std::ranges::any_of (animationData.bones,
        [] (const auto& bone) { return bone.spring.has_value (); });
    const auto frame = this->getScene ().getSceneFrame ();
    const bool advancePhysics = hasPhysics && stepPhysics && m_physicsFrame != frame
        && this->m_model.groupVisible->value->getBool () && this->isVisibleThroughParents ();
    // Setup and lazy reads retain the current pose without consuming a physical update.
    if (hasPhysics && !advancePhysics && !this->m_worldBones.empty ()) return;
    if (sceneTime == this->m_poseTime && !advancePhysics) {
	return;
    }

    for (const auto& layer : m_animationLayers) {
	if (layer->clip == nullptr) continue;
	const auto* settings = layer->settings;
	const bool visible = settings != nullptr ? settings->visible->value->getBool () : layer->visible;
	float weight = visible ? std::clamp (settings != nullptr ? settings->blend->value->getFloat () : layer->blend,
	    0.0f, 1.0f) : 0.0f;
	if (layer->single) {
	    const float elapsed = layer->timeline.elapsedFrame (sceneTime) / layer->timeline.fps;
	    const float duration = layer->timeline.length / layer->timeline.fps;
	    if (elapsed >= duration) continue;
	    if (layer->blendTime > 0.0f) {
		if (layer->blendIn) weight *= std::clamp (elapsed / layer->blendTime, 0.0f, 1.0f);
		if (layer->blendOut) weight *= std::clamp ((duration - elapsed) / layer->blendTime, 0.0f, 1.0f);
	    }
        }
	activeAnimations.push_back ({ .animation = layer->clip, .weight = weight,
	    .additive = settings != nullptr && settings->additive->value->getBool (),
	    .frame = layer->timeline.frameAt (sceneTime), .enabled = visible });
    }

    m_animationBounds = evaluateMdlAnimationBounds (activeAnimations,
        { m_model.mesh.boundingBoxMin, m_model.mesh.boundingBoxMax });

    if (m_morphingEnabled) {
        auto morphs = MdlAnimationEvaluator::evaluateModelMorphs (activeAnimations);
        if (!std::equal (morphs.begin (), morphs.end (), m_modelMorphs.begin (), m_modelMorphs.end (),
            [] (const auto& next, const auto& previous) { return next.mask == previous.mask && next.weights == previous.weights; })) {
            m_modelMorphs = std::move (morphs);
            m_morphDirty = true;
        }
    }
    std::optional<MdlPhysicsStep> physics;
    if (advancePhysics) {
        physics.emplace (MdlPhysicsStep {
            .state = this->m_physicsState,
            .model = this->resolveWorldMatrix (),
            .deltaTime = this->getScene ().getDeltaTime (),
        });
    }
    auto pose = MdlAnimationEvaluator::evaluate (animationData, activeAnimations,
        physics ? &*physics : nullptr);
    if (advancePhysics) m_physicsFrame = frame;
    this->m_worldBones = std::move (pose.worldBones);
    this->m_skinBones = std::move (pose.skinBones);
    this->m_gpuSkinBones.resize (this->m_skinBones.size ());
    for (size_t bone = 0; bone < this->m_skinBones.size (); bone++) {
	this->m_gpuSkinBones[bone] = glm::mat4x3 (this->m_skinBones[bone]);
    }
    this->m_poseTime = sceneTime;
}

void CModel::updateMorphBuffers () {
    if (!m_morphDirty) return;
    m_morphDirty = false;
    m_morphActive = false;
    GLint previous = 0;
    glGetIntegerv (GL_ARRAY_BUFFER_BINDING, &previous);
    for (size_t index = 0; index < m_submeshes.size (); ++index) {
        auto& buffers = m_submeshes[index];
        const auto& body = m_model.mesh.submeshes[index];
        if (!std::ranges::all_of (m_model.materials[index]->passes,
            [] (const auto& pass) { return supportsModelMorphShader (pass->shader); })) continue;
        const bool wasMorphed = !buffers.morphVertices.empty ();
        const bool morphed = index < m_modelMorphs.size () && deformModelMorphVertices (
            m_model.mesh, body, m_modelMorphs[index].weights, m_modelMorphs[index].mask, buffers.morphVertices
        );
        m_morphActive |= morphed;
        if (!morphed && !wasMorphed) continue;
        const auto& vertices = morphed ? buffers.morphVertices : body.vertices;
        glBindBuffer (GL_ARRAY_BUFFER, buffers.vertexBuffer);
        glBufferSubData (GL_ARRAY_BUFFER, 0, vertices.size () * sizeof (float), vertices.data ());
        if (!morphed) buffers.morphVertices.clear ();
    }
    glBindBuffer (GL_ARRAY_BUFFER, static_cast<GLuint> (previous));
}

glm::mat4 CModel::resolveModelMatrix () const {
    glm::mat4 model = this->resolveWorldMatrix ();
    const auto& camera = this->getScene ().getCamera ();

    if (camera.isOrthogonal ()) {
	// Orthographic layer origins are authored from the canvas's top-left in
	// Y-down coordinates. The camera, however, is centered and Y-up. Image and
	// particle layers already perform this conversion; model layers need the
	// same basis change or the canvas center lands at the bottom-right and the
	// mesh is vertically mirrored.
	const glm::mat4 flipY = glm::scale (glm::mat4 (1.0f), glm::vec3 (1.0f, -1.0f, 1.0f));
	const glm::mat4 sceneToRender = glm::translate (
	    glm::mat4 (1.0f), glm::vec3 (-camera.getWidth () * 0.5f, camera.getHeight () * 0.5f, 0.0f)
	) * flipY;
	// The root offset is already in render coordinates. Prepend it so a
	// parent's rotation or scale does not change the scene-space drift.
	model = glm::translate (glm::mat4 (1.0f), glm::vec3 (this->resolveParallaxOffset (), 0.0f))
	    * sceneToRender * model;
    }

    return model;
}

void CModel::updateMatrices () {
    const auto& camera = this->getScene ().getCamera ();

    this->m_modelMatrix = this->resolveModelMatrix ();
    this->m_viewProjectionMatrix = camera.getRasterProjection () * camera.getLookAt ();
    const bool perspective = camera.isOrthogonal () && this->m_model.perspective->value->getBool ();
    if (perspective) {
	// Native derives the virtual eye from the current orthographic scale,
	// preserving the size of geometry on Z=0 while giving depth perspective.
	const float fov = glm::radians (glm::clamp (camera.getFov (), 0.1f, 179.9f));
	const float distance = 1.0f / (std::tan (fov * 0.5f) * camera.getProjection ()[1][1]);
	glm::mat4 view = camera.getLookAt ();
	view[3][2] = -distance;
	const auto& output = this->getScene ().getOutputSize ();
	const glm::mat4 projection = glm::perspective (fov, output.x / output.y, 5.0f,
	    glm::max (15000.0f, distance + 1000.0f));
	this->m_viewProjectionMatrix = projection * view;
	this->m_eyePosition = glm::vec3 (glm::inverse (view)[3]);
    }
    this->m_modelViewProjectionMatrix = this->m_viewProjectionMatrix * this->m_modelMatrix;
    this->m_modelViewProjectionMatrixInverse = glm::inverse (this->m_modelViewProjectionMatrix);
    this->m_normalMatrix = Render::Utils::calculateNormalModelMatrix (this->m_modelMatrix);
    if (!perspective && camera.isOrthogonal ()) {
	const float depth = glm::max (glm::abs (camera.getNearZ ()), glm::abs (camera.getFarZ ()));
	this->m_eyePosition = glm::vec3 (0.0f, 0.0f, depth);
    } else if (!perspective) {
	this->m_eyePosition = camera.getEye ();
    }
}

const std::vector<RenderSortClass>& CModel::getRenderSortClasses () const { return this->m_renderSortClasses; }

void CModel::render () { this->renderPasses (std::nullopt); }

void CModel::render (const RenderSortClass renderClass) { this->renderPasses (renderClass); }

void CModel::prepareRender () {
    syncModelDataMaterials ();
    // Physics advances here; other pose reads retain its scene-update result.
    this->updateAnimationPose (true);
    // Material sorting may prepare an attached model before its parent.
    const Object* current = &this->m_model;
    for (int depth = 0; current->parent.has_value () && depth <= 32; ++depth) {
        const auto* parent = this->getScene ().getObject (*current->parent);
        if (parent == nullptr) break;
        if (const auto* model = dynamic_cast<const CModel*> (parent)) model->updateAnimationPose ();
        current = &parent->getObject ();
    }
    // A hidden container hides its whole subtree.
    this->m_frameVisible = this->m_initialized && this->m_model.groupVisible->value->getBool ()
	&& this->isVisibleThroughParents ();
    if (!this->m_frameVisible) return;
    this->updateMatrices ();
    this->updateMorphBuffers ();
    const auto& mesh = m_modelData ? m_modelData->mesh : m_model.mesh;
    const auto bounds = m_modelData ? std::optional<MdlAnimationBounds> {} : m_animationBounds;
    const auto volume = bounds.value_or (MdlAnimationBounds { mesh.boundingBoxMin, mesh.boundingBoxMax });
    const auto& camera = getScene ().getCamera ();
    m_renderBounds = (bounds || mesh.hasBoundingBox) && volume.minimum.x < volume.maximum.x
        ? std::optional<MdlAnimationBounds> (volume) : std::nullopt;
    m_mainViewVisible = !m_renderBounds
        || modelAnimationVolumeIntersects (*m_renderBounds, m_modelMatrix, camera.getProjection () * camera.getLookAt ());
    this->m_posedShadowBounds = this->m_shadowBounds.evaluate (this->m_gpuSkinBones);
    // Rest-pose bounds cannot reject geometry displaced by an active morph.
    if (m_morphActive) m_posedShadowBounds = {};
    if (m_modelData && m_modelData->mesh.hasBoundingBox) {
	m_posedShadowBounds.minimum = m_modelData->mesh.boundingBoxMin;
	m_posedShadowBounds.maximum = m_modelData->mesh.boundingBoxMax;
    }
}

void CModel::renderPasses (const std::optional<RenderSortClass> renderClass) {
    if (!this->m_frameVisible || !m_mainViewVisible
        || (m_modelData && m_structureRevision != m_modelData->structureRevision)) return;

#if !NDEBUG
    const std::string debugName
	= "Model " + this->m_model.name + " (" + std::to_string (this->getId ()) + ", " + this->m_model.filename + ")";

    glPushDebugGroup (GL_DEBUG_SOURCE_APPLICATION, 0, -1, debugName.c_str ());
#endif /* DEBUG */

    // Scene alpha belongs to the compositor; isolated composition targets need
    // the model's authored alpha, just like image and particle children.
    glColorMask (true, true, true, this->getScene ().isRenderingToComposition () ? GL_TRUE : GL_FALSE);

    for (const auto& pass : this->m_passes) {
	if (!renderClass.has_value () || passRenderClass (pass->getBlendingMode ()) == *renderClass) {
	    pass->render ();
	}
    }

    glColorMask (true, true, true, true);

#if !NDEBUG
    glPopDebugGroup ();
#endif /* DEBUG */
}

void CModel::renderShadow (const glm::mat4& lightViewProjection) {
    if (!this->m_frameVisible || this->m_shadowPrograms.empty ()
        || (m_modelData && m_structureRevision != m_modelData->structureRevision)
	|| !this->m_model.castShadow->value->getBool ()) {
	return;
    }

    // Rest/animation bounds do not include shader-driven foliage displacement.
    const bool boundsIntersect = m_renderBounds
	? modelAnimationVolumeIntersects (*m_renderBounds, m_modelMatrix, lightViewProjection, getScene ().getCamera ().usesReverseDepth ())
	: m_posedShadowBounds.intersects (lightViewProjection * m_modelMatrix);
    for (size_t submeshIndex = 0; submeshIndex < this->m_submeshes.size (); submeshIndex++) {
	auto& buffers = this->m_submeshes[submeshIndex];
	const auto& caster = buffers.shadowCaster;
	const auto& layout = m_modelData ? m_modelData->shapes[submeshIndex].layout : m_model.mesh;
	const auto stride = static_cast<GLsizei> (layout.strideBytes);
	if (!caster.pass || (!boundsIntersect && caster.leavesUVMode < 0)
	    || (m_modelData ? m_modelData->shapes[submeshIndex].drawCount == 0 : buffers.indexCount == 0 && buffers.vertexCount == 0)
	    || layout.positionOffset == MdlMesh::AttributeAbsent) {
	    continue;
	}
	const auto& program = m_shadowPrograms.at (caster.leavesUVMode);
	glUseProgram (program.program->id);
	glUniformMatrix4fv (program.lightViewProjection, 1, GL_FALSE, glm::value_ptr (lightViewProjection));
	glUniformMatrix4fv (program.model, 1, GL_FALSE, glm::value_ptr (m_modelMatrix));
	glUniform1i (program.albedo, 0);
	if (m_skinningEnabled && program.bones >= 0 && !m_gpuSkinBones.empty ()) {
	    glUniformMatrix4x3fv (program.bones, static_cast<GLsizei> (m_gpuSkinBones.size ()), GL_FALSE,
		glm::value_ptr (m_gpuSkinBones.front ()));
	}
	if (caster.leavesUVMode >= 0) {
	    glUniform1f (program.time, getScene ().getTime ());
	    for (size_t index = 0; index < foliageShadowUniforms.size (); ++index) {
		const auto value = foliageShadowValue (*caster.pass,
		    foliageShadowUniforms[index].second, getScene ().getElapsedTime ());
		if (index == foliageShadowUniforms.size () - 1) {
		    glUniform2fv (program.foliageUniforms[index], 1, glm::value_ptr (value));
		} else {
		    glUniform1f (program.foliageUniforms[index], value.x);
		}
	    }
	}
	const bool alphaToCoverage = caster.pass->getBlendingMode () == BlendingMode_AlphaToCoverage;
	glUniform1i (program.alphaToCoverage, alphaToCoverage);
	if (alphaToCoverage && !caster.pass->bindAlbedoTexture ()) continue;
	// The atlas has one depth sample; cutout coverage is handled by the fragment discard.
	glDisable (GL_SAMPLE_ALPHA_TO_COVERAGE);
	// Native's ordinary shadow raster state disables culling, including normal materials.
	glDisable (GL_CULL_FACE);
	// Shadows have no output flip; retain authored winding changes here too.
	glFrontFace (this->getScene ().getCamera ().isOrthogonal () ? GL_CW : GL_CCW);

	if (buffers.shadowVao == GL_NONE || buffers.shadowVertexTag != layout.vertexTag) {
	    if (buffers.shadowVao == GL_NONE) {
		glGenVertexArrays (1, &buffers.shadowVao);
	    }
	    buffers.shadowVertexTag = layout.vertexTag;
	    glBindVertexArray (buffers.shadowVao);
	    glBindBuffer (GL_ARRAY_BUFFER, buffers.vertexBuffer);
	    glEnableVertexAttribArray (0);
	    glVertexAttribPointer (0, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*> (layout.positionOffset));
	    if (layout.uvOffset != MdlMesh::AttributeAbsent) {
		glEnableVertexAttribArray (3);
		glVertexAttribPointer (3, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*> (layout.uvOffset));
	    } else {
		glDisableVertexAttribArray (3);
	    }
	    if (this->m_skinningEnabled) {
		glEnableVertexAttribArray (1);
		glVertexAttribIPointer (
		    1, 4, GL_UNSIGNED_INT, stride,
		    reinterpret_cast<void*> (this->m_model.mesh.blendIndicesOffset)
		);
		glEnableVertexAttribArray (2);
		glVertexAttribPointer (
		    2, 4, GL_FLOAT, GL_FALSE, stride,
		    reinterpret_cast<void*> (this->m_model.mesh.blendWeightsOffset)
		);
	    }
	    glBindBuffer (GL_ELEMENT_ARRAY_BUFFER, m_modelData ? m_modelData->shapes[submeshIndex].indexBuffer : buffers.indexBuffer);
	}
	glBindVertexArray (buffers.shadowVao);
	if (m_modelData) {
	    const auto& shape = m_modelData->shapes[submeshIndex];
	    glBindBuffer (GL_ELEMENT_ARRAY_BUFFER, shape.indexBuffer);
	    if (shape.input.indexWidth == 0) {
		glDrawArrays (GL_TRIANGLES, 0, shape.drawCount);
	    } else {
		glDrawElements (GL_TRIANGLES, shape.drawCount,
		    shape.input.indexWidth == 2 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT, nullptr);
	    }
	} else {
	    glDrawElements (GL_TRIANGLES, buffers.indexCount, buffers.indexType, nullptr);
	}
    }

    glFrontFace (GL_CCW);
}

const Model3D& CModel::getModel () const { return this->m_model; }

std::optional<glm::mat4> CModel::getAttachmentTransform (const std::string& name) const {
    // Native attachment queries retain the evaluated pose until the next scene update.
    // Keep lazy initialization for models queried before setup has evaluated their bones.
    if (this->m_worldBones.empty ()) this->updateAnimationPose ();
    return MdlAnimationEvaluator::attachmentTransform (this->m_model.animationData, this->m_worldBones, name);
}

std::optional<size_t> CModel::getAttachmentIndex (const std::string& name) const {
    size_t index = 0;
    for (const auto& attachmentName : this->m_model.animationData.attachments | std::views::keys) {
	if (attachmentName == name) {
	    return index;
	}
	index++;
    }
    return std::nullopt;
}

std::optional<std::string> CModel::getAttachmentName (const size_t requestedIndex) const {
    size_t index = 0;
    for (const auto& attachmentName : this->m_model.animationData.attachments | std::views::keys) {
	if (index++ == requestedIndex) {
	    return attachmentName;
	}
    }
    return std::nullopt;
}

std::optional<size_t> CModel::getBoneIndex (const std::string& name) const {
    if (name.empty ()) return std::nullopt;
    for (size_t index = 0; index < this->m_model.animationData.bones.size (); ++index) {
	if (this->m_model.animationData.bones[index].name == name) return index;
    }
    return std::nullopt;
}

std::optional<glm::mat4> CModel::getBoneTransform (const size_t index) const {
    if (this->m_worldBones.empty ()) this->updateAnimationPose ();
    if (index >= this->m_worldBones.size ()) return std::nullopt;
    return this->resolveWorldMatrix () * this->m_worldBones[index];
}

const float& CModel::getBrightness () const { return this->m_brightness; }

const float& CModel::getUserAlpha () const { return this->m_alpha; }

const float& CModel::getAlpha () const { return this->m_alpha; }

const glm::vec3& CModel::getColor () const { return this->m_color; }

const glm::vec4& CModel::getColor4 () const { return this->m_color4; }

const glm::vec3& CModel::getCompositeColor () const { return this->m_color; }
