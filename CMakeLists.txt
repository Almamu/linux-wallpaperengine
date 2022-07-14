cmake_minimum_required(VERSION 3.12)
project(wallengine)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/CMakeModules")
set(OpenGL_GL_PREFERENCE "LEGACY")

# if you're developing you might find this debug option useful for shader output, although RenderDoc is encouraged
add_compile_definitions(DEBUG=0)

find_package(X11 REQUIRED)
find_package(Xrandr REQUIRED)
find_package(OpenGL REQUIRED)
find_package(GLEW REQUIRED)
find_package(GLUT REQUIRED)
find_package(ZLIB REQUIRED)
find_package(SDL REQUIRED)
find_package(LZ4 REQUIRED)
find_package(FFMPEG REQUIRED)
find_package(FreeImage REQUIRED)


include_directories(
    ${X11_INCLUDE_DIR}
    ${XRANDR_INCLUDE_DIR}
    ${GLEW_INCLUDE_DIR}
    ${LZ4_INCLUDE_DIR}
    ${SDL_INCLUDE_DIRS}
    ${SDL_MIXER_INCLUDE_DIRS}
    ${FFMPEG_INCLUDE_DIR}
    ${FREEIMAGE_INCLUDE_DIR}
    src
    include)

add_executable(
        wallengine
        main.cpp

        src/WallpaperEngine/Assets/CPackageLoadException.cpp
        src/WallpaperEngine/Assets/CPackageLoadException.h
        src/WallpaperEngine/Assets/CAssetLoadException.cpp
        src/WallpaperEngine/Assets/CAssetLoadException.h
        src/WallpaperEngine/Assets/CContainer.h
        src/WallpaperEngine/Assets/CContainer.cpp
        src/WallpaperEngine/Assets/CCombinedContainer.h
        src/WallpaperEngine/Assets/CCombinedContainer.cpp
        src/WallpaperEngine/Assets/CDirectory.h
        src/WallpaperEngine/Assets/CDirectory.cpp
        src/WallpaperEngine/Assets/CPackage.h
        src/WallpaperEngine/Assets/CPackage.cpp
        src/WallpaperEngine/Assets/ITexture.h
        src/WallpaperEngine/Assets/CTexture.h
        src/WallpaperEngine/Assets/CTexture.cpp

        src/WallpaperEngine/Core/Core.h
        src/WallpaperEngine/Core/Core.cpp

        src/WallpaperEngine/Audio/CAudioStream.cpp
        src/WallpaperEngine/Audio/CAudioStream.h

        src/WallpaperEngine/Input/CMouseInput.h
        src/WallpaperEngine/Input/CMouseInput.cpp

        src/WallpaperEngine/Render/Shaders/Variables/CShaderVariable.h
        src/WallpaperEngine/Render/Shaders/Variables/CShaderVariable.cpp
        src/WallpaperEngine/Render/Shaders/Variables/CShaderVariableFloat.h
        src/WallpaperEngine/Render/Shaders/Variables/CShaderVariableFloat.cpp
        src/WallpaperEngine/Render/Shaders/Variables/CShaderVariableInteger.h
        src/WallpaperEngine/Render/Shaders/Variables/CShaderVariableInteger.cpp
        src/WallpaperEngine/Render/Shaders/Variables/CShaderVariableVector2.h
        src/WallpaperEngine/Render/Shaders/Variables/CShaderVariableVector2.cpp
        src/WallpaperEngine/Render/Shaders/Variables/CShaderVariableVector3.h
        src/WallpaperEngine/Render/Shaders/Variables/CShaderVariableVector3.cpp
        src/WallpaperEngine/Render/Shaders/Variables/CShaderVariableVector4.h
        src/WallpaperEngine/Render/Shaders/Variables/CShaderVariableVector4.cpp

        src/WallpaperEngine/Render/Shaders/Compiler.h
        src/WallpaperEngine/Render/Shaders/Compiler.cpp

        src/WallpaperEngine/Render/CContext.h
        src/WallpaperEngine/Render/CContext.cpp

        src/WallpaperEngine/Render/CWallpaper.h
        src/WallpaperEngine/Render/CWallpaper.cpp
        src/WallpaperEngine/Render/CScene.h
        src/WallpaperEngine/Render/CScene.cpp
        src/WallpaperEngine/Render/CVideo.h
        src/WallpaperEngine/Render/CVideo.cpp
        src/WallpaperEngine/Render/CCamera.h
        src/WallpaperEngine/Render/CCamera.cpp
        src/WallpaperEngine/Render/CObject.h
        src/WallpaperEngine/Render/CObject.cpp

        src/WallpaperEngine/Render/Objects/CImage.h
        src/WallpaperEngine/Render/Objects/CImage.cpp
        src/WallpaperEngine/Render/Objects/CSound.h
        src/WallpaperEngine/Render/Objects/CSound.cpp
        src/WallpaperEngine/Render/Objects/CEffect.h
        src/WallpaperEngine/Render/Objects/CEffect.cpp

        src/WallpaperEngine/Render/CFBO.h
        src/WallpaperEngine/Render/CFBO.cpp
        src/WallpaperEngine/Render/Objects/Effects/CPass.h
        src/WallpaperEngine/Render/Objects/Effects/CPass.cpp
        src/WallpaperEngine/Render/Objects/Effects/CMaterial.h
        src/WallpaperEngine/Render/Objects/Effects/CMaterial.cpp

        src/WallpaperEngine/FileSystem/FileSystem.cpp
        src/WallpaperEngine/FileSystem/FileSystem.h

        src/WallpaperEngine/Core/Types/FloatColor.h
        src/WallpaperEngine/Core/Types/IntegerColor.h

        src/WallpaperEngine/Core/CProject.cpp
        src/WallpaperEngine/Core/CProject.h
        src/WallpaperEngine/Core/CWallpaper.cpp
        src/WallpaperEngine/Core/CWallpaper.h
        src/WallpaperEngine/Core/CScene.cpp
        src/WallpaperEngine/Core/CScene.h
        src/WallpaperEngine/Core/CVideo.cpp
        src/WallpaperEngine/Core/CVideo.h
        src/WallpaperEngine/Core/CObject.cpp
        src/WallpaperEngine/Core/CObject.h

        src/WallpaperEngine/Core/Projects/CProperty.h
        src/WallpaperEngine/Core/Projects/CProperty.cpp
        src/WallpaperEngine/Core/Projects/CPropertyColor.h
        src/WallpaperEngine/Core/Projects/CPropertyColor.cpp
        src/WallpaperEngine/Core/Projects/CPropertyBoolean.h
        src/WallpaperEngine/Core/Projects/CPropertyBoolean.cpp
        src/WallpaperEngine/Core/Projects/CPropertySlider.h
        src/WallpaperEngine/Core/Projects/CPropertySlider.cpp
        src/WallpaperEngine/Core/Projects/CPropertyCombo.h
        src/WallpaperEngine/Core/Projects/CPropertyCombo.cpp

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

        src/WallpaperEngine/Core/Objects/Effects/CFBO.h
        src/WallpaperEngine/Core/Objects/Effects/CFBO.cpp
        src/WallpaperEngine/Core/Objects/Effects/CBind.h
        src/WallpaperEngine/Core/Objects/Effects/CBind.cpp

        src/WallpaperEngine/Core/Objects/Effects/Constants/CShaderConstant.h
        src/WallpaperEngine/Core/Objects/Effects/Constants/CShaderConstant.cpp
        src/WallpaperEngine/Core/Objects/Effects/Constants/CShaderConstantFloat.h
        src/WallpaperEngine/Core/Objects/Effects/Constants/CShaderConstantFloat.cpp
        src/WallpaperEngine/Core/Objects/Effects/Constants/CShaderConstantVector3.h
        src/WallpaperEngine/Core/Objects/Effects/Constants/CShaderConstantVector3.cpp
        src/WallpaperEngine/Core/Objects/Effects/Constants/CShaderConstantVector4.h
        src/WallpaperEngine/Core/Objects/Effects/Constants/CShaderConstantVector4.cpp
        src/WallpaperEngine/Core/Objects/Effects/Constants/CShaderConstantInteger.h
        src/WallpaperEngine/Core/Objects/Effects/Constants/CShaderConstantInteger.cpp

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
        src/WallpaperEngine/Core/Objects/Particles/Initializers/CTurbulentVelocityRandom.cpp
        src/WallpaperEngine/Core/Objects/Particles/Initializers/CTurbulentVelocityRandom.h

        src/WallpaperEngine/Core/Objects/Images/CMaterial.cpp
        src/WallpaperEngine/Core/Objects/Images/CMaterial.h

        src/WallpaperEngine/Core/Objects/Images/Materials/CPass.cpp
        src/WallpaperEngine/Core/Objects/Images/Materials/CPass.h
)

target_link_libraries(wallengine
    ${X11_LIBRARIES}
    ${Xrandr_LIBRARIES}
    ${X11_Xxf86vm_LIB}
    ${OPENGL_LIBRARIES}
    ${GLEW_LIBRARIES}
    ${GLUT_LIBRARIES}
    ${ZLIB_LIBRARIES}
    ${LZ4_LIBRARY}
    ${SDL_LIBRARY}
    ${SDL_MIXER_LIBRARIES}
    ${FFMPEG_LIBRARIES}
    ${FREEIMAGE_LIBRARIES}
    glfw)

set(CMAKE_REQUIRED_LIBRARIES ${X11_LIBRARIES})

# make sure some of the X11 functions we'll use are available
check_function_exists(XSetIOErrorExitHandler HAVE_XSETIOERROREXITHANDLER)

if(HAVE_XSETIOERROREXITHANDLER)
    add_compile_definitions(HAVE_XSETIOERROREXITHANDLER=1)
endif()