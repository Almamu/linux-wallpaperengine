#include "WallpaperEngine/Data/Model/DynamicValue.h"
#include "WallpaperEngine/Data/Assets/Texture.h"
#include "WallpaperEngine/Data/Parsers/TextureParser.h"
#include "WallpaperEngine/Data/Utils/BinaryReader.h"

#include "WallpaperEngine/Data/Model/Property.h"
#include "WallpaperEngine/Data/Parsers/PropertyParser.h"
#include "TextureJpegFixture.h"
#include <lz4.h>
#include <stb_image_write.h>

#ifdef CHECK
#undef CHECK
#endif
#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstdlib>
#include <fstream>
#include <sstream>

using WallpaperEngine::Data::Assets::FIF_PNG;
using WallpaperEngine::Data::Assets::Mipmap;
using WallpaperEngine::Data::Assets::Texture;
using WallpaperEngine::Data::Assets::TextureFlags_ClampUVs;
using WallpaperEngine::Data::Assets::TextureFlags_Video;
using WallpaperEngine::Data::Assets::TextureFormat_ARGB8888;
using WallpaperEngine::Data::Parsers::TextureParser;
using WallpaperEngine::Data::Utils::BinaryReader;

namespace {
template <typename T> void appendValue (std::string& output, const T value) {
    output.append (reinterpret_cast<const char*> (&value), sizeof (value));
}

/** A PNG of the given size, with pixels varying per position so a mixed-up decode shows up */
std::string encodePng (const int size, const int seed) {
    std::vector<unsigned char> pixels (static_cast<size_t> (size) * size * 4);
    for (size_t index = 0; index < pixels.size (); index++) {
	pixels[index] = static_cast<unsigned char> ((index * 7 + seed * 31) % 251);
    }

    std::string encoded;
    stbi_write_png_to_func (
	[] (void* context, void* data, int length) {
	    static_cast<std::string*> (context)->append (static_cast<const char*> (data), length);
	},
	&encoded, size, size, 4, pixels.data (), size * 4
    );

    return encoded;
}

/** One texture holding mipmapCount PNG mipmaps, halving in size like a real chain */
std::unique_ptr<Texture> makePngTexture (const int mipmapCount, const int seed) {
    auto texture = std::make_unique<Texture> ();
    texture->freeImageFormat = FIF_PNG;
    texture->imageCount = 1;

    WallpaperEngine::Data::Assets::MipmapList mipmaps;
    for (int level = 0; level < mipmapCount; level++) {
	const int size = 64 >> level;
	const std::string encoded = encodePng (size, seed + level);

	auto mipmap = std::make_shared<Mipmap> ();
	mipmap->width = size;
	mipmap->height = size;
	mipmap->uncompressedSize = static_cast<int> (encoded.size ());
	mipmap->uncompressedData = std::unique_ptr<char[]> (new char[encoded.size ()]);
	std::memcpy (mipmap->uncompressedData.get (), encoded.data (), encoded.size ());
	mipmaps.emplace_back (std::move (mipmap));
    }

    texture->images.emplace (0, std::move (mipmaps));

    return texture;
}
} // namespace

TEST_CASE ("compiled JPEG mips match native top-down RGBA", "[texture][jpeg]") {
    using namespace WallpaperEngine::Data::Assets;
    using TextureJpegFixture::width;
    using TextureJpegFixture::height;
    const auto& jpeg = TextureJpegFixture::jpeg;
    const auto& native = TextureJpegFixture::nativeRgba;
    static_assert (sizeof (native) == width * height * 4);

    std::string bytes ("TEXV0005", 9);
    bytes.append ("TEXI0001", 9);
    for (uint32_t value : { uint32_t (TextureFormat_ARGB8888), uint32_t (TextureFlags_ClampUVs),
			   width, height, width, height, 0u }) appendValue (bytes, value);
    bytes.append ("TEXB0003", 9);
    for (uint32_t value : { 1u, uint32_t (FIF_JPEG), 1u, width, height, 0u,
			   uint32_t (sizeof (jpeg)), uint32_t (sizeof (jpeg)) }) appendValue (bytes, value);
    bytes.append (reinterpret_cast<const char*> (jpeg), sizeof (jpeg));
    const auto parseTexture = [&] {
	return TextureParser::parse (BinaryReader (std::make_shared<std::istringstream> (bytes, std::ios::binary)));
    };
    const auto single = parseTexture ();
    const auto first = parseTexture ();
    const auto second = parseTexture ();
    REQUIRE_FALSE (single->images.at (0).front ()->decodedData);
    TextureParser::decodeMipmaps (*single);
    TextureParser::decodeMipmaps (std::vector<Texture*> { first.get (), second.get () });
    for (const auto* texture : { single.get (), first.get (), second.get () }) {
	CHECK (texture->freeImageFormat == FIF_JPEG);
	CHECK (texture->flags == TextureFlags_ClampUVs);
	const auto& mipmap = *texture->images.at (0).front ();
	REQUIRE (mipmap.decodedData);
	CHECK (mipmap.decodedWidth == width);
	CHECK (mipmap.decodedHeight == height);
	CHECK (std::memcmp (mipmap.decodedData.get (), native, sizeof (native)) == 0);
    }
}

TEST_CASE ("JPEGs outside the compiled decoder scope retain the previous pixels", "[texture][jpeg]") {
    using namespace WallpaperEngine::Data::Assets;
    const auto& jpeg = TextureJpegFixture::jpeg;
    const std::string bytes (reinterpret_cast<const char*> (jpeg), sizeof (jpeg));
    const auto source = TextureParser::parseImage (bytes, { { "nomip", true } });
    const auto& expected = *source->images.at (0).front ();

    FIF format = FIF_JPEG;
    auto payload = bytes;
    SECTION ("not declared as JPEG") { format = FIF_PNG; }
    SECTION ("APP1 EXIF orientation") {
	payload.insert (2, "\xff\xe1\x00\x22" "Exif\0\0" "II\x2a\0\x08\0\0\0"
	    "\x01\0\x12\x01\x03\0\x01\0\0\0\x06\0\0\0\0\0\0\0", 36);
    }
    SECTION ("APP2 ICC marker") { payload.insert (2, "\xff\xe2\x00\x10" "ICC_PROFILE\0\x01\x01", 18); }
    int width = 0, height = 0;
    const auto pixels = TextureParser::decodeCompiledImage (format, payload.data (), payload.size (), width, height);
    REQUIRE (pixels);
    REQUIRE (width == TextureJpegFixture::width);
    REQUIRE (height == TextureJpegFixture::height);
    CHECK (std::memcmp (pixels.get (), expected.uncompressedData.get (), expected.uncompressedSize) == 0);
}

