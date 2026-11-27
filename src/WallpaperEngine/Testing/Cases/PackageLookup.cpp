#include <catch2/catch_test_macros.hpp>

#include "WallpaperEngine/FileSystem/Adapters/Package.h"
#include "WallpaperEngine/FileSystem/Adapters/Directory.h"
#include "WallpaperEngine/Assets/AssetLoadException.h"
#include "WallpaperEngine/Assets/AssetLocator.h"
#include "WallpaperEngine/Data/Parsers/PackageParser.h"
#include "WallpaperEngine/Data/Utils/MemoryStream.h"
#include "WallpaperEngine/WebBrowser/CEF/ResourceRange.h"
#include <algorithm>
#include <array>
#include <fstream>
#include <sstream>
#include <string_view>
#include <tuple>
#include <unistd.h>

using namespace WallpaperEngine::Data::Assets;
using WallpaperEngine::Data::Parsers::PackageParser;
using WallpaperEngine::Data::Utils::BinaryReader;
using WallpaperEngine::Data::Utils::MemoryStream;
using WallpaperEngine::FileSystem::Adapters::PackageAdapter;

namespace {
struct SequentialBuffer : std::streambuf {
    explicit SequentialBuffer (std::string& bytes) { setg (bytes.data (), bytes.data (), bytes.data () + bytes.size ()); }
};

PackageAdapter buildAdapter (const std::initializer_list<std::string> filenames) {
    auto package = std::make_unique<Package> ();

    for (const auto& filename : filenames) {
	package->files.push_back (std::make_unique<FileEntry> (filename, 0, 0));
    }

    return PackageAdapter (std::move (package));
}

std::string packageBytes (const bool padded) {
    constexpr std::array files {
	std::pair { std::string_view ("first"), std::string_view ("alpha") },
	std::pair { std::string_view ("second.txt"), std::string_view ("beta") },
    };

    std::string bytes;
    const auto appendUInt32 = [&] (const size_t value) {
	for (unsigned shift = 0; shift < 32; shift += 8) {
	    bytes.push_back (static_cast<char> (value >> shift));
	}
    };
    const auto appendString = [&] (std::string_view value) {
	appendUInt32 (value.size ());
	bytes.append (value);
    };
    const auto padding = [padded] (const size_t length) { return padded ? (4 - length % 4) % 4 : 0; };

    appendString (padded ? "PKGV0021" : "PKGV0024");
    appendUInt32 (files.size ());

    size_t offset = 0;
    if (padded) {
	offset = bytes.size ();
	for (const auto& [name, content] : files) {
	    offset += 12 + name.size () + padding (name.size ());
	}
    }

    for (const auto& [name, content] : files) {
	appendString (name);
	bytes.append (padding (name.size ()), '\0');
	appendUInt32 (offset);
	appendUInt32 (content.size ());
	offset += content.size ();
    }
    for (const auto& [name, content] : files) {
	bytes.append (content);
    }
    return bytes;
}
} // namespace

TEST_CASE ("package lookups ignore case") {
    // workshop 2719499501 stores "Sounds/01 Overworld (MM).mp3" but its scene references
    // "sounds/...", and 2935233995 asks for "models/Rayman.json" against "models/rayman.json".
    // both are fatal on a case-sensitive lookup even though the file is right there
    const auto adapter = buildAdapter ({"Sounds/01 Overworld (MM).mp3", "models/rayman.json"});

    REQUIRE (adapter.exists ("Sounds/01 Overworld (MM).mp3"));
    REQUIRE (adapter.exists ("sounds/01 Overworld (MM).mp3"));
    REQUIRE (adapter.exists ("models/Rayman.json"));
    REQUIRE (adapter.exists ("MODELS/RAYMAN.JSON"));

    REQUIRE_FALSE (adapter.exists ("sounds/missing.mp3"));
    REQUIRE_FALSE (adapter.exists ("rayman.json"));
}

TEST_CASE ("both package index layouts deliver complete entries", "[assets][package-bounds]") {
    for (const bool padded : { false, true }) {
	CAPTURE (padded);
	const auto stream = std::make_shared<std::istringstream> (packageBytes (padded));
	const PackageAdapter adapter (PackageParser::parse (stream));
	std::string contents;
	std::getline (*adapter.open ("first"), contents);
	CHECK (contents == "alpha");
	std::getline (*adapter.open ("second.txt"), contents);
	CHECK (contents == "beta");
    }
}

