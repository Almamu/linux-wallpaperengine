#include "CWallpaperApplication.h"

#include "Steam/FileSystem/FileSystem.h"
#include "WallpaperEngine/Application/CApplicationState.h"
#include "WallpaperEngine/Assets/CAssetLoadException.h"
#include "WallpaperEngine/Assets/CDirectory.h"
#include "WallpaperEngine/Assets/CVirtualContainer.h"
#include "WallpaperEngine/Audio/Drivers/Detectors/CPulseAudioPlayingDetector.h"
#include "WallpaperEngine/Core/CVideo.h"
#include "WallpaperEngine/Input/Drivers/CGLFWMouseInput.h"
#include "WallpaperEngine/Logging/CLog.h"
#include "WallpaperEngine/Render/CRenderContext.h"

#include "WallpaperEngine/Input/Drivers/CWaylandMouseInput.h"
#include "WallpaperEngine/Render/Drivers/CWaylandOpenGLDriver.h"

float g_Time;
float g_TimeLast;
float g_Daytime;

namespace WallpaperEngine::Application {
CWallpaperApplication::CWallpaperApplication (CApplicationContext& context) :
    m_context (context),
    m_defaultBackground (nullptr) {
    this->loadBackgrounds ();
    this->setupProperties ();
}

CWallpaperApplication::~CWallpaperApplication () {
    delete context;
    delete videoDriver;
    delete audioContext;
    delete audioDriver;
    delete inputContext;
}

void CWallpaperApplication::setupContainer (CCombinedContainer& container, const std::string& bg) const {
    const std::filesystem::path basepath = bg;

    container.add (new CDirectory (basepath));
    container.addPkg (basepath / "scene.pkg");
    container.addPkg (basepath / "gifscene.pkg");
    container.add (new CDirectory (this->m_context.settings.general.assets));

    // add two possible patches directories to the container
    // hopefully one sticks
    bool relative = true;
    bool absolute = true;

    try {
        container.add (new CDirectory ("../share/"));
    } catch (CAssetLoadException&) {
        relative = false;
    }

    try {
        container.add (new CDirectory (DATADIR));
    } catch (CAssetLoadException&) {
        absolute = false;
    }

    if (!relative && !absolute)
        sLog.error ("WARNING: Shader patches directory cannot be found, this might make some backgrounds not work "
                    "properly");

    // TODO: move this somewhere else?
    auto* virtualContainer = new CVirtualContainer ();

    //
    // Had to get a little creative with the effects to achieve the same bloom effect without any custom code
    // these virtual files are loaded by an image in the scene that takes current _rt_FullFrameBuffer and
    // applies the bloom effect to render it out to the screen
    //

    // add the effect file for screen bloom

    // add some model for the image element even if it's going to waste rendering cycles
    virtualContainer->add ("effects/wpenginelinux/bloomeffect.json",
                           "{"
                           "\t\"name\":\"camerabloom_wpengine_linux\","
                           "\t\"group\":\"wpengine_linux_camera\","
                           "\t\"dependencies\":[],"
                           "\t\"passes\":"
                           "\t["
                           "\t\t{"
                           "\t\t\t\"material\": \"materials/util/downsample_quarter_bloom.json\","
                           "\t\t\t\"target\": \"_rt_4FrameBuffer\","
                           "\t\t\t\"bind\":"
                           "\t\t\t["
                           "\t\t\t\t{"
                           "\t\t\t\t\t\"name\": \"_rt_FullFrameBuffer\","
                           "\t\t\t\t\t\"index\": 0"
                           "\t\t\t\t}"
                           "\t\t\t]"
                           "\t\t},"
                           "\t\t{"
                           "\t\t\t\"material\": \"materials/util/downsample_eighth_blur_v.json\","
                           "\t\t\t\"target\": \"_rt_8FrameBuffer\","
                           "\t\t\t\"bind\":"
                           "\t\t\t["
                           "\t\t\t\t{"
                           "\t\t\t\t\t\"name\": \"_rt_4FrameBuffer\","
                           "\t\t\t\t\t\"index\": 0"
                           "\t\t\t\t}"
                           "\t\t\t]"
                           "\t\t},"
                           "\t\t{"
                           "\t\t\t\"material\": \"materials/util/blur_h_bloom.json\","
                           "\t\t\t\"target\": \"_rt_Bloom\","
                           "\t\t\t\"bind\":"
                           "\t\t\t["
                           "\t\t\t\t{"
                           "\t\t\t\t\t\"name\": \"_rt_8FrameBuffer\","
                           "\t\t\t\t\t\"index\": 0"
                           "\t\t\t\t}"
                           "\t\t\t]"
                           "\t\t},"
                           "\t\t{"
                           "\t\t\t\"material\": \"materials/util/combine.json\","
                           "\t\t\t\"target\": \"_rt_FullFrameBuffer\","
                           "\t\t\t\"bind\":"
                           "\t\t\t["
                           "\t\t\t\t{"
                           "\t\t\t\t\t\"name\": \"_rt_imageLayerComposite_-1_a\","
                           "\t\t\t\t\t\"index\": 0"
                           "\t\t\t\t},"
                           "\t\t\t\t{"
                           "\t\t\t\t\t\"name\": \"_rt_Bloom\","
                           "\t\t\t\t\t\"index\": 1"
                           "\t\t\t\t}"
                           "\t\t\t]"
                           "\t\t}"
                           "\t]"
                           "}");

    virtualContainer->add ("models/wpenginelinux.json", "{"
                                                        "\t\"material\":\"materials/wpenginelinux.json\""
                                                        "}");

    // models require materials, so add that too
    virtualContainer->add ("materials/wpenginelinux.json", "{"
                                                           "\t\"passes\":"
                                                           "\t\t["
                                                           "\t\t\t{"
                                                           "\t\t\t\t\"blending\": \"normal\","
                                                           "\t\t\t\t\"cullmode\": \"nocull\","
                                                           "\t\t\t\t\"depthtest\": \"disabled\","
                                                           "\t\t\t\t\"depthwrite\": \"disabled\","
                                                           "\t\t\t\t\"shader\": \"genericimage2\","
                                                           "\t\t\t\t\"textures\": [\"_rt_FullFrameBuffer\"]"
                                                           "\t\t\t}"
                                                           "\t\t]"
                                                           "}");

    container.add (virtualContainer);
}

void CWallpaperApplication::loadBackgrounds () {
    for (const auto& [background, path] : this->m_context.settings.general.screenBackgrounds) {
        // ignore the screen settings if there was no background specified
        // the default will be used
        if (path.empty ())
            continue;

        this->m_backgrounds [background] = this->loadBackground (path);
    }

    // load the default project if required
    if (!this->m_context.settings.general.defaultBackground.empty ())
        this->m_defaultBackground = this->loadBackground (this->m_context.settings.general.defaultBackground);
}

Core::CProject* CWallpaperApplication::loadBackground (const std::string& bg) {
    auto* container = new CCombinedContainer ();

    this->setupContainer (*container, bg);

    return Core::CProject::fromFile ("project.json", container);
}

void CWallpaperApplication::setupPropertiesForProject (const Core::CProject* project) {
    // show properties if required
    for (const auto cur : project->getProperties ()) {
        // update the value of the property
        auto override = this->m_context.settings.general.properties.find (cur->getName ());

        if (override != this->m_context.settings.general.properties.end ()) {
            sLog.out ("Applying override value for ", cur->getName ());

            cur->update (override->second);
        }

        if (this->m_context.settings.general.onlyListProperties)
            sLog.out (cur->dump ());
    }
}

void CWallpaperApplication::setupProperties () {
    for (const auto& [backgrounc, info] : this->m_backgrounds)
        this->setupPropertiesForProject (info);

    if (this->m_defaultBackground != nullptr)
        this->setupPropertiesForProject (this->m_defaultBackground);
}

void CWallpaperApplication::takeScreenshot (const Render::CRenderContext& context,
                                            const std::filesystem::path& filename, FREE_IMAGE_FORMAT format) {
    // this should be getting called at the end of the frame, so the right thing should be bound already
    const int width = context.getOutput ().getFullWidth ();
    const int height = context.getOutput ().getFullHeight ();

    // build the output file with FreeImage
    static FIBITMAP* bitmap = FreeImage_Allocate (width, height, 24);
    RGBQUAD color;
    int xoffset = 0;

    for (const auto& [screen, viewport] : context.getOutput ().getViewports ()) {
        // activate opengl context so we can read from the framebuffer
        viewport->makeCurrent ();
        // make room for storing the pixel of this viewport
        auto* buffer = new uint8_t [viewport->viewport.z * viewport->viewport.w * sizeof (uint8_t) * 3];
        const uint8_t* pixel = buffer;

        // read the viewport data into the pixel buffer
        glReadPixels (viewport->viewport.x, viewport->viewport.y, viewport->viewport.z, viewport->viewport.w, GL_RGB,
                      GL_UNSIGNED_BYTE, buffer);

        // now get access to the pixels
        for (int y = viewport->viewport.w; y > 0; y--) {
            for (int x = 0; x < viewport->viewport.z; x++) {
                color.rgbRed = *pixel++;
                color.rgbGreen = *pixel++;
                color.rgbBlue = *pixel++;

                // set the pixel in the destination
                FreeImage_SetPixelColor (bitmap, x + xoffset,
                                         context.getOutput ().renderVFlip () ? (viewport->viewport.w - y) : y, &color);
            }
        }

        if (viewport->single)
            xoffset += viewport->viewport.z;

        // free the buffer allocated for the viewport
        delete [] buffer;
    }

    // finally save the file
    FreeImage_Save (format, bitmap, filename.c_str (), 0);

    FreeImage_Unload (bitmap);
}

void CWallpaperApplication::show () {
#ifdef ENABLE_WAYLAND
    const bool WAYLAND_DISPLAY = getenv ("WAYLAND_DISPLAY");

    // setup the right video driver based on the environment and the startup mode requested
    if (WAYLAND_DISPLAY && this->m_context.settings.render.mode == CApplicationContext::DESKTOP_BACKGROUND) {
        const auto waylandDriver = new WallpaperEngine::Render::Drivers::CWaylandOpenGLDriver (this->m_context, *this);
        inputContext = new WallpaperEngine::Input::CInputContext (
            new WallpaperEngine::Input::Drivers::CWaylandMouseInput (waylandDriver));

        videoDriver = waylandDriver;
    } else
#endif
    {
        const auto x11Driver =
            new WallpaperEngine::Render::Drivers::CX11OpenGLDriver ("wallpaperengine", this->m_context, *this);
        // no wayland detected, try the old X11 method
        inputContext = new WallpaperEngine::Input::CInputContext (
            new WallpaperEngine::Input::Drivers::CGLFWMouseInput (x11Driver));

        videoDriver = x11Driver;
    }

    // stereo mix recorder for audio processing
    WallpaperEngine::Audio::Drivers::Recorders::CPulseAudioPlaybackRecorder audioRecorder;
    // audio playing detector
    WallpaperEngine::Audio::Drivers::Detectors::CPulseAudioPlayingDetector audioDetector (
        this->m_context, videoDriver->getFullscreenDetector ());
    // initialize sdl audio driver
    audioDriver = new WallpaperEngine::Audio::Drivers::CSDLAudioDriver (this->m_context, audioDetector, audioRecorder);
    // initialize audio context
    audioContext = new WallpaperEngine::Audio::CAudioContext (*audioDriver);
    // initialize render context
    context = new WallpaperEngine::Render::CRenderContext (*videoDriver, *inputContext, *this);

    // set all the specific wallpapers required
    for (const auto& [background, info] : this->m_backgrounds)
        context->setWallpaper (background, WallpaperEngine::Render::CWallpaper::fromWallpaper (
                                               info->getWallpaper (), *context, *audioContext,
                                               this->m_context.settings.general.screenScalings [background]));

    // set the default rendering wallpaper if available
    if (this->m_defaultBackground != nullptr)
        context->setDefaultWallpaper (WallpaperEngine::Render::CWallpaper::fromWallpaper (
            this->m_defaultBackground->getWallpaper (), *context, *audioContext,
            this->m_context.settings.render.window.scalingMode));

    static time_t seconds;
    static struct tm* timeinfo;

    while (this->m_context.state.general.keepRunning && !videoDriver->closeRequested ()) {
        // update g_Daytime
        time (&seconds);
        timeinfo = localtime (&seconds);
        g_Daytime = ((timeinfo->tm_hour * 60) + timeinfo->tm_min) / (24.0 * 60.0);

        // keep track of the previous frame's time
        g_TimeLast = g_Time;
        // calculate the current time value
        g_Time = videoDriver->getRenderTime ();
        // update audio recorder
        audioDriver->update ();
        // update input information
        inputContext->update ();
        // process driver events
        videoDriver->dispatchEventQueue ();

        if (!this->m_context.settings.screenshot.take || videoDriver->getFrameCounter () < 5)
            continue;

        this->takeScreenshot (*context, this->m_context.settings.screenshot.path,
                              this->m_context.settings.screenshot.format);
        this->m_context.settings.screenshot.take = false;
    }

    // ensure this is updated as sometimes it might not come from a signal
    this->m_context.state.general.keepRunning = false;

    sLog.out ("Stop requested");

    SDL_Quit ();
}

void CWallpaperApplication::update (Render::Drivers::Output::COutputViewport* viewport) {
    // render the scene
    context->render (viewport);
}

void CWallpaperApplication::signal (int signal) {
    this->m_context.state.general.keepRunning = false;
}

const std::map<std::string, Core::CProject*>& CWallpaperApplication::getBackgrounds () const {
    return this->m_backgrounds;
}

Core::CProject* CWallpaperApplication::getDefaultBackground () const {
    return this->m_defaultBackground;
}

CApplicationContext& CWallpaperApplication::getContext () const {
    return this->m_context;
}

const WallpaperEngine::Render::Drivers::Output::COutput& CWallpaperApplication::getOutput () const {
    return this->context->getOutput ();
}
} // namespace WallpaperEngine::Application