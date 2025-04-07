#include <sys/stat.h>

#include <cstring>
#include <utility>

#include "CAssetLoadException.h"
#include "CDirectory.h"

using namespace WallpaperEngine::Assets;

CDirectory::CDirectory (const std::filesystem::path& basepath) {
    // ensure the specified path exists
    struct stat buffer {};

    // resolve the path to it's real location
    char* finalpath = realpath (basepath.c_str (), nullptr);

    if (finalpath == nullptr) {
        throw CAssetLoadException (basepath, "Cannot resolve to real path in the filesystem");
    }

    if (stat (finalpath, &buffer) != 0) {
        throw CAssetLoadException (basepath, "Cannot find directory");
    }

    if (!S_ISDIR (buffer.st_mode)) {
        throw CAssetLoadException (basepath, "Expected directory but found a file");
    }

    this->m_basepath = finalpath;
    // the value returned from realpath has to be free'd
    free (finalpath);
}

std::filesystem::path CDirectory::resolveRealFile (const std::string& filename) const {
    return std::filesystem::path (this->m_basepath) / filename;
}

const uint8_t* CDirectory::readFile (const std::string& filename, uint32_t* length) const {
    const std::filesystem::path final = std::filesystem::path (this->m_basepath) / filename;

    // resolve real path and ensure it's within the bounds
    char* finalpath = realpath (final.c_str (), nullptr);

    if (finalpath == nullptr) {
        throw CAssetLoadException (filename, "Cannot find file");
    }

    // ensure it's within the basepath
    size_t baseLength = strlen (finalpath);

    if (baseLength < this->m_basepath.string ().size()) {
        free (finalpath);
        throw CAssetLoadException (filename, "File is not a child of the given directory");
    }

    if (strncmp (this->m_basepath.c_str (), finalpath, this->m_basepath.string ().size ()) != 0) {
        free (finalpath);
        throw CAssetLoadException (filename, "File is not a child of the given directory");
    }

    // first check the cache, if the file is there already just return the data in there
    const auto it = this->m_cache.find (finalpath);

    if (it != this->m_cache.end ()) {
        if (length != nullptr)
            *length = it->second.length;

        return it->second.address;
    }

    FILE* fp = fopen (finalpath, "rb");

    if (fp == nullptr) {
        free (finalpath);
        throw CAssetLoadException (filename, "Cannot open file for reading");
    }

    // go to the end, get the position and return to the beginning
    fseek (fp, 0, SEEK_END);
    const long size = ftell (fp);
    fseek (fp, 0, SEEK_SET);

    // now read the whole file
    auto* contents = new uint8_t [size];

    if (fread (contents, size, 1, fp) != 1) {
        delete [] contents;
        throw CAssetLoadException (filename, "Unexpected error when reading the file");
    }

    if (length != nullptr) {
        *length = size;
    }

    free (finalpath);

    return contents;
}