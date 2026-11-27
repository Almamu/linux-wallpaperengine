#include "CParticle.h"
#include "WallpaperEngine/Render/Objects/CImage.h"
#include "CModel.h"

#include "WallpaperEngine/Render/Depth.h"

#include "WallpaperEngine/Data/Model/Property.h"
#include "WallpaperEngine/Data/Parsers/ObjectParser.h"
#include "WallpaperEngine/Logging/Log.h"
#include "WallpaperEngine/Maths.h"
#include "WallpaperEngine/Render/Utils/NoiseUtils.h"

#include <GL/glew.h>
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <limits>
#include <utility>
#if defined(__SSE__)
#include <xmmintrin.h>
#endif

extern float g_Daytime;

using namespace WallpaperEngine::Render::Objects;
using namespace WallpaperEngine::Render::Utils;
using namespace WallpaperEngine::Data::Model;

float WallpaperEngine::Render::Objects::advanceParticleRenderTime (const double currentTime, double& previousTime) {
    const float delta = previousTime == 0.0 ? 0.0f : static_cast<float> (currentTime - previousTime);
    previousTime = currentTime;
    return delta;
}

float WallpaperEngine::Render::Objects::calculateParticleSimulationDelta (const float elapsed, const float rate) {
    return glm::max (elapsed, 0.0f) * glm::max (rate, 0.0f);
}

float WallpaperEngine::Render::Objects::calculateParticleEmissionRate (const float emitterRate, const float count) {
    return glm::max (emitterRate, 0.0f) * glm::max (count, 0.0f);
}

std::vector<ParticleLayerImageSample> WallpaperEngine::Render::Objects::buildParticleLayerImageSamples (
    const std::vector<uint8_t>& rgba, const glm::uvec2 extent, const glm::uvec2 sourceExtent
) {
    std::vector<ParticleLayerImageSample> samples;
    const float scale = static_cast<float> (sourceExtent.x) / extent.x;
    for (uint32_t x = 0; x < extent.x; ++x) {
	for (uint32_t y = 0; y < extent.y; ++y) {
	    const size_t pixel = (static_cast<size_t> (y) * extent.x + x) * 4;
	    if (rgba[pixel + 3] < 127) continue;
	    samples.push_back ({
		static_cast<int16_t> (scale * x - static_cast<float> (sourceExtent.x / 2) + scale * 0.5f),
		static_cast<int16_t> (scale * y - static_cast<float> (sourceExtent.y / 2) + scale * 0.5f),
		{ rgba[pixel], rgba[pixel + 1], rgba[pixel + 2] }
	    });
	}
    }
    return samples;
}

float WallpaperEngine::Render::Objects::calculateParticleLifetime (
    const float minimum, const float maximum, const float exponent, float fraction, const float multiplier
) {
    if (exponent != 1.0f) fraction = std::pow (fraction, exponent);
    const float lifetime = (minimum + (maximum - minimum) * fraction) * multiplier;
    // Native birth opcode 1 floors the final, instance-scaled sample (14023b637).
    return lifetime <= 0.001f ? 0.001f : lifetime;
}

float WallpaperEngine::Render::Objects::calculateParticleAudioResponse (
    const float* left, const float* right, const int mode, const glm::vec2& bounds,
    const float exponent, const int frequencyStart, const int frequencyEnd
) {
    if (mode == 0) return 1.0f;
    // Native 14022a8a0 uses the strongest selected band. Stereo averages
    // each pair before taking that maximum, rather than averaging the range.
    float peak = 0.0f;
    for (int band = std::clamp (frequencyStart, 0, 15); band <= std::clamp (frequencyEnd, 0, 15); ++band) {
	const float value = mode == 1 ? left[band] : mode == 2 ? right[band]
	    : mode == 3 ? (left[band] + right[band]) * 0.5f : 0.0f;
	peak = std::max (peak, value);
    }
    const float mapped = (peak - bounds.x) / (bounds.y - bounds.x);
    const float value = mapped < 0.0f ? 0.0f : mapped < 1.0f ? mapped : 1.0f;
    const float response = std::pow ((3.0f - 2.0f * value) * value * value, exponent);
    return response < 0.0f ? 0.0f : response < 1.0f ? response : 1.0f;
}

void WallpaperEngine::Render::Objects::initializeParticleBetweenControlPoints (
    ParticleInstance& particle, const MapSequenceBetweenControlPointsInitializer& initializer,
    const glm::vec3& start, const glm::vec3& end, float& phase, float& direction, const float countMultiplier
) {
    // Native initializer opcode 14 (14023ca93) preserves only the emitter's
    // displacement perpendicular to the control-point segment.
    const glm::vec3 segment = end - start;
    const float distance = glm::length (segment);
    const glm::vec3 axis = distance > 0.0f ? segment / distance : glm::vec3 (0.0f);
    glm::vec3 displacement = particle.position - axis * glm::dot (particle.position, axis);
    const float weight = 1.0f - std::pow (std::abs (phase * 2.0f - 1.0f), 2.0f);
    if ((initializer.flags & 1) != 0) displacement *= weight;
    const glm::vec2 bounds = initializer.bounds->value->getVec2 ();
    particle.position = start + segment * glm::mix (bounds.x, bounds.y, phase) + displacement;
    if ((initializer.flags & 8) != 0) {
	const glm::vec3 arcDirection (initializer.arcDirection.x, -initializer.arcDirection.y,
				    initializer.arcDirection.z);
	particle.position += arcDirection * (weight * distance * initializer.arcAmount);
    }
    if ((initializer.flags & 2) != 0) particle.velocity *= weight;
    if ((initializer.flags & 4) != 0) {
	particle.size *= 1.0f - initializer.sizeReductionAmount + initializer.sizeReductionAmount * weight;
	particle.initial.size = particle.size;
    }

    // Both endpoints are samples: unlike the circular initializer, the step
    // is 1/(count-1). Native repeat resets to zero; mirror reflects overshoot.
    phase += direction / std::max (0.0001f, initializer.count->value->getFloat () * countMultiplier - 1.0f);
    if (phase > 1.0f) {
	if (initializer.limitBehavior == "mirror") {
	    phase = 2.0f - phase;
	    direction = -direction;
	} else {
	    phase = 0.0f;
	}
    } else if (phase < 0.0f) {
	phase = -phase;
	direction = -direction;
    }
}

glm::vec3
WallpaperEngine::Render::Objects::convertParticleRotationForRender (
    const glm::vec3& rotation, const bool preserveZ
) {
    // Particle definitions use Wallpaper Engine's Y-down scene space. Reflection across
    // Y changes the handedness of axial rotation vectors, so X and Z change sign.
    // Perspective billboards and upright 2D sprite tangents already account
    // for that reflection. Preserve native Z spin (1402308a0 uploads it unchanged).
    return { -rotation.x, rotation.y, preserveZ ? rotation.z : -rotation.z };
}

float WallpaperEngine::Render::Objects::calculateRopeTrailVisualValue (
    const float currentValue, const float trailPosition, const bool fadeAlongTrail
) {
    return currentValue * (fadeAlongTrail ? std::clamp (trailPosition, 0.0f, 1.0f) : 1.0f);
}

glm::vec3 WallpaperEngine::Render::Objects::calculateControlPointAttraction (
    const glm::vec3& toCenter, const float strength, const float radius, const float deltaTime,
    const bool limitToDistance, const float lifetimeWeight
) {
    const float distance = glm::length (toCenter);
    if (distance <= std::numeric_limits<float>::min () || distance >= radius) {
	return glm::vec3 (0.0f);
    }
    // The native controlpointattract operator fades linearly over the complete
    // threshold radius. A constant force in half that radius makes flocks turn
    // abruptly and changes the shape of magic vortices.
    const float falloff = 1.0f - distance / radius;
    float magnitude = strength * deltaTime * falloff;
    // Native flags default to 2: cap positive attraction before lifetime blending.
    // Negative strength remains an uncapped repulsion.
    if (limitToDistance) magnitude = std::min (magnitude, distance);
    return toCenter * (magnitude * lifetimeWeight / distance);
}

glm::vec3 WallpaperEngine::Render::Objects::resolveParticleControlPoint (
    const glm::vec3& offset, const glm::mat4& worldToLocal, const bool worldSpace
) {
    // Native 14022bd40/14022a070: instance overrides replace the reference
    // position without changing its space. World points use the full inverse
    // layer transform; local points remain relative to the emitter.
    const glm::vec3 local = worldSpace ? glm::vec3 (worldToLocal * glm::vec4 (offset, 1.0f)) : offset;
    if (!std::isfinite (local.x) || !std::isfinite (local.y) || !std::isfinite (local.z)) {
	return glm::vec3 (0.0f); // A zero-scale layer has no invertible coordinate space.
    }
    return { local.x, -local.y, local.z };
}

glm::vec3 WallpaperEngine::Render::Objects::calculateFollowChildDisplacement (
    const std::optional<glm::vec3>& previous, const glm::vec3& next, const bool worldSpace
) {
    // Local-space event-follow particles ride with their parent particle, so a
    // one-shot head stays on a meteor's tip. World-space births stay where emitted.
    return previous.has_value () && !worldSpace ? next - *previous : glm::vec3 (0.0f);
}

glm::mat3 WallpaperEngine::Render::Objects::calculateFixedParticleOrientation (
    const glm::vec3& axis, const glm::mat3& model, const bool worldSpace
) {
    const auto normalize = [] (const glm::vec3& value, const glm::vec3& fallback) {
	const float lengthSquared = glm::dot (value, value);
	return std::isfinite (lengthSquared) && lengthSquared > 0.0f
	    ? value / std::sqrt (lengthSquared) : fallback;
    };
    const glm::vec3 forward = normalize (axis, { 0.0f, 1.0f, 0.0f });
    glm::vec3 up (0.0f, 0.0f, -1.0f);
    if (forward.x != 0.0f || forward.z != 0.0f) {
	up = normalize (glm::cross (forward, glm::cross (glm::vec3 (0.0f, 1.0f, 0.0f), forward)), up);
    }

    // Native 1401c22e0/1402298b0: axis is the plane normal; the default up
    // tangent is -Z, so depth streaks lie along the scene rather than the screen.
    // The draw helper applies the model and its transpose before normalizing.
    // Keep that order for nonuniform scale; renderer flag 1 skips the first step.
    const glm::mat3 transform = glm::transpose (model) * (worldSpace ? glm::mat3 (1.0f) : model);
    const glm::vec3 localForward = transform * forward;
    const glm::vec3 localUp = transform * up;
    return {
	normalize (glm::cross (localUp, localForward), { 1.0f, 0.0f, 0.0f }),
	normalize (localUp, up), normalize (localForward, forward)
    };
}

glm::mat3 WallpaperEngine::Render::Objects::calculateUprightParticleOrientation (
    const glm::vec3& axis, const glm::mat3& model, const glm::vec3& cameraForward, const bool worldSpace
) {
    const float axisLength = glm::dot (axis, axis);
    const glm::vec3 normalizedAxis = axisLength > 0.0f ? axis / std::sqrt (axisLength) : glm::vec3 (0, 1, 0);
    // Native 1402298b0 uses world Y for flag 1, otherwise the authored axis.
    // Both vectors cross the model transpose before constructing local right.
    const glm::vec3 up = worldSpace ? glm::vec3 (0, 1, 0) : model * normalizedAxis;
    const glm::vec3 localUp = glm::transpose (model) * up;
    const glm::vec3 localForward = glm::transpose (model) * glm::cross (up, glm::cross (cameraForward, up));
    return {
        glm::normalize (glm::cross (localUp, localForward)), glm::normalize (localUp), glm::normalize (localForward)
    };
}

glm::vec3 WallpaperEngine::Render::Objects::calculateUprightRopeForward (
    const glm::vec3& axis, const glm::mat3& model, const glm::vec3& cameraForward, const bool worldSpace
) {
    const float axisLength = glm::dot (axis, axis);
    const glm::vec3 normalizedAxis = axisLength > 0.0f ? axis / std::sqrt (axisLength) : glm::vec3 (0, 1, 0);
    // Native 1402298b0 locks upright to the authored axis. Renderer flag 1
    // instead uses world Y, independently of the authored axis.
    const glm::vec3 up = worldSpace ? glm::vec3 (0, 1, 0) : model * normalizedAxis;
    const glm::vec3 forward = glm::transpose (model) * glm::cross (up, glm::cross (cameraForward, up));
    const float lengthSquared = glm::dot (forward, forward);
    return lengthSquared > 0.0f ? forward / std::sqrt (lengthSquared) : cameraForward;
}

glm::mat3 WallpaperEngine::Render::Objects::calculateBillboardParticleOrientation (
    const glm::mat4& modelInverse, const glm::mat4& cameraWorld, const float roll
) {
    // Keep the layer's authored roll in the camera plane. Cancelling the entire
    // model rotation makes deliberately upside-down billboards stand upright.
    // Rotate before converting to local space so nonuniform scales do not shear
    // the camera-facing directions when the model matrix is applied again.
    const glm::mat3 cameraToLocal (modelInverse * glm::rotate (cameraWorld, roll, { 0.0f, 0.0f, 1.0f }));
    return {
	glm::normalize (cameraToLocal[0]), glm::normalize (cameraToLocal[1]), glm::normalize (cameraToLocal[2])
    };
}

CParticle::CParticle (Wallpapers::CScene& scene, const Particle& particle, CParticle* parent) :
    CObject (scene, particle), CRenderable (scene, particle, *particle.material->material),
    ScriptableObject (scene, particle), m_particle (particle), m_particleParent (parent) {
    this->registerProperty ("scale", *particle.scale);
    this->registerProperty ("angles", *particle.angles);
    this->registerProperty ("visible", *particle.visible);
    this->registerProperty ("parallaxDepth", *particle.parallaxDepth);
    const auto hasTimeline = [] (const auto& entry) { return entry.second->animation != nullptr; };
    const auto& rootSettings = getInstanceOverride ();
    if (parent == nullptr) {
	// Native root setup clamps the exposed field before script init callbacks.
	rootSettings.rate->value->update (std::max (0.01f, rootSettings.rate->value->getFloat ()),
	    DynamicValue::UpdateSource::Initialization);
    }
    activateInstanceParameters ();
    const auto subscribeParameters = [this] (const UserSetting& setting, const int channels) {
	m_instanceParameterSubscriptions.emplace_back (Data::Utils::ScopeGuard (setting.value->listen (
	    [this] (const DynamicValue&, DynamicValue::UpdateSource source) {
		if (source != DynamicValue::UpdateSource::Initialization) m_instanceParametersDirty = true;
	    }
	)));
	if (setting.animation != nullptr) {
	    bool complete = true;
	    for (int channel = 0; channel < channels; ++channel) {
		const auto curve = setting.animation->channels.find (channel);
		complete &= curve != setting.animation->channels.end () && !curve->second.empty ();
	    }
	    m_instanceParameterTimelines |= complete;
	}
    };
    // These native setters dirty common parameters even for equal values.
    // Rate alone does not. Paused complete timelines still invoke the setter.
    if (parent == nullptr) {
	for (const auto* setting : { rootSettings.alpha.get (), rootSettings.size.get (), rootSettings.count.get (),
		rootSettings.speed.get (), rootSettings.lifetime.get (), rootSettings.brightness.get () }) {
	    subscribeParameters (*setting, 1);
	}
	subscribeParameters (*rootSettings.colorn, 3);
	for (const auto& [id, setting] : rootSettings.controlPointOffsets) subscribeParameters (*setting, 3);
	for (const auto& [id, setting] : rootSettings.controlPointAngles) subscribeParameters (*setting, 3);
    }
    m_controlPointTimelines = std::ranges::any_of (rootSettings.controlPointOffsets, hasTimeline)
	|| std::ranges::any_of (rootSettings.controlPointAngles, hasTimeline);
    if (parent == nullptr) {
	// Child systems consume the root's live overrides. Queue their property
	// scripts once on that root, with the same values used by the simulation.
	const auto& settings = particle.instanceOverride;
	for (const auto& [name, value] : std::initializer_list<std::pair<const char*, const UserSetting*>> {
	    { "enabled", settings.enabled.get () }, { "alpha", settings.alpha.get () },
	    { "brightness", settings.brightness.get () },
	    { "size", settings.size.get () }, { "lifetime", settings.lifetime.get () },
	    { "rate", settings.rate.get () }, { "speed", settings.speed.get () },
	    { "count", settings.count.get () }, { "color", settings.color.get () },
	    { "colorn", settings.colorn.get () } }) {
	    this->registerProperty (name, *value);
	}
	for (const auto& [id, value] : settings.controlPointOffsets) {
	    m_controlPointSubscriptions.emplace_back (Data::Utils::ScopeGuard (value->value->listen ([this, id] (const DynamicValue&, DynamicValue::UpdateSource source) {
		if (source != DynamicValue::UpdateSource::Initialization) {
		    m_controlPointOverridesChanged[id] = true;
		    m_controlPointAnglesChanged = true;
		}
	    })));
	    this->registerProperty ("instance.controlpoint" + std::to_string (id), *value, "instance.");
	}
	const auto subscribeAngleRefresh = [this] (const UserSetting& setting) {
	    m_controlPointSubscriptions.emplace_back (Data::Utils::ScopeGuard (setting.value->listen ([this] (const DynamicValue&, DynamicValue::UpdateSource source) {
		if (source != DynamicValue::UpdateSource::Initialization) m_controlPointAnglesChanged = true;
	    })));
	};
	// A common native parameter refresh applies every authored angle to children.
	for (const auto* setting : { settings.alpha.get (), settings.size.get (), settings.count.get (),
		settings.speed.get (), settings.lifetime.get (), settings.brightness.get (), settings.colorn.get () }) {
	    subscribeAngleRefresh (*setting);
	}
	for (const auto& [id, value] : settings.controlPointAngles) {
	    subscribeAngleRefresh (*value);
	    this->registerProperty ("instance.controlpointangle" + std::to_string (id), *value, "instance.");
	}
    }

    this->detectTexture ();
    // Initialize random number generator with time-based seed
    std::random_device rd;
    m_rng.seed (rd ());

    // Read renderer configuration early to determine rendering mode
    if (!m_particle.renderers.empty ()) {
	const auto& renderer = m_particle.renderers[0];
	if (renderer.name == "rope" || renderer.name == "ropetrail") {
	    // Both rope and ropetrail use genericropeparticle shader
	    m_useRopeRenderer = true;
	    m_ropeSubdivision = std::max (0, static_cast<int> (renderer.subdivision));
	    m_ropeUVScale = renderer.uvScale;
	    m_ropeUVScrolling = renderer.uvScrolling;
	    m_ropeUVSmoothing = renderer.uvSmoothing;

	    if (renderer.name == "ropetrail") {
		m_useTrailRenderer = true;
		m_useRopeTrailRenderer = true;
		m_trailLength = renderer.length;
		if (renderer.segmentsExplicit) {
		    m_ropeSegments = std::max (2, static_cast<int> (renderer.segments));
		} else {
		    // Wallpaper Engine omits the default segment count from JSON. Four
		    // samples made long trails visibly step at 1-5 Hz, so derive a smooth
		    // bounded history rate from the authored duration instead.
		    m_ropeSegments
			= std::clamp (static_cast<int> (std::ceil (std::max (m_trailLength, 0.001f) * 30.0f)), 8, 96);
		}
		m_ropeFadeAlpha = renderer.fadeAlpha;
		m_ropeFadeSize = renderer.fadeSize;
	    }
	} else if (renderer.name == "spritetrail") {
	    // spritetrail uses genericparticle with TRAILRENDERER combo
	    m_useTrailRenderer = true;
	    m_trailLength = renderer.length;
	    m_trailMaxLength = renderer.maxLength;
	    m_trailMinLength = renderer.minLength;
	}
    }

    // maxCount is the authored capacity. The instance count override changes emission,
    // not the pool size.
    m_maxParticles = particle.maxCount > 0 ? particle.maxCount : DEFAULT_MAX_PARTICLES;

    m_particles.resize (m_maxParticles);

    // Calculate buffer sizes based on renderer type
    if (m_useRopeRenderer) {
	const int subdivision = std::max (1, m_ropeSubdivision);
	// A rope connects the particle pool once. A rope trail builds an independent
	// history for every particle and must never bridge two particle instances.
	const int maxSegments = m_useRopeTrailRenderer
	    ? std::max (1, static_cast<int> (m_maxParticles)) * m_ropeSegments
	    : std::max (1, static_cast<int> (m_maxParticles - 1));
	const int maxSubSegments = maxSegments * subdivision;
	m_vertices.resize (maxSubSegments * 4 * ROPE_FLOATS_PER_VERTEX);
	m_indices.resize (maxSubSegments * 6);
    } else {
	// Trail particles: (N+1) * 2 vertices for ribbon strip, N * 6 indices for N quads
	// Normal particles: 4 vertices, 6 indices
	const int verticesPerParticle = 4;
	const int indicesPerParticle = 6;

	m_vertices.resize (m_maxParticles * verticesPerParticle * SPRITE_FLOATS_PER_VERTEX);
	m_indices.resize (m_maxParticles * indicesPerParticle);
    }
}

CParticle::~CParticle () {
    delete m_pass;

    if (m_vao != 0) {
	glDeleteVertexArrays (1, &m_vao);
    }
    if (m_vbo != 0) {
	glDeleteBuffers (1, &m_vbo);
    }
    if (m_ebo != 0) {
	glDeleteBuffers (1, &m_ebo);
    }

    m_vertices.clear ();
    m_indices.clear ();
}

void CParticle::setup () {
    if (m_initialized) {
	return;
    }

    // Load particle material constants
    if (m_particle.material && m_particle.material->material && !m_particle.material->material->passes.empty ()) {
	auto& firstPass = *m_particle.material->material->passes.begin ();

	// Read overbright constant (brightness multiplier for additive particles)
	auto overbrightIt = firstPass->constants.find ("ui_editor_properties_overbright");
	if (overbrightIt != firstPass->constants.end ()) {
	    m_overbright = overbrightIt->second->value->getFloat ();
	}
    }

    // Static sheets use .tex-json grids. Binary TEXS frames supply their own
    // count and axes, including sheets whose frames span several texture pages.
    if (const auto texture = getTexture ()) {
	m_spritesheetCols = static_cast<int> (texture->getSpritesheetCols ());
	m_spritesheetRows = static_cast<int> (texture->getSpritesheetRows ());
	m_spritesheetFrames = static_cast<int> (texture->getSpritesheetFrames ());
	const auto& frames = texture->getFrames ();
	if (texture->isAnimated () && !frames.empty () && frames.front ()->width1 > 0.0f
	    && frames.front ()->height1 > 0.0f) {
	    m_spritesheetFrames = static_cast<int> (frames.size ());
	}
    }

    setupEmitters ();
    setupInitializers ();
    setupOperators ();
    setupPass ();

    m_controlPoints.resize (PARTICLE_CONTROL_POINT_COUNT);
    for (const auto& cp : m_particle.controlPoints) {
	if (cp.id < 0 || cp.id >= PARTICLE_CONTROL_POINT_COUNT) continue;
	auto& runtime = m_controlPoints[cp.id];
	runtime.offset = cp.offset;
	runtime.flags = cp.flags;
	runtime.parentControlPoint = cp.parentControlPoint;
    }
    updateControlPoints ();
    m_commonControlPointTransform = m_controlPointTransform;

    setupChildren ();
    m_initialized = true;
    m_hasPrewarmInTree = m_particle.startTime > 0.0f && std::isfinite (m_particle.startTime);
    m_hasPrewarmInTree |= std::ranges::any_of (m_children, [] (const auto& child) {
	return child.renderer->m_hasPrewarmInTree;
    });
    if (m_particleParent == nullptr) prewarm (true);
}

const ParticleInstanceOverride& CParticle::getInstanceOverride () const {
    return m_particleParent ? m_particleParent->getInstanceOverride () : m_particle.instanceOverride;
}

void CParticle::setupChildren () {
    const auto supported = [] (const ParticleChild& child) {
	return (child.type == "static" || ((child.type == "eventfollow" || child.type == "eventdeath") && child.maxCount > 0))
	    && !child.particleFile.empty ();
    };
    size_t remainingChildren = std::ranges::count_if (m_particle.children, supported);
    for (const auto& child : m_particle.children) {
	if (!supported (child)) {
	    continue;
	}
	bool recursive = false;
	int depth = 0;
	CParticle* root = this;
	for (auto* ancestor = this; ancestor; ancestor = ancestor->m_particleParent) {
	    root = ancestor;
	    recursive |= ancestor->m_particle.particleFile == child.particleFile;
	    ++depth;
	}
	// Reserve a share for every sibling before letting this branch allocate
	// nested systems. A large first event pool must not discard later children
	// such as the star sprite accompanying a shooting star's glow.
	const size_t branchEnd = root->m_childSystemCount
	    + (m_childAllocationLimit - root->m_childSystemCount) / remainingChildren--;
	if (recursive || depth >= 8 || root->m_childSystemCount >= branchEnd) {
	    sLog.error ("Skipping recursive or excessive particle child: ", child.particleFile);
	    continue;
	}
	try {
	    using WallpaperEngine::Data::JSON::JSON;
	    auto definition = WallpaperEngine::Data::Parsers::ObjectParser::parse (
		JSON { { "id", -1 }, { "name", child.name }, { "particle", child.particleFile } },
		getScene ().getScene ().project
	    );
	    auto* particle = definition->as<Particle> ();
	    if (!particle || !particle->material || !particle->material->material
		|| particle->material->material->passes.empty ()) {
		continue;
	    }
	    particle->origin->value->update (child.origin, DynamicValue::UpdateSource::Initialization);
	    particle->angles->value->update (child.angles, DynamicValue::UpdateSource::Initialization);
	    particle->scale->value->update (child.scale, DynamicValue::UpdateSource::Initialization);
	    if (child.type == "static") {
		++root->m_childSystemCount;
		auto renderer = std::make_unique<CParticle> (getScene (), *particle, this);
		renderer->m_childAllocationLimit = branchEnd;
		renderer->m_controlPointStartIndex = child.controlPointStartIndex;
		renderer->setup ();
		m_children.push_back ({ std::move (definition), std::move (renderer) });
	    } else {
		FollowChildSystem system { .definition = std::move (definition), .settings = &child, .instances = {} };
		// Prepare a bounded pool on the load worker. Spawning an event child must
		// never parse assets or compile shaders in the middle of a rendered frame.
		const auto capacity = child.type == "eventdeath" ? child.maxCount
		    : std::min (child.maxCount, static_cast<int> (m_maxParticles));
		for (int i = 0; i < capacity; ++i) {
		    if (root->m_childSystemCount >= branchEnd) break;
		    ++root->m_childSystemCount;
		    auto renderer = std::make_unique<CParticle> (getScene (), *particle, this);
		    renderer->m_childAllocationLimit = branchEnd;
		    renderer->m_controlPointStartIndex = child.controlPointStartIndex;
		    renderer->m_followsWorldParticle = (m_particle.flags & 1) != 0;
		    renderer->setup ();
		    renderer->pause ();
		    system.instances.push_back ({ .renderer = std::move (renderer) });
		}
		m_followChildren.push_back (std::move (system));
	    }
	} catch (const std::exception& error) {
	    sLog.error ("Cannot load child particle system: ", child.particleFile, " - ", error.what ());
	}
    }
}

void CParticle::prewarm (const bool includePermanentChildren) {
    if (!m_hasPrewarmInTree) return;
    const float duration = m_particle.startTime;
    const bool warmThisSystem = duration > 0.0f && std::isfinite (duration);
    if (!warmThisSystem && !includePermanentChildren) return;

    // Seed control-point history at the current transform, including when a
    // pooled child has just been moved to a new parent particle.
    updateControlPoints ();
    if (includePermanentChildren) {
	for (auto& child : m_children) child.renderer->prewarm (true);
    }
    if (!warmThisSystem) return;

    // Native 14022f28e calls the local simulation directly, with event dispatch
    // disabled. The owning scene and playback clocks do not advance.
    auto eventChildren = std::exchange (m_followChildren, {});
    const bool hasRopeTrail = std::exchange (m_useRopeTrailRenderer, false);
    const Data::Utils::ScopeGuard restore ([&] {
	m_followChildren = std::move (eventChildren);
	m_useRopeTrailRenderer = hasRopeTrail;
    });
    const float step = m_maxParticles < 500 ? 0.05f : 0.20f;

    // Bound work for excessive authored durations. The largest installed
    // value checked was 300 seconds (about 6,000 steps at the finer interval).
    constexpr int maximumSteps = 10000;
    float elapsed = 0.0f;
    for (int i = 0; i < maximumSteps && elapsed < duration; ++i, elapsed += step) {
	updateControlPoints (step);
	update (step);
    }
    if (elapsed < duration) {
	sLog.error ("Particle prewarm reached the ", maximumSteps, "-step limit: ", m_particle.particleFile);
    }
    if (hasRopeTrail) {
	m_ropeHistoryTimer = 0.0f;
	for (uint32_t i = 0; i < m_particleCount; ++i) {
	    auto& particle = m_particles[i];
	    particle.trailLastFrame = { .position = particle.position };
	    particle.trailLastFrameValid = true;
	    particle.trailHistory.assign (1, particle.trailLastFrame);
	}
    }
}

void CParticle::updateInstanceParameters (const bool allowColorOverride) {
    const auto& settings = getInstanceOverride ();
    m_turbulenceRate = settings.rate->value->getFloat ();
    const glm::vec3 color = settings.colorn->value->getVec3 ();
    m_colorOverrideActive = allowColorOverride && (m_particle.flags & 8) == 0 && color.x >= 0.0f
	&& glm::any (glm::greaterThanEqual (glm::abs (color - m_particle.colorReference), glm::vec3 (0.9f / 255.0f)));
    if (m_colorOverrideActive) {
	m_colorOverrideDelta = WallpaperEngine::Maths::particleRgbToHsv (color)
	    - WallpaperEngine::Maths::particleRgbToHsv (m_particle.colorReference);
    }
    // Native caches brightness with birth color. Existing particles retain it;
    // common refresh prepares subsequent births and color-operator endpoints.
    m_instanceColor = glm::vec3 (getScene ().isHdr () && (m_particle.flags & 8) == 0
	? settings.brightness->value->getFloat () : 1.0f);
    if (m_colorOverrideActive && (getScene ().getScene ().version < 5 || !m_particle.hasColor)) {
	m_instanceColor *= color;
    }
    m_instanceParametersDirty = false;
    if (m_initialized && (m_hasMutatingRemapInitializer || m_hasControlPointOutputOperator))
        updateControlPoints (0.0f, true);
}

void CParticle::activateInstanceParameters () {
    const bool inherited = m_particleParent == nullptr || m_particleParent->m_propagateColorOverride;
    updateInstanceParameters (inherited);
    // First activation copies the parent's propagation bit, even when this
    // child locally opts out. Permanent descendants follow the same rule.
    m_propagateColorOverride = m_particleParent ? inherited : m_colorOverrideActive;
    for (auto& child : m_children) child.renderer->activateInstanceParameters ();
}

void CParticle::refreshInstanceParameters (bool suppressColorOverride) {
    updateInstanceParameters (!suppressColorOverride);
    if (m_particleParent == nullptr && !m_colorOverrideActive) suppressColorOverride = true;
    m_propagateColorOverride = m_colorOverrideActive;
    // Native common refresh also visits permanent and pooled children while
    // hidden. Deferring their cache read until drawing would sample later writes.
    for (auto& child : m_children) child.renderer->refreshInstanceParameters (suppressColorOverride);
    for (auto& child : m_followChildren) {
	for (auto& instance : child.instances) instance.renderer->refreshInstanceParameters (suppressColorOverride);
    }
}

glm::vec3 CParticle::adjustInstanceColor (const glm::vec3& color) const {
    if (!m_colorOverrideActive) return color;
    glm::vec3 hsv = WallpaperEngine::Maths::particleRgbToHsv (color) + m_colorOverrideDelta;
    hsv.x -= std::floor (hsv.x);
    hsv.y = std::clamp (hsv.y, 0.0f, 1.0f);
    hsv.z = std::clamp (hsv.z, 0.0f, 1.0f);
    return WallpaperEngine::Maths::particleHsvToRgb (hsv);
}

void CParticle::rebaseRenderTime (double offset) {
    CRenderable::rebaseRenderTime (offset);
    if (this->m_time != 0.0) this->m_time += offset;
    for (auto& child : this->m_children) child.renderer->rebaseRenderTime (offset);
    for (auto& child : this->m_followChildren)
	for (auto& instance : child.instances) instance.renderer->rebaseRenderTime (offset);
}

bool CParticle::updateForRender () {
    m_frameSimulationDelta = 0.0f;
    if (!m_initialized) {
	return false;
    }
    if (m_instanceParametersDirty || m_instanceParameterTimelines) {
	auto& rate = *getInstanceOverride ().rate->value;
	if (m_particleParent == nullptr) {
	    // Native 1402307f1 writes back only on the root common refresh.
	    // Child activation samples raw root rate without clamping it again.
	    rate.update (std::max (0.01f, rate.getFloat ()), DynamicValue::UpdateSource::Initialization);
	    if (m_instanceParameterTimelines) m_controlPointAnglesChanged = true;
	}
	refreshInstanceParameters ();
    }
    const bool visible = m_particle.visible->value->getBool () && isVisibleThroughParents ();
    updateRemapClocks (getScene ().getDeltaTime (), visible);
    // Native common refresh reads the previous system+3a0 publication, then
    // the layer update publishes its current matrix even while integration is held.
    const glm::mat4 pointReflection = glm::scale (glm::mat4 (1.0f), glm::vec3 (1.0f, -1.0f, 1.0f));
    m_commonControlPointTransform = pointReflection * particleWorldMatrix () * pointReflection;
    if (!visible) return false;

    // All systems in this scene frame share one double driver sample. Narrow only
    // the interval; float timestamps lose individual frames after long uptimes.
    float dt = advanceParticleRenderTime (getScene ().getFrameRenderTime (), m_time);

    // Native callback births already exist before the next age pass. Consume
    // their saved point state before this frame moves points or reclaims slots.
    if (m_pendingEmission > 0) update (0.0f);

    // Control-point motion uses elapsed frame time, before simulation rate and
    // the physics stability cap. Mutating owners retain native current/previous
    // state while held; their resolver runs only at a positive simulation step.
    if (!(m_hasMutatingRemapInitializer || m_hasControlPointOutputOperator) || (dt > 0.0f && getInstanceOverride ().rate->value->getFloat () > 0.0f))
        updateControlPoints (dt);

    if (m_particleParent != nullptr) {
	// Native dispatch gives children the parent's already rate-scaled interval,
	// including systems acquired during the current frame's emission.
	dt = m_particleParent->m_frameSimulationDelta;
    } else if (dt > 0.0f) {
	// Cap dt to prevent simulation instability
	// Also provides more consistent behavior across different FPS
	dt = std::min (dt, 0.1f);
	dt = calculateParticleSimulationDelta (dt, getInstanceOverride ().rate->value->getFloat ());
    }
    if (dt > 0.0f) {
	m_frameSimulationDelta = dt;
	m_simulationTime += dt;
	update (dt);
    }
    return true;
}

