#include "CEffect.h"

using namespace WallpaperEngine::Render;
using namespace WallpaperEngine::Render::Objects;

CEffect::CEffect (CImage* image, const Core::Objects::CEffect* effect) :
    m_image (image),
    m_effect (effect) {
    this->generateFBOs ();
    this->generatePasses ();
}

CImage* CEffect::getImage () const {
    return this->m_image;
}

const std::vector<Effects::CMaterial*>& CEffect::getMaterials () const {
    return this->m_materials;
}

const CFBO* CEffect::findFBO (const std::string& name) const {
    const auto fbo = this->m_fbos.find (name);

    if (fbo == this->m_fbos.end ()) {
        return nullptr;
    }

    return fbo->second;
}

void CEffect::generatePasses () {
    for (const auto& cur : this->m_effect->getMaterials ())
        this->m_materials.emplace_back (new Effects::CMaterial (this, cur));
}

void CEffect::generateFBOs () {
    for (const auto& cur : this->m_effect->getFbos ()) {
        // TODO: IS THAT DIVISION OKAY? SHOULDN'T IT BE A MULTIPLICATION? WTF?
        this->m_fbos.insert (std::pair (
            cur->getName(),
            new CFBO (
                // TODO: SET PROPER FLAGS AND FORMAT
                cur->getName (), ITexture::TextureFormat::ARGB8888,
                this->m_image->getTexture ()->getFlags (), cur->getScale (),
                this->m_image->getSize ().x / cur->getScale (),
                this->m_image->getSize ().y / cur->getScale (),
                this->m_image->getSize ().x / cur->getScale (),
                this->m_image->getSize ().y / cur->getScale ()
            )
        ));
    }
}

const std::map<std::string, CFBO*>& CEffect::getFBOs () const {
    return this->m_fbos;
}

bool CEffect::isVisible () const {
    return this->m_effect->isVisible ();
}