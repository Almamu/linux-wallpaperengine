#include <memory.h>

#include "CAssetLoadException.h"
#include "CVirtualContainer.h"

using namespace WallpaperEngine::Assets;

void CVirtualContainer::add (const std::filesystem::path& filename, const std::shared_ptr<const uint8_t[]>& contents, uint32_t length) {
    this->m_virtualFiles.insert (std::pair (filename, std::make_unique<CFileEntry> (contents, length)));
}

void CVirtualContainer::add (const std::filesystem::path& filename, const std::string& contents) {
    std::shared_ptr<uint8_t[]> copy = std::shared_ptr<uint8_t[]> (new uint8_t [contents.length () + 1]);

    // copy the text AND the \0
    memcpy (copy.get(), contents.c_str (), contents.length () + 1);

    // finally add to the container
    this->add (filename, copy, contents.length () + 1);
}

void CVirtualContainer::add (const std::filesystem::path& filename, const char* contents) {
    this->add (filename, std::string (contents));
}

void CVirtualContainer::add (const std::filesystem::path& filename, const json& contents) {
    this->add (filename, contents.dump ());
}

std::shared_ptr<const uint8_t[]> CVirtualContainer::readFile (const std::filesystem::path& filename, uint32_t* length) const {
    const auto cur = this->m_virtualFiles.find (filename);

    if (cur == this->m_virtualFiles.end ())
        throw CAssetLoadException (filename, "Cannot find file in the virtual container");

    if (length != nullptr)
        *length = cur->second->length;

    // clone original first
    return cur->second->content;
}