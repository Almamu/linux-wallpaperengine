#include "CParticle.h"
#include "WallpaperEngine/Data/Model/Property.h"
#include "WallpaperEngine/Logging/Log.h"
#include "WallpaperEngine/Render/Utils/NoiseUtils.h"

#include <GL/glew.h>
#include <algorithm>
#include <cmath>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>

extern float g_Time;

using namespace WallpaperEngine::Render::Objects;
using namespace WallpaperEngine::Render::Utils;
using namespace WallpaperEngine::Data::Model;

namespace {
// Helper: Random float in range
inline float randomFloat (std::mt19937& rng, float min, float max) {
    if (max < min) {
	std::swap (min, max);
    }
    std::uniform_real_distribution<float> dist (min, max);
    return dist (rng);
}

// Helper: Random vec3 in range
inline glm::vec3 randomVec3 (std::mt19937& rng, const glm::vec3& min, const glm::vec3& max) {
    return glm::vec3 (
	randomFloat (rng, min.x, max.x), randomFloat (rng, min.y, max.y), randomFloat (rng, min.z, max.z)
    );
}

// Helper: Linear interpolation
inline float lerp (float t, float a, float b) { return a + t * (b - a); }

// Helper: Fade value change over lifetime
inline float fadeValue (float life, float startTime, float endTime, float startValue, float endValue) {
    if (life <= startTime) {
	return startValue;
    } else if (life >= endTime) {
	return endValue;
    } else {
	float t = (life - startTime) / (endTime - startTime);
	return lerp (t, startValue, endValue);
    }
}
}

CParticle::CParticle (Wallpapers::CScene& scene, const Particle& particle) :
    CRenderable (scene, particle), m_particle (particle) {
    // Initialize random number generator with time-based seed
    std::random_device rd;
    m_rng.seed (rd ());

    // Read renderer configuration early to determine if trails are used
    if (!m_particle.renderers.empty ()) {
	const auto& renderer = m_particle.renderers[0];
	if (renderer.name == "spritetrail" || renderer.name == "ropetrail") {
	    m_useTrailRenderer = true;
	    m_trailLength = renderer.length;
	    m_trailMaxLength = renderer.maxLength;
	    m_trailMinLength = renderer.minLength;
	    m_trailSubdivision = static_cast<int> (renderer.subdivision);
	    if (m_trailSubdivision < 1) {
		m_trailSubdivision = 1;
	    }
	}
    }

    // Apply count instance override to particle pool size
    float countMultiplier = particle.instanceOverride.count->value->getFloat ();
    uint32_t adjustedMaxCount = static_cast<uint32_t> (particle.maxCount * countMultiplier);

    // Use wallpaper's specified count, or default if maxCount is 0
    m_maxParticles = (adjustedMaxCount > 0) ? adjustedMaxCount : DEFAULT_MAX_PARTICLES;
    m_particles.resize (m_maxParticles);

    // Calculate buffer sizes
    // Trail particles: (N+1) * 2 vertices for ribbon strip, N * 6 indices for N quads
    // Normal particles: 4 vertices, 6 indices
    const int segmentsPerParticle = m_useTrailRenderer ? m_trailSubdivision : 1;
    const int verticesPerParticle = m_useTrailRenderer ? (segmentsPerParticle + 1) * 2 : 4;
    const int indicesPerParticle = m_useTrailRenderer ? segmentsPerParticle * 6 : 6;

    m_vertices.resize (m_maxParticles * verticesPerParticle * 17);
    m_indices.resize (m_maxParticles * indicesPerParticle);
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

    // Convert origin from screen space to centered space
    // Projection uses ortho(-width/2, width/2, -height/2, height/2)
    // but particle origins are in screen space where (0,0) is top-left
    m_lastScreenWidth = getScene ().getCamera ().getWidth ();
    m_lastScreenHeight = getScene ().getCamera ().getHeight ();

    glm::vec3 origin = m_particle.origin->value->getVec3 ();
    origin.x -= m_lastScreenWidth / 2.0f;
    origin.y = m_lastScreenHeight / 2.0f - origin.y;
    m_transformedOrigin = origin;

    // Load particle material constants
    if (m_particle.material && m_particle.material->material && !m_particle.material->material->passes.empty ()) {
	auto& firstPass = *m_particle.material->material->passes.begin ();

	// Read overbright constant (brightness multiplier for additive particles)
	auto overbrightIt = firstPass->constants.find ("ui_editor_properties_overbright");
	if (overbrightIt != firstPass->constants.end ()) {
	    m_overbright = overbrightIt->second->value->getFloat ();
	}
    }

    // Texture is resolved by CRenderable base class; read spritesheet data.
    // The GL texture may be a full atlas (e.g. 256x1280 with 5 frames of 256x256),
    // even for animated textures — getRealWidth/Height reports per-frame size but
    // the actual GL texture contains all frames. We need SPRITESHEET to UV-select frames.
    if (const auto texture = getTexture ()) {
	if (texture->isAnimated ()) {
	    // Animated texture: the GL texture is the full atlas, but getRealWidth/Height
	    // reports per-frame dimensions. Compute grid from atlas vs per-frame ratio.
	    const glm::vec4* res = texture->getResolution ();
	    float atlasW = res->x;
	    float atlasH = res->y;
	    float frameW = static_cast<float> (texture->getRealWidth ());
	    float frameH = static_cast<float> (texture->getRealHeight ());

	    if (frameW > 0.0f && frameH > 0.0f) {
		m_spritesheetCols = std::max (1, static_cast<int> (std::round (atlasW / frameW)));
		m_spritesheetRows = std::max (1, static_cast<int> (std::round (atlasH / frameH)));
		m_spritesheetFrames = m_spritesheetCols * m_spritesheetRows;
	    }
	    m_spritesheetDuration = texture->getSpritesheetDuration ();
	} else {
	    // Static texture: use spritesheet metadata from .tex-json if available
	    m_spritesheetCols = static_cast<int> (texture->getSpritesheetCols ());
	    m_spritesheetRows = static_cast<int> (texture->getSpritesheetRows ());
	    m_spritesheetFrames = static_cast<int> (texture->getSpritesheetFrames ());
	    m_spritesheetDuration = texture->getSpritesheetDuration ();
	}

	const glm::vec4* res = texture->getResolution ();
	sLog.debug ("Particle ", m_particle.name, ": spritesheet=", m_spritesheetFrames,
	    " (", m_spritesheetCols, "x", m_spritesheetRows, "), animated=", texture->isAnimated (),
	    ", animMode=", m_particle.animationMode,
	    ", texture=", texture->getRealWidth (), "x", texture->getRealHeight (),
	    ", atlas=", res->x, "x", res->y);
    }

    setupEmitters ();
    setupInitializers ();
    setupOperators ();
    setupPass ();

    // Setup control points (max 8)
    m_controlPoints.resize (8);
    for (const auto& cp : m_particle.controlPoints) {
	if (cp.id >= 0 && cp.id < 8) {
	    m_controlPoints[cp.id].offset = cp.offset;
	    // Link to mouse if either flags bit 0 is set
	    m_controlPoints[cp.id].linkMouse = (cp.flags & 1) != 0;
	    m_controlPoints[cp.id].worldSpace = (cp.flags & 2) != 0;

	    // Initialize position to offset for non-mouse-linked control points
	    // Mouse-linked CPs will have their position updated in update()
	    if (!m_controlPoints[cp.id].linkMouse) {
		if (m_controlPoints[cp.id].worldSpace) {
		    // World space: offset is in screen-centered coords, convert to particle local space
		    m_controlPoints[cp.id].position = cp.offset - m_transformedOrigin;
		} else {
		    // Local space: offset is already relative to particle system center
		    m_controlPoints[cp.id].position = cp.offset;
		}
	    }
	}
    }

    m_initialized = true;
}

void CParticle::render () {
    if (!m_initialized || !m_particle.visible->value->getBool ()) {
	return;
    }

    // Initialize time on first render to avoid huge dt spike
    if (m_time == 0.0) {
	m_time = g_Time;
	// Skip update on first frame to avoid weird initial burst
	// This ensures all particles start from a clean state
	renderSprites ();
	return;
    }

    // Update particles
    float dt = g_Time - static_cast<float> (m_time);
    m_time = g_Time;

    if (dt > 0.0f) {
	// Cap dt to prevent simulation instability
	// Also provides more consistent behavior across different FPS
	dt = std::min (dt, 0.1f);
	update (dt);
    }

    // Render particles
    if (m_particleCount > 0 && m_particle.material) {
	renderSprites ();
    }
}

