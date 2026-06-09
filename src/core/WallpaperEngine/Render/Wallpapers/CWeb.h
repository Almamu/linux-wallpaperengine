#pragma once

#include <glm/ext.hpp>
#include <glm/glm.hpp>

#include <string>

#include "WallpaperEngine/Audio/AudioStream.h"
#include "WallpaperEngine/Data/Model/Wallpaper.h"
#include "WallpaperEngine/Input/MouseClickStatus.h"
#include "WallpaperEngine/Render/CWallpaper.h"

namespace WallpaperEngine::Render::Wallpapers {

class CWeb : public CWallpaper {
public:
    CWeb (const Wallpaper& wallpaper, RenderContext& context, AudioContext& audioContext, wp_mouse_input* mouseInput);
    ~CWeb () override;

    [[nodiscard]] int getWidth () const override { return this->m_width; }
    [[nodiscard]] int getHeight () const override { return this->m_height; }

    void setSize (int width, int height);

protected:
    void renderFrame () override;
    void updateMouse ();
    const Web& getWeb () const { return *this->getWallpaperData ().as<Web> (); }

    friend class CWallpaper;

private:
    std::string m_uuid;

    int m_width = 16;
    int m_height = 17;

    WallpaperEngine::Input::MouseClickStatus m_leftClick = Input::Released;
    WallpaperEngine::Input::MouseClickStatus m_rightClick = Input::Released;

    glm::vec2 m_mousePosition = {};
    glm::vec2 m_mousePositionLast = {};
};

} // namespace WallpaperEngine::Render::Wallpapers
