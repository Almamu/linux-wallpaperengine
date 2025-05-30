#include "CCombinedContainer.h"
#include "CAssetLoadException.h"
#include "CPackage.h"
#include "CPackageLoadException.h"
#include "WallpaperEngine/Logging/CLog.h"

using namespace WallpaperEngine::Assets;

CCombinedContainer::CCombinedContainer () : CContainer () {}

void CCombinedContainer::add (const std::shared_ptr<CContainer>& container) {
    this->m_containers.emplace_back (container);
}

void CCombinedContainer::addPkg (const std::filesystem::path& path) {
    try {
        // add the package to the list
        this->add (std::make_shared<CPackage> (path));
        sLog.out ("Detected ", path.filename (), " file at ", path, ". Adding to list of searchable paths");
    } catch (CPackageLoadException&) {
        // ignore this error, the package file was not found
        sLog.out ("No ", path.filename (), " file found at ", path, ". Defaulting to normal folder storage");
    } catch (std::runtime_error& ex) {
        // the package was found but there was an error loading it (wrong header or something)
        sLog.exception ("Failed to load ", path.filename(), " file: ", ex.what ());
    }
}

std::filesystem::path CCombinedContainer::resolveRealFile (const std::filesystem::path& filename) const {
    for (const auto& cur : this->m_containers) {
        try {
            // try to read the file on the current container, if the file doesn't exists
            // an exception will be thrown
            return cur->resolveRealFile (filename);
        } catch (CAssetLoadException&) {
            // not found in this container, next try
        }
    }

    // no container was able to load the file, abort!
    throw CAssetLoadException (filename, "Cannot resolve file in any of the containers");
}

std::shared_ptr<const uint8_t[]> CCombinedContainer::readFile (const std::filesystem::path& filename, uint32_t* length) const {
    for (const auto& cur : this->m_containers) {
        try {
            // try to read the file on the current container, if the file doesn't exists
            // an exception will be thrown
            return cur->readFile (filename, length);
        } catch (CAssetLoadException& e) {
            // not found in this container, next try
        }
    }

    // no container was able to load the file, abort!
    throw CAssetLoadException (filename, "Cannot find file in any of the containers");
}