void CParticle::update (float dt) {
    // Detect resolution changes and recalculate transformed origin
    float screenWidth = static_cast<float> (getScene ().getWidth ());
    float screenHeight = static_cast<float> (getScene ().getHeight ());

    if (screenWidth != m_lastScreenWidth || screenHeight != m_lastScreenHeight) {
	// Resolution changed - recalculate transformed origin
	glm::vec3 origin = m_particle.origin->value->getVec3 ();
	origin.x -= screenWidth / 2.0f;
	origin.y = screenHeight / 2.0f - origin.y;
	m_transformedOrigin = origin;

	// Update world-space control points that aren't mouse-linked
	for (size_t i = 0; i < m_controlPoints.size (); i++) {
	    auto& cp = m_controlPoints[i];
	    if (!cp.linkMouse && cp.worldSpace) {
		// Recalculate position from offset using new transformed origin
		cp.position = cp.offset - m_transformedOrigin;
	    }
	}

	m_lastScreenWidth = screenWidth;
	m_lastScreenHeight = screenHeight;
    }

    // Update control points with mouse position
    const glm::vec2* mousePos = getScene ().getMousePosition ();
    if (mousePos) {

	for (auto& cp : m_controlPoints) {
	    if (cp.linkMouse) {
		// Convert mouse position from normalized [0,1] to centered screen space
		glm::vec3 position;
		position.x = (mousePos->x * screenWidth) - (screenWidth / 2.0f);
		position.y = (screenHeight / 2.0f) - (mousePos->y * screenHeight);
		position.z = 0.0f;

		// Apply control point offset
		position += cp.offset;

		// Convert to particle local space to prevent double transformation by model matrix
		// Both world-space and local-space CPs are handled the same way now
		cp.position = position - m_transformedOrigin;
	    }
	}
    }

    // Emit particles
    for (auto& emitter : m_emitters) {
	emitter (m_particles, m_particleCount, dt);
    }

    // Update particle age
    for (uint32_t i = 0; i < m_particleCount; i++) {
	m_particles[i].age += dt;
    }

    // Apply operators to living particles (including alphafade)
    for (auto& op : m_operators) {
	op (m_particles, m_particleCount, m_controlPoints, static_cast<float> (m_time), dt);
    }

    // Update animation frames and remove dead particles
    for (uint32_t i = 0; i < m_particleCount;) {
	auto& p = m_particles[i];

	// Update animation frame if we have a spritesheet
	if (m_spritesheetFrames > 0) {
	    // Calculate frame based on particle lifetime
	    float lifetimePos = p.getLifetimePos ();

	    // Apply sequence multiplier if present
	    float animSpeed = m_particle.sequenceMultiplier > 0.0f ? m_particle.sequenceMultiplier : 1.0f;

	    // Calculate frame based on animation mode
	    if (m_particle.animationMode == "randomframe") {
		// Random frame mode: frame is set once at spawn and never changes
		if (p.frame < 0.0f) {
		    // Use particle memory address as seed for deterministic randomness per particle
		    std::mt19937 particleRng (
			static_cast<std::mt19937::result_type> (reinterpret_cast<uintptr_t> (&p))
		    );
		    std::uniform_int_distribution<int> dist (0, m_spritesheetFrames - 1);
		    p.frame = static_cast<float> (dist (particleRng));
		}
	    } else if (m_particle.animationMode == "once") {
		// Play animation once over particle lifetime
		p.frame = std::min (
		    lifetimePos * m_spritesheetFrames * animSpeed, static_cast<float> (m_spritesheetFrames - 1)
		);
	    } else {
		// Default to "loop" or "sequence" mode - loop animation based on duration
		if (m_spritesheetDuration > 0.0f) {
		    float timeInCycle = std::fmod (p.age * animSpeed, m_spritesheetDuration);
		    float cyclePos = timeInCycle / m_spritesheetDuration;
		    p.frame = std::fmod (cyclePos * m_spritesheetFrames, static_cast<float> (m_spritesheetFrames));
		} else {
		    // No duration, use lifetime-based for sequence mode
		    p.frame = std::fmod (
			lifetimePos * m_spritesheetFrames * animSpeed, static_cast<float> (m_spritesheetFrames)
		    );
		}
	    }
	}

	// Consider particle dead if: not alive, OR size is zero/negative (from sizechange operator)
	// This ensures invisible particles are removed from pool to make room for new ones
	if (!p.isAlive () || p.size <= 0.0f) {
	    // Swap with last particle and reduce count
	    if (i < m_particleCount - 1) {
		p = m_particles[m_particleCount - 1];
	    }
	    m_particleCount--;
	} else {
	    i++;
	}
    }
}

const Particle& CParticle::getParticle () const { return m_particle; }

const float& CParticle::getBrightness () const { return m_overbright; }

const float& CParticle::getUserAlpha () const { return m_particle.instanceOverride.alpha->value->getFloat (); }

const float& CParticle::getAlpha () const { return m_particle.instanceOverride.alpha->value->getFloat (); }

const glm::vec3& CParticle::getColor () const {
    static const glm::vec3 defaultColor (1.0f);
    if (m_particle.instanceOverride.color && m_particle.instanceOverride.color->value) {
	return m_particle.instanceOverride.color->value->getVec3 ();
    }
    return defaultColor;
}

const glm::vec4& CParticle::getColor4 () const {
    static const glm::vec4 defaultColor (1.0f);
    if (m_particle.instanceOverride.color && m_particle.instanceOverride.color->value) {
	return m_particle.instanceOverride.color->value->getVec4 ();
    }
    return defaultColor;
}

const glm::vec3& CParticle::getCompositeColor () const { return getColor (); }

// ========== EMITTERS ==========

void CParticle::setupEmitters () {
    for (const auto& emitter : m_particle.emitters) {
	EmitterFunc func;

	if (emitter.name == "boxrandom") {
	    func = createBoxEmitter (emitter);
	} else if (emitter.name == "sphererandom") {
	    func = createSphereEmitter (emitter);
	} else {
	    sLog.out ("Unknown emitter type: ", emitter.name);
	    continue;
	}

	if (func) {
	    m_emitters.push_back (std::move (func));
	}
    }
}

EmitterFunc CParticle::createBoxEmitter (const ParticleEmitter& emitter) {
    float rate = emitter.rate * m_particle.instanceOverride.rate->value->getFloat ();

    glm::vec3 transformedEmitterOrigin = emitter.origin;
    transformedEmitterOrigin.y = -transformedEmitterOrigin.y;

    int controlPointIndex = emitter.controlPoint;
    if (controlPointIndex == -1 && !m_particle.controlPoints.empty ()) {
	const auto& cp0 = m_particle.controlPoints[0];
	if ((cp0.flags & 1) != 0) {
	    controlPointIndex = 0;
	}
    }

    glm::vec3 flippedDirections = emitter.directions;
    flippedDirections.y = -flippedDirections.y;

    bool limitOnePerFrame = (emitter.flags & 2) != 0;
    bool randomPeriodicEmission = (emitter.flags & 4) != 0;

    return [this, emitter, transformedEmitterOrigin, controlPointIndex, rate, flippedDirections, limitOnePerFrame,
	    randomPeriodicEmission, emissionTimer = 0.0f, elapsedTime = 0.0f, delayTimer = emitter.delay,
	    durationTimer = 0.0f, periodicTimer = 0.0f, periodicDuration = 0.0f, periodicDelay = 0.0f, emitting = false,
	    instantaneousEmitted
	    = false] (std::vector<ParticleInstance>& particles, uint32_t& count, float dt) mutable {
	if (count >= particles.size ()) {
	    return;
	}

	elapsedTime += dt;

	// Handle delay
	if (delayTimer > 0.0f) {
	    delayTimer -= dt;
	    return;
	}

	// Handle duration
	if (emitter.duration > 0.0f) {
	    durationTimer += dt;
	    if (durationTimer >= emitter.duration) {
		return;
	    }
	}

	// Handle random periodic emission
	if (randomPeriodicEmission) {
	    periodicTimer += dt;

	    if (!emitting) {
		if (periodicTimer >= periodicDelay) {
		    emitting = true;
		    periodicTimer = 0.0f;
		    periodicDuration = randomFloat (m_rng, emitter.minPeriodicDuration, emitter.maxPeriodicDuration);
		} else {
		    return;
		}
	    } else {
		if (periodicTimer >= periodicDuration) {
		    emitting = false;
		    periodicTimer = 0.0f;
		    periodicDelay = randomFloat (m_rng, emitter.minPeriodicDelay, emitter.maxPeriodicDelay);
		    return;
		}
	    }
	}

	// TODO: Audio processing (audioProcessingMode, audioProcessingBounds, etc.)

	// Handle instantaneous emission
	uint32_t toEmit = 0;
	if (emitter.instantaneous > 0 && !instantaneousEmitted) {
	    toEmit = emitter.instantaneous;
	    instantaneousEmitted = true;
	}

	// Handle rate-based emission
	if (emitter.rate > 0.0f && !limitOnePerFrame) {
	    emissionTimer += dt * rate;
	    toEmit += static_cast<uint32_t> (emissionTimer);
	    emissionTimer -= static_cast<float> (static_cast<uint32_t> (emissionTimer));
	}

	// Handle limit one per frame (flags bit 1)
	// Reset at start of each frame, then emit exactly one particle if rate > 0
	if (limitOnePerFrame && emitter.rate > 0.0f) {
	    toEmit += 1; // Always emit exactly 1 per frame
	}

	// Emit particles
	for (uint32_t i = 0; i < toEmit && count < particles.size (); i++) {
	    auto& p = particles[count];

	    glm::vec3 spawnOrigin = transformedEmitterOrigin;
	    if (controlPointIndex >= 0 && controlPointIndex < static_cast<int> (m_controlPoints.size ())) {
		spawnOrigin += m_controlPoints[controlPointIndex].position;
	    }

	    // Generate random position within box volume centered on origin
	    // This creates a centered box (or hollow box if distanceMin > 0)
	    glm::vec3 randomPos;
	    for (int axis = 0; axis < 3; axis++) {
		float minDist = emitter.distanceMin[axis];
		float maxDist = emitter.distanceMax[axis];
		// Generate value in [minDist, maxDist]
		float dist = randomFloat (m_rng, minDist, maxDist);
		// Randomly flip sign to center the distribution
		if (randomFloat (m_rng, 0.0f, 1.0f) < 0.5f) {
		    dist = -dist;
		}
		randomPos[axis] = dist;
	    }
	    randomPos *= flippedDirections;

	    p.position = spawnOrigin + randomPos;

	    // Emitter does not set velocity - initializers handle that
	    p.velocity = glm::vec3 (0.0f);
	    p.acceleration = glm::vec3 (0.0f);
	    p.rotation = glm::vec3 (0.0f);
	    p.angularVelocity = glm::vec3 (0.0f);
	    p.angularAcceleration = glm::vec3 (0.0f);

	    p.color = glm::vec3 (1.0f) * m_particle.instanceOverride.colorn->value->getVec3 ();
	    p.alpha = 1.0f * m_particle.instanceOverride.alpha->value->getFloat ();
	    p.size = 20.0f * m_particle.instanceOverride.size->value->getFloat ();
	    p.lifetime = 1.0f * m_particle.instanceOverride.lifetime->value->getFloat ();
	    p.age = 0.0f;
	    p.alive = true;
	    p.frame = -1.0f;

	    p.initial.color = p.color;
	    p.initial.alpha = p.alpha;
	    p.initial.size = p.size;
	    p.initial.lifetime = p.lifetime;

	    // Reset oscillator state for reused particles
	    p.oscillateAlpha = {};
	    p.oscillateSize = {};
	    p.oscillatePosition = {};

	    // Apply initializers
	    for (auto& init : m_initializers) {
		init (p);
	    }

	    count++;
	}
    };
}

