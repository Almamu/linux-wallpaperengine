#include "CAssetLoadException.h"
#include "CDirectory.h"

using namespace WallpaperEngine::Assets;

CDirectory::CDirectory (const std::filesystem::path& basepath) {
    try {
        // resolve the path to it's real location
        std::filesystem::path finalpath = std::filesystem::canonical(basepath);
        std::filesystem::file_status status = std::filesystem::status (finalpath);

        if (!std::filesystem::exists (finalpath)) {
            throw CAssetLoadException (basepath, "Cannot find directory");
        }

        if (!std::filesystem::is_directory(status)) {
            throw CAssetLoadException (basepath, "Expected directory but found a file");
        }

        this->m_basepath = finalpath;
    } catch (std::bad_alloc&) {
        throw CAssetLoadException (basepath, "Cannot allocate memory");
    } catch (std::filesystem::filesystem_error& e) {
        throw CAssetLoadException (basepath, e.what ());
    }
}

std::filesystem::path CDirectory::resolveRealFile (const std::filesystem::path& filename) const {
    try {
        std::filesystem::path final = std::filesystem::canonical (this->m_basepath / filename);

        // first validate the path, so the message doesn't reflect if the file exists or not unless it's under the actual directory
        if (final.string ().find (this->m_basepath.string ()) != 0) {
            throw CAssetLoadException (filename, "File is not a child of the given directory");
        }

        std::filesystem::file_status status = std::filesystem::status (final);

        if (!std::filesystem::exists (final)) {
            throw CAssetLoadException (filename, "Cannot find file");
        }

        if (!std::filesystem::is_regular_file (status)) {
            throw CAssetLoadException (filename, "Expected file but found a directory");
        }

        return final;
    } catch (std::filesystem::filesystem_error& e) {
        throw CAssetLoadException (filename, e.what ());
    }
}

const uint8_t* CDirectory::readFile (const std::filesystem::path& filename, uint32_t* length) const {
    try {
        std::filesystem::path final = this->resolveRealFile (filename);

        // first check the cache, if the file is there already just return the data in there
        const auto it = this->m_cache.find (final);

        if (it != this->m_cache.end ()) {
            if (length != nullptr)
                *length = it->second.length;

            return it->second.address;
        }

        FILE* fp = fopen (final.c_str (), "rb");

        if (fp == nullptr) {
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

        return contents;
    } catch (std::filesystem::filesystem_error& e) {
        throw CAssetLoadException (filename, e.what ());
    }
}