TEST_CASE ("truncated compiled JPEGs leave no decoded pixels", "[texture][jpeg]") {
    constexpr char truncated[] = "\xff\xd8\xff\xe0\0\x10" "JFIF";
    int width = 0, height = 0;
    CHECK_FALSE (TextureParser::decodeCompiledImage (WallpaperEngine::Data::Assets::FIF_JPEG,
	truncated, sizeof (truncated) - 1, width, height));
}

TEST_CASE ("legacy TEXV0004 preserves texture metadata and every mip payload", "[texture][legacy]") {
    using namespace WallpaperEngine::Data::Assets;
    for (const auto format : { TextureFormat_DXT5, TextureFormat_ARGB8888 }) {
	CAPTURE (format);
	std::string legacy ("TEXV0004", 9);
	const uint32_t flags = TextureFlags_ClampUVs | TextureFlags_NoInterpolation;
	for (const uint32_t value : { static_cast<uint32_t> (format), flags, 8u, 8u, 6u, 5u }) {
	    appendValue (legacy, value);
	}
	appendValue (legacy, 3u); // One implicit image, followed directly by its mip count.
	std::vector<std::string> payloads;
	for (const uint32_t size : { 8u, 4u, 2u }) {
	    const uint32_t bytes = format == TextureFormat_DXT5 ? ((size + 3) / 4) * ((size + 3) / 4) * 16
							      : size * size * 4;
	    payloads.emplace_back (bytes, static_cast<char> (size));
	    for (const uint32_t value : { size, size, bytes }) {
		appendValue (legacy, value);
	    }
	    legacy += payloads.back ();
	}
	// The equivalent version-5 file adds section headers, an extra info field,
	// and an image count. The shared mip records must decode identically.
	std::string modern ("TEXV0005", 9);
	modern.append ("TEXI0001", 9);
	modern.append (legacy, 9, 24);
	appendValue (modern, 0u);
	modern.append ("TEXB0001", 9);
	appendValue (modern, 1u);
	modern.append (legacy, 33, std::string::npos);
	for (const auto& bytes : { legacy, modern }) {
	    const auto stream = std::make_shared<std::istringstream> (bytes, std::ios::binary);
	    const auto texture = TextureParser::parse (BinaryReader (stream));
	    CHECK (texture->format == format);
	    CHECK (texture->flags == flags);
	    CHECK (texture->textureWidth == 8);
	    CHECK (texture->textureHeight == 8);
	    CHECK (texture->width == 6);
	    CHECK (texture->height == 5);
	    CHECK (texture->freeImageFormat == FIF_UNKNOWN);
	    CHECK (texture->imageCount == 1);
	    REQUIRE (texture->images.size () == 1);
	    const auto& mips = texture->images.at (0);
	    REQUIRE (mips.size () == payloads.size ());
	    for (size_t level = 0; level < mips.size (); ++level) {
		CHECK (mips[level]->width == (8u >> level));
		CHECK (mips[level]->height == (8u >> level));
		CHECK (mips[level]->compression == 0);
		CHECK (mips[level]->uncompressedSize == static_cast<int> (payloads[level].size ()));
		REQUIRE (mips[level]->uncompressedData != nullptr);
		CHECK (std::string (mips[level]->uncompressedData.get (), mips[level]->uncompressedSize) == payloads[level]);
	    }
	    CHECK (stream->tellg () == static_cast<std::streamoff> (bytes.size ()));
	}
    }
}

TEST_CASE ("legacy texture parsing rejects incomplete headers and payloads", "[texture][legacy]") {
    std::string bytes ("TEXV0004", 9);
    for (const uint32_t value : { static_cast<uint32_t> (TextureFormat_ARGB8888), 0u, 1u, 1u, 1u, 1u,
				 1u, 1u, 1u, 4u }) {
	appendValue (bytes, value);
    }
    bytes += "rgba";
    for (size_t length = 0; length < bytes.size (); ++length) {
	CAPTURE (length);
	const auto stream = std::make_shared<std::istringstream> (bytes.substr (0, length), std::ios::binary);
	CHECK_THROWS (TextureParser::parse (BinaryReader (stream)));
    }
    for (const uint32_t count : { 0u, 33u }) {
	auto invalid = bytes;
	std::memcpy (invalid.data () + 33, &count, sizeof (count));
	const auto stream = std::make_shared<std::istringstream> (invalid, std::ios::binary);
	CHECK_THROWS (TextureParser::parse (BinaryReader (stream)));
    }
    for (const char version : { '3', '6' }) {
	auto unsupported = bytes;
	unsupported[7] = version;
	const auto stream = std::make_shared<std::istringstream> (unsupported, std::ios::binary);
	CHECK_THROWS (TextureParser::parse (BinaryReader (stream)));
    }
}

TEST_CASE ("TEXB0004 video mipmaps preserve their embedded MP4 payload", "[texture][video]") {
    constexpr uint32_t width = 2324;
    constexpr uint32_t height = 2474;
    const std::string mp4 = std::string ("\0\0\0\x18" "ftypmp42\0\0\0\0mp42mp41", 24);

    std::string bytes;
    bytes.append ("TEXV0005", 9);
    bytes.append ("TEXI0001", 9);
    appendValue<uint32_t> (bytes, TextureFormat_ARGB8888);
    appendValue<uint32_t> (bytes, TextureFlags_Video | TextureFlags_ClampUVs);
    appendValue<uint32_t> (bytes, width);
    appendValue<uint32_t> (bytes, height);
    appendValue<uint32_t> (bytes, width);
    appendValue<uint32_t> (bytes, height);
    appendValue<uint32_t> (bytes, 0);
    bytes.append ("TEXB0004", 9);
    appendValue<uint32_t> (bytes, 1); // image count
    appendValue<uint32_t> (bytes, UINT32_MAX); // FIF_UNKNOWN
    appendValue<uint32_t> (bytes, 0); // no conditional images
    appendValue<uint32_t> (bytes, 1); // mip count
    appendValue<uint32_t> (bytes, width);
    appendValue<uint32_t> (bytes, height);
    appendValue<uint32_t> (bytes, 0); // compression
    appendValue<int> (bytes, 0); // video containers leave the uncompressed size empty
    appendValue<int> (bytes, static_cast<int> (mp4.size ()));
    bytes.append (mp4);

    const auto stream = std::make_shared<std::istringstream> (bytes, std::ios::in | std::ios::binary);
    const auto texture = TextureParser::parse (BinaryReader (stream));

    REQUIRE (texture->images.size () == 1);
    REQUIRE (texture->images.at (0).size () == 1);
    const auto& mipmap = texture->images.at (0).front ();
    CHECK (mipmap->width == width);
    CHECK (mipmap->height == height);
    CHECK (mipmap->compression == 0);
    CHECK (mipmap->compressedSize == static_cast<int> (mp4.size ()));
    CHECK (mipmap->uncompressedSize == static_cast<int> (mp4.size ()));
    REQUIRE (mipmap->uncompressedData != nullptr);
    CHECK (std::memcmp (mipmap->uncompressedData.get (), mp4.data (), mp4.size ()) == 0);
}

