cmake_minimum_required(VERSION 3.0)
project(wallengine)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "-fpermissive")
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/CMakeModules")
set(OpenGL_GL_PREFERENCE "LEGACY")

find_package(X11 REQUIRED)
find_package(OpenGL REQUIRED)
find_package(GLUT REQUIRED)
find_package(ZLIB REQUIRED)
find_package(Irrlicht REQUIRED)
find_package(SDL REQUIRED)
find_package(SDL_mixer REQUIRED)
find_package(LZ4 REQUIRED)

include_directories(${X11_INCLUDE_DIR} ${IRRLICHT_INCLUDE_DIR} ${LZ4_INCLUDE_DIR} ${SDL_INCLUDE_DIRS} ${SDL_MIXER_INCLUDE_DIRS} .)

add_executable(
        wallengine
        main.cpp
        wallpaperengine/shaders/compiler.h
        wallpaperengine/shaders/compiler.cpp
        wallpaperengine/project.cpp
        wallpaperengine/project.h
        wallpaperengine/scene.cpp
        wallpaperengine/scene.h
        wallpaperengine/object.cpp
        wallpaperengine/object.h
        wallpaperengine/camera.cpp
        wallpaperengine/camera.h
        wallpaperengine/core.cpp
        wallpaperengine/core.h
        wallpaperengine/image.cpp
        wallpaperengine/image.h
        wallpaperengine/object3d.cpp
        wallpaperengine/object3d.h
        wallpaperengine/effect.cpp
        wallpaperengine/effect.h
        wallpaperengine/fs/utils.cpp
        wallpaperengine/fs/utils.h
        wallpaperengine/irrlicht.cpp
        wallpaperengine/irrlicht.h
        wallpaperengine/video/renderer.cpp
        wallpaperengine/video/renderer.h
        wallpaperengine/video/node.cpp
        wallpaperengine/video/node.h
        wallpaperengine/video/material.cpp
        wallpaperengine/video/material.h
        wallpaperengine/texture.cpp
        wallpaperengine/texture.h
        wallpaperengine/irr/CImageLoaderTEX.h
        wallpaperengine/irr/CImageLoaderTEX.cpp
        wallpaperengine/irr/CPkgReader.h
        wallpaperengine/irr/CPkgReader.cpp
        wallpaperengine/irr/CFileList.h
        wallpaperengine/irr/CFileList.cpp
        wallpaperengine/sound.cpp
        wallpaperengine/sound.h

        wallpaperengine/core/CProject.cpp
        wallpaperengine/core/CProject.h
        wallpaperengine/core/CScene.cpp
        wallpaperengine/core/CScene.h
        wallpaperengine/core/CObject.cpp
        wallpaperengine/core/CObject.h

        wallpaperengine/core/Scenes/CCamera.cpp
        wallpaperengine/core/Scenes/CCamera.h
        wallpaperengine/core/Scenes/CProjection.cpp
        wallpaperengine/core/Scenes/CProjection.h

        wallpaperengine/core/Objects/CImage.cpp
        wallpaperengine/core/Objects/CImage.h
        wallpaperengine/core/Objects/CSound.cpp
        wallpaperengine/core/Objects/CSound.h
        wallpaperengine/core/Objects/CEffect.cpp
        wallpaperengine/core/Objects/CEffect.h
        wallpaperengine/core/Objects/Particles/CParticle.cpp
        wallpaperengine/core/Objects/Particles/CParticle.h

        wallpaperengine/core/Objects/Particles/CControlPoint.cpp
        wallpaperengine/core/Objects/Particles/CControlPoint.h
        wallpaperengine/core/Objects/Particles/CEmitter.cpp
        wallpaperengine/core/Objects/Particles/CEmitter.h
        wallpaperengine/core/Objects/Particles/CInitializer.cpp
        wallpaperengine/core/Objects/Particles/CInitializer.h

        wallpaperengine/core/Objects/Particles/Initializers/CLifeTimeRandom.cpp
        wallpaperengine/core/Objects/Particles/Initializers/CLifeTimeRandom.h
        wallpaperengine/core/Objects/Particles/Initializers/CSizeRandom.cpp
        wallpaperengine/core/Objects/Particles/Initializers/CSizeRandom.h
        wallpaperengine/core/Objects/Particles/Initializers/CRotationRandom.cpp
        wallpaperengine/core/Objects/Particles/Initializers/CRotationRandom.h
        wallpaperengine/core/Objects/Particles/Initializers/CVelocityRandom.cpp
        wallpaperengine/core/Objects/Particles/Initializers/CVelocityRandom.h
        wallpaperengine/core/Objects/Particles/Initializers/CColorRandom.cpp
        wallpaperengine/core/Objects/Particles/Initializers/CColorRandom.h
        wallpaperengine/core/Objects/Particles/Initializers/CAlphaRandom.cpp
        wallpaperengine/core/Objects/Particles/Initializers/CAlphaRandom.h
        wallpaperengine/core/Objects/Particles/Initializers/CAngularVelocityRandom.cpp
        wallpaperengine/core/Objects/Particles/Initializers/CAngularVelocityRandom.h

        wallpaperengine/core/Objects/Images/CMaterial.cpp
        wallpaperengine/core/Objects/Images/CMaterial.h

        wallpaperengine/core/Objects/Images/Materials/CPassess.cpp
        wallpaperengine/core/Objects/Images/Materials/CPassess.h
)

target_link_libraries(wallengine ${X11_LIBRARIES} ${X11_Xxf86vm_LIB} ${OPENGL_LIBRARIES} ${GLUT_LIBRARIES} ${ZLIB_LIBRARIES} ${IRRLICHT_LIBRARY} ${LZ4_LIBRARY} ${SDL_LIBRARY} ${SDL_MIXER_LIBRARIES})