TEST_CASE ("both package index layouts reject incomplete files", "[assets][package-bounds]") {
    for (const bool padded : { false, true }) {
	CAPTURE (padded);
	const std::string complete = packageBytes (padded);
	for (const auto size : { complete.size () / 2, complete.size () - 1 }) {
	    CAPTURE (size);
	    const auto stream = std::make_shared<std::istringstream> (complete.substr (0, size));
	    CHECK_THROWS (PackageParser::parse (stream));
	}
    }
}

TEST_CASE ("binary reader supports exact reads on sequential streams", "[assets][package-bounds]") {
    std::string bytes ("\x03\0\0\0one" "two\0end", 14);
    SequentialBuffer buffer (bytes);
    const BinaryReader reader (std::make_shared<std::istream> (&buffer));

    CHECK (reader.nextSizedString () == "one");
    CHECK (reader.nextNullTerminatedString () == "two");
    char block[3];
    reader.next (block, sizeof (block));
    CHECK (std::string_view (block, sizeof (block)) == "end");
    CHECK_THROWS (reader.nextUInt32 ());
}

TEST_CASE ("package parsing requires a seekable stream", "[assets][package-bounds]") {
    auto bytes = packageBytes (false);
    SequentialBuffer buffer (bytes);
    CHECK_THROWS (PackageParser::parse (std::make_shared<std::istream> (&buffer)));
}

TEST_CASE ("binary reader rejects incomplete fields", "[assets][package-bounds]") {
    SECTION ("float") {
	const BinaryReader reader (std::make_shared<std::istringstream> ("ab"));
	CHECK_THROWS (reader.nextFloat ());
    }
    SECTION ("sized string") {
	const BinaryReader reader (std::make_shared<std::istringstream> (std::string ("\x03\0\0\0ab", 6)));
	CHECK_THROWS (reader.nextSizedString ());
    }
    SECTION ("null terminated string") {
	const BinaryReader reader (std::make_shared<std::istringstream> ("ab"));
	CHECK_THROWS (reader.nextNullTerminatedString ());
    }
}

TEST_CASE ("memory stream seeks are bounded and leave the cursor intact on failure", "[assets][package-bounds]") {
    auto buffer = std::make_unique<char[]> (3);
    std::copy_n ("abc", 3, buffer.get ());
    MemoryStream stream (std::move (buffer), 3);

    REQUIRE (stream.seekg (std::streampos (1)).good ());
    CHECK (stream.get () == 'b');
    CHECK (stream.seekg (-3, std::ios::cur).fail ());
    stream.clear ();
    CHECK (stream.tellg () == std::streampos (2));
    CHECK (stream.seekg (1, std::ios::end).fail ());
    stream.clear ();
    CHECK (stream.tellg () == std::streampos (2));
    REQUIRE (stream.seekg (0, std::ios::end).good ());
    REQUIRE (stream.seekg (-3, std::ios::cur).good ());
    CHECK (stream.get () == 'a');
}