TEST_CASE ("batched mipmap decoding matches decoding one texture at a time", "[texture][decode]") {
    // enough textures and mipmaps to hand every thread of the pool some work
    constexpr int textureCount = 12;

    std::vector<std::unique_ptr<Texture>> serial;
    std::vector<std::unique_ptr<Texture>> batched;
    std::vector<Texture*> batch;

    for (int index = 0; index < textureCount; index++) {
	const int mipmapCount = 1 + index % 4;
	serial.emplace_back (makePngTexture (mipmapCount, index));
	batched.emplace_back (makePngTexture (mipmapCount, index));
	batch.emplace_back (batched.back ().get ());
    }

    for (const auto& texture : serial) {
	TextureParser::decodeMipmaps (*texture);
    }

    TextureParser::decodeMipmaps (batch);

    for (int index = 0; index < textureCount; index++) {
	const auto& expected = serial[index]->images.at (0);
	const auto& actual = batched[index]->images.at (0);

	REQUIRE (actual.size () == expected.size ());

	for (size_t level = 0; level < expected.size (); level++) {
	    REQUIRE (expected[level]->decodedData != nullptr);
	    REQUIRE (actual[level]->decodedData != nullptr);
	    CHECK (actual[level]->decodedWidth == expected[level]->decodedWidth);
	    CHECK (actual[level]->decodedHeight == expected[level]->decodedHeight);

	    const size_t bytes = static_cast<size_t> (expected[level]->decodedWidth)
		* expected[level]->decodedHeight * 4;
	    CHECK (std::memcmp (actual[level]->decodedData.get (), expected[level]->decodedData.get (), bytes) == 0);
	}
    }
}

namespace {
using namespace WallpaperEngine::Data::Assets;
using WallpaperEngine::Data::Model::Properties;

std::string variantHeader (uint32_t format, int fif, const std::vector<TextureVariant>& variants, uint32_t images = 1) {
    std::string bytes;
    bytes.append ("TEXV0005", 9);
    bytes.append ("TEXI0001", 9);
    for (uint32_t value : { format, 0u, 8u, 8u, 8u, 8u, 0u }) {
	appendValue (bytes, value);
    }
    bytes.append ("TEXB0004", 9);
    appendValue (bytes, images);
    appendValue (bytes, fif);
    appendValue (bytes, static_cast<uint32_t> (variants.size ()));
    for (const auto& variant : variants) {
	appendValue (bytes, variant.group);
	appendValue (bytes, variant.id);
	appendValue (bytes, variant.flags);
	bytes += WallpaperEngine::Data::JSON::JSON { { "condition", variant.condition } }.dump ();
	bytes += '\0';
    }
    return bytes;
}

std::string lz4 (const std::string& bytes) {
    std::string result (LZ4_compressBound (static_cast<int> (bytes.size ())), '\0');
    const int size = LZ4_compress_default (
	bytes.data (), result.data (), static_cast<int> (bytes.size ()), static_cast<int> (result.size ())
    );
    REQUIRE (size > 0);
    result.resize (size);
    return result;
}

void appendMip (std::string& bytes, uint32_t size, const std::string& pixels, bool compressed = false) {
    const auto payload = compressed ? lz4 (pixels) : pixels;
    for (uint32_t value : { size, size, compressed ? 1u : 0u, static_cast<uint32_t> (pixels.size ()),
			    static_cast<uint32_t> (payload.size ()) }) {
	appendValue (bytes, value);
    }
    bytes += payload;
}

void appendPatch (
    std::string& bytes, uint32_t id, uint32_t x, uint32_t y, uint32_t width, uint32_t height, const std::string& payload
) {
    for (uint32_t value : { 1u, id, x, y, width, height, 13u, static_cast<uint32_t> (payload.size ()) }) {
	appendValue (bytes, value);
    }
    bytes += payload;
}

std::unique_ptr<Texture> parseVariants (const std::string& bytes, const TextureParser::VariantSelector& select = {}) {
    return TextureParser::parse (BinaryReader (std::make_shared<std::istringstream> (bytes, std::ios::binary)), select);
}
} // namespace

TEST_CASE ("TEXB4 interleaves conditional patches after every image mip", "[texture][variants]") {
    const std::vector<TextureVariant> variants { { 1, 17, 0, "snow" }, { 1, 29, 0, "night" }, { 2, 45, 0, "detail" } };
    auto bytes = variantHeader (TextureFormat_ARGB8888, FIF_UNKNOWN, variants, 2);
    for (uint32_t image = 0; image < 2; ++image) {
	appendValue (bytes, 2u);
	for (uint32_t size : { 8u, 4u }) {
	    appendMip (bytes, size, std::string (size * size * 4, 'b'));
	    appendValue (bytes, 2u); // groups
	    appendValue (bytes, 2u); // patches in first group
	    appendPatch (bytes, 17, 0, 0, 2, 2, std::string (16, 's'));
	    appendPatch (bytes, 29, 0, 0, size, size, std::string (size * size * 4, 'n'));
	    appendValue (bytes, 1u);
	    appendPatch (bytes, 45, 2, 2, 1, 1, "dddd");
	}
    }
    const auto base = parseVariants (bytes);
    REQUIRE (base->containerVersion == ContainerVersion_TEXB0004);
    REQUIRE (base->images.size () == 2);
    const auto selected = parseVariants (bytes, [] (const auto&) { return true; });
    CHECK (
	TextureParser::selectedVariants (*selected, [] (const auto&) { return true; })
	== std::vector<uint32_t> { 17, 45 }
    );
    for (uint32_t image = 0; image < 2; ++image) {
	REQUIRE (base->images.at (image).size () == 2);
	REQUIRE (selected->images.at (image).size () == 2);
	for (const auto& mip : selected->images.at (image)) {
	    CHECK (mip->uncompressedData[0] == 's');
	    CHECK (mip->uncompressedData[mip->width * 4] == 's');
	    CHECK (mip->uncompressedData[(2 * mip->width + 2) * 4] == 'd');
	    CHECK (mip->uncompressedData[3 * 4] == 'b');
	}
	CHECK (base->images.at (image).back ()->uncompressedData[0] == 'b');
    }
    CHECK_THROWS (parseVariants (bytes.substr (0, bytes.size () - 1)));
    CHECK_THROWS (parseVariants (bytes.substr (0, 75)));
}