EmitterFunc CParticle::createSphereEmitter (const ParticleEmitter& emitter) {
    float rate = emitter.rate * m_particle.instanceOverride.rate->value->getFloat ();
    float lifetime = 1.0f * m_particle.instanceOverride.lifetime->value->getFloat ();

    // Convert emitter origin from screen space (Y down) to centered space (Y up)
    glm::vec3 transformedEmitterOrigin = emitter.origin;
    transformedEmitterOrigin.y = -transformedEmitterOrigin.y;

    int controlPointIndex = emitter.controlPoint;

    // Auto-detect control point 0 usage if controlPoint field not specified and CP0 has linkMouse
    if (controlPointIndex == -1 && !m_particle.controlPoints.empty ()) {
	const auto& cp0 = m_particle.controlPoints[0];
	if ((cp0.flags & 1) != 0) { // Bit 0: linkMouse flag
	    controlPointIndex = 0;
	}
    }

    return [this, emitter, transformedEmitterOrigin, controlPointIndex, rate, lifetime, emissionTimer = 0.0f,
	    remaining
	    = emitter.instantaneous] (std::vector<ParticleInstance>& particles, uint32_t& count, float dt) mutable {
	if (count >= particles.size ()) {
	    return;
	}

	emissionTimer += dt * rate;

	uint32_t toEmit = static_cast<uint32_t> (emissionTimer);
	emissionTimer -= static_cast<float> (toEmit);

	if (remaining > 0) {
	    toEmit = remaining;
	    remaining = 0;
	}

	for (uint32_t i = 0; i < toEmit && count < particles.size (); i++) {
	    auto& p = particles[count];

	    // Determine spawn origin (control point or emitter origin)
	    glm::vec3 spawnOrigin = transformedEmitterOrigin;
	    if (controlPointIndex >= 0 && controlPointIndex < static_cast<int> (m_controlPoints.size ())) {
		spawnOrigin += m_controlPoints[controlPointIndex].position;
	    }

	    // Spawn at random position on ellipsoid surface
	    glm::vec3 randomPos;

	    // Orthographic particles (flags & 4 == 0): use 2D disk distribution in X/Y plane
	    // Perspective particles (flags & 4 != 0): use 3D spherical shell distribution
	    if ((m_particle.flags & 4) == 0) {
		// 2D disk distribution with random Z offset
		float angle = randomFloat (m_rng, 0.0f, glm::two_pi<float> ());
		float minRadius = emitter.distanceMin.x;
		float maxRadius = emitter.distanceMax.x;

		// Use sqrt for uniform area distribution in annulus
		float minRadiusSq = minRadius * minRadius;
		float maxRadiusSq = maxRadius * maxRadius;
		float radiusXY = std::sqrt (randomFloat (m_rng, minRadiusSq, maxRadiusSq));

		randomPos = glm::vec3 (
		    radiusXY * std::cos (angle), radiusXY * std::sin (angle), randomFloat (m_rng, -maxRadius, maxRadius)
		);

		randomPos *= emitter.directions;
	    } else {
		// 3D spherical shell distribution
		float theta = randomFloat (m_rng, 0.0f, glm::two_pi<float> ());
		float cosTheta = randomFloat (m_rng, -1.0f, 1.0f);
		float sinTheta = std::sqrt (1.0f - cosTheta * cosTheta);

		randomPos = glm::vec3 (sinTheta * std::cos (theta), sinTheta * std::sin (theta), cosTheta);

		// Use cubic root for uniform volume distribution
		float minRadius = emitter.distanceMin.x;
		float maxRadius = emitter.distanceMax.x;
		float minRadiusCubed = minRadius * minRadius * minRadius;
		float maxRadiusCubed = maxRadius * maxRadius * maxRadius;
		float radius = std::cbrt (randomFloat (m_rng, minRadiusCubed, maxRadiusCubed));

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
	    p.position = spawnOrigin + randomPos;

	    // Set velocity only if emitter specifies speed (otherwise use initializers)
	    if (emitter.speedMax > 0.0f || emitter.speedMin != 0.0f) {
		// Velocity pointing outward from ellipsoid (randomPos already includes directions scaling)
		glm::vec3 direction
		    = glm::length (randomPos) > 0.0f ? glm::normalize (randomPos) : glm::vec3 (0.0f, 1.0f, 0.0f);
		float speed = randomFloat (m_rng, emitter.speedMin, emitter.speedMax);
		p.velocity = direction * speed;
	    } else {
		// No emitter speed specified, velocity will be set by initializers
		p.velocity = glm::vec3 (0.0f);
	    }

	    p.acceleration = glm::vec3 (0.0f);
	    p.rotation = glm::vec3 (0.0f);
	    p.angularVelocity = glm::vec3 (0.0f);
	    p.angularAcceleration = glm::vec3 (0.0f);

	    p.color = glm::vec3 (1.0f) * m_particle.instanceOverride.colorn->value->getVec3 ();
	    p.alpha = 1.0f * m_particle.instanceOverride.alpha->value->getFloat ();
	    p.size = 20.0f * m_particle.instanceOverride.size->value->getFloat ();
	    p.lifetime = lifetime;
	    p.age = 0.0f;
	    p.alive = true;
	    p.frame = -1.0f;

	    p.initial.color = p.color;
	    p.initial.alpha = p.alpha;
	    p.initial.size = p.size;
	    p.initial.lifetime = p.lifetime;

	    // Reset oscillator state for reused particles
	    p.oscillateAlpha = {};
	    p.oscillateSize = {};
	    p.oscillatePosition = {};

	    for (auto& init : m_initializers) {
		init (p);
	    }

	    count++;
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

	if (initializer->is<ColorRandomInitializer> ()) {
	    func = createColorRandomInitializer (*initializer->as<ColorRandomInitializer> ());
	} else if (initializer->is<SizeRandomInitializer> ()) {
	    func = createSizeRandomInitializer (*initializer->as<SizeRandomInitializer> ());
	} else if (initializer->is<AlphaRandomInitializer> ()) {
	    func = createAlphaRandomInitializer (*initializer->as<AlphaRandomInitializer> ());
	} else if (initializer->is<LifetimeRandomInitializer> ()) {
	    func = createLifetimeRandomInitializer (*initializer->as<LifetimeRandomInitializer> ());
	} else if (initializer->is<VelocityRandomInitializer> ()) {
	    func = createVelocityRandomInitializer (*initializer->as<VelocityRandomInitializer> ());
	} else if (initializer->is<RotationRandomInitializer> ()) {
	    func = createRotationRandomInitializer (*initializer->as<RotationRandomInitializer> ());
	} else if (initializer->is<AngularVelocityRandomInitializer> ()) {
	    func = createAngularVelocityRandomInitializer (*initializer->as<AngularVelocityRandomInitializer> ());
	} else if (initializer->is<TurbulentVelocityRandomInitializer> ()) {
	    func = createTurbulentVelocityRandomInitializer (*initializer->as<TurbulentVelocityRandomInitializer> ());
	} else if (initializer->is<MapSequenceAroundControlPointInitializer> ()) {
	    func = createMapSequenceAroundControlPointInitializer (
		*initializer->as<MapSequenceAroundControlPointInitializer> ()
	    );
	} else {
	    sLog.out ("Unknown initializer type");
	}

	if (func) {
	    m_initializers.push_back (std::move (func));
	}
    }
}

InitializerFunc CParticle::createColorRandomInitializer (const ColorRandomInitializer& init) {
    DynamicValue* minValue = init.min->value.get ();
    DynamicValue* maxValue = init.max->value.get ();
    DynamicValue* colorOverride = m_particle.instanceOverride.colorn->value.get ();

    return [this, minValue, maxValue, colorOverride] (ParticleInstance& p) {
	p.color = randomVec3 (m_rng, minValue->getVec3 (), maxValue->getVec3 ()) * colorOverride->getVec3 ();
	p.initial.color = p.color;
    };
}

InitializerFunc CParticle::createSizeRandomInitializer (const SizeRandomInitializer& init) {
    DynamicValue* minValue = init.min->value.get ();
    DynamicValue* maxValue = init.max->value.get ();
    DynamicValue* exponentValue = init.exponent->value.get ();
    DynamicValue* sizeOverride = m_particle.instanceOverride.size->value.get ();

    return [this, minValue, maxValue, exponentValue, sizeOverride] (ParticleInstance& p) {
	float t = randomFloat (m_rng, 0.0f, 1.0f);
	float exponent = exponentValue->getFloat ();
	float min = minValue->getFloat ();
	float max = maxValue->getFloat ();

	// Apply exponent for non-linear distribution
	float adjustedT = std::pow (t, exponent);
	p.size = (min + adjustedT * (max - min)) * sizeOverride->getFloat () / 2.0f;
	p.initial.size = p.size;
    };
}

InitializerFunc CParticle::createAlphaRandomInitializer (const AlphaRandomInitializer& init) {
    DynamicValue* minValue = init.min->value.get ();
    DynamicValue* maxValue = init.max->value.get ();
    DynamicValue* alphaOverride = m_particle.instanceOverride.alpha->value.get ();

    return [this, minValue, maxValue, alphaOverride] (ParticleInstance& p) {
	p.alpha = randomFloat (m_rng, minValue->getFloat (), maxValue->getFloat ()) * alphaOverride->getFloat ();
	p.initial.alpha = p.alpha;
    };
}

InitializerFunc CParticle::createLifetimeRandomInitializer (const LifetimeRandomInitializer& init) {
    DynamicValue* minValue = init.min->value.get ();
    DynamicValue* maxValue = init.max->value.get ();
    DynamicValue* lifetimeOverride = m_particle.instanceOverride.lifetime->value.get ();

    return [this, minValue, maxValue, lifetimeOverride] (ParticleInstance& p) {
	p.lifetime = randomFloat (m_rng, minValue->getFloat (), maxValue->getFloat ()) * lifetimeOverride->getFloat ();
	p.initial.lifetime = p.lifetime;
    };
}

InitializerFunc CParticle::createVelocityRandomInitializer (const VelocityRandomInitializer& init) {
    DynamicValue* minValue = init.min->value.get ();
    DynamicValue* maxValue = init.max->value.get ();
    DynamicValue* speedOverride = m_particle.instanceOverride.speed->value.get ();

    return [this, minValue, maxValue, speedOverride] (ParticleInstance& p) {
	glm::vec3 vel = randomVec3 (m_rng, minValue->getVec3 (), maxValue->getVec3 ()) * speedOverride->getFloat ();
	vel.y = -vel.y;
	p.velocity += vel;
    };
}

InitializerFunc CParticle::createRotationRandomInitializer (const RotationRandomInitializer& init) {
    DynamicValue* minValue = init.min->value.get ();
    DynamicValue* maxValue = init.max->value.get ();
    DynamicValue* speedOverride = m_particle.instanceOverride.speed->value.get ();

    return [this, minValue, maxValue, speedOverride] (ParticleInstance& p) {
	p.rotation = randomVec3 (m_rng, minValue->getVec3 (), maxValue->getVec3 ()) * speedOverride->getFloat ();
    };
}

InitializerFunc CParticle::createAngularVelocityRandomInitializer (const AngularVelocityRandomInitializer& init) {
    DynamicValue* minValue = init.min->value.get ();
    DynamicValue* maxValue = init.max->value.get ();
    DynamicValue* exponentValue = init.exponent->value.get ();
    DynamicValue* speedOverride = m_particle.instanceOverride.speed->value.get ();

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
	    float t = randomFloat (m_rng, 0.0f, 1.0f);
	    t = std::pow (t, exponent);
	    result[i] = minVec[i] + t * (maxVec[i] - minVec[i]);
	}

	p.angularVelocity = result * speedOverride->getFloat ();
    };
}

