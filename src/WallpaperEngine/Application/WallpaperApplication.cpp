#include "WallpaperApplication.h"

#include "Steam/FileSystem/FileSystem.h"
#include "WallpaperEngine/Application/ApplicationState.h"
#include "WallpaperEngine/Assets/AssetLoadException.h"
#include "WallpaperEngine/Audio/Drivers/Detectors/PulseAudioPlayingDetector.h"
#include "WallpaperEngine/FileSystem/Container.h"
#include "WallpaperEngine/Logging/Log.h"
#include "WallpaperEngine/Render/Drivers/VideoFactories.h"
#include "WallpaperEngine/Render/RenderContext.h"

#include "WallpaperEngine/Data/Dumpers/StringPrinter.h"
#include "WallpaperEngine/Data/Parsers/ProjectParser.h"

#include "WallpaperEngine/Data/Model/Wallpaper.h"
#include "WallpaperEngine/Data/Model/Property.h"

#if DEMOMODE
#include "recording.h"
#endif /* DEMOMODE */

#include <unistd.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#define FULLSCREEN_CHECK_WAIT_TIME 250

float g_Time;
float g_TimeLast;
float g_Daytime;

using namespace WallpaperEngine::Assets;
using namespace WallpaperEngine::Application;
using namespace WallpaperEngine::Data::Model;
using namespace WallpaperEngine::FileSystem;

WallpaperApplication::WallpaperApplication (ApplicationContext& context) :
    m_context (context) {
    this->loadBackgrounds ();
    this->setupProperties ();
    this->setupBrowser();
}

AssetLocatorUniquePtr WallpaperApplication::setupAssetLocator (const std::string& bg) const {
    auto container = std::make_unique <Container> ();

    const std::filesystem::path path = bg;

    container->mount (path, "/");
    try {
        container->mount (path / "scene.pkg", "/");
    } catch (std::runtime_error&) { }

    try {
        container->mount (path / "gifscene.pkg", "/");
    } catch (std::runtime_error&) { }

    try {
        container->mount (this->m_context.settings.general.assets, "/");
    } catch (std::runtime_error&) {
        sLog.exception ("Cannot find a valid assets folder, resolved to ", this->m_context.settings.general.assets);
    }

    auto& vfs = container->getVFS ();

    //
    // Had to get a little creative with the effects to achieve the same bloom effect without any custom code
    // these virtual files are loaded by an image in the scene that takes current _rt_FullFrameBuffer and
    // applies the bloom effect to render it out to the screen
    //

    // add the effect file for screen bloom

    // add some model for the image element even if it's going to waste rendering cycles
    vfs.add (
        "effects/wpenginelinux/bloomeffect.json",
        {
            {"name", "camerabloom_wpengine_linux"},
            {"group", "wpengine_linux_camera"},
            {"dependencies", JSON::array ()},
            {"passes",
                JSON::array (
                    {
                        {
                            {"material", "materials/util/downsample_quarter_bloom.json"},
                            {"target", "_rt_4FrameBuffer"},
                            {
                                "bind",
                                JSON::array (
                                    {
                                        {
                                            {"name", "_rt_FullFrameBuffer"},
                                            {"index", 0}
                                        }
                                    }
                                )
                            }
                        },
                        {
                            {"material", "materials/util/downsample_eighth_blur_v.json"},
                            {"target", "_rt_8FrameBuffer"},
                            {
                                "bind",
                                JSON::array (
                                    {
                                        {
                                            {"name", "_rt_4FrameBuffer"},
                                            {"index", 0}
                                        }
                                    }
                                )
                            }
                        },
                        {
                            {"material", "materials/util/blur_h_bloom.json"},
                            {"target", "_rt_Bloom"},
                            {
                                "bind",
                                JSON::array (
                                    {
                                        {
                                            {"name", "_rt_8FrameBuffer"},
                                            {"index", 0}
                                        }
                                    }
                                )
                            }
                        },
                        {
                            {"material", "materials/util/combine.json"},
                            {"target", "_rt_FullFrameBuffer"},
                            {
                                "bind",
                                JSON::array (
                                    {
                                        {
                                            {"name", "_rt_imageLayerComposite_-1_a"},
                                            {"index", 0}
                                        },
                                        {
                                            {"name", "_rt_Bloom"},
                                            {"index", 1}
                                        }
                                    }
                                )
                            }
                        }
                    }
               ),
            }
        }
    );

    vfs.add (
        "models/wpenginelinux.json",
        {
            {"material","materials/wpenginelinux.json"}
        }
    );

    vfs.add(
        "materials/wpenginelinux.json",
        {
            {"passes", JSON::array (
                {
                    {
                        {"blending", "normal"},
                        {"cullmode", "nocull"},
                        {"depthtest", "disabled"},
                        {"depthwrite", "disabled"},
                        {"shader", "genericimage2"},
                        {"textures", JSON::array ({"_rt_FullFrameBuffer"})}
                    }
                }
            )}}
    );

    vfs.add(
        "shaders/commands/copy.frag",
        "uniform sampler2D g_Texture0;\n"
        "in vec2 v_TexCoord;\n"
        "void main () {\n"
        "out_FragColor = texture (g_Texture0, v_TexCoord);\n"
        "}"
    );
    vfs.add(
        "shaders/commands/copy.vert",
        "in vec3 a_Position;\n"
        "in vec2 a_TexCoord;\n"
        "out vec2 v_TexCoord;\n"
        "void main () {\n"
        "gl_Position = vec4 (a_Position, 1.0);\n"
        "v_TexCoord = a_TexCoord;\n"
        "}"
    );

    return std::make_unique <AssetLocator> (std::move (container));
}

