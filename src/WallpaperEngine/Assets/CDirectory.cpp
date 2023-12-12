#include "common.h"
#include <sys/stat.h>

#include <utility>

#include "CDirectory.h"
#include "CAssetLoadException.h"

using namespace WallpaperEngine::Assets;

CDirectory::CDirectory (std::filesystem::path basepath) :
    m_basepath (std::move(basepath))
{
    // ensure the specified path exists
    struct stat buffer {};

    if (stat (this->m_basepath.c_str (), &buffer) != 0)
        throw CAssetLoadException (this->m_basepath, "Cannot find directory");

    if (!S_ISDIR(buffer.st_mode))
        throw CAssetLoadException (this->m_basepath, "Expected directory but found a file");
}

CDirectory::~CDirectory ()
= default;

std::filesystem::path CDirectory::resolveRealFile (const std::string& filename) const
{
    return std::filesystem::path (this->m_basepath) / filename;
}

const void* CDirectory::readFile (const std::string& filename, uint32_t* length) const
{
    std::filesystem::path final = std::filesystem::path (this->m_basepath) / filename;

    // first check the cache, if the file is there already just return the data in there
    auto it = this->m_cache.find (final);

    if (it != this->m_cache.end ())
    {
        if (length != nullptr)
            *length = (*it).second.length;

        return (*it).second.address;
    }

    FILE* fp = fopen (final.c_str (), "rb");

    if (fp == nullptr)
        throw CAssetLoadException(filename, "Cannot find file");

    // go to the end, get the position and return to the beginning
    fseek (fp, 0, SEEK_END);
    long size = ftell (fp);
    fseek (fp, 0, SEEK_SET);

    // now read the whole file
    char* contents = new char[size];

    if (fread (contents, size, 1, fp) != 1)
    {
        delete[] contents;
        throw CAssetLoadException (filename, "Unexpected error when reading the file");
    }

    if (length != nullptr)
        *length = size;

    return contents;
}