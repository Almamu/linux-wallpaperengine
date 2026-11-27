#include "MdlAnimation.h"

#include "WallpaperEngine/Render/Utils/NoiseUtils.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>

#include <glm/gtc/matrix_transform.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/quaternion.hpp>

using namespace WallpaperEngine::Data::Model;

namespace {
glm::mat4 poseMatrix (const MdlBoneFrame& pose) {
    glm::mat4 matrix = glm::translate (glm::mat4 (1.0f), pose.translation);
    matrix *= glm::mat4_cast (pose.rotation);
    return glm::scale (matrix, pose.scale);
}

MdlBoneFrame matrixPose (const glm::mat4& matrix) {
    MdlBoneFrame pose;
    pose.translation = glm::vec3 (matrix[3]);
    pose.scale = {
	glm::length (glm::vec3 (matrix[0])),
	glm::length (glm::vec3 (matrix[1])),
	glm::length (glm::vec3 (matrix[2])),
    };

    glm::mat3 rotation (1.0f);
    for (int column = 0; column < 3; column++) {
	if (pose.scale[column] > 1e-8f) {
	    rotation[column] = glm::vec3 (matrix[column]) / pose.scale[column];
	}
    }
    if (glm::determinant (rotation) < 0.0f) {
	pose.scale.x = -pose.scale.x;
	rotation[0] = -rotation[0];
    }
    pose.rotation = glm::normalize (glm::quat_cast (rotation));
    return pose;
}

MdlBoneFrame blendPose (const MdlBoneFrame& from, const MdlBoneFrame& to, const float weight) {
    // Native uses shortest-hemisphere nlerp for adjacent poses and partial replacement layers.
    const auto toRotation = glm::dot (from.rotation, to.rotation) < 0.0f ? -to.rotation : to.rotation;
    return {
	.translation = glm::mix (from.translation, to.translation, weight),
	.rotation = glm::normalize (from.rotation * (1.0f - weight) + toRotation * weight),
	.scale = glm::mix (from.scale, to.scale, weight),
    };
}

/** Playhead position in frames, wrapped according to the clip's play mode. */
float sampleFrame (const MdlActiveAnimation& layer) {
    const auto& animation = *layer.animation;
    const float frameCount = static_cast<float> (animation.frameCount);
    if (layer.frame.has_value ()) return std::clamp (*layer.frame, 0.0f, frameCount);
    float frame = layer.time * animation.fps;

    if (animation.mode == "single") {
	frame = std::clamp (frame, 0.0f, frameCount);
    } else if (animation.mode == "mirror" && frameCount > 0.0f) {
	frame = std::fmod (frame, frameCount * 2.0f);
	if (frame < 0.0f) {
	    frame += frameCount * 2.0f;
	}
	if (frame > frameCount) {
	    frame = frameCount * 2.0f - frame;
	}
    } else if (frameCount > 0.0f) {
	frame = std::fmod (frame, frameCount);
	if (frame < 0.0f) {
	    frame += frameCount;
	}
    } else {
	frame = 0.0f;
    }

    return frame;
}

struct FrameInterpolation {
    size_t first;
    float blend;
};

FrameInterpolation sampleInterpolatedFrame (const MdlActiveAnimation& layer) {
    const float frame = sampleFrame (layer);
    const auto& clip = *layer.animation;
    // Native 140170580 truncates the seconds quotient and computes the weight
    // with a separate remainder. The quotient can round up at an authored frame.
    // A playhead clamped to the end still samples the closing frame directly.
    if (clip.fps > 0.0f && frame > 0.0f && frame < static_cast<float> (clip.frameCount)) {
	const float step = 1.0f / clip.fps;
	// Puppet layers supply folded seconds alongside their explicit frame.
	// For time-only layers retain seconds when the playhead needed no wrapping.
	const bool hasSeconds = layer.frame ? layer.time > 0.0f : frame == layer.time * clip.fps;
	const float seconds = hasSeconds ? layer.time : frame * step;
	if (step > 0.0f && std::isfinite (seconds)) {
	    const auto first = static_cast<size_t> (seconds / step);
	    return { std::min (first, static_cast<size_t> (clip.frameCount) - 1), std::fmod (seconds, step) / step };
	}
    }
    const auto first = static_cast<size_t> (frame);
    return { first, frame - static_cast<float> (first) };
}

MdlBoneFrame samplePose (const MdlActiveAnimation& layer, const std::vector<MdlBoneFrame>& frames) {
    const auto [first, blend] = sampleInterpolatedFrame (layer);
    const auto& current = frames[std::min (first, frames.size () - 1)];
    const auto& next = frames[std::min (first + 1, frames.size () - 1)];
    return blendPose (current, next, blend);
}

void composeLayer (
    MdlBoneFrame& pose, const MdlBoneFrame& reference, const MdlBoneFrame& sampled,
    const MdlActiveAnimation& layer
) {
    const float weight = std::clamp (layer.weight, 0.0f, 1.0f);
    if (layer.additive) {
	pose.translation += (sampled.translation - reference.translation) * weight;
	const glm::quat delta = glm::normalize (glm::inverse (reference.rotation) * sampled.rotation);
	pose.rotation = glm::normalize (
	    pose.rotation * glm::slerp (glm::quat (1.0f, 0.0f, 0.0f, 0.0f), delta, weight)
	);
	pose.scale += (sampled.scale - reference.scale) * weight;
    } else {
	pose = blendPose (pose, sampled, weight);
    }
}

glm::vec3 directionOr (const glm::vec3& vector, const glm::vec3& fallback) {
    const float length = glm::length (vector);
    return length > 1e-6f ? vector / length : fallback;
}

MdlBoneFrame sampleControl (
    const MdlAnimationData& data, const std::vector<MdlActiveAnimation>& layers, const size_t index
) {
    const auto& control = data.controls[index];
    const auto reference = matrixPose (control.referenceWorld.value_or (control.bindWorld));
    auto value = reference;
    for (const auto& layer : layers) {
	if (!layer.animation || layer.weight <= 0.0f || index >= layer.animation->controlFrames.size ())
	    continue;
	if (index < layer.animation->controlFlags.size () && (layer.animation->controlFlags[index] & 1))
	    continue;
	const auto& frames = layer.animation->controlFrames[index];
	if (!frames.empty ()) composeLayer (value, reference, samplePose (layer, frames), layer);
    }
    return value;
}

/** Validated rope ownership, prepared before any persistent state changes. */
// Dependency transforms use native Euler decomposition, not the pose quaternion blend.
glm::mat4 blendDependency (const glm::mat4& current, const glm::mat4& target, const float weight) {
    const auto angles = [] (const glm::mat4& value) {
        const float z = std::atan2 (value[0][1], value[0][0]);
        const float y = std::atan2 (-value[0][2], std::sqrt (value[1][2] * value[1][2] + value[2][2] * value[2][2]));
        const float x = std::atan2 (std::sin (z) * value[2][0] - std::cos (z) * value[2][1],
            std::cos (z) * value[1][1] - std::sin (z) * value[1][0]);
        return glm::vec3 (std::fmod (x, glm::two_pi<float> ()),
            std::fmod (y, glm::two_pi<float> ()), std::fmod (z, glm::two_pi<float> ()));
    };
    const auto from = angles (current);
    auto to = angles (target);
    // This is deliberately pi, and only the positive difference is adjusted.
    for (int axis = 0; axis < 3; ++axis)
        if (to[axis] - from[axis] > glm::pi<float> ()) to[axis] -= glm::pi<float> ();
    const auto mixed = from * (1.0f - weight) + to * weight;
    auto result = glm::eulerAngleZYX (mixed.z, mixed.y, mixed.x);
    for (int column = 0; column < 3; ++column)
        result[column] *= glm::length (current[column]) * (1.0f - weight) + glm::length (target[column]) * weight;
    result[3] = glm::vec4 (glm::vec3 (current[3]) * (1.0f - weight) + glm::vec3 (target[3]) * weight, 1);
    return result;
}

void prepareDependencies (
    const MdlAnimationData& data, const MdlIkGroup& group,
    std::vector<std::optional<glm::mat4>>& controls, MdlPose& pose
) {
    for (const auto index : group.controlDependencies) {
        const auto& control = data.controls[index];
        auto& current = *controls[control.bone];
        for (const auto& dependency : control.dependencies) {
            const auto& source = pose.worldBones[dependency.source];
            const float weight = pose.dependencyWeights[dependency.weight];
            if (!(dependency.flags & 0x10002)) {
                if (dependency.flags & 1) {
                    current[3] = glm::vec4 (glm::vec3 (current[3]) * (1.0f - weight) + glm::vec3 (source[3]) * weight, 1);
                } else {
                    current = blendDependency (current, source, weight);
                }
                continue;
            }
            for (const auto& node : group.nodes) for (const auto& chain : node.branches) {
                if (chain.bones.back () != control.bone) continue;
                const auto origin = glm::vec3 (source[3]);
                const auto axis = glm::vec3 (source[0]) / glm::length (source[0]);
                const auto difference = glm::vec3 (current[3]) - origin;
                float projection = glm::dot (difference, axis);
                if (!(dependency.flags & 0x10000)) {
                    // 14026dce0: closest point on the source axis to the radial line through the control.
                    const auto radial = glm::normalize (difference - projection * axis);
                    const float aa = glm::dot (radial, radial), ab = glm::dot (radial, axis), bb = glm::dot (axis, axis);
                    const float determinant = aa * bb - ab * ab;
                    if (determinant != 0.0f) {
                        auto target = current;
                        const float along = (projection * aa - glm::dot (radial, difference) * ab) / determinant;
                        target[3] = glm::vec4 (origin + axis * along, 1);
                        current = blendDependency (current, target, weight);
                    }
                    continue;
                }
                const float rootDistance = glm::length (origin - glm::vec3 (pose.worldBones[chain.bones.front ()][3]));
                if (chain.maxLength <= rootDistance || projection < 0.0f) {
                    auto target = current;
                    target[3] = glm::vec4 (origin, 1);
                    current = blendDependency (current, target, weight);
                    continue;
                }
                float remaining = std::min (chain.maxLength - rootDistance, projection);
                auto target = source;
                target[3] = glm::vec4 (origin + axis * remaining, 1);
                current = blendDependency (current, target, weight);
                for (size_t bone = chain.bones.size () - 2; bone > 0; --bone) {
                    const float length = data.boneLengths[chain.bones[bone + 1]];
                    projection -= length;
                    if (projection < 0.0f) break;
                    const float distance = std::max (projection - dependency.start, 0.0f);
                    float influence = 1.0f;
                    if (distance / dependency.range < 1.0f) {
                        influence = distance / dependency.range;
                        if (influence <= 0.0f) break;
                    }
                    remaining -= length;
                    if (remaining < 0.0f) break;
                    target = source;
                    target[3] = glm::vec4 (glm::vec3 (source[3]) + axis * remaining, 1);
                    auto& working = pose.worldBones[chain.bones[bone]];
                    working = blendDependency (working, target, influence * weight);
                }
            }
        }
    }
}

void constrainDependencies (const MdlAnimationData& data, const MdlIkGroup& group, MdlPose& pose) {
    for (const auto index : group.controlDependencies) {
        const auto& control = data.controls[index];
        for (const auto& dependency : control.dependencies) {
            if (!(dependency.flags & 0x10000)) continue;
            const auto& source = pose.worldBones[dependency.source];
            for (const auto& node : group.nodes) for (const auto& chain : node.branches) {
                if (chain.bones.back () != control.bone) continue;
                const auto axis = glm::vec3 (source[0]) / glm::length (source[0]);
                const float initialDistance = glm::length (glm::vec3 (source[3] - pose.worldBones[chain.bones.front ()][3]));
                float remaining = initialDistance;
                size_t index = 1;
                for (int budget = 32; budget > 0 && index + 1 < chain.bones.size (); --budget, ++index) {
                    remaining -= data.boneLengths[chain.bones[index]];
                    if (remaining < 0.0f) break;
                    const auto parent = glm::vec3 (pose.worldBones[chain.bones[index - 1]][3]);
                    const auto span = glm::vec3 (pose.worldBones[chain.bones[index + 1]][3]) - parent;
                    auto normal = glm::cross (glm::cross (span, axis), span);
                    if (glm::dot (normal, normal) < 0.01f) continue;
                    normal = glm::normalize (normal);
                    auto& current = pose.worldBones[chain.bones[index]][3];
                    const float side = glm::dot (normal, glm::vec3 (current) - parent);
                    if (side > std::numeric_limits<float>::epsilon ()) {
                        current -= glm::vec4 (normal * (side + side), 0);
                        // Native resets to1 before the loop increment, so the next bone index is2.
                        index = 1;
                        remaining = initialDistance;
                    }
                }
            }
        }
    }
}

struct RopePlan {
    std::vector<const MdlIkGroup*> owners;
    std::vector<bool> solved;
    std::vector<std::optional<glm::mat4>> controls;
    std::vector<std::optional<glm::mat4>> poles;
    std::vector<glm::mat4> bindWorld;
    bool legacyWholeSkeleton = false;
};

std::optional<RopePlan> prepareRopes (
    const MdlAnimationData& data, const std::vector<MdlActiveAnimation>& layers, const MdlPhysicsStep& physics
) {
    if (!physics.rope || data.ikGroups.empty ()) {
	return std::nullopt;
    }
    const float determinant = glm::determinant (glm::mat3 (physics.model));
    if (determinant == 0.0f || !std::isfinite (determinant)) {
	return std::nullopt;
    }
    RopePlan plan {
	.owners = std::vector<const MdlIkGroup*> (data.bones.size (), nullptr),
	.solved = std::vector<bool> (data.bones.size (), false),
	.controls = std::vector<std::optional<glm::mat4>> (data.bones.size ()),
	.poles = std::vector<std::optional<glm::mat4>> (data.bones.size ()),
    };
    std::vector<int32_t> controlByBone (data.bones.size (), -1);
    std::vector<int32_t> poleByBone (data.bones.size (), -1);
    std::vector<bool> usedControls (data.controls.size (), false);
    for (size_t index = 0; index < data.controls.size (); ++index) {
	const auto& control = data.controls[index];
	if (control.type > 1 || control.bone >= data.bones.size ()) {
	    return std::nullopt;
	}
	auto& byBone = control.type == 0 ? controlByBone : poleByBone;
	if (byBone[control.bone] != -1) {
	    return std::nullopt;
	}
	byBone[control.bone] = static_cast<int32_t> (index);
    }
    const auto validDependencies = [&] (const auto& entries) {
        for (const auto& entry : entries) for (const auto& dependency : entry.dependencies)
            if (dependency.source >= data.bones.size () || dependency.weight >= data.dependencyCount) return false;
        return true;
    };
    if (!validDependencies (data.bones) || !validDependencies (data.controls)) return std::nullopt;
    std::vector<bool> childEdges (data.bones.size (), false);
    bool hasRope = false;
    for (const auto& group : data.ikGroups) {
	if (group.nodes.empty () || group.root >= data.bones.size ()
	    || plan.owners[group.root]) {
	    return std::nullopt;
	}
        for (const auto control : group.controlDependencies) {
            if (control >= data.controls.size () || data.controls[control].type != 0) return std::nullopt;
        }
	plan.owners[group.root] = &group;
	std::vector<bool> tips (data.bones.size (), false);
	std::vector<bool> nodeRoots (data.bones.size (), false);
	for (const auto& node : group.nodes) {
	    if (node.root >= data.bones.size () || nodeRoots[node.root] || node.branches.empty ()) {
		return std::nullopt;
	    }
	    nodeRoots[node.root] = true;
	    for (const auto& chain : node.branches) {
		if ((chain.flags & ~7u)
		    || chain.bones.size () < 2 || chain.bones.front () != node.root || chain.maxLength <= 0.0f) {
		    return std::nullopt;
		}
		for (size_t index = 0; index < chain.bones.size (); ++index) {
		    const auto bone = chain.bones[index];
		    if (bone >= data.bones.size () || bone >= data.boneLengths.size ()
			|| bone >= data.boneDirections.size ()) {
			return std::nullopt;
		    }
		    if (index
			&& (bone == node.root || childEdges[bone]
			    || data.bones[bone].parent != static_cast<int32_t> (chain.bones[index - 1])
			    || data.boneLengths[bone] <= 1e-6f
			    || !data.boneDirections[chain.bones[index - 1]].contains (bone))) {
			return std::nullopt;
		    }
		    if (index) {
			childEdges[bone] = true;
		    }
		    plan.solved[bone] = true;
		}
		const auto tip = chain.bones.back ();
		tips[tip] = true;
		if (chain.flags & 4) {
		    hasRope = true;
		    if (!data.bones[tip].rope) {
			return std::nullopt;
		    }
		}
		if (poleByBone[tip] >= 0) {
		    usedControls[poleByBone[tip]] = true;
		}
		if (chain.flags & 1) {
		    if (controlByBone[tip] < 0) {
			return std::nullopt;
		    }
		    usedControls[controlByBone[tip]] = true;
		}
	    }
	}
	if (!nodeRoots[group.root]) {
	    return std::nullopt;
	}
	for (const auto& node : group.nodes) {
	    if (node.root != group.root && !tips[node.root]) {
		return std::nullopt;
	    }
	}
    }
    // Native prepares and solves dependency owners even when all branches are ordinary IK.
    if ((!hasRope && data.dependencyCount == 0)
        || std::find (usedControls.begin (), usedControls.end (), false) != usedControls.end ()) {
	return std::nullopt;
    }
    const auto& first = data.ikGroups.front ();
    const bool wholeSingle = data.ikGroups.size () == 1 && first.nodes.size () == 1
	&& first.nodes.front ().branches.size () == 1
	&& first.nodes.front ().branches.front ().bones.size () == data.bones.size ();
    for (size_t bone = 0; bone < data.bones.size (); ++bone) {
	const auto& entry = data.bones[bone];
	const bool ordered = entry.parent < 0 || static_cast<size_t> (entry.parent) < bone;
	if (hasRope && wholeSingle && (!ordered || (!plan.owners[bone] && !(entry.type & 2)))) {
	    plan.legacyWholeSkeleton = true;
	} else if (!ordered || (plan.solved[bone] && !plan.owners[bone] && !(entry.type & 2))) {
	    return std::nullopt;
	}
	if (!plan.solved[bone]
	    && (entry.type != 0 || (!entry.spring && (entry.rope || entry.ikAngleLimits || entry.ikAimToTarget
		|| entry.ikFollowEnd)))) {
	    return std::nullopt;
	}
    }
    // Keep the compatibility route for synthetic, unordered skeletons unchanged.
    if (plan.legacyWholeSkeleton
        && std::any_of (data.bones.begin (), data.bones.end (), [] (const auto& bone) { return bone.spring.has_value (); }))
        return std::nullopt;
    plan.bindWorld.resize (data.bones.size ());
    for (size_t bone = 0; bone < data.bones.size (); ++bone) {
	const auto& entry = data.bones[bone];
	plan.bindWorld[bone] = plan.legacyWholeSkeleton
	    ? glm::inverse (entry.inverseBindWorld)
	    : (entry.parent >= 0 ? plan.bindWorld[entry.parent] : glm::mat4 (1.0f)) * entry.bindLocal;
	if (controlByBone[bone] >= 0) {
	    plan.controls[bone] = poseMatrix (sampleControl (data, layers, controlByBone[bone]));
	}
	if (poleByBone[bone] >= 0) {
	    plan.poles[bone] = poseMatrix (sampleControl (data, layers, poleByBone[bone]));
	}
    }
    return plan;
}

/** Native rope prediction and shared rope/ordinary-IK group solve. */
void solveRopeGroup (
    const MdlAnimationData& data, const MdlIkGroup& group, RopePlan& plan, MdlPhysicsStep& physics,
    const glm::mat4& toLocal, MdlPose& pose
) {
    // Native solver normalizes with one reciprocal, then multiplies each component.
    const auto directionOr = [] (const glm::vec3& value, const glm::vec3& fallback) {
        const float length = glm::length (value);
        return length > 1e-6f ? value * (1.0f / length) : fallback;
    };
    prepareDependencies (data, group, plan.controls, pose);
    auto& points = physics.state.ropePoints;
    const auto worldPoint = [&] (const glm::vec3& point) { return glm::vec3 (physics.model * glm::vec4 (point, 1)); };
    const auto localPoint = [&] (const glm::vec3& point) { return glm::vec3 (toLocal * glm::vec4 (point, 1)); };
    // Predictor reverses nodes but visits each node's branches forward.
    for (size_t nodeIndex = group.nodes.size (); nodeIndex-- > 0;) {
	for (const auto& chain : group.nodes[nodeIndex].branches) {
	    if (!(chain.flags & 4)) {
		continue;
	    }
	    const auto tip = chain.bones.back ();
	    const auto& settings = *data.bones[tip].rope;
	    const auto& endControl = plan.controls[tip];
	    const auto root = chain.bones.front ();
	    const size_t end = chain.bones.size () - (data.bones[tip].ikFollowEnd ? 0 : 1);
	    points[root].current = worldPoint (glm::vec3 (pose.worldBones[root][3]));
	    if (!data.bones[tip].ikFollowEnd) {
		const glm::vec3 origin (pose.worldBones[root][3]);
		glm::vec3 target (endControl ? (*endControl)[3] : pose.worldBones[tip][3]);
		const auto delta = target - origin;
		const float length = glm::length (delta);
		if (!settings.stretch && length > chain.maxLength) {
		    target = origin + delta / length * chain.maxLength;
		}
		pose.worldBones[tip][3] = glm::vec4 (target, 1);
		points[tip].current = worldPoint (target);
	    }
	    if (!points[root].initialized) {
		points[root].initialized = true;
		for (size_t index = 1; index < end; ++index) {
		    auto& point = points[chain.bones[index]];
		    point.current = point.previous = worldPoint (glm::vec3 (pose.worldBones[chain.bones[index]][3]));
		}
	    }
	    const float dtSquared = physics.rope->deltaTime * physics.rope->deltaTime;
	    const auto acceleration = settings.gravity ? settings.mass * physics.rope->gravity : glm::vec3 (0);
	    for (size_t index = 1; index < end; ++index) {
		const auto bone = chain.bones[index];
		auto& point = points[bone];
		auto predicted = point.current + point.current - point.previous;
		const auto velocity = point.previous - point.current;
		if (dtSquared > std::numeric_limits<float>::epsilon () && settings.friction > 0.01f
		    && glm::dot (velocity, velocity) > 0.0001f) {
		    predicted += velocity * dtSquared * (1.0f / dtSquared) * settings.friction;
		}
		auto force = acceleration;
		if (physics.rope->windStrength != 0.0f) {
		    const float phase = (predicted.x + physics.rope->windOriginX) / physics.rope->windWidth * 10.0f;
		    const float noise = std::max (
			WallpaperEngine::Render::Utils::simplexNoise2D (physics.rope->windTime, phase), 0.0f
		    );
		    force += noise * physics.rope->windDirection * physics.rope->windStrength;
		}
		predicted += force * dtSquared;
		point.previous = point.current;
		point.current = predicted;
		pose.worldBones[bone][3] = glm::vec4 (localPoint (predicted), 1);
	    }
	}
    }
    // Rope constraints only change persistent points, not this frame's working translations.
    const auto constrainRope = [&] (const MdlIkChain& chain, const bool forward) {
	const auto root = chain.bones.front ();
	const auto tip = chain.bones.back ();
	const auto& settings = *data.bones[tip].rope;
	const size_t end = chain.bones.size () - (data.bones[tip].ikFollowEnd ? 0 : 1);
	const float distance = glm::length (points[tip].current - points[root].current);
	const float stretch = settings.stretch && distance > chain.maxLength ? distance / chain.maxLength : 1.0f;
	for (size_t index = 1; index < end; ++index) {
	    const size_t childIndex = forward ? index : chain.bones.size () - index;
	    const auto child = chain.bones[childIndex];
	    const auto parent = chain.bones[childIndex - 1];
	    auto childPoint = localPoint (points[child].current);
	    auto parentPoint = localPoint (points[parent].current);
	    const auto delta = childPoint - parentPoint;
	    const float length = glm::length (delta);
	    const float error = data.boneLengths[child] * stretch - length;
	    if (std::abs (error) <= 0.001f) {
		continue;
	    }
	    const auto direction = length <= 0.01f ? glm::vec3 (0.707f, 0.707f, 0) : delta / length;
	    if (index == 1) {
		if (forward) {
		    childPoint += direction * error;
		} else {
		    parentPoint -= direction * error;
		}
	    } else {
		childPoint += direction * (error * 0.5f);
		parentPoint -= direction * (error * 0.5f);
	    }
	    if (index != 1 || forward) {
		points[child].current = worldPoint (childPoint);
	    }
	    if (index != 1 || !forward) {
		points[parent].current = worldPoint (parentPoint);
	    }
	}
    };
    const glm::vec3 anchor (pose.worldBones[group.root][3]);
    const auto position = [&] (const uint32_t bone) { return glm::vec3 (pose.worldBones[bone][3]); };
    const auto setPosition
	= [&] (const uint32_t bone, const glm::vec3& value) { pose.worldBones[bone][3] = glm::vec4 (value, 1.0f); };
    // Native 14026df30 limits the projected XY direction in the preceding segment's frame.
    const auto limitDirection = [&] (const uint32_t bone, glm::vec3 direction) {
	const auto& settings = data.bones[bone];
	const auto preceding = directionOr (position (bone) - position (settings.parent), glm::vec3 (1, 0, 0));
	const auto basis = glm::mat3_cast (glm::rotation (glm::vec3 (1, 0, 0), preceding));
	const float projectedLength = glm::length (glm::vec2 (direction));
	if (projectedLength == 0.0f) {
	    return direction;
	}
	const auto local = glm::inverse (basis) * glm::vec3 (direction.x, direction.y, 0) / projectedLength;
	const float angle = std::atan2 (local.y, local.x);
	float correction = angle < settings.ikMinAngle ? settings.ikMinAngle - angle : 0.0f;
	if (angle > settings.ikMaxAngle
	    && (correction == 0.0f || std::abs (settings.ikMaxAngle - angle) < std::abs (correction))) {
	    correction = settings.ikMaxAngle - angle;
	}
	if (correction != 0.0f) {
	    const auto adjusted = basis * glm::vec3 (
		glm::rotate (glm::mat4 (1), correction, glm::vec3 (0, 0, 1)) * glm::vec4 (local, 0)
	    );
	    direction.x = adjusted.x * projectedLength;
	    direction.y = adjusted.y * projectedLength;
	}
	return direction;
    };
    for (int iteration = 0; iteration < 10; ++iteration) {
	for (size_t nodeIndex = group.nodes.size (); nodeIndex-- > 0;) {
	    const auto& node = group.nodes[nodeIndex];
	    glm::vec3 sum (0.0f);
	    float count = 0.0f;
	    for (size_t branchIndex = node.branches.size (); branchIndex-- > 0;) {
		const auto& chain = node.branches[branchIndex];
		if (chain.flags & 4) {
		    constrainRope (chain, false);
		    continue;
		}
		const auto tip = chain.bones.back ();
		auto target = chain.flags & 1 ? glm::vec3 ((*plan.controls[tip])[3]) : position (tip);
		const auto delta = target - position (node.root);
		const auto axis = directionOr (delta, glm::vec3 (1.0f, 0.0f, 0.0f));
		float distance = glm::length (delta);
		if (chain.minLength > 0.0f && distance < chain.minLength) {
		    target += axis * (chain.minLength - distance);
		    distance = chain.minLength;
		}
		glm::vec3 pole (0.0f);
		if (plan.poles[tip]) {
		    pole = glm::vec3 ((*plan.poles[tip])[3]) - target;
		    pole -= axis * glm::dot (pole, axis);
		}
		const auto normal = glm::cross (axis, pole);
		const bool reachable = distance < chain.maxLength || (chain.flags & 2);
		setPosition (tip, target);
		float accumulatedLength = 0.0f;
		for (size_t index = chain.bones.size () - 1; index > 0; --index) {
		    const auto child = chain.bones[index];
		    const auto parent = chain.bones[index - 1];
		    glm::vec3 direction = -axis;
		    if (reachable) {
			direction = position (parent) - position (child);
			if (index > 1 && data.bones[parent].ikAngleLimits) {
			    direction = -limitDirection (parent, -directionOr (direction, -axis));
			}
			if (index == chain.bones.size () - 1
			    || accumulatedLength + data.boneLengths[child] < chain.maxLength * 0.5f) {
			    if (glm::dot (pole, pole) > 0.01f) {
				const auto plane = glm::normalize (normal);
				direction -= plane * glm::dot (direction, plane);
			    }
			    const float side = glm::dot (direction, pole);
			    if (side < 0.0f) {
				direction -= 2.0f * pole * side;
			    }
			}
			direction = directionOr (direction, -axis);
		    }
		    setPosition (parent, position (child) + direction * data.boneLengths[child]);
		    accumulatedLength += data.boneLengths[child];
		}
		sum += position (node.root);
		count += 1.0f;
	    }
	    if (count > 0.0f) {
		setPosition (node.root, sum / count);
	    }
	}
        if (iteration == 0) constrainDependencies (data, group, pose);
	bool converged = true;
	for (const auto& node : group.nodes) {
	    for (const auto& chain : node.branches) {
		if (chain.flags & 4) {
		    constrainRope (chain, true);
		    continue;
		}
		if (node.root == group.root && data.bones[group.root].parent >= 0) {
		    setPosition (node.root, anchor);
		}
		for (size_t index = 1; index < chain.bones.size (); ++index) {
		    const auto child = chain.bones[index];
		    const auto parent = chain.bones[index - 1];
		    const auto fallback = data.boneDirections[parent].at (child);
		    auto direction = directionOr (position (child) - position (parent), fallback);
		    if (index > 1 && data.bones[parent].ikAngleLimits) {
			direction = limitDirection (parent, direction);
		    }
		    setPosition (child, position (parent) + direction * data.boneLengths[child]);
		}
		if (chain.flags & 1) {
		    const auto tip = chain.bones.back ();
		    const auto error = position (tip) - glm::vec3 ((*plan.controls[tip])[3]);
		    if (error.y * error.y + error.x * error.x + error.z * error.z > 0.1f) {
			converged = false;
		    }
		}
	    }
	}
	if (converged) {
	    break;
	}
    }
    for (const auto& node : group.nodes) {
	for (const auto& chain : node.branches) {
	    const auto tip = chain.bones.back ();
	    const auto& endControl = plan.controls[tip];
	    for (size_t index = node.branches.size () > 1 ? 1 : 0; index + 1 < chain.bones.size (); ++index) {
		const auto bone = chain.bones[index];
		const auto child = chain.bones[index + 1];
		const auto reference = data.boneDirections[bone].at (child);
		const auto position = pose.worldBones[bone][3];
		const auto direction = directionOr (glm::vec3 (pose.worldBones[child][3] - position), reference);
		const auto rotation = glm::rotation (reference, direction);
		pose.worldBones[bone] = plan.bindWorld[bone] * glm::mat4_cast (rotation);
		pose.worldBones[bone][3] = position;
	    }
	    if ((chain.flags & 1) && data.bones[tip].ikFollowEnd) {
		if (chain.bones.size () > 2) {
		    const auto parent = chain.bones[chain.bones.size () - 2];
		    for (int column = 0; column < 3; ++column) {
			pose.worldBones[tip][column] = pose.worldBones[parent][column];
		    }
		}
	    } else if (chain.flags & 1) {
		// Native 140214eb0 normalizes each control column with one inverse-square-root
		// iteration; even a unit column retains its characteristic 0.998308 length.
		for (int column = 0; column < 3; ++column) {
		    const auto value = (*endControl)[column];
		    const float squared = value.w * value.w + value.z * value.z + value.y * value.y + value.x * value.x;
		    float inverse = std::bit_cast<float> (0x5f375a86u - (std::bit_cast<uint32_t> (squared) >> 1));
		    inverse = (1.5f - squared * 0.5f * inverse * inverse) * inverse;
		    pose.worldBones[tip][column] = value * inverse;
		}
		if (data.bones[tip].ikAimToTarget) {
		    const auto delta = glm::vec3 ((*endControl)[3] - pose.worldBones[tip][3]);
		    const float squared = glm::dot (delta, delta);
		    if (squared > 1.0f) {
			const float distance = std::sqrt (squared);
			const float weight = std::min ((distance - 1.0f) / data.bones[tip].ikAimDistance, 1.0f);
			const auto aim = glm::rotation (glm::vec3 (1, 0, 0), delta / distance);
			const auto control = glm::quat_cast (glm::mat3 (pose.worldBones[tip]));
			const auto basis = glm::mat4_cast (glm::slerp (control, aim, weight));
			for (int column = 0; column < 3; ++column) {
			    pose.worldBones[tip][column] = basis[column];
			}
		    }
		}
	    }
	}
	if (node.branches.size () > 1) {
	    // Native folds each branch forward from identity with constant 1/N, without normalization.
	    glm::quat rotation (1.0f, 0.0f, 0.0f, 0.0f);
	    const auto position = pose.worldBones[node.root][3];
	    for (size_t index = 0; index < node.branches.size (); ++index) {
		const auto child = node.branches[index].bones[1];
		const auto reference = data.boneDirections[node.root].at (child);
		const auto direction = directionOr (glm::vec3 (pose.worldBones[child][3] - position), reference);
		const auto branch = glm::rotation (reference, direction);
		rotation = glm::slerp (rotation, branch, 1.0f / static_cast<float> (node.branches.size ()));
	    }
	    pose.worldBones[node.root] = plan.bindWorld[node.root] * glm::mat4_cast (rotation);
	    pose.worldBones[node.root][3] = position;
	}
    }
}

/** Fixed-length, independent IK chains, as evaluated by the native 140271910 solver. */
void applyBoneControls (
    const MdlAnimationData& data, const std::vector<MdlActiveAnimation>& layers,
    const std::vector<glm::mat4>& locals, MdlPose& pose
) {
    std::vector<MdlBoneFrame> controls;
    for (size_t index = 0; index < data.controls.size (); ++index)
	controls.push_back (sampleControl (data, layers, index));
    const auto findControl = [&] (const uint32_t bone, const uint32_t type) -> const MdlBoneFrame* {
	for (size_t index = 0; index < data.controls.size (); ++index)
	    if (data.controls[index].bone == bone && data.controls[index].type == type) return &controls[index];
	return nullptr;
    };
    std::vector<bool> solved (data.bones.size (), false);
    for (const auto& group : data.ikGroups) {
	// Coupled branches, scalar dependencies, joint limits and physics need their
	// own constraint pass. Do not approximate those rigs with an independent chain.
	if (!group.controlDependencies.empty () || group.nodes.size () != 1 || group.nodes.front ().branches.size () != 1)
	    continue;
	const auto& chain = group.nodes.front ().branches.front ();
	if (chain.bones.size () < 2 || chain.flags != 1 || chain.bones.front () != group.root) continue;
	const auto tip = chain.bones.back ();
	const auto* target = findControl (tip, 0);
	if (!target) continue;
	bool valid = true;
	for (size_t index = 0; index < chain.bones.size (); ++index) {
	    const auto bone = chain.bones[index];
	    if (bone >= data.bones.size () || bone >= data.boneLengths.size ()
		|| bone >= data.boneDirections.size () || data.bones[bone].ikAngleLimits) {
		valid = false;
		break;
	    }
	    if (index && (data.bones[bone].parent != static_cast<int32_t> (chain.bones[index - 1])
		|| data.boneLengths[bone] <= 1e-6f
		|| !data.boneDirections[chain.bones[index - 1]].contains (bone))) valid = false;
	}
	if (!valid) continue;
	std::vector<glm::vec3> points;
	for (const auto bone : chain.bones) points.emplace_back (pose.worldBones[bone][3]);
	const auto root = points.front ();
	const auto* pole = findControl (tip, 1);
	const size_t last = points.size () - 1;
	for (int iteration = 0; iteration < 10; ++iteration) {
	    glm::vec3 end = target->translation;
	    const auto axis = directionOr (end - points.front (), glm::vec3 (1, 0, 0));
	    const float distance = glm::length (end - points.front ());
	    if (distance < chain.minLength) end += axis * (chain.minLength - distance);
	    glm::vec3 poleOffset = pole ? pole->translation - end : glm::vec3 (0.0f);
	    poleOffset -= axis * glm::dot (poleOffset, axis);
	    const auto normal = directionOr (glm::cross (axis, poleOffset), glm::vec3 (0.0f));
	    points.back () = end;
	    float travelled = 0.0f;
	    for (size_t index = last; index > 0; --index) {
		glm::vec3 direction = points[index - 1] - points[index];
		const float length = data.boneLengths[chain.bones[index]];
		if (distance >= chain.maxLength) {
		    direction = -axis;
		} else if (index == last || travelled + length < chain.maxLength * 0.5f) {
		    if (glm::dot (poleOffset, poleOffset) > 0.01f) direction -= normal * glm::dot (direction, normal);
		    const float side = glm::dot (direction, poleOffset);
		    if (side < 0.0f) direction -= 2.0f * poleOffset * side;
		}
		points[index - 1] = points[index] + directionOr (direction, -axis) * length;
		travelled += length;
	    }
	    // A root attached to another bone stays pinned; a top-level chain root
	    // follows its end control. Pinning every root stretches Miss Fortune's arm.
	    if (data.bones[group.root].parent >= 0) points.front () = root;
	    for (size_t index = 1; index <= last; ++index) {
		points[index] = points[index - 1]
		    + directionOr (points[index] - points[index - 1], axis) * data.boneLengths[chain.bones[index]];
	    }
	    const auto error = points.back () - target->translation;
	    if (glm::dot (error, error) <= 0.1f) break;
	}
	for (size_t index = 0; index < last; ++index) {
	    const auto bone = chain.bones[index];
	    const auto reference = data.boneDirections[bone].at (chain.bones[index + 1]);
	    const auto direction = directionOr (points[index + 1] - points[index], reference);
	    const auto rotation = glm::rotation (reference, direction);
	    pose.worldBones[bone] = glm::mat4_cast (rotation) * glm::inverse (data.bones[bone].inverseBindWorld);
	}
	const auto& endBone = data.bones[tip];
	if (!endBone.ikFollowEnd) {
	    auto rotation = target->rotation;
	    const auto error = target->translation - points.back ();
	    const float distance = glm::length (error);
	    if (endBone.ikAimToTarget && distance > 1.0f) {
		rotation = glm::slerp (rotation, glm::rotation (glm::vec3 (1, 0, 0), error / distance),
		    std::clamp ((distance - 1.0f) / endBone.ikAimDistance, 0.0f, 1.0f));
	    }
	    pose.worldBones[tip] = glm::mat4_cast (glm::normalize (rotation));
	} else if (last > 1) {
	    pose.worldBones[tip] = pose.worldBones[chain.bones[last - 1]];
	}
	for (size_t index = 0; index <= last; ++index) {
	    const auto bone = chain.bones[index];
	    pose.worldBones[bone][3] = glm::vec4 (points[index], 1.0f);
	    solved[bone] = true;
	}
	// Recompose descendants so the gun and its attachment follow the same wrist.
	for (size_t bone = 0; bone < data.bones.size (); ++bone) {
	    if (solved[bone]) continue;
	    const auto parent = data.bones[bone].parent;
	    pose.worldBones[bone] = parent >= 0 ? pose.worldBones[parent] * locals[bone] : locals[bone];
	}
    }
    for (size_t bone = 0; bone < data.bones.size (); ++bone)
	pose.skinBones[bone] = pose.worldBones[bone] * data.bones[bone].inverseBindWorld;
}

float wrapAngle (const float angle) {
    constexpr float pi = glm::pi<float> ();
    return angle >= 0.0f ? std::fmod (angle + pi, 2.0f * pi) - pi : std::fmod (angle - pi, 2.0f * pi) + pi;
}

/**
 * One spring/rigid bone step from native 1401fdf90, returning the local physics transform that
 * follows the animated bone. `animatedWorld` already carries the parent's physics.
 */
glm::mat4 stepSpring (
    const MdlBoneSpring& spring, MdlPhysicsStep& physics, MdlPhysicsState::Bone& state, const glm::mat4& previous,
    const glm::mat4& animatedWorld
) {
    const glm::quat identity (1.0f, 0.0f, 0.0f, 0.0f);
    const float dt = physics.deltaTime;
    const float scale = (glm::length (glm::vec3 (physics.model[0])) + glm::length (glm::vec3 (physics.model[1]))
			 + glm::length (glm::vec3 (physics.model[2]))) / 3.0f;
    const glm::mat4 current = physics.model * animatedWorld;
    const glm::vec3 position (current[3]);
    const float determinant = glm::determinant (glm::mat3 (current));
    if (determinant == 0.0f || !std::isfinite (determinant)) {
	state = {};
	return glm::mat4 (1.0f);
    }
    const glm::mat3 toLocal = glm::inverse (glm::mat3 (current));

    // Where the tip was last frame, and where the carried physics puts it now, in bone space.
    const glm::vec3 lagging = toLocal * (glm::vec3 (previous * glm::vec4 (spring.tip, 1.0f)) - position);
    glm::vec3 tip = glm::mat3_cast (glm::quat (state.angles)) * spring.tip + toLocal * state.offset;
    if (spring.gravity) {
	const glm::vec3 axis = directionOr (tip, glm::vec3 (1.0f, 0.0f, 0.0f));
	const glm::vec3 pull = toLocal * spring.gravityDirection * (spring.mass * scale);
	tip -= pull - glm::dot (axis, pull) * axis;
	if (spring.translation) state.velocity += spring.gravityDirection * (spring.mass * dt * 1000.0f);
    }
    // Native caps the tip's travel at 900 units per second.
    const float drag = std::min (glm::length (tip - lagging), 900.0f * dt);
    const glm::quat follow = glm::rotation (
	directionOr (tip, glm::vec3 (1.0f, 0.0f, 0.0f)), directionOr (lagging, glm::vec3 (1.0f, 0.0f, 0.0f))
    );

    if (spring.rotation) {
	const float degree = glm::radians (1.0f);
	auto& velocity = state.angularVelocity;
	velocity *= glm::slerp (identity, follow, std::min (drag * spring.rotationInertia * degree, 1.0f));
	if (!spring.rigid)
	    velocity *= glm::slerp (identity, glm::quat (-state.angles), std::min (spring.rotationStiffness * degree * dt, 1.0f));
	if (spring.limitTorque) {
	    // Native limits the three quaternion ratios, not Euler angles or total rotation.
	    const glm::vec3 ratios = glm::vec3 (velocity.x, velocity.y, velocity.z) / velocity.w;
	    glm::vec3 limited;
	    for (int axis = 0; axis < 3; ++axis) {
		const float angle = glm::degrees (2.0f * std::atan (ratios[axis]));
		const float capped = std::max (-spring.maxTorque, std::min (spring.maxTorque, angle));
		limited[axis] = std::tan (glm::radians (capped) * 0.5f);
	    }
	    velocity = glm::normalize (glm::quat (1.0f, limited.x, limited.y, limited.z));
	}
	// The velocity is one 60 Hz step; integrate it on top of the carried orientation.
	const glm::quat step = glm::slerp (identity, velocity, std::min (dt * 60.0f, 1.0f));
	glm::vec3 angles;
	glm::extractEulerAngleZYX (
	    glm::mat4 (glm::mat3_cast (step) * glm::mat3_cast (glm::quat (state.angles))), angles.z, angles.y, angles.x
	);
	if (!spring.rigid)
	    for (int axis = 0; axis < 3; axis++) angles[axis] = spring.rotationAxes[axis] ? wrapAngle (angles[axis]) : 0.0f;
	if (spring.limitAngles) {
	    const glm::vec3 limited = glm::clamp (angles, spring.minAngles, spring.maxAngles);
	    // A clamped rotation also stops the velocity pushing against the limit.
	    velocity = glm::conjugate (glm::quat (angles - limited)) * velocity;
	    angles = limited;
	}
	if (spring.rigid)
	    for (int axis = 0; axis < 3; axis++) if (!spring.rotationAxes[axis]) angles[axis] = 0.0f;
	state.angles = angles;
	velocity = glm::slerp (velocity, identity, std::min (dt * spring.rotationFriction, 1.0f));
    }

    if (spring.translation) {
	auto& offset = state.offset;
	offset -= (position + offset - glm::vec3 (previous[3])) * spring.translationInertia;
	if (!spring.rigid) state.velocity -= dt * spring.translationStiffness * offset;
	offset += dt * state.velocity;
	for (int axis = 0; axis < 3; axis++) {
	    const glm::vec3 direction (current[axis]);
	    if (!spring.translationAxes[axis]) offset -= glm::dot (offset, direction) / glm::dot (direction, direction) * direction;
	}
	const float limit = scale * spring.maxDistance;
	if (limit > 0.0f && glm::length (offset) > limit) offset *= limit / glm::length (offset);
	state.velocity -= std::min (dt * spring.translationFriction, 1.0f) * state.velocity;
    }

    return glm::translate (glm::mat4 (1.0f), toLocal * state.offset) * glm::mat4_cast (glm::quat (state.angles));
}

/** Same playhead and interpolation as the bones, on a scalar track instead of a pose. */
float sampleScalarTrack (const MdlActiveAnimation& layer, const std::vector<float>& track) {
    const auto [first, blend] = sampleInterpolatedFrame (layer);
    const float current = track[std::min (first, track.size () - 1)];
    const float next = track[std::min (first + 1, track.size () - 1)];
    return glm::mix (current, next, blend);
}
std::vector<float> sampleDependencies (
    const MdlAnimationData& data, const std::vector<MdlActiveAnimation>& layers, MdlPhysicsStep* physics
) {
    auto values = physics ? physics->state.dependencyWeights : std::vector<float> {};
    values.resize (data.dependencyCount, 0.0f);
    bool first = true;
    for (const auto& layer : layers) {
        if (!layer.enabled || !layer.animation) continue;
        const float weight = layer.weight;
        if (first && (layer.additive || weight != 1.0f))
            std::fill (values.begin (), values.end (), 0.0f);
        first = false;
        for (size_t row = 0; row < values.size () && row < layer.animation->dependencyTracks.size (); ++row) {
            if (row < layer.animation->dependencyFlags.size () && (layer.animation->dependencyFlags[row] & 1)) continue;
            const auto& track = layer.animation->dependencyTracks[row];
            if (track.empty ()) continue;
            const float sample = sampleScalarTrack (layer, track);
            if (layer.additive) values[row] += sample * weight;
            else values[row] = values[row] * (1.0f - weight) + sample * weight;
        }
    }
    if (physics) physics->state.dependencyWeights = values;
    return values;
}
} // namespace

