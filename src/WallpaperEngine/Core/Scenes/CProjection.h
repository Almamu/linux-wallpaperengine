#pragma once

#include "WallpaperEngine/Core/Core.h"

namespace WallpaperEngine::Core::Scenes {
using json = nlohmann::json;

class CProjection {
  public:
    static const CProjection* fromJSON (const json::const_iterator& data);

    [[nodiscard]] const int& getWidth () const;
    [[nodiscard]] const int& getHeight () const;
    [[nodiscard]] bool isAuto () const;

    // TODO: CHANGE THIS SO THE RENDER IS THE ONE RESPONSIBLE FOR THIS?
    void setWidth (int width) const;
    void setHeight (int height) const;

  protected:
    CProjection (int width, int height);
    explicit CProjection (bool isAuto);

  private:
    mutable int m_width;
    mutable int m_height;
    const bool m_isAuto;
};
} // namespace WallpaperEngine::Core::Scenes
