#include <stdexcept>
#include "CContext.h"

using namespace WallpaperEngine::Irrlicht;

void CContext::setDevice (irr::IrrlichtDevice* device)
{
    this->m_device = device;
}

irr::IrrlichtDevice* CContext::getDevice ()
{
    return this->m_device;
}

irr::io::path CContext::resolveFile (const irr::io::path& file)
{
    if (this->getDevice ()->getFileSystem ()->existFile (file) == false)
    {
        throw std::runtime_error ("Cannot find file " + std::string (file.c_str ()));
    }

    return file;
}

irr::io::path CContext::resolveMaterials (const std::string& materialName)
{
    return this->resolveFile (std::string ("materials/" + materialName + ".tex").c_str ());
}

irr::io::path CContext::resolveVertexShader (const std::string& vertexShader)
{
    return this->resolveFile (std::string ("shaders/" + vertexShader + ".vert").c_str ());
}
irr::io::path CContext::resolveFragmentShader (const std::string& fragmentShader)
{
    return this->resolveFile (std::string ("shaders/" + fragmentShader + ".frag").c_str ());
}

irr::io::path CContext::resolveIncludeShader (const std::string& includeShader)
{
    return this->resolveFile (std::string ("shaders/" + includeShader).c_str ());
}