TEST_CASE ("TEXB4 decompresses block-aligned partial DXT patches", "[texture][variants]") {
    auto bytes = variantHeader (TextureFormat_DXT5, FIF_UNKNOWN, { { 1, 1, 0, "alternate" } });
    appendValue (bytes, 2u);
    appendMip (bytes, 8, std::string (64, 'b'), true);
    appendValue (bytes, 1u);
    appendValue (bytes, 1u);
    appendPatch (bytes, 1, 4, 4, 4, 4, lz4 (std::string (16, 'v')));
    appendMip (bytes, 4, std::string (16, 'c'), true);
    appendValue (bytes, 1u);
    appendValue (bytes, 1u);
    appendPatch (bytes, 1, 0, 0, 4, 4, lz4 (std::string (16, 'w')));
    const auto texture = parseVariants (bytes, [] (const auto&) { return true; });
    REQUIRE (texture->images.at (0).size () == 2);
    const auto& first = texture->images.at (0).front ();
    CHECK (std::string (first->uncompressedData.get (), 48) == std::string (48, 'b'));
    CHECK (std::string (first->uncompressedData.get () + 48, 16) == std::string (16, 'v'));
    CHECK (std::string (texture->images.at (0).back ()->uncompressedData.get (), 16) == std::string (16, 'w'));
}

TEST_CASE ("TEXB4 patches decoded PNG pixels without dropping smaller levels", "[texture][variants]") {
    auto bytes = variantHeader (TextureFormat_DXT5, FIF_PNG, { { 1, 1, 0, "alternate" } });
    appendValue (bytes, 2u);
    for (uint32_t size : { 8u, 4u }) {
	appendMip (bytes, size, encodePng (size, 1));
	appendValue (bytes, 1u);
	appendValue (bytes, 1u);
	appendPatch (bytes, 1, 1, 1, 2, 2, encodePng (2, 2));
    }
    const auto texture = parseVariants (bytes, [] (const auto&) { return true; });
    REQUIRE (texture->images.at (0).size () == 2);
    for (const auto& mip : texture->images.at (0)) {
	REQUIRE (mip->decodedData != nullptr);
	CHECK (mip->decodedData[0] == 31);
	CHECK (mip->decodedData[(mip->width + 1) * 4] == 62);
	CHECK (mip->decodedWidth == static_cast<int> (mip->width));
    }
}

TEST_CASE ("texture variants follow combo and boolean properties independently", "[texture][variants]") {
    using namespace WallpaperEngine::Data::Model;
    const auto combo = WallpaperEngine::Data::Parsers::PropertyParser::parse (
	{ { "type", "combo" },
	  { "value", "0" },
	  { "options", { { { "value", "0" }, { "label", "Base" } }, { { "value", "1" }, { "label", "Alt" } } } } },
	"style"
    );
    const auto boolean
	= WallpaperEngine::Data::Parsers::PropertyParser::parse ({ { "type", "bool" }, { "value", false } }, "enabled");
    const Properties properties { { "style", combo }, { "enabled", boolean } };
    const WallpaperEngine::Data::JSON::JSON condition { { "name", "style" }, { "condition", "1" } };
    CHECK_FALSE (TextureParser::matchesCondition (condition, properties));
    combo->update (std::string ("1"), DynamicValue::UpdateSource::User);
    CHECK (TextureParser::matchesCondition (condition, properties));
    CHECK_FALSE (TextureParser::matchesCondition ("enabled", properties));
    boolean->update (std::string ("true"), DynamicValue::UpdateSource::User);
    CHECK (TextureParser::matchesCondition ("enabled", properties));
    CHECK_FALSE (TextureParser::matchesCondition (condition, {}));
    CHECK_FALSE (TextureParser::matchesCondition ("missing", properties));
}

TEST_CASE ("source textures need metadata and compiled previews stay texture-only", "[texture][assets]") {
    using WallpaperEngine::Assets::AssetLocator;
    using WallpaperEngine::FileSystem::Container;
    auto files = std::make_unique<Container> ();
    auto& vfs = files->getVFS ();
    vfs.add ("materials/effects/source.png", encodePng (8, 1));
    vfs.add ("materials/effects/source.tex-json", R"({"format":"rgba8888","nomip":true})");
    const AssetLocator locator (std::move (files));
    const auto texture = TextureParser::load (locator, "effects/source");
    REQUIRE (texture->images.at (0).size () == 1);
    CHECK (texture->width == 8);
    CHECK (texture->format == TextureFormat_ARGB8888);
    CHECK (texture->images.at (0).front ()->uncompressedData[0] == 31);

    auto previews = std::make_unique<Container> ();
    previews->getVFS ().add ("effects/example/preview/materials/effects/only.tex", "compiled");
    previews->getVFS ().add ("effects/example/preview/scene.json", "preview scene");
    previews->getVFS ().add ("effects/example/preview/materials/collision.json", "preview material");
    const AssetLocator fallback (std::move (previews), { "effects/example/preview/materials" });
    const auto stream = fallback.texture ("effects/only");
    CHECK (std::string (std::istreambuf_iterator<char> (*stream), {}) == "compiled");
    CHECK_THROWS (fallback.readString ("scene.json"));
    CHECK_THROWS (fallback.readString ("materials/collision.json"));
    CHECK_THROWS (fallback.texture ("../scene.json"));
}

TEST_CASE ("source texture imports preserve channels, sampling flags and compiled precedence", "[texture][assets]") {
    using WallpaperEngine::Assets::AssetLocator;
    using WallpaperEngine::FileSystem::Container;
    for (const auto& [format, components] : std::map<std::string, size_t> {{"rgba8888", 4}, {"rg88n", 2}, {"r8", 1}}) {
	INFO (format);
	auto files = std::make_unique<Container> ();
	files->getVFS ().add ("materials/source.png", encodePng (8, 1));
	files->getVFS ().add ("materials/source.tex-json", WallpaperEngine::Data::JSON::JSON {
	    {"format", format}, {"nomip", false}, {"clampuvs", true}, {"nointerpolation", true}
	});
	const AssetLocator locator (std::move (files));
	const auto texture = TextureParser::load (locator, "source");
	REQUIRE (texture->images.at (0).size () == 4);
	CHECK (texture->images.at (0).back ()->width == 1);
	CHECK (texture->images.at (0).back ()->uncompressedSize == static_cast<int> (components));
	CHECK ((texture->flags & TextureFlags_ClampUVs) != 0);
	CHECK ((texture->flags & TextureFlags_NoInterpolation) != 0);
	CHECK (texture->images.at (0).front ()->uncompressedData[components] == 59);
    }
    auto missing = std::make_unique<Container> ();
    missing->getVFS ().add ("materials/source.png", encodePng (8, 1));
    CHECK_THROWS (TextureParser::load (AssetLocator (std::move (missing)), "source"));

    auto corrupt = std::make_unique<Container> ();
    corrupt->getVFS ().add ("materials/source.tex", "broken compiled texture");
    corrupt->getVFS ().add ("materials/source.png", encodePng (8, 1));
    corrupt->getVFS ().add ("materials/source.tex-json", R"({"format":"rgba8888","nomip":true})");
    CHECK_THROWS (TextureParser::load (AssetLocator (std::move (corrupt)), "source"));
}