MdlPose MdlAnimationEvaluator::evaluate (
    const MdlAnimationData& animationData, const std::vector<MdlActiveAnimation>& activeAnimations,
    MdlPhysicsStep* physics
) {
    MdlPose pose;
    pose.worldBones.resize (animationData.bones.size ());
    pose.skinBones.resize (animationData.bones.size ());
    std::vector<glm::mat4> locals;
    if (!animationData.ikGroups.empty ()) locals.resize (animationData.bones.size ());
    auto ropes = physics ? prepareRopes (animationData, activeAnimations, *physics) : std::nullopt;
    if (ropes && physics->previewRope) {
        // Native setup leaves previous348 absent. Preview only after admission, so
        // standalone/rejected spring graphs retain their established state path.
        physics = nullptr;
        ropes.reset ();
    }
    if (physics != nullptr && physics->state.bones.size () != animationData.bones.size ()) {
	physics->state = { .bones = std::vector<MdlPhysicsState::Bone> (animationData.bones.size ()) };
    }
    pose.dependencyWeights = sampleDependencies (animationData, activeAnimations, physics);
    if (!ropes && physics && physics->ropeFallbackModel) physics->model = *physics->ropeFallbackModel;
    const bool ownerTraversal = ropes && !ropes->legacyWholeSkeleton;
    const bool springAfterDependencies = ownerTraversal || animationData.dependencyCount != 0;
    glm::mat4 toLocal (1.0f);
    if (ropes) {
        toLocal = glm::inverse (physics->model);
        physics->state.ropePoints.resize (animationData.bones.size ());
    }
    if (ownerTraversal || animationData.dependencyCount != 0) {
        if (physics && physics->state.workingWorldBones.size () == animationData.bones.size ()) {
            pose.worldBones = physics->state.workingWorldBones;
        } else {
            // Both forward dependencies and unvisited solver bones read these initial/reference worlds.
            for (size_t bone = 0; bone < animationData.bones.size (); ++bone) {
                const auto& entry = animationData.bones[bone];
                pose.worldBones[bone] = (entry.parent >= 0 ? pose.worldBones[entry.parent] : glm::mat4 (1.0f))
                    * entry.referenceLocal.value_or (entry.bindLocal);
            }
        }
        if (ownerTraversal) pose.publishedWorldBones.resize (animationData.bones.size ());
    }
    // Native skips spring motion until it has a preceding frame.
    const bool simulate = physics != nullptr && !physics->state.previousWorld.empty ();

    for (size_t bone = 0; bone < animationData.bones.size (); bone++) {
	// Native 1401fdf90 initializes the shared blend reference from MDLS's
	// optional reference matrices, falling back to the skeleton bind pose.
	// A clip's first frame can already reposition atlas parts; subtracting it
	// instead makes an idle additive layer displace the dragon's eye (3233141951).
	const auto& skeletonBone = animationData.bones[bone];
        // Solver-owned non-owners retain the working pose from earlier owners/frames.
        if (ownerTraversal && ropes->solved[bone] && !ropes->owners[bone]) {
            const auto parent = skeletonBone.parent;
            locals[bone] = parent >= 0 ? glm::inverse (pose.worldBones[parent]) * pose.worldBones[bone] : pose.worldBones[bone];
            pose.publishedWorldBones[bone] = pose.worldBones[bone];
            continue;
        }
	std::optional<MdlBoneFrame> reference;
	std::optional<MdlBoneFrame> local;
	const auto referencePose = [&] () -> const MdlBoneFrame& {
	    if (!reference) reference = matrixPose (skeletonBone.referenceLocal.value_or (skeletonBone.bindLocal));
	    return *reference;
	};

	for (size_t layerIndex = 0; layerIndex < activeAnimations.size (); layerIndex++) {
	    const auto& layer = activeAnimations[layerIndex];
	    if (layer.animation == nullptr || bone >= layer.animation->boneFrames.size ()) {
		continue;
	    }
	    // MDLA bit zero masks this track out of the layer. Blink-only clips
	    // retain static bone samples which must not overwrite the moving body.
	    if (bone < layer.animation->boneFlags.size () && (layer.animation->boneFlags[bone] & 1)) {
		continue;
	    }
	    const auto& frames = layer.animation->boneFrames[bone];
	    if (frames.empty () || layer.weight <= 0.0f) {
		continue;
	    }

	    const auto sampled = samplePose (layer, frames);
	    if (!layer.additive && layer.weight >= 1.0f) {
		// A full replacement needs neither the reference decomposition nor a
		// second blend whose result is already the sample.
		local = sampled;
		local->rotation = glm::normalize (local->rotation);
	    } else {
		if (!local) local = referencePose ();
		// Every partial/additive layer uses the same reference and separate T/R/S.
		composeLayer (*local, referencePose (), sampled, layer);
	    }
	}

	const auto parent = animationData.bones[bone].parent;
	const glm::mat4 parentWorld = parent >= 0 ? pose.worldBones[parent] : glm::mat4 (1.0f);
	glm::mat4 localMatrix = poseMatrix (local ? *local : referencePose ());
	if (simulate && skeletonBone.spring && !springAfterDependencies) {
	    localMatrix *= stepSpring (
		*skeletonBone.spring, *physics, physics->state.bones[bone], physics->state.previousWorld[bone],
		parentWorld * localMatrix
	    );
	}
	if (!locals.empty ()) locals[bone] = localMatrix;
	pose.worldBones[bone] = parentWorld * localMatrix;
        for (const auto& dependency : skeletonBone.dependencies) {
            pose.worldBones[bone] = blendDependency (pose.worldBones[bone], pose.worldBones[dependency.source],
                pose.dependencyWeights[dependency.weight]);
        }
        // The ordinary owner walk applies dependencies before spring inertia and publication.
        if (springAfterDependencies && simulate && skeletonBone.spring) {
            const auto spring = stepSpring (
                *skeletonBone.spring, *physics, physics->state.bones[bone], physics->state.previousWorld[bone],
                pose.worldBones[bone]
            );
            pose.worldBones[bone] *= spring;
            if (!locals.empty ()) locals[bone] *= spring;
        }
	pose.skinBones[bone] = pose.worldBones[bone] * animationData.bones[bone].inverseBindWorld;
        if (ownerTraversal) {
            // Native publishes +340 before this owner's group, not before every group.
            pose.publishedWorldBones[bone] = pose.worldBones[bone];
            if (ropes->owners[bone])
                solveRopeGroup (animationData, *ropes->owners[bone], *ropes, *physics, toLocal, pose);
        }
    }

    // Spring history and public getters share native's per-bone publication timing.
    if (physics != nullptr) {
	physics->state.previousWorld.resize (pose.worldBones.size ());
	for (size_t bone = 0; bone < pose.worldBones.size (); bone++)
	    physics->state.previousWorld[bone] = physics->model
                * (ownerTraversal ? pose.publishedWorldBones[bone] : pose.worldBones[bone]);
    }

    if (ropes) {
        if (!ownerTraversal) {
            // Preserve the existing whole-skeleton synthetic route with unordered/type-zero bones.
            const auto& group = animationData.ikGroups.front ();
            if (physics->state.workingWorldBones.size () == animationData.bones.size ())
                for (size_t bone = 0; bone < animationData.bones.size (); ++bone)
                    if (bone != group.root && (animationData.bones[bone].type & 2))
                        pose.worldBones[bone] = physics->state.workingWorldBones[bone];
            solveRopeGroup (animationData, group, *ropes, *physics, toLocal, pose);
        }
        physics->state.workingWorldBones = pose.worldBones;
        for (size_t bone = 0; bone < animationData.bones.size (); ++bone)
            pose.skinBones[bone] = pose.worldBones[bone] * animationData.bones[bone].inverseBindWorld;
    } else if (!animationData.ikGroups.empty ()) {
        applyBoneControls (animationData, activeAnimations, locals, pose);
    }

    if (physics && animationData.dependencyCount != 0) physics->state.workingWorldBones = pose.worldBones;

    // scalar blend tracks compose exactly like the bones do, one value per track row
    size_t blendRows = 0;
    for (const auto& layer : activeAnimations) {
	if (layer.animation != nullptr) {
	    blendRows = std::max (blendRows, layer.animation->blendTracks.size ());
	}
    }
    pose.blendWeights.assign (blendRows, 0.0f);

    for (size_t row = 0; row < blendRows; row++) {
	const auto hasRow = [row] (const MdlActiveAnimation& layer) {
	    return layer.animation != nullptr && row < layer.animation->blendTracks.size ()
		&& !layer.animation->blendTracks[row].empty ();
	};

	float value = 0.0f;
	size_t firstComposedLayer = 0;

	if (!activeAnimations.empty ()) {
	    const auto& baseLayer = activeAnimations.front ();
	    if (hasRow (baseLayer)) {
		value = glm::mix (
		    baseLayer.animation->blendTracks[row].front (), sampleScalarTrack (baseLayer, baseLayer.animation->blendTracks[row]),
		    std::clamp (baseLayer.weight, 0.0f, 1.0f)
		);
	    }
	    firstComposedLayer = 1;
	}

	for (size_t layerIndex = firstComposedLayer; layerIndex < activeAnimations.size (); layerIndex++) {
	    const auto& layer = activeAnimations[layerIndex];
	    if (!hasRow (layer) || layer.weight <= 0.0f) {
		continue;
	    }

	    const float sampled = sampleScalarTrack (layer, layer.animation->blendTracks[row]);
	    if (layer.additive) {
		value += (sampled - layer.animation->blendTracks[row].front ()) * layer.weight;
	    } else {
		value = glm::mix (value, sampled, std::clamp (layer.weight, 0.0f, 1.0f));
	    }
	}

	pose.blendWeights[row] = value;
    }

    // Native 1401fdf90 resets to the MDLS defaults and floor-samples the late
    // MDLA depth stream. An untouched result keeps the previous draw order.
    for (const auto& layer : activeAnimations) {
	if (!layer.animation || layer.weight <= 0.0f) continue;
	const auto& clip = *layer.animation;
	const auto frame = static_cast<size_t> (sampleFrame (layer));
	for (size_t bone = 0; bone < animationData.defaultDepths.size () && bone < clip.depthTracks.size (); ++bone) {
	    if (clip.depthTracks[bone].empty ()
		|| (bone < clip.boneFlags.size () && (clip.boneFlags[bone] & 3) == 1)) continue;
	    if (pose.depths.empty ())
		pose.depths.assign (animationData.defaultDepths.begin (), animationData.defaultDepths.end ());
	    const auto& samples = clip.depthTracks[bone];
	    const float sample = samples[std::min (frame, samples.size () - 1)];
	    float& value = pose.depths[bone];
	    const float weight = std::clamp (layer.weight, 0.0f, 1.0f);
	    if (layer.additive) {
		value = std::clamp (value + (sample - animationData.defaultDepths[bone]) * weight,
		    std::min (value, sample), std::max (value, sample));
	    } else {
		value = glm::mix (value, sample, weight);
	    }
	}
    }

    // 1401fdf90 keeps activation separate from the stored weight. A full zero
    // sample clears its bit without overwriting the value read by later layers.
    for (const auto& layer : activeAnimations) {
	if (!layer.enabled || !layer.animation || layer.animation->morphTracks.empty ()) continue;
	for (const auto& track : layer.animation->morphTracks.front ()) {
	    if (track.target >= 64 || track.frames.empty ()) continue;
	    if (pose.puppetMorphWeights.size () <= track.target)
		pose.puppetMorphWeights.resize (static_cast<size_t> (track.target) + 1, 0.0f);
	    const auto bit = uint64_t { 1 } << track.target;
	    const float sample = sampleScalarTrack (layer, track.frames);
	    const bool active = std::abs (sample) >= std::numeric_limits<float>::epsilon ();
	    const float weight = std::clamp (layer.weight, 0.0f, 1.0f);
	    float& value = pose.puppetMorphWeights[track.target];
	    if (layer.additive) {
		if (!active) continue;
		pose.puppetMorphMask |= bit;
		const float contribution = sample * weight;
		value = std::clamp (value + contribution, std::min (value, contribution), std::max (value, contribution));
	    } else if (weight == 1.0f) {
		if (active) {
		    pose.puppetMorphMask |= bit;
		    value = sample;
		} else {
		    pose.puppetMorphMask &= ~bit;
		}
	    } else {
		if (active) pose.puppetMorphMask |= bit;
		value = glm::mix (value, sample, weight);
	    }
	}
    }

    return pose;
}

