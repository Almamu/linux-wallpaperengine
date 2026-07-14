#include <catch2/catch_test_macros.hpp>

#include <cstring>
#include <memory>
#include <string>

#include "WallpaperEngine/Data/Assets/Package.h"
#include "WallpaperEngine/Data/Parsers/PackageParser.h"
#include "WallpaperEngine/Data/Parsers/TextureParser.h"
#include "WallpaperEngine/Data/Utils/BinaryReader.h"
#include "WallpaperEngine/Data/Utils/MemoryStream.h"

using namespace WallpaperEngine::Data::Utils;
using namespace WallpaperEngine::Data::Parsers;

namespace {
// ---- little-endian buffer builders ----
void u32 (std::string& b, uint32_t v) {
    b.push_back (char (v & 0xFF));
    b.push_back (char ((v >> 8) & 0xFF));
    b.push_back (char ((v >> 16) & 0xFF));
    b.push_back (char ((v >> 24) & 0xFF));
}

// texture magics are read as 9 bytes and compared with strncmp(..., 9): 8 chars + NUL
void magic (std::string& b, const char* m) {
    b.append (m, 8);
    b.push_back ('\0');
}

std::shared_ptr<std::istream> streamOf (const std::string& bytes) {
    auto buf = std::make_unique<char[]> (bytes.size ());
    std::memcpy (buf.get (), bytes.data (), bytes.size ());
    return std::make_shared<MemoryStream> (std::move (buf), bytes.size ());
}

std::shared_ptr<BinaryReader> reader (const std::string& bytes) {
    return std::make_shared<BinaryReader> (streamOf (bytes));
}

// a complete, valid single-mipmap texture body; flags 0 = static, 4 = animated (IsGif)
std::string validTexture (uint32_t flags, int compressedSize, const std::string& payload) {
    std::string b;
    magic (b, "TEXV0005");
    magic (b, "TEXI0001");
    u32 (b, 0 /* ARGB8888 */);
    u32 (b, flags);
    u32 (b, 4);
    u32 (b, 4);
    u32 (b, 4);
    u32 (b, 4);
    u32 (b, 0 /* ignored */);
    magic (b, "TEXB0001");
    u32 (b, 1 /* imageCount */);
    u32 (b, 1 /* mipmapCount */);
    u32 (b, 1 /* mip width */);
    u32 (b, 1 /* mip height */);
    u32 (b, uint32_t (compressedSize)); // compression stays 0 for TEXB0001
    b += payload;
    return b;
}
} // namespace

TEST_CASE ("BinaryReader rejects truncated reads instead of returning garbage") {
    std::string b;
    b.push_back (0x11);
    b.push_back (0x22); // only 2 of the 4 bytes a uint32 needs

    REQUIRE_THROWS (reader (b)->nextUInt32 ());
}

TEST_CASE ("BinaryReader reads a complete uint32") {
    std::string b;
    u32 (b, 0xDEADBEEF);

    REQUIRE (reader (b)->nextUInt32 () == 0xDEADBEEF);
}

TEST_CASE ("BinaryReader null-terminated string stops at end of stream") {
    // no terminator present: must return what it read rather than spin on an indeterminate byte
    std::string b = "abc";

    REQUIRE (reader (b)->nextNullTerminatedString () == "abc");
}

TEST_CASE ("BinaryReader sized string round-trips and rejects oversized lengths") {
    SECTION ("valid length") {
	std::string b;
	u32 (b, 3);
	b += "abc";

	REQUIRE (reader (b)->nextSizedString () == "abc");
    }

    SECTION ("length beyond the stream is rejected before allocating") {
	std::string b;
	u32 (b, 0xFFFFFFFF);

	REQUIRE_THROWS (reader (b)->nextSizedString ());
    }
}

TEST_CASE ("MemoryStream refuses seeks outside the buffer") {
    SECTION ("in-range seek succeeds") {
	auto r = reader (std::string (4, 'x'));
	r->base ().seekg (2, std::ios::beg);

	REQUIRE (r->base ().good ());
	REQUIRE (r->base ().tellg () == 2);
    }

    SECTION ("out-of-range seek fails cleanly") {
	auto r = reader (std::string (4, 'x'));
	r->base ().seekg (1000, std::ios::beg);

	REQUIRE (r->base ().fail ());
    }
}

TEST_CASE ("PackageParser bounds the declared file count") {
    SECTION ("empty but well-formed package parses") {
	std::string b;
	u32 (b, 8);
	b += "PKGV0001";
	u32 (b, 0 /* filesCount */);

	REQUIRE_NOTHROW (PackageParser::parse (streamOf (b)));
    }

    SECTION ("absurd file count is rejected instead of reserving gigabytes") {
	std::string b;
	u32 (b, 8);
	b += "PKGV0001";
	u32 (b, 0xFFFFFFFF);

	REQUIRE_THROWS (PackageParser::parse (streamOf (b)));
    }
}

TEST_CASE ("TextureParser validates untrusted sizes and counts") {
    SECTION ("a valid static texture parses") {
	REQUIRE_NOTHROW (TextureParser::parse (*reader (validTexture (0, 4, std::string (4, '\0')))));
    }

    SECTION ("a negative mipmap size is rejected before new[]") {
	REQUIRE_THROWS (TextureParser::parse (*reader (validTexture (0, -1, ""))));
    }

    SECTION ("an animated texture with zero frames does not dereference an empty vector") {
	std::string b = validTexture (4 /* IsGif */, 4, std::string (4, '\0'));
	magic (b, "TEXS0001");
	u32 (b, 0 /* frameCount */);

	REQUIRE_THROWS (TextureParser::parse (*reader (b)));
    }
}