TEST_CASE ("loose assets resolve Windows casing while preserving exact matches", "[assets][directory]") {
    char pattern[] = "/tmp/lwe-directory-test-XXXXXX";
    const auto* temporary = mkdtemp (pattern);
    REQUIRE (temporary != nullptr);
    const std::filesystem::path root = temporary;
    struct Cleanup {
	std::filesystem::path path;
	~Cleanup () { std::filesystem::remove_all (path); }
    } cleanup { root };
    std::filesystem::create_directories (root / "wallpaper/js");
    std::filesystem::create_directories (root / "wallpaper-neighbor");
    std::ofstream (root / "wallpaper/js/threePatcher.js") << "corrected reference";
    std::ofstream (root / "wallpaper/js/Exact.js") << "upper";
    std::ofstream (root / "wallpaper/js/exact.js") << "lower";
    std::ofstream (root / "wallpaper-neighbor/outside.js") << "outside";
    std::filesystem::create_directory_symlink (root / "wallpaper-neighbor", root / "wallpaper/link");
    const WallpaperEngine::FileSystem::Adapters::DirectoryAdapter adapter (root / "wallpaper");

    REQUIRE (adapter.exists ("JS/ThreePatcher.js"));
    CHECK (adapter.physicalPath ("JS/ThreePatcher.js") == root / "wallpaper/js/threePatcher.js");
    std::string contents;
    std::getline (*adapter.open ("JS/ThreePatcher.js"), contents);
    CHECK (contents == "corrected reference");
    std::getline (*adapter.open ("js/Exact.js"), contents);
    CHECK (contents == "upper");
    std::getline (*adapter.open ("js/exact.js"), contents);
    CHECK (contents == "lower");
    CHECK_FALSE (adapter.exists ("js/missing.js"));
    CHECK_FALSE (adapter.exists ("../wallpaper-neighbor/outside.js"));
    CHECK_FALSE (adapter.exists ("LINK/OUTSIDE.JS"));
    CHECK_THROWS (adapter.open ("../wallpaper-neighbor/outside.js"));
    CHECK_THROWS (adapter.physicalPath ("LINK/OUTSIDE.JS"));
}

TEST_CASE ("web media byte ranges bound seek responses", "[web][range]") {
    using WallpaperEngine::WebBrowser::CEF::parseResourceRange;
    for (const auto& [header, offset, length] : {
	     std::tuple { "", 0, 10 }, { "bytes=0-", 0, 10 }, { "bytes=4-", 4, 6 },
	     { "bytes=2-5", 2, 4 }, { "bytes=8-99", 8, 2 }, { "bytes=-3", 7, 3 }, { "bytes=-20", 0, 10 }
	 }) {
	INFO (header);
	const auto range = parseResourceRange (header, 10);
	REQUIRE (range.has_value ());
	CHECK (range->offset == offset);
	CHECK (range->length == length);
    }
    for (const auto* header : { "bytes=10-", "bytes=5-3", "bytes=-0", "bytes=a-3", "bytes=0-1,4-5", "items=0-2" }) {
	INFO (header);
	CHECK_FALSE (parseResourceRange (header, 10).has_value ());
    }
    CHECK_FALSE (parseResourceRange ("bytes=0-", 0).has_value ());
    REQUIRE (parseResourceRange ("", 0).has_value ());
    CHECK (parseResourceRange ("", 0)->length == 0);
}

TEST_CASE ("short workshop shader paths fail normally and valid overrides still resolve", "[assets]") {
    auto files = std::make_unique<WallpaperEngine::FileSystem::Container> ();
    auto& vfs = files->getVFS ();
    vfs.add ("shaders/generic.frag", "ordinary shader");
    vfs.add ("shaders/workshop/123/effect/example.frag", "authored shader");
    vfs.add ("zcompat/scene/shaders/123/example.frag", "compatibility shader");
    vfs.add ("shaders/workshop/456/effect/example.frag", "fallback shader");
    const WallpaperEngine::Assets::AssetLocator locator (std::move (files));

    for (const auto* path : { "", "workshop", "workshop/123", "workshop/123/", "workshop/123/missing.frag" }) {
	INFO (path);
	REQUIRE_THROWS_AS (locator.fragmentShader (path), WallpaperEngine::Assets::AssetLoadException);
    }
    CHECK (locator.fragmentShader ("generic.frag") == "ordinary shader");
    CHECK (locator.fragmentShader ("workshop/123/effect/example.frag") == "compatibility shader");
    CHECK (locator.fragmentShader ("workshop/456/effect/example.frag") == "fallback shader");
}

TEST_CASE ("virtual files can be reopened and read independently", "[assets]") {
    WallpaperEngine::FileSystem::Container files;
    const std::string contents ("a\0bc", 4);
    files.getVFS ().add ("generated.bin", contents);

    CHECK (files.readString ("generated.bin") == contents);
    CHECK (files.readString ("generated.bin") == contents);

    const auto first = files.read ("generated.bin");
    const auto second = files.read ("generated.bin");
    CHECK (first->get () == 'a');
    CHECK (first->get () == '\0');
    CHECK (second->get () == 'a');
    CHECK (first->get () == 'b');
    CHECK (files.readString ("generated.bin") == contents);
    CHECK (second->get () == '\0');
}
