#include "CCombinedContainer.h"
#include "CAssetLoadException.h"
#include "CPackage.h"
#include "CPackageLoadException.h"
#include "WallpaperEngine/Logging/CLog.h"

using namespace WallpaperEngine::Assets;

CCombinedContainer::CCombinedContainer () :
    CContainer (),
    m_containers ()
{
}

void CCombinedContainer::add (CContainer* container)
{
    this->m_containers.emplace_back (container);
}

void CCombinedContainer::addPkg (const std::filesystem::path& path)
{
    try
    {
        // add the package to the list
        this->add (new CPackage (path));
        sLog.out ("Detected ", path.filename (), " file at ", path, ". Adding to list of searchable paths");
    }
    catch (CPackageLoadException& ex)
    {
        // ignore this error, the package file was not found
        sLog.out ("No ", path.filename (), " file found at ", path, ". Defaulting to normal folder storage");
    }
    catch (std::runtime_error& ex)
    {
        // the package was found but there was an error loading it (wrong header or something)
        sLog.exception ("Failed to load scene.pkg file: ", ex.what());
    }
}


std::filesystem::path CCombinedContainer::resolveRealFile (const std::string& filename) const
{
    for (auto cur : this->m_containers)
    {
        try
        {
            // try to read the file on the current container, if the file doesn't exists
            // an exception will be thrown
            return cur->resolveRealFile (filename);
        }
        catch (CAssetLoadException& ex)
        {
            // not found in this container, next try
        }
    }

    // no container was able to load the file, abort!
    throw CAssetLoadException (filename, "Cannot resolve file in any of the containers");
}

const void* CCombinedContainer::readFile (const std::string& filename, uint32_t* length) const
{
    for (auto cur : this->m_containers)
    {
        try
        {
            // try to read the file on the current container, if the file doesn't exists
            // an exception will be thrown
            return cur->readFile (filename, length);
        }
        catch (CAssetLoadException& ex)
        {
            // not found in this container, next try
        }
    }

    // no container was able to load the file, abort!
    throw CAssetLoadException (filename, "Cannot find file in any of the containers");
}