#pragma once

#include "WallpaperEngine/Core/Core.h"

namespace WallpaperEngine::Core::Scenes {
using json = nlohmann::json;

class CProjection {
  public:
    static CProjection* fromJSON (json data);

    [[nodiscard]] const int& getWidth () const;
    [[nodiscard]] const int& getHeight () const;
    [[nodiscard]] bool isAuto () const;

    void setWidth (int width);
    void setHeight (int height);

  protected:
    CProjection (int width, int height);
    explicit CProjection (bool isAuto);

  private:
    int m_width;
    int m_height;
    bool m_isAuto;
};
} // namespace WallpaperEngine::Core::Scenes
