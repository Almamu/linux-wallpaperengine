#include <algorithm>
#include <array>
#include <cstdlib>
#include <fstream>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "WallpaperEngine/Data/Model/DynamicValue.h"
#include "WallpaperEngine/Data/Parsers/MdlParser.h"
#include "WallpaperEngine/Data/Parsers/ObjectParser.h"
#include "WallpaperEngine/Data/Model/Project.h"
#include "WallpaperEngine/Data/Model/Wallpaper.h"
#include "WallpaperEngine/FileSystem/Container.h"

#include <catch2/catch_test_macros.hpp>

using WallpaperEngine::Data::Model::MdlMesh;
using WallpaperEngine::Data::Parsers::MdlParser;

namespace {
template <typename T> void appendValue (std::vector<char>& data, const T& value) {
    const auto* bytes = reinterpret_cast<const char*> (&value);
    data.insert (data.end (), bytes, bytes + sizeof (T));
}

std::vector<char> makeModel (
    uint32_t submeshFlags, uint32_t vertexTag = 15, uint32_t vertexStride = 48,
    const std::array<float, 6>& bounds = {}, uint32_t version = 23, uint32_t materialCount = 1,
    bool appendEmptyTail = true
) {
    std::vector<char> data;
    const std::string marker =
	"MDLV" + std::string (2, '0') + static_cast<char> ('0' + version / 10) + static_cast<char> ('0' + version % 10);
    data.insert (data.end (), marker.begin (), marker.end ());
    data.push_back ('\0');

    appendValue<uint32_t> (data, vertexTag);
    appendValue<uint32_t> (data, materialCount);
    appendValue<uint32_t> (data, 1);

    for (uint32_t material = 0; material < materialCount; material++) {
	const std::string path = "materials/test" + std::to_string (material) + ".json";
	data.insert (data.end (), path.begin (), path.end ());
	data.push_back ('\0');
    }

    appendValue (data, submeshFlags);

    // flag bit one prefixes the bounds with one more uint
    if ((submeshFlags & 0x2) != 0) {
	appendValue<uint32_t> (data, 1);
    }

    // per-submesh bounds appear in revision 17, the per-submesh vertex tag in revision 16
    if (version >= 17) {
	for (const float value : bounds) {
	    appendValue (data, value);
	}
    }

    const uint32_t vertexBytes = 3 * vertexStride;
    if (version >= 16) {
	appendValue (data, vertexTag);
    }
    appendValue (data, vertexBytes);
    data.resize (data.size () + vertexBytes, '\0');

    const bool wideIndices = (submeshFlags & 1) != 0;
    const uint32_t indexBytes = 3 * (wideIndices ? sizeof (uint32_t) : sizeof (uint16_t));
    appendValue (data, indexBytes);
    for (uint32_t index = 0; index < 3; index++) {
	if (wideIndices) {
	    appendValue (data, index);
	} else {
	    appendValue (data, static_cast<uint16_t> (index));
	}
    }

    if (appendEmptyTail && version >= 21) {
	appendValue<uint8_t> (data, 0);
	appendValue<uint8_t> (data, 0);
    }
    if (appendEmptyTail && version >= 23) {
	appendValue<uint32_t> (data, 0);
    }

    return data;
}

/**
 * Puppet models pair a skinned body with a channelmap overlay: a second submesh using a
 * different vertex tag and an extra header field. Each revision-23 submesh ends
 * with the native empty auxiliary/range/clipping tail.
 */
std::vector<char> makeTwoLayoutModel () {
    std::vector<char> data;
    const std::string marker = "MDLV0023";
    data.insert (data.end (), marker.begin (), marker.end ());
    data.push_back ('\0');

    appendValue<uint32_t> (data, 0x0180000f);
    appendValue<uint32_t> (data, 1);
    appendValue<uint32_t> (data, 2);

    const std::array<std::pair<std::string, uint32_t>, 2> submeshes {
	std::pair {std::string ("materials/body.json"), 0x0180000fu},
	std::pair {std::string ("materials/overlay_channelmap.json"), 0x00800021u},
    };

    bool first = true;
    for (const auto& [path, tag] : submeshes) {
	data.insert (data.end (), path.begin (), path.end ());
	data.push_back ('\0');

	const uint32_t flags = first ? 0x0 : 0x2;
	appendValue (data, flags);
	if ((flags & 0x2) != 0) {
	    appendValue<uint32_t> (data, 1);
	}

	for (int bound = 0; bound < 6; bound++) {
	    appendValue (data, 0.0f);
	}

	const uint32_t stride = first ? 80 : 44;
	const uint32_t vertexBytes = 3 * stride;
	appendValue (data, tag);
	appendValue (data, vertexBytes);
	data.resize (data.size () + vertexBytes, '\0');

	appendValue<uint32_t> (data, 3 * sizeof (uint16_t));
	for (uint16_t index = 0; index < 3; index++) {
	    appendValue (data, index);
	}

	appendValue<uint8_t> (data, 0);
	appendValue<uint8_t> (data, 0);
	appendValue<uint32_t> (data, 0);
	first = false;
    }

    return data;
}

std::vector<char> makeClippingModel () {
    auto data = makeModel (0, 15, 48, {}, 23, 1, false);

    appendValue<uint8_t> (data, 1);
    appendValue<uint32_t> (data, 1);
    appendValue<uint32_t> (data, 3 * 3 * sizeof (float));
    for (const glm::vec3 position :
	 { glm::vec3 (1.0f, 2.0f, 3.0f), glm::vec3 (4.0f, 5.0f, 6.0f), glm::vec3 (7.0f, 8.0f, 9.0f) }) {
	appendValue (data, position.x);
	appendValue (data, position.y);
	appendValue (data, position.z);
    }

    appendValue<uint8_t> (data, 1);
    appendValue<uint32_t> (data, 2 * 4 * sizeof (uint32_t));
    for (const uint32_t value : { 17u, 0u, 0u, 3u, 18u, 0u, 0u, 3u }) {
	appendValue (data, value);
    }

    appendValue<uint32_t> (data, 1);
    appendValue<uint64_t> (data, 0x1122334455667788ull);
    const std::string asset = "masks/clipping_mask_test";
    data.insert (data.end (), asset.begin (), asset.end ());
    data.push_back ('\0');
    appendValue<uint32_t> (data, 1);
    appendValue<uint32_t> (data, 1);
    appendValue<uint32_t> (data, 1);
    appendValue<uint32_t> (data, 1);
    appendValue<uint32_t> (data, 0);

    return data;
}

TEST_CASE ("MDLV parser exposes skinned vertex attributes") {
    const auto mesh = MdlParser::parse (makeModel (0, 0x0180000f, 80), "test-skinned.mdl");

    CHECK (mesh.skinned);
    CHECK (mesh.strideBytes == 80);
    CHECK (mesh.blendIndicesOffset == 40);
    CHECK (mesh.blendWeightsOffset == 56);
    CHECK (mesh.uvOffset == 72);
}

TEST_CASE ("MDLV parser exposes model bounds for SceneScript") {
    const auto mesh = MdlParser::parse (
	makeModel (0, 15, 48, { -1.0f, -2.0f, -3.0f, 4.0f, 5.0f, 6.0f }), "test-bounds.mdl"
    );

    REQUIRE (mesh.hasBoundingBox);
    CHECK (mesh.boundingBoxMin.x == -1.0f);
    CHECK (mesh.boundingBoxMin.y == -2.0f);
    CHECK (mesh.boundingBoxMin.z == -3.0f);
    CHECK (mesh.boundingBoxMax.x == 4.0f);
    CHECK (mesh.boundingBoxMax.y == 5.0f);
    CHECK (mesh.boundingBoxMax.z == 6.0f);
}

TEST_CASE ("MDLV parser reads auxiliary positions, draw ranges and clipping descriptors") {
    const auto mesh = MdlParser::parse (makeClippingModel (), "test-clipping.mdl");

    CHECK (mesh.version == 23);
    CHECK (mesh.auxiliaryHeader == 1);
    REQUIRE (mesh.auxiliaryPositions.size () == 3);
    CHECK (mesh.auxiliaryPositions[2] == glm::vec3 (7.0f, 8.0f, 9.0f));

    REQUIRE (mesh.drawRanges.size () == 2);
    CHECK (mesh.drawRanges[0].partId == 17);
    CHECK (mesh.drawRanges[0].submeshIndex == 0);
    CHECK (mesh.drawRanges[0].firstIndex == 0);
    CHECK (mesh.drawRanges[0].indexCount == 3);

    REQUIRE (mesh.clippingDescriptors.size () == 1);
    const auto& descriptor = mesh.clippingDescriptors[0];
    CHECK (descriptor.opaqueId == 0x1122334455667788ull);
    CHECK (descriptor.maskAsset == "masks/clipping_mask_test");
    CHECK (descriptor.flags == 1);
    CHECK (descriptor.targets == std::vector<uint32_t> { 1 });
    CHECK (descriptor.sources == std::vector<uint32_t> { 0 });
}

TEST_CASE ("MDLV parser rejects malformed draw range blocks") {
    auto data = makeModel (0, 15, 48, {}, 23, 1, false);
    appendValue<uint8_t> (data, 0);
    appendValue<uint8_t> (data, 1);
    appendValue<uint32_t> (data, 15);
    data.resize (data.size () + 15);

    CHECK_THROWS_AS (MdlParser::parse (data, "test-bad-range-size.mdl"), std::runtime_error);
}

TEST_CASE ("MDLV parser rejects draw ranges outside their submesh") {
    auto data = makeModel (0, 15, 48, {}, 23, 1, false);
    appendValue<uint8_t> (data, 0);
    appendValue<uint8_t> (data, 1);
    appendValue<uint32_t> (data, 4 * sizeof (uint32_t));
    for (const uint32_t value : { 17u, 0u, 2u, 3u }) {
	appendValue (data, value);
    }

    CHECK_THROWS_AS (MdlParser::parse (data, "test-bad-range-bounds.mdl"), std::runtime_error);
}

TEST_CASE ("MDLV parser rejects clipping descriptors with invalid range references") {
    auto data = makeModel (0, 15, 48, {}, 23, 1, false);
    appendValue<uint8_t> (data, 0);
    appendValue<uint8_t> (data, 0);
    appendValue<uint32_t> (data, 1);
    appendValue<uint64_t> (data, 42);
    data.push_back ('\0');
    appendValue<uint32_t> (data, 0);
    appendValue<uint32_t> (data, 1);
    appendValue<uint32_t> (data, 0);
    appendValue<uint32_t> (data, 0);

    CHECK_THROWS_AS (MdlParser::parse (data, "test-bad-clipping-reference.mdl"), std::runtime_error);
}
} // namespace

