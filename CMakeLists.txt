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
        WallpaperEngine/Render/Shaders/Compiler.h
        WallpaperEngine/Render/Shaders/Compiler.cpp
        WallpaperEngine/project.cpp
        WallpaperEngine/project.h
        WallpaperEngine/scene.cpp
        WallpaperEngine/scene.h
        WallpaperEngine/object.cpp
        WallpaperEngine/object.h
        WallpaperEngine/camera.cpp
        WallpaperEngine/camera.h
        WallpaperEngine/Core/Core.cpp
        WallpaperEngine/Core/Core.h
        WallpaperEngine/image.cpp
        WallpaperEngine/image.h
        WallpaperEngine/object3d.cpp
        WallpaperEngine/object3d.h
        WallpaperEngine/effect.cpp
        WallpaperEngine/effect.h
        WallpaperEngine/FileSystem/utils.cpp
        WallpaperEngine/FileSystem/utils.h
        WallpaperEngine/video/renderer.cpp
        WallpaperEngine/video/renderer.h
        WallpaperEngine/video/node.cpp
        WallpaperEngine/video/node.h
        WallpaperEngine/video/material.cpp
        WallpaperEngine/video/material.h
        WallpaperEngine/texture.cpp
        WallpaperEngine/texture.h
        WallpaperEngine/Irrlicht/CImageLoaderTEX.h
        WallpaperEngine/Irrlicht/CImageLoaderTEX.cpp
        WallpaperEngine/Irrlicht/CPkgReader.h
        WallpaperEngine/Irrlicht/CPkgReader.cpp
        WallpaperEngine/Irrlicht/CFileList.h
        WallpaperEngine/Irrlicht/CFileList.cpp
        WallpaperEngine/Irrlicht/Irrlicht.cpp
        WallpaperEngine/Irrlicht/Irrlicht.h
        WallpaperEngine/sound.cpp
        WallpaperEngine/sound.h

        WallpaperEngine/Core/CProject.cpp
        WallpaperEngine/Core/CProject.h
        WallpaperEngine/Core/CScene.cpp
        WallpaperEngine/Core/CScene.h
        WallpaperEngine/Core/CObject.cpp
        WallpaperEngine/Core/CObject.h

        WallpaperEngine/Core/Projects/CProperty.h
        WallpaperEngine/Core/Projects/CProperty.cpp
        WallpaperEngine/Core/Projects/CPropertyColor.h
        WallpaperEngine/Core/Projects/CPropertyColor.cpp

        WallpaperEngine/Core/Scenes/CCamera.cpp
        WallpaperEngine/Core/Scenes/CCamera.h
        WallpaperEngine/Core/Scenes/CProjection.cpp
        WallpaperEngine/Core/Scenes/CProjection.h

        WallpaperEngine/Core/Objects/CImage.cpp
        WallpaperEngine/Core/Objects/CImage.h
        WallpaperEngine/Core/Objects/CSound.cpp
        WallpaperEngine/Core/Objects/CSound.h
        WallpaperEngine/Core/Objects/CEffect.cpp
        WallpaperEngine/Core/Objects/CEffect.h
        WallpaperEngine/Core/Objects/Particles/CParticle.cpp
        WallpaperEngine/Core/Objects/Particles/CParticle.h

        WallpaperEngine/Core/Objects/Particles/CControlPoint.cpp
        WallpaperEngine/Core/Objects/Particles/CControlPoint.h
        WallpaperEngine/Core/Objects/Particles/CEmitter.cpp
        WallpaperEngine/Core/Objects/Particles/CEmitter.h
        WallpaperEngine/Core/Objects/Particles/CInitializer.cpp
        WallpaperEngine/Core/Objects/Particles/CInitializer.h

        WallpaperEngine/Core/Objects/Particles/Initializers/CLifeTimeRandom.cpp
        WallpaperEngine/Core/Objects/Particles/Initializers/CLifeTimeRandom.h
        WallpaperEngine/Core/Objects/Particles/Initializers/CSizeRandom.cpp
        WallpaperEngine/Core/Objects/Particles/Initializers/CSizeRandom.h
        WallpaperEngine/Core/Objects/Particles/Initializers/CRotationRandom.cpp
        WallpaperEngine/Core/Objects/Particles/Initializers/CRotationRandom.h
        WallpaperEngine/Core/Objects/Particles/Initializers/CVelocityRandom.cpp
        WallpaperEngine/Core/Objects/Particles/Initializers/CVelocityRandom.h
        WallpaperEngine/Core/Objects/Particles/Initializers/CColorRandom.cpp
        WallpaperEngine/Core/Objects/Particles/Initializers/CColorRandom.h
        WallpaperEngine/Core/Objects/Particles/Initializers/CAlphaRandom.cpp
        WallpaperEngine/Core/Objects/Particles/Initializers/CAlphaRandom.h
        WallpaperEngine/Core/Objects/Particles/Initializers/CAngularVelocityRandom.cpp
        WallpaperEngine/Core/Objects/Particles/Initializers/CAngularVelocityRandom.h

        WallpaperEngine/Core/Objects/Images/CMaterial.cpp
        WallpaperEngine/Core/Objects/Images/CMaterial.h

        WallpaperEngine/Core/Objects/Images/Materials/CPassess.cpp
        WallpaperEngine/Core/Objects/Images/Materials/CPassess.h
)

target_link_libraries(wallengine ${X11_LIBRARIES} ${X11_Xxf86vm_LIB} ${OPENGL_LIBRARIES} ${GLUT_LIBRARIES} ${ZLIB_LIBRARIES} ${IRRLICHT_LIBRARY} ${LZ4_LIBRARY} ${SDL_LIBRARY} ${SDL_MIXER_LIBRARIES})