TEST_CASE ("raw user images preserve pixels and use explicit single-level sampling", "[texture][user-image]") {
    using namespace WallpaperEngine::Data::Assets;
    using Json = WallpaperEngine::Data::JSON::JSON;
    const auto texture = TextureParser::parseImage (encodePng (8, 1), Json {
	{"nomip", true}, {"nointerpolation", true}, {"clampuvs", true}
    });
    REQUIRE (texture->images.at (0).size () == 1);
    CHECK (texture->width == 8);
    CHECK (texture->height == 8);
    CHECK (texture->flags == (TextureFlags_NoInterpolation | TextureFlags_ClampUVs));
    CHECK (texture->images.at (0).front ()->uncompressedSize == 8 * 8 * 4);
    CHECK (texture->images.at (0).front ()->uncompressedData[0] == 31);
    CHECK (texture->images.at (0).front ()->uncompressedData[4] == 59);
    CHECK_THROWS (TextureParser::parseImage ("not an image", Json {{"nomip", true}}));
}

TEST_CASE ("installed TEXB4 corpus decodes every authored conditional mip chain", "[.][texture-corpus]") {
    const char* root = std::getenv ("LWE_TEXTURE_CORPUS");
    REQUIRE (root != nullptr);
    size_t textures = 0, variants = 0, mipmaps = 0;
    for (const auto& entry : std::filesystem::recursive_directory_iterator (root)) {
	if (entry.path ().extension () != ".tex") {
	    continue;
	}
	std::ifstream input (entry.path (), std::ios::binary);
	const std::string bytes { std::istreambuf_iterator<char> (input), {} };
	if (bytes.size () < 67 || bytes.substr (46, 9) != std::string ("TEXB0004", 9)) {
	    continue;
	}
	INFO (entry.path ().string ());
	auto base = parseVariants (bytes);
	++textures;
	for (const auto& [image, levels] : base->images) {
	    mipmaps += levels.size ();
	}
	for (const auto& variant : base->variants) {
	    ++variants;
	    const auto selected
		= parseVariants (bytes, [&variant] (const auto& condition) { return condition == variant.condition; });
	    REQUIRE (selected->images.size () == base->images.size ());
	    for (const auto& [image, levels] : base->images) {
		REQUIRE (selected->images.at (image).size () == levels.size ());
		for (size_t level = 0; level < levels.size (); ++level) {
		    CHECK (selected->images.at (image)[level]->width == levels[level]->width);
		    CHECK (selected->images.at (image)[level]->height == levels[level]->height);
		}
	    }
	}
    }
    std::cout << "TEXB4 corpus: " << textures << " textures, " << variants << " variants, " << mipmaps << " mipmaps\n";
    REQUIRE (variants > 0);
}

TEST_CASE ("selected GIFs retain animated logical canvases and frame delays", "[texture][user-image][gif]") {
    using namespace WallpaperEngine::Data::Assets;
    using Json = WallpaperEngine::Data::JSON::JSON;
    // Hand-authored 12x8 GIF: offset red; green restoring previous; blue
    // clearing to background; yellow. Every frame lasts 800 ms.
    const char bytes[]
	= "\x47\x49\x46\x38\x39\x61\x0c\x00\x08\x00\xf2\x00\x00\x00\x00\x00\xff\x00\x00\x00\xff\x00\x00\x00"
	  "\xff\xff\xff\x00\x11\x33\xef\x00\x00\x00\x00\x00\x00\x21\xff\x0b\x4e\x45\x54\x53\x43\x41\x50\x45"
	  "\x32\x2e\x30\x03\x01\x00\x00\x00\x21\xf9\x04\x05\x50\x00\x00\x00\x2c\x02\x00\x01\x00\x02\x00\x02"
	  "\x00\x00\x03\x05\x18\x18\x18\x18\x09\x00\x21\xf9\x04\x0d\x50\x00\x00\x00\x2c\x02\x00\x01\x00\x02"
	  "\x00\x02\x00\x00\x03\x05\x28\x28\x28\x28\x09\x00\x21\xf9\x04\x09\x50\x00\x00\x00\x2c\x08\x00\x04"
	  "\x00\x02\x00\x02\x00\x00\x03\x05\x38\x38\x38\x38\x09\x00\x21\xf9\x04\x05\x50\x00\x00\x00\x2c\x05"
	  "\x00\x03\x00\x01\x00\x01\x00\x00\x03\x02\x48\x09\x00\x3b";
    const auto texture = TextureParser::parseImage (
	std::string (bytes, sizeof (bytes) - 1),
	Json { { "nomip", true }, { "nointerpolation", true }, { "clampuvs", true } }
    );
    REQUIRE (texture->isAnimated ());
    REQUIRE (texture->imageCount == 4);
    REQUIRE (texture->frames.size () == 4);
    CHECK (texture->width == 12);
    CHECK (texture->height == 8);
    CHECK (texture->textureWidth == 12);
    CHECK (texture->textureHeight == 8);
    CHECK (texture->gifWidth == 12);
    CHECK (texture->gifHeight == 8);
    CHECK (texture->flags == (TextureFlags_IsGif | TextureFlags_NoInterpolation | TextureFlags_ClampUVs));
    for (uint32_t index = 0; index < 4; ++index) {
	CAPTURE (index);
	const auto& frame = texture->frames[index];
	CHECK (frame->frameNumber == index);
	CHECK (frame->frametime == 0.8f);
	CHECK (frame->x == 0);
	CHECK (frame->y == 0);
	CHECK (frame->width1 == 12);
	CHECK (frame->height1 == 8);
	CHECK (frame->width2 == 0);
	CHECK (frame->height2 == 0);
	REQUIRE (texture->images.at (index).size () == 1);
	const auto& mip = texture->images.at (index).front ();
	REQUIRE (mip->uncompressedSize == 12 * 8 * 4);
	for (int y = 0; y < 8; ++y) {
	    for (int x = 0; x < 12; ++x) {
		CAPTURE (x, y);
		unsigned char expected[4] = { 0, 0, 0, 0 };
		if (x >= 2 && x < 4 && y >= 1 && y < 3) {
		    expected[index == 1 ? 1 : 0] = 255;
		    expected[3] = 255;
		} else if (index == 2 && x >= 8 && x < 10 && y >= 4 && y < 6) {
		    expected[2] = expected[3] = 255;
		} else if (index == 3 && x == 5 && y == 3) {
		    expected[0] = expected[1] = expected[3] = 255;
		}
		for (int channel = 0; channel < 4; ++channel) {
		    CHECK (
			static_cast<unsigned char> (mip->uncompressedData[(y * 12 + x) * 4 + channel])
			== expected[channel]
		    );
		}
	    }
	}
    }
    // A zero delay advances on the next render update; preserve it for playback
    // instead of inventing a 100 ms duration in the decoder.
    std::string zeroDelay (bytes, sizeof (bytes) - 1);
    for (size_t frame = 0; frame < 4; ++frame) {
	zeroDelay[60 + frame * 26] = 0;
    }
    const auto zeroTexture = TextureParser::parseImage (zeroDelay, Json { { "nomip", true } });
    REQUIRE (zeroTexture->frames.size () == 4);
    for (const auto& frame : zeroTexture->frames) CHECK (frame->frametime == 0.0f);

    // Variable authored delays and source metadata must survive the animation path.
    std::string varied (bytes, sizeof (bytes) - 1);
    for (size_t frame = 0; frame < 4; ++frame) {
	varied[60 + frame * 26] = static_cast<char> (12 + frame * 22);
    }
    const auto converted = TextureParser::parseImage (varied, Json { { "format", "r8" }, { "nomip", false } });
    REQUIRE (converted->frames.size () == 4);
    const float durations[] = { 0.12f, 0.34f, 0.56f, 0.78f };
    for (uint32_t frame = 0; frame < 4; ++frame) {
	CAPTURE (frame);
	CHECK (converted->frames[frame]->frametime == durations[frame]);
	REQUIRE (converted->images.at (frame).size () == 4);
	CHECK (converted->images.at (frame).front ()->uncompressedSize == 12 * 8);
	CHECK (converted->images.at (frame).back ()->uncompressedSize == 1);
	CHECK (
	    static_cast<unsigned char> (converted->images.at (frame).front ()->uncompressedData[14])
	    == (frame == 1 ? 0 : 255)
	);
    }
    CHECK (converted->format == TextureFormat_R8);
    CHECK_THROWS (TextureParser::parseImage ("GIF89a", Json::object ()));
    std::string oversized (bytes, sizeof (bytes) - 1);
    oversized.replace (6, 4, 4, static_cast<char> (255));
    CHECK_THROWS (TextureParser::parseImage (oversized, Json::object ()));
}

