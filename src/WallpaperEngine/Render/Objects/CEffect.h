#pragma once

#include "WallpaperEngine/Render/CFBO.h"
#include "WallpaperEngine/Render/Objects/CImage.h"
#include "WallpaperEngine/Render/Objects/Effects/CMaterial.h"
#include "WallpaperEngine/Render/Objects/Effects/CPass.h"

#include "WallpaperEngine/PrettyPrinter/CPrettyPrinter.h"

namespace WallpaperEngine::PrettyPrinter {
class CPrettyPrinter;
}

namespace WallpaperEngine::Render::Objects::Effects {
class CMaterial;
}

namespace WallpaperEngine::Render::Objects {
class CImage;

class CEffect {
  public:
    CEffect (CImage* image, const Core::Objects::CEffect* effect);

    [[nodiscard]] CImage* getImage () const;

    [[nodiscard]] const std::vector<Effects::CMaterial*>& getMaterials () const;

    [[nodiscard]] const CFBO* findFBO (const std::string& name) const;
    [[nodiscard]] bool isVisible () const;

  protected:
    friend class WallpaperEngine::PrettyPrinter::CPrettyPrinter;

    [[nodiscard]] const std::map<std::string, CFBO*>& getFBOs () const;

  private:
    void generatePasses ();
    void generateFBOs ();

    CImage* m_image;
    const Core::Objects::CEffect* m_effect;

    std::map<std::string, CFBO*> m_fbos;
    std::vector<Effects::CMaterial*> m_materials;
};
} // namespace WallpaperEngine::Render::Objects
