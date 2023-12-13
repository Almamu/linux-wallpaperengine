#include <memory.h>

#include "CAssetLoadException.h"
#include "CVirtualContainer.h"

using namespace WallpaperEngine::Assets;

void CVirtualContainer::add (const std::string& filename, const char* contents, uint32_t length) {
    this->m_virtualFiles.insert (std::make_pair (filename, new CFileEntry (contents, length)));
}

void CVirtualContainer::add (const std::string& filename, const std::string& contents) {
    char* copy = new char [contents.length () + 1];

    // copy the text AND the \0
    memcpy (copy, contents.c_str (), contents.length () + 1);

    // finally add to the container
    this->add (filename, copy, contents.length () + 1);
}

const void* CVirtualContainer::readFile (const std::string& filename, uint32_t* length) const {
    const auto cur = this->m_virtualFiles.find (filename);

    if (cur == this->m_virtualFiles.end ())
        throw CAssetLoadException (filename, "Cannot find file in the virtual container");

    if (length != nullptr)
        *length = cur->second->length;

    // clone original first
    char* result = new char [cur->second->length];

    memcpy (result, cur->second->address, cur->second->length);

    return result;
}