TEST_CASE ("MDLV auxiliary submesh flag keeps 16-bit indices") {
    const auto mesh = MdlParser::parse (makeModel (0x400), "test-auxiliary.mdl");

    REQUIRE (mesh.submeshes.size () == 1);
    CHECK (mesh.strideBytes == 48);
    CHECK (mesh.submeshes[0].indices == std::vector<uint32_t> { 0, 1, 2 });
}

TEST_CASE ("MDLV auxiliary submesh flag composes with wide indices") {
    const auto mesh = MdlParser::parse (makeModel (0x401), "test-wide.mdl");

    REQUIRE (mesh.submeshes.size () == 1);
    CHECK (mesh.submeshes[0].indices == std::vector<uint32_t> { 0, 1, 2 });
}

TEST_CASE ("MDLV parser reads through descriptive submesh flags") {
    // only bit zero changes the payload; rejecting the rest costs a dozen wallpapers that
    // set purely descriptive bits, and a genuine desync still trips the structural checks
    const auto mesh = MdlParser::parse (makeModel (0x800), "test-descriptive.mdl");

    REQUIRE (mesh.submeshes.size () == 1);
    CHECK (mesh.strideBytes == 48);
    CHECK (mesh.submeshes[0].indices == std::vector<uint32_t> { 0, 1, 2 });
}

