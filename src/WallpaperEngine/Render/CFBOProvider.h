#pragma once

#include "CFBO.h"
#include "WallpaperEngine/Data/Model/Effect.h"

namespace WallpaperEngine::Render {
using namespace WallpaperEngine::Data::Model;

class CFBOProvider {
  public:
    CFBOProvider (const CFBOProvider* parent);

    std::shared_ptr<CFBO> create (const FBO& base, ITexture::TextureFlags flags, glm::vec2 size);
    std::shared_ptr<CFBO> create (
        const std::string& name, ITexture::TextureFormat format, ITexture::TextureFlags flags, float scale,
        glm::vec2 realSize, glm::vec2 textureSize);
    std::shared_ptr<CFBO> alias (const std::string& newName, const std::string& original);
    [[nodiscard]] std::shared_ptr<CFBO> find (const std::string& name) const;

  private:
    const CFBOProvider* m_parent;
    std::map <std::string, std::shared_ptr<CFBO>> m_fbos = {};
};
}
