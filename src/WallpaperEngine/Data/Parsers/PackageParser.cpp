#include "PackageParser.h"

#include "WallpaperEngine/Data/Utils/BinaryReader.h"
#include "WallpaperEngine/Logging/Log.h"

#include "WallpaperEngine/Data/Assets/Package.h"

#include <istream>
#include <limits>
#include <memory>
#include <stdexcept>

using namespace WallpaperEngine::Data::Parsers;
using namespace WallpaperEngine::Data::Assets;
using namespace WallpaperEngine::Data::Utils;

// Highest pkg format version whose binary layout has been verified compatible. Newer versions are
// still attempted (the index structure has been stable across all known versions) but get a warning.
static constexpr int PKG_MAX_KNOWN_VERSION = 24;

PackageUniquePtr PackageParser::parse (ReadStreamSharedPtr stream) {
    auto reader = std::make_unique<BinaryReader> (std::move (stream));

    const std::string header = reader->nextSizedString ();

    if (header.starts_with ("PKGV") == false) {
	sLog.exception ("Expected header to start with PKGV, got ", header);
    }

    int version = 0;

    try {
	version = std::stoi (header.substr (4));
    } catch (...) {
	sLog.exception ("Cannot parse version number from pkg header: ", header);
    }

    if (version > PKG_MAX_KNOWN_VERSION) {
	sLog.error ("scene.pkg version ", header, " has not been verified, some assets may fail to load");
    }

    // work out the total file size up-front so the index layout can be self-validated below
    auto& base = reader->base ();
    const std::streamoff headerEnd = base.tellg ();
    if (headerEnd < 0 || !base.seekg (0, std::ios::end)) {
	throw std::runtime_error ("Package requires a seekable stream");
    }
    const std::streamoff fileEnd = base.tellg ();
    if (fileEnd < headerEnd || !base.seekg (headerEnd, std::ios::beg)) {
	throw std::runtime_error ("Cannot determine package size");
    }

    auto result = std::make_unique<Package> (Package {
	.file = std::move (reader),
    });

    result->files = parseFileList (*result->file, static_cast<uint64_t> (fileEnd));
    result->baseOffset = 0;

    return result;
}

FileEntryList PackageParser::parseFileList (const BinaryReader& stream, const uint64_t fileSize) {
    const uint32_t filesCount = stream.nextUInt32 ();
    auto& input = stream.base ();
    const std::streamoff indexStart = input.tellg ();

    // Both layouts need at least three uint32 fields for each entry, even with an empty name.
    constexpr uint64_t minimumEntrySize = 3 * sizeof (uint32_t);
    if (indexStart < 0 || static_cast<uint64_t> (indexStart) > fileSize
	|| filesCount > (fileSize - static_cast<uint64_t> (indexStart)) / minimumEntrySize) {
	throw std::runtime_error ("Package file count exceeds the available index bytes");
    }

    // Unpadded entries use offsets relative to the end of the index. The PKGV0021
    // variant pads each filename to a four-byte boundary and uses absolute offsets.
    const auto parseIndex = [&] (const bool padded) {
	FileEntryList entries;
	entries.reserve (filesCount);
	for (uint32_t i = 0; i < filesCount; ++i) {
	    auto filename = stream.nextSizedString ();
	    if (padded) {
		char padding[3];
		stream.next (padding, (4 - filename.size () % 4) % 4);
	    }
	    entries.push_back (std::make_unique<FileEntry> (FileEntry {
		.filename = std::move (filename), .offset = stream.nextUInt32 (), .length = stream.nextUInt32 () }));
	}

	const std::streamoff position = input.tellg ();
	if (position < 0 || static_cast<uint64_t> (position) > fileSize) {
	    throw std::runtime_error ("Package index exceeds the available bytes");
	}
	const auto dataStart = static_cast<uint64_t> (position);
	const auto baseOffset = padded ? uint64_t { 0 } : dataStart;
	for (const auto& entry : entries) {
	    const uint64_t offset = baseOffset + entry->offset;
	    if (offset > std::numeric_limits<uint32_t>::max () || offset + entry->length > fileSize
		|| (entry->length > 0 && offset < dataStart)) {
		throw std::runtime_error ("Package entry lies outside the data section");
	    }
	    entry->offset = static_cast<uint32_t> (offset);
	}

	const auto lastEntryEnd = entries.empty ()
	    ? dataStart
	    : static_cast<uint64_t> (entries.back ()->offset) + entries.back ()->length;
	if (!padded && lastEntryEnd != fileSize) {
	    throw std::runtime_error ("Unpadded package index does not match the file size");
	}

	return entries;
    };

    try {
	return parseIndex (false);
    } catch (const std::runtime_error&) {
	// Trying the unpadded layout can reach EOF before the actual padded index is decoded.
	input.clear ();
	if (!input.seekg (indexStart, std::ios::beg)) {
	    throw std::runtime_error ("Cannot rewind package index");
	}
    }
    return parseIndex (true);
}
