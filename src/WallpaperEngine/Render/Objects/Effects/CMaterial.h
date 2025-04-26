#pragma once

#include "WallpaperEngine/Core/Objects/CEffect.h"
#include "WallpaperEngine/Core/Objects/Images/CMaterial.h"

#include "WallpaperEngine/Render/Objects/CEffect.h"
#include "WallpaperEngine/Render/Objects/Effects/CPass.h"

#include "WallpaperEngine/Render/Helpers/CContextAware.h"

#include "CPass.h"

using namespace WallpaperEngine;

namespace WallpaperEngine::Render::Objects {
class CEffect;
class CImage;
} // namespace WallpaperEngine::Render::Objects

namespace WallpaperEngine::Render::Objects::Effects {
class CPass;

class CMaterial final : public Helpers::CContextAware {
    friend class CPass;

  public:
    CMaterial (const Render::Objects::CEffect* effect, const Core::Objects::Images::CMaterial* material);

    [[nodiscard]] const std::vector<CPass*>& getPasses () const;
    [[nodiscard]] CImage* getImage () const;
    [[nodiscard]] const Core::Objects::Images::CMaterial* getMaterial () const;
    [[nodiscard]] const CEffect* getEffect () const;

  private:
    void generatePasses ();

    const Render::Objects::CEffect* m_effect;
    const Core::Objects::Images::CMaterial* m_material;

    std::vector<CPass*> m_passes = {};
};
} // namespace WallpaperEngine::Render::Objects::Effects
