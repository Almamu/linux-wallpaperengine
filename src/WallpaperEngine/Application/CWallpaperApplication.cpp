#include "CWallpaperApplication.h"

#include "Steam/FileSystem/FileSystem.h"
#include "WallpaperEngine/Application/CApplicationState.h"
#include "WallpaperEngine/Assets/CAssetLoadException.h"
#include "WallpaperEngine/Assets/CDirectory.h"
#include "WallpaperEngine/Assets/CVirtualContainer.h"
#include "WallpaperEngine/Audio/Drivers/Detectors/CPulseAudioPlayingDetector.h"
#include "WallpaperEngine/Input/Drivers/CGLFWMouseInput.h"
#include "WallpaperEngine/Logging/CLog.h"
#include "WallpaperEngine/Render/CRenderContext.h"

#ifdef ENABLE_WAYLAND
#include "WallpaperEngine/Input/Drivers/CWaylandMouseInput.h"
#include "WallpaperEngine/Render/Drivers/CWaylandOpenGLDriver.h"
#endif /* ENABLE_WAYLAND */

#ifdef ENABLE_X11
#include "WallpaperEngine/Core/Wallpapers/CWeb.h"
#include "WallpaperEngine/Render/Drivers/Detectors/CX11FullScreenDetector.h"
#endif /* ENABLE_X11 */

#include <unistd.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#define FULLSCREEN_CHECK_WAIT_TIME 250

float g_Time;
float g_TimeLast;
float g_Daytime;

