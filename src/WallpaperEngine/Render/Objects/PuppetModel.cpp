#include "PuppetModel.h"

#include "WallpaperEngine/Logging/Log.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>

using namespace WallpaperEngine::Render::Objects;

namespace {

/** Bounded little-endian cursor; all reads are validated against the buffer */
struct Cursor {
    const char* data;
    size_t size;
    size_t off { 0 };
    bool ok { true };

    template <typename T> T read () {
	T value {};
	if (!ok || off + sizeof (T) > size) {
	    ok = false;
	    return value;
	}
	std::memcpy (&value, data + off, sizeof (T));
	off += sizeof (T);
	return value;
    }

    std::string readCString () {
	if (!ok) {
	    return {};
	}
	const auto* end = static_cast<const char*> (std::memchr (data + off, 0, size - off));
	if (end == nullptr) {
	    ok = false;
	    return {};
	}
	std::string value (data + off, end);
	off = static_cast<size_t> (end - data) + 1;
	return value;
    }

    void skip (size_t bytes) {
	if (off + bytes > size) {
	    ok = false;
	    return;
	}
	off += bytes;
    }
};

size_t findMarker (const std::vector<char>& data, const char* marker, size_t from = 0) {
    const size_t len = std::strlen (marker);
    for (size_t offset = from; offset + len <= data.size (); offset++) {
	if (std::memcmp (data.data () + offset, marker, len) == 0) {
	    return offset;
	}
    }
    return data.size ();
}

/** File matrices are D3D row-major with translation in row 3; a plain memcpy into
 *  column-major glm is exactly the transpose, which converts row-vector convention
 *  to column-vector convention (translation lands in column 3) */
glm::mat4 readFileMatrix (Cursor& cur) {
    glm::mat4 out (1.0f);
    float values[16];
    for (float& value : values) {
	value = cur.read<float> ();
    }
    if (cur.ok) {
	std::memcpy (&out, values, sizeof (values));
    }
    return out;
}

PuppetModel::PlaybackMode parseMode (const std::string& mode) {
    if (mode == "mirror") {
	return PuppetModel::PlaybackMode::Mirror;
    }
    if (mode == "single") {
	return PuppetModel::PlaybackMode::Single;
    }
    // "loop" and anything unknown (silent-default)
    return PuppetModel::PlaybackMode::Loop;
}

bool parseSkeletonCandidate (
    const std::vector<char>& data, size_t boneCountOffset, size_t firstRecordOffset, PuppetModel& model
) {
    if (boneCountOffset + sizeof (uint32_t) > data.size ()) {
	return false;
    }

    uint32_t boneCount = 0;
    std::memcpy (&boneCount, data.data () + boneCountOffset, sizeof (boneCount));
    if (boneCount == 0 || boneCount > 512) {
	return false;
    }

    Cursor cur { data.data (), data.size (), firstRecordOffset };
    std::vector<PuppetModel::Bone> parsed;
    std::vector<glm::mat4> bindWorld (boneCount);
    parsed.reserve (boneCount);

    const auto looksLikeRecord = [&data] (size_t off, uint32_t index) {
	if (off + 12 > data.size ()) {
	    return false;
	}
	int32_t parent = 0;
	uint32_t matrixBytes = 0;
	std::memcpy (&parent, data.data () + off + 4, sizeof (parent));
	std::memcpy (&matrixBytes, data.data () + off + 8, sizeof (matrixBytes));
	return matrixBytes == 64 && parent >= -1 && parent < static_cast<int32_t> (index);
    };

    for (uint32_t i = 0; i < boneCount; i++) {
	if (!looksLikeRecord (cur.off, i)) {
	    return false;
	}

	cur.read<uint32_t> (); // per-bone flags
	const auto parent = cur.read<int32_t> ();
	const auto matrixBytes = cur.read<uint32_t> ();
	if (!cur.ok || matrixBytes != 64 || parent < -1 || parent >= static_cast<int32_t> (i)) {
	    return false;
	}

	const glm::mat4 bindLocal = readFileMatrix (cur);
	if (!cur.ok) {
	    return false;
	}

	bindWorld[i] = parent >= 0 ? bindWorld[parent] * bindLocal : bindLocal;
	parsed.push_back (
	    PuppetModel::Bone {
		.parent = parent, .bindLocal = bindLocal, .bindWorldInverse = glm::inverse (bindWorld[i]) }
	);

	if (i + 1 < boneCount) {
	    // Different MDLS revisions use different small alignment/padding fields
	    // between bone records. Search a bounded window instead of assuming +2.
	    bool foundNext = false;
	    for (size_t pad = 0; pad <= 16 && cur.off + pad < data.size (); pad++) {
		if (looksLikeRecord (cur.off + pad, i + 1)) {
		    cur.off += pad;
		    foundNext = true;
		    break;
		}
	    }
	    if (!foundNext) {
		return false;
	    }
	}
    }

    model.bones = std::move (parsed);
    return true;
}

/**
 * MDLS0004 stores an absolute MDLA offset, the bone count, and variable-sized
 * bone records (name, flags, parent, matrix, metadata). The legacy heuristic
 * below assumes adjacent fixed-size records and cannot walk this layout.
 */
bool matrixIsFinite (const glm::mat4& matrix) {
    for (int column = 0; column < 4; ++column) {
	for (int row = 0; row < 4; ++row) {
	    if (!std::isfinite (matrix[column][row])) {
		return false;
	    }
	}
    }
    return true;
}

bool readMdls0004Bone (
    Cursor& cur, uint32_t index, std::vector<glm::mat4>& bindWorld, PuppetModel::Bone& bone
) {
    const std::string name = cur.readCString ();
    const uint32_t flags = cur.read<uint32_t> ();
    const int32_t parent = cur.read<int32_t> ();
    const uint32_t matrixBytes = cur.read<uint32_t> ();
    // MDLS0004 may contain unnamed helper bones. Names are not used by the
    // renderer (channels and mesh weights address bones by index), so keep the
    // record if its terminator, matrix and hierarchy are structurally valid.
    if (!cur.ok || matrixBytes != 64 || parent < -1 || parent >= static_cast<int32_t> (index)) {
	return false;
    }
    (void)name;
    (void)flags;

    const glm::mat4 bindLocal = readFileMatrix (cur);
    if (!cur.ok || !matrixIsFinite (bindLocal)) {
	return false;
    }

    // Per-bone JSON metadata follows the matrix before the next bone name.
    const std::string metadata = cur.readCString ();
    if (!cur.ok || (!metadata.empty () && metadata.front () != '{')) {
	return false;
    }

    const glm::mat4 world = parent >= 0 ? bindWorld[static_cast<size_t> (parent)] * bindLocal : bindLocal;
    const glm::mat4 inverse = glm::inverse (world);
    if (!matrixIsFinite (inverse)) {
	return false;
    }

    bindWorld.push_back (world);
    bone = PuppetModel::Bone { .parent = parent, .bindLocal = bindLocal, .bindWorldInverse = inverse };
    return true;
}

bool parseMdls0004Header (
    const std::vector<char>& data, size_t mdlsOffset, size_t requiredBones, uint32_t& mdlaOffset, uint32_t& boneCount,
    Cursor& header
) {
    constexpr size_t markerLength = 8;
    if (mdlsOffset + markerLength + 1 + 2 * sizeof (uint32_t) > data.size ()
	|| std::memcmp (data.data () + mdlsOffset, "MDLS0004", markerLength) != 0
	|| data[mdlsOffset + markerLength] != '\0') {
	return false;
    }

    header = Cursor { data.data (), data.size (), mdlsOffset + markerLength + 1 };
    mdlaOffset = header.read<uint32_t> ();
    boneCount = header.read<uint32_t> ();
    if (!header.ok || mdlaOffset <= header.off || mdlaOffset > data.size () || data.size () - mdlaOffset < 4
	|| boneCount < requiredBones || boneCount == 0 || boneCount > 512) {
	return false;
    }

    const bool pointsToMdla = std::memcmp (data.data () + mdlaOffset, "MDLA", 4) == 0;
    // Some valid MDLS0004 packages point at the MDAT chunk immediately after
    // the bone records; their MDLA animation chunk follows later in the file.
    const bool pointsToMdat = std::memcmp (data.data () + mdlaOffset, "MDAT", 4) == 0;
    return pointsToMdla || pointsToMdat;
}

/**
 * MDLS0004 stores an absolute MDLA offset, the bone count, and variable-sized
 * bone records (name, flags, parent, matrix, metadata). The legacy heuristic
 * below assumes adjacent fixed-size records and cannot walk this layout.
 */
bool parseMdls0004Skeleton (
    const std::vector<char>& data, size_t mdlsOffset, size_t requiredBones, std::vector<PuppetModel::Bone>& bones
) {
    Cursor header { data.data (), data.size (), 0 };
    uint32_t mdlaOffset = 0;
    uint32_t boneCount = 0;
    if (!parseMdls0004Header (data, mdlsOffset, requiredBones, mdlaOffset, boneCount, header)) {
	return false;
    }

    Cursor cur { data.data (), mdlaOffset, header.off };
    std::vector<PuppetModel::Bone> parsed;
    std::vector<glm::mat4> bindWorld;
    parsed.reserve (boneCount);
    bindWorld.reserve (boneCount);

    for (uint32_t index = 0; index < boneCount; ++index) {
	PuppetModel::Bone bone;
	if (!readMdls0004Bone (cur, index, bindWorld, bone)) {
	    return false;
	}
	parsed.push_back (bone);
    }

    if (parsed.size () < requiredBones) {
	return false;
    }
    bones = std::move (parsed);
    return true;
}

bool parseSkeleton (const std::vector<char>& data, PuppetModel& model) {
    // The mesh tells us a hard lower bound for the real skeleton size: every
    // non-zero blend weight must reference an existing bone. Use this to reject
    // accidental one-record matches in large MDLS blocks.
    size_t requiredBones = 0;
    for (size_t i = 0; i < model.blendIndices.size () && i < model.blendWeights.size (); i++) {
	for (int j = 0; j < 4; j++) {
	    if (model.blendWeights[i][j] > 0.0f) {
		requiredBones = std::max (requiredBones, static_cast<size_t> (model.blendIndices[i][j]) + 1);
	    }
	}
    }
    requiredBones = std::max<size_t> (requiredBones, 1);

    // MDLS0004 records are variable length. Prefer its explicit format parser,
    // then retain the structural heuristics for earlier and unknown revisions.
    size_t searchV4 = 0;
    while (searchV4 < data.size ()) {
	const size_t mdls = findMarker (data, "MDLS0004", searchV4);
	if (mdls >= data.size ()) {
	    break;
	}
	std::vector<PuppetModel::Bone> parsed;
	if (parseMdls0004Skeleton (data, mdls, requiredBones, parsed)) {
	    model.bones = std::move (parsed);
	    sLog.out (
		"Puppet MDLS0004 skeleton parsed offset=", mdls, " bones=", model.bones.size (),
		" requiredByMesh=", requiredBones
	    );
	    return true;
	}
	searchV4 = mdls + 8;
    }

    std::vector<PuppetModel::Bone> best;
    size_t bestMdls = data.size ();

    size_t searchFrom = 0;
    while (searchFrom < data.size ()) {
	const size_t mdls = findMarker (data, "MDLS", searchFrom);
	if (mdls >= data.size ()) {
	    break;
	}

	const size_t nextMdla = findMarker (data, "MDLA", mdls + 4);
	const size_t limit = nextMdla < data.size () ? nextMdla : data.size ();
	const size_t payload = std::min (mdls + 8, limit);

	// Count-based layouts: search much farther than the old fixed 256-byte
	// window, and retain the largest structurally valid skeleton instead of the
	// first candidate.
	const size_t countLimit = std::min (payload + 65536, limit);
	for (size_t countOff = payload; countOff + sizeof (uint32_t) <= countLimit; countOff++) {
	    uint32_t count = 0;
	    std::memcpy (&count, data.data () + countOff, sizeof (count));
	    if (count < requiredBones || count > 512) {
		continue;
	    }

	    const size_t recordMin = countOff + sizeof (uint32_t);
	    const size_t recordLimit = std::min (recordMin + 512, limit);
	    for (size_t recordOff = recordMin; recordOff + 12 <= recordLimit; recordOff++) {
		PuppetModel candidate;
		if (!parseSkeletonCandidate (data, countOff, recordOff, candidate)) {
		    continue;
		}
		if (candidate.bones.size () >= requiredBones && candidate.bones.size () > best.size ()) {
		    best = std::move (candidate.bones);
		    bestMdls = mdls;
		}
	    }
	}

	// Count-less layouts: infer the longest chain. Scan the full MDLS section
	// and allow larger revision-specific gaps between records.
	for (size_t start = payload; start + 12 + 64 <= limit; start++) {
	    int32_t firstParent = 0;
	    uint32_t firstMatrixBytes = 0;
	    std::memcpy (&firstParent, data.data () + start + 4, sizeof (firstParent));
	    std::memcpy (&firstMatrixBytes, data.data () + start + 8, sizeof (firstMatrixBytes));
	    if (firstParent != -1 || firstMatrixBytes != 64) {
		continue;
	    }

	    Cursor cur { data.data (), data.size (), start };
	    std::vector<PuppetModel::Bone> parsed;
	    std::vector<glm::mat4> bindWorld;
	    for (uint32_t index = 0; index < 512; index++) {
		if (cur.off + 12 + 64 > limit) {
		    break;
		}

		int32_t parent = 0;
		uint32_t matrixBytes = 0;
		std::memcpy (&parent, data.data () + cur.off + 4, sizeof (parent));
		std::memcpy (&matrixBytes, data.data () + cur.off + 8, sizeof (matrixBytes));
		if (matrixBytes != 64 || parent < -1 || parent >= static_cast<int32_t> (index)) {
		    break;
		}

		cur.read<uint32_t> ();
		parent = cur.read<int32_t> ();
		cur.read<uint32_t> ();
		const glm::mat4 bindLocal = readFileMatrix (cur);
		if (!cur.ok) {
		    break;
		}

		bindWorld.push_back (parent >= 0 ? bindWorld[parent] * bindLocal : bindLocal);
		parsed.push_back (
		    PuppetModel::Bone {
			.parent = parent, .bindLocal = bindLocal, .bindWorldInverse = glm::inverse (bindWorld.back ()) }
		);

		bool foundNext = false;
		for (size_t pad = 0; pad <= 512 && cur.off + pad + 12 <= limit; pad++) {
		    int32_t p = 0;
		    uint32_t bytes = 0;
		    std::memcpy (&p, data.data () + cur.off + pad + 4, sizeof (p));
		    std::memcpy (&bytes, data.data () + cur.off + pad + 8, sizeof (bytes));
		    if (bytes == 64 && p >= -1 && p <= static_cast<int32_t> (index)) {
			cur.off += pad;
			foundNext = true;
			break;
		    }
		}
		if (!foundNext) {
		    break;
		}
	    }

	    if (parsed.size () >= requiredBones && parsed.size () > best.size ()) {
		best = std::move (parsed);
		bestMdls = mdls;
	    }
	}

	searchFrom = mdls + 4;
    }

    if (best.empty ()) {
	sLog.error ("Puppet skeleton not found; mesh requires at least ", requiredBones, " bones");
	return false;
    }

    model.bones = std::move (best);
    sLog.out (
	"Puppet skeleton parsed offset=", bestMdls, " bones=", model.bones.size (), " requiredByMesh=", requiredBones
    );
    return true;
}

bool parseAnimationCandidate (
    const std::vector<char>& data, size_t start, const PuppetModel& model, PuppetModel::Clip& clip, size_t& endOffset
) {
    Cursor cur { data.data (), data.size (), start };
    clip = {};
    clip.id = cur.read<uint32_t> ();
    const uint32_t zero0 = cur.read<uint32_t> ();
    clip.name = cur.readCString ();
    const std::string mode = cur.readCString ();
    clip.mode = parseMode (mode);
    clip.fps = cur.read<float> ();
    clip.frameCount = cur.read<uint32_t> ();
    const uint32_t zero1 = cur.read<uint32_t> ();
    const auto channelCount = cur.read<uint32_t> ();

    if (!cur.ok || zero0 != 0 || zero1 != 0 || (mode != "loop" && mode != "mirror" && mode != "single")
	|| !std::isfinite (clip.fps) || clip.fps <= 0.0f || clip.fps > 1000.0f || clip.frameCount == 0
	|| clip.frameCount > 1000000 || channelCount == 0 || channelCount > model.bones.size ()) {
	return false;
    }

    clip.channels.resize (channelCount);
    for (uint32_t channelIndex = 0; channelIndex < channelCount; channelIndex++) {
	if (cur.off + 8 > cur.size) {
	    return false;
	}
	cur.read<uint32_t> (); // channel flags/pad
	const auto byteSize = cur.read<uint32_t> ();
	if (!cur.ok || byteSize == 0 || byteSize % 36 != 0 || byteSize > cur.size - cur.off) {
	    return false;
	}

	const size_t records = byteSize / 36;
	auto& channel = clip.channels[channelIndex];
	channel.reserve (records);
	for (size_t record = 0; record < records; record++) {
	    PuppetModel::Key key;
	    key.position = { cur.read<float> (), cur.read<float> (), cur.read<float> () };
	    key.rotation = { cur.read<float> (), cur.read<float> (), cur.read<float> () };
	    key.scale = { cur.read<float> (), cur.read<float> (), cur.read<float> () };
	    if (!cur.ok) {
		return false;
	    }
	    channel.push_back (key);
	}
    }

    endOffset = cur.off;
    return true;
}

bool parseAnimations (const std::vector<char>& data, PuppetModel& model) {
    if (model.bones.empty ()) {
	return false;
    }

    // Scan MDLA sections for structurally valid clip headers. This avoids relying
    // on one revision-specific animation-count preamble.
    size_t searchFrom = 0;
    std::vector<PuppetModel::Clip> found;
    while (searchFrom < data.size ()) {
	const size_t mdla = findMarker (data, "MDLA", searchFrom);
	if (mdla >= data.size ()) {
	    break;
	}

	// MDLA uses a NUL-terminated version header (e.g. MDLA0006), followed by
	// a block-size field and the authored clip count. Large puppet files can
	// keep all clips under a single MDLA marker, so never cap this scan at 1 MiB.
	size_t declaredClipCount = 0;
	if (const auto* headerEnd
	    = static_cast<const char*> (std::memchr (data.data () + mdla, 0, data.size () - mdla));
	    headerEnd != nullptr) {
	    const size_t afterHeader = static_cast<size_t> (headerEnd - data.data ()) + 1;
	    if (afterHeader + 2 * sizeof (uint32_t) <= data.size ()) {
		uint32_t count = 0;
		std::memcpy (&count, data.data () + afterHeader + sizeof (uint32_t), sizeof (count));
		if (count > 0 && count <= 512) {
		    declaredClipCount = count;
		}
	    }
	}

	const size_t scanBegin = std::min (mdla + 4, data.size ());
	const size_t scanEnd = data.size ();
	size_t offset = scanBegin;
	while (offset + 24 < scanEnd) {
	    PuppetModel::Clip candidate;
	    size_t candidateEnd = offset;
	    if (parseAnimationCandidate (data, offset, model, candidate, candidateEnd)) {
		const bool duplicate = std::any_of (found.begin (), found.end (), [&candidate] (const auto& existing) {
		    return existing.id == candidate.id;
		});
		if (!duplicate) {
		    found.push_back (std::move (candidate));
		    if (declaredClipCount > 0 && found.size () >= declaredClipCount) {
			break;
		    }
		}
		offset = std::max (candidateEnd, offset + 1);
	    } else {
		offset++;
	    }
	}

	searchFrom = mdla + 4;
    }

    if (found.empty ()) {
	return false;
    }

    model.clips = std::move (found);
    sLog.out ("Puppet animations parsed via MDLA scan clips=", model.clips.size ());
    return true;
}

bool parseAttachments (const std::vector<char>& data, PuppetModel& model) {
    const size_t mdat = findMarker (data, "MDAT");
    if (mdat >= data.size ()) {
	return false;
    }

    const auto* headerEnd = static_cast<const char*> (std::memchr (data.data () + mdat, 0, data.size () - mdat));
    if (headerEnd == nullptr) {
	return false;
    }

    Cursor cur { data.data (), data.size (), static_cast<size_t> (headerEnd - data.data ()) + 1 };
    const uint32_t blockEnd = cur.read<uint32_t> ();
    const uint16_t count = cur.read<uint16_t> ();
    if (!cur.ok || count > 256 || blockEnd > data.size () || blockEnd <= cur.off) {
	return false;
    }

    std::vector<PuppetModel::Attachment> parsed;
    parsed.reserve (count);
    for (uint16_t i = 0; i < count; i++) {
	const uint16_t bone = cur.read<uint16_t> ();
	const std::string name = cur.readCString ();
	const glm::mat4 local = readFileMatrix (cur);
	if (!cur.ok || bone >= model.bones.size () || name.empty () || cur.off > blockEnd) {
	    return false;
	}
	parsed.push_back (PuppetModel::Attachment { .bone = bone, .name = name, .local = local });
    }

    model.attachments = std::move (parsed);
    sLog.out ("Puppet attachments parsed count=", model.attachments.size ());
    return !model.attachments.empty ();
}

/** Wrap into [0, period) handling negative phases (rate can be user-driven negative) */
double wrapPhase (double phase, double period) {
    double cycle = std::fmod (phase, period);
    if (cycle < 0.0) {
	cycle += period;
    }
    return cycle;
}

float clipFrame (const PuppetModel::Clip& clip, double phase) {
    const auto frames = static_cast<double> (clip.frameCount);
    switch (clip.mode) {
	case PuppetModel::PlaybackMode::Single:
	    return static_cast<float> (std::clamp (phase, 0.0, frames));
	case PuppetModel::PlaybackMode::Mirror:
	    {
		const double cycle = wrapPhase (phase, frames * 2.0);
		return static_cast<float> (cycle <= frames ? cycle : frames * 2.0 - cycle);
	    }
	case PuppetModel::PlaybackMode::Loop:
	default:
	    return static_cast<float> (wrapPhase (phase, frames));
    }
}

PuppetModel::Key sampleChannel (const std::vector<PuppetModel::Key>& channel, float frame) {
    if (channel.empty ()) {
	return {};
    }
    const auto i0 = static_cast<size_t> (frame);
    const size_t i1 = std::min (i0 + 1, channel.size () - 1);
    const float t = frame - static_cast<float> (i0);
    const auto& a = channel[std::min (i0, channel.size () - 1)];
    const auto& b = channel[i1];
    return PuppetModel::Key { .position = glm::mix (a.position, b.position, t),
			      .rotation = glm::mix (a.rotation, b.rotation, t),
			      .scale = glm::mix (a.scale, b.scale, t) };
}


struct SkinnedLayout {
    size_t stride { 80 };
    size_t idxOff { 40 };
    size_t weightOff { 56 };
    size_t uvOff { 72 };
};

struct MeshBlock {
    SkinnedLayout layout {};
    size_t vertexStride { 80 };
    size_t verticesOffset { 0 };
    uint32_t vertexBytes { 0 };
    uint32_t indexBytes { 0 };
};

enum class StructuredRecordResult { Invalid, Continue, Found };

uint32_t parseMdlvVersion (const std::vector<char>& data) {
    uint32_t version = 0;
    for (size_t digit = 4; digit < 8 && digit < data.size (); digit++) {
	const char value = data[digit];
	if (value < '0' || value > '9') {
	    return 0;
	}
	version = version * 10 + static_cast<uint32_t> (value - '0');
    }
    return version;
}

size_t findMaterialsEnd (const std::vector<char>& data) {
    const size_t marker = findMarker (data, "materials/");
    if (marker >= data.size ()) {
	return 9;
    }
    const auto* end
	= static_cast<const char*> (std::memchr (data.data () + marker, 0, data.size () - marker));
    return end != nullptr ? static_cast<size_t> (end - data.data ()) : 9;
}

bool readMeshU32 (const std::vector<char>& data, size_t& offset, uint32_t& out) {
    if (offset + sizeof (uint32_t) > data.size ()) {
	return false;
    }
    std::memcpy (&out, data.data () + offset, sizeof (out));
    offset += sizeof (uint32_t);
    return true;
}

bool skipMaterialNames (const std::vector<char>& data, size_t& offset, uint32_t materialCount) {
    for (uint32_t material = 0; material < materialCount; material++) {
	while (offset < data.size () && static_cast<unsigned char> (data[offset]) <= 0x20) {
	    offset++;
	}
	if (offset >= data.size ()) {
	    return false;
	}
	const auto* nameEnd
	    = static_cast<const char*> (std::memchr (data.data () + offset, 0, data.size () - offset));
	if (nameEnd == nullptr) {
	    return false;
	}
	offset = static_cast<size_t> (nameEnd - data.data ()) + 1;
    }
    return true;
}

bool buildSkinnedLayout (
    uint32_t tag, SkinnedLayout& layout, size_t& stride, bool& hasIndices, bool& hasWeights, bool& hasUv
) {
    constexpr uint32_t KNOWN_VERTEX_BITS = 0x0181002F;
    struct AttributeSize {
	uint32_t bit;
	size_t bytes;
    };
    constexpr AttributeSize ATTRIBUTES[] = {
	{ 0x00000002, 12 }, { 0x00000004, 16 }, { 0x00010000, 4 }, { 0x00800000, 16 },
	{ 0x01000000, 16 }, { 0x00000020, 16 }, { 0x00000008, 8 },
    };
    if ((tag & ~KNOWN_VERTEX_BITS) != 0) {
	return false;
    }

    stride = 12;
    for (const auto& attribute : ATTRIBUTES) {
	if ((tag & attribute.bit) == 0) {
	    continue;
	}
	if (attribute.bit == 0x00800000u) {
	    layout.idxOff = stride;
	    hasIndices = true;
	} else if (attribute.bit == 0x01000000u) {
	    layout.weightOff = stride;
	    hasWeights = true;
	} else if (attribute.bit == 0x00000008u) {
	    layout.uvOff = stride;
	    hasUv = true;
	}
	stride += attribute.bytes;
    }
    layout.stride = stride;
    return true;
}

StructuredRecordResult parseStructuredMeshRecord (
    const std::vector<char>& data, uint32_t mdlvVersion, uint32_t headerTag, uint32_t materialCount,
    size_t& offset, MeshBlock& mesh
) {
    if (!skipMaterialNames (data, offset, materialCount)) {
	return StructuredRecordResult::Invalid;
    }

    uint32_t flags = 0;
    if (!readMeshU32 (data, offset, flags)) {
	return StructuredRecordResult::Invalid;
    }
    if ((flags & 0x2u) != 0) {
	uint32_t extra = 0;
	if (!readMeshU32 (data, offset, extra)) {
	    return StructuredRecordResult::Invalid;
	}
    }
    if (mdlvVersion >= 17) {
	offset += 6 * sizeof (float);
    }

    uint32_t tag = headerTag;
    if (mdlvVersion >= 16 && !readMeshU32 (data, offset, tag)) {
	return StructuredRecordResult::Invalid;
    }

    SkinnedLayout layout {};
    size_t stride = 0;
    bool hasIndices = false;
    bool hasWeights = false;
    bool hasUv = false;
    if (!buildSkinnedLayout (tag, layout, stride, hasIndices, hasWeights, hasUv)) {
	return StructuredRecordResult::Invalid;
    }

    uint32_t vertexBytes = 0;
    if (!readMeshU32 (data, offset, vertexBytes) || vertexBytes == 0 || vertexBytes % stride != 0
	|| offset + vertexBytes > data.size ()) {
	return StructuredRecordResult::Invalid;
    }
    const size_t verticesOffset = offset;
    offset += vertexBytes;

    const size_t indexWidth = (flags & 0x1u) != 0 ? 4 : 2;
    uint32_t indexBytes = 0;
    if (!readMeshU32 (data, offset, indexBytes) || indexBytes == 0 || indexBytes % (indexWidth * 3) != 0
	|| offset + indexBytes > data.size ()) {
	return StructuredRecordResult::Invalid;
    }
    offset += indexBytes;

    if (!hasIndices || !hasWeights || !hasUv) {
	return StructuredRecordResult::Continue;
    }
    if (indexWidth != 2) {
	sLog.error ("Puppet mesh uses 32-bit indices - unsupported, falling back");
	return StructuredRecordResult::Invalid;
    }

    mesh.layout = layout;
    mesh.vertexStride = stride;
    mesh.verticesOffset = verticesOffset;
    mesh.vertexBytes = vertexBytes;
    mesh.indexBytes = indexBytes;
    return StructuredRecordResult::Found;
}

bool walkStructuredMesh (const std::vector<char>& data, uint32_t mdlvVersion, MeshBlock& mesh) {
    const size_t magicEnd = std::string_view (data.data (), data.size ()).find ('\0');
    if (magicEnd == std::string_view::npos) {
	return false;
    }
    size_t offset = magicEnd + 1;

    uint32_t headerTag = 0;
    uint32_t materialCount = 0;
    uint32_t submeshCount = 0;
    if (!readMeshU32 (data, offset, headerTag) || !readMeshU32 (data, offset, materialCount)
	|| !readMeshU32 (data, offset, submeshCount)) {
	return false;
    }
    if (submeshCount == 0 || submeshCount > 16 || materialCount == 0 || materialCount > 16) {
	return false;
    }

    for (uint32_t record = 0; record < submeshCount; record++) {
	const auto result
	    = parseStructuredMeshRecord (data, mdlvVersion, headerTag, materialCount, offset, mesh);
	if (result == StructuredRecordResult::Found) {
	    return true;
	}
	if (result == StructuredRecordResult::Invalid) {
	    return false;
	}
    }
    return false;
}

bool scanMeshRange (
    const std::vector<char>& data, size_t from, size_t to, size_t skeletonStart, MeshBlock& mesh
) {
    for (size_t offset = from; offset + sizeof (uint32_t) < skeletonStart && offset < to; offset++) {
	uint32_t candidate = 0;
	std::memcpy (&candidate, data.data () + offset, sizeof (candidate));
	if (candidate == 0 || candidate % mesh.vertexStride != 0) {
	    continue;
	}

	const size_t indexLengthOffset = offset + sizeof (uint32_t) + candidate;
	if (indexLengthOffset + sizeof (uint32_t) > skeletonStart) {
	    continue;
	}
	uint32_t candidateIndexBytes = 0;
	std::memcpy (
	    &candidateIndexBytes, data.data () + indexLengthOffset, sizeof (candidateIndexBytes)
	);
	if (candidateIndexBytes == 0 || candidateIndexBytes % (sizeof (uint16_t) * 3) != 0
	    || indexLengthOffset + sizeof (uint32_t) + candidateIndexBytes > data.size ()) {
	    continue;
	}

	mesh.verticesOffset = offset + sizeof (uint32_t);
	mesh.vertexBytes = candidate;
	mesh.indexBytes = candidateIndexBytes;
	return true;
    }
    return false;
}

bool findSkinnedMesh (const std::vector<char>& data, MeshBlock& mesh) {
    const uint32_t mdlvVersion = parseMdlvVersion (data);
    if (walkStructuredMesh (data, mdlvVersion, mesh)) {
	return true;
    }

    const size_t skeletonStart = findMarker (data, "MDLS");
    const size_t materialsEnd = findMaterialsEnd (data);
    if (scanMeshRange (data, materialsEnd, materialsEnd + 128, skeletonStart, mesh)) {
	return true;
    }
    return scanMeshRange (data, 9, skeletonStart, skeletonStart, mesh);
}

bool decodeSkinnedMesh (
    const std::vector<char>& data, const MeshBlock& mesh, PuppetModel& model, std::string& error
) {
    const size_t vertexCount = mesh.vertexBytes / mesh.vertexStride;
    model.positions.reserve (vertexCount);
    model.uvs.reserve (vertexCount);
    model.blendIndices.reserve (vertexCount);
    model.blendWeights.reserve (vertexCount);

    for (size_t i = 0; i < vertexCount; i++) {
	const char* vertex = data.data () + mesh.verticesOffset + i * mesh.vertexStride;
	float position[3];
	std::memcpy (position, vertex, sizeof (position));
	uint32_t indices[4];
	std::memcpy (indices, vertex + mesh.layout.idxOff, sizeof (indices));
	float weights[4];
	std::memcpy (weights, vertex + mesh.layout.weightOff, sizeof (weights));
	float uv[2];
	std::memcpy (uv, vertex + mesh.layout.uvOff, sizeof (uv));
	model.positions.emplace_back (position[0], position[1], position[2]);
	model.blendIndices.emplace_back (indices[0], indices[1], indices[2], indices[3]);
	model.blendWeights.emplace_back (weights[0], weights[1], weights[2], weights[3]);
	model.uvs.emplace_back (uv[0], uv[1]);
    }

    const size_t indicesOffset = mesh.verticesOffset + mesh.vertexBytes + sizeof (uint32_t);
    const size_t indexCount = mesh.indexBytes / sizeof (uint16_t);
    model.indices.resize (indexCount);
    std::memcpy (model.indices.data (), data.data () + indicesOffset, mesh.indexBytes);
    if (std::any_of (model.indices.begin (), model.indices.end (), [vertexCount] (uint16_t index) {
	    return index >= vertexCount;
	})) {
	error = "mesh index out of range";
	return false;
    }
    return true;
}

void loadOptionalPuppetData (const std::vector<char>& data, PuppetModel& model) {
    if (!parseSkeleton (data, model)) {
	model.bones.clear ();
	return;
    }
    parseAttachments (data, model);
    if (!parseAnimations (data, model)) {
	model.clips.clear ();
    }
}

} // namespace

