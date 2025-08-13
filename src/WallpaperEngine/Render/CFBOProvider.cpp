#include "CFBOProvider.h"
#include <gmpxx.h>

using namespace WallpaperEngine::Render;
using namespace WallpaperEngine::Data::Model;


CFBOProvider::CFBOProvider (const CFBOProvider* parent) :
    m_parent (parent) {}


std::shared_ptr<CFBO> CFBOProvider::create(const FBO& base, ITexture::TextureFlags flags, glm::vec2 size) {
    return this->m_fbos[base.name] = std::make_shared <CFBO> (
        base.name,
        // TODO: PROPERLY DETERMINE FBO FORMAT BASED ON THE STRING
        ITexture::TextureFormat::ARGB8888,
        flags,
        base.scale,
        size.x / base.scale,
        size.y / base.scale,
        size.x / base.scale,
        size.y / base.scale
    );
}

std::shared_ptr<CFBO> CFBOProvider::create (
    const std::string& name, ITexture::TextureFormat format, ITexture::TextureFlags flags, float scale,
    glm::vec2 realSize, glm::vec2 textureSize
) {
    return this->m_fbos[name] = std::make_shared <CFBO> (
        name,
        ITexture::TextureFormat::ARGB8888,
        flags,
        scale,
        realSize.x,
        realSize.y,
        textureSize.x,
        textureSize.y
    );
}

std::shared_ptr<CFBO> CFBOProvider::alias (const std::string& newName, const std::string& original) {
    return this->m_fbos[newName] = this->m_fbos[original];
}

std::shared_ptr<CFBO> CFBOProvider::find (const std::string& name) const {
    const auto it = this->m_fbos.find (name);

    if (it != this->m_fbos.end ())
        return it->second;

    if (this->m_parent == nullptr) {
        return nullptr;
    }

    return this->m_parent->find (name);
}