std::vector<MdlModelMorphState> MdlAnimationEvaluator::evaluateModelMorphs (
    const std::vector<MdlActiveAnimation>& activeAnimations
) {
    std::vector<MdlModelMorphState> result;
    // 14021c480 skips zero-weight model layers and clamps only partial replacement.
    // Puppet activation and unclamped partial blending remain a separate contract.
    for (const auto& layer : activeAnimations) {
        if (!layer.enabled || !layer.animation || layer.weight <= 0.0f) continue;
        const auto& submeshes = layer.animation->morphTracks;
        result.resize (std::max (result.size (), submeshes.size ()));
        const float weight = std::clamp (layer.weight, 0.0f, 1.0f);
        for (size_t submesh = 0; submesh < submeshes.size (); ++submesh) {
            auto& state = result[submesh];
            for (const auto& track : submeshes[submesh]) {
                if (track.target >= 64 || track.frames.empty ()) continue;
                if (state.weights.size () <= track.target) state.weights.resize (track.target + 1, 0.0f);
                const float sample = sampleScalarTrack (layer, track.frames);
                const bool active = std::abs (sample) >= std::numeric_limits<float>::epsilon ();
                const uint64_t bit = uint64_t { 1 } << track.target;
                float& value = state.weights[track.target];
                if (layer.additive) {
                    if (!active) continue;
                    state.mask |= bit;
                    const float contribution = sample * weight;
                    value = std::clamp (value + contribution, std::min (value, contribution), std::max (value, contribution));
                } else if (weight == 1.0f) {
                    if (active) {
                        state.mask |= bit;
                        value = sample;
                    } else {
                        state.mask &= ~bit;
                    }
                } else {
                    if (active) state.mask |= bit;
                    value = std::clamp (glm::mix (value, sample, weight), 0.0f, 1.0f);
                }
            }
        }
    }
    return result;
}

