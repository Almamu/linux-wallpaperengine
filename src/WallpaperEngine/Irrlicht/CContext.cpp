#include <iostream>
#include <stdexcept>
#include <utility>

#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>

#include "WallpaperEngine/Irrlicht/CImageLoaderTEX.h"
#include "WallpaperEngine/Irrlicht/CPkgReader.h"

#include "WallpaperEngine/Render/Shaders/Variables/CShaderVariableFloatPointer.h"
#include "WallpaperEngine/Render/Shaders/Variables/CShaderVariableVector2Pointer.h"

#include "CContext.h"

using namespace WallpaperEngine;
using namespace WallpaperEngine::Irrlicht;

CContext::CContext (std::vector<std::string>  screens, bool isRootWindow) :
    m_screens (std::move(screens)),
    m_isRootWindow (isRootWindow)
{
}

void CContext::setDevice (irr::IrrlichtDevice* device)
{
    this->m_device = device;
}

void CContext::initializeContext ()
{
    irr::SIrrlichtCreationParameters irrlichtCreationParameters;

    // initialize event receiver first
    this->m_eventReceiver = new CEventReceiver ();

    // prepare basic configuration for irrlicht
    irrlichtCreationParameters.AntiAlias = 8;
    irrlichtCreationParameters.Bits = 16;
    // _irr_params.DeviceType = Irrlicht::EIDT_X11;
    irrlichtCreationParameters.DriverType = irr::video::EDT_OPENGL;
    irrlichtCreationParameters.Doublebuffer = false;
    irrlichtCreationParameters.EventReceiver = this->m_eventReceiver;
    irrlichtCreationParameters.Fullscreen = false;
    irrlichtCreationParameters.HandleSRGB = false;
    irrlichtCreationParameters.IgnoreInput = false;
    irrlichtCreationParameters.Stencilbuffer = true;
    irrlichtCreationParameters.UsePerformanceTimer = false;
    irrlichtCreationParameters.Vsync = false;
    irrlichtCreationParameters.WithAlphaChannel = false;
    irrlichtCreationParameters.ZBufferBits = 24;
    irrlichtCreationParameters.LoggingLevel = irr::ELL_DEBUG;

    this->initializeViewports (irrlichtCreationParameters);

    this->setDevice (irr::createDeviceEx (irrlichtCreationParameters));

    if (this->getDevice () == nullptr)
    {
        throw std::runtime_error ("Cannot create irrlicht device");
    }

    this->getDevice ()->setWindowCaption (L"Test game");

    // check for ps and vs support
    if (
        this->getDevice ()->getVideoDriver()->queryFeature (irr::video::EVDF_PIXEL_SHADER_2_0) == false &&
        this->getDevice ()->getVideoDriver()->queryFeature (irr::video::EVDF_ARB_FRAGMENT_PROGRAM_1) == false)
    {
        throw std::runtime_error ("Pixel Shader 2.0 not supported by your video driver/hardware");
    }

    if (
        this->getDevice ()->getVideoDriver()->queryFeature (irr::video::EVDF_VERTEX_SHADER_2_0) == false &&
        this->getDevice ()->getVideoDriver()->queryFeature (irr::video::EVDF_ARB_VERTEX_PROGRAM_1) == false)
    {
        throw std::runtime_error ("Vertex Shader 2.0 not supported by your video driver/hardware");
    }

    if (this->getDevice ()->getVideoDriver ()->queryFeature (irr::video::EVDF_RENDER_TO_TARGET) == false)
    {
        throw std::runtime_error ("Render to texture not supported by your video driver/hardware");
    }

    // load the assets from wallpaper engine
    this->getDevice ()->getFileSystem ()->addFileArchive ("assets.zip", true, false);

    // register custom loaders
    this->getDevice ()->getVideoDriver()->addExternalImageLoader (
            new WallpaperEngine::Irrlicht::CImageLoaderTex (this)
    );
    this->getDevice ()->getFileSystem ()->addArchiveLoader (
            new WallpaperEngine::Irrlicht::CArchiveLoaderPkg (this)
    );
    // register time shader variable
    this->insertShaderVariable (
        new Render::Shaders::Variables::CShaderVariableFloatPointer (&this->m_time, "g_Time")
    );
    // register normalized uv position for mouse
    this->insertShaderVariable (
        new Render::Shaders::Variables::CShaderVariableVector2Pointer (&this->m_pointerPosition, "g_PointerPosition")
    );
}