InitializerFunc CParticle::createTurbulentVelocityRandomInitializer (const TurbulentVelocityRandomInitializer& init) {
    DynamicValue* speedMin = init.speedMin->value.get ();
    DynamicValue* speedMax = init.speedMax->value.get ();
    DynamicValue* offsetVal = init.offset->value.get ();
    DynamicValue* scaleVal = init.scale->value.get ();
    DynamicValue* forwardVal = init.forward->value.get ();
    DynamicValue* phaseMinVal = init.phaseMin->value.get ();
    DynamicValue* phaseMaxVal = init.phaseMax->value.get ();
    DynamicValue* rightVal = init.right->value.get ();
    DynamicValue* speedOverride = m_particle.instanceOverride.speed->value.get ();

    return [this, speedMin, speedMax, offsetVal, scaleVal, forwardVal, phaseMinVal, phaseMaxVal, rightVal,
	    speedOverride] (ParticleInstance& p) {
	// Get direction parameters
	glm::vec3 forward = forwardVal->getVec3 ();
	glm::vec3 right = rightVal->getVec3 ();
	// Y-flip for coordinate system conversion
	forward.y = -forward.y;
	right.y = -right.y;

	if (glm::length (forward) > 0.0001f) {
	    forward = glm::normalize (forward);
	}
	if (glm::length (right) > 0.0001f) {
	    right = glm::normalize (right);
	}

	float speed = randomFloat (m_rng, speedMin->getFloat (), speedMax->getFloat ());
	float scale = scaleVal->getFloat ();
	float offset = offsetVal->getFloat ();
	float phaseMin = phaseMinVal->getFloat ();
	float phaseMax = phaseMaxVal->getFloat ();

	// Each particle gets its own random noise position (no shared state)
	glm::vec3 noisePos = randomVec3 (m_rng, glm::vec3 (0.0f), glm::vec3 (10.0f));

	// Phase adds per-particle randomization to noise position
	float phase = randomFloat (m_rng, phaseMin, phaseMax);
	glm::vec3 samplePos = noisePos + glm::vec3 (phase, phase * 0.7f, phase * 1.3f);

	// Sample curl noise for direction and normalize
	glm::vec3 result = curlNoise (samplePos);
	float len = glm::length (result);
	if (len < 0.0001f) {
	    result = forward;
	} else {
	    result = result / len;
	}

	// Scale limits how far direction can deviate from forward
	if (scale < 2.0f) {
	    float cosAngle = glm::dot (result, forward);
	    float angle = std::acos (glm::clamp (cosAngle, -1.0f, 1.0f)) / glm::pi<float> ();
	    float maxAngle = scale / 2.0f;

	    if (angle > maxAngle && maxAngle > 0.0001f) {
		glm::vec3 axis = glm::cross (result, forward);
		float axisLen = glm::length (axis);
		if (axisLen > 0.0001f) {
		    axis = axis / axisLen;
		    float rotAngle = (angle - maxAngle) * glm::pi<float> ();
		    glm::mat3 rot = glm::mat3 (glm::rotate (glm::mat4 (1.0f), rotAngle, axis));
		    result = rot * result;
		}
	    }
	}

	// Offset rotates result around right axis (tilts up/down)
	if (std::abs (offset) > 0.0001f) {
	    glm::mat3 rot = glm::mat3 (glm::rotate (glm::mat4 (1.0f), -offset, right));
	    result = rot * result;
	}

	// Apply speed and instance override
	glm::vec3 finalVel = result * speed * speedOverride->getFloat ();

	p.velocity += finalVel;
    };
}

InitializerFunc
CParticle::createMapSequenceAroundControlPointInitializer (const MapSequenceAroundControlPointInitializer& init) {
    DynamicValue* controlPointValue = init.controlPoint->value.get ();
    DynamicValue* countValue = init.count->value.get ();
    DynamicValue* speedMinValue = init.speedMin->value.get ();
    DynamicValue* speedMaxValue = init.speedMax->value.get ();
    DynamicValue* speedOverride = m_particle.instanceOverride.speed->value.get ();

    // Sequence counter shared across all particles spawned with this initializer
    // This creates the circular distribution pattern
    int sequenceIndex = 0;

    return [this, controlPointValue, countValue, speedMinValue, speedMaxValue, sequenceIndex,
	    speedOverride] (ParticleInstance& p) mutable {
	int controlPoint = static_cast<int> (controlPointValue->getFloat ());
	int count = static_cast<int> (countValue->getFloat ());

	// Calculate angle for this particle in the sequence (evenly distributed around circle)
	float angle = (static_cast<float> (sequenceIndex) / static_cast<float> (count)) * glm::two_pi<float> ();
	sequenceIndex = (sequenceIndex + 1) % count; // Wrap around after reaching count

	// Get control point position to spawn around
	glm::vec3 centerPos = glm::vec3 (0.0f);
	if (controlPoint >= 0 && controlPoint < static_cast<int> (m_controlPoints.size ())) {
	    centerPos = m_controlPoints[controlPoint].position;
	}

	// Set particle position in circular pattern around control point
	// This creates the natural clustering seen in the original
	p.position = centerPos;

	// Set velocity based on angle and speed range
	glm::vec3 speedMin = speedMinValue->getVec3 ();
	glm::vec3 speedMax = speedMaxValue->getVec3 ();
	glm::vec3 speed = randomVec3 (m_rng, speedMin, speedMax);

	// Flip Y before rotation to convert to centered space
	speed.y = -speed.y;

	// Rotate velocity based on sequence angle (creates outward radial pattern)
	glm::mat3 rotationMatrix = glm::mat3 (
	    std::cos (angle), -std::sin (angle), 0.0f, std::sin (angle), std::cos (angle), 0.0f, 0.0f, 0.0f, 1.0f
	);
	glm::vec3 rotatedSpeed = rotationMatrix * speed * speedOverride->getFloat ();

	// Set velocity (speed override applied in movement operator)
	p.velocity = rotatedSpeed;
    };
}

