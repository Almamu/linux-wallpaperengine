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
    for (const auto& cur : this->m_fbos)
        if (cur->getName () == name)
            return cur;

    return nullptr;
}

void CEffect::generatePasses () {
    for (const auto& cur : this->m_effect->getMaterials ())
        this->m_materials.emplace_back (new Effects::CMaterial (this, cur));
}

void CEffect::generateFBOs () {
    for (const auto& cur : this->m_effect->getFbos ()) {
        // TODO: IS THAT DIVISION OKAY? SHOULDN'T IT BE A MULTIPLICATION? WTF?
        this->m_fbos.push_back (new CFBO (cur->getName (),
                                          ITexture::TextureFormat::ARGB8888,         // TODO: CHANGE
                                          this->m_image->getTexture ()->getFlags (), // TODO: CHANGE
                                          cur->getScale (), this->m_image->getSize ().x / cur->getScale (),
                                          this->m_image->getSize ().y / cur->getScale (),
                                          this->m_image->getSize ().x / cur->getScale (),
                                          this->m_image->getSize ().y / cur->getScale ()));
    }
}

const std::vector<CFBO*>& CEffect::getFBOs () const {
    return this->m_fbos;
}

bool CEffect::isVisible () const {
    return this->m_effect->isVisible ();
}