TEST_CASE ("selected GIFs distinguish clearing an overwritten patch from restoring it", "[texture][user-image][gif]") {
    using Json = WallpaperEngine::Data::JSON::JSON;
    // Both GIFs overwrite the existing red patch with green, then draw yellow
    // elsewhere. The old patch must be cleared by disposal2, restored by disposal3.
    const char clearBytes[]
	= "\x47\x49\x46\x38\x39\x61\x0c\x00\x08\x00\xf2\x00\x00\x00\x00\x00\xff\x00\x00\x00\xff\x00\x00\x00"
	  "\xff\xff\xff\x00\x11\x33\xef\x00\x00\x00\x00\x00\x00\x21\xff\x0b\x4e\x45\x54\x53\x43\x41\x50\x45"
	  "\x32\x2e\x30\x03\x01\x00\x00\x00\x21\xf9\x04\x05\x50\x00\x00\x00\x2c\x02\x00\x01\x00\x02\x00\x02"
	  "\x00\x00\x03\x05\x18\x18\x18\x18\x09\x00\x21\xf9\x04\x09\x50\x00\x00\x00\x2c\x02\x00\x01\x00\x02"
	  "\x00\x02\x00\x00\x03\x05\x28\x28\x28\x28\x09\x00\x21\xf9\x04\x05\x50\x00\x00\x00\x2c\x05\x00\x03"
	  "\x00\x01\x00\x01\x00\x00\x03\x02\x48\x09\x00\x3b";
    const char restoreBytes[]
	= "\x47\x49\x46\x38\x39\x61\x0c\x00\x08\x00\xf2\x00\x00\x00\x00\x00\xff\x00\x00\x00\xff\x00\x00\x00"
	  "\xff\xff\xff\x00\x11\x33\xef\x00\x00\x00\x00\x00\x00\x21\xff\x0b\x4e\x45\x54\x53\x43\x41\x50\x45"
	  "\x32\x2e\x30\x03\x01\x00\x00\x00\x21\xf9\x04\x05\x50\x00\x00\x00\x2c\x02\x00\x01\x00\x02\x00\x02"
	  "\x00\x00\x03\x05\x18\x18\x18\x18\x09\x00\x21\xf9\x04\x0d\x50\x00\x00\x00\x2c\x02\x00\x01\x00\x02"
	  "\x00\x02\x00\x00\x03\x05\x28\x28\x28\x28\x09\x00\x21\xf9\x04\x05\x50\x00\x00\x00\x2c\x05\x00\x03"
	  "\x00\x01\x00\x01\x00\x00\x03\x02\x48\x09\x00\x3b";
    for (bool restore : { false, true }) {
	CAPTURE (restore);
	const auto texture = TextureParser::parseImage (
	    restore ? std::string (restoreBytes, sizeof (restoreBytes) - 1)
		    : std::string (clearBytes, sizeof (clearBytes) - 1),
	    Json { { "nomip", true } }
	);
	REQUIRE (texture->images.size () == 3);
	const auto* pixels
	    = reinterpret_cast<const unsigned char*> (texture->images.at (2).front ()->uncompressedData.get ());
	const int oldPatch = (1 * 12 + 2) * 4;
	CHECK (pixels[oldPatch] == (restore ? 255 : 0));
	CHECK (pixels[oldPatch + 1] == 0);
	CHECK (pixels[oldPatch + 2] == 0);
	CHECK (pixels[oldPatch + 3] == (restore ? 255 : 0));
	const int yellow = (3 * 12 + 5) * 4;
	CHECK (pixels[yellow] == 255);
	CHECK (pixels[yellow + 1] == 255);
	CHECK (pixels[yellow + 2] == 0);
	CHECK (pixels[yellow + 3] == 255);
    }
}