TEST_CASE ("MDLV parser rejects vertex tags it cannot size") {
    CHECK_THROWS_AS (MdlParser::parse (makeModel (0, 0x40000000), "test-unknown-tag.mdl"), std::runtime_error);
}

TEST_CASE ("MDLV revision 16 carries no per-submesh bounds") {
    const auto mesh = MdlParser::parse (makeModel (0, 15, 48, {}, 16), "test-r16.mdl");

    REQUIRE (mesh.submeshes.size () == 1);
    CHECK_FALSE (mesh.hasBoundingBox);
    CHECK (mesh.strideBytes == 48);
    CHECK (mesh.submeshes[0].indices == std::vector<uint32_t> { 0, 1, 2 });
}

TEST_CASE ("MDLV revisions before 16 take the vertex tag from the header") {
    const auto mesh = MdlParser::parse (makeModel (0, 0x0180000f, 80, {}, 7), "test-r7.mdl");

    REQUIRE (mesh.submeshes.size () == 1);
    CHECK (mesh.skinned);
    CHECK (mesh.strideBytes == 80);
    CHECK (mesh.uvOffset == 72);
}

TEST_CASE ("MDLV revision 14 zero tag uses the legacy compact puppet layout") {
    const auto mesh = MdlParser::parse (makeModel (0, 0, 52, {}, 14), "test-r14-puppet.mdl");

    REQUIRE (mesh.submeshes.size () == 1);
    CHECK (mesh.skinned);
    CHECK (mesh.strideBytes == 52);
    CHECK (mesh.blendIndicesOffset == 12);
    CHECK (mesh.blendWeightsOffset == 28);
    CHECK (mesh.uvOffset == 44);
}

TEST_CASE ("MDLV vertex layout omits attributes the tag leaves out") {
    // position | uv | skin: no normal or tangent block, so uv sits right after the weights
    const auto mesh = MdlParser::parse (makeModel (0, 0x01800009, 52), "test-sparse.mdl");

    REQUIRE (mesh.submeshes.size () == 1);
    CHECK (mesh.strideBytes == 52);
    CHECK (mesh.blendIndicesOffset == 12);
    CHECK (mesh.uvOffset == 44);
    CHECK (mesh.normalOffset == MdlMesh::AttributeAbsent);
    CHECK (mesh.tangentOffset == MdlMesh::AttributeAbsent);
}

TEST_CASE ("MDLV channelmap overlay vertices carry a vec4 texcoord") {
    // the blink overlay layout: position, four blend indices and a vec4 texcoord, no weights
    const auto mesh = MdlParser::parse (makeModel (0x2, 0x00800021, 44), "test-channelmap.mdl");

    REQUIRE (mesh.submeshes.size () == 1);
    CHECK (mesh.strideBytes == 44);
    CHECK (mesh.submeshes[0].strideBytes == 44);
    CHECK (mesh.submeshes[0].blendIndicesOffset == 12);
    CHECK (mesh.submeshes[0].texCoordVec4Offset == 28);
    CHECK (mesh.submeshes[0].indices == std::vector<uint32_t> { 0, 1, 2 });
}

