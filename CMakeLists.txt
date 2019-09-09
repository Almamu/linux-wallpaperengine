cmake_minimum_required(VERSION 3.0)
project(wallengine)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "-fpermissive")
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/CMakeModules")
set(OpenGL_GL_PREFERENCE "LEGACY")

find_package(X11 REQUIRED)
find_package(Xrandr REQUIRED)
find_package(OpenGL REQUIRED)
find_package(GLUT REQUIRED)
find_package(ZLIB REQUIRED)
find_package(Irrlicht REQUIRED)
find_package(SDL REQUIRED)
find_package(SDL_mixer REQUIRED)
find_package(LZ4 REQUIRED)

include_directories(${X11_INCLUDE_DIR} ${XRANDR_INCLUDE_DIR} ${IRRLICHT_INCLUDE_DIR} ${LZ4_INCLUDE_DIR} ${SDL_INCLUDE_DIRS} ${SDL_MIXER_INCLUDE_DIRS} src include)

add_executable(
        wallengine
        main.cpp

        src/WallpaperEngine/Core/Core.h
        src/WallpaperEngine/Core/Core.cpp

        src/WallpaperEngine/Render/Shaders/Parameters/CShaderParameter.h
        src/WallpaperEngine/Render/Shaders/Parameters/CShaderParameter.cpp
        src/WallpaperEngine/Render/Shaders/Parameters/CShaderParameterFloat.h
        src/WallpaperEngine/Render/Shaders/Parameters/CShaderParameterFloat.cpp
        src/WallpaperEngine/Render/Shaders/Parameters/CShaderParameterInteger.h
        src/WallpaperEngine/Render/Shaders/Parameters/CShaderParameterInteger.cpp
        src/WallpaperEngine/Render/Shaders/Parameters/CShaderParameterVector2.h
        src/WallpaperEngine/Render/Shaders/Parameters/CShaderParameterVector2.cpp
        src/WallpaperEngine/Render/Shaders/Parameters/CShaderParameterVector3.h
        src/WallpaperEngine/Render/Shaders/Parameters/CShaderParameterVector3.cpp
        src/WallpaperEngine/Render/Shaders/Parameters/CShaderParameterVector4.h
        src/WallpaperEngine/Render/Shaders/Parameters/CShaderParameterVector4.cpp

        src/WallpaperEngine/Render/Shaders/Compiler.h
        src/WallpaperEngine/Render/Shaders/Compiler.cpp

        src/WallpaperEngine/Render/CScene.h
        src/WallpaperEngine/Render/CScene.cpp
        src/WallpaperEngine/Render/CCamera.h
        src/WallpaperEngine/Render/CCamera.cpp
        src/WallpaperEngine/Render/CObject.h
        src/WallpaperEngine/Render/CObject.cpp

        src/WallpaperEngine/Render/Objects/CImage.h
        src/WallpaperEngine/Render/Objects/CImage.cpp
        src/WallpaperEngine/Render/Objects/CSound.h
        src/WallpaperEngine/Render/Objects/CSound.cpp

        src/WallpaperEngine/FileSystem/FileSystem.cpp
        src/WallpaperEngine/FileSystem/FileSystem.h

        src/WallpaperEngine/Irrlicht/CContext.h
        src/WallpaperEngine/Irrlicht/CContext.cpp
        src/WallpaperEngine/Irrlicht/CImageLoaderTEX.h
        src/WallpaperEngine/Irrlicht/CImageLoaderTEX.cpp
        src/WallpaperEngine/Irrlicht/CPkgReader.h
        src/WallpaperEngine/Irrlicht/CPkgReader.cpp
        src/WallpaperEngine/Irrlicht/CFileList.h
        src/WallpaperEngine/Irrlicht/CFileList.cpp

        src/WallpaperEngine/Core/CProject.cpp
        src/WallpaperEngine/Core/CProject.h
        src/WallpaperEngine/Core/CScene.cpp
        src/WallpaperEngine/Core/CScene.h
        src/WallpaperEngine/Core/CObject.cpp
        src/WallpaperEngine/Core/CObject.h

        src/WallpaperEngine/Core/Projects/CProperty.h
        src/WallpaperEngine/Core/Projects/CProperty.cpp
        src/WallpaperEngine/Core/Projects/CPropertyColor.h
        src/WallpaperEngine/Core/Projects/CPropertyColor.cpp

        src/WallpaperEngine/Core/Scenes/CCamera.cpp
        src/WallpaperEngine/Core/Scenes/CCamera.h
        src/WallpaperEngine/Core/Scenes/CProjection.cpp
        src/WallpaperEngine/Core/Scenes/CProjection.h

        src/WallpaperEngine/Core/Objects/CImage.cpp
        src/WallpaperEngine/Core/Objects/CImage.h
        src/WallpaperEngine/Core/Objects/CSound.cpp
        src/WallpaperEngine/Core/Objects/CSound.h
        src/WallpaperEngine/Core/Objects/CEffect.cpp
        src/WallpaperEngine/Core/Objects/CEffect.h
        src/WallpaperEngine/Core/Objects/CParticle.cpp
        src/WallpaperEngine/Core/Objects/CParticle.h

        src/WallpaperEngine/Core/Objects/Effects/CShaderConstant.h
        src/WallpaperEngine/Core/Objects/Effects/CShaderConstant.cpp
        src/WallpaperEngine/Core/Objects/Effects/CShaderConstantFloat.h
        src/WallpaperEngine/Core/Objects/Effects/CShaderConstantFloat.cpp
        src/WallpaperEngine/Core/Objects/Effects/CShaderConstantString.h
        src/WallpaperEngine/Core/Objects/Effects/CShaderConstantString.cpp
        src/WallpaperEngine/Core/Objects/Effects/CShaderConstantInteger.h
        src/WallpaperEngine/Core/Objects/Effects/CShaderConstantInteger.cpp

        src/WallpaperEngine/Core/Objects/Particles/CControlPoint.cpp
        src/WallpaperEngine/Core/Objects/Particles/CControlPoint.h
        src/WallpaperEngine/Core/Objects/Particles/CEmitter.cpp
        src/WallpaperEngine/Core/Objects/Particles/CEmitter.h
        src/WallpaperEngine/Core/Objects/Particles/CInitializer.cpp
        src/WallpaperEngine/Core/Objects/Particles/CInitializer.h

        src/WallpaperEngine/Core/Objects/Particles/Initializers/CLifeTimeRandom.cpp
        src/WallpaperEngine/Core/Objects/Particles/Initializers/CLifeTimeRandom.h
        src/WallpaperEngine/Core/Objects/Particles/Initializers/CSizeRandom.cpp
        src/WallpaperEngine/Core/Objects/Particles/Initializers/CSizeRandom.h
        src/WallpaperEngine/Core/Objects/Particles/Initializers/CRotationRandom.cpp
        src/WallpaperEngine/Core/Objects/Particles/Initializers/CRotationRandom.h
        src/WallpaperEngine/Core/Objects/Particles/Initializers/CVelocityRandom.cpp
        src/WallpaperEngine/Core/Objects/Particles/Initializers/CVelocityRandom.h
        src/WallpaperEngine/Core/Objects/Particles/Initializers/CColorRandom.cpp
        src/WallpaperEngine/Core/Objects/Particles/Initializers/CColorRandom.h
        src/WallpaperEngine/Core/Objects/Particles/Initializers/CAlphaRandom.cpp
        src/WallpaperEngine/Core/Objects/Particles/Initializers/CAlphaRandom.h
        src/WallpaperEngine/Core/Objects/Particles/Initializers/CAngularVelocityRandom.cpp
        src/WallpaperEngine/Core/Objects/Particles/Initializers/CAngularVelocityRandom.h

        src/WallpaperEngine/Core/Objects/Images/CMaterial.cpp
        src/WallpaperEngine/Core/Objects/Images/CMaterial.h

        src/WallpaperEngine/Core/Objects/Images/Materials/CPassess.cpp
        src/WallpaperEngine/Core/Objects/Images/Materials/CPassess.h
)

target_link_libraries(wallengine ${X11_LIBRARIES} ${XRANDR_LIBRARIES} ${X11_Xxf86vm_LIB} ${OPENGL_LIBRARIES} ${GLUT_LIBRARIES} ${ZLIB_LIBRARIES} ${IRRLICHT_LIBRARY} ${LZ4_LIBRARY} ${SDL_LIBRARY} ${SDL_MIXER_LIBRARIES})