TEST_CASE ("single-frame selected GIFs retain native static background colors", "[texture][user-image][gif]") {
    using Json = WallpaperEngine::Data::JSON::JSON;
    // Native preserves this legacy BGR background fill, while palette pixels are RGB.
    const char bytes[]
	= "\x47\x49\x46\x38\x39\x61\x0c\x00\x08\x00\xf2\x05\x00\x00\x00\x00\xff\x00\x00\x00\xff\x00\x00\x00"
	  "\xff\xff\xff\x00\x11\x33\xef\x00\x00\x00\x00\x00\x00\x21\xff\x0b\x4e\x45\x54\x53\x43\x41\x50\x45"
	  "\x32\x2e\x30\x03\x01\x00\x00\x00\x21\xf9\x04\x04\x50\x00\x00\x00\x2c\x02\x00\x01\x00\x02\x00\x02"
	  "\x00\x00\x03\x05\x18\x18\x18\x18\x09\x00\x3b";
    const auto texture
	= TextureParser::parseImage (std::string (bytes, sizeof (bytes) - 1), Json { { "nomip", true } });
    CHECK_FALSE (texture->isAnimated ());
    CHECK (texture->frames.empty ());
    REQUIRE (texture->images.size () == 1);
    CHECK (texture->width == 12);
    CHECK (texture->height == 8);
    const auto* pixels
	= reinterpret_cast<const unsigned char*> (texture->images.at (0).front ()->uncompressedData.get ());
    CHECK (pixels[0] == 239);
    CHECK (pixels[1] == 51);
    CHECK (pixels[2] == 17);
    CHECK (pixels[3] == 255);
    CHECK (pixels[14 * 4] == 255);
    CHECK (pixels[14 * 4 + 1] == 0);
    CHECK (pixels[14 * 4 + 2] == 0);
    CHECK (pixels[14 * 4 + 3] == 255);
}

TEST_CASE ("animated GIF backgrounds preserve native palette and disposal semantics", "[texture][user-image][gif][gif-background]") {
    using Json = WallpaperEngine::Data::JSON::JSON;
    // Native 2.8.42 controls: global background is BGR; foreground stays RGB.
    // Background index zero and absent global tables remain transparent.
    struct Probe {
	const char* name;
	bool global;
	int background;
	int local; // 0: none, 1: first frame, 2: every frame.
	std::array<bool, 4> transparent;
	int disposal;
	bool holes = false;
    };
    const Probe probes[] = {
	{ "global opaque", true, 5, 0, { false, false, false, false }, 2 },
	{ "global transparent", true, 5, 0, { true, true, true, true }, 2 },
	{ "background zero", true, 0, 0, { false, false, false, false }, 2 },
	{ "local first", true, 5, 1, { false, false, false, false }, 2 },
	{ "local only", false, 5, 2, { false, false, false, false }, 2 },
	{ "restore previous", true, 5, 0, { false, false, false, false }, 3 },
	{ "opaque then transparent", true, 5, 0, { false, true, true, true }, 2 },
	{ "transparent then opaque", true, 5, 0, { true, false, false, false }, 2 },
	{ "transparent holes", true, 5, 0, { false, true, true, true }, 2, true },
    };
    const unsigned char palette[] = {
	37, 83, 211, 255, 0, 0, 0, 255, 0, 0, 0, 255,
	255, 255, 0, 17, 51, 239, 239, 51, 17, 0, 0, 0
    };
    const std::array<std::array<uint16_t, 4>, 4> rectangles = {{
	{ 2, 1, 4, 2 }, { 2, 1, 4, 2 }, { 8, 4, 2, 2 }, { 5, 3, 1, 1 }
    }};
    using Pixel = std::array<unsigned char, 4>;
    const Pixel clear { 0, 0, 0, 0 }, background { 239, 51, 17, 255 };
    const Pixel red { 255, 0, 0, 255 }, green { 0, 255, 0, 255 };
    const Pixel blue { 0, 0, 255, 255 }, yellow { 255, 255, 0, 255 };
    for (const auto& probe : probes) {
	CAPTURE (probe.name);
	std::string bytes ("GIF89a\x0c\x00\x08\x00", 10);
	bytes += static_cast<char> (probe.global ? 0xf2 : 0x72);
	bytes += static_cast<char> (probe.background);
	bytes += '\0';
	if (probe.global) bytes.append (reinterpret_cast<const char*> (palette), sizeof (palette));
	bytes.append ("\x21\xff\x0bNETSCAPE2.0\x03\x01\x00\x00\x00", 19);
	for (size_t frame = 0; frame < 4; ++frame) {
	    const int disposal = frame == 1 ? probe.disposal : frame == 2 ? 3 : 1;
	    bytes.append ("\x21\xf9\x04", 3);
	    bytes += static_cast<char> ((disposal << 2) | probe.transparent[frame]);
	    bytes.append ("\x50\x00\x00\x00", 4);
	    bytes += ',';
	    for (const auto coordinate : rectangles[frame]) appendValue (bytes, coordinate);
	    const bool local = probe.local == 2 || (probe.local == 1 && frame == 0);
	    bytes += static_cast<char> (local ? 0x82 : 0);
	    if (local) {
		std::string colors (reinterpret_cast<const char*> (palette), sizeof (palette));
		colors.replace (15, 3, "\x3d\xad\x1d", 3);
		bytes += colors;
	    }
	    const auto count = rectangles[frame][2] * rectangles[frame][3];
	    bytes += '\x03'; // Clear before each pixel keeps LZW codes four bits wide.
	    bytes += static_cast<char> (count + 1);
	    for (int pixel = 0; pixel < count; ++pixel) {
		const int color = frame == 0 ? (pixel % 4 < 2 ? 1 : 5)
		    : frame == 1 && probe.holes && pixel % 2 != 0 ? 0 : static_cast<int> (frame) + 1;
		bytes += static_cast<char> (8 | (color << 4));
	    }
	    bytes.append ("\x09\x00", 2);
	}
	bytes += ';';
	struct Truncation { const char* probe; size_t length; const char* block; };
	const Truncation truncations[] = {
	    { "global opaque", 36, "global palette" },
	    { "global opaque", 45, "application extension payload" },
	    { "global opaque", 70, "image descriptor" },
	    { "global opaque", 74, "LZW minimum code size" },
	    { "global opaque", 80, "image subblock payload" },
	    { "global opaque", 85, "image subblock terminator" },
	    { "local first", 97, "local palette" },
	    { "local first", 102, "local image subblock payload" },
	};
	for (const auto& truncated : truncations) {
	    if (std::string (probe.name) != truncated.probe) continue;
	    CAPTURE (truncated.block, truncated.length);
	    CHECK_THROWS (TextureParser::parseImage (bytes.substr (0, truncated.length), Json::object ()));
	}
	const auto texture = TextureParser::parseImage (bytes, Json { { "nomip", true } });
	REQUIRE (texture->isAnimated ());
	REQUIRE (texture->frames.size () == 4);
	REQUIRE (texture->images.size () == 4);
	const bool hasBackground = probe.global && probe.background != 0;
	const Pixel initial = hasBackground && !probe.transparent[0] ? background : clear;
	const Pixel foreground = probe.local ? Pixel { 61, 173, 29, 255 } : Pixel { 17, 51, 239, 255 };
	const Pixel disposed = hasBackground && !probe.transparent[1] ? background : clear;
	for (size_t frame = 0; frame < 4; ++frame) {
	    CAPTURE (frame);
	    CHECK (texture->frames[frame]->frametime == 0.8f);
	    const auto& mip = texture->images.at (frame).front ();
	    REQUIRE (mip->uncompressedSize == 12 * 8 * 4);
	    const std::array<Pixel, 7> expected = {
		initial,
		frame == 1 ? green : frame == 0 || probe.disposal == 3 ? red : disposed,
		frame == 1 ? green : frame == 0 || probe.disposal == 3 ? foreground : disposed,
		frame == 2 ? blue : initial,
		frame == 3 ? yellow : initial,
		frame == 1 ? (probe.holes ? red : green) : frame == 0 || probe.disposal == 3 ? red : disposed,
		frame == 1 ? (probe.holes ? foreground : green)
		    : frame == 0 || probe.disposal == 3 ? foreground : disposed,
	    };
	    const std::array<size_t, 7> positions = { 0, 14, 16, 56, 41, 15, 17 };
	    for (size_t sample = 0; sample < positions.size (); ++sample) {
		CAPTURE (sample);
		for (size_t channel = 0; channel < 4; ++channel) {
		    CHECK (static_cast<unsigned char> (mip->uncompressedData[positions[sample] * 4 + channel])
			== expected[sample][channel]);
		}
	    }
	}
    }
}

