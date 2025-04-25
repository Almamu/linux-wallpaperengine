#include <memory.h>

#include "CAssetLoadException.h"
#include "CVirtualContainer.h"

using namespace WallpaperEngine::Assets;

CVirtualContainer::~CVirtualContainer() {
    for (auto& [_, entry] : this->m_virtualFiles) {
        delete entry;
    }

    this->m_virtualFiles.clear();
}

void CVirtualContainer::add (const std::filesystem::path& filename, const uint8_t* contents, uint32_t length) {
    this->m_virtualFiles.insert (std::make_pair (filename, new CFileEntry (contents, length)));
}

void CVirtualContainer::add (const std::filesystem::path& filename, const std::string& contents) {
    auto* copy = new uint8_t [contents.length () + 1];

    // copy the text AND the \0
    memcpy (copy, contents.c_str (), contents.length () + 1);

    // finally add to the container
    this->add (filename, copy, contents.length () + 1);
}

void CVirtualContainer::add (const std::filesystem::path& filename, const char* contents) {
    this->add (filename, std::string (contents));
}

void CVirtualContainer::add (const std::filesystem::path& filename, const json& contents) {
    this->add (filename, contents.dump ());
}

const uint8_t* CVirtualContainer::readFile (const std::filesystem::path& filename, uint32_t* length) const {
    const auto cur = this->m_virtualFiles.find (filename);

    if (cur == this->m_virtualFiles.end ())
        throw CAssetLoadException (filename, "Cannot find file in the virtual container");

    if (length != nullptr)
        *length = cur->second->length;

    // clone original first
    auto* result = new uint8_t [cur->second->length];

    memcpy (result, cur->second->address, cur->second->length);

    return result;
}