TEST_CASE ("MDLV morph positions precede the normal and tangent", "[mdl][morph-layout]") {
    // Native attribute tables 140484a20/1404849b0 put vec4 position before normal/tangent.
    // The installed Reze puppet uses this 84-byte layout.
    const auto mesh = MdlParser::parse (makeModel (0, 0x0181000e, 84), "morph-layout.mdl");
    REQUIRE (mesh.submeshes.size () == 1);
    const auto& submesh = mesh.submeshes.front ();
    CHECK (mesh.strideBytes == 84);
    CHECK (mesh.positionOffset == 0);
    CHECK (mesh.normalOffset == 16);
    CHECK (mesh.tangentOffset == 28);
    CHECK (mesh.blendIndicesOffset == 44);
    CHECK (mesh.blendWeightsOffset == 60);
    CHECK (mesh.uvOffset == 76);
    CHECK (submesh.blendIndicesOffset == mesh.blendIndicesOffset);
    CHECK (submesh.blendWeightsOffset == mesh.blendWeightsOffset);
    CHECK (submesh.uvOffset == mesh.uvOffset);

    const auto sparse = MdlParser::parse (makeModel (0, 0x01810008, 56), "morph-sparse.mdl");
    CHECK (sparse.strideBytes == 56);
    CHECK (sparse.normalOffset == MdlMesh::AttributeAbsent);
    CHECK (sparse.tangentOffset == MdlMesh::AttributeAbsent);
    CHECK (sparse.blendIndicesOffset == 16);
    CHECK (sparse.blendWeightsOffset == 32);
    CHECK (sparse.uvOffset == 48);
}

TEST_CASE ("MDLV positions and texture coordinates use independent native format bits", "[mdl][morph-layout]") {
    const auto mesh = MdlParser::parse (makeModel (0, 0x01810009, 68), "two-positions.mdl");
    CHECK (mesh.strideBytes == 68);
    CHECK (mesh.blendIndicesOffset == 28);
    CHECK (mesh.blendWeightsOffset == 44);
    CHECK (mesh.uvOffset == 60);

    const auto uv = MdlParser::parse (makeModel (0, 0x29, 36), "two-texcoords.mdl");
    CHECK (uv.uvOffset == 12);
    REQUIRE (uv.submeshes.size () == 1);
    CHECK (uv.submeshes.front ().texCoordVec4Offset == 20);
    REQUIRE_THROWS (MdlParser::parse (makeModel (0, 8, 20), "missing-position.mdl"));
}

TEST_CASE ("Reze puppet keeps attributes aligned after its vec4 position", "[.][mdl][morph-layout-installed]") {
    const auto* path = std::getenv ("WPE_TEST_MORPH_LAYOUT_MDL");
    if (!path) SKIP ("Set WPE_TEST_MORPH_LAYOUT_MDL to the extracted Reze sl_puppet.mdl");
    std::ifstream input (path, std::ios::binary);
    REQUIRE (input.good ());
    const std::vector<char> data { std::istreambuf_iterator<char> (input), std::istreambuf_iterator<char> () };
    const auto mesh = MdlParser::parse (data, path);
    REQUIRE (mesh.submeshes.size () == 1);
    CHECK (mesh.vertexTag == 0x0181000e);
    CHECK (mesh.strideBytes == 84);
    CHECK (mesh.normalOffset == 16);
    CHECK (mesh.tangentOffset == 28);
    CHECK (mesh.blendIndicesOffset == 44);
    CHECK (mesh.blendWeightsOffset == 60);
    CHECK (mesh.uvOffset == 76);
    CHECK (mesh.submeshes.front ().vertices.size () == 3741 * 84 / sizeof (float));
    CHECK (mesh.submeshes.front ().indices.size () == 20088);
}

TEST_CASE ("MDLV submeshes may disagree on the vertex layout") {
    const auto mesh = MdlParser::parse (makeTwoLayoutModel (), "test-two-layouts.mdl");

    REQUIRE (mesh.submeshes.size () == 2);
    // the mesh-level layout describes the first submesh only
    CHECK (mesh.vertexTag == 0x0180000f);
    CHECK (mesh.strideBytes == 80);
    CHECK (mesh.submeshes[0].vertexTag == 0x0180000f);
    CHECK (mesh.submeshes[1].vertexTag == 0x00800021);
    CHECK (mesh.submeshes[1].strideBytes == 44);
    CHECK (mesh.submeshes[1].texCoordVec4Offset == 28);
    CHECK (mesh.submeshes[1].materialPath == "materials/overlay_channelmap.json");
    CHECK (mesh.submeshes[1].indices == std::vector<uint32_t> { 0, 1, 2 });
}

TEST_CASE ("MDLV submeshes carry as many material paths as the header announces") {
    // grid and prism models pair a base material with an auxiliary one; missing the second
    // path used to leave the reader sitting on ASCII where the submesh flags belong
    const auto mesh = MdlParser::parse (makeModel (0, 15, 48, {}, 23, 2), "test-materials.mdl");

    REQUIRE (mesh.submeshes.size () == 1);
    CHECK (mesh.submeshes[0].materialPath == "materials/test0.json");
    CHECK (mesh.strideBytes == 48);
    CHECK (mesh.submeshes[0].indices == std::vector<uint32_t> { 0, 1, 2 });
}

