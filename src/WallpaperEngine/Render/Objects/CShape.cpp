#include "CShape.h"

#include "WallpaperEngine/Data/Model/Object.h"
#include "WallpaperEngine/Render/Camera.h"
#include "WallpaperEngine/Render/Shaders/Variables/ShaderVariable.h"

#include <cmath>
#include <glm/gtc/matrix_transform.hpp>

using namespace WallpaperEngine;
using namespace WallpaperEngine::Render::Objects;
using namespace WallpaperEngine::Data::Model;

namespace {
const Material shapeMaterial {};
}

CShape::CShape (Wallpapers::CScene& scene, const Shape& shape) :
    CObject (scene, shape), CRenderable (scene, shape, shapeMaterial), ScriptableObject (scene, shape),
    m_shape (shape) {
    registerProperty ("perspective", *shape.perspective);
    registerProperty ("parallaxDepth", *shape.parallaxDepth);
    registerProperty ("alpha", *shape.alpha);
    registerProperty ("color", *shape.color);
    registerProperty ("brightness", *shape.brightness);
    for (const auto& effect : shape.effects) {
	registerProperty ("visible_fx" + std::to_string (effect->id), *effect->visible);
	size_t index = 0;
	for (const auto& pass : effect->passOverrides) {
	    const auto scope = "effect_" + std::to_string (effect->id) + "_pass" + std::to_string (index++) + "_";
	    for (const auto& [name, setting] : pass->constants) {
		registerProperty (scope + name, *setting, scope);
	    }
	}
    }
}

CShape::~CShape () {
    m_passes.clear ();
    glDeleteBuffers (1, &m_vertices);
    glDeleteBuffers (1, &m_indices);
}

void CShape::setup () {
    // DIRECTDRAW shaders have no rasterized image input. A stable fallback satisfies
    // CPass' optional texture/uniform bindings without allocating image effect FBOs.
    m_texture = getScene ().resolveTexture ("util/white");
    glGenBuffers (1, &m_vertices);
    glGenBuffers (1, &m_indices);
    const GLushort indices[] { 0, 2, 1, 0, 3, 2 };
    // Upload as an array buffer: async setup has no render-context VAO bound.
    glBindBuffer (GL_ARRAY_BUFFER, m_indices);
    glBufferData (GL_ARRAY_BUFFER, sizeof (indices), indices, GL_STATIC_DRAW);
    const auto provider = std::make_shared<FBOProvider> (this);
    for (const auto& effect : m_shape.effects) {
	size_t index = 0;
	for (const auto& effectPass : effect->effect->passes) {
	    const auto override = index < effect->passOverrides.size ()
		? std::optional<std::reference_wrapper<const ImageEffectPassOverride>> (*effect->passOverrides[index])
		: std::nullopt;
	    ++index;
	    if (!effectPass->enabled) {
		continue;
	    }
	    for (const auto& material : effectPass->material.value ()->passes) {
		auto draw = std::make_unique<Effects::CPass> (
		    *this, provider, *material, override, effectPass->binds, std::nullopt,
		    ComboMap { { "DIRECTDRAW", 1 } }
		);
		auto* pass = draw.get ();
		pass->setInput (m_texture);
		pass->setModelMatrix (&m_model);
		pass->setViewProjectionMatrix (&m_viewProjection);
		pass->setModelViewProjectionMatrix (&m_mvp, getScene ().getCamera ().usesReverseDepth ());
		pass->setModelViewProjectionMatrixInverse (&m_mvpInverse);
		pass->setGeometryCallback (
		    [this, pass] {
			glBindBuffer (GL_ARRAY_BUFFER, m_vertices);
			const GLint position = glGetAttribLocation (pass->getProgramID (), "a_Position");
			const GLint uv = glGetAttribLocation (pass->getProgramID (), "a_TexCoord");
			if (position >= 0) {
			    glEnableVertexAttribArray (position);
			    glVertexAttribPointer (position, 3, GL_FLOAT, GL_FALSE, 5 * sizeof (float), nullptr);
			}
			if (uv >= 0) {
			    glEnableVertexAttribArray (uv);
			    glVertexAttribPointer (
				uv, 2, GL_FLOAT, GL_FALSE, 5 * sizeof (float),
				reinterpret_cast<void*> (3 * sizeof (float))
			    );
			}
		    },
		    [this] {
			glBindBuffer (GL_ELEMENT_ARRAY_BUFFER, m_indices);
			glDrawElements (GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, nullptr);
		    },
		    [pass] {
			for (const auto* name : { "a_Position", "a_TexCoord" }) {
			    const GLint attribute = glGetAttribLocation (pass->getProgramID (), name);
			    if (attribute >= 0) {
				glDisableVertexAttribArray (attribute);
			    }
			}
		    }
		);
		m_passes.push_back ({ effect.get (), std::move (draw) });
	    }
	}
    }
    if (!m_passes.empty ()) {
	// Native shapes override the final material pass, even when its effect says "normal".
	m_passes.back ().draw->setBlendingMode (BlendingMode_Additive);
    }
}