// ========== OPERATORS ==========

void CParticle::setupOperators () {
    for (const auto& op : m_particle.operators) {
	if (!op) {
	    continue;
	}

	OperatorFunc func;

	if (op->is<MovementOperator> ()) {
	    func = createMovementOperator (*op->as<MovementOperator> ());
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
    DynamicValue* speedOverride = m_particle.instanceOverride.speed->value.get ();

    return [dragValue, gravityValue, speedOverride] (
	       std::vector<ParticleInstance>& particles, uint32_t count, const std::vector<ControlPointData>&, float,
	       float dt
	   ) {
	float speed = speedOverride->getFloat ();
	float drag = dragValue->getFloat ();
	glm::vec3 gravity = gravityValue->getVec3 ();
	// Flip gravity Y for centered space
	gravity.y = -gravity.y;

	for (uint32_t i = 0; i < count; i++) {
	    auto& p = particles[i];
	    if (!p.alive) {
		continue;
	    }

	    // Update position FIRST using current velocity
	    // Velocity is already scaled by speed override
	    p.position += p.velocity * dt;

	    // Then apply forces to modify velocity for NEXT frame
	    // Apply gravity
	    p.velocity += gravity * dt * speed;

	    // Apply drag (velocity decay)
	    // Clamp to prevent velocity reversal if drag*dt > 1.0
	    float dragFactor = 1.0f - (drag * dt);
	    if (dragFactor < 0.0f) {
		dragFactor = 0.0f;
	    }
	    p.velocity *= dragFactor;
	}
    };
}

OperatorFunc CParticle::createAngularMovementOperator (const AngularMovementOperator& op) {
    DynamicValue* dragValue = op.drag->value.get ();
    DynamicValue* forceValue = op.force->value.get ();
    DynamicValue* speedOverride = m_particle.instanceOverride.speed->value.get ();

    return [dragValue, forceValue, speedOverride] (
	       std::vector<ParticleInstance>& particles, uint32_t count, const std::vector<ControlPointData>&, float,
	       float dt
	   ) {
	float drag = dragValue->getFloat ();
	float speed = speedOverride->getFloat ();
	glm::vec3 force = forceValue->getVec3 ();

	for (uint32_t i = 0; i < count; i++) {
	    auto& p = particles[i];
	    if (!p.alive) {
		continue;
	    }

	    // Update rotation using current angular velocity
	    p.rotation += p.angularVelocity * dt * speed;

	    // Apply force (angular acceleration)
	    p.angularVelocity += force * dt * speed;

	    // Apply drag (angular velocity decay)
	    // Positive drag slows down, negative drag speeds up
	    // Clamp to prevent velocity reversal if drag*dt > 1.0
	    float dragFactor = 1.0f - (drag * dt);
	    if (dragFactor < 0.0f) {
		dragFactor = 0.0f;
	    }
	    p.angularVelocity *= dragFactor;

	    // Wrap rotation to prevent floating-point precision issues
	    const float pi = glm::pi<float> ();
	    const float two_pi = glm::two_pi<float> ();
	    for (int j = 0; j < 3; j++) {
		while (p.rotation[j] > pi) {
		    p.rotation[j] -= two_pi;
		}
		while (p.rotation[j] < -pi) {
		    p.rotation[j] += two_pi;
		}
	    }
	}
    };
}

OperatorFunc CParticle::createAlphaFadeOperator (const AlphaFadeOperator& op) {
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
		if (!p.alive) {
		    continue;
		}

		float life = p.getLifetimePos ();

		if (life <= fadeInTime) {
		    float fade = fadeValue (life, 0.0f, fadeInTime, 0.0f, 1.0f);
		    p.alpha = p.initial.alpha * fade;
		} else if (life > fadeOutTime) {
		    float fade = 1.0f - fadeValue (life, fadeOutTime, 1.0f, 0.0f, 1.0f);
		    p.alpha = p.initial.alpha * fade;
		} else {
		    p.alpha = p.initial.alpha;
		}

		// Update oscillator base so oscillateAlpha combines properly
		p.oscillateAlpha.base = p.alpha;
	    }
	};
}

OperatorFunc CParticle::createSizeChangeOperator (const SizeChangeOperator& op) {
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
		if (!p.alive) {
		    continue;
		}

		float life = p.getLifetimePos ();
		float multiplier = fadeValue (life, startTime, endTime, startValue, endValue);
		p.size = p.initial.size * multiplier;

		// Update oscillator base so oscillateSize combines properly
		p.oscillateSize.base = p.size;
	    }
	};
}

OperatorFunc CParticle::createAlphaChangeOperator (const AlphaChangeOperator& op) {
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
		if (!p.alive) {
		    continue;
		}

		float life = p.getLifetimePos ();
		float multiplier = fadeValue (life, startTime, endTime, startValue, endValue);
		p.alpha = p.initial.alpha * multiplier;

		// Update oscillator base so oscillateAlpha combines properly
		p.oscillateAlpha.base = p.alpha;
	    }
	};
}

OperatorFunc CParticle::createColorChangeOperator (const ColorChangeOperator& op) {
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
	    glm::vec3 startValue = startValueValue->getVec3 ();
	    glm::vec3 endValue = endValueValue->getVec3 ();

	    for (uint32_t i = 0; i < count; i++) {
		auto& p = particles[i];
		if (!p.alive) {
		    continue;
		}

		float life = p.getLifetimePos ();

		glm::vec3 color;
		color.r = fadeValue (life, startTime, endTime, startValue.r, endValue.r);
		color.g = fadeValue (life, startTime, endTime, startValue.g, endValue.g);
		color.b = fadeValue (life, startTime, endTime, startValue.b, endValue.b);

		p.color = p.initial.color * color;
	    }
	};
}

OperatorFunc CParticle::createTurbulenceOperator (const TurbulenceOperator& op) {
    DynamicValue* scaleValue = op.scale->value.get ();
    DynamicValue* speedMinValue = op.speedMin->value.get ();
    DynamicValue* speedMaxValue = op.speedMax->value.get ();
    DynamicValue* timeScaleValue = op.timeScale->value.get ();
    DynamicValue* maskValue = op.mask->value.get ();
    DynamicValue* phaseMinValue = op.phaseMin->value.get ();
    DynamicValue* phaseMaxValue = op.phaseMax->value.get ();
    DynamicValue* speedOverride = m_particle.instanceOverride.speed->value.get ();

    // TODO: Audio processing support
    // DynamicValue* audioModeValue = op.audioProcessingMode->value.get ();
    // DynamicValue* audioBoundsValue = op.audioProcessingBounds->value.get ();
    // DynamicValue* audioExponentValue = op.audioProcessingExponent->value.get ();
    // DynamicValue* audioFreqStartValue = op.audioProcessingFrequencyStart->value.get ();
    // DynamicValue* audioFreqEndValue = op.audioProcessingFrequencyEnd->value.get ();

    // Phase and speed are randomized once per operator instance, not per particle
    const float phase = randomFloat (m_rng, phaseMinValue->getFloat (), phaseMaxValue->getFloat ());
    const float turbSpeed = randomFloat (m_rng, speedMinValue->getFloat (), speedMaxValue->getFloat ());

    return [scaleValue, timeScaleValue, maskValue, speedOverride, phase, turbSpeed] (
	       std::vector<ParticleInstance>& particles, uint32_t count, const std::vector<ControlPointData>&,
	       float currentTime, float dt
	   ) {
	const float noiseScale = scaleValue->getFloat () * 2.0f;
	const float timeScale = timeScaleValue->getFloat ();
	const glm::vec3 mask = maskValue->getVec3 ();
	const float speed = speedOverride->getFloat ();

	if (turbSpeed <= 0.0001f) {
	    return;
	}

	for (size_t i = 0; i < count; ++i) {
	    ParticleInstance& p = particles[i];
	    if (!p.alive) {
		continue;
	    }

	    glm::vec3 noisePos = p.position;
	    noisePos.x += phase + timeScale * currentTime;
	    noisePos *= noiseScale;

	    glm::vec3 curlDir = curlNoise (noisePos);
	    const float len = glm::length (curlDir);
	    if (len > 0.0001f) {
		curlDir = (curlDir / len) * turbSpeed;
	    }

	    curlDir *= mask;
	    p.velocity += curlDir * dt * speed;
	}
    };
}

