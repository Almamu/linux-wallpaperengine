#include "TestingMouseInput.h"

using namespace WallpaperEngine::Testing::Input;


void TestingMouseInput::update () {
}

glm::dvec2 TestingMouseInput::position () const {
    return {};
}

MouseClickStatus TestingMouseInput::leftClick () const {
    return MouseClickStatus::Released;
}

MouseClickStatus TestingMouseInput::rightClick () const {
    return MouseClickStatus::Released;
}