std::optional<glm::mat4> MdlAnimationEvaluator::attachmentTransform (
    const MdlAnimationData& animationData, const std::vector<glm::mat4>& worldBones, const std::string& name
) {
    const auto attachment = animationData.attachments.find (name);
    if (attachment == animationData.attachments.end () || attachment->second.bone >= worldBones.size ()) {
	return std::nullopt;
    }
    return worldBones[attachment->second.bone] * attachment->second.local;
}

std::optional<std::string> MdlAnimationEvaluator::hitBox (
    const MdlAnimationData& animationData, const std::vector<glm::mat4>& worldBones,
    const glm::vec3& rayOrigin, const glm::vec3& rayDirection
) {
    for (auto entry = animationData.hitBoxOrder.rbegin (); entry != animationData.hitBoxOrder.rend (); ++entry) {
	const auto bone = *entry;
	if (bone >= animationData.hitBoxes.size () || bone >= worldBones.size ()) continue;
	const auto& box = animationData.hitBoxes[bone];
	const glm::mat4 transform = worldBones[bone] * box.local;
	const float determinant = glm::determinant (transform);
	if (!std::isfinite (determinant) || determinant == 0.0f) continue;
	const glm::mat4 inverse = glm::inverse (transform);
	const glm::vec3 origin = glm::vec3 (inverse * glm::vec4 (rayOrigin, 1.0f));
	const glm::vec3 direction = glm::vec3 (inverse * glm::vec4 (rayDirection, 0.0f));
	float near = -std::numeric_limits<float>::infinity ();
	float far = std::numeric_limits<float>::infinity ();
	bool intersects = true;
	for (int axis = 0; axis < 3; ++axis) {
	    if (!std::isfinite (origin[axis]) || !std::isfinite (direction[axis])) {
		intersects = false;
		break;
	    }
	    if (direction[axis] == 0.0f) {
		if (std::abs (origin[axis]) > box.halfExtents[axis]) intersects = false;
		continue;
	    }
	    const float first = (-box.halfExtents[axis] - origin[axis]) / direction[axis];
	    const float second = (box.halfExtents[axis] - origin[axis]) / direction[axis];
	    near = std::max (near, std::min (first, second));
	    far = std::min (far, std::max (first, second));
	}
	// Native selects the first intersecting box in authored order, not the
	// nearest triangle or bone. It tests the whole cursor line in this space.
	if (intersects && near <= far) {
	    const auto& name = animationData.bones[bone].name;
	    return name.empty () ? std::to_string (bone) : name;
	}
    }
    return std::nullopt;
}
