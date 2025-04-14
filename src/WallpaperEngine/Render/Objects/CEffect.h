#pragma once

#include "WallpaperEngine/Render/CFBO.h"
#include "WallpaperEngine/Render/Objects/CImage.h"
#include "WallpaperEngine/Render/Objects/Effects/CMaterial.h"
#include "WallpaperEngine/Render/Objects/Effects/CPass.h"

namespace WallpaperEngine::Render::Objects::Effects {
class CMaterial;
}

namespace WallpaperEngine::Render::Objects {
class CImage;

class CEffect {
  public:
    CEffect (CImage* image, const Core::Objects::CEffect* effect);

    CImage* getImage () const;

    const std::vector<Effects::CMaterial*>& getMaterials () const;

    const CFBO* findFBO (const std::string& name) const;
    bool isVisible () const;

  private:
    void generatePasses ();
    void generateFBOs ();

    CImage* m_image;
    const Core::Objects::CEffect* m_effect;

    std::vector<CFBO*> m_fbos;
    std::vector<Effects::CMaterial*> m_materials;
};
} // namespace WallpaperEngine::Render::Objects