OperatorFunc CParticle::createVortexOperator (const VortexOperator& op) {
    int controlPoint = op.controlPoint;
    int flags = op.flags;
    DynamicValue* axisValue = op.axis->value.get ();
    DynamicValue* offsetValue = op.offset->value.get ();
    DynamicValue* distanceInnerValue = op.distanceInner->value.get ();
    DynamicValue* distanceOuterValue = op.distanceOuter->value.get ();
    DynamicValue* speedInnerValue = op.speedInner->value.get ();
    DynamicValue* speedOuterValue = op.speedOuter->value.get ();
    DynamicValue* centerForceValue = op.centerForce->value.get ();
    DynamicValue* ringRadiusValue = op.ringRadius->value.get ();
    DynamicValue* ringWidthValue = op.ringWidth->value.get ();
    DynamicValue* ringPullDistanceValue = op.ringPullDistance->value.get ();
    DynamicValue* ringPullForceValue = op.ringPullForce->value.get ();
    DynamicValue* audioModeValue = op.audioProcessingMode->value.get ();
    DynamicValue* speedOverride = m_particle.instanceOverride.speed->value.get ();

    // Check if audio processing is enabled
    int audioMode = static_cast<int> (audioModeValue->getFloat ());

    // Extract flag bits
    bool infiniteAxis = (flags & 1) != 0;
    bool maintainDistance = (flags & 2) != 0;
    bool ringShape = (flags & 4) != 0;

    return [this, controlPoint, axisValue, offsetValue, distanceInnerValue, distanceOuterValue, speedInnerValue,
	    speedOuterValue, centerForceValue, ringRadiusValue, ringWidthValue, ringPullDistanceValue,
	    ringPullForceValue, audioMode, infiniteAxis, maintainDistance, ringShape, speedOverride] (
	       std::vector<ParticleInstance>& particles, uint32_t count,
	       const std::vector<ControlPointData>& controlPoints, float, float dt
	   ) {
	// Audio modulation (when implemented, this will sample from audio context)
	float audioAmplitude = 0.0f; // TODO: Sample from AudioContext when audio processing is implemented

	// If audio mode is enabled but no audio, skip vortex entirely
	if (audioMode > 0 && audioAmplitude == 0.0f) {
	    return;
	}

	glm::vec3 axis = axisValue->getVec3 ();
	glm::vec3 offset = offsetValue->getVec3 ();
	float distanceInner = distanceInnerValue->getFloat ();
	float distanceOuter = distanceOuterValue->getFloat ();
	float speedInner = speedInnerValue->getFloat ();
	float speedOuter = speedOuterValue->getFloat ();
	float centerForce = centerForceValue->getFloat ();
	float ringRadius = ringRadiusValue->getFloat ();
	float ringWidth = ringWidthValue->getFloat ();
	float ringPullDistance = ringPullDistanceValue->getFloat ();
	float ringPullForce = ringPullForceValue->getFloat ();

	// Apply audio modulation to speeds
	if (audioMode > 0) {
	    speedInner *= (1.0f + audioAmplitude);
	    speedOuter *= (1.0f + audioAmplitude);
	}

	// Get vortex center from control point
	glm::vec3 center = glm::vec3 (0.0f);
	if (controlPoint >= 0 && controlPoint < static_cast<int> (controlPoints.size ())) {
	    center = controlPoints[controlPoint].position + offset;
	} else {
	    center = offset;
	}

	// Normalize axis
	if (glm::length (axis) > 0.0f) {
	    axis = glm::normalize (axis);
	} else {
	    axis = glm::vec3 (0.0f, 0.0f, 1.0f); // Default to Z-axis
	}

	for (uint32_t i = 0; i < count; i++) {
	    auto& p = particles[i];
	    if (!p.alive) {
		continue;
	    }

	    // Calculate vector from center to particle
	    glm::vec3 toParticle = p.position - center;

	    // For infinite axis mode, project onto plane perpendicular to axis (cylinder shape)
	    // Otherwise use full 3D distance (sphere shape)
	    float axialDistance = 0.0f;
	    glm::vec3 radialVector = toParticle;
	    if (infiniteAxis) {
		// Project out the axis component
		axialDistance = glm::dot (toParticle, axis);
		radialVector = toParticle - axis * axialDistance;
	    }

	    float distance = glm::length (radialVector);

	    // Compute tangent direction (perpendicular to both axis and radial vector)
	    glm::vec3 tangent = glm::cross (axis, radialVector);
	    if (glm::length (tangent) > 0.001f) {
		tangent = glm::normalize (tangent);
	    } else {
		continue; // Particle is on the axis
	    }

	    // Calculate spin speed and apply forces based on mode
	    float speed = 0.0f;
	    glm::vec3 radialForce = glm::vec3 (0.0f);

	    if (ringShape) {
		// Ring mode: hollow center with ring-shaped influence zone
		float ringInner = ringRadius - ringWidth * 0.5f;
		float ringOuter = ringRadius + ringWidth * 0.5f;

		if (distance < ringInner) {
		    // Inside the ring's hollow center - no spin, but may be pulled outward
		    speed = 0.0f;
		} else if (distance <= ringOuter) {
		    // Inside the ring - full effect
		    float t = (distance - ringInner) / ringWidth;
		    speed = glm::mix (speedInner, speedOuter, t);
		} else if (distance <= ringOuter + ringPullDistance) {
		    // Outside ring but within pull distance - attract toward ring
		    float pullT = (distance - ringOuter) / ringPullDistance;
		    speed = speedOuter * (1.0f - pullT);
		    // Pull toward ring
		    if (distance > 0.001f) {
			glm::vec3 towardRing = -glm::normalize (radialVector);
			radialForce = towardRing * ringPullForce * pullT;
		    }
		} else {
		    // Too far from ring - no effect
		    speed = 0.0f;
		}
	    } else {
		// Standard vortex mode
		float disMid = distanceOuter - distanceInner + 0.1f;

		if (disMid < 0 || distance < distanceInner) {
		    speed = speedInner;
		} else if (distance > distanceOuter) {
		    speed = speedOuter;
		} else {
		    float t = (distance - distanceInner) / disMid;
		    speed = glm::mix (speedInner, speedOuter, t);
		}
	    }

	    // Apply tangential velocity (spinning)
	    p.velocity += tangent * speed * dt * speedOverride->getFloat ();

	    // Apply radial force (ring pull)
	    p.velocity += radialForce * dt * speedOverride->getFloat ();

	    // Apply center force when maintain distance is enabled
	    if (maintainDistance && distance > 0.001f) {
		glm::vec3 towardCenter = -glm::normalize (radialVector);
		p.velocity += towardCenter * centerForce * dt * speedOverride->getFloat ();
	    }
	}
    };
}

OperatorFunc CParticle::createControlPointAttractOperator (const ControlPointAttractOperator& op) {
    int controlPoint = op.controlPoint;
    DynamicValue* originValue = op.origin->value.get ();
    DynamicValue* scaleValue = op.scale->value.get ();
    DynamicValue* thresholdValue = op.threshold->value.get ();
    DynamicValue* speedOverride = m_particle.instanceOverride.speed->value.get ();

    return [this, controlPoint, originValue, scaleValue, thresholdValue, speedOverride] (
	       std::vector<ParticleInstance>& particles, uint32_t count,
	       const std::vector<ControlPointData>& controlPoints, float currentTime, float dt
	   ) {
	// Get dynamic values
	glm::vec3 origin = originValue->getVec3 ();
	float scale = scaleValue->getFloat ();
	float threshold = thresholdValue->getFloat () / 2.0f;

	// Get control point position
	if (controlPoint < 0 || controlPoint >= static_cast<int> (controlPoints.size ())) {
	    return;
	}

	glm::vec3 center = controlPoints[controlPoint].position + origin;

	// Apply attraction force to all particles within threshold
	for (uint32_t i = 0; i < count; i++) {
	    auto& p = particles[i];
	    if (!p.alive) {
		continue;
	    }

	    // Calculate distance and direction to control point
	    glm::vec3 toCenter = center - p.position;
	    float distance = glm::length (toCenter);

	    // Only apply force if within threshold
	    if (distance > 0.001f && distance < threshold) {
		// Normalize direction
		glm::vec3 direction = toCenter / distance;

		// Apply constant force in direction of control point
		glm::vec3 forceVec = direction * scale * dt;
		p.velocity += forceVec * speedOverride->getFloat ();
	    }
	}
    };
}

OperatorFunc CParticle::createOscillateAlphaOperator (const OscillateAlphaOperator& op) {
    DynamicValue* freqMinValue = op.frequencyMin->value.get ();
    DynamicValue* freqMaxValue = op.frequencyMax->value.get ();
    DynamicValue* scaleMinValue = op.scaleMin->value.get ();
    DynamicValue* scaleMaxValue = op.scaleMax->value.get ();
    DynamicValue* phaseMinValue = op.phaseMin->value.get ();
    DynamicValue* phaseMaxValue = op.phaseMax->value.get ();

    return
	[this, freqMinValue, freqMaxValue, scaleMinValue, scaleMaxValue, phaseMinValue, phaseMaxValue] (
	    std::vector<ParticleInstance>& particles, uint32_t count, const std::vector<ControlPointData>&, float, float
	) {
	    float freqMin = freqMinValue->getFloat ();
	    float freqMax = freqMaxValue->getFloat ();
	    float scaleMin = scaleMinValue->getFloat ();
	    float scaleMax = scaleMaxValue->getFloat ();
	    float phaseMin = phaseMinValue->getFloat ();
	    float phaseMax = phaseMaxValue->getFloat ();

	    for (uint32_t i = 0; i < count; i++) {
		auto& p = particles[i];

		// Initialize per-particle oscillator values on first use
		if (!p.oscillateAlpha.initialized) {
		    p.oscillateAlpha.frequency = randomFloat (m_rng, freqMin, freqMax);
		    p.oscillateAlpha.scale = randomFloat (m_rng, scaleMin, scaleMax);
		    p.oscillateAlpha.phase = randomFloat (m_rng, phaseMin, phaseMax + 2.0f * glm::pi<float> ());
		    p.oscillateAlpha.base = p.alpha; // Capture initial base
		    p.oscillateAlpha.initialized = true;
		}

		// Calculate oscillation: interpolate between scaleMin and scaleMax using cosine wave
		float w = p.oscillateAlpha.frequency;
		float t = p.age;
		float cosVal = (std::cos (w * t + p.oscillateAlpha.phase) + 1.0f) * 0.5f;
		float multiplier = glm::mix (scaleMin, scaleMax, cosVal);

		// Apply to base value (alphafade updates base each frame if present)
		p.alpha = p.oscillateAlpha.base * multiplier;
	    }
	};
}