TEST_CASE ("Model objects preserve authored shadow participation") {
    using namespace WallpaperEngine::Data::Model;
    using WallpaperEngine::Data::JSON::JSON;
    auto filesystem = std::make_unique<WallpaperEngine::FileSystem::Container> ();
    const auto bytes = makeModel (0);
    filesystem->getVFS ().add ("models/sky.mdl", std::string (bytes.begin (), bytes.end ()));
    filesystem->getVFS ().add ("materials/test0.json", R"({"passes":[]})");
    Project project {};
    project.assetLocator = std::make_unique<WallpaperEngine::Assets::AssetLocator> (std::move (filesystem));

    const auto parse = [&] (const std::string& field) {
	return WallpaperEngine::Data::Parsers::ObjectParser::parse (
	    JSON::parse (R"({"id":281,"name":"sky","model":"models/sky.mdl")" + field + "}"), project
	);
    };
    const auto defaultCaster = parse ("");
    REQUIRE (defaultCaster->is<Model3D> ());
    CHECK (defaultCaster->as<Model3D> ()->castShadow->value->getBool ());

    const auto excludedSky = parse (R"(,"castshadow":false)");
    REQUIRE (excludedSky->is<Model3D> ());
    CHECK_FALSE (excludedSky->as<Model3D> ()->castShadow->value->getBool ());

    const auto scripted = parse (R"(,"castshadow":{"value":false,"script":"export function update(){return true;}"})");
    REQUIRE (scripted->is<Model3D> ());
    CHECK_FALSE (scripted->as<Model3D> ()->castShadow->value->getBool ());
    scripted->as<Model3D> ()->castShadow->value->update (true, DynamicValue::UpdateSource::Script);
    CHECK (scripted->as<Model3D> ()->castShadow->value->getBool ());
}

TEST_CASE ("Model objects in one object list share one MDL parse") {
    using namespace WallpaperEngine::Data::Model;
    using WallpaperEngine::Data::JSON::JSON;
    using WallpaperEngine::Data::Parsers::ObjectParser;
    auto filesystem = std::make_unique<WallpaperEngine::FileSystem::Container> ();
    auto& vfs = filesystem->getVFS ();
    const auto add = [&vfs] (const std::vector<char>& bytes) {
	vfs.add ("models/rock.mdl", std::string (bytes.begin (), bytes.end ()));
    };
    add (makeModel (0, 15, 48, { -1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 1.0f }));
    vfs.add ("materials/test0.json", R"({"passes":[]})");
    Project project {};
    project.assetLocator = std::make_unique<WallpaperEngine::Assets::AssetLocator> (std::move (filesystem));
    const auto parse = [&] (int id) {
	auto object = ObjectParser::parse (
	    JSON::parse (R"({"id":)" + std::to_string (id) + R"(,"model":"models/rock.mdl"})"), project
	);
	REQUIRE (object->is<Model3D> ());
	return object->as<Model3D> ()->mesh.boundingBoxMax.x;
    };

    {
	const ObjectParser::ModelCacheScope scope;
	CHECK (parse (1) == 1.0f);
	// the file changing under an open scope proves the second object reused the first parse
	add (makeModel (0, 15, 48, { -2.0f, -2.0f, -2.0f, 2.0f, 2.0f, 2.0f }));
	CHECK (parse (2) == 1.0f);
    }
    // nothing outlives the scope
    CHECK (parse (3) == 2.0f);
}

TEST_CASE ("MDLV puppet ranges retain signed depth offsets") {
    auto data = makeModel (8, 15, 48, {}, 23, 1, false);
    appendValue<uint8_t> (data, 0);
    appendValue<uint8_t> (data, 1);
    appendValue<uint32_t> (data, 16);
    appendValue<uint32_t> (data, 7);
    appendValue<int32_t> (data, -500);
    appendValue<uint32_t> (data, 0);
    appendValue<uint32_t> (data, 3);
    appendValue<uint32_t> (data, 0);
    const auto mesh = MdlParser::parse (data, "signed-depth.mdl");
    CHECK (mesh.submeshes.front ().flags == 8);
    REQUIRE (mesh.drawRanges.size () == 1);
    CHECK (mesh.drawRanges.front ().submeshIndex == 0);
    CHECK (mesh.drawRanges.front ().depthOffset == -500);
}

TEST_CASE ("MDMP rejects misaligned packed position streams", "[mdl][morph-targets]") {
    auto data = makeModel (0);
    const char marker[] = "MDMP0001";
    data.insert (data.end (), marker, marker + sizeof (marker));
    const auto endField = data.size ();
    appendValue<uint32_t> (data, 0);
    appendValue<uint16_t> (data, 1);
    appendValue<float> (data, 2);
    appendValue<uint32_t> (data, 3);
    appendValue<uint64_t> (data, 0x123456789abcdef0);
    data.push_back ('\0');
    appendValue<uint32_t> (data, 16); // Three signed XYZ triplets require 18 bytes.
    data.resize (data.size () + 16, '\0');
    const auto end = static_cast<uint32_t> (data.size ());
    std::memcpy (data.data () + endField, &end, sizeof (end));
    REQUIRE_THROWS (MdlParser::parse (data, "misaligned-morph.mdl"));
}