bool CShape::corner (const Effects::CPass& pass, const std::string& name, glm::vec2& value) const {
    // Read the same override/material/default priority as CPass::setupShaderVariables.
    const auto parameter = pass.getShader ()->findParameter (name);
    const auto* variable = parameter.vertex != nullptr ? parameter.vertex : parameter.fragment;
    if (variable == nullptr || glGetUniformLocation (pass.getProgramID (), variable->getName ().c_str ()) < 0) {
	return false;
    }
    for (const auto* constants : { &pass.getOverride ().constants, &pass.getPass ().constants }) {
	const auto entry = constants->find (name);
	if (entry != constants->end ()) {
	    value = glm::vec2 (entry->second->evaluateVec3 (getScene ().getElapsedTime ()));
	    return true;
	}
    }
    value = variable->getVec2 ();
    return true;
}

void CShape::render () {
    if (!m_shape.groupVisible->value->getBool () || !isVisibleThroughParents ()) {
	return;
    }
    const Pass* first = nullptr;
    const ImageEffect* last = nullptr;
    for (const auto& pass : m_passes) {
	if (!pass.effect->visible->value->getBool ()) {
	    continue;
	}
	if (first == nullptr) {
	    first = &pass;
	}
	last = pass.effect;
    }
    if (first == nullptr) {
	return;
    }

    std::array<glm::vec2, 4> corners { { { 0, 0 }, { 1, 0 }, { 1, 1 }, { 0, 1 } } };
    auto points = corners;
    bool complete = true;
    for (size_t i = 0; i < points.size (); ++i) {
	complete &= corner (*first->draw, "point" + std::to_string (i), points[i]);
    }
    if (complete) {
	corners = points;
    }
    std::array<float, 20> vertices;
    const float height = getScene ().getHeight ();
    for (size_t i = 0; i < corners.size (); ++i) {
	vertices[i * 5] = height * (corners[i].x - 0.5f);
	vertices[i * 5 + 1] = height * (0.5f - corners[i].y);
	vertices[i * 5 + 2] = 0;
	vertices[i * 5 + 3] = corners[i].x;
	vertices[i * 5 + 4] = corners[i].y;
    }
    glBindBuffer (GL_ARRAY_BUFFER, m_vertices);
    glBufferData (GL_ARRAY_BUFFER, sizeof (vertices), vertices.data (), GL_DYNAMIC_DRAW);
    m_model = resolveWorldMatrix ();
    const auto& camera = getScene ().getCamera ();
    m_viewProjection = camera.getRasterProjection () * camera.getLookAt ();
    if (!getScene ().getScene ().camera.projection.isPerspective) {
	m_model = glm::translate (glm::mat4 (1), glm::vec3 (resolveParallaxOffset (), 0))
	    * glm::scale (glm::mat4 (1), glm::vec3 (1, -1, 1))
	    * glm::translate (glm::mat4 (1), glm::vec3 (-getScene ().getWidth () * .5f, -height * .5f, 0)) * m_model;
	if (m_shape.perspective->value->getBool ()) {
	    const float fov = glm::radians (glm::clamp (camera.getFov (), .1f, 179.9f));
	    const float distance = 1.0f / (std::tan (fov * .5f) * camera.getProjection ()[1][1]);
	    auto view = camera.getLookAt ();
	    view[3][2] = -distance;
	    const auto& output = getScene ().getOutputSize ();
	    m_viewProjection
		= glm::perspective (fov, output.x / output.y, 5.0f, glm::max (15000.0f, distance + 1000.0f)) * view;
	}
    }
    m_mvp = m_viewProjection * m_model;
    m_mvpInverse = glm::inverse (m_mvp);
    const double time = getScene ().getElapsedTime ();
    m_alpha = m_shape.alpha->evaluateFloat (time);
    m_brightness = m_shape.brightness->evaluateFloat (time);
    m_color = m_shape.color->value->getVec3 ();
    m_color4 = m_shape.color->value->getVec4 ();
    glColorMask (GL_TRUE, GL_TRUE, GL_TRUE, getScene ().isRenderingToComposition () ? GL_TRUE : GL_FALSE);
    for (const auto& pass : m_passes) {
	if (pass.effect != last) {
	    continue;
	}
	pass.draw->setDestination (getScene ().getActiveRenderTarget ());
	pass.draw->render ();
    }
    glColorMask (GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
}