OperatorFunc CParticle::createOscillateSizeOperator (const OscillateSizeOperator& op) {
    DynamicValue* freqMinValue = op.frequencyMin->value.get ();
    DynamicValue* freqMaxValue = op.frequencyMax->value.get ();
    DynamicValue* scaleMinValue = op.scaleMin->value.get ();
    DynamicValue* scaleMaxValue = op.scaleMax->value.get ();
    DynamicValue* phaseMinValue = op.phaseMin->value.get ();
    DynamicValue* phaseMaxValue = op.phaseMax->value.get ();

    return
	[this, freqMinValue, freqMaxValue, scaleMinValue, scaleMaxValue, phaseMinValue, phaseMaxValue] (
	    std::vector<ParticleInstance>& particles, uint32_t count, const std::vector<ControlPointData>&, float, float
	) {
	    float freqMin = freqMinValue->getFloat ();
	    float freqMax = freqMaxValue->getFloat ();
	    float scaleMin = scaleMinValue->getFloat ();
	    float scaleMax = scaleMaxValue->getFloat ();
	    float phaseMin = phaseMinValue->getFloat ();
	    float phaseMax = phaseMaxValue->getFloat ();

	    for (uint32_t i = 0; i < count; i++) {
		auto& p = particles[i];

		// Initialize per-particle oscillator values on first use
		if (!p.oscillateSize.initialized) {
		    p.oscillateSize.frequency = randomFloat (m_rng, freqMin, freqMax);
		    p.oscillateSize.scale = randomFloat (m_rng, scaleMin, scaleMax);
		    p.oscillateSize.phase = randomFloat (m_rng, phaseMin, phaseMax + 2.0f * glm::pi<float> ());
		    p.oscillateSize.base = p.size; // Capture initial base
		    p.oscillateSize.initialized = true;
		}

		// Calculate oscillation: interpolate between scaleMin and scaleMax using cosine wave
		float w = p.oscillateSize.frequency;
		float t = p.age;
		float cosVal = (std::cos (w * t + p.oscillateSize.phase) + 1.0f) * 0.5f;
		float multiplier = glm::mix (scaleMin, scaleMax, cosVal);

		// Apply to base value (sizeChange updates base each frame if present)
		p.size = p.oscillateSize.base * multiplier;
	    }
	};
}

OperatorFunc CParticle::createOscillatePositionOperator (const OscillatePositionOperator& op) {
    DynamicValue* freqMinValue = op.frequencyMin->value.get ();
    DynamicValue* freqMaxValue = op.frequencyMax->value.get ();
    DynamicValue* scaleMinValue = op.scaleMin->value.get ();
    DynamicValue* scaleMaxValue = op.scaleMax->value.get ();
    DynamicValue* phaseMinValue = op.phaseMin->value.get ();
    DynamicValue* phaseMaxValue = op.phaseMax->value.get ();
    DynamicValue* maskValue = op.mask->value.get ();
    DynamicValue* speedOverride = m_particle.instanceOverride.speed->value.get ();

    return [this, freqMinValue, freqMaxValue, scaleMinValue, scaleMaxValue, phaseMinValue, phaseMaxValue, maskValue,
	    speedOverride] (
	       std::vector<ParticleInstance>& particles, uint32_t count, const std::vector<ControlPointData>&, float,
	       float dt
	   ) {
	float freqMin = freqMinValue->getFloat ();
	float freqMax = freqMaxValue->getFloat ();
	float scaleMin = scaleMinValue->getFloat ();
	float scaleMax = scaleMaxValue->getFloat ();
	float phaseMin = phaseMinValue->getFloat ();
	float phaseMax = phaseMaxValue->getFloat ();
	glm::vec3 mask = maskValue->getVec3 ();

	for (uint32_t i = 0; i < count; i++) {
	    auto& p = particles[i];

	    // Initialize per-particle oscillator values on first use (per axis)
	    if (!p.oscillatePosition.initialized) {
		for (int axis = 0; axis < 3; axis++) {
		    p.oscillatePosition.frequency[axis] = randomFloat (m_rng, freqMin, freqMax);
		    p.oscillatePosition.scale[axis] = randomFloat (m_rng, scaleMin, scaleMax);
		    p.oscillatePosition.phase[axis]
			= randomFloat (m_rng, phaseMin, phaseMax + 2.0f * glm::pi<float> ());
		}
		p.oscillatePosition.initialized = true;
	    }

	    // Calculate position delta for each axis
	    float t = p.age;
	    glm::vec3 delta (0.0f);

	    for (int axis = 0; axis < 3; axis++) {
		float w = 2.0f * glm::pi<float> () * p.oscillatePosition.frequency[axis] / (2.0f * glm::pi<float> ());
		// Derivative of cos is -sin, multiply by dt for position change
		float move
		    = -p.oscillatePosition.scale[axis] * w * std::sin (w * t + p.oscillatePosition.phase[axis]) * dt;
		// Apply mask as bias multiplier for this axis
		delta[axis] = move * mask[axis] * speedOverride->getFloat ();
	    }

	    p.position += delta;
	}
    };
}

// ========== RENDERING ==========

void CParticle::setupPass () {
    if (!m_particle.material || !m_particle.material->material || m_particle.material->material->passes.empty ()) {
	sLog.error ("No valid material for particle ", m_particle.name);
	return;
    }

    const auto& firstPass = **m_particle.material->material->passes.begin ();

    // Build override with particle-specific combos
    m_passOverride = std::make_unique<ImageEffectPassOverride> ();
    m_passOverride->combos["THICKFORMAT"] = 1;
    if (m_spritesheetFrames > 0) {
	m_passOverride->combos["SPRITESHEET"] = 1;
    }
    if (m_useTrailRenderer) {
	m_passOverride->combos["TRAILRENDERER"] = 1;
    }

    // Force texture 0 to use the input (particle texture) rather than the shader's
    // default "util/white" annotation, which would override it in setupRenderTexture()
    m_passBinds = {{0, "previous"}};

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
	auto sceneFBO = getScene ().getFBO ();
	float w = static_cast<float> (sceneFBO->getRealWidth ());
	float h = static_cast<float> (sceneFBO->getRealHeight ());
	m_refractFBO = m_passFBOProvider->create (
	    "_rt_FullFrameBuffer", TextureFormat_ARGB8888, TextureFlags_ClampUVs, 1.0f, { w, h }, { w, h }
	);
    }

    // Create CPass with the WP particle shader
    m_pass = new Effects::CPass (
	*this, m_passFBOProvider, firstPass, *m_passOverride, m_passBinds, std::nullopt
    );

    // Set destination to scene FBO and input to particle texture
    m_pass->setDestination (getScene ().getFBO ());
    m_pass->setInput (getTexture ());

    // Set matrix pointers - CPass will dereference these each frame
    m_pass->setModelViewProjectionMatrix (&m_mvpMatrix);
    m_pass->setModelViewProjectionMatrixInverse (&m_mvpMatrixInverse);
    m_pass->setModelMatrix (&m_modelMatrix);
    m_pass->setViewProjectionMatrix (&m_viewProjectionMatrix);

    // Create OpenGL buffers
    GLint prevVAO = 0;
    glGetIntegerv (GL_VERTEX_ARRAY_BINDING, &prevVAO);

    glGenVertexArrays (1, &m_vao);
    glGenBuffers (1, &m_vbo);
    glGenBuffers (1, &m_ebo);

    glBindVertexArray (m_vao);
    glBindBuffer (GL_ARRAY_BUFFER, m_vbo);
    glBindBuffer (GL_ELEMENT_ARRAY_BUFFER, m_ebo);

    // WP vertex layout: a_Position(3) + a_TexCoordVec4(4) + a_Color(4) + a_TexCoordVec4C1(4) + a_TexCoordC2(2) = 17
    const GLsizei stride = sizeof (float) * 17;
    const GLuint program = m_pass->getProgramID ();

    const GLint locPosition = glGetAttribLocation (program, "a_Position");
    const GLint locTexCoordVec4 = glGetAttribLocation (program, "a_TexCoordVec4");
    const GLint locColor = glGetAttribLocation (program, "a_Color");
    const GLint locTexCoordVec4C1 = glGetAttribLocation (program, "a_TexCoordVec4C1");
    const GLint locTexCoordC2 = glGetAttribLocation (program, "a_TexCoordC2");

    if (locPosition >= 0) {
	glEnableVertexAttribArray (locPosition);
	glVertexAttribPointer (locPosition, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
    }
    if (locTexCoordVec4 >= 0) {
	glEnableVertexAttribArray (locTexCoordVec4);
	glVertexAttribPointer (locTexCoordVec4, 4, GL_FLOAT, GL_FALSE, stride, (void*)(sizeof (float) * 3));
    }
    if (locColor >= 0) {
	glEnableVertexAttribArray (locColor);
	glVertexAttribPointer (locColor, 4, GL_FLOAT, GL_FALSE, stride, (void*)(sizeof (float) * 7));
    }
    if (locTexCoordVec4C1 >= 0) {
	glEnableVertexAttribArray (locTexCoordVec4C1);
	glVertexAttribPointer (locTexCoordVec4C1, 4, GL_FLOAT, GL_FALSE, stride, (void*)(sizeof (float) * 11));
    }
    if (locTexCoordC2 >= 0) {
	glEnableVertexAttribArray (locTexCoordC2);
	glVertexAttribPointer (locTexCoordC2, 2, GL_FLOAT, GL_FALSE, stride, (void*)(sizeof (float) * 15));
    }

    glBindVertexArray (prevVAO);

    setupGeometryCallbacks ();
    setupParticleUniforms ();
}