void CParticle::renderReflection (const glm::mat4& view, const glm::vec3& eye) {
    // Layer-image emitters read an earlier layer's composite during the ordinary traversal.
    if (!m_pass || m_particleParent != nullptr || (m_particle.flags & (1 | 4)) != 0
	|| !getScene ().getScene ().camera.projection.isPerspective
	|| !m_useTrailRenderer || m_useRopeRenderer || m_hasRefract
	|| !m_particle.children.empty () || !m_children.empty () || !m_followChildren.empty ()
	|| m_pass->getPass ().shader != "genericparticle" || m_pass->getBlendingMode () != BlendingMode_Additive
	|| std::ranges::any_of (m_particle.emitters, [](const ParticleEmitter& emitter) {
	    return emitter.name == "layerimage";
	})) return;

    const uint32_t frame = getScene ().getSceneFrame ();
    if (m_reflectionPreparedFrame != frame) {
	m_reflectionFrameVisible = updateForRender ();
	m_reflectionPreparedFrame = frame;
    }
    if (!m_reflectionFrameVisible || m_particleCount == 0) return;

    const ReflectionView reflection { view, eye };
    glColorMask (GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE);
    renderSprites (&reflection);
    glColorMask (GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
}

void CParticle::render () {
    // The planar pass may already have consumed manual births and advanced this frame.
    const bool visible = m_reflectionPreparedFrame == getScene ().getSceneFrame ()
	? m_reflectionFrameVisible : updateForRender ();
    if (!visible) return;

    // Render particles
    if (m_particleCount > 0 && m_particle.material) {
	// Scene alpha belongs to the compositor, just as for image/model draws.
	// Composition layers need the particle alpha for their later group blend.
	glColorMask (true, true, true, getScene ().isRenderingToComposition () ? GL_TRUE : GL_FALSE);
	if (m_useRopeRenderer) {
	    renderRope ();
	} else {
	    renderSprites ();
	}
	glColorMask (true, true, true, true);
    }
    for (auto& child : m_children) {
	child.renderer->m_propagateColorOverride = m_propagateColorOverride;
	child.renderer->render ();
    }
    for (auto& child : m_followChildren) {
	for (auto& instance : child.instances) {
	    if (instance.parentSerial == 0 && !instance.renderer->hasLivingParticles ()) continue;
	    instance.renderer->m_propagateColorOverride = m_propagateColorOverride;
	    instance.renderer->render ();
	    // Native releases completed event children immediately after their update.
	    if (instance.parentSerial != 0 && instance.renderer->m_simulationTime > 0.0f
		&& !instance.renderer->hasLivingParticles () && !instance.renderer->hasPendingEmission ()) {
		instance.parentSerial = 0;
		instance.renderer->pause ();
	    }
	}
    }
}

void CParticle::play () {
    m_emitting = true;
    for (auto& child : m_children) {
	child.renderer->play ();
    }
    for (auto& child : m_followChildren) {
	for (auto& instance : child.instances) {
	    if (instance.parentSerial != 0) instance.renderer->play ();
	}
    }
}

void CParticle::pause () {
    m_emitting = false;
    for (auto& child : m_children) {
	child.renderer->pause ();
    }
    for (auto& child : m_followChildren) {
	for (auto& instance : child.instances) instance.renderer->pause ();
    }
}

void CParticle::emitParticles (const uint32_t count) {
    if (m_hasMutatingRemapInitializer || m_hasControlPointOutputOperator) {
        // Point mutation and lifetime-zero slot reuse are synchronous per call;
        // summing separate script bursts changes the next invocation's state.
        m_pendingBirthTransform = m_controlPointTransform;
        m_pendingSystemTime = m_remapSystemTime;
        m_pendingLayerTime = remapLayerTime ();
        const uint32_t first = m_particleCount;
        m_initializingManualEmission = true;
        m_remapAllocationCursor = 0;
        for (auto& emitter : m_emitters) emitter (m_particles, m_particleCount, 0.0f, std::max (1u, count));
        m_initializingManualEmission = false;
        finalizeParticleBirths (first, first);
        finishManualParticleBirths (first);
        return;
    }
    if (count > 0 && m_pendingEmission == 0) {
	m_pendingControlPoints = m_controlPoints;
	m_pendingBirthTransform = m_controlPointTransform;
	m_pendingSystemTime = m_remapSystemTime;
	m_pendingLayerTime = remapLayerTime ();
    }
    m_pendingEmission += std::min (count, m_maxParticles - m_pendingEmission);
}

void CParticle::stop () {
    if (m_particleParent == nullptr) m_remapLayerTime = 0.0f;
    clearParticles (true);
}

void CParticle::clearParticles (const bool stopEmission) {
    m_eventParentSerial = 0;
    m_pendingEmission = 0;
    m_pendingControlPoints.clear ();
    if (stopEmission) m_emitting = false;
    m_remapSystemTime = 0.0f;
    m_particleCount = 0;
    if (usesRoundedParticleSlots ()) {
        // Native stop clears lifetime/extent while retaining position and velocity planes.
        for (auto& slot : m_remapSlots) { slot.lifetime = 0.0f; slot.alive = false; }
        m_boidsSlotExtent = 0;
    } else m_remapSlots.clear ();
    m_remapAllocationCursor = 0;
    m_remapNativeCount = 0;
    m_retiredParticleCount = 0;
    m_emitters.clear ();
    setupEmitters ();
    for (auto& child : m_children) {
	child.renderer->clearParticles (stopEmission);
    }
    for (auto& child : m_followChildren) {
	for (auto& instance : child.instances) {
	    // Native stop discards active children but keeps already-free pool slots.
	    // Our renderers stay allocated; the next use of an active slot is fresh.
	    if (instance.parentSerial == 0 && !instance.renderer->hasLivingParticles ()) continue;
	    instance.hasStarted = false;
	    instance.parentSerial = 0;
	    instance.renderer->stop ();
	}
    }
}

void CParticle::updateRemapClocks (const float frameDelta, const bool visible) {
    if (m_particleParent == nullptr) {
        // Native hidden-live clearing resets the layer and permanent systems,
        // while an already empty hidden layer retains its layer clock.
        if (!visible && (usesNativeParticleSlots () ? m_remapNativeCount : m_particleCount) != 0) {
            m_remapLayerTime = 0.0f;
            clearParticles (false);
        }
        m_remapDelta = frameDelta * getInstanceOverride ().rate->value->getFloat ();
        if (visible) m_remapLayerTime += m_remapDelta;
    } else {
        // Native permanent and active pooled children receive the same signed
        // parent interval; their own reset points keep independent clocks.
        m_remapDelta = m_particleParent->m_remapDelta;
    }
    m_remapSystemTime += m_remapDelta;
    if (!visible) {
        for (auto& child : m_children) child.renderer->updateRemapClocks (frameDelta, false);
        for (auto& child : m_followChildren) {
            for (auto& instance : child.instances) {
                if (instance.parentSerial != 0 || instance.renderer->hasLivingParticles ())
                    instance.renderer->updateRemapClocks (frameDelta, false);
            }
        }
    }
}

float CParticle::remapLayerTime () const {
    if (m_initializingManualEmission) return m_pendingLayerTime;
    const CParticle* root = this;
    while (root->m_particleParent != nullptr) root = root->m_particleParent;
    return root->m_initializingManualEmission ? root->m_pendingLayerTime : root->m_remapLayerTime;
}

bool CParticle::hasLivingParticles () const {
    if (m_particleCount != 0) return true;
    for (const auto& child : m_children) {
	if (child.renderer->hasLivingParticles ()) return true;
    }
    for (const auto& child : m_followChildren) {
	for (const auto& instance : child.instances) {
	    if (instance.renderer->hasLivingParticles ()) return true;
	}
    }
    return false;
}

bool CParticle::hasPendingEmission () const {
    // Pause suspends scheduled births; play must still find the retained slot.
    if (m_pendingEmission > 0 || m_pendingEmitterCount > 0) return true;
    for (const auto& child : m_children) {
	if (child.renderer->hasPendingEmission ()) return true;
    }
    for (const auto& child : m_followChildren) {
	for (const auto& instance : child.instances) {
	    if (instance.parentSerial != 0 && instance.renderer->hasPendingEmission ()) return true;
	}
    }
    return false;
}

void CParticle::spawnEventChild (FollowChildSystem& child, const ParticleInstance& particle) {
    if (WallpaperEngine::Maths::randomFloat (m_rng, 0.0f, 1.0f) >= child.settings->probability) return;
    const auto slot = std::find_if (child.instances.begin (), child.instances.end (), [] (const auto& instance) {
	return instance.parentSerial == 0 && !instance.renderer->hasLivingParticles ();
    });
    if (slot == child.instances.end ()) return;
    slot->parentSerial = particle.serial;
    auto& renderer = *slot->renderer;
    renderer.stop ();
    renderer.m_eventParentSerial = particle.serial;
    renderer.m_simulationTime = 0.0f;
    renderer.m_propagateColorOverride = m_propagateColorOverride;
    // Native samples raw overrides on first activation. Recycling retains
    // the cache, including any explicit root refresh since the previous use.
    const bool firstActivation = !slot->hasStarted;
    if (firstActivation) {
	renderer.activateInstanceParameters ();
	slot->hasStarted = true;
    }
    const glm::mat4 flipY = glm::scale (glm::mat4 (1.0f), glm::vec3 (1.0f, -1.0f, 1.0f));
    const glm::mat4 parentToLocal = glm::inverse (flipY * renderer.resolveWorldMatrix () * flipY);
    renderer.m_followPosition = glm::vec3 (parentToLocal * glm::vec4 (particle.position, 1.0f));
    renderer.m_time = getScene ().getFrameRenderTime ();
    renderer.play ();
    renderer.prewarm (firstActivation);
}

void CParticle::updateFollowChildren (const uint32_t firstNewParticle) {
    for (auto& child : m_followChildren) {
	if (child.settings->type == "eventdeath") {
	    continue;
	}
	for (uint32_t i = firstNewParticle; i < m_particleCount; ++i) {
	    if (m_particles[i].isAlive ()) spawnEventChild (child, m_particles[i]);
	}
	for (auto& instance : child.instances) {
	    if (instance.parentSerial == 0) continue;
	    const auto parent = std::find_if (
		m_particles.begin (), m_particles.begin () + m_particleCount, [&] (const auto& particle) {
		    return particle.serial == instance.parentSerial && particle.isAlive ();
		}
	    );
	    if (parent == m_particles.begin () + m_particleCount) {
		instance.parentSerial = 0;
		// Released child particles finish their lifetime at their own positions.
		instance.renderer->pause ();
	    } else {
		const glm::mat4 flipY = glm::scale (glm::mat4 (1.0f), glm::vec3 (1.0f, -1.0f, 1.0f));
		const glm::mat4 parentToLocal = glm::inverse (flipY * instance.renderer->resolveWorldMatrix () * flipY);
		auto& follower = *instance.renderer;
		const glm::vec3 follow = glm::vec3 (parentToLocal * glm::vec4 (parent->position, 1.0f));
		const glm::vec3 displacement = calculateFollowChildDisplacement (
		    follower.m_followPosition, follow, (follower.m_particle.flags & 1) != 0
		);
		// ponytail: rope-trail history is not shifted; add it when a local-space trail child needs it.
		for (uint32_t i = 0; i < follower.m_particleCount; ++i) follower.m_particles[i].position += displacement;
		follower.m_followPosition = follow;
	    }
	}
    }
}

glm::mat4 CParticle::particleWorldMatrix () {
    const glm::mat4 local = resolveWorldMatrix ();
    // Native sizes a world-space system's follow children without the layer scale (paired probe).
    return m_particleParent && !m_followsWorldParticle ? m_particleParent->particleWorldMatrix () * local : local;
}

void ControlPointData::sampleVelocity (const glm::vec3& simulationPosition, const float frameTime) {
    displacement = hasPreviousPosition ? simulationPosition - previousPosition : glm::vec3 (0.0f);
    velocity = frameTime > 0.0f ? displacement / frameTime : glm::vec3 (0.0f);
    previousPosition = simulationPosition;
    hasPreviousPosition = true;
}

void CParticle::includeControlPoint (const uint32_t index) {
    // Native allocates through the highest referenced slot. Definitions alone
    // do not make a point available for a linked child.
    m_controlPointCount = std::max (m_controlPointCount, std::min (index, 7u) + 1);
}

void CParticle::updateControlPoints (const float frameTime, const bool refreshOnly) {
    const CParticle* root = this;
    while (root->m_particleParent != nullptr) root = root->m_particleParent;
    const bool mutablePoints = m_hasMutatingRemapInitializer || m_hasControlPointOutputOperator;
    const glm::mat4 flipY = glm::scale (glm::mat4 (1.0f), glm::vec3 (1.0f, -1.0f, 1.0f));
    const glm::mat4 localToWorld = refreshOnly && mutablePoints
        ? flipY * m_commonControlPointTransform * flipY : particleWorldMatrix ();
    const glm::mat4 worldToLocal = glm::inverse (localToWorld);
    const bool worldSpace = (m_particle.flags & 1) != 0;
    const glm::vec2* mouse = getScene ().getMousePositionNormalized ();
    glm::vec3 mouseWorld (0.0f);
    if (mouse) {
	if (getScene ().getScene ().camera.projection.isPerspective) {
	    const auto& camera = getScene ().getCamera ();
	    const glm::vec4 unprojected = glm::inverse (camera.getProjection () * camera.getLookAt ())
		* glm::vec4 (mouse->x * 2.0f - 1.0f, mouse->y * 2.0f - 1.0f, 0.0f, 1.0f);
	    mouseWorld = glm::vec3 (glm::vec2 (unprojected) / unprojected.w, 0.0f);
	} else {
	    mouseWorld = { mouse->x * getScene ().getCamera ().getWidth (),
		mouse->y * getScene ().getCamera ().getHeight (), 0.0f };
	}
    }
    const glm::mat4 parentToLocal = m_particleParent
	? flipY * glm::inverse (resolveWorldMatrix ()) * flipY : glm::mat4 (1.0f);
    for (size_t i = 0; i < m_controlPoints.size (); ++i) {
	auto& cp = m_controlPoints[i];
        // Common refresh restores active authored overrides; ordinary local
        // resolution otherwise retains operator-written current matrices.
        if (refreshOnly && ((cp.flags & 0x10005) != 0 || i >= m_controlPointCount)) continue;
        const bool refresh = !cp.hasPreviousPosition || refreshOnly;
        if (!cp.hasPreviousPosition) cp.basePosition = cp.offset;
        bool activeOverride = false;
	if ((cp.flags & 1) != 0 && mouse) {
	    // Native 14022e3e0 replaces the translation with the cursor position;
	    // neither the definition offset nor an instance override is added to it.
	    cp.position = resolveParticleControlPoint (mouseWorld, worldToLocal, true);
	} else if ((cp.flags & 4) != 0 && m_particleParent && cp.parentControlPoint >= 0
	    && cp.parentControlPoint < static_cast<int> (m_particleParent->m_controlPointCount)) {
	    const auto& source = m_particleParent->m_controlPoints[cp.parentControlPoint];
	    cp.position = (cp.flags & 8) != 0 ? source.position
		: glm::vec3 (parentToLocal * glm::vec4 (source.position, 1.0f));
	    cp.orientation = (cp.flags & 8) != 0 ? source.orientation
		: glm::mat3 (parentToLocal) * source.orientation;
	} else {
	    glm::vec3 offset = mutablePoints ? cp.basePosition : cp.offset;
	    // Authored root offsets stay on that system. Script/user assignments
	    // propagate to descendants, including assignments of the same value.
	    const auto& overrides = root->m_particle.instanceOverride.controlPointOffsets;
	    if ((cp.flags & 5) == 0 && (!mutablePoints || refresh)) {
		if (const auto it = overrides.find (static_cast<int> (i)); it != overrides.end ()) {
		    if (this == root || root->m_controlPointOverridesChanged[i] || it->second->animation != nullptr) {
			const auto value = it->second->evaluateVec3 (getScene ().getElapsedTime ());
			if (value.x != std::numeric_limits<float>::max ()) {
                            offset = value;
                            activeOverride = true;
                        }
		    }
		}
	    }
	    // Native 14022a070 keeps point zero attached to a world-space emitter
	    // even when flagged as world. Other world points remain independent.
	    const bool worldOffset = (cp.flags & 2) != 0 && !(worldSpace && i == 0);
	    const auto& angles = root->m_particle.instanceOverride.controlPointAngles;
	    if ((cp.flags & 0x10005) == 0 && i < m_controlPointCount
                && (!mutablePoints || refresh)) {
		if (const auto it = angles.find (static_cast<int> (i)); it != angles.end ()) {
		    if (this == root || root->m_controlPointAnglesChanged || m_controlPointTimelines) {
			const auto value = it->second->evaluateVec3 (getScene ().getElapsedTime ());
			// Native stores radians, composes Z*Y*X, and retains the last
			// applied basis when the script restores the FLT_MAX sentinel.
			if (value.x != std::numeric_limits<float>::max ()) {
			    const glm::mat4 rotation = glm::rotate (glm::rotate (glm::rotate (
				glm::mat4 (1.0f), value.z, { 0.0f, 0.0f, 1.0f }),
				value.y, { 0.0f, 1.0f, 0.0f }), value.x, { 1.0f, 0.0f, 0.0f });
			    cp.baseOrientation = glm::mat3 (flipY * rotation * flipY);
                            activeOverride = true;
			}
		    }
		}
	    }
            if (mutablePoints) cp.basePosition = offset;
            if (refreshOnly && !activeOverride) continue;
            const bool resolve = !mutablePoints || !cp.hasPreviousPosition
                || (refresh && activeOverride) || (!refreshOnly && worldSpace != worldOffset);
            if (resolve) cp.position = resolveParticleControlPoint (offset, worldToLocal, worldOffset);
            else if (worldSpace) {
                cp.position = glm::vec3 (flipY * worldToLocal * flipY * glm::vec4 (cp.simulationPosition, 1.0f));
            }
	    cp.orientation = worldOffset ? glm::mat3 (flipY * worldToLocal * flipY) * cp.baseOrientation
		: cp.baseOrientation;
	}
        if (refreshOnly) {
            cp.simulationPosition = worldSpace
                ? glm::vec3 (flipY * localToWorld * flipY * glm::vec4 (cp.position, 1.0f)) : cp.position;
            if (worldSpace) {
                // A held refresh replaces only this current point. Express its
                // new matrix in the retained birth bridge alongside untouched points.
                const auto toBirth = glm::inverse (m_controlPointTransform);
                cp.position = glm::vec3 (toBirth * glm::vec4 (cp.simulationPosition, 1.0f));
                cp.orientation = glm::mat3 (toBirth * flipY * localToWorld * flipY) * cp.orientation;
            }
            cp.displacement = cp.simulationPosition - cp.birthPreviousPosition;
        }
    }
    if (refreshOnly) return;
    if (m_followPosition.has_value () && (m_controlPoints[0].flags & 5) == 0) {
	m_controlPoints[0].position = *m_followPosition;
    }
    const glm::mat4 particleToWorld = flipY * localToWorld * flipY;
    m_controlPointTransform = particleToWorld;
    const glm::mat3 worldToParticle = glm::mat3 (flipY * worldToLocal * flipY);
    for (auto& cp : m_controlPoints) {
        cp.simulationPosition = worldSpace ? glm::vec3 (particleToWorld * glm::vec4 (cp.position, 1.0f)) : cp.position;
        cp.birthPreviousPosition = cp.hasPreviousPosition ? cp.previousPosition : cp.simulationPosition;
        cp.sampleVelocity (cp.simulationPosition, frameTime);
	// Native initializer opcode 8 measures in simulation space, then undoes
	// the layer transform for local-authored points before the birth transform.
	if (worldSpace && (cp.flags & 2) == 0) {
	    cp.velocity = worldToParticle * cp.velocity;
	}
    }
}

void CParticle::update (float dt) {
    // Reclaim this frame's expired slots before emitting. A full one-particle
    // glow otherwise rejects its replacement, then disappears for one frame.
    if (dt > 0.0f && usesNativeParticleSlots ()) advanceRemapSlots (dt);
    uint32_t writeIdx = 0;
    for (uint32_t readIdx = 0; dt > 0.0f && readIdx < m_particleCount; ++readIdx) {
	auto& particle = m_particles[readIdx];
        if (usesNativeParticleSlots ()) {
            const auto& slot = m_remapSlots[particle.remapSlot];
            if (slot.serial != particle.serial) continue;
            particle.age = slot.age;
            particle.lifetime = slot.lifetime;
        } else {
            particle.age += dt;
        }
	if (particle.isAlive ()) {
	    if (writeIdx != readIdx) m_particles[writeIdx] = std::move (particle);
	    ++writeIdx;
	} else if (!usesNativeParticleSlots ()) {
	    ++m_retiredParticleCount;
	    // Death bursts keep the last simulated tip before this slot is reused.
	    for (auto& child : m_followChildren) {
		if (child.settings->type == "eventdeath") spawnEventChild (child, particle);
	    }
	}
    }
    if (dt > 0.0f) m_particleCount = writeIdx;

    // Pausing stops new emission; bubbles already released keep moving and aging.
    const uint32_t firstNewParticle = m_particleCount;
    if (m_emitting && dt > 0.0f) {
	m_pendingEmitterCount = 0;
        m_remapAllocationCursor = 0;
	for (auto& emitter : m_emitters) {
	    emitter (m_particles, m_particleCount, dt, 0);
	}
    }
    const uint32_t firstManualParticle = m_particleCount;
    if (m_pendingEmission > 0) {
	// Native emitParticles() uses the matrices resolved before its callback.
	// Queue that same point state, even if the callback also changes a point.
	m_controlPoints.swap (m_pendingControlPoints);
	m_initializingManualEmission = true;
        m_remapAllocationCursor = 0;
	for (auto& emitter : m_emitters) emitter (m_particles, m_particleCount, 0.0f, m_pendingEmission);
	m_initializingManualEmission = false;
	m_controlPoints.swap (m_pendingControlPoints);
	m_pendingControlPoints.clear ();
	m_pendingEmission = 0;
    }
    finalizeParticleBirths (firstNewParticle, firstManualParticle);

    std::vector<ControlPointData> worldControlPoints;
    const auto* operatorControlPoints = &m_controlPoints;
    if ((m_particle.flags & 1) != 0) {
	// Native 1402378a0/14023b340 transform births and initial velocity into
	// world space. Size and subsequent forces stay in world units; applying
	// the emitter model again made scaled meteor trails too wide and steep.
	const glm::mat4 flipY = glm::scale (glm::mat4 (1.0f), glm::vec3 (1.0f, -1.0f, 1.0f));
	const glm::mat4 localToWorld = flipY * particleWorldMatrix () * flipY;
	// Emitters and child inheritance still need layer-local points. Operators
	// act in the same space as the already spawned particles.
	worldControlPoints = m_controlPoints;
	for (auto& point : worldControlPoints) {
            point.position = (m_hasMutatingRemapInitializer || m_hasControlPointOutputOperator) ? point.simulationPosition
                : glm::vec3 (localToWorld * glm::vec4 (point.position, 1.0f));
	}
	operatorControlPoints = &worldControlPoints;
    }

    if (dt <= 0.0f) {
        finishManualParticleBirths (firstNewParticle);
        return;
    }

    // Native 140237754 repeats only operator dispatch at configured FPS 1..20.
    // Aging, emission, simulation time and control-point history advance once.
    const int fps = getScene ().getContext ().getApp ().getContext ().settings.render.maximumFPS;
    const int substeps = fps >= 1 && fps <= 20 ? 2 : 1;
    const float operatorDt = dt / static_cast<float> (substeps);
    if (usesNativeParticleSlots ()) synchronizeRemapSlots ();
    auto& simulatedParticles = usesNativeParticleSlots () ? m_remapSlots : m_particles;
    const uint32_t simulatedCount = usesRoundedParticleSlots () ? (m_boidsSlotExtent + 3u) & ~3u
        : m_hasControlPointOutputOperator ? m_remapSlots.size () : m_particleCount;
    const uint32_t restoredCount = usesRoundedParticleSlots () ? m_boidsSlotExtent : simulatedCount;
    for (int step = 0; step < substeps; ++step) {
	// Each native dispatch restores birth color/alpha/size before operators.
	for (uint32_t i = 0; i < restoredCount; ++i) {
	    if (m_hasColorOperators) simulatedParticles[i].color = simulatedParticles[i].initial.color;
	    if (m_hasAlphaOperators) simulatedParticles[i].alpha = simulatedParticles[i].initial.alpha;
	    simulatedParticles[i].size = simulatedParticles[i].initial.size;
	}
	for (auto& op : m_operators) {
	    op (simulatedParticles, simulatedCount, *operatorControlPoints, m_simulationTime, operatorDt);
            if (m_hasControlPointOutputOperator && !worldControlPoints.empty ()) {
                for (size_t index = 0; index < worldControlPoints.size (); ++index) {
                    worldControlPoints[index].position = m_controlPoints[index].simulationPosition;
                    worldControlPoints[index].displacement = m_controlPoints[index].displacement;
                }
            }
	}
    }

    if (usesNativeParticleSlots ()) publishRemapSlots ();

    updateRopeTrailHistory (dt);
    updateFollowChildren (firstNewParticle);
    if (m_hasMutatingRemapInitializer || m_hasControlPointOutputOperator) {
        // Native publishes previous matrices after emission and operators.
        for (auto& point : m_controlPoints) {
            point.previousPosition = point.simulationPosition;
            point.birthPreviousPosition = point.simulationPosition;
        }
    }

}

void CParticle::advanceRemapSlots (const float dt) {
    synchronizeRemapSlots ();
    const size_t extent = usesRoundedParticleSlots () ? (m_boidsSlotExtent + 3u) & ~3u : m_remapSlots.size ();
    for (size_t index = 0; index < extent; ++index) {
        auto& slot = m_remapSlots[index];
        slot.age += dt;
        if (slot.lifetime != 0.0f && slot.lifetime < slot.age) {
            slot.lifetime = 0.0f;
            --m_remapNativeCount;
            ++m_retiredParticleCount;
            for (auto& child : m_followChildren) {
                if (child.settings->type == "eventdeath") spawnEventChild (child, slot);
            }
        }
        // A rewritten inactive lifetime participates in native retirement, but
        // does not recreate a dense particle or its follower identity.
        if (slot.lifetime == 0.0f) slot.alive = false;
    }
}

uint32_t CParticle::acquireRemapSlot () {
    if (m_remapNativeCount >= m_maxParticles) return m_maxParticles;
    const uint32_t extent = usesRoundedParticleSlots () ? m_boidsSlotExtent : m_remapSlots.size ();
    for (uint32_t index = m_remapAllocationCursor; index < extent; ++index) {
        if (m_remapSlots[index].lifetime == 0.0f) {
            m_remapAllocationCursor = index + 1;
            ++m_remapNativeCount;
            return index;
        }
    }
    if (extent == m_maxParticles) return m_maxParticles;
    const uint32_t index = extent;
    if (usesRoundedParticleSlots ()) ++m_boidsSlotExtent;
    else {
        m_remapSlots.emplace_back ();
        m_remapSlots.back ().lifetime = 0.0f;
    }
    m_remapAllocationCursor = index + 1;
    ++m_remapNativeCount;
    return index;
}

void CParticle::synchronizeRemapSlots () {
    for (uint32_t index = 0; index < m_particleCount; ++index) {
        const auto& particle = m_particles[index];
        auto& slot = m_remapSlots[particle.remapSlot];
        if (slot.serial != particle.serial) continue;
        slot = particle;
        slot.nativeSlotState = true;
        slot.trailHistory.clear ();
    }
}

void CParticle::publishRemapSlots () {
    // Membership remains the dense live view, including same-tick deletions.
    // A lifetime remap of a retired slot does not invent a new live particle.
    for (uint32_t index = 0; index < m_particleCount; ++index) {
        auto& particle = m_particles[index];
        if (m_remapSlots[particle.remapSlot].serial != particle.serial) continue;
        auto history = std::move (particle.trailHistory);
        const auto frame = particle.trailLastFrame;
        const bool valid = particle.trailLastFrameValid;
        particle = m_remapSlots[particle.remapSlot];
        particle.trailHistory = std::move (history);
        particle.trailLastFrame = frame;
        particle.trailLastFrameValid = valid;
    }
}

void CParticle::finalizeParticleBirths (const uint32_t firstNewParticle, const uint32_t firstManual) {
    for (uint32_t i = firstNewParticle; i < m_particleCount; ++i) {
        m_particles[i].serial = ++m_nextParticleSerial;
        if (usesNativeParticleSlots ())
            m_remapSlots[m_particles[i].remapSlot].serial = m_particles[i].serial;
    }
    if ((m_particle.flags & 1) == 0) return;
    const glm::mat4 flipY = glm::scale (glm::mat4 (1.0f), glm::vec3 (1.0f, -1.0f, 1.0f));
    const glm::mat4 localToWorld = firstNewParticle < firstManual
        ? flipY * particleWorldMatrix () * flipY : m_pendingBirthTransform;
    for (uint32_t i = firstNewParticle; i < m_particleCount; ++i) {
        auto& particle = m_particles[i];
        const auto& transform = i < firstManual ? localToWorld : m_pendingBirthTransform;
        particle.position = glm::vec3 (transform * glm::vec4 (particle.position, 1.0f));
        particle.previousPosition = glm::vec3 (transform * glm::vec4 (particle.previousPosition, 1.0f));
        particle.velocity = glm::mat3 (transform) * particle.velocity;
    }
}

void CParticle::finishManualParticleBirths (const uint32_t firstNewParticle) {
	// Native emitParticles() invokes birth initializers and children, not the
	// simulation operators (some of which change alpha/size even at zero dt).
	// Birth size is separate from the working half-size: the first positive
	// simulation step copies it, while a held new particle still displays .5.
	for (uint32_t i = firstNewParticle; i < m_particleCount; ++i) {
	    auto& particle = m_particles[i];
	    particle.size = 0.5f;
	    // Native separates working alpha/color only when an operator needs it.
	    if (m_hasAlphaOperators) particle.alpha = getInstanceOverride ().alpha->value->getFloat ();
	    if (m_hasColorOperators) particle.color = particle.emitterColor;
	}
	updateFollowChildren (firstNewParticle);
}

void CParticle::updateRopeTrailHistory (float dt) {
    if (!m_useRopeTrailRenderer || dt <= 0.0f) {
	return;
    }

    const int maxSegments = std::max (2, m_ropeSegments);
    const float historyLength = std::max (m_trailLength, 0.001f);
    const float sampleInterval = historyLength / static_cast<float> (maxSegments);
    const float timerBeforeFrame = m_ropeHistoryTimer;
    const float accumulatedTime = timerBeforeFrame + dt;
    const int sampleCount = static_cast<int> (std::floor (accumulatedTime / sampleInterval));
    const int firstSample = std::max (0, sampleCount - maxSegments);

    auto snapshot = [] (const ParticleInstance& particle) {
	return ParticleInstance::TrailPoint { .position = particle.position };
    };

    auto interpolate = [] (const ParticleInstance::TrailPoint& from, const ParticleInstance::TrailPoint& to, float t) {
	return ParticleInstance::TrailPoint { .position = glm::mix (from.position, to.position, t) };
    };

    for (uint32_t i = 0; i < m_particleCount; i++) {
	auto& particle = m_particles[i];
	if (!particle.isAlive ()) {
	    continue;
	}

	const auto current = snapshot (particle);
	if (!particle.trailLastFrameValid) {
	    particle.trailHistory.clear ();
	    particle.trailHistory.reserve (static_cast<size_t> (maxSegments));
	    particle.trailHistory.push_back (current);
	    particle.trailLastFrame = current;
	    particle.trailLastFrameValid = true;
	    continue;
	}

	for (int sample = firstSample; sample < sampleCount; sample++) {
	    const float sampleOffset = sampleInterval - timerBeforeFrame + static_cast<float> (sample) * sampleInterval;
	    const float frameFraction = std::clamp (sampleOffset / dt, 0.0f, 1.0f);
	    particle.trailHistory.push_back (interpolate (particle.trailLastFrame, current, frameFraction));
	    if (static_cast<int> (particle.trailHistory.size ()) > maxSegments) {
		particle.trailHistory.erase (particle.trailHistory.begin ());
	    }
	}

	particle.trailLastFrame = current;
    }

    m_ropeHistoryTimer = std::fmod (accumulatedTime, sampleInterval);
}

const Particle& CParticle::getParticle () const { return m_particle; }

const float& CParticle::getBrightness () const { return m_overbright; }

const float& CParticle::getUserAlpha () const { return getInstanceOverride ().alpha->value->getFloat (); }

const float& CParticle::getAlpha () const { return getInstanceOverride ().alpha->value->getFloat (); }

const glm::vec3& CParticle::getColor () const {
    static const glm::vec3 defaultColor (1.0f);
    return defaultColor;
}

const glm::vec4& CParticle::getColor4 () const {
    static const glm::vec4 defaultColor (1.0f);
    return defaultColor;
}

const glm::vec3& CParticle::getCompositeColor () const { return getColor (); }

// ========== EMITTERS ==========

void CParticle::setupEmitters () {
    m_pendingEmitterCount = 0;
    uint32_t imageIndex = 0;
    for (const auto& emitter : m_particle.emitters) {
	EmitterFunc func;

	if (emitter.name == "boxrandom") {
	    func = createBoxEmitter (emitter);
	} else if (emitter.name == "sphererandom") {
	    func = createSphereEmitter (emitter);
	} else if (emitter.name == "layerimage") {
	    func = createLayerImageEmitter (emitter, imageIndex++);
	} else {
	    sLog.out ("Unknown emitter type: ", emitter.name);
	    continue;
	}

	if (func) {
	    if (m_ropeEmitterRate == 0.0f) {
		m_ropeEmitterRate = emitter.rate;
	    }
	    if (emitter.instantaneous > 0 || emitter.rate > 0.0f) ++m_pendingEmitterCount;
	    m_emitters.push_back (std::move (func));
	}
    }
}

glm::vec3 CParticle::resolveEmitterOrigin (const glm::vec3& origin) const {
    if ((m_particle.flags & 1) == 0 || origin == glm::vec3 (0.0f)) return origin;
    const glm::mat3 basis (m_initializingManualEmission ? m_pendingBirthTransform : m_controlPointTransform);
    // World emitter offsets bypass the layer basis. Undo it before initializers
    // sample the birth position; the common birth path applies it afterward.
    // A collapsed basis cannot represent this offset in local coordinates.
    return glm::determinant (basis) == 0.0f ? origin : glm::inverse (basis) * origin;
}

EmitterFunc CParticle::createBoxEmitter (const ParticleEmitter& definition) {
    ParticleEmitter emitter = definition;
    if (getScene ().getScene ().camera.projection.isPerspective) {
	if (emitter.defaultDirections) emitter.directions = glm::vec3 (1.0f);
	if (emitter.defaultDistanceMax) emitter.distanceMax = glm::vec3 (1.0f);
    }
    includeControlPoint (emitter.controlPoint);
    DynamicValue* countOverride = getInstanceOverride ().count->value.get ();

    glm::vec3 transformedEmitterOrigin = emitter.origin;
    transformedEmitterOrigin.y = -transformedEmitterOrigin.y;

    const int controlPointIndex = emitter.controlPoint;

    glm::vec3 flippedDirections = emitter.directions;
    flippedDirections.y = -flippedDirections.y;

    bool limitOnePerFrame = (emitter.flags & 2) != 0;
    bool randomPeriodicEmission = (emitter.flags & 4) != 0;

    return
	[this, emitter, transformedEmitterOrigin, controlPointIndex, countOverride, flippedDirections, limitOnePerFrame,
	 randomPeriodicEmission, emissionTimer = 0.0f, delayTimer = emitter.delay, durationTimer = 0.0f,
	 periodicTimer = 0.0f, emittedThisPeriod = 0u,
	 instantaneousEmitted = false] (std::vector<ParticleInstance>& particles, uint32_t& count, float dt, uint32_t burst) mutable {
	    const Data::Utils::ScopeGuard pending ([&] {
		if (burst == 0 && (emitter.duration <= 0.0f || durationTimer < emitter.duration)
		    && ((!instantaneousEmitted && emitter.instantaneous > 0) || emitter.rate > 0.0f))
		    ++m_pendingEmitterCount;
	    });
	    if (burst == 0) {
		// Handle delay
		if (delayTimer > 0.0f) {
		    delayTimer -= dt;
		    return;
		}

		// Native advances duration even with a full pool, after admitting this tick.
		if (emitter.duration > 0.0f) {
		    if (durationTimer >= emitter.duration) {
			return;
		    }
		    durationTimer += dt;
		}
	    }
	    if (count >= particles.size ()) {
		return;
	    }

	    uint32_t toEmit = burst;
	    if (burst == 0) {
		// Native uses a positive active clock and a negative waiting clock.
		if (randomPeriodicEmission) {
		    if (periodicTimer > 0.0f) {
			periodicTimer -= dt;
			if (periodicTimer < 0.0f) {
			    periodicTimer = -WallpaperEngine::Maths::randomFloat (
				m_rng, emitter.minPeriodicDelay, emitter.maxPeriodicDelay
			    );
			}
			// The tick crossing into the wait still admits scheduled births.
		    } else {
			periodicTimer += dt;
			if (periodicTimer < 0.0f) {
			    return;
			}
			periodicTimer = WallpaperEngine::Maths::randomFloat (
			    m_rng, emitter.minPeriodicDuration, emitter.maxPeriodicDuration
			);
			emittedThisPeriod = 0;
		    }
		}

		// Handle instantaneous emission
		toEmit = 0;
		if (emitter.instantaneous > 0 && !instantaneousEmitted) {
		    toEmit = emitter.instantaneous;
		    instantaneousEmitted = true;
		}

		// Rate-based emission with optional cap at 1 per frame
		if (emitter.rate > 0.0f) {
		    const auto& recorder = getScene ().getAudioContext ().getRecorder ();
		    const float response = calculateParticleAudioResponse (
			recorder.audio16Left, recorder.audio16Right, emitter.audioProcessingMode,
			emitter.audioProcessingBounds, emitter.audioProcessingExponent,
			emitter.audioProcessingFrequencyStart, emitter.audioProcessingFrequencyEnd
		    );
		    const float countMultiplier = (m_particle.flags & 32) != 0 ? 1.0f : countOverride->getFloat ();
		    const float rate = calculateParticleEmissionRate (emitter.rate, countMultiplier) * response;
		    emissionTimer += dt * rate;
		    uint32_t rateEmit = static_cast<uint32_t> (emissionTimer);
		    emissionTimer -= static_cast<float> (rateEmit);
		    // limitOnePerFrame (flags bit 1): cap at 1 to prevent rope artifacts
		    if (limitOnePerFrame && rateEmit > 1) {
			rateEmit = 1;
		    }
		    if (randomPeriodicEmission && emitter.maxToEmitPerPeriod != 0) {
			uint32_t cap = emitter.maxToEmitPerPeriod;
			if ((m_particle.flags & 32) == 0) {
			    const float scaled = static_cast<float> (cap) * countMultiplier;
			    // Native truncates to signed64, then retains the low32 bits.
			    // Its invalid-conversion sentinel also has zero low bits.
			    cap = scaled >= -0x1p63f && scaled < 0x1p63f
				? static_cast<uint32_t> (static_cast<int64_t> (scaled)) : 0;
			}
			if (cap != 0) {
			    const uint32_t remaining = cap > emittedThisPeriod ? cap - emittedThisPeriod : 0;
			    rateEmit = std::min (rateEmit, remaining);
			    emittedThisPeriod += rateEmit;
			}
		    }
		    toEmit += rateEmit;
		}

	    }
	    const glm::vec3 emitterOrigin = resolveEmitterOrigin (transformedEmitterOrigin);
            glm::vec3 spawnOrigin = emitterOrigin;
            if (controlPointIndex >= 0 && controlPointIndex < static_cast<int> (m_controlPoints.size ())) {
                spawnOrigin += m_controlPoints[controlPointIndex].position;
            }
	    // Native caches the origin before the birth loop; initializers may mutate it.
	    for (uint32_t i = 0; i < toEmit && count < particles.size (); i++) {
                const uint32_t slot = usesNativeParticleSlots () ? acquireRemapSlot () : 0;
                if (usesNativeParticleSlots () && slot == m_maxParticles) break;
		auto& p = particles[count];
                p.remapSlot = slot;
                p.nativeSlotState = usesNativeParticleSlots ();

		// Generate random position with directions applied before the inner distance.
		// Native preserves that distance when an axis direction has non-unit magnitude.
		glm::vec3 randomPos;
		for (int axis = 0; axis < 3; axis++) {
		    const float sample = WallpaperEngine::Maths::randomFloat (m_rng, -1.0f, 1.0f)
			* flippedDirections[axis];
		    randomPos[axis] = glm::sign (sample) * (std::abs (sample)
			* (emitter.distanceMax[axis] - emitter.distanceMin[axis]) + emitter.distanceMin[axis]);
		}
		if (controlPointIndex >= 0 && controlPointIndex < static_cast<int> (m_controlPoints.size ())
		    && ((m_particle.flags & 1) != 0 || controlPointIndex != 0)) {
		    // Only the sampled volume is oriented; the authored origin stays outside.
		    randomPos = m_controlPoints[controlPointIndex].orientation * randomPos;
		}

		p.position = spawnOrigin + randomPos;
		p.previousPosition = p.position;

		p.velocity = glm::vec3 (0.0f);
		if (emitter.speedMin != 0.0f || emitter.speedMax != 0.0f) {
		    glm::vec3 direction = randomPos;
		    if (glm::length (direction) < 0.0001f) {
			direction = WallpaperEngine::Maths::randomVec3 (m_rng, glm::vec3 (-1.0f), glm::vec3 (1.0f))
				* flippedDirections;
			if (controlPointIndex >= 0 && controlPointIndex < static_cast<int> (m_controlPoints.size ())
				&& ((m_particle.flags & 1) != 0 || controlPointIndex != 0)) {
			    direction = m_controlPoints[controlPointIndex].orientation * direction;
			}
		    }
		    if (glm::dot (direction, direction) > 0.0f) direction = glm::normalize (direction);
		    const float speed = WallpaperEngine::Maths::randomFloat (m_rng, emitter.speedMin, emitter.speedMax);
		    const float speedMultiplier = (m_particle.flags & 16) != 0 ? 1.0f : getInstanceOverride ().speed->value->getFloat ();
		    p.velocity = direction * speed * speedMultiplier;
		}
		p.acceleration = glm::vec3 (0.0f);
		p.rotation = glm::vec3 (0.0f);
		p.angularVelocity = glm::vec3 (0.0f);
		p.angularAcceleration = glm::vec3 (0.0f);

		p.color = m_instanceColor;
		p.emitterColor = p.color;
		p.alpha = 1.0f * getInstanceOverride ().alpha->value->getFloat ();
		p.size = 0.5f;
		p.lifetime = 1.0f;
		p.age = 0.0f;
		p.alive = true;

		p.initial.color = p.color;
		p.initial.alpha = p.alpha;
		p.initial.size = p.size;
		p.initial.lifetime = p.lifetime;

		// Reset oscillator state for reused particles. Native publishes randomframe
		// phase before initializer dispatch, shared with later animation/oscillation.
		p.oscillationRandom = m_particle.animationMode == "randomframe"
		    ? WallpaperEngine::Maths::randomFloat (m_rng, 0.0f, 1.0f) : -1.0f;
		p.trailHistory.clear ();
		p.trailLastFrameValid = false;

		// Apply initializers
		for (auto& init : m_initializers) {
		    init (p);
		}

                if (usesNativeParticleSlots ()) m_remapSlots[slot] = p;
		count++;
	    }
	};
}

bool CParticle::updateLayerImageCache (LayerImageCache& cache) {
    const auto* sourceLayer = dynamic_cast<const CImage*> (getScene ().getObject (cache.sourceId));
    if (sourceLayer != cache.source) return false;
    const auto& image = cache.source->getImage ();
    const auto texture = cache.source->getTexture ();
    // The first slice is an ordinary static textured layer. Other native source
    // types and dynamic cache refresh have separate sampling contracts.
    if (!texture || !texture->isReady () || texture->isAnimated ()
	|| (texture->getFlags () & (TextureFlags_Video | TextureFlags_Volume)) != 0 || !image.model
	|| image.model->solidlayer || image.model->fullscreen || image.model->passthrough
	|| image.model->puppet || cache.source->isCompositionLayer () || !image.effects.empty ()) return false;
    const auto source = cache.source->find ("_rt_imageLayerComposite_" + std::to_string (image.id) + "_a");
    if (!source || !source->isReady ()
	|| (source->getFlags () & (TextureFlags_NoInterpolation | TextureFlags_ClampUVs | TextureFlags_ClampUVsBorder))
	    != TextureFlags_ClampUVs) return false;
    const glm::uvec2 extent (source->getRealWidth (), source->getRealHeight ());
    if (extent.x > 3840 || extent.y > 2160) return false;
    if (cache.ready) return cache.extent == extent && !cache.samples.empty ();

    GLint drawFramebuffer, readFramebuffer, viewport[4], vao, program, arrayBuffer;
    GLint packBuffer, packAlignment, packRowLength, packSkipRows, packSkipPixels;
    GLint activeTexture, texture0, sampler0;
    glGetIntegerv (GL_DRAW_FRAMEBUFFER_BINDING, &drawFramebuffer);
    glGetIntegerv (GL_READ_FRAMEBUFFER_BINDING, &readFramebuffer);
    glGetIntegerv (GL_VIEWPORT, viewport);
    glGetIntegerv (GL_VERTEX_ARRAY_BINDING, &vao);
    glGetIntegerv (GL_CURRENT_PROGRAM, &program);
    glGetIntegerv (GL_ARRAY_BUFFER_BINDING, &arrayBuffer);
    glGetIntegerv (GL_PIXEL_PACK_BUFFER_BINDING, &packBuffer);
    glGetIntegerv (GL_PACK_ALIGNMENT, &packAlignment);
    glGetIntegerv (GL_PACK_ROW_LENGTH, &packRowLength);
    glGetIntegerv (GL_PACK_SKIP_ROWS, &packSkipRows);
    glGetIntegerv (GL_PACK_SKIP_PIXELS, &packSkipPixels);
    glGetIntegerv (GL_ACTIVE_TEXTURE, &activeTexture);
    glActiveTexture (GL_TEXTURE0);
    glGetIntegerv (GL_TEXTURE_BINDING_2D, &texture0);
    glGetIntegeri_v (GL_SAMPLER_BINDING, 0, &sampler0);
    const GLboolean scissor = glIsEnabled (GL_SCISSOR_TEST);
    const Data::Utils::ScopeGuard restore ([&] {
	glBindFramebuffer (GL_DRAW_FRAMEBUFFER, drawFramebuffer);
	glBindFramebuffer (GL_READ_FRAMEBUFFER, readFramebuffer);
	glViewport (viewport[0], viewport[1], viewport[2], viewport[3]);
	glBindVertexArray (vao);
	glUseProgram (program);
	glBindBuffer (GL_ARRAY_BUFFER, arrayBuffer);
	glBindBuffer (GL_PIXEL_PACK_BUFFER, packBuffer);
	glPixelStorei (GL_PACK_ALIGNMENT, packAlignment);
	glPixelStorei (GL_PACK_ROW_LENGTH, packRowLength);
	glPixelStorei (GL_PACK_SKIP_ROWS, packSkipRows);
	glPixelStorei (GL_PACK_SKIP_PIXELS, packSkipPixels);
	glActiveTexture (GL_TEXTURE0);
	glBindTexture (GL_TEXTURE_2D, texture0);
	glBindSampler (0, sampler0);
	glActiveTexture (activeTexture);
	if (scissor) glEnable (GL_SCISSOR_TEST);
    });
    glDisable (GL_SCISSOR_TEST);
    glBindSampler (0, 0);
    const glm::uvec2 stagingExtent = glm::max (extent / 4u, glm::uvec2 (2));
    auto staging = std::make_shared<CFBO> (
	"_rt_layerImageSamples_" + std::to_string (getId ()), TextureFormat_ARGB8888,
	TextureFlags_NoFlags, 1.0f, stagingExtent.x, stagingExtent.y, stagingExtent.x, stagingExtent.y
    );
    MaterialPass material {
	.blending = BlendingMode_Normal, .cullmode = CullingMode_Disable,
	.depthtest = DepthtestMode_Disabled, .depthwrite = DepthwriteMode_Disabled,
	.shader = "downsample_quarter", .combos = { { "WRITEALPHA", 1 }, { "OPACITYMASK", 0 } }
    };
    const TextureMap binds { { 0, "previous" } };
    Effects::CPass pass (*this, std::make_shared<FBOProvider> (this), material,
	std::nullopt, std::cref (binds), std::nullopt, {}, true);
    pass.setInput (source);
    pass.setDestination (staging);
    pass.setPosition (cache.source->getPassSpacePosition ());
    pass.setTexCoord (cache.source->getTexCoordPass ());
    pass.initialize ();
    pass.render ();
    glBindFramebuffer (GL_READ_FRAMEBUFFER, staging->getFramebuffer ());
    glBindBuffer (GL_PIXEL_PACK_BUFFER, 0);
    glPixelStorei (GL_PACK_ALIGNMENT, 1);
    glPixelStorei (GL_PACK_ROW_LENGTH, 0);
    glPixelStorei (GL_PACK_SKIP_ROWS, 0);
    glPixelStorei (GL_PACK_SKIP_PIXELS, 0);
    std::vector<uint8_t> pixels (static_cast<size_t> (stagingExtent.x) * stagingExtent.y * 4);
    glReadPixels (0, 0, stagingExtent.x, stagingExtent.y, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data ());
    cache.samples = buildParticleLayerImageSamples (pixels, stagingExtent, extent);
    cache.extent = extent;
    cache.ready = true;
    return !cache.samples.empty ();
}

EmitterFunc CParticle::createLayerImageEmitter (const ParticleEmitter& emitter, const uint32_t imageIndex) {
    // Refresh, motion inheritance and masks remain separate paths.
    if ((emitter.flags & ~(65536u | 524288u)) != 0) return {};
    const bool perspective = getScene ().getScene ().camera.projection.isPerspective;
    const glm::vec3 offsetMin = emitter.offsetMin.value_or (
	perspective ? glm::vec3 (0.0f) : glm::vec3 (-5.0f, -5.0f, 0.0f)
    );
    const glm::vec3 offsetMax = emitter.offsetMax.value_or (
	perspective ? glm::vec3 (0.0f) : glm::vec3 (5.0f, 5.0f, 0.0f)
    );
    const glm::vec3 offsetSpan = offsetMax - offsetMin;
    auto cache = std::make_shared<LayerImageCache> ();
    const ObjectDependency* binding = nullptr;
    for (const auto& dependency : m_particle.typedDependencies) {
	if (dependency.id != 0 && dependency.type == "emitterimage" && dependency.index == imageIndex)
	    binding = &dependency;
    }
    if (!binding || !binding->mask.empty ()) return {};
    cache->sourceId = binding->id;
    cache->source = dynamic_cast<const CImage*> (getScene ().getObject (cache->sourceId));
    if (!cache->source) return {};
    DynamicValue* countOverride = getInstanceOverride ().count->value.get ();
    return [this, emitter, cache, countOverride, offsetMin, offsetSpan, emissionTimer = 0.0f,
	    delayTimer = emitter.delay, durationTimer = 0.0f, remaining = emitter.instantaneous]
	(std::vector<ParticleInstance>& particles, uint32_t& count, float dt, uint32_t burst) mutable {
	const Data::Utils::ScopeGuard pending ([&] {
	    if (burst == 0 && (emitter.duration <= 0.0f || durationTimer < emitter.duration)
		&& (remaining > 0 || emitter.rate > 0.0f)) ++m_pendingEmitterCount;
	});
	uint32_t toEmit = count < particles.size () ? burst : 0;
	if (delayTimer > 0.0f) {
	    delayTimer -= dt;
	} else if (emitter.duration <= 0.0f || durationTimer < emitter.duration) {
	    if (emitter.duration > 0.0f) durationTimer += dt;
	    if (count >= particles.size ()) return;
	    toEmit += remaining;
	    remaining = 0;
	    const auto& recorder = getScene ().getAudioContext ().getRecorder ();
	    const float response = calculateParticleAudioResponse (
		recorder.audio16Left, recorder.audio16Right, emitter.audioProcessingMode,
		emitter.audioProcessingBounds, emitter.audioProcessingExponent,
		emitter.audioProcessingFrequencyStart, emitter.audioProcessingFrequencyEnd
	    );
	    const float multiplier = (m_particle.flags & 32) != 0 ? 1.0f : countOverride->getFloat ();
	    emissionTimer += dt * calculateParticleEmissionRate (emitter.rate, multiplier) * response;
	    const uint32_t rateCount = emissionTimer >= 1.0f ? static_cast<uint32_t> (emissionTimer) : 0;
	    // Native consumes requested instantaneous/manual births as well as the
	    // rate floor before cache availability or pool capacity suppress births.
	    emissionTimer -= static_cast<float> (rateCount) + static_cast<float> (toEmit);
	    toEmit += rateCount;
	}
	if (toEmit == 0 || count >= particles.size () || !updateLayerImageCache (*cache)) return;
	const glm::mat4 flipY = glm::scale (glm::mat4 (1.0f), glm::vec3 (1.0f, -1.0f, 1.0f));
	const glm::mat4 birth = m_initializingManualEmission ? m_pendingBirthTransform
	    : flipY * particleWorldMatrix () * flipY;
	if (glm::determinant (birth) == 0.0f) return;
	// Keep the fork's initializer-local birth representation. Common finalization
	// reapplies birth for world particles, yielding the native source world point.
	const glm::mat4 sourceToBirth = glm::inverse (birth)
	    * flipY * cache->source->resolveLayerModelMatrix ();
	std::uniform_int_distribution<size_t> select (0, cache->samples.size () - 1);
	for (uint32_t i = 0; i < toEmit && count < particles.size (); ++i) {
	    const uint32_t slot = usesNativeParticleSlots () ? acquireRemapSlot () : 0;
	    if (usesNativeParticleSlots () && slot == m_maxParticles) break;
	    auto& p = particles[count];
	    p.remapSlot = slot;
	    p.nativeSlotState = usesNativeParticleSlots ();
	    const auto& sample = cache->samples[select (m_rng)];
	    glm::vec3 position (sample.x, -sample.y, 0.0f);
	    if ((emitter.flags & 524288) != 0) {
		// Offsets share the native sample basis and precede the source transform.
		for (int axis = 0; axis < 3; ++axis) {
		    float offset = WallpaperEngine::Maths::randomFloat (m_rng, 0.0f, 1.0f) * offsetSpan[axis];
		    offset += offsetMin[axis];
		    position[axis] += offset;
		}
	    }
	    p.position = glm::vec3 (sourceToBirth * glm::vec4 (position, 1.0f));
	    p.previousPosition = p.position;
	    p.velocity = glm::vec3 (0.0f);
	    p.acceleration = glm::vec3 (0.0f);
	    p.rotation = glm::vec3 (0.0f);
	    p.angularVelocity = glm::vec3 (0.0f);
	    p.angularAcceleration = glm::vec3 (0.0f);
	    p.color = m_instanceColor;
	    if ((emitter.flags & 65536) != 0)
		p.color *= glm::vec3 (sample.color[0], sample.color[1], sample.color[2]) / 255.0f;
	    p.emitterColor = p.color;
	    p.alpha = getInstanceOverride ().alpha->value->getFloat ();
	    p.size = 0.5f;
	    p.lifetime = 1.0f;
	    p.age = 0.0f;
	    p.alive = true;
	    p.initial.color = p.color;
	    p.initial.alpha = p.alpha;
	    p.initial.size = p.size;
	    p.initial.lifetime = p.lifetime;
	    p.oscillationRandom = m_particle.animationMode == "randomframe"
		? WallpaperEngine::Maths::randomFloat (m_rng, 0.0f, 1.0f) : -1.0f;
	    p.trailHistory.clear ();
	    p.trailLastFrameValid = false;
	    for (auto& init : m_initializers) init (p);
	    if (usesNativeParticleSlots ()) m_remapSlots[slot] = p;
	    ++count;
	}
    };
}

EmitterFunc CParticle::createSphereEmitter (const ParticleEmitter& emitter) {
    includeControlPoint (emitter.controlPoint);
    DynamicValue* countOverride = getInstanceOverride ().count->value.get ();
    DynamicValue* speedOverride = getInstanceOverride ().speed->value.get ();

    // Convert emitter origin from screen space (Y down) to centered space (Y up)
    glm::vec3 transformedEmitterOrigin = emitter.origin;
    transformedEmitterOrigin.y = -transformedEmitterOrigin.y;

    const int controlPointIndex = emitter.controlPoint;

    bool limitOnePerFrame = (emitter.flags & 2) != 0;
    bool randomPeriodicEmission = (emitter.flags & 4) != 0;

    return [this, emitter, transformedEmitterOrigin, controlPointIndex, countOverride, speedOverride, limitOnePerFrame,
	    randomPeriodicEmission, emissionTimer = 0.0f, delayTimer = emitter.delay, durationTimer = 0.0f,
	    periodicTimer = 0.0f, emittedThisPeriod = 0u,
	    remaining
	    = emitter.instantaneous] (std::vector<ParticleInstance>& particles, uint32_t& count, float dt, uint32_t burst) mutable {
	const Data::Utils::ScopeGuard pending ([&] {
	    if (burst == 0 && (emitter.duration <= 0.0f || durationTimer < emitter.duration)
		&& (remaining > 0 || emitter.rate > 0.0f)) ++m_pendingEmitterCount;
	});
	if (burst == 0) {
	    if (delayTimer > 0.0f) {
		delayTimer -= dt;
		return;
	    }
	    if (emitter.duration > 0.0f) {
		if (durationTimer >= emitter.duration) {
		    return;
		}
		durationTimer += dt;
	    }
	}
	if (count >= particles.size ()) {
	    return;
	}

	uint32_t toEmit = burst;
	if (burst == 0) {
	    // Rate-based emission with optional cap at 1 per frame
	    const auto& recorder = getScene ().getAudioContext ().getRecorder ();
	    const float response = calculateParticleAudioResponse (
		recorder.audio16Left, recorder.audio16Right, emitter.audioProcessingMode,
		emitter.audioProcessingBounds, emitter.audioProcessingExponent,
		emitter.audioProcessingFrequencyStart, emitter.audioProcessingFrequencyEnd
	    );
	    const float countMultiplier = (m_particle.flags & 32) != 0 ? 1.0f : countOverride->getFloat ();
	    float rate = calculateParticleEmissionRate (emitter.rate, countMultiplier) * response;
	    if (randomPeriodicEmission) {
		// Native uses a positive active clock and a negative waiting clock.
		if (periodicTimer > 0.0f) {
		    periodicTimer -= dt;
		    if (periodicTimer < 0.0f) {
			periodicTimer = -WallpaperEngine::Maths::randomFloat (
			    m_rng, emitter.minPeriodicDelay, emitter.maxPeriodicDelay
			);
		    }
		    // The tick crossing into the wait still admits scheduled births.
		} else {
		    periodicTimer += dt;
		    if (periodicTimer < 0.0f) {
			rate = 0.0f;
		    } else {
			periodicTimer = WallpaperEngine::Maths::randomFloat (
			    m_rng, emitter.minPeriodicDuration, emitter.maxPeriodicDuration
			);
			emittedThisPeriod = 0;
		    }
		}
	    }
	    emissionTimer += dt * rate;
	    toEmit = static_cast<uint32_t> (emissionTimer);
	    emissionTimer -= static_cast<float> (toEmit);
	    // limitOnePerFrame (flags bit 1): cap at 1 to prevent rope artifacts
	    if (limitOnePerFrame && toEmit > 1) {
		toEmit = 1;
	    }
	    if (randomPeriodicEmission && emitter.maxToEmitPerPeriod != 0) {
		uint32_t cap = emitter.maxToEmitPerPeriod;
		if ((m_particle.flags & 32) == 0) {
		    const float scaled = static_cast<float> (cap) * countMultiplier;
		    // Match the box path's native signed64 truncation and low32 bits.
		    cap = scaled >= -0x1p63f && scaled < 0x1p63f
			? static_cast<uint32_t> (static_cast<int64_t> (scaled)) : 0;
		}
		if (cap != 0) {
		    const uint32_t available = cap > emittedThisPeriod ? cap - emittedThisPeriod : 0;
		    toEmit = std::min (toEmit, available);
		    emittedThisPeriod += toEmit;
		}
	    }

	    if (remaining > 0) {
		toEmit = remaining;
		remaining = 0;
	    }
	}

	const glm::vec3 emitterOrigin = resolveEmitterOrigin (transformedEmitterOrigin);
            glm::vec3 spawnOrigin = emitterOrigin;
            if (controlPointIndex >= 0 && controlPointIndex < static_cast<int> (m_controlPoints.size ())) {
                spawnOrigin += m_controlPoints[controlPointIndex].position;
            }
	    // Native caches the origin before the birth loop; initializers may mutate it.
	    for (uint32_t i = 0; i < toEmit && count < particles.size (); i++) {
                const uint32_t slot = usesNativeParticleSlots () ? acquireRemapSlot () : 0;
                if (usesNativeParticleSlots () && slot == m_maxParticles) break;
		auto& p = particles[count];
                p.remapSlot = slot;
                p.nativeSlotState = usesNativeParticleSlots ();
	    // Spawn at random position on ellipsoid surface
	    glm::vec3 randomPos;

	    // Orthographic particles (flags & 4 == 0): use 2D disk distribution in X/Y plane
	    // Perspective particles (flags & 4 != 0): use 3D spherical shell distribution
	    if (!getScene ().getScene ().camera.projection.isPerspective && (m_particle.flags & 4) == 0) {
		// 2D disk distribution with random Z offset
		float angle = WallpaperEngine::Maths::randomFloat (m_rng, 0.0f, glm::two_pi<float> ());
		float minRadius = emitter.distanceMin.x;
		float maxRadius = emitter.distanceMax.x;

		// Use sqrt for uniform area distribution in annulus
		float minRadiusSq = minRadius * minRadius;
		float maxRadiusSq = maxRadius * maxRadius;
		float radiusXY = std::sqrt (WallpaperEngine::Maths::randomFloat (m_rng, minRadiusSq, maxRadiusSq));

		randomPos = glm::vec3 (
		    radiusXY * std::cos (angle), radiusXY * std::sin (angle),
		    WallpaperEngine::Maths::randomFloat (m_rng, -maxRadius, maxRadius)
		);

		randomPos *= emitter.directions;
	    } else {
		// 3D spherical shell distribution
		float theta = WallpaperEngine::Maths::randomFloat (m_rng, 0.0f, glm::two_pi<float> ());
		float cosTheta = WallpaperEngine::Maths::randomFloat (m_rng, -1.0f, 1.0f);
		float sinTheta = std::sqrt (1.0f - cosTheta * cosTheta);

		randomPos = glm::vec3 (sinTheta * std::cos (theta), sinTheta * std::sin (theta), cosTheta);

		// Use cubic root for uniform volume distribution
		float minRadius = emitter.distanceMin.x;
		float maxRadius = emitter.distanceMax.x;
		float minRadiusCubed = minRadius * minRadius * minRadius;
		float maxRadiusCubed = maxRadius * maxRadius * maxRadius;
		float radius = std::cbrt (WallpaperEngine::Maths::randomFloat (m_rng, minRadiusCubed, maxRadiusCubed));

		randomPos *= radius;
		randomPos *= emitter.directions;
	    }

	    // Apply sign property to force positive/negative values per axis
	    // 0 = both, 1 = positive only, -1 = negative only
	    for (int i = 0; i < 3; i++) {
		if (emitter.sign[i] == 1) {
		    randomPos[i] = std::abs (randomPos[i]); // Force positive
		} else if (emitter.sign[i] == -1) {
		    randomPos[i] = -std::abs (randomPos[i]); // Force negative
		}
		// If sign[i] == 0, leave as-is (both positive and negative possible)
	    }
	    if (controlPointIndex >= 0 && controlPointIndex < static_cast<int> (m_controlPoints.size ())
		&& ((m_particle.flags & 1) != 0 || controlPointIndex != 0)) {
		randomPos = m_controlPoints[controlPointIndex].orientation * randomPos;
	    }
	    p.position = spawnOrigin + randomPos;
	    p.previousPosition = p.position;

	    // Set velocity only if emitter specifies speed (otherwise use initializers)
	    if (emitter.speedMax > 0.0f || emitter.speedMin != 0.0f) {
		// Velocity pointing outward from ellipsoid (randomPos already includes directions scaling)
		glm::vec3 direction = randomPos;
		if (glm::dot (direction, direction) < 0.0001f) {
		    direction = WallpaperEngine::Maths::randomVec3 (m_rng, glm::vec3 (-1.0f), glm::vec3 (1.0f))
			* emitter.directions;
		    direction.y = -direction.y;
		    if (controlPointIndex >= 0 && controlPointIndex < static_cast<int> (m_controlPoints.size ())
			&& ((m_particle.flags & 1) != 0 || controlPointIndex != 0)) {
			direction = m_controlPoints[controlPointIndex].orientation * direction;
		    }
		}
		if (glm::dot (direction, direction) > 0.0f) direction = glm::normalize (direction);
		float speed = WallpaperEngine::Maths::randomFloat (m_rng, emitter.speedMin, emitter.speedMax);
		const float speedMultiplier = (m_particle.flags & 16) != 0 ? 1.0f : speedOverride->getFloat ();
		p.velocity = direction * speed * speedMultiplier;
	    } else {
		// No emitter speed specified, velocity will be set by initializers
		p.velocity = glm::vec3 (0.0f);
	    }

	    p.acceleration = glm::vec3 (0.0f);
	    p.rotation = glm::vec3 (0.0f);
	    p.angularVelocity = glm::vec3 (0.0f);
	    p.angularAcceleration = glm::vec3 (0.0f);

	    p.color = m_instanceColor;
	    p.emitterColor = p.color;
	    p.alpha = 1.0f * getInstanceOverride ().alpha->value->getFloat ();
	    p.size = 0.5f;
	    p.lifetime = 1.0f;
	    p.age = 0.0f;
	    p.alive = true;

	    p.initial.color = p.color;
	    p.initial.alpha = p.alpha;
	    p.initial.size = p.size;
	    p.initial.lifetime = p.lifetime;

	    // Reset oscillator state for reused particles. Keep the birth phase available
	    // to initializers and retain the same fraction for later consumers.
	    p.oscillationRandom = m_particle.animationMode == "randomframe"
		? WallpaperEngine::Maths::randomFloat (m_rng, 0.0f, 1.0f) : -1.0f;
	    p.trailHistory.clear ();
	    p.trailLastFrameValid = false;

	    for (auto& init : m_initializers) {
		init (p);
	    }

            if (usesNativeParticleSlots ()) m_remapSlots[slot] = p;
	    count++;
	}
    };
}

static float particleOperatorBlend (float lifetimePosition, glm::vec4 blendTimes);

static float particleRemapInverseLength (const float squared) {
#if defined(__SSE__)
    // Exact normalization can cross a following periodic-transform boundary.
    return _mm_cvtss_f32 (_mm_rsqrt_ss (_mm_set_ss (squared)));
#else
    return 1.0f / std::sqrt (squared);
#endif
}

static float particleRemapFbmNoise (const float input, const float inputScale, const uint32_t octaves) {
    // Native initializer14027b4b0 consumes the authored count with unseeded
    // scalar noise; preserve its float accumulation order.
    float frequency = inputScale;
    float amplitude = 1.0f;
    float sum = 0.0f;
    float amplitudeSum = 0.0f;
    for (uint32_t octave = 0; octave < octaves; ++octave) {
        sum += simplexNoise1D (frequency * input) * amplitude;
        amplitudeSum += amplitude;
        frequency *= 2.0f;
        amplitude *= .5f;
    }
    return sum / amplitudeSum;
}

void WallpaperEngine::Render::Objects::applyParticleControlPointRemap (
    ParticleInstance& particle, const ControlPointRemap& remap,
    const std::vector<ControlPointData>& controlPoints, const bool initializer, const bool angularVelocityAvailable,
    const float runtime, const float timeOfDay, const glm::vec3 layerOrigin, const float initializerPhase,
    const float systemTime, const float layerTime, glm::vec3* pointOutput
) {
    const bool controlPointOutput = remap.output == ControlPointRemap::Output::ControlPoint;
    if (controlPointOutput && pointOutput == nullptr) return;
    const float weight = initializer ? 1.0f : particleOperatorBlend (particle.getLifetimePos (), remap.blendTimes);
    if (!initializer && remap.input == ControlPointRemap::Input::LifetimeFraction
        && ((remap.output == ControlPointRemap::Output::Velocity && remap.transform == ControlPointRemap::Transform::SimplexNoise)
            || (remap.output == ControlPointRemap::Output::Speed && remap.transform == ControlPointRemap::Transform::FbmNoise))) {
        const float minimum = remap.inputRangeMin->value->getFloat ();
        float span = remap.inputRangeMax->value->getFloat () - minimum;
        if (span == 0.0f) span = std::numeric_limits<float>::epsilon ();
        const float input = glm::clamp ((particle.getLifetimePos () - minimum) / span, 0.0f, 1.0f)
            * remap.transformInputScale;
        // Native shares this exact float-bit seed with oscillation/turbulence;
        // independent random draws or numeric float-to-uint conversion differ.
        const uint32_t seed = std::bit_cast<uint32_t> (particle.oscillationRandom);
        if (remap.transform == ControlPointRemap::Transform::FbmNoise) {
            const float noise = seededFbmNoise2D (seed, input, 0.0f, remap.transformOctaves) * .5f + .5f;
            const float outputMin = remap.outputRangeMin->value->getFloat ();
            const float mapped = outputMin + noise * (remap.outputRangeMax->value->getFloat () - outputMin);
            // Native speed multiplication preserves direction; flags 3 clamp
            // the mapped scalar as well as the normalized input.
            const float multiplier = glm::clamp (mapped, 0.0f, 1.0f);
            // Native blends the operated speed against its current value.
            particle.velocity *= weight == 1.0f ? multiplier : 1.0f + (multiplier - 1.0f) * weight;
            return;
        }
        const glm::vec3 noise {
            seededSimplexNoise2D (seed, input, 0.0f),
            seededSimplexNoise2D (seed ^ 188294317u, input, 0.0f),
            seededSimplexNoise2D (seed ^ 1228574339u, input, 0.0f)
        };
        const glm::vec3 outputMin = remap.outputRangeMin->value->getVec3 ();
        const glm::vec3 mapped = outputMin + (noise * .5f + .5f)
            * (remap.outputRangeMax->value->getVec3 () - outputMin);
        const glm::vec3 target = mapped * glm::vec3 (1.0f, -1.0f, 1.0f);
        particle.velocity = weight == 1.0f ? target : particle.velocity + (target - particle.velocity) * weight;
        return;
    }
    // Native allocates the input pair too, but this scalar input branch reads
    // the OUTPUT pair (14023d074 / 140244c5e; paired native readback).
    const glm::vec3 start = controlPoints[remap.outputControlPoint0].position;
    const glm::vec3 delta = controlPoints[remap.outputControlPoint1].position - start;
    const float lengthSquared = glm::dot (delta, delta);
    constexpr float epsilon = std::numeric_limits<float>::epsilon ();
    const bool positionOutput = remap.output == ControlPointRemap::Output::BetweenPoints;
    const float length = initializer ? glm::length (delta) : 0.0f;
    const bool tinyInitializer = initializer && length <= epsilon;
    // Operator position-output degeneracy remains outside the measured subset.
    // Scalar input selects zero and still runs mapping/output (14023d1ff /
    // 140244d85); initializer input uses the native FLT_EPSILON length boundary.
    if (positionOutput && !initializer && lengthSquared <= 0.0f) return;
    const bool zeroInput = tinyInitializer || (!positionOutput && lengthSquared <= 0.0f);
    const float fraction = zeroInput ? 0.0f : glm::dot (particle.position - start, delta) / lengthSquared;
    float input = 0.0f;
    switch (remap.input) {
        case ControlPointRemap::Input::ControlPoint:
        case ControlPointRemap::Input::DeltaToPoint:
        case ControlPointRemap::Input::DirectionToPoint:
        case ControlPointRemap::Input::LayerOrigin:
        case ControlPointRemap::Input::Color:
        case ControlPointRemap::Input::Position:
        case ControlPointRemap::Input::Velocity: break;
        case ControlPointRemap::Input::BetweenPoints: input = fraction; break;
        case ControlPointRemap::Input::DistanceToPoint:
            input = glm::length (particle.position - controlPoints[remap.inputControlPoint0].position);
            break;
        case ControlPointRemap::Input::MaxLifetime: input = particle.lifetime; break;
        case ControlPointRemap::Input::LifetimeFraction:
            // Initializer enum0 reads the birth animation phase; operators use age/maxlife.
            input = initializer ? initializerPhase : particle.getLifetimePos ();
            break;
        // Native size input reads birth +0x278 or working +0x270.
        case ControlPointRemap::Input::Size: input = initializer ? particle.initial.size : particle.size; break;
        // Alpha arrays alias until an alpha-writing operator separates them.
        case ControlPointRemap::Input::Opacity: input = initializer ? particle.initial.alpha : particle.alpha; break;
        case ControlPointRemap::Input::Runtime: input = runtime; break;
        // Native initializer reads system+3f0; operators deliberately read runtime.
        case ControlPointRemap::Input::ParticleSystemTime: input = initializer ? systemTime : runtime; break;
        case ControlPointRemap::Input::LayerTime: input = layerTime; break;
        case ControlPointRemap::Input::TimeOfDay: input = timeOfDay; break;
        case ControlPointRemap::Input::Speed: input = glm::length (particle.velocity); break;
        case ControlPointRemap::Input::Rotation: input = particle.rotation.z; break;
        case ControlPointRemap::Input::AngularSpeed: input = angularVelocityAvailable ? particle.angularVelocity.z : 0.0f; break;
    }
    const bool pointInput = remap.input == ControlPointRemap::Input::ControlPoint
        || remap.input == ControlPointRemap::Input::DeltaToPoint || remap.input == ControlPointRemap::Input::DirectionToPoint;
    const bool vectorInput = pointInput || remap.input == ControlPointRemap::Input::LayerOrigin
        || remap.input == ControlPointRemap::Input::Position
        || remap.input == ControlPointRemap::Input::Velocity || remap.input == ControlPointRemap::Input::Color;
    glm::vec3 inputValue (input);
    if (vectorInput) {
        if (remap.input == ControlPointRemap::Input::LayerOrigin) {
            inputValue = layerOrigin;
        } else if (pointInput) {
            inputValue = controlPoints[remap.inputControlPoint0].position;
            if (remap.input != ControlPointRemap::Input::ControlPoint) inputValue -= particle.position;
            if (remap.input == ControlPointRemap::Input::DirectionToPoint) {
                const float squared = glm::dot (inputValue, inputValue);
                // Operators mask exact zero before rsqrt; nonzero subnormal squared
                // distances produce nonfinite values and remain outside this subset.
                if (initializer) {
                    // The birth getter uses sqrt/divide, without the operator's zero mask.
                    inputValue *= 1.0f / std::sqrt (squared);
                } else {
                    if (squared > 0.0f && squared < std::numeric_limits<float>::min ()) return;
                    if (squared > 0.0f) inputValue *= particleRemapInverseLength (squared);
                    else inputValue = glm::vec3 (0.0f);
                }
            }
            inputValue *= glm::vec3 (1.0f, -1.0f, 1.0f);
        } else {
            // Particle storage is reflected Y; remap components use authored coordinates.
            inputValue = remap.input == ControlPointRemap::Input::Color
                ? (initializer ? particle.initial.color : particle.color)
                : (remap.input == ControlPointRemap::Input::Position ? particle.position : particle.velocity)
                    * glm::vec3 (1.0f, -1.0f, 1.0f);
        }
        using Component = ControlPointRemap::Component;
        switch (remap.inputComponent) {
            case Component::All: break;
            case Component::X: inputValue = glm::vec3 (inputValue.x); break;
            case Component::Y: inputValue = glm::vec3 (inputValue.y); break;
            case Component::Z: inputValue = glm::vec3 (inputValue.z); break;
            case Component::Sum: inputValue = glm::vec3 (inputValue.x + inputValue.y + inputValue.z); break;
            case Component::Average: inputValue = glm::vec3 ((inputValue.x + inputValue.y + inputValue.z) * (1.0f / 3.0f)); break;
            case Component::Max: inputValue = glm::vec3 (std::max ({ inputValue.x, inputValue.y, inputValue.z })); break;
            case Component::Min: inputValue = glm::vec3 (std::min ({ inputValue.x, inputValue.y, inputValue.z })); break;
        }
    }
    const glm::vec3 minimum = remap.inputRangeMin->value->getVec3 ();
    glm::vec3 span = remap.inputRangeMax->value->getVec3 () - minimum;
    // Both native parsers replace exactly equal range endpoints with FLT_EPSILON.
    for (int axis = 0; axis < 3; ++axis) if (span[axis] == 0.0f) span[axis] = epsilon;
    glm::vec3 normalized = (inputValue - minimum) / span;
    // Native operators normalize a scalar through range X, then replicate it.
    if (!initializer && !vectorInput) normalized = glm::vec3 (normalized.x);
    if (remap.flags & 1u) normalized = glm::clamp (normalized, glm::vec3 (0), glm::vec3 (1));
    if (remap.transform != ControlPointRemap::Transform::Identity) for (int axis = 0; axis < 3; ++axis) {
        // Native transforms each normalized component before output saturation.
        const float phase = normalized[axis] * remap.transformInputScale;
        switch (remap.transform) {
            case ControlPointRemap::Transform::SimplexNoise:
                // Native operators use shared float-bit seeds, with separate
                // XOR seeds per output channel; initializers are unseeded.
                normalized[axis] = (initializer ? simplexNoise1D (phase) : seededSimplexNoise2D (
                    std::bit_cast<uint32_t> (particle.oscillationRandom) ^ (axis == 1 ? 188294317u : axis == 2 ? 1228574339u : 0u),
                    phase, 0.0f)) * .5f + .5f;
                break;
            case ControlPointRemap::Transform::FbmNoise:
                normalized[axis] = (initializer ? particleRemapFbmNoise (normalized[axis], remap.transformInputScale, remap.transformOctaves)
                    : seededFbmNoise2D (std::bit_cast<uint32_t> (particle.oscillationRandom)
                        ^ (axis == 1 ? 188294317u : axis == 2 ? 1228574339u : 0u), phase, 0.0f, remap.transformOctaves)) * .5f + .5f;
                break;
            case ControlPointRemap::Transform::Sine: {
                const float angle = initializer ? (normalized[axis] * glm::pi<float> ()) * remap.transformInputScale
                    : normalized[axis] * (remap.transformInputScale * glm::pi<float> ());
                normalized[axis] = std::sin (angle - glm::half_pi<float> ()) * .5f + .5f;
                break;
            }
            case ControlPointRemap::Transform::Square: {
                const float fraction = phase - (initializer ? std::floor (phase) : std::trunc (phase));
                // The scalar initializer rounds half away; SIMD rounds ties even.
                normalized[axis] = (initializer ? std::round (fraction) : glm::roundEven (fraction))
                    + (phase < 0.0f ? 1.0f : 0.0f);
                break;
            }
            case ControlPointRemap::Transform::Saw:
                normalized[axis] = phase - (initializer ? std::floor (phase) : std::trunc (phase))
                    + (normalized[axis] < 0.0f ? 1.0f : 0.0f);
                break;
            case ControlPointRemap::Transform::Triangle: {
                const float fraction = std::abs (phase) - std::floor (std::abs (phase));
                normalized[axis] = 1.0f - std::abs (fraction * 2.0f - 1.0f);
                break;
            }
            default: break;
        }
    }
    glm::vec3 mapped = remap.outputRangeMin->value->getVec3 ()
        + normalized * (remap.outputRangeMax->value->getVec3 () - remap.outputRangeMin->value->getVec3 ());
    // Output saturation applies before arithmetic, not to the final position.
    if (remap.flags & 2u) mapped = glm::clamp (mapped, glm::vec3 (0), glm::vec3 (1));
    const auto operate = [&remap, &mapped] (const float current) {
        switch (remap.operation) {
            case ControlPointRemap::Operation::Remap: return mapped.x;
            case ControlPointRemap::Operation::Multiply: return current * mapped.x;
            case ControlPointRemap::Operation::Add: return current + mapped.x;
            case ControlPointRemap::Operation::Subtract: return current - mapped.x;
        }
        return mapped.x;
    };
    if (remap.output == ControlPointRemap::Output::Speed) {
        if (remap.operation == ControlPointRemap::Operation::Multiply) {
            particle.velocity *= weight == 1.0f ? mapped.x : 1.0f + (mapped.x - 1.0f) * weight;
        } else {
            const float speed = glm::length (particle.velocity);
            const float result = operate (speed);
            const float blended = weight == 1.0f ? result : speed + (result - speed) * weight;
            // Native scales the existing direction, including when its length is zero.
            particle.velocity *= speed > 0.0f ? blended / speed : blended;
        }
    } else if (remap.output == ControlPointRemap::Output::DistanceToPoint) {
        glm::vec3 direction = particle.position - start;
        const float distanceSquared = glm::dot (direction, direction);
        // Native operator rsqrtps produces nonfinite positions for zero/subnormal
        // squared distances; those remain outside the finite output subset.
        if (!initializer && distanceSquared < std::numeric_limits<float>::min ()) return;
        const float distance = glm::length (direction);
        // Initializers normalize every nonzero length, even below FLT_EPSILON.
        // A squared-underflow direction remains unnormalized, as in native.
        if (distance != 0.0f) direction /= distance;
        const float result = operate (distance);
        const float blended = weight == 1.0f ? result : distance + (result - distance) * weight;
        particle.position = start + direction * blended;
    } else if (remap.output == ControlPointRemap::Output::DeltaToPoint
               || remap.output == ControlPointRemap::Output::DirectionToPoint) {
        using Component = ControlPointRemap::Component;
        const glm::vec3 coordinateScale (1.0f, -1.0f, 1.0f);
        glm::vec3 relative = (start - particle.position) * coordinateScale;
        const bool direction = remap.output == ControlPointRemap::Output::DirectionToPoint;
        float radius = 0.0f;
        if (direction) {
            const float squared = glm::dot (relative, relative);
            if (initializer) {
                radius = std::sqrt (squared);
                if (radius > epsilon) relative /= radius;
            } else {
                // Native zero/subnormal source radius produces nonfinite output.
                if (squared < std::numeric_limits<float>::min ()) return;
                const float inverseLength = particleRemapInverseLength (squared);
                radius = squared * inverseLength;
                relative *= inverseLength;
            }
        }
        glm::vec3 operated = mapped;
        switch (remap.operation) {
            case ControlPointRemap::Operation::Remap: break;
            case ControlPointRemap::Operation::Multiply: operated = relative * mapped; break;
            case ControlPointRemap::Operation::Add: operated = relative + mapped; break;
            case ControlPointRemap::Operation::Subtract: operated = relative - mapped; break;
        }
        for (int axis = 0; axis < 3; ++axis) {
            if (remap.outputComponent == Component::All
                || static_cast<int> (remap.outputComponent) == static_cast<int> (Component::X) + axis)
                relative[axis] = weight == 1.0f ? operated[axis]
                    : relative[axis] + (operated[axis] - relative[axis]) * weight;
        }
        if (direction) {
            const float squared = glm::dot (relative, relative);
            if (initializer) {
                // Native normalizes the operated vector unconditionally; zero is
                // nonfinite there and remains outside the finite initializer subset.
                if (squared == 0.0f) return;
                relative = (relative * (1.0f / std::sqrt (squared))) * radius;
            } else if (squared == 0.0f) {
                // Unlike a zero source radius, zero operated direction is masked.
                relative = glm::vec3 (0.0f);
            } else {
                if (squared < std::numeric_limits<float>::min ()) return;
                relative *= particleRemapInverseLength (squared) * radius;
            }
        }
        // Delta/direction selectors reduce which intermediate components change;
        // even output reduction names still reconstruct the resulting position.
        particle.position = start - relative * coordinateScale;
    } else if (remap.output == ControlPointRemap::Output::Velocity || remap.output == ControlPointRemap::Output::Position
               || remap.output == ControlPointRemap::Output::Color || controlPointOutput) {
        using Component = ControlPointRemap::Component;
        // Native output reductions leave values unchanged; all and XYZ select channels.
        if (remap.outputComponent > Component::Z) return;
        const bool colorOutput = remap.output == ControlPointRemap::Output::Color;
        glm::vec3& destination = controlPointOutput ? *pointOutput : colorOutput ? (initializer ? particle.initial.color : particle.color)
            : remap.output == ControlPointRemap::Output::Position ? particle.position : particle.velocity;
        // Spatial outputs reflect authored Y; RGB stays in its stored units.
        const glm::vec3 coordinateScale = colorOutput ? glm::vec3 (1.0f) : glm::vec3 (1.0f, -1.0f, 1.0f);
        glm::vec3 target = mapped * coordinateScale;
        switch (remap.operation) {
            case ControlPointRemap::Operation::Remap: {
                if (!initializer && !controlPointOutput && remap.outputComponent == Component::All) {
                    glm::vec4 blendTimes = remap.blendTimes;
                    blendTimes.x = std::min (blendTimes.x, blendTimes.y - 0.0001f);
                    blendTimes.w = std::max (blendTimes.w, blendTimes.z + 0.0001f);
                    // Native blended all-component remap reuses mapped X for
                    // Y/Z, even where the selected envelope has full weight.
                    if ((blendTimes.y > 0.01f || blendTimes.z < 0.99f)
                        && (blendTimes.z - blendTimes.y > 0.01f || blendTimes.y - blendTimes.x > 0.01f
                            || blendTimes.w - blendTimes.z > 0.01f))
                        target = glm::vec3 (mapped.x) * coordinateScale;
                }
                break;
            }
            // Multipliers are dimensionless factors, not reflected velocity deltas.
            case ControlPointRemap::Operation::Multiply: target = destination * mapped; break;
            case ControlPointRemap::Operation::Add: target = destination + target; break;
            case ControlPointRemap::Operation::Subtract: target = destination - target; break;
        }
        if (remap.outputComponent == Component::All) {
            destination = weight == 1.0f ? target : destination + (target - destination) * weight;
        } else {
            const int axis = static_cast<int> (remap.outputComponent) - static_cast<int> (Component::X);
            destination[axis] = weight == 1.0f ? target[axis] : destination[axis] + (target[axis] - destination[axis]) * weight;
        }
        if (initializer && colorOutput) particle.color = particle.initial.color;
    } else {
        // Angular storage is allocated by angularvelocityrandom/angularmovement,
        // not by remap. Native skips angular output when its storage bit is clear.
        if (remap.output == ControlPointRemap::Output::AngularSpeed && !angularVelocityAvailable) return;
        float* scalar = nullptr;
        float* initial = nullptr;
        switch (remap.output) {
            case ControlPointRemap::Output::Size: scalar = &particle.size; initial = &particle.initial.size; break;
            case ControlPointRemap::Output::Opacity: scalar = &particle.alpha; initial = &particle.initial.alpha; break;
            case ControlPointRemap::Output::MaxLifetime: scalar = &particle.lifetime; initial = &particle.initial.lifetime; break;
            case ControlPointRemap::Output::Rotation: scalar = &particle.rotation.z; break;
            case ControlPointRemap::Output::AngularSpeed: scalar = &particle.angularVelocity.z; break;
            default: break;
        }
        const float current = scalar ? *scalar
            : tinyInitializer ? glm::dot (particle.position - start, delta) : fraction;
        const float result = operate (current);
        if (scalar) {
            *scalar = weight == 1.0f ? result : current + (result - current) * weight;
            if (initializer && initial) *initial = *scalar;
        } else if (tinyInitializer) {
            // Native 14023e0f8..14023e1e4 keeps tiny directions unnormalized,
            // uses their raw projection for output arithmetic, and scales the
            // reconstructed displacement by the original length.
            const glm::vec3 residual = (particle.position - start) - delta * current;
            particle.position = start + residual + (delta * result) * length;
        } else {
            // Keep the position offset perpendicular to the segment.
            particle.position += delta * ((result - fraction) * weight);
        }
    }
}

glm::vec3 CParticle::remapLayerOrigin () const {
    const CParticle* root = this;
    while (root->m_particleParent != nullptr) root = root->m_particleParent;
    // Native system+8 keeps the root layer for children. Its matrix translation
    // includes scene parents; queued manual births retain their resolved snapshot.
    const auto& transform = root->m_initializingManualEmission
        ? root->m_pendingBirthTransform : root->m_controlPointTransform;
    return glm::vec3 (transform[3]) * glm::vec3 (1.0f, -1.0f, 1.0f);
}

void CParticle::applyInitialControlPointRemap (ParticleInstance& particle, const ControlPointRemap& remap) {
    const bool world = (m_particle.flags & 1) != 0;
    const bool output = remap.output == ControlPointRemap::Output::ControlPoint;
    const auto& transform = m_initializingManualEmission ? m_pendingBirthTransform : m_controlPointTransform;
    auto& point = m_controlPoints[output ? remap.outputControlPoint0 : remap.inputControlPoint0];
    // Input getters zero current XYZ before reading the output pair. CP output
    // instead reads that pair first, then writes only its selected current XYZ.
    if (!output) {
        point.simulationPosition = glm::vec3 (0.0f);
        point.position = world ? glm::vec3 (glm::inverse (transform) * glm::vec4 (0, 0, 0, 1)) : glm::vec3 (0.0f);
        point.displacement = -point.birthPreviousPosition;
    }
    glm::vec3 target = point.simulationPosition;
    if (!world) {
        applyParticleControlPointRemap (particle, remap, m_controlPoints, true, false, 0, 0, glm::vec3 (0), 0, 0, 0,
                                       output ? &target : nullptr);
    } else {
        auto points = m_controlPoints;
        for (auto& value : points) value.position = value.simulationPosition;
        auto simulationParticle = particle;
        simulationParticle.position = glm::vec3 (transform * glm::vec4 (particle.position, 1.0f));
        applyParticleControlPointRemap (simulationParticle, remap, points, true, false, 0, 0, glm::vec3 (0), 0, 0, 0,
                                       output ? &target : nullptr);
        if (!output) particle.position = glm::vec3 (glm::inverse (transform) * glm::vec4 (simulationParticle.position, 1.0f));
    }
    if (output) {
        point.simulationPosition = target;
        point.position = world ? glm::vec3 (glm::inverse (transform) * glm::vec4 (target, 1.0f)) : target;
        point.displacement = target - point.birthPreviousPosition;
    }
}

InitializerFunc CParticle::createControlPointRemapInitializer (const ControlPointRemapInitializer& init) {
    const auto& remap = init.remap;
    includeControlPoint (remap.inputControlPoint0);
    includeControlPoint (remap.inputControlPoint1);
    includeControlPoint (remap.outputControlPoint0);
    includeControlPoint (remap.outputControlPoint1);
    const bool mutatesPoint = remap.output == ControlPointRemap::Output::ControlPoint
        || remap.input == ControlPointRemap::Input::ControlPoint
        || remap.input == ControlPointRemap::Input::DeltaToPoint || remap.input == ControlPointRemap::Input::DirectionToPoint;
    m_hasMutatingRemapInitializer |= mutatesPoint;
    return [this, &remap, mutatesPoint] (ParticleInstance& particle) {
        if (mutatesPoint) {
            applyInitialControlPointRemap (particle, remap);
            return;
        }
        const auto apply = [&] (const std::vector<ControlPointData>& points) {
            applyParticleControlPointRemap (particle, remap, points, true, m_hasAngularVelocity, getScene ().getTime (), g_Daytime,
                remap.input == ControlPointRemap::Input::LayerOrigin ? remapLayerOrigin () : glm::vec3 (0.0f),
                m_particle.animationMode == "randomframe" ? particle.oscillationRandom : 0.0f,
                m_initializingManualEmission ? m_pendingSystemTime : m_remapSystemTime, remapLayerTime ());
        };
        if (!(m_particle.flags & 1)) {
            apply (m_controlPoints);
            return;
        }
        // Native remap initializers read/write simulation-space birth arrays.
        // The fork keeps local births until finalization, so bridge this dispatch
        // using the same matrix that will publish its position and velocity.
        const auto& transform = m_initializingManualEmission ? m_pendingBirthTransform : m_controlPointTransform;
        const glm::vec3 position = particle.position, velocity = particle.velocity;
        auto points = m_controlPoints;
        for (auto& point : points) point.position = point.simulationPosition;
        particle.position = glm::vec3 (transform * glm::vec4 (position, 1.0f));
        particle.velocity = glm::mat3 (transform) * velocity;
        apply (points);
        using Output = ControlPointRemap::Output;
        const bool positionOutput = remap.output == Output::BetweenPoints || remap.output == Output::Position
            || remap.output == Output::DistanceToPoint || remap.output == Output::DeltaToPoint
            || remap.output == Output::DirectionToPoint;
        particle.position = positionOutput ? glm::vec3 (glm::inverse (transform) * glm::vec4 (particle.position, 1.0f)) : position;
        particle.velocity = remap.output == Output::Velocity || remap.output == Output::Speed
            ? glm::inverse (glm::mat3 (transform)) * particle.velocity : velocity;
    };
}

OperatorFunc CParticle::createControlPointRemapOperator (const ControlPointRemapOperator& op) {
    const auto& remap = op.remap;
    if (remap.output == ControlPointRemap::Output::ControlPoint) m_hasControlPointOutputOperator = true;
    if (remap.output == ControlPointRemap::Output::Opacity) m_hasAlphaOperators = true;
    if (remap.output == ControlPointRemap::Output::Color) m_hasColorOperators = true;
    includeControlPoint (remap.inputControlPoint0);
    includeControlPoint (remap.inputControlPoint1);
    includeControlPoint (remap.outputControlPoint0);
    includeControlPoint (remap.outputControlPoint1);
    return [this, &remap] (std::vector<ParticleInstance>& particles, uint32_t count,
                     const std::vector<ControlPointData>& controlPoints, float, float) {
        const glm::vec3 layerOrigin = remap.input == ControlPointRemap::Input::LayerOrigin
            ? remapLayerOrigin () : glm::vec3 (0.0f);
        const float layerTime = remapLayerTime ();
        const bool output = remap.output == ControlPointRemap::Output::ControlPoint;
        auto points = output ? controlPoints : std::vector<ControlPointData> ();
        // Native CP stores use lane zero of each SIMD block, even when retired.
        // The next block observes the preceding block's current-point write.
        for (uint32_t i = 0; i < count; i += output ? 4 : 1) {
            if (!output && !particles[i].shouldSimulate ()) continue;
            if ((remap.transform == ControlPointRemap::Transform::SimplexNoise
                 || remap.transform == ControlPointRemap::Transform::FbmNoise) && particles[i].oscillationRandom < 0.0f) {
                particles[i].oscillationRandom = WallpaperEngine::Maths::randomFloat (m_rng, 0.0f, 1.0f);
            }
            glm::vec3 target = output ? points[remap.outputControlPoint0].position : glm::vec3 (0.0f);
            applyParticleControlPointRemap (particles[i], remap, output ? points : controlPoints, false, m_hasAngularVelocity,
                getScene ().getTime (), g_Daytime, layerOrigin, 0.0f, m_remapSystemTime, layerTime, output ? &target : nullptr);
            if (output) points[remap.outputControlPoint0].position = target;
        }
        if (output && count != 0) {
            auto& point = m_controlPoints[remap.outputControlPoint0];
            point.simulationPosition = points[remap.outputControlPoint0].position;
            point.position = (m_particle.flags & 1) != 0
                ? glm::vec3 (glm::inverse (m_controlPointTransform) * glm::vec4 (point.simulationPosition, 1.0f))
                : point.simulationPosition;
            point.displacement = point.simulationPosition - point.birthPreviousPosition;
        }
    };
}

// ========== INITIALIZERS ==========

void CParticle::setupInitializers () {
    for (const auto& initializer : m_particle.initializers) {
	if (!initializer) {
	    continue;
	}

	InitializerFunc func;

	if (initializer->is<ControlPointRemapInitializer> ()) {
	    func = createControlPointRemapInitializer (*initializer->as<ControlPointRemapInitializer> ());
	} else if (initializer->is<InheritInitialEventColorInitializer> ()) {
	    func = createInheritInitialEventColorInitializer (*initializer->as<InheritInitialEventColorInitializer> ());
	} else if (initializer->is<ColorRandomInitializer> ()) {
	    func = createColorRandomInitializer (*initializer->as<ColorRandomInitializer> ());
	} else if (initializer->is<SizeRandomInitializer> ()) {
	    func = createSizeRandomInitializer (*initializer->as<SizeRandomInitializer> ());
	} else if (initializer->is<AlphaRandomInitializer> ()) {
	    func = createAlphaRandomInitializer (*initializer->as<AlphaRandomInitializer> ());
	} else if (initializer->is<LifetimeRandomInitializer> ()) {
	    const auto& lifeInit = *initializer->as<LifetimeRandomInitializer> ();
	    if (m_ropeMeanLifetime == 0.0f) {
		const float minimum = lifeInit.min->value->getFloat ();
		m_ropeMeanLifetime = minimum + 0.5f * (lifeInit.max->value->getFloat () - minimum);
	    }
	    func = createLifetimeRandomInitializer (lifeInit);
	} else if (initializer->is<VelocityRandomInitializer> ()) {
	    func = createVelocityRandomInitializer (*initializer->as<VelocityRandomInitializer> ());
	} else if (initializer->is<InheritControlPointVelocityInitializer> ()) {
	    func = createInheritControlPointVelocityInitializer (*initializer->as<InheritControlPointVelocityInitializer> ());
	} else if (initializer->is<RotationRandomInitializer> ()) {
	    func = createRotationRandomInitializer (*initializer->as<RotationRandomInitializer> ());
	} else if (initializer->is<AngularVelocityRandomInitializer> ()) {
	    func = createAngularVelocityRandomInitializer (*initializer->as<AngularVelocityRandomInitializer> ());
	} else if (initializer->is<TurbulentVelocityRandomInitializer> ()) {
	    func = createTurbulentVelocityRandomInitializer (*initializer->as<TurbulentVelocityRandomInitializer> ());
	} else if (initializer->is<PositionOffsetRandomInitializer> ()) {
	    func = createPositionOffsetRandomInitializer (*initializer->as<PositionOffsetRandomInitializer> ());
	} else if (initializer->is<MapSequenceAroundControlPointInitializer> ()) {
	    func = createMapSequenceAroundControlPointInitializer (
		*initializer->as<MapSequenceAroundControlPointInitializer> ()
	    );
	} else if (initializer->is<MapSequenceBetweenControlPointsInitializer> ()) {
	    func = createMapSequenceBetweenControlPointsInitializer (
		*initializer->as<MapSequenceBetweenControlPointsInitializer> ()
	    );
	} else {
	    sLog.out ("Unknown initializer type");
	}

	if (func) {
	    m_initializers.push_back (std::move (func));
	}
    }
}

void WallpaperEngine::Render::Objects::initializeParticleEventColor (
    ParticleInstance& particle, const ParticleInstance* parent, const bool copyOpacity
) {
    if (!parent) return;
    // Native opcode16 copies current parent values into the child's birth arrays.
    // Unlike the per-step operator, the initializer has no parent lifetime gate.
    particle.color = particle.initial.color = parent->color;
    if (copyOpacity) particle.alpha = particle.initial.alpha = parent->alpha;
}

InitializerFunc CParticle::createInheritInitialEventColorInitializer (const InheritInitialEventColorInitializer& init) {
    return [this, &init] (ParticleInstance& particle) {
        if (!m_particleParent || m_eventParentSerial == 0) return;
        const auto end = m_particleParent->m_particles.begin () + m_particleParent->m_particleCount;
        const auto parent = std::find_if (m_particleParent->m_particles.begin (), end, [this] (const auto& candidate) {
            return candidate.serial == m_eventParentSerial;
        });
        // Follow children sample the current retained record at each birth.
        // Death-child retention after parent compaction remains unsupported.
        if (parent != end) initializeParticleEventColor (particle, &*parent, init.copyOpacity);
    };
}

InitializerFunc CParticle::createColorRandomInitializer (const ColorRandomInitializer& init) {
    DynamicValue* minValue = init.min->value.get ();
    DynamicValue* maxValue = init.max->value.get ();
    const float exponent = init.exponent;
    return [this, minValue, maxValue, exponent] (ParticleInstance& p) {
	// Native adjusts the endpoints before interpolating one shared RGB fraction.
	float fraction = WallpaperEngine::Maths::randomFloat (m_rng, 0.0f, 1.0f);
	if (exponent != 1.0f) fraction = std::pow (fraction, exponent);
	p.color *= glm::mix (adjustInstanceColor (minValue->getVec3 ()),
	    adjustInstanceColor (maxValue->getVec3 ()), fraction);
	p.initial.color = p.color;
    };
}

InitializerFunc CParticle::createSizeRandomInitializer (const SizeRandomInitializer& init) {
    DynamicValue* minValue = init.min->value.get ();
    DynamicValue* maxValue = init.max->value.get ();
    DynamicValue* exponentValue = init.exponent->value.get ();
    DynamicValue* sizeOverride = getInstanceOverride ().size->value.get ();

    return [this, minValue, maxValue, exponentValue, sizeOverride] (ParticleInstance& p) {
	float t = WallpaperEngine::Maths::randomFloat (m_rng, 0.0f, 1.0f);
	float exponent = exponentValue->getFloat ();
	float min = minValue->getFloat ();
	float max = maxValue->getFloat ();

	// Apply exponent for non-linear distribution
	float adjustedT = std::pow (t, exponent);
	const float sizeMultiplier = (m_particle.flags & 128) != 0 ? 1.0f : sizeOverride->getFloat ();
	// Each initializer multiplies the retained half-size, starting at .5.
	p.size *= (min + adjustedT * (max - min)) * sizeMultiplier;
	p.initial.size = p.size;
    };
}

InitializerFunc CParticle::createAlphaRandomInitializer (const AlphaRandomInitializer& init) {
    DynamicValue* minValue = init.min->value.get ();
    DynamicValue* maxValue = init.max->value.get ();

    return [this, minValue, maxValue] (ParticleInstance& p) {
	p.alpha *= WallpaperEngine::Maths::randomFloat (m_rng, minValue->getFloat (), maxValue->getFloat ());
	p.initial.alpha = p.alpha;
    };
}

InitializerFunc CParticle::createLifetimeRandomInitializer (const LifetimeRandomInitializer& init) {
    DynamicValue* minValue = init.min->value.get ();
    DynamicValue* maxValue = init.max->value.get ();
    DynamicValue* exponentValue = init.exponent->value.get ();
    DynamicValue* lifetimeOverride = getInstanceOverride ().lifetime->value.get ();

    return [this, minValue, maxValue, exponentValue, lifetimeOverride] (ParticleInstance& p) {
	const float lifetimeMultiplier = (m_particle.flags & 64) != 0 ? 1.0f : lifetimeOverride->getFloat ();
	p.lifetime = calculateParticleLifetime (
	    minValue->getFloat (), maxValue->getFloat (), exponentValue->getFloat (),
	    WallpaperEngine::Maths::randomFloat (m_rng, 0.0f, 1.0f), lifetimeMultiplier
	);
	p.initial.lifetime = p.lifetime;
    };
}

InitializerFunc CParticle::createVelocityRandomInitializer (const VelocityRandomInitializer& init) {
    DynamicValue* minValue = init.min->value.get ();
    DynamicValue* maxValue = init.max->value.get ();
    DynamicValue* speedOverride = getInstanceOverride ().speed->value.get ();

    return [this, minValue, maxValue, speedOverride] (ParticleInstance& p) {
	const float speedMultiplier = (m_particle.flags & 16) != 0 ? 1.0f : speedOverride->getFloat ();
	glm::vec3 vel = WallpaperEngine::Maths::randomVec3 (m_rng, minValue->getVec3 (), maxValue->getVec3 ())
	    * speedMultiplier;
	vel.y = -vel.y;
	p.velocity += vel;
    };
}

InitializerFunc CParticle::createInheritControlPointVelocityInitializer (
    const InheritControlPointVelocityInitializer& init
) {
    includeControlPoint (init.controlPoint);
    DynamicValue* minValue = init.min->value.get ();
    DynamicValue* maxValue = init.max->value.get ();
    DynamicValue* speedOverride = getInstanceOverride ().speed->value.get ();
    const int point = init.controlPoint;
    return [this, minValue, maxValue, speedOverride, point] (ParticleInstance& particle) {
	// Use one scalar for all axes; independent samples would change direction.
	const float weight = WallpaperEngine::Maths::randomFloat (m_rng, minValue->getFloat (), maxValue->getFloat ());
        // Unmutated scripted points have already been copied to previous. A
        // preceding remap getter can change that delta within the same birth.
        if (point >= 0 && point < static_cast<int> (m_controlPoints.size ())
            && (!m_initializingManualEmission || m_hasMutatingRemapInitializer)) {
            const auto& cp = m_controlPoints[point];
            glm::vec3 velocity = cp.velocity;
            if (m_hasMutatingRemapInitializer) {
                // Opcode8 divides live current-minus-previous by context+150:
                // the previous raw scene interval, including scripted dt0 births.
                const float interval = getScene ().getPreviousDeltaTime ();
                velocity = (cp.simulationPosition - cp.birthPreviousPosition) / interval;
                if ((m_particle.flags & 1) != 0 && (cp.flags & 2) == 0) {
                    const auto& transform = m_initializingManualEmission ? m_pendingBirthTransform : m_controlPointTransform;
                    velocity = glm::inverse (glm::mat3 (transform)) * velocity;
                }
            }
            const float speedMultiplier = (m_particle.flags & 16) != 0 ? 1.0f : speedOverride->getFloat ();
            particle.velocity += velocity * weight * speedMultiplier;
        }
    };
}

InitializerFunc CParticle::createRotationRandomInitializer (const RotationRandomInitializer& init) {
    DynamicValue* minValue = init.min->value.get ();
    DynamicValue* maxValue = init.max->value.get ();

    return [this, minValue, maxValue] (ParticleInstance& p) {
	p.rotation += WallpaperEngine::Maths::randomVec3 (m_rng, minValue->getVec3 (), maxValue->getVec3 ());
    };
}

InitializerFunc CParticle::createAngularVelocityRandomInitializer (const AngularVelocityRandomInitializer& init) {
    m_hasAngularVelocity = true;
    DynamicValue* minValue = init.min->value.get ();
    DynamicValue* maxValue = init.max->value.get ();
    DynamicValue* exponentValue = init.exponent->value.get ();
    DynamicValue* speedOverride = getInstanceOverride ().speed->value.get ();

    return [this, minValue, maxValue, exponentValue, speedOverride] (ParticleInstance& p) {
	glm::vec3 minVec = minValue->getVec3 ();
	glm::vec3 maxVec = maxValue->getVec3 ();
	float exponent = exponentValue->getFloat ();

	// Apply exponent bias to random distribution
	// exponent = 1: uniform distribution
	// exponent -> 0: bias towards max
	// exponent >= 2: bias towards min
	glm::vec3 result;
	for (int i = 0; i < 3; i++) {
	    float t = WallpaperEngine::Maths::randomFloat (m_rng, 0.0f, 1.0f);
	    t = std::pow (t, exponent);
	    result[i] = minVec[i] + t * (maxVec[i] - minVec[i]);
	}

	const float speedMultiplier = (m_particle.flags & 16) != 0 ? 1.0f : speedOverride->getFloat ();
	p.angularVelocity += result * speedMultiplier;
    };
}

InitializerFunc CParticle::createPositionOffsetRandomInitializer (const PositionOffsetRandomInitializer& init) {
    const bool perspective = getScene ().getScene ().camera.projection.isPerspective;
    return [this, &init, perspective] (ParticleInstance& p) {
	const glm::vec3 directions = init.directions ? init.directions->value->getVec3 ()
	    : glm::vec3 (1.0f, 1.0f, perspective ? 1.0f : 0.0f);
	const float scale = init.scale ? init.scale->value->getFloat () : (perspective ? 1.0f : 0.001f);
	const float distance = init.distance ? init.distance->value->getFloat () : (perspective ? 0.1f : 100.0f);
	const float time = getScene ().getTime () * init.timeScale->value->getFloat ();
	const bool worldSpace = (m_particle.flags & 1) != 0;
	const auto& birthTransform = m_initializingManualEmission ? m_pendingBirthTransform : m_controlPointTransform;
	const glm::mat3 basis (birthTransform);
	// A collapsed birth transform cannot represent a world offset locally.
	// Keep its particles finite rather than applying a singular inverse.
	if (distance == 0.0f || (worldSpace && glm::determinant (basis) == 0.0f)) return;
	const glm::vec3 position = (worldSpace ? glm::vec3 (birthTransform * glm::vec4 (p.position, 1.0f)) : p.position)
	    * glm::vec3 (1.0f, -1.0f, 1.0f);
	glm::vec3 offset (
	    fractalNoise2D (position.x * scale, time, init.octaves),
	    fractalNoise2D (time, position.y * scale, init.octaves),
	    fractalNoise2D (position.z * scale, -time, init.octaves)
	);
	offset *= directions;
	const glm::vec3 sign = init.sign->value->getVec3 ();
	offset = offset * (glm::vec3 (1.0f) - glm::abs (sign)) + glm::abs (offset) * sign;
	offset *= distance * glm::vec3 (1.0f, -1.0f, 1.0f);
	// The ordinary birth path applies this matrix after all initializers.
	p.position += worldSpace ? glm::inverse (basis) * offset : offset;
    };
}

InitializerFunc CParticle::createTurbulentVelocityRandomInitializer (const TurbulentVelocityRandomInitializer& init) {
    DynamicValue* speedMin = init.speedMin->value.get ();
    DynamicValue* speedMax = init.speedMax->value.get ();
    DynamicValue* offsetVal = init.offset->value.get ();
    DynamicValue* scaleVal = init.scale->value.get ();
    DynamicValue* forwardVal = init.forward->value.get ();
    DynamicValue* timeScaleVal = init.timeScale->value.get ();
    DynamicValue* phaseMinVal = init.phaseMin->value.get ();
    DynamicValue* phaseMaxVal = init.phaseMax->value.get ();
    DynamicValue* rightVal = init.right->value.get ();
    DynamicValue* speedOverride = getInstanceOverride ().speed->value.get ();

    return [this, speedMin, speedMax, offsetVal, scaleVal, forwardVal, timeScaleVal, phaseMinVal, phaseMaxVal, rightVal,
	    speedOverride] (ParticleInstance& p) {
	// Native opcode 9 rotates the authored forward vector with scalar time
	// noise. Sampling spatial curl made adjacent ribbon births point apart.
	const float phase = WallpaperEngine::Maths::randomFloat (m_rng, phaseMinVal->getFloat (), phaseMaxVal->getFloat ());
	const float speed = WallpaperEngine::Maths::randomFloat (m_rng, speedMin->getFloat (), speedMax->getFloat ());
	const float timeScale = timeScaleVal->getFloat () * m_turbulenceRate;
	const float sample = (getScene ().getTime () + phase) * timeScale;
	const float angle = glm::pi<float> () * scaleVal->getFloat () * simplexNoise1D (sample) + offsetVal->getFloat ();
	const glm::vec3 direction = glm::mat3 (glm::rotate (glm::mat4 (1.0f), angle, rightVal->getVec3 ()))
	    * forwardVal->getVec3 ();
	const float speedMultiplier = (m_particle.flags & 16) != 0 ? 1.0f : speedOverride->getFloat ();
	// Preserve authored forward magnitude; convert to simulation coordinates
	// only after rotation in Wallpaper Engine's coordinate system.
	p.velocity += direction * glm::vec3 (1.0f, -1.0f, 1.0f) * speed * speedMultiplier;
    };
}

InitializerFunc
CParticle::createMapSequenceBetweenControlPointsInitializer (const MapSequenceBetweenControlPointsInitializer& init) {
    includeControlPoint (init.controlPointStart);
    includeControlPoint (init.controlPointEnd);
    // Native 1401ca62c/1401ca637 gates the root count by this definition's flags.
    DynamicValue* countOverride = (m_particle.flags & 32) == 0 && (init.flags & 16) != 0
	? getInstanceOverride ().count->value.get () : nullptr;
    return [this, &init, countOverride, phase = 0.0f, direction = 1.0f] (ParticleInstance& particle) mutable {
	const auto point = [this] (const int index) {
	    const int clamped = std::clamp (index, 0, PARTICLE_CONTROL_POINT_COUNT - 1);
	    return clamped < static_cast<int> (m_controlPoints.size ())
		? m_controlPoints[clamped].position : glm::vec3 (0.0f);
	};
	initializeParticleBetweenControlPoints (
	    particle, init, point (init.controlPointStart), point (init.controlPointEnd), phase, direction,
	    countOverride ? countOverride->getFloat () : 1.0f
	);
    };
}

InitializerFunc
CParticle::createMapSequenceAroundControlPointInitializer (const MapSequenceAroundControlPointInitializer& init) {
    includeControlPoint (init.controlPoint->value->getInt ());
    DynamicValue* countValue = init.count->value.get ();
    DynamicValue* boundsValue = init.bounds->value.get ();
    DynamicValue* speedMinValue = init.speedMin->value.get ();
    DynamicValue* speedMaxValue = init.speedMax->value.get ();
    DynamicValue* axisValue = init.axis->value.get ();
    DynamicValue* controlPointValue = init.controlPoint->value.get ();
    DynamicValue* speedOverride = getInstanceOverride ().speed->value.get ();
    // Native 1401ca17d/1401ca184 uses initializer bit1 for circular spacing.
    DynamicValue* countOverride = (m_particle.flags & 32) == 0 && (init.flags & 1) != 0
	? getInstanceOverride ().count->value.get () : nullptr;
    const bool mirror = init.limitBehavior == "mirror";

    // The official initializer stores a normalized [0, 1] phase and a signed
    // 1/count step. Both are shared by every particle emitted by this initializer.
    return [this, countValue, boundsValue, speedMinValue, speedMaxValue, axisValue, controlPointValue,
	       speedOverride, countOverride, mirror, phase = 0.0f, direction = 1.0f] (ParticleInstance& p) mutable {
	constexpr float normalizationEpsilon = 1.1920929e-7f;

	const int controlPoint = std::clamp (static_cast<int> (controlPointValue->getFloat ()), 0,
					     PARTICLE_CONTROL_POINT_COUNT - 1);
	const glm::vec3 centerPos = controlPoint < static_cast<int> (m_controlPoints.size ())
	    ? m_controlPoints[controlPoint].position
	    : glm::vec3 (0.0f);

	glm::vec3 axis = axisValue->getVec3 ();
	const float axisLength = glm::length (axis);
	if (axisLength <= normalizationEpsilon) {
	    axis = glm::vec3 (0.0f, 0.0f, 1.0f);
	} else {
	    axis /= axisLength;
	}

	// Wallpaper Engine constructs these two perpendicular vectors when it
	// compiles the initializer. This particular orientation is significant:
	// phase zero starts on basis2 and advances toward basis1.
	glm::vec3 basis1;
	const float axisXYLength = glm::length (glm::vec2 (axis));
	if (axisXYLength <= normalizationEpsilon) {
	    basis1 = glm::vec3 (1.0f, 0.0f, 0.0f);
	} else {
	    basis1 = glm::vec3 (axis.y, -axis.x, 0.0f) / axisXYLength;
	}
	glm::vec3 basis2 = glm::normalize (glm::cross (axis, basis1));
	// Positions and control points use reflected simulation coordinates. Build
	// the authored basis first, then reflect all three vectors to retain phase
	// direction as well as the plane's orientation.
	axis.y = -axis.y;
	basis1.y = -basis1.y;
	basis2.y = -basis2.y;

	const glm::vec2 bounds = boundsValue->getVec2 ();
	const float angle = (bounds.x + phase * (bounds.y - bounds.x)) * glm::two_pi<float> ();
	const float sine = std::sin (angle);
	const float cosine = std::cos (angle);
	const glm::vec3 radialDirection = cosine * basis2 + sine * basis1;
	const glm::vec3 tangentDirection = -sine * basis2 + cosine * basis1;

	// Keep the emitter's radius and its displacement along the selected axis;
	// only its angle around the control point is replaced by the sequence.
	const glm::vec3 relativePosition = p.position - centerPos;
	const float axialDistance = glm::dot (relativePosition, axis);
	const glm::vec3 radialPosition = relativePosition - axialDistance * axis;
	p.position = centerPos + radialDirection * glm::length (radialPosition) + axis * axialDistance;

	// Authored X/Y/Z speed components are tangent/radial/axial respectively.
	const glm::vec3 speed
	    = WallpaperEngine::Maths::randomVec3 (m_rng, speedMinValue->getVec3 (), speedMaxValue->getVec3 ());
	const float speedMultiplier = (m_particle.flags & 16) != 0 ? 1.0f : speedOverride->getFloat ();
	p.velocity += (tangentDirection * speed.x + radialDirection * speed.y + axis * speed.z)
	    * speedMultiplier;

	// The compiler clamps count to 0.0001, not to an integer. Fractional
	// counts (including shipped values such as 3.02) are intentional.
	const float count = std::max (0.0001f, countValue->getFloat () * (countOverride ? countOverride->getFloat () : 1.0f));
	const float step = direction / count;
	phase += step;
	if (phase > 1.0f) {
	    if (mirror) {
		phase = 1.0f - (phase - 1.0f);
		direction = -direction;
	    } else {
		phase = std::fmod (phase, 1.0f);
	    }
	} else if (phase < 0.0f) {
	    phase = -phase;
	    direction = -direction;
	}
    };
}

// ========== OPERATORS ==========

static float particleOperatorBlend (const float lifetimePosition, glm::vec4 blendTimes) {
    // Native 1401c2a40 expands coincident endpoints and selects the blended
    // opcode only when its lifetime envelope has a meaningful duration.
    blendTimes.x = std::min (blendTimes.x, blendTimes.y - 0.0001f);
    blendTimes.w = std::max (blendTimes.w, blendTimes.z + 0.0001f);
    float weight = 1.0f;
    if ((blendTimes.y > 0.01f || blendTimes.z < 0.99f)
	&& (blendTimes.z - blendTimes.y > 0.01f || blendTimes.y - blendTimes.x > 0.01f
	    || blendTimes.w - blendTimes.z > 0.01f)) {
	weight = std::clamp ((lifetimePosition - blendTimes.x) / (blendTimes.y - blendTimes.x), 0.0f, 1.0f)
	    * std::clamp ((blendTimes.w - lifetimePosition) / (blendTimes.w - blendTimes.z), 0.0f, 1.0f);
    }
    return weight;
}

glm::vec3 WallpaperEngine::Render::Objects::calculateParticleVelocityCap (
    const glm::vec3& velocity, const float maxSpeed, const float lifetimePosition, glm::vec4 blendTimes
) {
    const float speed = glm::length (velocity);
    if (speed == 0.0f) return velocity;
    const float weight = particleOperatorBlend (lifetimePosition, blendTimes);
    // Native 140244790 blends the velocity toward its capped value each tick;
    // it does not blend the limit or multiply by the simulation delta.
    return velocity * (1.0f + std::min (0.0f, maxSpeed / speed - 1.0f) * weight);
}

glm::vec3 WallpaperEngine::Render::Objects::calculateParticleMovementReduction (
    const glm::vec3& velocity, const float distance, const glm::vec2 distanceRange, const glm::vec2 reductions,
    const float deltaTime, const float lifetimePosition, const glm::vec4 blendTimes
) {
    const float span = distanceRange.y == distanceRange.x ? 1.0f : distanceRange.y - distanceRange.x;
    // Native rsqrt(0)*0 followed by MINPS selects the outer endpoint at the exact center.
    const float position = distance == 0.0f ? 1.0f : std::clamp ((distance - distanceRange.x) / span, 0.0f, 1.0f);
    // Native parser 1401cd3d1 also uses a unit delta for equal reduction endpoints.
    const float change = reductions.y == reductions.x ? 1.0f : reductions.y - reductions.x;
    const float damping = std::clamp ((reductions.x + position * change) * deltaTime, 0.0f, 1.0f);
    return velocity * (1.0f - damping * particleOperatorBlend (lifetimePosition, blendTimes));
}

void WallpaperEngine::Render::Objects::applyParticleEventColor (
    ParticleInstance& particle, const ParticleInstance* parent, const glm::vec4 blendTimes
) {
    // Native opcodes 20/40 read the live parent's working RGBA, never a birth/death snapshot.
    if (!parent || !parent->isAlive ()) return;
    const float weight = particleOperatorBlend (particle.getLifetimePos (), blendTimes);
    particle.color += (parent->color - particle.color) * weight;
    particle.alpha += (parent->alpha - particle.alpha) * weight;
}

void WallpaperEngine::Render::Objects::applyParticleDistanceConstraint (
    ParticleInstance& particle, const ControlPointData& point, const glm::mat3& inverseBasis,
    const float distance, const float variableStrength, const float deltaTime, const glm::vec4 blendTimes
) {
    // Native opcodes 11/33 carry the point's translation without weighting it.
    // Only the radial correction receives strength and the lifetime envelope.
    particle.position += point.displacement;
    const glm::vec3 relative = particle.position - point.position;
    const float localDistance = glm::length (inverseBasis * relative);
    const float strength = variableStrength == 0.0f ? 1.0f : std::clamp (variableStrength * deltaTime, 0.0f, 1.0f);
    particle.position += relative * ((distance / localDistance - 1.0f) * strength
        * particleOperatorBlend (particle.getLifetimePos (), blendTimes));
}

void CParticle::setupOperators () {
    uint32_t collisionModelIndex = 0;
    for (const auto& op : m_particle.operators) {
	if (!op) {
	    continue;
	}

	OperatorFunc func;

	if (op->is<ControlPointRemapOperator> ()) {
	    func = createControlPointRemapOperator (*op->as<ControlPointRemapOperator> ());
	} else if (op->is<InheritEventColorOperator> ()) {
	    func = createInheritEventColorOperator (*op->as<InheritEventColorOperator> ());
	} else if (op->is<MovementOperator> ()) {
	    func = createMovementOperator (*op->as<MovementOperator> ());
	} else if (op->is<CollisionPlaneOperator> ()) {
	    func = createCollisionPlaneOperator (*op->as<CollisionPlaneOperator> ());
	} else if (op->is<CollisionQuadOperator> ()) {
	    func = createCollisionQuadOperator (*op->as<CollisionQuadOperator> ());
	} else if (op->is<CollisionSphereOperator> ()) {
	    func = createCollisionSphereOperator (*op->as<CollisionSphereOperator> ());
	} else if (op->is<CollisionModelOperator> ()) {
            func = createCollisionModelOperator (*op->as<CollisionModelOperator> (), collisionModelIndex++);
	} else if (op->is<BoidsOperator> ()) {
	    func = createBoidsOperator (*op->as<BoidsOperator> ());
	} else if (op->is<CollisionBoundsOperator> ()) {
	    func = createCollisionBoundsOperator (*op->as<CollisionBoundsOperator> ());
	} else if (op->is<CapVelocityOperator> ()) {
	    func = createCapVelocityOperator (*op->as<CapVelocityOperator> ());
	} else if (op->is<ReduceMovementNearControlPointOperator> ()) {
	    func = createReduceMovementNearControlPointOperator (*op->as<ReduceMovementNearControlPointOperator> ());
	} else if (op->is<MaintainDistanceToControlPointOperator> ()) {
	    func = createMaintainDistanceToControlPointOperator (*op->as<MaintainDistanceToControlPointOperator> ());
	} else if (op->is<MaintainDistanceBetweenControlPointsOperator> ()) {
	    func = createMaintainDistanceBetweenControlPointsOperator (*op->as<MaintainDistanceBetweenControlPointsOperator> ());
	} else if (op->is<AngularMovementOperator> ()) {
	    func = createAngularMovementOperator (*op->as<AngularMovementOperator> ());
	} else if (op->is<AlphaFadeOperator> ()) {
	    func = createAlphaFadeOperator (*op->as<AlphaFadeOperator> ());
	} else if (op->is<SizeChangeOperator> ()) {
	    func = createSizeChangeOperator (*op->as<SizeChangeOperator> ());
	} else if (op->is<AlphaChangeOperator> ()) {
	    func = createAlphaChangeOperator (*op->as<AlphaChangeOperator> ());
	} else if (op->is<ColorChangeOperator> ()) {
	    func = createColorChangeOperator (*op->as<ColorChangeOperator> ());
	} else if (op->is<TurbulenceOperator> ()) {
	    func = createTurbulenceOperator (*op->as<TurbulenceOperator> ());
	} else if (op->is<VortexOperator> ()) {
	    func = createVortexOperator (*op->as<VortexOperator> ());
	} else if (op->is<ControlPointAttractOperator> ()) {
	    func = createControlPointAttractOperator (*op->as<ControlPointAttractOperator> ());
	} else if (op->is<OscillateAlphaOperator> ()) {
	    func = createOscillateAlphaOperator (*op->as<OscillateAlphaOperator> ());
	} else if (op->is<OscillateSizeOperator> ()) {
	    func = createOscillateSizeOperator (*op->as<OscillateSizeOperator> ());
	} else if (op->is<OscillatePositionOperator> ()) {
	    func = createOscillatePositionOperator (*op->as<OscillatePositionOperator> ());
	} else {
	    sLog.out ("Unknown operator type");
	}

	if (func) {
	    m_operators.push_back (std::move (func));
	}
    }
}

OperatorFunc CParticle::createMovementOperator (const MovementOperator& op) {
    DynamicValue* dragValue = op.drag->value.get ();
    DynamicValue* gravityValue = op.gravity->value.get ();
    DynamicValue* speedOverride = getInstanceOverride ().speed->value.get ();

    return [this, dragValue, gravityValue, speedOverride] (
	       std::vector<ParticleInstance>& particles, uint32_t count, const std::vector<ControlPointData>&, float,
	       float dt
	   ) {
	float speed = (m_particle.flags & 16) != 0 ? 1.0f : speedOverride->getFloat ();
	float drag = dragValue->getFloat ();
	// Native 140237720 uses the scene frame interval before instance rate.
	// Position/gravity keep raw dt; only drag receives the adjusted force dt.
	const float forceDt = dt * std::pow (std::min (0.025f / getScene ().getDeltaTime (), 1.0f), 0.7f);
	glm::vec3 gravity = gravityValue->getVec3 ();
	// Flip gravity Y for centered space
	gravity.y = -gravity.y;

	for (uint32_t i = 0; i < count; i++) {
	    auto& p = particles[i];
	    if (!p.shouldSimulate ()) {
		continue;
	    }

	    // Native snapshots each movement operator's start for swept attraction deletion.
	    if (!usesRoundedParticleSlots () || i < m_boidsSlotExtent) p.previousPosition = p.position;
	    // Native movement applies gravity before advancing position, then drag.
	    // Otherwise a large drag*dt discards every frame's force without motion.
	    p.velocity += gravity * dt * speed;
	    p.position += p.velocity * dt;

	    p.velocity *= 1.0f - std::min (drag * forceDt, 1.0f - std::numeric_limits<float>::epsilon ());
	}
    };
}

void WallpaperEngine::Render::Objects::applyParticlePlaneCollision (
    ParticleInstance& particle, const glm::vec3& plane, const float distance, const float bounceFactor,
    const ParticleCollisionBehavior behavior, const bool stopRotation
) {
    const float projection = glm::dot (particle.position, plane);
    // Native opcode 21 tests only the current position, with a strict boundary.
    if (!(projection < distance)) return;
    if (behavior == ParticleCollisionBehavior::Delete) {
        particle.age = particle.lifetime;
    } else {
        particle.position -= plane * (projection - distance);
        if (behavior == ParticleCollisionBehavior::Stop) particle.velocity = glm::vec3 (0.0f);
        else {
            const float response = behavior == ParticleCollisionBehavior::Slide ? 1.0f : 1.0f + bounceFactor;
            particle.velocity -= plane * (response * glm::dot (particle.velocity, plane));
        }
    }
    if (stopRotation) particle.angularVelocity = glm::vec3 (0.0f);
}

OperatorFunc CParticle::createCollisionPlaneOperator (const CollisionPlaneOperator& op) {
    includeControlPoint (op.controlPoint);
    const float defaultDistance = getScene ().getScene ().camera.projection.isPerspective ? 0.0f : -150.0f;
    glm::vec3 plane = op.plane;
    plane.y = -plane.y;
    return [this, &op, plane, defaultDistance] (
        std::vector<ParticleInstance>& particles, uint32_t count,
        const std::vector<ControlPointData>& controlPoints, float, float
    ) {
        glm::vec3 currentPlane = plane;
        float distance = op.distance ? op.distance->value->getFloat () : defaultDistance;
        if ((op.flags & 1) != 0) {
            const auto& point = controlPoints[op.controlPoint];
            const glm::mat3 basis = (m_particle.flags & 1) != 0
                ? glm::mat3 (m_controlPointTransform) * point.orientation : point.orientation;
            // Native transforms the normalized authored plane without normalizing again.
            currentPlane = basis * plane;
            distance = glm::dot (currentPlane, point.position);
        }
        const float bounceFactor = op.bounceFactor->value->getFloat ();
        for (uint32_t i = 0; i < count; ++i) {
            auto& particle = particles[i];
            if (particle.shouldSimulate ()) applyParticlePlaneCollision (
                particle, currentPlane, distance, bounceFactor, op.behavior, (op.flags & 2) != 0
            );
        }
    };
}

void WallpaperEngine::Render::Objects::applyParticleQuadCollision (
    ParticleInstance& particle, const glm::vec3& origin, const glm::vec3& plane,
    const glm::vec3& right, const glm::vec3& forward, const glm::vec2& halfSize, const float bounceFactor,
    const ParticleCollisionBehavior behavior, const bool stopRotation
) {
    const glm::vec3 relative = particle.position - origin;
    const float projection = glm::dot (relative, plane);
    // Native quad tests a one-way crossing and the current endpoint's strict bounds.
    if (!(glm::dot (particle.previousPosition - origin, plane) > 0.0f && projection <= 0.0f
          && std::abs (glm::dot (relative, forward)) < halfSize.y
          && std::abs (glm::dot (relative, right)) < halfSize.x)) return;
    if (behavior == ParticleCollisionBehavior::Delete) {
        particle.age = particle.lifetime;
    } else {
        particle.position -= plane * (1.05f * projection);
        if (behavior == ParticleCollisionBehavior::Stop) particle.velocity = glm::vec3 (0.0f);
        else {
            const float response = behavior == ParticleCollisionBehavior::Slide ? 1.0f : 1.0f + bounceFactor;
            particle.velocity -= plane * (response * glm::dot (particle.velocity, plane));
        }
    }
    if (stopRotation) particle.angularVelocity = glm::vec3 (0.0f);
}

OperatorFunc CParticle::createCollisionQuadOperator (const CollisionQuadOperator& op) {
    includeControlPoint (op.controlPoint);
    const bool perspective = getScene ().getScene ().camera.projection.isPerspective;
    const glm::vec3 defaultOrigin = perspective ? glm::vec3 (0.0f) : glm::vec3 (0.0f, -150.0f, 0.0f);
    const glm::vec2 defaultSize = perspective ? glm::vec2 (1.0f) : glm::vec2 (200.0f);
    const glm::vec3 flipY (1.0f, -1.0f, 1.0f);
    return [this, &op, defaultOrigin, defaultSize, flipY] (
        std::vector<ParticleInstance>& particles, uint32_t count,
        const std::vector<ControlPointData>& controlPoints, float, float
    ) {
        glm::vec3 origin = (op.origin ? op.origin->value->getVec3 () : defaultOrigin) * flipY;
        glm::vec3 plane = op.plane * flipY;
        glm::vec3 right = op.right * flipY;
        glm::vec3 forward = op.forward * flipY;
        if ((op.flags & 1) != 0) {
            const auto& point = controlPoints[op.controlPoint];
            const glm::mat3 basis = (m_particle.flags & 1) != 0
                ? glm::mat3 (m_controlPointTransform) * point.orientation : point.orientation;
            origin = point.position;
            plane = basis * plane;
            right = basis * right;
            forward = basis * forward;
        }
        const glm::vec2 halfSize = (op.size ? op.size->value->getVec2 () : defaultSize) * 0.5f;
        for (uint32_t i = 0; i < count; ++i) {
            auto& particle = particles[i];
            if (particle.shouldSimulate ()) applyParticleQuadCollision (
                particle, origin, plane, right, forward, halfSize, op.bounceFactor->value->getFloat (),
                op.behavior, (op.flags & 2) != 0
            );
        }
    };
}

void WallpaperEngine::Render::Objects::applyParticleSphereCollision (
    ParticleInstance& particle, const glm::vec3& origin, const float radius, const float bounceFactor,
    const ParticleCollisionBehavior behavior, const bool stopRotation
) {
    const glm::vec3 relative = particle.position - origin;
    const float squared = (relative.y * relative.y + relative.x * relative.x) + relative.z * relative.z;
    // Native opcode 22 tests the current strict interior, including signed radii.
    if (!(squared < radius * radius)) return;
    if (behavior == ParticleCollisionBehavior::Delete) {
        particle.age = particle.lifetime;
    } else {
        const float inverseLength = particleRemapInverseLength (squared);
        const glm::vec3 normal = relative * inverseLength;
        particle.position -= normal * (squared * inverseLength - radius);
        if (behavior == ParticleCollisionBehavior::Stop) particle.velocity = glm::vec3 (0.0f);
        else {
            const float projection = (particle.velocity.y * normal.y + particle.velocity.x * normal.x)
                + particle.velocity.z * normal.z;
            const float response = behavior == ParticleCollisionBehavior::Slide ? 1.0f : 1.0f + bounceFactor;
            particle.velocity -= normal * (response * projection);
        }
    }
    if (stopRotation) particle.angularVelocity = glm::vec3 (0.0f);
}

OperatorFunc CParticle::createCollisionSphereOperator (const CollisionSphereOperator& op) {
    includeControlPoint (op.controlPoint);
    const bool perspective = getScene ().getScene ().camera.projection.isPerspective;
    const glm::vec3 defaultOrigin = perspective ? glm::vec3 (0.0f) : glm::vec3 (0.0f, -200.0f, 0.0f);
    const float defaultRadius = perspective ? 1.0f : 50.0f;
    return [&op, defaultOrigin, defaultRadius] (
        std::vector<ParticleInstance>& particles, uint32_t count,
        const std::vector<ControlPointData>& controlPoints, float, float
    ) {
        const glm::vec3 origin = (op.flags & 1) != 0 ? controlPoints[op.controlPoint].position
            : (op.origin ? op.origin->value->getVec3 () : defaultOrigin) * glm::vec3 (1.0f, -1.0f, 1.0f);
        const float radius = op.radius ? op.radius->value->getFloat () : defaultRadius;
        const float bounceFactor = op.bounceFactor->value->getFloat ();
        for (uint32_t i = 0; i < count; ++i) {
            auto& particle = particles[i];
            if (particle.shouldSimulate ()) applyParticleSphereCollision (
                particle, origin, radius, bounceFactor, op.behavior, (op.flags & 2) != 0
            );
        }
    };
}

static float particleBoidsReciprocal (const float value) {
#if defined(__SSE__)
    return _mm_cvtss_f32 (_mm_rcp_ss (_mm_set_ss (value)));
#else
    return 1.0f / value;
#endif
}

std::optional<int> WallpaperEngine::Render::Objects::resolveParticleModelCollisionDependency (
    const std::vector<ObjectDependency>& dependencies, const uint32_t index
) {
    std::optional<int> sourceId;
    for (const auto& dependency : dependencies)
        if (dependency.type == "collisionmodel" && dependency.index == index && dependency.id != 0)
            sourceId = dependency.id;
    return sourceId;
}

glm::mat4 WallpaperEngine::Render::Objects::calculateParticleModelCollisionSpace (
    const glm::mat4& ownerWorld, const bool worldSpace
) {
    const glm::mat4 flipY = glm::scale (glm::mat4 (1.0f), glm::vec3 (1.0f, -1.0f, 1.0f));
    return worldSpace ? flipY : flipY * glm::inverse (ownerWorld);
}

ParticleModelCapsule WallpaperEngine::Render::Objects::calculateParticleModelCapsule (
    const glm::vec3& halfExtents, const glm::vec3& center, const glm::mat4& transform, const bool boneHitBox
) {
    glm::vec3 axis (0.0f, 1.0f, 0.0f), radial (0.0f);
    float halfSegment = 0.0f;
    if (!boneHitBox || glm::dot (halfExtents, halfExtents) >= 0.01f) {
        // Native chooses axes before applying scale; the original X/Y loser wins radial ties.
        const glm::vec3 x (halfExtents.x, 0.0f, 0.0f), y (0.0f, halfExtents.y, 0.0f), z (0.0f, 0.0f, halfExtents.z);
        const bool longestX = halfExtents.x * halfExtents.x >= halfExtents.y * halfExtents.y;
        glm::vec3 longest = longestX ? x : y;
        const glm::vec3 firstRadial = longestX ? y : x;
        glm::vec3 secondRadial = z;
        if (halfExtents.z * halfExtents.z > glm::dot (longest, longest)) {
            secondRadial = longest;
            longest = z;
        }
        radial = glm::dot (secondRadial, secondRadial) > glm::dot (firstRadial, firstRadial)
            ? secondRadial : firstRadial;
        const float longestExtent = (longest.x + longest.y) + longest.z;
        axis = longest / longestExtent;
        halfSegment = longestExtent - ((radial.x + radial.y) + radial.z);
    }
    const glm::mat3 basis (transform);
    const glm::vec3 transformedCenter (transform * glm::vec4 (center, 1.0f));
    const glm::vec3 a = transformedCenter + basis * (-axis * halfSegment);
    const glm::vec3 b = transformedCenter + basis * (axis * halfSegment);
    return { a, glm::normalize (basis * axis), glm::length (b - a), glm::length (basis * radial) };
}

void WallpaperEngine::Render::Objects::applyParticleModelCollision (
    ParticleInstance& particle, const std::vector<ParticleModelCapsule>& capsules, const float bounceFactor,
    const ParticleCollisionBehavior behavior
) {
    glm::vec3 normal (0.0f);
    float penetration = 0.0f;
    bool hit = false;
    for (const auto& capsule : capsules) {
        const glm::vec3 relative = particle.position - capsule.origin;
        const float projection = (relative.y * capsule.direction.y + relative.x * capsule.direction.x)
            + relative.z * capsule.direction.z;
        // MAXPS then MINPS select the second operand on unordered inputs.
        const float lower = projection > 0.0f ? projection : 0.0f;
        const float t = lower < capsule.length ? lower : capsule.length;
        const glm::vec3 delta = particle.position - (capsule.origin + capsule.direction * t);
        const float squared = (delta.y * delta.y + delta.x * delta.x) + delta.z * delta.z;
        if (!(squared < capsule.radius * capsule.radius)) continue;
        hit = true;
        if (behavior == ParticleCollisionBehavior::Delete) break;
        const float inverseLength = particleRemapInverseLength (squared);
        normal = delta * inverseLength;
        penetration = squared * inverseLength - capsule.radius;
    }
    if (!hit) return;
    if (behavior == ParticleCollisionBehavior::Delete) {
        particle.age = particle.lifetime;
        return;
    }
    // All tests use the original position; the last intersecting capsule supplies the response.
    particle.position -= normal * penetration;
    if (behavior == ParticleCollisionBehavior::Stop) particle.velocity = glm::vec3 (0.0f);
    else {
        const float projection = (particle.velocity.y * normal.y + particle.velocity.x * normal.x)
            + particle.velocity.z * normal.z;
        if (behavior == ParticleCollisionBehavior::Slide) particle.velocity -= normal * projection;
        else particle.velocity += normal * ((-1.0f - bounceFactor) * projection);
    }
}

OperatorFunc CParticle::createCollisionModelOperator (const CollisionModelOperator& op, const uint32_t dependencyIndex) {
    // Model collision uses the same rounded high-water backing as boids, including inactive/padded lanes.
    if (!usesRoundedParticleSlots ()) {
        m_remapSlots.resize ((m_maxParticles + 3u) & ~3u);
        for (auto& slot : m_remapSlots) { slot.lifetime = 0.0f; slot.nativeSlotState = true; }
    }
    m_hasModelCollisionOperator = true;
    const auto dependency = resolveParticleModelCollisionDependency (m_particle.typedDependencies, dependencyIndex);
    if (!dependency) return {};
    const int sourceId = *dependency;
    return [this, sourceId, bounceFactor = op.bounceFactor, behavior = op.behavior] (
        std::vector<ParticleInstance>& particles, uint32_t count,
        const std::vector<ControlPointData>&, float, float
    ) {
        // Resolve the live object before dereferencing it: script destroyLayer can remove a target.
        const auto* target = dynamic_cast<const CModel*> (getScene ().getObject (sourceId));
        if (!target) return;
        const auto& model = target->getModel ();
        const auto& hitBoxes = model.animationData.hitBoxes;
        const glm::mat4 simulation = calculateParticleModelCollisionSpace (particleWorldMatrix (), (m_particle.flags & 1) != 0);
        std::vector<ParticleModelCapsule> capsules;
        if (hitBoxes.empty ()) {
            const auto& mesh = model.mesh;
            if (!(mesh.boundingBoxMin.x < mesh.boundingBoxMax.x)) return;
            const glm::vec3 halfExtents = (mesh.boundingBoxMax - mesh.boundingBoxMin) * 0.5f;
            capsules.push_back (calculateParticleModelCapsule (
                halfExtents, mesh.boundingBoxMin + halfExtents, simulation * target->resolveWorldMatrix ()
            ));
        } else {
            capsules.reserve (hitBoxes.size ());
            for (size_t index = 0; index < hitBoxes.size (); ++index) {
                const auto bone = target->getBoneTransform (index);
                if (!bone) continue;
                capsules.push_back (calculateParticleModelCapsule (
                    hitBoxes[index].halfExtents, glm::vec3 (0.0f), simulation * *bone * hitBoxes[index].local, true
                ));
            }
        }
        const uint32_t divisor = m_boidsSlotExtent / 10000u + 1u;
        for (uint32_t base = 4u * (getScene ().getSceneFrame () % divisor); base < count; base += 4u * divisor)
            for (uint32_t lane = 0; lane < 4u; ++lane)
                applyParticleModelCollision (particles[base + lane], capsules, bounceFactor, behavior);
    };

}

OperatorFunc CParticle::createBoidsOperator (const BoidsOperator& op) {
    if (!m_hasBoidsOperator) {
        m_hasBoidsOperator = true;
        m_remapSlots.resize ((m_maxParticles + 3u) & ~3u);
        for (auto& slot : m_remapSlots) {
            slot.lifetime = 0.0f;
            slot.nativeSlotState = true;
        }
    }
    const bool perspective = getScene ().getScene ().camera.projection.isPerspective;
    const float separationThreshold = op.separationThreshold.value_or (perspective ? 0.02f : 20.0f);
    const float neighborThreshold = op.neighborThreshold.value_or (perspective ? 0.2f : 50.0f);
    const float maxSpeed = op.maxSpeed.value_or (perspective ? 1.0f : 500.0f);
    return [this, op, separationThreshold, neighborThreshold, maxSpeed] (
        std::vector<ParticleInstance>& particles, uint32_t,
        const std::vector<ControlPointData>&, float, float dt
    ) {
        const uint32_t count = m_boidsSlotExtent;
        const uint32_t stride = count / 200u + 1u;
        const uint32_t frame = getScene ().getSceneFrame ();
        const float forceDt = dt * std::pow (std::min (0.025f / getScene ().getDeltaTime (), 1.0f), 0.7f);
        const float separationFactor = (float (stride) * op.separationFactor) * forceDt;
        const float alignmentFactor = (float (stride) * op.alignmentFactor) * forceDt;
        const float cohesionFactor = (float (stride) * op.cohesionFactor) * forceDt;
        for (uint32_t base = 4u * (frame % stride); base < count; base += 4u * stride) {
            std::array<glm::vec3, 4> separation {}, position {}, velocity {}, result {};
            std::array<uint32_t, 4> separationCount {}, neighborCount {};
            for (uint32_t neighbor = 4u * ((frame + base) % stride); neighbor < count; neighbor += 4u * stride) {
                for (uint32_t rotation = 0; rotation < 4; ++rotation) {
                    for (uint32_t lane = 0; lane < 4; ++lane) {
                        // Native rotates coordinates/velocity while retaining this unrotated lane mask.
                        if (particles[neighbor + lane].lifetime == 0.0f) continue;
                        const auto& other = particles[neighbor + ((lane - rotation) & 3u)];
                        const glm::vec3 delta = particles[base + lane].position - other.position;
                        const float squared = glm::dot (delta, delta);
                        if (squared == 0.0f) continue;
                        const float inverseDistance = particleRemapInverseLength (squared);
                        const float distance = squared * inverseDistance;
                        if (distance < separationThreshold) {
                            separation[lane] += delta * (separationThreshold * inverseDistance - 1.0f);
                            ++separationCount[lane];
                        }
                        if (distance < neighborThreshold) {
                            position[lane] += other.position;
                            velocity[lane] += other.velocity;
                            ++neighborCount[lane];
                        }
                    }
                }
            }
            for (uint32_t lane = 0; lane < 4; ++lane) {
                const auto& particle = particles[base + lane];
                const float neighborInverse = neighborCount[lane] != 0
                    ? particleBoidsReciprocal (float (neighborCount[lane])) : 0.0f;
                glm::vec3 delta (0.0f);
                if (neighborCount[lane] != 0) delta = (velocity[lane] * neighborInverse - particle.velocity)
                    * alignmentFactor;
                if (separationCount[lane] != 0) delta += separation[lane]
                    * (particleBoidsReciprocal (float (separationCount[lane])) * separationFactor);
                if (neighborCount[lane] != 0) delta += (position[lane] * neighborInverse - particle.position)
                    * cohesionFactor;
                result[lane] = particle.velocity + delta;
                const float squared = glm::dot (result[lane], result[lane]);
                if ((op.flags & 1) != 0 && squared > std::max (glm::dot (particle.velocity, particle.velocity), maxSpeed * maxSpeed))
                    result[lane] *= maxSpeed * particleRemapInverseLength (squared);
            }
            // Write four lanes together; later batches read these updated velocities.
            for (uint32_t lane = 0; lane < 4; ++lane) particles[base + lane].velocity = result[lane];
        }
    };
}

std::array<glm::vec4, 4> WallpaperEngine::Render::Objects::calculateParticleBoundsPlanes (
    const glm::ivec2& extent, const glm::mat4& worldToLocal
) {
    const glm::vec3 first (worldToLocal * glm::vec4 (0, 0, 0, 1));
    const glm::vec3 last (worldToLocal * glm::vec4 (extent.x, -static_cast<float> (extent.y), 0, 1));
    std::array<glm::vec4, 4> planes = {
        glm::vec4 (1, 0, 0, 0), glm::vec4 (0, -1, 0, 0), glm::vec4 (-1, 0, 0, 0), glm::vec4 (0, 1, 0, 0)
    };
    for (size_t i = 0; i < planes.size (); ++i) {
        planes[i] = glm::vec4 (glm::vec3 (worldToLocal * planes[i]), 0.0f);
        planes[i].w = glm::dot (i < 2 ? first : last, glm::vec3 (planes[i]));
    }
    return planes;
}

void WallpaperEngine::Render::Objects::applyParticleBoundsCollision (
    ParticleInstance& particle, const std::array<glm::vec4, 4>& planes, const float bounceFactor,
    const ParticleCollisionBehavior behavior
) {
    // Native selects only the last violating plane, before modifying the position.
    for (int i = 3; i >= 0; --i) {
        const glm::vec3 normal (planes[i]);
        if (!(glm::dot (particle.position, normal) < planes[i].w)) continue;
        applyParticlePlaneCollision (particle, normal, planes[i].w, bounceFactor, behavior, false);
        return;
    }
}

OperatorFunc CParticle::createCollisionBoundsOperator (const CollisionBoundsOperator& op) {
    const auto& projection = getScene ().getScene ().camera.projection;
    if (projection.isAuto && !getScene ().getAutoCollisionBoundsExtent ()) {
        sLog.out ("Warning: collisionbounds disabled for auto projection in particle ", m_particle.name,
            "; native first-image sizing is not implemented");
        return {};
    }
    const glm::ivec2 extent = projection.isAuto ? *getScene ().getAutoCollisionBoundsExtent ()
        : projection.isPerspective ? glm::ivec2 (0) : glm::ivec2 (projection.width, projection.height);
    const glm::mat4 flipY = glm::scale (glm::mat4 (1.0f), glm::vec3 (1.0f, -1.0f, 1.0f));
    return [this, &op, extent, flipY] (
        std::vector<ParticleInstance>& particles, uint32_t count, const std::vector<ControlPointData>&, float, float
    ) {
        const glm::mat4 worldToLocal = (m_particle.flags & 1) != 0 ? glm::mat4 (1.0f)
            : glm::inverse (flipY * particleWorldMatrix () * flipY);
        const auto planes = calculateParticleBoundsPlanes (extent, worldToLocal);
        for (uint32_t i = 0; i < count; ++i) {
            auto& particle = particles[i];
            if (particle.shouldSimulate ()) applyParticleBoundsCollision (
                particle, planes, op.bounceFactor->value->getFloat (), op.behavior
            );
        }
    };
}

OperatorFunc CParticle::createCapVelocityOperator (const CapVelocityOperator& op) {
    DynamicValue* maxSpeedValue = op.maxSpeed ? op.maxSpeed->value.get () : nullptr;
    const float defaultMaxSpeed = getScene ().getScene ().camera.projection.isPerspective ? 1.0f : 100.0f;
    return [maxSpeedValue, defaultMaxSpeed, blendTimes = op.blendTimes] (
	       std::vector<ParticleInstance>& particles, uint32_t count, const std::vector<ControlPointData>&,
	       float, float
	   ) {
	const float maxSpeed = maxSpeedValue ? maxSpeedValue->getFloat () : defaultMaxSpeed;
	for (uint32_t i = 0; i < count; i++) {
	    auto& particle = particles[i];
	    if (particle.shouldSimulate ()) {
		particle.velocity = calculateParticleVelocityCap (
		    particle.velocity, maxSpeed, particle.getLifetimePos (), blendTimes
		);
	    }
	}
    };
}

OperatorFunc CParticle::createReduceMovementNearControlPointOperator (const ReduceMovementNearControlPointOperator& op) {
    includeControlPoint (op.controlPoint);
    const bool perspective = getScene ().getScene ().camera.projection.isPerspective;
    return [&op, perspective] (
        std::vector<ParticleInstance>& particles, uint32_t count,
        const std::vector<ControlPointData>& controlPoints, float, float dt
    ) {
        const glm::vec2 distances (
            op.distanceInner ? op.distanceInner->value->getFloat () : (perspective ? 0.5f : 100.0f),
            op.distanceOuter ? op.distanceOuter->value->getFloat () : (perspective ? 1.0f : 350.0f)
        );
        const glm::vec2 reductions (op.reductionInner->value->getFloat (), op.reductionOuter->value->getFloat ());
        const glm::vec3 center = controlPoints[op.controlPoint].position;
        for (uint32_t i = 0; i < count; ++i) {
            auto& particle = particles[i];
            if (!particle.shouldSimulate ()) continue;
            particle.velocity = calculateParticleMovementReduction (
                particle.velocity, glm::length (particle.position - center), distances, reductions,
                dt, particle.getLifetimePos (), op.blendTimes
            );
        }
    };
}

OperatorFunc CParticle::createInheritEventColorOperator (const InheritEventColorOperator& op) {
    m_hasColorOperators = true;
    m_hasAlphaOperators = true;
    return [this, &op] (
        std::vector<ParticleInstance>& particles, uint32_t count,
        const std::vector<ControlPointData>&, float, float
    ) {
        if (!m_particleParent || m_eventParentSerial == 0) return;
        const auto end = m_particleParent->m_particles.begin () + m_particleParent->m_particleCount;
        const auto parent = std::find_if (m_particleParent->m_particles.begin (), end, [this] (const auto& particle) {
            return particle.serial == m_eventParentSerial && particle.isAlive ();
        });
        if (parent == end) return;
        for (uint32_t i = 0; i < count; ++i) {
            if (particles[i].shouldSimulate ()) applyParticleEventColor (particles[i], &*parent, op.blendTimes);
        }
    };
}

OperatorFunc CParticle::createMaintainDistanceToControlPointOperator (const MaintainDistanceToControlPointOperator& op) {
    includeControlPoint (op.controlPoint);
    const float defaultDistance = getScene ().getScene ().camera.projection.isPerspective ? 1.0f : 200.0f;
    return [this, &op, defaultDistance] (
        std::vector<ParticleInstance>& particles, uint32_t count,
        const std::vector<ControlPointData>& controlPoints, float, float dt
    ) {
        const auto& point = controlPoints[op.controlPoint];
        const glm::mat3 basis = (m_particle.flags & 1) != 0
            ? glm::mat3 (m_controlPointTransform) * point.orientation : point.orientation;
        const glm::mat3 inverseBasis = glm::inverse (basis);
        const float distance = op.distance ? op.distance->value->getFloat () : defaultDistance;
        const float strength = op.variableStrength->value->getFloat ();
        for (uint32_t i = 0; i < count; ++i) {
            auto& particle = particles[i];
            if (!particle.shouldSimulate ()) continue;
            applyParticleDistanceConstraint (particle, point, inverseBasis, distance, strength, dt, op.blendTimes);
        }
    };
}

void WallpaperEngine::Render::Objects::applyParticleBetweenControlPointConstraint (
    std::vector<ParticleInstance>& particles, const uint32_t count, const ControlPointData& start,
    const ControlPointData& end, const glm::vec4 blendTimes
) {
    const glm::vec3 previousStart = start.position - start.displacement;
    const glm::vec3 previousSegment = end.position - end.displacement - previousStart;
    const glm::vec3 currentSegment = end.position - start.position;
    const float previousLengthSquared = glm::dot (previousSegment, previousSegment);
    const float currentLengthSquared = glm::dot (currentSegment, currentSegment);
    constexpr float epsilon = std::numeric_limits<float>::epsilon ();
    // Native opcodes 12/34 skip either segment at or below FLT_EPSILON squared.
    if (previousLengthSquared <= epsilon * epsilon || currentLengthSquared <= epsilon * epsilon) return;
    const float previousLength = std::sqrt (previousLengthSquared);
    const glm::vec3 previousAxis = previousSegment / previousLength;
    for (uint32_t index = 0; index < count; ++index) {
        auto& particle = particles[index];
        if (!particle.shouldSimulate ()) continue;
        const float distance = glm::dot (particle.position - previousStart, previousAxis);
        const float fraction = glm::clamp (distance / previousLength, 0.0f, 1.0f);
        // Preserve the world perpendicular residual; weight translation too.
        particle.position += (start.displacement + currentSegment * fraction - previousAxis * distance)
            * particleOperatorBlend (particle.getLifetimePos (), blendTimes);
    }
}

OperatorFunc CParticle::createMaintainDistanceBetweenControlPointsOperator (
    const MaintainDistanceBetweenControlPointsOperator& op
) {
    includeControlPoint (op.controlPointStart);
    includeControlPoint (op.controlPointEnd);
    return [&op] (
        std::vector<ParticleInstance>& particles, uint32_t count,
        const std::vector<ControlPointData>& controlPoints, float, float
    ) {
        applyParticleBetweenControlPointConstraint (
            particles, count, controlPoints[op.controlPointStart], controlPoints[op.controlPointEnd], op.blendTimes
        );
    };
}

void WallpaperEngine::Render::Objects::applyParticleAngularMovement (
    ParticleInstance& particle, const glm::vec3& force, const float drag,
    const float rawDelta, const float forceDelta, const float lifetimeWeight
) {
    // Native opcodes 2/28 accelerate before integrating rotation, then damp
    // velocity. The envelope therefore weights this step's force contribution
    // to rotation twice. Angular force/rotation use raw time; drag uses force time.
    particle.angularVelocity += force * (rawDelta * lifetimeWeight);
    particle.rotation += particle.angularVelocity * (rawDelta * lifetimeWeight);
    const float damping = std::min (drag * forceDelta, 1.0f - std::numeric_limits<float>::epsilon ());
    particle.angularVelocity *= 1.0f - lifetimeWeight * damping;
    // Native preserves accumulated angles; wrapping here changes shader inputs.
}

OperatorFunc CParticle::createAngularMovementOperator (const AngularMovementOperator& op) {
    m_hasAngularVelocity = true;
    return [this, &op] (
        std::vector<ParticleInstance>& particles, uint32_t count, const std::vector<ControlPointData>&,
        float, float dt
    ) {
        const float speed = (m_particle.flags & 16) != 0 ? 1.0f : getInstanceOverride ().speed->value->getFloat ();
        const glm::vec3 force = op.force->value->getVec3 () * speed;
        const float drag = op.drag->value->getFloat ();
        const float forceDt = dt * std::pow (std::min (0.025f / getScene ().getDeltaTime (), 1.0f), 0.7f);
        for (uint32_t i = 0; i < count; ++i) {
            auto& particle = particles[i];
            if (!particle.shouldSimulate ()) continue;
            applyParticleAngularMovement (
                particle, force, drag, dt, forceDt, particleOperatorBlend (particle.getLifetimePos (), op.blendTimes)
            );
        }
    };
}

OperatorFunc CParticle::createAlphaFadeOperator (const AlphaFadeOperator& op) {
    m_hasAlphaOperators = true;
    DynamicValue* fadeInTimeValue = op.fadeInTime->value.get ();
    DynamicValue* fadeOutTimeValue = op.fadeOutTime->value.get ();

    return
	[fadeInTimeValue, fadeOutTimeValue] (
	    std::vector<ParticleInstance>& particles, uint32_t count, const std::vector<ControlPointData>&, float, float
	) {
	    float fadeInTime = fadeInTimeValue->getFloat ();
	    float fadeOutTime = fadeOutTimeValue->getFloat ();

	    for (uint32_t i = 0; i < count; i++) {
		auto& p = particles[i];
		if (!p.shouldSimulate ()) {
		    continue;
		}

		float life = p.getLifetimePos ();

		if (life <= fadeInTime) {
		    float fade = WallpaperEngine::Maths::fadeValue (life, 0.0f, fadeInTime, 0.0f, 1.0f);
		    p.alpha *= fade;
		} else if (life > fadeOutTime) {
		    float fade = 1.0f - WallpaperEngine::Maths::fadeValue (life, fadeOutTime, 1.0f, 0.0f, 1.0f);
		    p.alpha *= fade;
		}
	    }
	};
}

OperatorFunc CParticle::createSizeChangeOperator (const SizeChangeOperator& op) {
    DynamicValue* startTimeValue = op.startTime->value.get ();
    DynamicValue* endTimeValue = op.endTime->value.get ();
    DynamicValue* startValueValue = op.startValue->value.get ();
    DynamicValue* endValueValue = op.endValue->value.get ();
    DynamicValue* sizeOverride = getInstanceOverride ().size->value.get ();

    return
	[this, startTimeValue, endTimeValue, startValueValue, endValueValue, sizeOverride] (
	    std::vector<ParticleInstance>& particles, uint32_t count, const std::vector<ControlPointData>&, float, float
	) {
	    float startTime = startTimeValue->getFloat ();
	    float endTime = endTimeValue->getFloat ();
	    const float sizeMultiplier = (m_particle.flags & 128) != 0 ? 1.0f : sizeOverride->getFloat ();
	    float startValue = startValueValue->getFloat () * sizeMultiplier;
	    float endValue = endValueValue->getFloat () * sizeMultiplier;

	    for (uint32_t i = 0; i < count; i++) {
		auto& p = particles[i];
		if (!p.shouldSimulate ()) {
		    continue;
		}

		float life = p.getLifetimePos ();
		float multiplier = WallpaperEngine::Maths::fadeValue (life, startTime, endTime, startValue, endValue);
		p.size *= multiplier;
	    }
	};
}

OperatorFunc CParticle::createAlphaChangeOperator (const AlphaChangeOperator& op) {
    m_hasAlphaOperators = true;
    DynamicValue* startTimeValue = op.startTime->value.get ();
    DynamicValue* endTimeValue = op.endTime->value.get ();
    DynamicValue* startValueValue = op.startValue->value.get ();
    DynamicValue* endValueValue = op.endValue->value.get ();

    return
	[startTimeValue, endTimeValue, startValueValue, endValueValue] (
	    std::vector<ParticleInstance>& particles, uint32_t count, const std::vector<ControlPointData>&, float, float
	) {
	    float startTime = startTimeValue->getFloat ();
	    float endTime = endTimeValue->getFloat ();
	    float startValue = startValueValue->getFloat ();
	    float endValue = endValueValue->getFloat ();

	    for (uint32_t i = 0; i < count; i++) {
		auto& p = particles[i];
		if (!p.shouldSimulate ()) {
		    continue;
		}

		float life = p.getLifetimePos ();
		float multiplier = WallpaperEngine::Maths::fadeValue (life, startTime, endTime, startValue, endValue);
		p.alpha *= multiplier;
	    }
	};
}

OperatorFunc CParticle::createColorChangeOperator (const ColorChangeOperator& op) {
    m_hasColorOperators = true;
    DynamicValue* startTimeValue = op.startTime->value.get ();
    DynamicValue* endTimeValue = op.endTime->value.get ();
    DynamicValue* startValueValue = op.startValue->value.get ();
    DynamicValue* endValueValue = op.endValue->value.get ();

    return
	[this, startTimeValue, endTimeValue, startValueValue, endValueValue] (
	    std::vector<ParticleInstance>& particles, uint32_t count, const std::vector<ControlPointData>&, float, float
	) {
	    float startTime = startTimeValue->getFloat ();
	    float endTime = endTimeValue->getFloat ();
	    glm::vec3 startValue = adjustInstanceColor (startValueValue->getVec3 ());
	    glm::vec3 endValue = adjustInstanceColor (endValueValue->getVec3 ());

	    for (uint32_t i = 0; i < count; i++) {
		auto& p = particles[i];
		if (!p.shouldSimulate ()) {
		    continue;
		}

		float life = p.getLifetimePos ();

		glm::vec3 color;
		color.r = WallpaperEngine::Maths::fadeValue (life, startTime, endTime, startValue.r, endValue.r);
		color.g = WallpaperEngine::Maths::fadeValue (life, startTime, endTime, startValue.g, endValue.g);
		color.b = WallpaperEngine::Maths::fadeValue (life, startTime, endTime, startValue.b, endValue.b);

		p.color *= color;
	    }
	};
}

OperatorFunc CParticle::createTurbulenceOperator (const TurbulenceOperator& op) {
    const bool perspective = getScene ().getScene ().camera.projection.isPerspective;
    DynamicValue* scaleValue = op.scale ? op.scale->value.get () : nullptr;
    DynamicValue* speedMinValue = op.speedMin ? op.speedMin->value.get () : nullptr;
    DynamicValue* speedMaxValue = op.speedMax ? op.speedMax->value.get () : nullptr;
    DynamicValue* timeScaleValue = op.timeScale ? op.timeScale->value.get () : nullptr;
    DynamicValue* maskValue = op.mask ? op.mask->value.get () : nullptr;
    DynamicValue* phaseMinValue = op.phaseMin->value.get ();
    DynamicValue* phaseMaxValue = op.phaseMax->value.get ();
    DynamicValue* speedOverride = getInstanceOverride ().speed->value.get ();

    return [this, &op, perspective, scaleValue, timeScaleValue, maskValue, speedOverride, speedMinValue, speedMaxValue,
            phaseMinValue, phaseMaxValue, blendTimes = op.blendTimes] (
	       std::vector<ParticleInstance>& particles, uint32_t count, const std::vector<ControlPointData>&,
	       float, float dt
	   ) {
	const float noiseScale = scaleValue ? scaleValue->getFloat () : (perspective ? 0.5f : 0.01f);
	const float timeScale = timeScaleValue ? timeScaleValue->getFloat () : (perspective ? 1.0f : 20.0f);
	// Native samples owner time with the cached rate, not integrated particle time.
	const float timeOffset = (timeScale * m_turbulenceRate) * getScene ().getTime ();
	const glm::vec3 mask = maskValue ? maskValue->getVec3 () : glm::vec3 (1.0f, 1.0f, perspective ? 1.0f : 0.0f);
	// Flag16 skips native's speed updater, but keeps the rate/time updater.
	// Scale endpoints before forming the span, including negative multipliers.
	const float speed = (m_particle.flags & 16) != 0 ? 1.0f : speedOverride->getFloat ();
	float speedMin = (speedMinValue ? speedMinValue->getFloat () : (perspective ? 1.0f : 500.0f)) * speed;
	float speedSpan = (speedMaxValue ? speedMaxValue->getFloat () : (perspective ? 5.0f : 1000.0f)) * speed - speedMin;
	const auto& recorder = getScene ().getAudioContext ().getRecorder ();
	auto firstBand = std::min (static_cast<uint32_t> (op.audioProcessingFrequencyStart->value->getInt ()), 15u);
	auto lastBand = std::min (static_cast<uint32_t> (op.audioProcessingFrequencyEnd->value->getInt ()), 15u);
	if (firstBand > lastBand) std::swap (firstBand, lastBand);
	const float audio = calculateParticleAudioResponse (
	    recorder.audio16Left, recorder.audio16Right, op.audioProcessingMode->value->getInt (),
	    op.audioProcessingBounds->value->getVec2 (), op.audioProcessingExponent->value->getFloat (), firstBand, lastBand
	);
	// Native scales the minimum and span before particle interpolation, without changing phase/time.
	speedMin *= audio;
	speedSpan *= audio;
	const float phaseSpan = phaseMaxValue->getFloat () - phaseMinValue->getFloat ();
	const float forceDt = dt * std::pow (std::min (0.025f / getScene ().getDeltaTime (), 1.0f), 0.7f);

	for (size_t i = 0; i < count; ++i) {
	    ParticleInstance& p = particles[i];
	    if (!p.shouldSimulate ()) {
		continue;
	    }
	    if (p.oscillationRandom < 0.0f) {
		p.oscillationRandom = WallpaperEngine::Maths::randomFloat (m_rng, 0.0f, 1.0f);
	    }
	    const float turbSpeed = speedMin + p.oscillationRandom * speedSpan;
	    // Native shares one particle fraction with the oscillators and uses
	    // only the phase span; the stored phase minimum is not added.
	    const float phase = p.oscillationRandom * phaseSpan;

	    glm::vec3 noisePos = p.position;
	    noisePos.y = -noisePos.y;
	    noisePos += glm::vec3 (phase + timeOffset);
	    noisePos *= noiseScale;

	    glm::vec3 force (
		simplexNoise3D (noisePos.x, noisePos.y, noisePos.z),
		-simplexNoise3D (noisePos.z, noisePos.x, noisePos.y),
		simplexNoise3D (noisePos.y, noisePos.z, noisePos.x)
	    );
	    // Native keeps the scalar field's magnitude and signed speed; it neither
	    // takes a curl nor normalizes the three cyclic samples.
	    force *= mask * turbSpeed;
	    p.velocity += force * forceDt * particleOperatorBlend (p.getLifetimePos (), blendTimes);
	}
    };
}

glm::vec3 WallpaperEngine::Render::Objects::calculateParticleVortexV2 (
    const glm::vec3& displacement, const glm::vec3& velocity, const glm::vec3& axis, const int flags,
    const glm::vec2 distances, const glm::vec2 speeds, const float centerForce, const glm::vec4 ring,
    const float rawDt, const float forceDt, const float lifetimeWeight
) {
    if (rawDt <= 0.0f || lifetimeWeight == 0.0f) return glm::vec3 (0.0f);
    const glm::vec3 radial = (flags & 1) != 0
        ? displacement - axis * glm::dot (displacement, axis) : displacement;
    const float distance = glm::length (radial);
    // Native rsqrt is singular at the center. Keep that undefined case finite.
    if (distance <= std::numeric_limits<float>::min ()) return glm::vec3 (0.0f);
    const glm::vec3 predicted = radial + velocity * rawDt;
    const float predictedDistance = glm::length (predicted);
    const bool ringShape = (flags & 4) != 0;
    const float span = ringShape ? ring.z : distances.y - distances.x;
    const float offset = ringShape ? std::abs (ring.x - distance) - ring.y : distance - distances.x;
    const float t = std::clamp (offset * (span == 0.0f ? 1.0f : 1.0f / span), 0.0f, 1.0f);
    float correction = (flags & 2) != 0 && predictedDistance > std::numeric_limits<float>::min ()
        ? (distance / predictedDistance - 1.0f) * centerForce / rawDt : 0.0f;
    if (ringShape) {
        const float pull = 1.0f - t;
        correction += std::copysign (pull == 1.0f ? 0.0f : pull, ring.x - distance) * rawDt * ring.w;
    }
    // Native cross(radial, axis) reverses under our Y reflection. Do not
    // normalize the tangent separately: sphere poles retain their attenuation.
    const glm::vec3 tangent = glm::cross (axis, radial / distance);
    return (tangent * (glm::mix (speeds.x, speeds.y, t) * forceDt) + predicted * correction) * lifetimeWeight;
}

OperatorFunc CParticle::createVortexV2Operator (const VortexOperator& op) {
    const bool perspective = getScene ().getScene ().camera.projection.isPerspective;
    return [this, &op, perspective] (
        std::vector<ParticleInstance>& particles, uint32_t count,
        const std::vector<ControlPointData>& controlPoints, float, float dt
    ) {
        if (op.controlPoint >= static_cast<int> (controlPoints.size ())) return;
        const auto value = [] (const UserSettingUniquePtr& setting, float fallback) {
            return setting ? setting->value->getFloat () : fallback;
        };
        const glm::vec2 distances (value (op.distanceInner, perspective ? 1.0f : 500.0f),
                                  value (op.distanceOuter, perspective ? 2.0f : 650.0f));
        const glm::vec4 ring (value (op.ringRadius, perspective ? 1.0f : 300.0f),
                             value (op.ringWidth, perspective ? .2f : 50.0f),
                             value (op.ringPullDistance, perspective ? .25f : 50.0f),
                             value (op.ringPullForce, perspective ? .05f : 10.0f));
        const float speed = (m_particle.flags & 16) != 0 ? 1.0f : getInstanceOverride ().speed->value->getFloat ();
        const auto& recorder = getScene ().getAudioContext ().getRecorder ();
        auto firstBand = std::min (static_cast<uint32_t> (op.audioProcessingFrequencyStart->value->getInt ()), 15u);
        auto lastBand = std::min (static_cast<uint32_t> (op.audioProcessingFrequencyEnd->value->getInt ()), 15u);
        if (firstBand > lastBand) std::swap (firstBand, lastBand);
        const float audio = calculateParticleAudioResponse (
            recorder.audio16Left, recorder.audio16Right, op.audioProcessingMode->value->getInt (),
            op.audioProcessingBounds->value->getVec2 (), op.audioProcessingExponent->value->getFloat (), firstBand, lastBand
        );
        const glm::vec2 speeds = glm::vec2 (value (op.speedInner, perspective ? 1.0f : 2500.0f),
                                          op.speedOuter->value->getFloat ()) * speed * audio;
        glm::vec3 axis = op.axis->value->getVec3 ();
        axis = glm::dot (axis, axis) < .001f ? glm::vec3 (0, 0, 1) : glm::normalize (axis);
        axis.y = -axis.y;
        if ((m_particle.flags & 1) != 0) axis = glm::mat3 (m_controlPointTransform) * axis;
        const glm::vec3 center = controlPoints[op.controlPoint].position;
        const float centerForce = op.centerForce->value->getFloat ();
        const float forceDt = dt * std::pow (std::min (0.025f / getScene ().getDeltaTime (), 1.0f), 0.7f);
        for (uint32_t i = 0; i < count; ++i) {
            auto& particle = particles[i];
            if (!particle.shouldSimulate ()) continue;
            particle.velocity += calculateParticleVortexV2 (
                particle.position - center, particle.velocity, axis, op.flags, distances, speeds,
                centerForce, ring, dt, forceDt, particleOperatorBlend (particle.getLifetimePos (), op.blendTimes)
            );
        }
    };
}

glm::vec3 WallpaperEngine::Render::Objects::calculateParticleLegacyVortex (
    const glm::vec3& displacement, const glm::vec3& axis, const int flags,
    const glm::vec2 distances, const glm::vec2 speeds, const float forceDt
) {
    const glm::vec3 radial = (flags & 1) != 0
        ? displacement - axis * glm::dot (displacement, axis) : displacement;
    const float distance = glm::length (radial);
    // Native rsqrt is undefined at the center; keep that singular case finite.
    if (distance <= std::numeric_limits<float>::min ()) return glm::vec3 (0.0f);
    const float span = distances.y - distances.x;
    const float fraction = std::clamp ((distance - distances.x) * (span == 0.0f ? 1.0f : 1.0f / span), 0.0f, 1.0f);
    // Opcode 15 normalizes the radial vector, not the tangent. Only flag1 is read.
    return glm::cross (axis, radial / distance) * (glm::mix (speeds.x, speeds.y, fraction) * forceDt);
}

OperatorFunc CParticle::createVortexOperator (const VortexOperator& op) {
    includeControlPoint (op.controlPoint);
    if (op.nativeV2) return createVortexV2Operator (op);
    const bool perspective = getScene ().getScene ().camera.projection.isPerspective;
    return [this, &op, perspective] (
        std::vector<ParticleInstance>& particles, uint32_t count,
        const std::vector<ControlPointData>& controlPoints, float, float dt
    ) {
        const auto& recorder = getScene ().getAudioContext ().getRecorder ();
        auto firstBand = std::min (static_cast<uint32_t> (op.audioProcessingFrequencyStart->value->getInt ()), 15u);
        auto lastBand = std::min (static_cast<uint32_t> (op.audioProcessingFrequencyEnd->value->getInt ()), 15u);
        if (firstBand > lastBand) std::swap (firstBand, lastBand);
        const float audio = calculateParticleAudioResponse (
            recorder.audio16Left, recorder.audio16Right, op.audioProcessingMode->value->getInt (),
            op.audioProcessingBounds->value->getVec2 (), op.audioProcessingExponent->value->getFloat (), firstBand, lastBand
        );
        const auto value = [] (const UserSettingUniquePtr& setting, float fallback) {
            return setting ? setting->value->getFloat () : fallback;
        };
        const float speed = (m_particle.flags & 16) != 0 ? 1.0f : getInstanceOverride ().speed->value->getFloat ();
        const glm::vec2 speeds = glm::vec2 (value (op.speedInner, perspective ? 1.0f : 2500.0f), op.speedOuter->value->getFloat ()) * speed * audio;
        const glm::vec2 distances (value (op.distanceInner, perspective ? 1.0f : 500.0f),
                                   value (op.distanceOuter, perspective ? 2.0f : 650.0f));
        glm::vec3 axis = op.axis->value->getVec3 ();
        axis = glm::dot (axis, axis) < .001f ? glm::vec3 (0, 0, 1) : glm::normalize (axis);
        axis.y = -axis.y;
        glm::vec3 center = op.offset->value->getVec3 () * glm::vec3 (1, -1, 1);
        if (op.controlPoint >= 0 && op.controlPoint < static_cast<int> (controlPoints.size ())) {
            center += controlPoints[op.controlPoint].position;
        }
        // Native opcode 15 consumes the same scene-attenuated force time as attraction.
        const float forceDt = dt * std::pow (std::min (0.025f / getScene ().getDeltaTime (), 1.0f), 0.7f);
        for (uint32_t i = 0; i < count; ++i) {
            auto& particle = particles[i];
            if (!particle.shouldSimulate ()) continue;
            particle.velocity += calculateParticleLegacyVortex (
                particle.position - center, axis, op.flags, distances, speeds, forceDt
            );
        }
    };
}

void WallpaperEngine::Render::Objects::applyParticleAttractionDeletion (
    ParticleInstance& particle, const glm::vec3& center, const float radius
) {
    const glm::vec3 segment = particle.position - particle.previousPosition;
    const float lengthSquared = glm::dot (segment, segment);
    const float fraction = lengthSquared == 0.0f ? 0.0f
        : std::clamp (glm::dot (center - particle.previousPosition, segment) / lengthSquared, 0.0f, 1.0f);
    const glm::vec3 closest = particle.previousPosition + segment * fraction - center;
    // Native 14022a150 marks age, including the radius boundary. Subsequent
    // operators still run; normal aging reclaims the slot on the next update.
    if (glm::dot (closest, closest) <= radius * radius) particle.age = particle.lifetime;
}

OperatorFunc CParticle::createControlPointAttractOperator (const ControlPointAttractOperator& op) {
    includeControlPoint (op.controlPoint);
    // The authored defaults use pixels in an orthographic scene and world units
    // in a perspective scene. The project is still being parsed when operators
    // are read, so resolve omitted settings here rather than guessing in the parser.
    const bool isPerspective = getScene ().getScene ().camera.projection.isPerspective;
    const float defaultScale = isPerspective ? 20.0f : 512.0f;
    const float defaultThreshold = isPerspective ? 5.0f : 512.0f;
    const float defaultDeleteThreshold = isPerspective ? 0.5f : 15.0f;

    return [this, &op, defaultScale, defaultThreshold, defaultDeleteThreshold] (
	       std::vector<ParticleInstance>& particles, uint32_t count,
	       const std::vector<ControlPointData>& controlPoints, float, float dt
	   ) {
	const float speed = (m_particle.flags & 16) != 0 ? 1.0f : getInstanceOverride ().speed->value->getFloat ();
	const float scale = (op.scale ? op.scale->value->getFloat () : defaultScale) * speed;
	const float threshold = op.threshold ? op.threshold->value->getFloat () : defaultThreshold;
	const float deleteThreshold = op.deleteThreshold ? op.deleteThreshold->value->getFloat () : defaultDeleteThreshold;
	// Native attraction receives force time, attenuated by the scene frame
	// interval before instance rate (140237720, opcodes 10/32).
	const float forceDt = dt * std::pow (std::min (0.025f / getScene ().getDeltaTime (), 1.0f), 0.7f);

	// Get control point position
	if (op.controlPoint < 0 || op.controlPoint >= static_cast<int> (controlPoints.size ())) {
	    return;
	}

	// Native uses only the selected control point; origin/offset do not move it.
	const glm::vec3 center = controlPoints[op.controlPoint].position;

	// Apply attraction force to all particles within threshold
	for (uint32_t i = 0; i < count; i++) {
	    auto& p = particles[i];
	    if (!p.shouldSimulate ()) {
		continue;
	    }

	    p.velocity += calculateControlPointAttraction (
		center - p.position, scale, threshold, forceDt, (op.flags & 2) != 0,
		particleOperatorBlend (p.getLifetimePos (), op.blendTimes)
	    );
	    if ((op.flags & 1) != 0) applyParticleAttractionDeletion (p, center, deleteThreshold);
	}
    };
}

float WallpaperEngine::Render::Objects::calculateParticleOscillationMultiplier (
    const ParticleInstance& particle, const glm::vec2 frequencyRange, const glm::vec2 phaseRange,
    const glm::vec2 scaleRange, const glm::vec4 blendTimes
) {
    // Native opcodes 8/30 (alpha) and 9/31 (size) use the same particle random fraction for frequency,
    // phase and amplitude. Phase offsets age before multiplication by frequency.
    const float random = particle.oscillationRandom;
    const float frequency = glm::mix (frequencyRange.x, frequencyRange.y, random);
    const float phase = glm::mix (phaseRange.x, phaseRange.y, random);
    const float minimum = scaleRange.x;
    const float amplitude = (scaleRange.y - minimum) * random;
    const float multiplier = minimum + amplitude * 0.5f * (1.0f + std::sin ((particle.age + phase) * frequency));
    const float weight = particleOperatorBlend (particle.getLifetimePos (), blendTimes);
    return 1.0f + (multiplier - 1.0f) * weight;
}

OperatorFunc CParticle::createOscillateAlphaOperator (const OscillateAlphaOperator& op) {
    m_hasAlphaOperators = true;
    return [this, &op] (
        std::vector<ParticleInstance>& particles, uint32_t count, const std::vector<ControlPointData>&, float, float
    ) {
        const glm::vec2 frequencies (op.frequencyMin->value->getFloat (), op.frequencyMax->value->getFloat ());
        const glm::vec2 phases (op.phaseMin->value->getFloat (), op.phaseMax->value->getFloat ());
        const glm::vec2 scales (op.scaleMin->value->getFloat (), op.scaleMax->value->getFloat ());
        for (uint32_t i = 0; i < count; ++i) {
            auto& particle = particles[i];
            if (!particle.shouldSimulate ()) continue;
            if (particle.oscillationRandom < 0.0f) {
                particle.oscillationRandom = WallpaperEngine::Maths::randomFloat (m_rng, 0.0f, 1.0f);
            }
            particle.alpha *= calculateParticleOscillationMultiplier (particle, frequencies, phases, scales, op.blendTimes);
        }
    };
}

OperatorFunc CParticle::createOscillateSizeOperator (const OscillateSizeOperator& op) {
    return [this, &op] (
        std::vector<ParticleInstance>& particles, uint32_t count, const std::vector<ControlPointData>&, float, float
    ) {
        const glm::vec2 frequencies (op.frequencyMin->value->getFloat (), op.frequencyMax->value->getFloat ());
        const glm::vec2 phases (op.phaseMin->value->getFloat (), op.phaseMax->value->getFloat ());
        const glm::vec2 scales (op.scaleMin->value->getFloat (), op.scaleMax->value->getFloat ());
        for (uint32_t i = 0; i < count; ++i) {
            auto& particle = particles[i];
            if (!particle.shouldSimulate ()) continue;
            if (particle.oscillationRandom < 0.0f) {
                particle.oscillationRandom = WallpaperEngine::Maths::randomFloat (m_rng, 0.0f, 1.0f);
            }
            particle.size *= calculateParticleOscillationMultiplier (particle, frequencies, phases, scales, op.blendTimes);
        }
    };
}

glm::vec3 WallpaperEngine::Render::Objects::calculateParticlePositionOscillation (
    const ParticleInstance& particle, const glm::vec2 frequencyRange, const glm::vec2 phaseRange,
    const glm::vec2 scaleRange, const glm::vec3 mask, const glm::vec4 blendTimes, const float deltaTime
) {
    // Native opcodes 7/29 integrate the finite sine difference, including a
    // negative previous age at birth. X/Z share a wave; Y offsets time by r*2pi.
    const float random = particle.oscillationRandom;
    const float frequency = glm::mix (frequencyRange.x, frequencyRange.y, random);
    const float time = particle.age + glm::mix (phaseRange.x, phaseRange.y, random);
    const float yTime = time + random * glm::two_pi<float> ();
    const float amplitude = glm::mix (scaleRange.x, scaleRange.y, random)
        * particleOperatorBlend (particle.getLifetimePos (), blendTimes);
    const float xz = std::sin (time * frequency) - std::sin ((time - deltaTime) * frequency);
    const float y = std::sin (yTime * frequency) - std::sin ((yTime - deltaTime) * frequency);
    return glm::vec3 (xz, y, xz) * mask * amplitude;
}

OperatorFunc CParticle::createOscillatePositionOperator (const OscillatePositionOperator& op) {
    const float defaultScaleMax = getScene ().getScene ().camera.projection.isPerspective ? 0.5f : 10.0f;
    return [this, &op, defaultScaleMax] (
        std::vector<ParticleInstance>& particles, uint32_t count, const std::vector<ControlPointData>&, float,
        float dt
    ) {
        // Native updater 1401d1908 scales frequency endpoints by instance speed;
        // particle flag 0x10 suppresses that updater, not the oscillation itself.
        const float speed = (m_particle.flags & 16) != 0 ? 1.0f : getInstanceOverride ().speed->value->getFloat ();
        const glm::vec2 frequencies = glm::vec2 (op.frequencyMin->value->getFloat (), op.frequencyMax->value->getFloat ()) * speed;
        const glm::vec2 phases (op.phaseMin->value->getFloat (), op.phaseMax->value->getFloat ());
        const glm::vec2 scales (op.scaleMin->value->getFloat (), op.scaleMax ? op.scaleMax->value->getFloat () : defaultScaleMax);
        // Authored displacement is Y-up; particle simulation coordinates are Y-down.
        const glm::vec3 mask = op.mask->value->getVec3 () * glm::vec3 (1.0f, -1.0f, 1.0f);
        for (uint32_t i = 0; i < count; ++i) {
            auto& particle = particles[i];
            if (!particle.shouldSimulate ()) continue;
            if (particle.oscillationRandom < 0.0f) {
                particle.oscillationRandom = WallpaperEngine::Maths::randomFloat (m_rng, 0.0f, 1.0f);
            }
            particle.position += calculateParticlePositionOscillation (particle, frequencies, phases, scales, mask, op.blendTimes, dt);
        }
    };
}

// ========== RENDERING ==========

static bool ropeFacesEye (const std::string& orientation) {
    return orientation != "fixed" && orientation != "upright";
}

static std::array<uint32_t, 6> ropeQuadIndices (const std::string& orientation, const WallpaperEngine::Render::Camera& camera) {
    // Screen ropes use midpoint-minus-eye facing. The 3D particle basis alone
    // reflects Y when the perspective projection does not flip it back.
    if (ropeFacesEye (orientation) && (camera.isOrthogonal () || camera.isYFlipped ())) {
	return { 0, 1, 2, 2, 3, 0 };
    }
    return { 0, 2, 1, 2, 0, 3 };
}

void CParticle::setupPass () {
    if (!m_particle.material || !m_particle.material->material || m_particle.material->material->passes.empty ()) {
	sLog.error ("No valid material for particle ", m_particle.name);
	return;
    }

    const auto& firstPass = **m_particle.material->material->passes.begin ();

    // Build override with particle-specific combos
    m_passOverride = std::make_unique<ImageEffectPassOverride> ();
    m_passOverride->combos["THICKFORMAT"] = 1;
    if (m_useRopeRenderer) {
	m_passOverride->shaderOverride = "genericropeparticle";
	const auto& orientation = m_particle.renderers[0].orientation;
	m_passOverride->combos["LWE_ROPE_SCREEN_FACING"] = ropeFacesEye (orientation);
    }
    if (const auto texture = getTexture (); texture && texture->isAnimated ()) {
	m_passOverride->combos["SPRITESHEET"] = 1;
	// Native particle overrides win over authored material combinations.
	m_passOverride->combos["SPRITESHEETBLEND"]
	    = (m_particle.flags & 2) == 0 && m_particle.animationMode != "randomframe";
	const glm::vec4& resolution = *texture->getResolution ();
	m_passOverride->combos["SPRITESHEETBLENDNPOT"] = resolution.z < resolution.x;
    }
    if (m_useTrailRenderer) {
	m_passOverride->combos["TRAILRENDERER"] = 1;
    }

    // Force texture 0 to use the input (particle texture) rather than the shader's
    // default "util/white" annotation, which would override it in setupRenderTexture()
    m_passBinds = { { 0, "previous" } };

    // Check if material uses REFRACT combo
    auto refractIt = firstPass.combos.find ("REFRACT");
    m_hasRefract = refractIt != firstPass.combos.end () && refractIt->second != 0;

    // Create the FBO provider for CPass
    m_passFBOProvider = std::make_shared<FBOProvider> (this);

    // For REFRACT: create a copy FBO that shadows _rt_FullFrameBuffer.
    // The REFRACT shader reads g_Texture3 (= _rt_FullFrameBuffer) while we render TO the scene FBO.
    // Reading from the same FBO being rendered to is undefined behavior in OpenGL, causing
    // black reads on NVIDIA. By placing a copy FBO with the same name in our FBOProvider,
    // CPass resolves g_Texture3 to the copy instead. We blit the scene content before each render.
    if (m_hasRefract) {
	const glm::vec2 size = getScene ().getOutputSize ();
	m_refractFBO = m_passFBOProvider->create (
	    "_rt_FullFrameBuffer", this->getScene ().getColorFormat (), TextureFlags_ClampUVs, 1.0f, size, size
	);
    }

    // Create CPass with the WP particle shader
    m_pass = new Effects::CPass (*this, m_passFBOProvider, firstPass, *m_passOverride, m_passBinds, std::nullopt);

    // Set destination to scene FBO and input to particle texture
    m_pass->setDestination (getScene ().getFBO ());
    m_pass->setInput (getTexture ());

    // Set matrix pointers - CPass will dereference these each frame
    m_pass->setModelViewProjectionMatrix (&m_mvpMatrix, getScene ().getCamera ().usesReverseDepth ());
    m_pass->setModelViewProjectionMatrixInverse (&m_mvpMatrixInverse);
    m_pass->setModelMatrix (&m_modelMatrix);
    m_pass->setViewProjectionMatrix (&m_viewProjectionMatrix);

    // NOTE: the VAO/VBO/EBO are created lazily by uploadGeometryBuffers() on the first
    // render: VAOs are not shared between GL contexts, so they cannot be created here
    // when this constructor runs on the async switch worker's build context

    setupGeometryCallbacks ();
    setupParticleUniforms ();
}

void CParticle::setupVao () {
    GLint prevVAO = 0;
    GLint prevArrayBuffer = 0;
    glGetIntegerv (GL_VERTEX_ARRAY_BINDING, &prevVAO);
    glGetIntegerv (GL_ARRAY_BUFFER_BINDING, &prevArrayBuffer);

    // A scene fog toggle can change the linked attribute locations. Rebuild only
    // the VAO, retaining the particle geometry already uploaded to these buffers.
    if (m_vao != 0) glDeleteVertexArrays (1, &m_vao);
    glGenVertexArrays (1, &m_vao);
    if (m_vbo == 0) glGenBuffers (1, &m_vbo);
    if (m_ebo == 0) glGenBuffers (1, &m_ebo);

    glBindVertexArray (m_vao);
    glBindBuffer (GL_ARRAY_BUFFER, m_vbo);
    glBindBuffer (GL_ELEMENT_ARRAY_BUFFER, m_ebo);

    const GLuint program = m_pass->getProgramID ();
    m_vaoShaderRevision = m_pass->getShaderRevision ();

    if (m_useRopeRenderer) {
	// Rope vertex layout: 7 attributes, 26 floats/vertex, stride=104 bytes
	// a_PositionVec4(4) + a_TexCoordVec4(4) + a_TexCoordVec4C1(4) + a_TexCoordVec4C2(4)
	// + a_TexCoordVec4C3(4) + a_TexCoordC4(2) + a_Color(4) = 26
	const GLsizei stride = sizeof (float) * ROPE_FLOATS_PER_VERTEX;

	const GLint loc0 = glGetAttribLocation (program, "a_PositionVec4");
	const GLint loc1 = glGetAttribLocation (program, "a_TexCoordVec4");
	const GLint loc2 = glGetAttribLocation (program, "a_TexCoordVec4C1");
	const GLint loc3 = glGetAttribLocation (program, "a_TexCoordVec4C2");
	const GLint loc4 = glGetAttribLocation (program, "a_TexCoordVec4C3");
	const GLint loc5 = glGetAttribLocation (program, "a_TexCoordC4");
	const GLint loc6 = glGetAttribLocation (program, "a_Color");

	if (loc0 >= 0) {
	    glEnableVertexAttribArray (loc0);
	    glVertexAttribPointer (loc0, 4, GL_FLOAT, GL_FALSE, stride, (void*)(sizeof (float) * 0));
	}
	if (loc1 >= 0) {
	    glEnableVertexAttribArray (loc1);
	    glVertexAttribPointer (loc1, 4, GL_FLOAT, GL_FALSE, stride, (void*)(sizeof (float) * 4));
	}
	if (loc2 >= 0) {
	    glEnableVertexAttribArray (loc2);
	    glVertexAttribPointer (loc2, 4, GL_FLOAT, GL_FALSE, stride, (void*)(sizeof (float) * 8));
	}
	if (loc3 >= 0) {
	    glEnableVertexAttribArray (loc3);
	    glVertexAttribPointer (loc3, 4, GL_FLOAT, GL_FALSE, stride, (void*)(sizeof (float) * 12));
	}
	if (loc4 >= 0) {
	    glEnableVertexAttribArray (loc4);
	    glVertexAttribPointer (loc4, 4, GL_FLOAT, GL_FALSE, stride, (void*)(sizeof (float) * 16));
	}
	if (loc5 >= 0) {
	    glEnableVertexAttribArray (loc5);
	    glVertexAttribPointer (loc5, 2, GL_FLOAT, GL_FALSE, stride, (void*)(sizeof (float) * 20));
	}
	if (loc6 >= 0) {
	    glEnableVertexAttribArray (loc6);
	    glVertexAttribPointer (loc6, 4, GL_FLOAT, GL_FALSE, stride, (void*)(sizeof (float) * 22));
	}
    } else {
	// Sprite vertex layout: 5 attributes, 17 floats/vertex, stride=68 bytes
	// a_Position(3) + a_TexCoordVec4(4) + a_Color(4) + a_TexCoordVec4C1(4) + a_TexCoordC2(2) = 17
	const GLsizei stride = sizeof (float) * SPRITE_FLOATS_PER_VERTEX;

	const GLint loc0 = glGetAttribLocation (program, "a_Position");
	const GLint loc1 = glGetAttribLocation (program, "a_TexCoordVec4");
	const GLint loc2 = glGetAttribLocation (program, "a_Color");
	const GLint loc3 = glGetAttribLocation (program, "a_TexCoordVec4C1");
	const GLint loc4 = glGetAttribLocation (program, "a_TexCoordC2");

	if (loc0 >= 0) {
	    glEnableVertexAttribArray (loc0);
	    glVertexAttribPointer (loc0, 3, GL_FLOAT, GL_FALSE, stride, (void*)(sizeof (float) * 0));
	}
	if (loc1 >= 0) {
	    glEnableVertexAttribArray (loc1);
	    glVertexAttribPointer (loc1, 4, GL_FLOAT, GL_FALSE, stride, (void*)(sizeof (float) * 3));
	}
	if (loc2 >= 0) {
	    glEnableVertexAttribArray (loc2);
	    glVertexAttribPointer (loc2, 4, GL_FLOAT, GL_FALSE, stride, (void*)(sizeof (float) * 7));
	}
	if (loc3 >= 0) {
	    glEnableVertexAttribArray (loc3);
	    glVertexAttribPointer (loc3, 4, GL_FLOAT, GL_FALSE, stride, (void*)(sizeof (float) * 11));
	}
	if (loc4 >= 0) {
	    glEnableVertexAttribArray (loc4);
	    glVertexAttribPointer (loc4, 2, GL_FLOAT, GL_FALSE, stride, (void*)(sizeof (float) * 15));
	}
    }

    glBindVertexArray (prevVAO);
    glBindBuffer (GL_ARRAY_BUFFER, prevArrayBuffer);
}

void CParticle::uploadGeometryBuffers (GLsizeiptr vertexBytes, GLsizeiptr indexBytes) {
    // The first particle may be emitted several frames after setup. Create the GL
    // objects before uploading; calling glBufferData while m_vbo/m_ebo are still 0
    // is GL_INVALID_OPERATION in a core profile.
    if (m_vao == 0) {
	setupVao ();
    }

    GLint prevVAO = 0;
    GLint prevArrayBuffer = 0;
    glGetIntegerv (GL_VERTEX_ARRAY_BINDING, &prevVAO);
    glGetIntegerv (GL_ARRAY_BUFFER_BINDING, &prevArrayBuffer);

    // GL_ELEMENT_ARRAY_BUFFER is VAO state. Bind the particle VAO while updating
    // both buffers so the upload does not detach or replace another pass's EBO.
    glBindVertexArray (m_vao);
    glBindBuffer (GL_ARRAY_BUFFER, m_vbo);
    glBufferData (GL_ARRAY_BUFFER, vertexBytes, m_vertices.data (), GL_DYNAMIC_DRAW);
    glBindBuffer (GL_ELEMENT_ARRAY_BUFFER, m_ebo);
    glBufferData (GL_ELEMENT_ARRAY_BUFFER, indexBytes, m_indices.data (), GL_DYNAMIC_DRAW);

    glBindVertexArray (prevVAO);
    glBindBuffer (GL_ARRAY_BUFFER, prevArrayBuffer);
}

void CParticle::setupGeometryCallbacks () {
    m_pass->setGeometryCallback (
	// Setup attribs: save current VAO, bind particle VAO
	[this] () {
	    // created lazily: VAOs are not shared between GL contexts (async builds)
	    if (m_vao == 0 || m_vaoShaderRevision != m_pass->getShaderRevision ()) {
		setupVao ();
	    }
	    glGetIntegerv (GL_VERTEX_ARRAY_BINDING, &m_prevVAO);
	    glBindVertexArray (m_vao);
	},
	// Draw geometry: indexed rendering
	[this] () { glDrawElements (GL_TRIANGLES, m_activeIndexCount, GL_UNSIGNED_INT, nullptr); },
	// Cleanup: restore previous VAO
	[this] () { glBindVertexArray (m_prevVAO); }
    );
}

void CParticle::setupParticleUniforms () {
    // Add particle-specific uniforms from common_particles.h that CPass doesn't provide
    // These are pointer-based: CPass reads the current value each frame
    m_pass->addUniform ("g_ModelMatrixInverse", &m_modelMatrixInverse);
    m_pass->addUniform ("g_OrientationUp", &m_orientationUp);
    m_pass->addUniform ("g_OrientationRight", &m_orientationRight);
    m_pass->addUniform ("g_OrientationForward", &m_orientationForward);
    m_pass->addUniform ("g_ViewUp", &m_viewUp);
    m_pass->addUniform ("g_ViewRight", &m_viewRight);
    m_pass->addUniform ("g_EyePosition", &m_eyePosition);
    m_pass->addUniform ("g_RenderVar0", &m_renderVar0);
    m_pass->addUniform ("g_RenderVar1", &m_renderVar1);

    // REFRACT: set g_RefractAmount (shader default 0.05, may not be applied by CPass's parameter system)
    if (m_hasRefract) {
	m_pass->addUniform ("g_RefractAmount", &m_refractAmount);
    }
}

void CParticle::updateMatrices (const ReflectionView* reflection) {
    const auto& camera = getScene ().getCamera ();
    const float width = camera.getWidth ();
    const float height = camera.getHeight ();
    const glm::mat4 flipY = glm::scale (glm::mat4 (1.0f), glm::vec3 (1.0f, -1.0f, 1.0f));

    // Scene layer transforms use a top-left origin and Y-down coordinates, while
    // particles are simulated around a centered, Y-up origin. Convert the complete
    // parent chain between those spaces; using only this layer's origin left child
    // particles near (0, 0) instead of around their parent emitter.
    const glm::mat4 sceneToParticle
	= glm::translate (glm::mat4 (1.0f), glm::vec3 (-width * 0.5f, height * 0.5f, 0.0f)) * flipY;
    const bool is3D = getScene ().getScene ().camera.projection.isPerspective;
    // Keep the world-to-render transform separate from the emitter transform.
    // World-space births already include the emitter/attachment transform and
    // must not follow later layer movement or inherit its size a second time.
    if (m_particleParent) {
	m_particleParent->updateMatrices ();
	m_worldModelMatrix = m_particleParent->m_worldModelMatrix;
    } else {
	m_modelMatrix = (is3D ? glm::mat4 (1.0f) : sceneToParticle) * flipY;
	if (!is3D) this->applyParallaxToModelMatrix ();
	m_worldModelMatrix = m_modelMatrix;
    }
    m_modelMatrix = (m_particle.flags & 1) != 0 ? m_worldModelMatrix
	: m_worldModelMatrix * flipY * particleWorldMatrix () * flipY;
    m_modelMatrixInverse = glm::inverse (m_modelMatrix);

    const glm::mat4& view = reflection ? reflection->view : camera.getLookAt ();
    if (reflection) {
	glm::mat4 projection = camera.getRasterProjection ();
	if (camera.isYFlipped ()) projection[1][1] *= -1.0f;
	m_viewProjectionMatrix = projection * view;
	m_eyePosition = reflection->eye;
    } else {
	this->updateParticleViewProjection ();
    }
    m_mvpMatrix = m_viewProjectionMatrix * m_modelMatrix;
    m_mvpMatrixInverse = glm::inverse (m_mvpMatrix);

    // Image quads map texture top toward -Y in the reflected 2D scene FBO.
    // Sprite tangents must use the same basis for asymmetric textures.
    m_orientationUp = glm::vec3 (0.0f, is3D ? 1.0f : -1.0f, 0.0f);
    m_orientationRight = glm::vec3 (1.0f, 0.0f, 0.0f);
    m_orientationForward = glm::vec3 (0.0f, 0.0f, 1.0f);
    m_viewUp = glm::vec3 (0.0f, 1.0f, 0.0f);
    m_viewRight = glm::vec3 (1.0f, 0.0f, 0.0f);

    if (is3D) {
	// Sprite corners are expanded in particle-local space. Express the camera's
	// basis there while preserving the layer's in-plane roll and authored size.
	const glm::mat4 cameraWorld = glm::inverse (view);
	const glm::mat3 viewToLocal = calculateBillboardParticleOrientation (m_modelMatrixInverse, cameraWorld, 0.0f);
	const float roll = (m_particle.flags & 1) != 0 ? 0.0f
	    : m_particle.angles->evaluateVec3 (getScene ().getElapsedTime ()).z;
	const glm::mat3 cameraToLocal = calculateBillboardParticleOrientation (
	    m_modelMatrixInverse, cameraWorld, roll
	);
	m_orientationRight = cameraToLocal[0];
	m_orientationUp = cameraToLocal[1];
	m_orientationForward = cameraToLocal[2];
	m_viewRight = viewToLocal[0];
	m_viewUp = viewToLocal[1];
    }

    if (!m_particle.renderers.empty () && m_particle.renderers[0].orientation == "fixed") {
	const auto& renderer = m_particle.renderers[0];
	// Recover the authored layer transform before crossing the simulation's Y
	// reflection. View tangents remain camera-facing for refraction and lighting.
	glm::mat3 authoredModel = glm::mat3 (m_modelMatrix * flipY);
	if (!is3D) {
	    authoredModel = glm::mat3 (flipY) * authoredModel;
	}
	const glm::mat3 orientation = glm::mat3 (flipY)
	    * calculateFixedParticleOrientation (renderer.axis, authoredModel, (renderer.flags & 1) != 0);
	m_orientationRight = orientation[0];
	m_orientationUp = orientation[1];
	m_orientationForward = orientation[2];
    }
    if (!m_useRopeRenderer && !m_useTrailRenderer && !m_particle.renderers.empty ()
        && m_particle.renderers[0].orientation == "upright") {
        const auto& renderer = m_particle.renderers[0];
        glm::mat3 authoredModel = glm::mat3 (m_modelMatrix * flipY);
        if (!is3D) authoredModel = glm::mat3 (flipY) * authoredModel;
        const glm::vec3 cameraForward (glm::inverse (getScene ().getCamera ().getLookAt ())[2]);
        const glm::mat3 orientation = glm::mat3 (flipY) * calculateUprightParticleOrientation (
            renderer.axis, authoredModel, cameraForward, (renderer.flags & 1) != 0
        );
        m_orientationRight = orientation[0];
        m_orientationUp = orientation[1];
        m_orientationForward = orientation[2];
    }
    if (m_useRopeRenderer && !m_particle.renderers.empty ()) {
	const auto& renderer = m_particle.renderers[0];
	if (renderer.orientation == "upright") {
	    glm::mat3 authoredModel = glm::mat3 (m_modelMatrix * flipY);
	    if (!is3D) authoredModel = glm::mat3 (flipY) * authoredModel;
	    const glm::vec3 cameraForward (glm::inverse (getScene ().getCamera ().getLookAt ())[2]);
	    const glm::vec3 forward = calculateUprightRopeForward (
		renderer.axis, authoredModel, cameraForward, (renderer.flags & 1) != 0
	    );
	    m_orientationForward = is3D ? forward : glm::mat3 (flipY) * forward;
	} else if (renderer.orientation == "fixed" && is3D) {
	    // Rope shaders transform the authored forward through the inverse model;
	    // the sprite-local Y reflection would otherwise be applied a second time.
	    m_orientationForward = glm::mat3 (flipY) * m_orientationForward;
	} else if (renderer.orientation != "fixed") {
	    // Sprite tangents are local already, but the stock rope vertex shader
	    // applies g_ModelMatrixInverse itself. Supply the world-facing vector.
	    m_orientationForward = glm::mat3 (m_modelMatrix) * m_orientationForward;
	}
    }

    this->updateParticleRenderVars ();
}

void CParticle::applyParallaxToModelMatrix () {
    if (!getScene ().getScene ().camera.parallax.enabled->value->getBool ()
	|| getScene ().getContext ().getApp ().getContext ().settings.mouse.disableparallax) {
	return;
    }

    const glm::vec3 parallaxOffset (this->resolveParallaxOffset (), 0.0f);
    // Parallax translates the complete layer subtree in world space. Pre-multiplying
    // keeps the camera offset from being scaled or rotated by a particle parent.
    m_modelMatrix = glm::translate (glm::mat4 (1.0f), parallaxOffset) * m_modelMatrix;
}

void CParticle::updateParticleViewProjection () {
    const auto& camera = getScene ().getCamera ();
    const bool perspective = getScene ().getScene ().camera.projection.isPerspective;
    const bool facesEye = m_useRopeRenderer ? ropeFacesEye (m_particle.renderers[0].orientation) : m_useTrailRenderer;
    if ((m_particle.flags & 4) != 0) {
	// Native flag-4 draws replace the projection in both scene modes. The
	// perspective producer derives its virtual FOV from the ordinary Y scale.
	const float projectionY = perspective ? glm::abs (camera.getProjection ()[1][1])
	    : camera.getProjection ()[1][1];
	const float fov = perspective ? 2.0f * std::atan (1.0f / (projectionY * 2000.0f))
	    : glm::radians (glm::clamp (camera.getFov (), 0.1f, 179.9f));
	const float distance = 1.0f / (std::tan (fov * 0.5f) * projectionY);
	glm::mat4 view = camera.getLookAt ();
	view[3][2] = -distance;
	const auto& output = getScene ().getOutputSize ();
	const float farZ = glm::max (15000.0f, distance + 1000.0f);
	glm::mat4 projection = camera.usesReverseDepth ()
	    ? reversePerspective (fov, output.x / output.y, 5.0f, farZ)
	    : glm::perspective (fov, output.x / output.y, 5.0f, farZ);
	if (perspective && camera.isYFlipped ()) projection[1][1] *= -1.0f;
	m_viewProjectionMatrix = projection * view;
	// Native virtual projection preserves the scene eye used by ribbons.
	m_eyePosition = perspective ? camera.getEye ()
	    : facesEye ? glm::vec3 (camera.getEye ().x, -camera.getEye ().y, 2000.0f)
	    : glm::vec3 (glm::inverse (view)[3]);
    } else if (perspective) {
	m_viewProjectionMatrix = camera.getRasterProjection () * camera.getLookAt ();
	m_eyePosition = camera.getEye ();
    } else {
	// Orthographic projection from scene camera
	m_viewProjectionMatrix = getScene ().getCamera ().getProjection () * getScene ().getCamera ().getLookAt ();
	// Native ordinary 2D ribbons use the scene eye at Z2000. Reflect its
	// authored Y into the centered particle render basis. Preserve other eyes.
	m_eyePosition = facesEye ? glm::vec3 (camera.getEye ().x, -camera.getEye ().y, 2000.0f)
	    : glm::vec3 (0.0f, 0.0f, 1000.0f);
    }
}

void CParticle::updateParticleRenderVars () {
    // Native 1402366f0 supplies the elapsed fraction of the history interval.
    // The live head supplies geometry; this phase advances its texture between samples.
    // Sprite trails use the authored length limits directly in genericparticle.
    const float trailPhase = m_ropeHistoryTimer / (std::max (m_trailLength, 0.001f) / std::max (2, m_ropeSegments));
    m_renderVar0 = m_useRopeTrailRenderer ? glm::vec4 (0.0f, 0.0f, trailPhase, 0.0f)
					  : glm::vec4 (m_trailLength, m_trailMaxLength, m_trailMinLength, 0.0f);

    if (const auto texture = getTexture (); texture && texture->isAnimated () && !texture->getFrames ().empty ()) {
	const auto& frame = *texture->getFrames ().front ();
	const glm::vec4& resolution = *texture->getResolution ();
	if (frame.width1 > 0.0f && frame.height1 > 0.0f && resolution.x > 0.0f && resolution.y > 0.0f) {
	    // TEXS axes are pixels in the stored atlas, independent of the GIF canvas
	    // and the logical atlas padding used by ComputeSpriteFrame's row wrap.
	    m_renderVar1 = glm::vec4 (
		frame.width1 / resolution.x, frame.height1 / resolution.y,
		static_cast<float> (m_spritesheetFrames), frame.height1 / frame.width1
	    );
	    return;
	}
    }

    if (m_spritesheetFrames > 0 && m_spritesheetCols > 0 && m_spritesheetRows > 0) {
	float frameWidth = 1.0f / static_cast<float> (m_spritesheetCols);
	float frameHeight = 1.0f / static_cast<float> (m_spritesheetRows);
	float textureRatio = 1.0f;
	if (const auto texture = getTexture ()) {
	    // Use atlas dimensions (from resolution vec4) for textureRatio, NOT getRealWidth/Height
	    // which returns per-frame dimensions for animated textures. The shader needs the
	    // per-frame pixel aspect ratio: (atlasH * frameHeight) / (atlasW * frameWidth).
	    const glm::vec4* res = texture->getResolution ();
	    float w = res->x; // atlas/GL texture width
	    float h = res->y; // atlas/GL texture height
	    if (w > 0.0f) {
		textureRatio = (h * frameHeight) / (w * frameWidth);
	    }
	}
	m_renderVar1 = glm::vec4 (frameWidth, frameHeight, static_cast<float> (m_spritesheetFrames), textureRatio);
    } else {
	// No spritesheet - texture ratio is height/width
	float textureRatio = 1.0f;
	if (const auto texture = getTexture ()) {
	    float w = static_cast<float> (texture->getRealWidth ());
	    float h = static_cast<float> (texture->getRealHeight ());
	    if (w > 0.0f) {
		textureRatio = h / w;
	    }
	}
	m_renderVar1 = glm::vec4 (0.0f, 0.0f, 0.0f, textureRatio);
    }
}

void CParticle::renderSprites (const ReflectionView* reflection) {
    if (m_particleCount == 0 || m_pass == nullptr) {
	return;
    }

    // Count alive particles
    uint32_t aliveCount = 0;
    for (uint32_t i = 0; i < m_particleCount; i++) {
	if (m_particles[i].alive) {
	    aliveCount++;
	}
    }

    if (aliveCount == 0) {
	return;
    }

    // Build vertex data in WP shader layout:
    // a_Position(3) + a_TexCoordVec4(uv.x, uv.y, rotZ, size)(4) + a_Color(4)
    //   + a_TexCoordVec4C1(vel.x, vel.y, vel.z, lifetime)(4) + a_TexCoordC2(rotX, rotY)(2) = 17 floats
    uint32_t vertexIndex = 0;
    uint32_t indexOffset = 0;
    const bool is3D = getScene ().getScene ().camera.projection.isPerspective;
    const bool perspectiveBillboard = is3D
	&& (m_particle.renderers.empty () || m_particle.renderers[0].orientation != "fixed");
    const auto texture = getTexture ();
    const bool animated = texture && texture->isAnimated ();

    for (uint32_t i = 0; i < m_particleCount; i++) {
	const auto& p = m_particles[i];
	if (!p.alive) {
	    continue;
	}

	const glm::vec3 renderRotation = convertParticleRotationForRender (p.rotation, perspectiveBillboard || !is3D);

	// Skip particles with invalid values
	if (!std::isfinite (p.position.x) || !std::isfinite (p.position.y) || !std::isfinite (p.position.z)
	    || !std::isfinite (p.size) || p.size <= 0.0f || p.size > 10000.0f) {
	    continue;
	}

	// Native uploads an unwrapped sequence phase or the retained birth random.
	// Stock shaders apply frac when selecting the frame; static sequences stay zero.
	const float animationPhase = m_particle.animationMode == "randomframe" ? p.oscillationRandom
	    : animated ? p.getLifetimePos () * m_particle.sequenceMultiplier : 0.0f;

	// All four corners share the same particle attributes. Pack them once,
	// then change only the UV coordinates in each copy.
	const float vertex[SPRITE_FLOATS_PER_VERTEX] = {
	    p.position.x, p.position.y, p.position.z,
	    0.0f, 0.0f, renderRotation.z, p.size,
	    p.color.r, p.color.g, p.color.b, p.alpha,
	    p.velocity.x, p.velocity.y, p.velocity.z, animationPhase,
	    renderRotation.x, renderRotation.y,
	};
	constexpr float corners[4][2] = { { 0.0f, 1.0f }, { 1.0f, 1.0f }, { 1.0f, 0.0f }, { 0.0f, 0.0f } };
	const uint32_t baseVertex = vertexIndex;
	for (const auto& uv : corners) {
	    float* destination = m_vertices.data () + vertexIndex * SPRITE_FLOATS_PER_VERTEX;
	    std::copy_n (vertex, SPRITE_FLOATS_PER_VERTEX, destination);
	    destination[3] = uv[0];
	    destination[4] = uv[1];
	    vertexIndex++;
	}

	// 6 indices forming 2 triangles
	m_indices[indexOffset++] = baseVertex + 0;
	m_indices[indexOffset++] = baseVertex + 1;
	m_indices[indexOffset++] = baseVertex + 2;
	m_indices[indexOffset++] = baseVertex + 2;
	m_indices[indexOffset++] = baseVertex + 3;
	m_indices[indexOffset++] = baseVertex + 0;
    }

    m_activeIndexCount = static_cast<GLsizei> (indexOffset);
    if (m_activeIndexCount == 0) {
	return;
    }

#if !NDEBUG
    std::string str = "Particles ";
    str += this->getParticle ().name + " (" + std::to_string (this->getId ()) + ", " + this->getParticle ().particleFile
	+ ")";
    glPushDebugGroup (GL_DEBUG_SOURCE_APPLICATION, 0, -1, str.c_str ());
#endif

    uploadGeometryBuffers (
	static_cast<GLsizeiptr> (vertexIndex * SPRITE_FLOATS_PER_VERTEX * sizeof (float)),
	static_cast<GLsizeiptr> (indexOffset * sizeof (uint32_t))
    );

    // Update matrices and uniform data
    updateMatrices (reflection);

    // For REFRACT: blit current scene content into the copy FBO before rendering.
    // This gives the shader a snapshot of what's behind the particles for refraction,
    // without a feedback loop (rendering to scene FBO while reading from copy FBO).
    if (m_hasRefract && m_refractFBO) {
	auto sceneFBO = getScene ().getActiveRenderTarget ();
	GLint w = static_cast<GLint> (sceneFBO->getRealWidth ());
	GLint h = static_cast<GLint> (sceneFBO->getRealHeight ());
	m_refractFBO->resize (w, h);
	glBindFramebuffer (GL_READ_FRAMEBUFFER, sceneFBO->getFramebuffer ());
	glBindFramebuffer (GL_DRAW_FRAMEBUFFER, m_refractFBO->getFramebuffer ());
	glBlitFramebuffer (0, 0, w, h, 0, 0, w, h, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    }

    // The shader's ComputeParticleTrailTangents produces a right vector with a Z component
    // (from cross(eyeDirection, velocity) where eyeDirection has XY offset from model transform).
    // For 2D/ortho particles at z≈0, the ortho near plane sits at ndc.z=-1 — any Z offset from
    // the right vector pushes vertices past the near plane, causing half the quad to be clipped.
    // GL_DEPTH_CLAMP prevents near/far clipping by clamping depth instead.
    glEnable (GL_DEPTH_CLAMP);

    // The perspective projection or upright 2D sprite tangents reflect Y.
    // Trail tangents come from velocity and retain their 2D winding.
    // In the planar pass the reflected eye rebuilds the trail's facing in its
    // local basis. With the unflipped target projection the quad uses CCW.
    const bool reflectedWinding = !reflection
	&& (is3D ? getScene ().getCamera ().isYFlipped () : !m_useTrailRenderer);
    if (reflectedWinding) glFrontFace (GL_CW);

    // CPass::render() handles: FBO binding, texture setup, uniforms, blending, draw call, cleanup
    m_pass->render ();

    if (reflectedWinding) glFrontFace (GL_CCW);
    glDisable (GL_DEPTH_CLAMP);

#if !NDEBUG
    glPopDebugGroup ();
#endif
}

void CParticle::renderRopeTrail () {
    if (m_particleCount == 0 || m_pass == nullptr) {
	return;
    }

    const auto quadIndices = ropeQuadIndices (m_particle.renderers[0].orientation, getScene ().getCamera ());
    const int subdivision = std::max (1, m_ropeSubdivision);
    uint32_t vertexIndex = 0;
    uint32_t indexOffset = 0;
    bool bufferFull = false;
    std::vector<ParticleInstance::TrailPoint> points;
    std::vector<glm::vec3> splinePositions;
    std::vector<float> splineSizes;
    std::vector<glm::vec4> splineColors;
    points.reserve (static_cast<size_t> (m_ropeSegments + 1));

    const float trailPhase = m_ropeHistoryTimer / (std::max (m_trailLength, 0.001f) / std::max (2, m_ropeSegments));
    auto catmullRom = [] (const glm::vec3& p0, const glm::vec3& p1, const glm::vec3& p2, const glm::vec3& p3,
			  float t) -> glm::vec3 {
	const float t2 = t * t;
	const float t3 = t2 * t;
	return 0.5f
	    * ((2.0f * p1) + (-p0 + p2) * t + (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t2
	       + (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t3);
    };

    for (uint32_t particleIndex = 0; particleIndex < m_particleCount && !bufferFull; particleIndex++) {
	const auto& particle = m_particles[particleIndex];
	if (!particle.isAlive () || !particle.trailLastFrameValid) {
	    continue;
	}

	points.clear ();

	auto appendPoint = [&] (const ParticleInstance::TrailPoint& point) {
	if (!std::isfinite (point.position.x) || !std::isfinite (point.position.y)
	    || !std::isfinite (point.position.z)) {
		return;
	    }

	    points.push_back (point);
	};

	// The stock TRAILRENDERER shader starts UV 0 at the live head, followed
	// by its recent history. Authored shaft textures narrow toward UV 1;
	// oldest-first geometry put their wide end at the tail instead.
	appendPoint (
	    ParticleInstance::TrailPoint {
		.position = particle.position,
	    }
	);
	for (auto point = particle.trailHistory.rbegin (); point != particle.trailHistory.rend (); ++point) {
	    appendPoint (*point);
	}

	if (points.size () < 2) {
	    continue;
	}

	const uint32_t segmentCount = static_cast<uint32_t> (points.size () - 1);
	const uint32_t totalPoints = segmentCount * subdivision + 1;
	splinePositions.resize (totalPoints);
	splineSizes.resize (totalPoints);
	splineColors.resize (totalPoints);

	for (uint32_t segment = 0; segment < segmentCount; segment++) {
	    const auto& point1 = points[segment];
	    const auto& point2 = points[segment + 1];
	    const auto& point0 = (segment > 0) ? points[segment - 1] : point1;
	    const auto& point3 = (segment + 2 < points.size ()) ? points[segment + 2] : point2;
	    const glm::vec3 intervalDelta = point2.position - point1.position;
	    const bool stationaryInterval = glm::dot (intervalDelta, intervalDelta) <= 0.000001f;

	    for (int step = 0; step < subdivision; step++) {
		const float t = static_cast<float> (step) / static_cast<float> (subdivision);
		const uint32_t index = segment * subdivision + step;
		splinePositions[index] = stationaryInterval ? point1.position
		    : catmullRom (point0.position, point1.position, point2.position, point3.position, t);
		// Wallpaper Engine's rope history contains positions only. Visual state
		// remains attached to the live particle so alpha/size operators fade the
		// complete trail instead of leaving bright historical samples behind.
		splineSizes[index] = particle.size;
		splineColors[index] = glm::vec4 (particle.color, particle.alpha);
	    }
	}

	const auto& lastPoint = points.back ();
	splinePositions.back () = lastPoint.position;
	splineSizes.back () = particle.size;
	splineColors.back () = glm::vec4 (particle.color, particle.alpha);

	if (m_ropeFadeAlpha || m_ropeFadeSize) {
	    const float denominator = static_cast<float> (totalPoints - 1);
	    for (uint32_t pointIndex = 0; pointIndex < totalPoints; pointIndex++) {
		const float tailFade = 1.0f - static_cast<float> (pointIndex) / denominator;
		splineColors[pointIndex].a
		    = calculateRopeTrailVisualValue (particle.alpha, tailFade, m_ropeFadeAlpha);
		splineSizes[pointIndex]
		    = calculateRopeTrailVisualValue (particle.size, tailFade, m_ropeFadeSize);
	    }
	}

	const uint32_t subSegmentCount = totalPoints - 1;
	const float uvScale = (m_ropeUVScale > 0.0f) ? m_ropeUVScale : 1.0f;
	// The first segment contains only trailPhase of a history interval. While
	// filling, stretch to the birth point; once full, keep the texture duration
	// fixed so the oldest segment disappears beyond UV 1 before being removed.
	const float usableLength = (static_cast<float> (segmentCount - 1)
	    + (particle.trailHistory.size () < static_cast<size_t> (m_ropeSegments) ? trailPhase : 0.0f)) / uvScale;
	if (usableLength <= 0.0f) continue;

	for (uint32_t segment = 0; segment < subSegmentCount; segment++) {
	    if ((vertexIndex + 4) * ROPE_FLOATS_PER_VERTEX > m_vertices.size ()
		|| indexOffset + 6 > m_indices.size ()) {
		bufferFull = true;
		break;
	    }

	    const glm::vec3& positionStart = splinePositions[segment];
	    const glm::vec3& positionEnd = splinePositions[segment + 1];
	    const glm::vec3 delta = positionEnd - positionStart;
	    // Keep the history index even when the live head coincides with a sample.
	    if (glm::dot (delta, delta) <= 0.000001f) continue;

	    // CPU subdivisions must preserve the native shader's history-time UVs.
	    // Pack each smaller interval into its existing length/position inputs.
	    const uint32_t historySegment = segment / subdivision;
	    const float segmentFraction = static_cast<float> (segment % subdivision) / subdivision;
	    const float intervalLength = historySegment == 0 ? trailPhase : 1.0f;
	    if (intervalLength <= 0.0f) continue;
	    const float intervalStart = historySegment == 0 ? segmentFraction * trailPhase
		: static_cast<float> (historySegment - 1) + trailPhase + segmentFraction;
	    const float shaderLength = usableLength * subdivision / (segment == 0 ? 1.0f : intervalLength);
	    const float shaderPosition = segment == 0 ? 0.0f
		: intervalStart * subdivision / intervalLength + 1.0f - trailPhase;
	    const glm::vec3& positionPrevious = (segment > 0) ? splinePositions[segment - 1] : positionStart;
	    const glm::vec3& positionAfter = (segment + 2 < totalPoints) ? splinePositions[segment + 2] : positionEnd;
	    const float sizeStart = splineSizes[segment];
	    const float sizeEnd = splineSizes[segment + 1];
	    const glm::vec4& colorStart = splineColors[segment];
	    const glm::vec4& colorEnd = splineColors[segment + 1];

	    auto addVertex = [&] (float uvX, float uvY) {
		const uint32_t base = vertexIndex * ROPE_FLOATS_PER_VERTEX;
		m_vertices[base + 0] = positionStart.x;
		m_vertices[base + 1] = positionStart.y;
		m_vertices[base + 2] = positionStart.z;
		m_vertices[base + 3] = sizeStart;
		m_vertices[base + 4] = positionEnd.x;
		m_vertices[base + 5] = positionEnd.y;
		m_vertices[base + 6] = positionEnd.z;
		m_vertices[base + 7] = shaderLength + 1.0f;
		m_vertices[base + 8] = positionPrevious.x;
		m_vertices[base + 9] = positionPrevious.y;
		m_vertices[base + 10] = positionPrevious.z;
		m_vertices[base + 11] = shaderPosition;
		m_vertices[base + 12] = positionAfter.x;
		m_vertices[base + 13] = positionAfter.y;
		m_vertices[base + 14] = positionAfter.z;
		m_vertices[base + 15] = sizeEnd;
		m_vertices[base + 16] = colorEnd.r;
		m_vertices[base + 17] = colorEnd.g;
		m_vertices[base + 18] = colorEnd.b;
		m_vertices[base + 19] = colorEnd.a;
		m_vertices[base + 20] = uvX;
		m_vertices[base + 21] = uvY;
		m_vertices[base + 22] = colorStart.r;
		m_vertices[base + 23] = colorStart.g;
		m_vertices[base + 24] = colorStart.b;
		m_vertices[base + 25] = colorStart.a;
		vertexIndex++;
	    };

	    const uint32_t baseVertex = vertexIndex;
	    addVertex (0.0f, 0.0f);
	    addVertex (1.0f, 0.0f);
	    addVertex (1.0f, 1.0f);
	    addVertex (0.0f, 1.0f);

	    for (const uint32_t index : quadIndices) m_indices[indexOffset++] = baseVertex + index;
	}
    }

    m_activeIndexCount = static_cast<GLsizei> (indexOffset);
    if (m_activeIndexCount == 0) {
	return;
    }

#if !NDEBUG
    std::string debugName = "Rope trail particles ";
    debugName += this->getParticle ().name + " (" + std::to_string (this->getId ()) + ", "
	+ this->getParticle ().particleFile + ")";
    glPushDebugGroup (GL_DEBUG_SOURCE_APPLICATION, 0, -1, debugName.c_str ());
#endif

    uploadGeometryBuffers (
	static_cast<GLsizeiptr> (vertexIndex * ROPE_FLOATS_PER_VERTEX * sizeof (float)),
	static_cast<GLsizeiptr> (indexOffset * sizeof (uint32_t))
    );
    updateMatrices ();

    if (m_hasRefract && m_refractFBO) {
	auto sceneFBO = getScene ().getActiveRenderTarget ();
	const GLint width = static_cast<GLint> (sceneFBO->getRealWidth ());
	const GLint height = static_cast<GLint> (sceneFBO->getRealHeight ());
	m_refractFBO->resize (width, height);
	glBindFramebuffer (GL_READ_FRAMEBUFFER, sceneFBO->getFramebuffer ());
	glBindFramebuffer (GL_DRAW_FRAMEBUFFER, m_refractFBO->getFramebuffer ());
	glBlitFramebuffer (0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    }

    glEnable (GL_DEPTH_CLAMP);
    m_pass->render ();
    glDisable (GL_DEPTH_CLAMP);

#if !NDEBUG
    glPopDebugGroup ();
#endif
}

void CParticle::renderRope () {
    if (m_useRopeTrailRenderer) {
	renderRopeTrail ();
	return;
    }

    if (m_particleCount < 2 || m_pass == nullptr) {
	return;
    }

    // Array is already in spawn order (oldest at index 0) thanks to order-preserving
    // compaction in update(). All particles in [0, m_particleCount) are alive.
    const uint32_t aliveCount = m_particleCount;

    // Build vertex data with Catmull-Rom spline subdivision.
    // Each segment between consecutive particles is subdivided into m_ropeSubdivision
    // sub-segments for smooth curves instead of harsh corners at particle positions.
    //
    // Rope vertex layout (26 floats per vertex, THICKFORMAT):
    // [0-3]   a_PositionVec4:   startPos.xyz, sizeStart
    // [4-7]   a_TexCoordVec4:   endPos.xyz, trailLength
    // [8-11]  a_TexCoordVec4C1: CP0.xyz, trailPosition
    // [12-15] a_TexCoordVec4C2: CP1.xyz, sizeEnd
    // [16-19] a_TexCoordVec4C3: colorEnd.rgba
    // [20-21] a_TexCoordC4:     uvs.xy
    // [22-25] a_Color:          colorStart.rgba

    const uint32_t numSegments = aliveCount - 1;
    const auto quadIndices = ropeQuadIndices (m_particle.renderers[0].orientation, getScene ().getCamera ());
    const int subdivision = std::max (1, m_ropeSubdivision);

    // Catmull-Rom spline evaluation
    auto catmullRom = [] (const glm::vec3& p0, const glm::vec3& p1, const glm::vec3& p2, const glm::vec3& p3,
			  float t) -> glm::vec3 {
	float t2 = t * t, t3 = t2 * t;
	return 0.5f
	    * ((2.0f * p1) + (-p0 + p2) * t + (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t2
	       + (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t3);
    };

    // First pass: evaluate spline to get all interpolated points
    const uint32_t totalPoints = numSegments * subdivision + 1;
    // Store position, size, color (rgba) per point = 3 + 1 + 4 = 8 floats
    std::vector<glm::vec3> splinePositions (totalPoints);
    std::vector<float> splineSizes (totalPoints);
    std::vector<glm::vec4> splineColors (totalPoints); // rgba

    for (uint32_t i = 0; i < numSegments; i++) {
	const auto& p1 = m_particles[i];
	const auto& p2 = m_particles[i + 1];
	const auto& p0 = (i > 0) ? m_particles[i - 1] : p1;
	const auto& p3 = (i + 2 < aliveCount) ? m_particles[i + 2] : p2;

	for (int k = 0; k < subdivision; k++) {
	    float t = static_cast<float> (k) / static_cast<float> (subdivision);
	    uint32_t idx = i * subdivision + k;

	    splinePositions[idx] = catmullRom (p0.position, p1.position, p2.position, p3.position, t);
	    splineSizes[idx] = glm::mix (p1.size, p2.size, t);
	    splineColors[idx] = glm::mix (glm::vec4 (p1.color, p1.alpha),
					glm::vec4 (p2.color, p2.alpha), t);
	}
    }
    // Last point is the final particle
    {
	const auto& pLast = m_particles[aliveCount - 1];
	splinePositions[totalPoints - 1] = pLast.position;
	splineSizes[totalPoints - 1] = pLast.size;
	splineColors[totalPoints - 1] = glm::vec4 (pLast.color, pLast.alpha);
    }

    // Second pass: build quads from consecutive spline points.
    // The shader computes UV.v from trailPosition / (trailLength - 1), consuming
    // 1/(trailLength-1) of UV space per quad. Express trailLength and trailPosition
    // in sub-segment units so each sub-segment quad gets the correct UV slice.
    // UV scale divides the effective length, making UVs exceed [0,1] → texture repeats.
    uint32_t vertexIndex = 0;
    uint32_t indexOffset = 0;
    const uint32_t totalSubSegments = totalPoints - 1;
    const float reciprocalUVScale = m_ropeUVScale == 0.0f ? 1.0f : 1.0f / m_ropeUVScale;
    float nativeLength = static_cast<float> (aliveCount) * reciprocalUVScale;
    float scrollOffset = 0.0f;
    if (m_ropeUVScrolling || m_ropeUVSmoothing) {
	const auto& settings = getInstanceOverride ();
	const float lifetime = m_ropeMeanLifetime * settings.lifetime->value->getFloat ();
	const float rate = m_ropeEmitterRate * settings.count->value->getFloat ();
	float predictedRate = rate;
	if (static_cast<float> (aliveCount) < rate * lifetime) {
	    const auto fps = static_cast<uint32_t> (
		getScene ().getContext ().getApp ().getContext ().settings.render.maximumFPS
	    );
	    predictedRate = std::min (rate, static_cast<float> (fps));
	}
	const float predictedCount = predictedRate * lifetime;
	if (m_ropeUVScrolling) {
	    nativeLength = (predictedCount - 1.0f) * reciprocalUVScale;
	    scrollOffset = static_cast<float> (m_retiredParticleCount);
	} else if (predictedCount > 0.0f && static_cast<float> (aliveCount) >= predictedCount - 1.0f) {
	    nativeLength = (predictedCount - 1.0f) * reciprocalUVScale;
	    if (aliveCount > 2) {
		// Native smoothing follows the oldest particle's remaining interval.
		scrollOffset = glm::clamp ((lifetime - m_particles[0].age) / (1.0f / predictedRate), 0.0f, 1.0f)
		    - 1.0f;
	    }
	}
    }
    // Preserve the stock shader's W-1 denominator and phase in sub-segment units.
    const float trailLength = (nativeLength - 1.0f) * subdivision + 1.0f;
    scrollOffset *= subdivision;

    for (uint32_t s = 0; s < totalSubSegments; s++) {
	const glm::vec3& posStart = splinePositions[s];
	const glm::vec3& posEnd = splinePositions[s + 1];
	float sizeStart = splineSizes[s];
	float sizeEnd = splineSizes[s + 1];
	const glm::vec4& colorStart = splineColors[s];
	const glm::vec4& colorEnd = splineColors[s + 1];

	// Neighboring points for shader tangent computation (CP0/CP1)
	const glm::vec3& posPrev = (s > 0) ? splinePositions[s - 1] : posStart;
	const glm::vec3& posAfter = (s + 2 < totalPoints) ? splinePositions[s + 2] : posEnd;

	// Compute trailPosition for UV mapping
	const float trailPosition = static_cast<float> (s) + scrollOffset;

	auto addRopeVertex = [&] (float uvX, float uvY) {
	    const uint32_t base = vertexIndex * ROPE_FLOATS_PER_VERTEX;

	    // a_PositionVec4: startPos.xyz, sizeStart
	    m_vertices[base + 0] = posStart.x;
	    m_vertices[base + 1] = posStart.y;
	    m_vertices[base + 2] = posStart.z;
	    m_vertices[base + 3] = sizeStart;

	    // a_TexCoordVec4: endPos.xyz, trailLength
	    m_vertices[base + 4] = posEnd.x;
	    m_vertices[base + 5] = posEnd.y;
	    m_vertices[base + 6] = posEnd.z;
	    m_vertices[base + 7] = trailLength;

	    // a_TexCoordVec4C1: CP0.xyz (neighbor before start), trailPosition
	    m_vertices[base + 8] = posPrev.x;
	    m_vertices[base + 9] = posPrev.y;
	    m_vertices[base + 10] = posPrev.z;
	    m_vertices[base + 11] = trailPosition;

	    // a_TexCoordVec4C2: CP1.xyz (neighbor after end), sizeEnd
	    m_vertices[base + 12] = posAfter.x;
	    m_vertices[base + 13] = posAfter.y;
	    m_vertices[base + 14] = posAfter.z;
	    m_vertices[base + 15] = sizeEnd;

	    // a_TexCoordVec4C3: colorEnd.rgba
	    m_vertices[base + 16] = colorEnd.r;
	    m_vertices[base + 17] = colorEnd.g;
	    m_vertices[base + 18] = colorEnd.b;
	    m_vertices[base + 19] = colorEnd.a;

	    // a_TexCoordC4: uvs.xy
	    m_vertices[base + 20] = uvX;
	    m_vertices[base + 21] = uvY;

	    // a_Color: colorStart.rgba
	    m_vertices[base + 22] = colorStart.r;
	    m_vertices[base + 23] = colorStart.g;
	    m_vertices[base + 24] = colorStart.b;
	    m_vertices[base + 25] = colorStart.a;

	    vertexIndex++;
	};

	// Quad: 4 vertices (left/right at start/end of segment)
	uint32_t baseVertex = vertexIndex;
	addRopeVertex (0.0f, 0.0f); // left at start
	addRopeVertex (1.0f, 0.0f); // right at start
	addRopeVertex (1.0f, 1.0f); // right at end
	addRopeVertex (0.0f, 1.0f); // left at end

	for (const uint32_t index : quadIndices) m_indices[indexOffset++] = baseVertex + index;
    }

    m_activeIndexCount = static_cast<GLsizei> (indexOffset);
    if (m_activeIndexCount == 0) {
	return;
    }

#if !NDEBUG
    std::string str = "Rope particles ";
    str += this->getParticle ().name + " (" + std::to_string (this->getId ()) + ", " + this->getParticle ().particleFile
	+ ")";
    glPushDebugGroup (GL_DEBUG_SOURCE_APPLICATION, 0, -1, str.c_str ());
#endif

    uploadGeometryBuffers (
	static_cast<GLsizeiptr> (vertexIndex * ROPE_FLOATS_PER_VERTEX * sizeof (float)),
	static_cast<GLsizeiptr> (indexOffset * sizeof (uint32_t))
    );

    // Update matrices and uniform data
    updateMatrices ();

    // For REFRACT: blit current scene content into the copy FBO before rendering
    if (m_hasRefract && m_refractFBO) {
	auto sceneFBO = getScene ().getActiveRenderTarget ();
	GLint w = static_cast<GLint> (sceneFBO->getRealWidth ());
	GLint h = static_cast<GLint> (sceneFBO->getRealHeight ());
	m_refractFBO->resize (w, h);
	glBindFramebuffer (GL_READ_FRAMEBUFFER, sceneFBO->getFramebuffer ());
	glBindFramebuffer (GL_DRAW_FRAMEBUFFER, m_refractFBO->getFramebuffer ());
	glBlitFramebuffer (0, 0, w, h, 0, 0, w, h, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    }

    glEnable (GL_DEPTH_CLAMP);
    m_pass->render ();
    glDisable (GL_DEPTH_CLAMP);

#if !NDEBUG
    glPopDebugGroup ();
#endif
}