namespace {
size_t beginMdlSection (std::vector<char>& data, const char (&marker)[9]) {
    data.insert (data.end (), marker, marker + sizeof (marker));
    const auto endField = data.size ();
    appendValue<uint32_t> (data, 0);
    return endField;
}

void finishMdlSection (std::vector<char>& data, size_t endField) {
    const auto end = static_cast<uint32_t> (data.size ());
    std::memcpy (data.data () + endField, &end, sizeof (end));
}

void appendMorphSkeleton (std::vector<char>& data) {
    const auto endField = beginMdlSection (data, "MDLS0002");
    appendValue<uint32_t> (data, 2);
    for (uint32_t bone = 0; bone < 2; ++bone) {
	data.push_back (static_cast<char> ('a' + bone));
	data.push_back ('\0');
	appendValue<uint32_t> (data, 0);
	appendValue<uint32_t> (data, bone == 0 ? UINT32_MAX : 0);
	appendValue<uint32_t> (data, 16 * sizeof (float));
	for (int component = 0; component < 16; ++component)
	    appendValue<float> (data, component % 5 == 0 ? 1 : 0);
	data.push_back ('\0');
    }
    finishMdlSection (data, endField);
}

void appendMorphTarget (std::vector<char>& data, uint32_t flags, uint64_t id, const std::string& name) {
    appendValue (data, id);
    data.insert (data.end (), name.begin (), name.end ());
    data.push_back ('\0');
    appendValue<uint32_t> (data, 9 * sizeof (int16_t));
    for (const int16_t value : { -32768, -123, 0, 123, 32767, 4, 5, 6, 7 }) appendValue (data, value);
    for (const auto flag : { 0x400u, 0x800u }) {
	if ((flags & flag) == 0) continue;
	appendValue<uint32_t> (data, 9 * sizeof (int16_t));
	for (int component = 0; component < 9; ++component)
	    appendValue<int16_t> (data, flag == 0x400 ? 17 : -8);
    }
    if ((flags & 0x1000) != 0) {
	appendValue<uint32_t> (data, 3 * sizeof (int16_t));
	for (const int16_t value : { 0, 1234, 32767 }) appendValue (data, value);
    }
    if ((flags & 0x2000) != 0) {
	appendValue<uint32_t> (data, 1);
	appendValue<uint32_t> (data, 7); // Retain the rule code without interpreting it.
	appendValue<float> (data, -0.25f);
	appendValue<float> (data, 2);
    }
}
} // namespace

TEST_CASE ("MDMP retains target identifiers and all packed streams", "[mdl][morph-targets]") {
    for (const uint32_t flags : { 0u, 0x400u, 0x800u, 0x1000u, 0x3c00u }) {
	CAPTURE (flags);
	auto data = makeModel (flags);
	appendMorphSkeleton (data);
	const auto endField = beginMdlSection (data, "MDMP0001");
	appendValue<uint16_t> (data, 2);
	appendValue<float> (data, 12.5f);
	appendValue<uint32_t> (data, 5); // Native clamps this to the three-vertex XYZ stream.
	appendMorphTarget (data, flags, 0x123456789abcdef0, "Smile");
	appendMorphTarget (data, flags, 19, "瞳小");
	finishMdlSection (data, endField);

	const auto mesh = MdlParser::parse (data, "morph-streams.mdl");
	REQUIRE (mesh.submeshes.size () == 1);
	const auto& submesh = mesh.submeshes.front ();
	CHECK (submesh.morphScale == 12.5f);
	CHECK (submesh.morphVertexCount == 3);
	REQUIRE (submesh.morphTargets.size () == 2);
	CHECK (submesh.morphTargets[0].id == 0x123456789abcdef0);
	CHECK (submesh.morphTargets[0].name == "Smile");
	CHECK (submesh.morphTargets[1].id == 19);
	CHECK (submesh.morphTargets[1].name == "瞳小");
	for (const auto& target : submesh.morphTargets) {
	    CHECK (target.positionDeltas == std::vector<int16_t> { -32768, -123, 0, 123, 32767, 4, 5, 6, 7 });
	    CHECK (target.normalDeltas == std::vector<int16_t> ((flags & 0x400) != 0 ? 9 : 0, 17));
	    CHECK (target.tangentDeltas == std::vector<int16_t> ((flags & 0x800) != 0 ? 9 : 0, -8));
	    if ((flags & 0x1000) != 0) CHECK (target.alphaValues == std::vector<int16_t> { 0, 1234, 32767 });
	    else CHECK (target.alphaValues.empty ());
	    REQUIRE (target.modifier.has_value () == ((flags & 0x2000) != 0));
	    if (target.modifier) {
		CHECK (target.modifier->boneIndex == 1);
		CHECK (target.modifier->rule == 7);
		CHECK (target.modifier->range == glm::vec2 (-0.25f, 2));
	    }
	}
    }
}

