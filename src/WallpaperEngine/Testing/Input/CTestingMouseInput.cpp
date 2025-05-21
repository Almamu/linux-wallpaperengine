#include "CTestingMouseInput.h"

using namespace WallpaperEngine::Testing::Input;


void CTestingMouseInput::update () {

}

glm::dvec2 CTestingMouseInput::position () const {
    return {};
}

MouseClickStatus CTestingMouseInput::leftClick () const {
    return MouseClickStatus::Released;
}

MouseClickStatus CTestingMouseInput::rightClick () const {
    return MouseClickStatus::Released;
}