#pragma once

#include <glm/vec2.hpp>

#include "CFBO.h"
#include "WallpaperEngine/Data/Model/Effect.h"

namespace WallpaperEngine::Render {
using namespace WallpaperEngine::Data::Model;

class FBOProvider {
public:
    explicit FBOProvider (const FBOProvider* parent);

    std::shared_ptr<CFBO> create (const FBO& base, uint32_t flags, glm::vec2 size);
    std::shared_ptr<CFBO> create (
	const std::string& name, TextureFormat format, uint32_t flags, float scale, glm::vec2 realSize,
	glm::vec2 textureSize
    );
    std::shared_ptr<CFBO> alias (const std::string& newName, const std::string& original);
    [[nodiscard]] std::shared_ptr<CFBO> find (const std::string& name) const;

private:
    const FBOProvider* m_parent;
    std::map<std::string, std::shared_ptr<CFBO>> m_fbos = {};
};
}
