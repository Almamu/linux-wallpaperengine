#include "PackageParser.h"

#include "WallpaperEngine/Data/Utils/BinaryReader.h"
#include "WallpaperEngine/Logging/Log.h"

#include "WallpaperEngine/Data/Assets/Package.h"

#include <fstream>
#include <memory>

using namespace WallpaperEngine::Data::Parsers;
using namespace WallpaperEngine::Data::Assets;
using namespace WallpaperEngine::Data::Utils;

PackageUniquePtr PackageParser::parse (ReadStreamSharedPtr stream) {
    auto reader = std::make_unique<BinaryReader> (std::move (stream));

    if (const std::string header = reader->nextSizedString (); header.starts_with ("PKGV") == false) {
	sLog.exception ("Expected header to start with PKGV, got ", header);
    }

    auto result = std::make_unique<Package> (Package {
	.file = std::move (reader),
    });

    result->files = parseFileList (*result->file);
    result->baseOffset = result->file->base ().tellg ();

    return result;
}

FileEntryList PackageParser::parseFileList (const BinaryReader& stream) {
    FileEntryList result = {};
    const uint32_t filesCount = stream.nextUInt32 ();

    // each entry is at least a 4-byte name length + 4-byte offset + 4-byte length, so a file list
    // cannot contain more entries than remaining()/12; refuse (and avoid reserving) beyond that so a
    // bogus count in an untrusted package cannot request a multi-gigabyte allocation
    if (const std::streamsize maxEntries = stream.remaining () / 12;
	static_cast<std::streamsize> (filesCount) > maxEntries) {
	sLog.exception ("Package declares ", filesCount, " files but only room for ", maxEntries);
    }

    result.reserve (filesCount);

    for (uint32_t i = 0; i < filesCount; i++) {
	result.push_back (
	    std::make_unique<FileEntry> (FileEntry {
		.filename = stream.nextSizedString (), .offset = stream.nextUInt32 (), .length = stream.nextUInt32 () })
	);
    }

    return result;
}
