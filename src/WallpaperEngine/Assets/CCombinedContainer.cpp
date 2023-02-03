#include "CCombinedContainer.h"
#include "CAssetLoadException.h"

using namespace WallpaperEngine::Assets;

void CCombinedContainer::add (CContainer* container)
{
    this->m_containers.emplace_back (container);
}

const void* CCombinedContainer::readFile (std::string filename, uint32_t* length) const
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