namespace {
std::string volumeTexture (bool encoded, bool legacy = false) {
    std::string pixels;
    for (uint32_t z = 0; z < 4; ++z) {
	for (uint32_t y = 0; y < 3; ++y) {
	    for (uint32_t x = 0; x < 2; ++x) {
		for (const uint8_t channel : { uint8_t (11 + x * 200), uint8_t (21 + y * 80),
					       uint8_t (31 + z * 70), uint8_t (255) }) {
		    pixels += static_cast<char> (channel);
		}
	    }
	}
    }
    if (encoded) {
	std::string png;
	stbi_write_png_to_func ([] (void* context, void* data, int length) {
	    static_cast<std::string*> (context)->append (static_cast<char*> (data), length);
	}, &png, 2, 12, 4, pixels.data (), 8);
	pixels = std::move (png);
    }
    std::string bytes (legacy ? "TEXV0004" : "TEXV0005", 9);
    if (!legacy) bytes.append ("TEXI0001", 9);
    // 0x40 adds depth after authored dimensions, before the existing versioned word.
    for (uint32_t word : { 0u, 0x42u, 2u, 3u, 8u, 3u, 4u }) appendValue (bytes, word);
    if (!legacy) {
	appendValue (bytes, 0xff554433u);
	bytes.append ("TEXB0004", 9);
	for (uint32_t word : { 1u, encoded ? 13u : UINT32_MAX, 0u }) appendValue (bytes, word);
    }
    for (uint32_t word : { 1u, 2u, 3u, 4u }) appendValue (bytes, word);
    if (!legacy) {
	appendValue (bytes, 0u);
	appendValue (bytes, 0u);
    }
    appendValue (bytes, static_cast<uint32_t> (pixels.size ()));
    return bytes + pixels;
}

auto parseVolume (const std::string& bytes) {
    return TextureParser::parse (BinaryReader (std::make_shared<std::istringstream> (bytes, std::ios::binary)));
}
} // namespace

TEST_CASE ("native volume headers and stacked PNG pixels retain every coordinate", "[texture][volume]") {
    for (bool encoded : { false, true }) {
	for (bool legacy : { false, true }) {
	    if (legacy && encoded) continue;
	    CAPTURE (encoded, legacy);
	    const auto bytes = volumeTexture (encoded, legacy);
	    const auto texture = parseVolume (bytes);
	    REQUIRE (texture->isVolume ());
	    CHECK (texture->textureWidth == 2);
	    CHECK (texture->textureHeight == 3);
	    CHECK (texture->textureDepth == 4);
	    CHECK (texture->width == 8);
	    CHECK (texture->height == 3);
	    const auto& mip = texture->images.at (0).front ();
	    CHECK (mip->width == 2);
	    CHECK (mip->height == 3);
	    CHECK (mip->depth == 4);
	    if (encoded) {
		// Exercise the same batched CPU decode used by asynchronous texture preparation.
		auto second = parseVolume (bytes);
		TextureParser::decodeMipmaps (std::vector<Texture*> { texture.get (), second.get () });
		REQUIRE (mip->decodedData);
		CHECK (mip->decodedWidth == 2);
		CHECK (mip->decodedHeight == 12);
		REQUIRE (second->images.at (0).front ()->decodedData);
		CHECK (std::memcmp (mip->decodedData.get (), second->images.at (0).front ()->decodedData.get (), 96) == 0);
	    }
	    const auto* pixels = encoded ? mip->decodedData.get ()
		: reinterpret_cast<const unsigned char*> (mip->uncompressedData.get ());
	    for (size_t z = 0; z < 4; ++z) for (size_t y = 0; y < 3; ++y) for (size_t x = 0; x < 2; ++x) {
		const size_t index = ((z * 3 + y) * 2 + x) * 4;
		CHECK (pixels[index] == 11 + x * 200);
		CHECK (pixels[index + 1] == 21 + y * 80);
		CHECK (pixels[index + 2] == 31 + z * 70);
		CHECK (pixels[index + 3] == 255);
	    }
	}
    }
}

TEST_CASE ("volume parsing rejects missing dimensions and mismatched slice payloads", "[texture][volume]") {
    const auto bytes = volumeTexture (true);
    for (const size_t offset : { 42u, 75u, 79u, 83u }) {
	CAPTURE (offset);
	auto invalid = bytes;
	const uint32_t zero = 0;
	std::memcpy (invalid.data () + offset, &zero, 4);
	CHECK_THROWS (parseVolume (invalid));
    }
    for (const uint32_t depth : { 3u, 129u, UINT32_MAX }) {
	auto invalid = bytes;
	std::memcpy (invalid.data () + 83, &depth, 4);
	CHECK_THROWS (parseVolume (invalid));
    }
    for (size_t end : std::array<size_t, 5> { 46, 50, 83, 87, bytes.size () - 1 }) {
	CHECK_THROWS (parseVolume (bytes.substr (0, end)));
    }
    auto raw = volumeTexture (false);
    const uint32_t wrongSize = 95;
    std::memcpy (raw.data () + 95, &wrongSize, 4);
    raw.pop_back ();
    CHECK_THROWS (parseVolume (raw));
}