void CContext::initializeViewports (irr::SIrrlichtCreationParameters &irrlichtCreationParameters)
{
    if (this->m_isRootWindow == false || this->m_screens.empty () == true)
        return;

    Display* display = XOpenDisplay (nullptr);
    int xrandr_result, xrandr_error;

    if (!XRRQueryExtension (display, &xrandr_result, &xrandr_error))
    {
        std::cerr << "XRandr is not present, cannot detect specified screens, running in window mode" << std::endl;
        return;
    }

    XRRScreenResources* screenResources = XRRGetScreenResources (display, DefaultRootWindow (display));

    // there are some situations where xrandr returns null (like screen not using the extension)
    if (screenResources == nullptr)
        return;

    for (int i = 0; i < screenResources->noutput; i ++)
    {
        XRROutputInfo* info = XRRGetOutputInfo (display, screenResources, screenResources->outputs [i]);

        // there are some situations where xrandr returns null (like screen not using the extension)
        if (info == nullptr)
            continue;

        auto cur = this->m_screens.begin ();
        auto end = this->m_screens.end ();

        for (; cur != end; cur ++)
        {
            if (info->connection == RR_Connected && strcmp (info->name, (*cur).c_str ()) == 0)
            {
                XRRCrtcInfo* crtc = XRRGetCrtcInfo (display, screenResources, info->crtc);

                std::cout << "Found requested screen: " << info->name << " -> " << crtc->x << "x" << crtc->y << ":" << crtc->width << "x" << crtc->height << std::endl;

                irr::core::rect<irr::s32> viewport;

                viewport.UpperLeftCorner.X = crtc->x;
                viewport.UpperLeftCorner.Y = crtc->y;
                viewport.LowerRightCorner.X = crtc->x + crtc->width;
                viewport.LowerRightCorner.Y = crtc->y + crtc->height;

                this->m_viewports.push_back (viewport);

                XRRFreeCrtcInfo (crtc);
            }
        }

        XRRFreeOutputInfo (info);
    }

    XRRFreeScreenResources (screenResources);

    irrlichtCreationParameters.WindowId = reinterpret_cast<void*> (DefaultRootWindow (display));
}

void CContext::renderFrame (Render::CScene* scene)
{
    this->m_time = this->getDevice ()->getTimer ()->getTime () / 1000.0f;
    this->m_pointerPosition.X = this->m_eventReceiver->getPosition ().X / (irr::f32) this->getDevice ()->getVideoDriver ()->getScreenSize ().Width;
    this->m_pointerPosition.Y = this->m_eventReceiver->getPosition ().Y / (irr::f32) this->getDevice ()->getVideoDriver ()->getScreenSize ().Height;

    if (this->m_viewports.empty () == true)
    {
        this->drawScene (scene, true);
    }
    else
    {
        auto cur = this->m_viewports.begin ();
        auto end = this->m_viewports.end ();

        for (; cur != end; cur ++)
        {
            // change viewport to render to the correct portion of the display
            this->getDevice ()->getVideoDriver ()->setViewPort (*cur);
            this->drawScene (scene, false);
        }
    }
}

void CContext::drawScene (Render::CScene* scene, bool backBuffer)
{
    this->getDevice ()->getVideoDriver ()->beginScene (false, true, scene->getScene ()->getClearColor ().toSColor());
    this->getDevice ()->getSceneManager ()->drawAll ();
    this->getDevice ()->getVideoDriver ()->endScene ();
}

void CContext::insertShaderVariable (Render::Shaders::Variables::CShaderVariable* variable)
{
    this->m_globalShaderVariables.push_back (variable);
}

const std::vector<Render::Shaders::Variables::CShaderVariable*>& CContext::getShaderVariables () const
{
    return this->m_globalShaderVariables;
}

irr::IrrlichtDevice* CContext::getDevice ()
{
    return this->m_device;
}

irr::io::path CContext::resolveFile (const irr::io::path& file)
{
    if (this->getDevice ()->getFileSystem ()->existFile (file) == false)
    {
        throw std::runtime_error ("Cannot find file " + std::string (file.c_str ()));
    }

    return file;
}

irr::io::path CContext::resolveMaterials (const std::string& materialName)
{
    return this->resolveFile (std::string ("materials/" + materialName + ".tex").c_str ());
}

irr::io::path CContext::resolveVertexShader (const std::string& vertexShader)
{
    return this->resolveFile (std::string ("shaders/" + vertexShader + ".vert").c_str ());
}
irr::io::path CContext::resolveFragmentShader (const std::string& fragmentShader)
{
    return this->resolveFile (std::string ("shaders/" + fragmentShader + ".frag").c_str ());
}

irr::io::path CContext::resolveIncludeShader (const std::string& includeShader)
{
    return this->resolveFile (std::string ("shaders/" + includeShader).c_str ());
}