TEST_CASE ("MDMP follows section boundaries and counts targets per submesh", "[mdl][morph-targets]") {
    auto data = makeTwoLayoutModel ();
    const auto unknownEnd = beginMdlSection (data, "MDZZ0001");
    const auto embeddedEnd = beginMdlSection (data, "MDMP0001"); // Not an actual section.
    finishMdlSection (data, embeddedEnd);
    finishMdlSection (data, unknownEnd);
    const auto endField = beginMdlSection (data, "MDMP0001");
    appendValue<uint16_t> (data, 0); // The first submesh has no targets.
    appendValue<uint16_t> (data, 1);
    appendValue<float> (data, 3);
    appendValue<uint32_t> (data, 3);
    appendMorphTarget (data, 0, 28, "second");
    appendValue<uint32_t> (data, 0xaabbccdd); // Native resumes at the declared end, skipping padding.
    finishMdlSection (data, endField);
    const auto mesh = MdlParser::parse (data, "two-morph-submeshes.mdl");
    REQUIRE (mesh.submeshes.size () == 2);
    CHECK (mesh.submeshes[0].morphTargets.empty ());
    REQUIRE (mesh.submeshes[1].morphTargets.size () == 1);
    CHECK (mesh.submeshes[1].morphTargets.front ().name == "second");
    CHECK (mesh.submeshes[1].morphScale == 3);
    CHECK (mesh.submeshes[1].morphVertexCount == 3);
}

TEST_CASE ("MDMP cannot read packed data beyond its section or declared count", "[mdl][morph-targets]") {
    auto data = makeModel (0);
    const auto endField = beginMdlSection (data, "MDMP0001");
    appendValue<uint16_t> (data, 1);
    appendValue<float> (data, 1);
    const auto countField = data.size ();
    appendValue<uint32_t> (data, 3);
    appendMorphTarget (data, 0, 0, "bounded");
    finishMdlSection (data, endField);
    auto shortSection = data;
    const auto shortEnd = static_cast<uint32_t> (data.size () - 2);
    std::memcpy (shortSection.data () + endField, &shortEnd, sizeof (shortEnd));
    REQUIRE_THROWS (MdlParser::parse (shortSection, "short-morph-section.mdl"));
    const uint32_t shortCount = 2;
    std::memcpy (data.data () + countField, &shortCount, sizeof (shortCount));
    REQUIRE_THROWS (MdlParser::parse (data, "short-morph-count.mdl"));

    auto modifier = makeModel (0x2000);
    const auto modifierEnd = beginMdlSection (modifier, "MDMP0001");
    appendValue<uint16_t> (modifier, 1);
    appendValue<float> (modifier, 1);
    appendValue<uint32_t> (modifier, 3);
    appendMorphTarget (modifier, 0x2000, 0, "missing-skeleton");
    finishMdlSection (modifier, modifierEnd);
    REQUIRE_THROWS (MdlParser::parse (modifier, "missing-morph-skeleton.mdl"));
}

TEST_CASE ("Rayman MDMP preserves its dormant authored targets", "[.][mdl][morph-installed]") {
    const auto* path = std::getenv ("WPE_TEST_MORPH_MDL");
    if (!path) SKIP ("Set WPE_TEST_MORPH_MDL to the extracted Rayman rayman.mdl");
    std::ifstream input (path, std::ios::binary);
    REQUIRE (input.good ());
    const std::vector<char> data { std::istreambuf_iterator<char> (input), std::istreambuf_iterator<char> () };
    const auto mesh = MdlParser::parse (data, path);
    REQUIRE (mesh.submeshes.size () == 18);
    for (size_t index = 0; index < mesh.submeshes.size (); ++index)
	if (index != 8 && index != 9) CHECK (mesh.submeshes[index].morphTargets.empty ());
    for (const auto index : { 8u, 9u }) {
	const auto& submesh = mesh.submeshes[index];
	CHECK (submesh.morphVertexCount == (index == 8 ? 198 : 224));
	CHECK (submesh.morphScale == (index == 8 ? 19.9653377533f : 25.8520183563f));
	REQUIRE (submesh.morphTargets.size () == 3);
	CHECK (submesh.morphTargets[0].name == "瞳小");
	CHECK (submesh.morphTargets[1].name == "Helicopter");
	CHECK (submesh.morphTargets[2].name == "Puff Cheeks");
	for (size_t targetIndex = 0; targetIndex < 3; ++targetIndex) {
	    const auto& target = submesh.morphTargets[targetIndex];
	    CHECK (target.id == 0);
	    CHECK (target.positionDeltas.size () == submesh.morphVertexCount * 3);
	    CHECK (target.normalDeltas.size () == submesh.morphVertexCount * 3);
	    CHECK (std::any_of (target.positionDeltas.begin (), target.positionDeltas.end (),
		[] (int16_t value) { return value != 0; }) == (targetIndex == 1));
	    CHECK (std::all_of (target.normalDeltas.begin (), target.normalDeltas.end (),
		[] (int16_t value) { return value == 0; }));
	}
    }
}