void CParticle::setupGeometryCallbacks () {
    m_pass->setGeometryCallback (
	// Setup attribs: save current VAO, bind particle VAO
	[this] () {
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

void CParticle::updateMatrices () {
    // Build model matrix from particle object transform
    glm::vec3 scale = m_particle.scale->value->getVec3 ();
    glm::vec3 angles = m_particle.angles->value->getVec3 ();

    m_modelMatrix = glm::mat4 (1.0f);
    m_modelMatrix = glm::translate (m_modelMatrix, m_transformedOrigin);
    // Negate X and Z rotations to account for Y-flipped coordinate system
    m_modelMatrix = glm::rotate (m_modelMatrix, -angles.z, glm::vec3 (0, 0, 1));
    m_modelMatrix = glm::rotate (m_modelMatrix, angles.y, glm::vec3 (0, 1, 0));
    m_modelMatrix = glm::rotate (m_modelMatrix, -angles.x, glm::vec3 (1, 0, 0));
    m_modelMatrix = glm::scale (m_modelMatrix, scale);
    m_modelMatrixInverse = glm::inverse (m_modelMatrix);

    // Build model-view-projection matrix
    if ((m_particle.flags & 4) != 0) {
	// Perspective particles use a dedicated perspective projection
	float width = getScene ().getCamera ().getWidth ();
	float height = getScene ().getCamera ().getHeight ();
	float aspect = width / height;
	float fov = glm::radians (getScene ().getCamera ().getFov ());
	float nearz = getScene ().getCamera ().getNearZ ();
	float farz = getScene ().getCamera ().getFarZ ();

	glm::mat4 perspectiveProj = glm::perspective (fov, aspect, nearz, farz);
	glm::mat4 perspectiveView = glm::lookAt (
	    glm::vec3 (0.0f, 0.0f, 1000.0f), glm::vec3 (0.0f, 0.0f, 0.0f), glm::vec3 (0.0f, 1.0f, 0.0f)
	);

	m_viewProjectionMatrix = perspectiveProj * perspectiveView;
	m_eyePosition = glm::vec3 (0.0f, 0.0f, 1000.0f);
    } else {
	// Orthographic projection from scene camera
	m_viewProjectionMatrix = getScene ().getCamera ().getProjection () * getScene ().getCamera ().getLookAt ();
	m_eyePosition = getScene ().getCamera ().getEye ();
    }

    m_mvpMatrix = m_viewProjectionMatrix * m_modelMatrix;
    m_mvpMatrixInverse = glm::inverse (m_mvpMatrix);

    // Orientation vectors for billboard computation (screen-aligned)
    // These define the coordinate frame that common_particles.h uses for billboard orientation
    m_orientationUp = glm::vec3 (0.0f, 1.0f, 0.0f);
    m_orientationRight = glm::vec3 (1.0f, 0.0f, 0.0f);
    m_orientationForward = glm::vec3 (0.0f, 0.0f, 1.0f);
    m_viewUp = glm::vec3 (0.0f, 1.0f, 0.0f);
    m_viewRight = glm::vec3 (1.0f, 0.0f, 0.0f);

    // Update g_RenderVar0: trail parameters (.x=length, .y=maxLength, .z=minLength)
    m_renderVar0 = glm::vec4 (m_trailLength, m_trailMaxLength, m_trailMinLength, 0.0f);

    // Update g_RenderVar1: spritesheet params (.x=frameWidth, .y=frameHeight, .z=numFrames, .w=textureRatio)
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
	m_renderVar1 = glm::vec4 (
	    frameWidth, frameHeight, static_cast<float> (m_spritesheetFrames), textureRatio
	);
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

void CParticle::renderSprites () {
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

    for (uint32_t i = 0; i < m_particleCount; i++) {
	const auto& p = m_particles[i];
	if (!p.alive) {
	    continue;
	}

	// Skip particles with invalid values
	if (!std::isfinite (p.position.x) || !std::isfinite (p.position.y) || !std::isfinite (p.position.z)
	    || !std::isfinite (p.size) || p.size <= 0.0f || p.size > 10000.0f) {
	    continue;
	}

	// Compute the lifetime value for the WP shader's ComputeSpriteFrame.
	// The shader computes: floor(frac(lifetime) * numFrames) to get current frame.
	// Different animation modes need different lifetime encodings.
	float lifetime = p.getLifetimePos ();

	if (m_spritesheetFrames > 0 && m_particle.animationMode == "randomframe" && p.frame >= 0.0f) {
	    // Encode the fixed random frame so shader always picks the same one
	    lifetime = (p.frame + 0.5f) / static_cast<float> (m_spritesheetFrames);
	}

	auto addVertex = [&] (float u, float v) {
	    const uint32_t base = vertexIndex * 17;
	    // a_Position (vec3)
	    m_vertices[base + 0] = p.position.x;
	    m_vertices[base + 1] = p.position.y;
	    m_vertices[base + 2] = p.position.z;
	    // a_TexCoordVec4 (vec4: uv.x, uv.y, rotZ, size)
	    m_vertices[base + 3] = u;
	    m_vertices[base + 4] = v;
	    m_vertices[base + 5] = p.rotation.z;
	    m_vertices[base + 6] = p.size;
	    // a_Color (vec4: r, g, b, a)
	    m_vertices[base + 7] = p.color.r;
	    m_vertices[base + 8] = p.color.g;
	    m_vertices[base + 9] = p.color.b;
	    m_vertices[base + 10] = p.alpha;
	    // a_TexCoordVec4C1 (vec4: vel.x, vel.y, vel.z, lifetime)
	    m_vertices[base + 11] = p.velocity.x;
	    m_vertices[base + 12] = p.velocity.y;
	    m_vertices[base + 13] = p.velocity.z;
	    m_vertices[base + 14] = lifetime;
	    // a_TexCoordC2 (vec2: rotX, rotY)
	    m_vertices[base + 15] = p.rotation.x;
	    m_vertices[base + 16] = p.rotation.y;
	    vertexIndex++;
	};

	// 4 vertices for quad corners
	uint32_t baseVertex = vertexIndex;
	addVertex (0.0f, 1.0f); // 0: Bottom-left
	addVertex (1.0f, 1.0f); // 1: Bottom-right
	addVertex (1.0f, 0.0f); // 2: Top-right
	addVertex (0.0f, 0.0f); // 3: Top-left

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
    std::string str = "Rendering particles ";
    str += this->getParticle ().name + " (" + std::to_string (this->getId ()) + ", " + this->getParticle ().particleFile
	+ ")";
    glPushDebugGroup (GL_DEBUG_SOURCE_APPLICATION, 0, -1, str.c_str ());
#endif

    // Upload vertex and index data
    glBindBuffer (GL_ARRAY_BUFFER, m_vbo);
    glBufferData (
	GL_ARRAY_BUFFER, static_cast<GLsizeiptr> (vertexIndex * 17 * sizeof (float)), m_vertices.data (), GL_DYNAMIC_DRAW
    );

    glBindBuffer (GL_ELEMENT_ARRAY_BUFFER, m_ebo);
    glBufferData (
	GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr> (indexOffset * sizeof (uint32_t)), m_indices.data (),
	GL_DYNAMIC_DRAW
    );

    // Update matrices and uniform data
    updateMatrices ();

    // For REFRACT: blit current scene content into the copy FBO before rendering.
    // This gives the shader a snapshot of what's behind the particles for refraction,
    // without a feedback loop (rendering to scene FBO while reading from copy FBO).
    if (m_hasRefract && m_refractFBO) {
	auto sceneFBO = getScene ().getFBO ();
	GLint w = static_cast<GLint> (sceneFBO->getRealWidth ());
	GLint h = static_cast<GLint> (sceneFBO->getRealHeight ());
	glBindFramebuffer (GL_READ_FRAMEBUFFER, sceneFBO->getFramebuffer ());
	glBindFramebuffer (GL_DRAW_FRAMEBUFFER, m_refractFBO->getFramebuffer ());
	glBlitFramebuffer (0, 0, w, h, 0, 0, w, h, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    }

    // CPass::render() handles: FBO binding, texture setup, uniforms, blending, draw call, cleanup
    m_pass->render ();

#if !NDEBUG
    glPopDebugGroup ();
#endif
}