const PuppetModel::Clip* PuppetModel::findClip (uint32_t id) const {
    for (const auto& clip : clips) {
	if (clip.id == id) {
	    return &clip;
	}
    }
    return nullptr;
}

const PuppetModel::Attachment* PuppetModel::findAttachment (const std::string& name) const {
    for (const auto& attachment : attachments) {
	if (attachment.name == name) {
	    return &attachment;
	}
    }
    return nullptr;
}

void PuppetModel::evaluateWorldPose (
    const std::vector<ActiveLayer>& layers, double time, std::vector<glm::mat4>& outWorld
) const {
    const size_t boneCount = bones.size ();
    outWorld.resize (boneCount);

    std::vector<Key> accumulated (boneCount);
    std::vector<bool> hasPose (boneCount, false);

    const auto blendRotation = [] (const glm::vec3& from, const glm::vec3& to, const float amount) {
	glm::vec3 delta = to - from;
	for (int axis = 0; axis < 3; axis++) {
	    delta[axis] = std::remainder (delta[axis], glm::two_pi<float> ());
	}
	return from + delta * amount;
    };

    for (const auto& layer : layers) {
	const float blend = std::clamp (layer.blend, 0.0f, 1.0f);
	if (layer.clip == nullptr || blend == 0.0f) {
	    continue;
	}
	const double phase = time * static_cast<double> (layer.clip->fps) * static_cast<double> (layer.rate);
	const float frame = clipFrame (*layer.clip, phase);

	for (size_t b = 0; b < boneCount && b < layer.clip->channels.size (); b++) {
	    const auto& channel = layer.clip->channels[b];
	    if (channel.empty ()) {
		continue;
	    }
	    const Key rest = channel.front ();
	    const Key key = sampleChannel (channel, frame);
	    if (!hasPose[b]) {
		accumulated[b] = rest;
		hasPose[b] = true;
	    }

	    if (layer.additive) {
		accumulated[b].position += (key.position - rest.position) * blend;
		accumulated[b].rotation += (key.rotation - rest.rotation) * blend;
		const glm::vec3 restScale = glm::max (glm::abs (rest.scale), glm::vec3 (1e-6f));
		const glm::vec3 scaleRatio = key.scale / restScale;
		accumulated[b].scale *= glm::mix (glm::vec3 (1.0f), scaleRatio, blend);
	    } else {
		accumulated[b].position = glm::mix (accumulated[b].position, key.position, blend);
		accumulated[b].rotation = blendRotation (accumulated[b].rotation, key.rotation, blend);
		accumulated[b].scale = glm::mix (accumulated[b].scale, key.scale, blend);
	    }
	}
    }

    for (size_t b = 0; b < boneCount; b++) {
	glm::mat4 local;
	if (hasPose[b]) {
	    const auto& key = accumulated[b];
	    local = glm::translate (glm::mat4 (1.0f), key.position);
	    // Keep rotations in the model's authored coordinate space; CImage reflects
	    // final puppet vertices when it uploads them into scene space.
	    local = glm::rotate (local, key.rotation.z, glm::vec3 (0, 0, 1));
	    local = glm::rotate (local, key.rotation.y, glm::vec3 (0, 1, 0));
	    local = glm::rotate (local, key.rotation.x, glm::vec3 (1, 0, 0));
	    local = glm::scale (local, key.scale);
	} else {
	    local = bones[b].bindLocal;
	}
	outWorld[b] = bones[b].parent >= 0 ? outWorld[bones[b].parent] * local : local;
    }
}

