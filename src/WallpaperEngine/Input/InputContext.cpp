#include "InputContext.h"

using namespace WallpaperEngine::Input;
using namespace WallpaperEngine::Render::Drivers;

InputContext::InputContext (MouseInput& mouseInput) : m_mouse (mouseInput) {}

void InputContext::update () {
    this->m_mouse.update ();
}

const MouseInput& InputContext::getMouseInput () const {
    return this->m_mouse;
}