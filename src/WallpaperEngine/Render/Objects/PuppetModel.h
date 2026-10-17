#pragma once

#include <cstdint>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <optional>
#include <string>
#include <vector>

namespace WallpaperEngine::Render::Objects {

class PuppetModel {
public:
    struct Bone {
	int parent { -1 };
	glm::mat4 bindLocal { 1.0f };
	glm::mat4 bindWorldInverse { 1.0f };
    };

    struct Attachment {
	uint16_t bone { 0 };
	std::string name;
	glm::mat4 local { 1.0f };
    };

    struct Key {
	glm::vec3 position { 0.0f };
	glm::vec3 rotation { 0.0f }; // euler radians
	glm::vec3 scale { 1.0f };
    };

    enum class PlaybackMode { Loop, Mirror, Single };

    struct Clip {
	/** Referenced by the scene object's animationlayers[].animation */
	uint32_t id { 0 };
	std::string name;
	PlaybackMode mode { PlaybackMode::Loop };
	float fps { 30.0f };
	uint32_t frameCount { 0 };
	/** [bone][record]; record count = frameCount+1, record 0 = rest pose */
	std::vector<std::vector<Key>> channels;
    };

    struct ActiveLayer {
	const Clip* clip { nullptr };
	float rate { 1.0f };
	float blend { 1.0f };
	bool additive { false };
    };

    // mesh (bind pose, image-local coordinates)
    std::vector<glm::vec3> positions;
    std::vector<glm::vec2> uvs;
    std::vector<uint16_t> indices;
    std::vector<glm::uvec4> blendIndices;
    std::vector<glm::vec4> blendWeights;

    std::vector<Bone> bones;
    std::vector<Clip> clips;
    std::vector<Attachment> attachments;

    [[nodiscard]] const Clip* findClip (uint32_t id) const;
    [[nodiscard]] const Attachment* findAttachment (const std::string& name) const;
    [[nodiscard]] bool hasAnimation () const { return !bones.empty () && !clips.empty (); }

    void
    evaluateWorldPose (const std::vector<ActiveLayer>& layers, double time, std::vector<glm::mat4>& outWorld) const;
    void evaluateSkinning (const std::vector<ActiveLayer>& layers, double time, std::vector<glm::mat4>& out) const;

    void skinPositions (const std::vector<glm::mat4>& skin, std::vector<glm::vec3>& out) const;

    /** Parses a .mdl puppet. Mesh failure -> nullopt; skeleton/animation failures
     *  degrade to a static mesh (silent-default policy). */
    static std::optional<PuppetModel> parse (const std::vector<char>& data, std::string& error);
};

} // namespace WallpaperEngine::Render::Objects