void PuppetModel::evaluateSkinning (
    const std::vector<ActiveLayer>& layers, double time, std::vector<glm::mat4>& out
) const {
    std::vector<glm::mat4> world;
    this->evaluateWorldPose (layers, time, world);
    out.resize (bones.size ());
    for (size_t b = 0; b < bones.size (); b++) {
	out[b] = world[b] * bones[b].bindWorldInverse;
    }
}

void PuppetModel::skinPositions (const std::vector<glm::mat4>& skin, std::vector<glm::vec3>& out) const {
    const size_t count = positions.size ();
    out.resize (count);
    for (size_t i = 0; i < count; i++) {
	const glm::vec4 pos (positions[i], 1.0f);
	const auto& idx = blendIndices[i];
	const auto& weight = blendWeights[i];
	glm::vec4 result (0.0f);
	for (int j = 0; j < 4; j++) {
	    const float w = weight[j];
	    if (w == 0.0f) {
		continue;
	    }
	    const uint32_t bone = idx[j];
	    if (bone < skin.size ()) {
		result += skin[bone] * pos * w;
	    }
	}
	out[i] = glm::vec3 (result);
    }
}

std::optional<PuppetModel> PuppetModel::parse (const std::vector<char>& data, std::string& error) {
    if (data.size () < 32 || std::memcmp (data.data (), "MDLV", 4) != 0) {
	error = "not an MDLV container";
	return std::nullopt;
    }

    MeshBlock mesh;
    if (!findSkinnedMesh (data, mesh)) {
	error = "no skinned mesh block found (structured walk + stride-80 scan)";
	return std::nullopt;
    }

    PuppetModel model;
    if (!decodeSkinnedMesh (data, mesh, model, error)) {
	return std::nullopt;
    }

    loadOptionalPuppetData (data, model);
    return model;
}