void WallpaperApplication::loadBackgrounds () {
    if (this->m_context.settings.render.mode == ApplicationContext::NORMAL_WINDOW ||
        this->m_context.settings.render.mode == ApplicationContext::EXPLICIT_WINDOW) {
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

ProjectUniquePtr WallpaperApplication::loadBackground (const std::string& bg) {
    auto container = this->setupAssetLocator (bg);
    auto json = WallpaperEngine::Data::JSON::JSON::parse (container->readString ("project.json"));

    return WallpaperEngine::Data::Parsers::ProjectParser::parse (json, std::move(container));
}

void WallpaperApplication::setupPropertiesForProject (const Project& project) {
    // show properties if required
    for (const auto& [key, cur] : project.properties) {
        // update the value of the property
        auto override = this->m_context.settings.general.properties.find (key);

        if (override != this->m_context.settings.general.properties.end ()) {
            sLog.out ("Applying override value for ", key);

            cur->update (override->second);
        }

        if (this->m_context.settings.general.onlyListProperties) {
            sLog.out (cur->dump ());
        }
    }
}

void WallpaperApplication::setupProperties () {
    for (const auto& [background, info] : this->m_backgrounds)
        this->setupPropertiesForProject (*info);
}

void WallpaperApplication::setupBrowser () {
    bool anyWebProject = std::any_of (
        this->m_backgrounds.begin (), this->m_backgrounds.end (),
        [](const std::pair<const std::string, ProjectUniquePtr>& pair) -> bool {
            return pair.second->wallpaper->is<Web> ();
        }
    );

    // do not perform any initialization if no web background is present
    if (!anyWebProject) {
        return;
    }

    this->m_browserContext = std::make_unique <WebBrowser::WebBrowserContext> (*this);
}

void WallpaperApplication::takeScreenshot (const std::filesystem::path& filename) const {
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

        if (const GLenum error = glGetError (); error != GL_NO_ERROR) {
            sLog.error ("Cannot obtain pixel data for screen ", screen, ". OpenGL error: ", error);
            delete [] buffer;
            continue;
        }

        // now get access to the pixels
        for (int y = 0; y < viewport->viewport.w; y++) {
            for (int x = 0; x < viewport->viewport.z; x++) {
                const int xfinal = x + xoffset;
                const int yfinal = vflip ? (viewport->viewport.w - y - 1) : y;

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

    if (const auto extension = filename.extension (); extension == ".bmp") {
        stbi_write_bmp (filename.c_str (), width, height, 3, bitmap);
    } else if (extension == ".png") {
        stbi_write_png (filename.c_str (), width, height, 3, bitmap, width * 3);
    } else if (extension == ".jpg" || extension == ".jpeg") {
        stbi_write_jpg (filename.c_str (), width, height, 3, bitmap, 100);
    }

    delete [] bitmap;
}

void WallpaperApplication::setupOutput () {
    const char* XDG_SESSION_TYPE = getenv ("XDG_SESSION_TYPE");

    if (!XDG_SESSION_TYPE) {
        sLog.exception (
            "Cannot read environment variable XDG_SESSION_TYPE, window server detection failed. Please ensure proper values are set");
    }

    sLog.debug ("Checking for window servers: ");

    for (const auto& windowServer : sVideoFactories.getRegisteredDrivers ()) {
        sLog.debug("\t", windowServer);
    }

    this->m_videoDriver = sVideoFactories.createVideoDriver (
        this->m_context.settings.render.mode, XDG_SESSION_TYPE, this->m_context, *this);
    this->m_fullScreenDetector = sVideoFactories.createFullscreenDetector (XDG_SESSION_TYPE, this->m_context, *this->m_videoDriver);
}

void WallpaperApplication::setupAudio () {
    // ensure audioprocessing is required by any background, and we have it enabled
    const bool audioProcessingRequired = std::ranges::any_of (
        this->m_backgrounds,
        [](const std::pair<const std::string, ProjectUniquePtr>& pair) -> bool {
            return pair.second->supportsAudioProcessing;
        }
    );

    if (audioProcessingRequired && this->m_context.settings.audio.audioprocessing) {
        this->m_audioRecorder = std::make_unique <WallpaperEngine::Audio::Drivers::Recorders::PulseAudioPlaybackRecorder> ();
    } else {
        this->m_audioRecorder = std::make_unique <WallpaperEngine::Audio::Drivers::Recorders::PlaybackRecorder> ();
    }

    if (this->m_context.settings.audio.automute) {
        m_audioDetector = std::make_unique <WallpaperEngine::Audio::Drivers::Detectors::PulseAudioPlayingDetector> (this->m_context, *this->m_fullScreenDetector);
    } else {
        m_audioDetector = std::make_unique <WallpaperEngine::Audio::Drivers::Detectors::AudioPlayingDetector> (this->m_context, *this->m_fullScreenDetector);
    }

    // initialize sdl audio driver
    m_audioDriver = std::make_unique <WallpaperEngine::Audio::Drivers::SDLAudioDriver> (this->m_context, *this->m_audioDetector, *this->m_audioRecorder);
    // initialize audio context
    m_audioContext = std::make_unique <WallpaperEngine::Audio::AudioContext> (*m_audioDriver);
}

void WallpaperApplication::prepareOutputs () {
    // initialize render context
    m_renderContext = std::make_unique <WallpaperEngine::Render::RenderContext> (*m_videoDriver, *this);
    // create a new background for each screen

    // set all the specific wallpapers required
    for (const auto& [background, info] : this->m_backgrounds) {
        m_renderContext->setWallpaper (
            background,
            WallpaperEngine::Render::CWallpaper::fromWallpaper (
                *info->wallpaper, *m_renderContext, *m_audioContext, m_browserContext.get (),
                this->m_context.settings.general.screenScalings [background],
                this->m_context.settings.general.screenClamps [background]
            )
        );
    }
}

void WallpaperApplication::show () {
    this->setupOutput ();
    this->setupAudio ();
    this->prepareOutputs ();

    static time_t seconds;
    static struct tm* timeinfo;

    if (this->m_context.settings.general.dumpStructure) {
        auto prettyPrinter = Data::Dumpers::StringPrinter ();

        for (const auto& [background, info] : this->m_renderContext->getWallpapers ()) {
            prettyPrinter.printWallpaper (info->getWallpaperData ());
        }

        std::cout << prettyPrinter.str () << std::endl;
    }

#if DEMOMODE
    // ensure only one background is running so everything can be properly caught
    if (this->m_renderContext->getWallpapers ().size () > 1) {
        sLog.exception ("Demo mode only supports one background");
    }

    int width = this->m_renderContext->getWallpapers ().begin ()->second->getWidth ();
    int height = this->m_renderContext->getWallpapers ().begin ()->second->getHeight ();
    std::vector<uint8_t> pixels(width * height * 3);
    bool initialized = false;
    int frame = 0;
#endif /* DEMOMODE */

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
        m_videoDriver->getInputContext ().update ();
        // process driver events
        m_videoDriver->dispatchEventQueue ();

        if (m_videoDriver->closeRequested()) {
            sLog.out ("Stop requested by driver");
            this->m_context.state.general.keepRunning = false;
        }

#if DEMOMODE
        // wait for a full render cycle before actually starting
        // this gives some extra time for video and web decoders to set themselves up
        // because of size changes
        if (m_videoDriver->getFrameCounter () > (uint32_t) this->m_context.settings.render.maximumFPS) {
            if (!initialized) {
                width = this->m_renderContext->getWallpapers ().begin ()->second->getWidth ();
                height = this->m_renderContext->getWallpapers ().begin ()->second->getHeight ();
                pixels.reserve(width * height * 3);
                init_encoder ("output.webm", width, height);
                initialized = true;
            }

            glBindFramebuffer (GL_FRAMEBUFFER, this->m_renderContext->getWallpapers ().begin ()->second->getWallpaperFramebuffer());

            glPixelStorei (GL_PACK_ALIGNMENT, 1);
            glReadPixels (0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels.data ());
            write_video_frame (pixels.data ());
            frame ++;

            // stop after the given framecount
            if (frame >= FRAME_COUNT) {
                this->m_context.state.general.keepRunning = false;
            }
        }
#endif /* DEMOMODE */
        // check for fullscreen windows and wait until there's none fullscreen
        if (this->m_fullScreenDetector->anythingFullscreen () && this->m_context.state.general.keepRunning) {
            m_renderContext->setPause (true);
            while (this->m_fullScreenDetector->anythingFullscreen () && this->m_context.state.general.keepRunning)
                usleep (FULLSCREEN_CHECK_WAIT_TIME);
            m_renderContext->setPause (false);
        }

        if (!this->m_context.settings.screenshot.take || m_videoDriver->getFrameCounter () < this->m_context.settings.screenshot.delay)
            continue;

        this->takeScreenshot (this->m_context.settings.screenshot.path);
        this->m_context.settings.screenshot.take = false;
    }

    sLog.out ("Stopping");

#if DEMOMODE
    close_encoder ();
#endif /* DEMOMODE */

    SDL_Quit ();
}

void WallpaperApplication::update (Render::Drivers::Output::OutputViewport* viewport) {
    // render the scene
    m_renderContext->render (viewport);
}

void WallpaperApplication::signal (int signal) {
    sLog.out ("Stop requested by signal ", signal);
    this->m_context.state.general.keepRunning = false;
}

const std::map<std::string, ProjectUniquePtr>& WallpaperApplication::getBackgrounds () const {
    return this->m_backgrounds;
}

ApplicationContext& WallpaperApplication::getContext () const {
    return this->m_context;
}

const WallpaperEngine::Render::Drivers::Output::Output& WallpaperApplication::getOutput () const {
    return this->m_renderContext->getOutput ();
}