TEST_CASE ("MDLV optional tails belong to each submesh", "[mdl][submesh-tails]") {
    for (const uint32_t version : { 20u, 21u, 23u }) {
	INFO ("MDLV version " << version);
	auto data = makeModel (0, 15, 48, {}, version, 1, false);
	const uint32_t two = 2;
	std::memcpy (data.data () + 17, &two, sizeof (two));
	const auto appendTail = [&] (uint32_t part, uint32_t secondFirst) {
	    if (version < 21) return;
	    appendValue<uint8_t> (data, 1);
	    appendValue<uint32_t> (data, part + 1);
	    appendValue<uint32_t> (data, 12);
	    for (float value : { float (part + 1), 2.0f, 3.0f }) appendValue (data, value);
	    appendValue<uint8_t> (data, 1);
	    appendValue<uint32_t> (data, 32);
	    for (uint32_t range = 0; range < 2; ++range) {
		appendValue<uint32_t> (data, 20 + part * 10 + range);
		appendValue<int32_t> (data, -50 - static_cast<int32_t> (part));
		appendValue<uint32_t> (data, range == 0 ? 0 : secondFirst);
		appendValue<uint32_t> (data, 3);
	    }
	    if (version < 23) return;
	    appendValue<uint32_t> (data, 1);
	    appendValue<uint64_t> (data, 100 + part);
	    const std::string asset = "masks/submesh" + std::to_string (part);
	    data.insert (data.end (), asset.begin (), asset.end ());
	    data.push_back ('\0');
	    appendValue<uint32_t> (data, 0);
	    appendValue<uint32_t> (data, 1);
	    appendValue<uint32_t> (data, 1);
	    appendValue<uint32_t> (data, 1);
	    appendValue<uint32_t> (data, 0);
	};
	appendTail (0, 0);
	auto second = makeModel (0, 15, 48, {}, version, 1, false);
	const uint32_t bytes = 12;
	std::memcpy (second.data () + second.size () - 10, &bytes, sizeof (bytes));
	for (uint16_t index : { 0, 1, 2 }) appendValue (second, index);
	data.insert (data.end (), second.begin () + 21, second.end ());
	appendTail (1, 3);
	const auto mesh = MdlParser::parse (data, "per-submesh-tail.mdl");
	REQUIRE (mesh.submeshes.size () == 2);
	CHECK (mesh.submeshes[0].indices.size () == 3);
	CHECK (mesh.submeshes[1].indices.size () == 6);
	if (version < 21) {
	    CHECK (mesh.auxiliaryPositions.empty ());
	    CHECK (mesh.submeshes[1].drawRanges.empty ());
	    continue;
	}
	CHECK (mesh.auxiliaryHeader == 1);
	REQUIRE (mesh.auxiliaryPositions.size () == 1);
	CHECK (mesh.auxiliaryPositions[0].x == 1);
	REQUIRE (mesh.drawRanges.size () == 2);
	CHECK (mesh.drawRanges[0].partId == 20);
	for (uint32_t part = 0; part < 2; ++part) {
	    const auto& submesh = mesh.submeshes[part];
	    CHECK (submesh.auxiliaryHeader == part + 1);
	    REQUIRE (submesh.auxiliaryPositions.size () == 1);
	    CHECK (submesh.auxiliaryPositions[0].x == part + 1);
	    REQUIRE (submesh.drawRanges.size () == 2);
	    CHECK (submesh.drawRanges[1].submeshIndex == part);
	    CHECK (submesh.drawRanges[1].partId == 21 + part * 10);
	    CHECK (submesh.drawRanges[1].depthOffset == -50 - static_cast<int32_t> (part));
	    CHECK (submesh.drawRanges[1].firstIndex == (part == 0 ? 0 : 3));
	    REQUIRE (submesh.clippingDescriptors.size () == (version == 23 ? 1 : 0));
	    if (version == 23) {
		CHECK (submesh.clippingDescriptors[0].opaqueId == 100 + part);
		CHECK (submesh.clippingDescriptors[0].targets == std::vector<uint32_t> { 1 });
		CHECK (submesh.clippingDescriptors[0].sources == std::vector<uint32_t> { 0 });
	    }
	}
	REQUIRE (mesh.clippingDescriptors.size () == (version == 23 ? 1 : 0));
	if (version == 23) {
	    CHECK (mesh.clippingDescriptors[0].opaqueId == 100);
	    auto invalidReference = data;
	    const uint32_t outsideLocalRanges = 2;
	    std::memcpy (invalidReference.data () + invalidReference.size () - 12,
		&outsideLocalRanges, sizeof (outsideLocalRanges));
	    CHECK_THROWS (MdlParser::parse (invalidReference, "nonlocal-range-reference.mdl"));
	}
	// Reject a truncated required field in the second submesh's tail.
	data.pop_back ();
	CHECK_THROWS (MdlParser::parse (data, "truncated-second-tail.mdl"));
    }
}