namespace WallpaperEngine::Application {
CWallpaperApplication::CWallpaperApplication (CApplicationContext& context) :
    m_context (context),
    m_audioContext (nullptr),
    m_audioDriver (nullptr),
    m_audioRecorder (nullptr),
    m_audioDetector (nullptr),
    m_inputContext (nullptr),
    m_renderContext (nullptr),
    m_videoDriver (nullptr),
    m_fullScreenDetector (nullptr),
    m_browserContext (nullptr) {
    this->loadBackgrounds ();
    this->setupProperties ();
    this->setupBrowser();
}

CWallpaperApplication::~CWallpaperApplication () {
    delete m_renderContext;
    delete m_videoDriver;
    delete m_audioContext;
    delete m_audioDriver;
    delete m_audioDetector;
    delete m_audioRecorder;
    delete m_inputContext;
    delete m_browserContext;
}

void CWallpaperApplication::setupContainer (CCombinedContainer& container, const std::string& bg) const {
    const std::filesystem::path basepath = bg;

    container.add (new CDirectory (basepath));
    container.addPkg (basepath / "scene.pkg");
    container.addPkg (basepath / "gifscene.pkg");

    try {
        container.add (new CDirectory (this->m_context.settings.general.assets));
    } catch (CAssetLoadException&) {
        sLog.exception ("Cannot find a valid assets folder, resolved to ", this->m_context.settings.general.assets);
    }

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
    if (this->m_context.settings.render.mode == CApplicationContext::NORMAL_WINDOW ||
        this->m_context.settings.render.mode == CApplicationContext::EXPLICIT_WINDOW) {
        this->m_backgrounds ["default"] = this->loadBackground (this->m_context.settings.general.defaultBackground);
        return;
    }

    for (const auto& [screen, path] : this->m_context.settings.general.screenBackgrounds) {
        // screens with no screen should use the default
        if (path.empty ()) {
            this->m_backgrounds [screen] = this->loadBackground (this->m_context.settings.general.defaultBackground);
        } else {
            this->m_backgrounds [screen] = this->loadBackground (path);
        }
    }
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
    for (const auto& [background, info] : this->m_backgrounds)
        this->setupPropertiesForProject (info);
}

void CWallpaperApplication::setupBrowser () {
    bool anyWebProject = std::any_of (this->m_backgrounds.begin (), this->m_backgrounds.end (), [](std::pair<const std::string, const Core::CProject*> pair) -> bool {
        return pair.second->getWallpaper()->is<Core::Wallpapers::CWeb> ();
    });

    // do not perform any initialization if no web background is present
    if (!anyWebProject) {
        return;
    }

    this->m_browserContext = new WebBrowser::CWebBrowserContext (*this);
}

void CWallpaperApplication::takeScreenshot (const std::filesystem::path& filename) const {
    // this should be getting called at the end of the frame, so the right thing should be bound already
    const int width = this->m_renderContext->getOutput ().getFullWidth ();
    const int height = this->m_renderContext->getOutput ().getFullHeight ();
    const bool vflip = this->m_renderContext->getOutput ().renderVFlip ();

    // build the output file with stbi_image_write
    auto* bitmap = new uint8_t [width * height * 3] {0};
    int xoffset = 0;

    for (const auto& [screen, viewport] : this->m_renderContext->getOutput ().getViewports ()) {
        // activate opengl context so we can read from the framebuffer
        viewport->makeCurrent ();
        // make room for storing the pixel of this viewport
        const auto bufferSize = (viewport->viewport.z - viewport->viewport.x) * (viewport->viewport.w - viewport->viewport.y) * 3;
        auto* buffer = new uint8_t [bufferSize];
        const uint8_t* pixel = buffer;

        // read the viewport data into the pixel buffer
        glPixelStorei (GL_PACK_ALIGNMENT, 1);
        // 4.5 supports glReadnPixels, anything older doesn't...
        if (GLEW_VERSION_4_5) {
            glReadnPixels (viewport->viewport.x, viewport->viewport.y, viewport->viewport.z, viewport->viewport.w,
                           GL_RGB, GL_UNSIGNED_BYTE, bufferSize, buffer);
        } else {
            // fallback to old version
            glReadPixels (viewport->viewport.x, viewport->viewport.y, viewport->viewport.z, viewport->viewport.w,
                          GL_RGB, GL_UNSIGNED_BYTE, buffer);
        }

        GLenum error = glGetError();

        if (error != GL_NO_ERROR) {
            sLog.error ("Cannot obtain pixel data for screen ", screen, ". OpenGL error: ", error);
            delete [] buffer;
            continue;
        }

        // now get access to the pixels
        for (int y = 0; y < viewport->viewport.w; y++) {
            for (int x = 0; x < viewport->viewport.z; x++) {
                int xfinal = x + xoffset;
                int yfinal = vflip ? (viewport->viewport.w - y - 1) : y;

                bitmap [yfinal * width * 3 + xfinal * 3] = *pixel++;
                bitmap [yfinal * width * 3 + xfinal * 3 + 1] = *pixel++;
                bitmap [yfinal * width * 3 + xfinal * 3 + 2] = *pixel++;
            }
        }

        if (viewport->single)
            xoffset += viewport->viewport.z;

        // free the buffer allocated for the viewport
        delete [] buffer;
    }

    auto extension = filename.extension ();

    if (extension == ".bmp") {
        stbi_write_bmp (filename.c_str (), width, height, 3, bitmap);
    } else if (extension == ".png") {
        stbi_write_png (filename.c_str (), width, height, 3, bitmap, width * 3);
    } else if (extension == ".jpg" || extension == ".jpeg") {
        stbi_write_jpg (filename.c_str (), width, height, 3, bitmap, 100);
    }

    delete [] bitmap;
}

void CWallpaperApplication::setupOutput () {
    const char* XDG_SESSION_TYPE = getenv ("XDG_SESSION_TYPE");

    if (!XDG_SESSION_TYPE) {
        sLog.exception (
            "Cannot read environment variable XDG_SESSION_TYPE, window server detection failed. Please ensure proper values are set");
    }

    sLog.debug ("Checking for window servers: ");

#ifdef ENABLE_WAYLAND
    sLog.debug ("\twayland");
#endif // ENABLE_WAYLAND

#ifdef ENABLE_X11
    sLog.debug ("\tx11");
#endif // ENABLE_X11

#ifdef ENABLE_WAYLAND
    bool isWayland = strncmp ("wayland", XDG_SESSION_TYPE, strlen ("wayland")) == 0;
#endif // ENABLE_WAYLAND
#ifdef ENABLE_X11
    bool isX11 = strncmp ("x11", XDG_SESSION_TYPE, strlen ("x11")) == 0;
#endif // ENABLE_X11

    if (this->m_context.settings.render.mode == CApplicationContext::DESKTOP_BACKGROUND) {
#ifdef ENABLE_WAYLAND
        if (isWayland) {
            m_videoDriver = new WallpaperEngine::Render::Drivers::CWaylandOpenGLDriver (this->m_context, *this);
            m_inputContext =
                new WallpaperEngine::Input::CInputContext (new WallpaperEngine::Input::Drivers::CWaylandMouseInput (
                    reinterpret_cast<WallpaperEngine::Render::Drivers::CWaylandOpenGLDriver*> (m_videoDriver)));
            this->m_fullScreenDetector =
                new WallpaperEngine::Render::Drivers::Detectors::CWaylandFullScreenDetector (this->m_context);
        }
#endif // ENABLE_WAYLAND
#ifdef ENABLE_X11
#ifdef ENABLE_WAYLAND
        else
#endif // ENABLE_WAYLAND
            if (isX11) {
                m_videoDriver =
                    new WallpaperEngine::Render::Drivers::CGLFWOpenGLDriver ("wallpaperengine", this->m_context, *this);
                m_inputContext =
                    new WallpaperEngine::Input::CInputContext (new WallpaperEngine::Input::Drivers::CGLFWMouseInput (
                        reinterpret_cast<Render::Drivers::CGLFWOpenGLDriver*> (m_videoDriver)));
                this->m_fullScreenDetector = new WallpaperEngine::Render::Drivers::Detectors::CX11FullScreenDetector (
                    this->m_context, *reinterpret_cast<Render::Drivers::CGLFWOpenGLDriver*> (m_videoDriver));
            }
#endif // ENABLE_X11
#ifdef ENABLE_X11
            else
#endif // ENABLE_X11
            {
                sLog.exception (
                    "Cannot run in background mode, window server could not be detected. XDG_SESSION_TYPE must be wayland or x11");
            }
    } else {
        m_videoDriver =
            new WallpaperEngine::Render::Drivers::CGLFWOpenGLDriver ("wallpaperengine", this->m_context, *this);

#ifdef ENABLE_WAYLAND
        if (isWayland) {
            this->m_fullScreenDetector =
                new WallpaperEngine::Render::Drivers::Detectors::CWaylandFullScreenDetector (this->m_context);
        }
#endif // ENABLE_WAYLAND
#ifdef ENABLE_X11
#ifdef ENABLE_WAYLAND
        else
#endif // ENABLE_WAYLAND
            if (isX11) {
                this->m_fullScreenDetector = new WallpaperEngine::Render::Drivers::Detectors::CX11FullScreenDetector (
                    this->m_context, *reinterpret_cast<Render::Drivers::CGLFWOpenGLDriver*> (m_videoDriver));
            }
#endif // ENABLE_X11
            else {
                this->m_fullScreenDetector =
                    new WallpaperEngine::Render::Drivers::Detectors::CFullScreenDetector (this->m_context);
            }

        m_inputContext =
            new WallpaperEngine::Input::CInputContext (new WallpaperEngine::Input::Drivers::CGLFWMouseInput (
                reinterpret_cast<Render::Drivers::CGLFWOpenGLDriver*> (m_videoDriver)));
    }
}

void CWallpaperApplication::setupAudio () {
    if (this->m_context.settings.audio.audioprocessing) {
        this->m_audioRecorder = new WallpaperEngine::Audio::Drivers::Recorders::CPulseAudioPlaybackRecorder ();
    } else {
        this->m_audioRecorder = new WallpaperEngine::Audio::Drivers::Recorders::CPlaybackRecorder ();
    }

    // audio playing detector
    m_audioDetector = new WallpaperEngine::Audio::Drivers::Detectors::CPulseAudioPlayingDetector (this->m_context, *this->m_fullScreenDetector);
    // initialize sdl audio driver
    m_audioDriver =
        new WallpaperEngine::Audio::Drivers::CSDLAudioDriver (this->m_context, *this->m_audioDetector, *this->m_audioRecorder);
    // initialize audio context
    m_audioContext = new WallpaperEngine::Audio::CAudioContext (*m_audioDriver);
}

void CWallpaperApplication::prepareOutputs () {
    // initialize render context
    m_renderContext = new WallpaperEngine::Render::CRenderContext (*m_videoDriver, *m_inputContext, *this);
    // create a new background for each screen

    // set all the specific wallpapers required
    for (const auto& [background, info] : this->m_backgrounds) {
        m_renderContext->setWallpaper (background,
                                       WallpaperEngine::Render::CWallpaper::fromWallpaper (
                                           info->getWallpaper (), *m_renderContext, *m_audioContext, *m_browserContext,
                                           this->m_context.settings.general.screenScalings [background]));
    }
}

void CWallpaperApplication::show () {
    this->setupOutput ();
    this->setupAudio ();
    this->prepareOutputs ();

    static time_t seconds;
    static struct tm* timeinfo;

    while (this->m_context.state.general.keepRunning) {
        // update g_Daytime
        time (&seconds);
        timeinfo = localtime (&seconds);
        g_Daytime = ((timeinfo->tm_hour * 60.0f) + timeinfo->tm_min) / (24.0f * 60.0f);

        // keep track of the previous frame's time
        g_TimeLast = g_Time;
        // calculate the current time value
        g_Time = m_videoDriver->getRenderTime ();
        // update audio recorder
        m_audioDriver->update ();
        // update input information
        m_inputContext->update ();
        // check for fullscreen windows and wait until there's none fullscreen
        if (this->m_fullScreenDetector->anythingFullscreen () && this->m_context.state.general.keepRunning) {
            m_renderContext->setPause (true);
            while (this->m_fullScreenDetector->anythingFullscreen () && this->m_context.state.general.keepRunning)
                usleep (FULLSCREEN_CHECK_WAIT_TIME);
            m_renderContext->setPause (false);
        }
        // process driver events
        m_videoDriver->dispatchEventQueue ();

        if (m_videoDriver->closeRequested()) {
            sLog.out ("Stop requested by driver");
            this->m_context.state.general.keepRunning = false;
        }

        if (!this->m_context.settings.screenshot.take || m_videoDriver->getFrameCounter () < this->m_context.settings.screenshot.delay)
            continue;

        this->takeScreenshot (this->m_context.settings.screenshot.path);
        this->m_context.settings.screenshot.take = false;
    }

    sLog.out ("Stopping");

    SDL_Quit ();
}

void CWallpaperApplication::update (Render::Drivers::Output::COutputViewport* viewport) {
    // render the scene
    m_renderContext->render (viewport);
}

void CWallpaperApplication::signal (int signal) {
    sLog.out ("Stop requested by signal ", signal);
    this->m_context.state.general.keepRunning = false;
}

const std::map<std::string, Core::CProject*>& CWallpaperApplication::getBackgrounds () const {
    return this->m_backgrounds;
}

CApplicationContext& CWallpaperApplication::getContext () const {
    return this->m_context;
}

const WallpaperEngine::Render::Drivers::Output::COutput& CWallpaperApplication::getOutput () const {
    return this->m_renderContext->getOutput ();
}
} // namespace WallpaperEngine::Application
