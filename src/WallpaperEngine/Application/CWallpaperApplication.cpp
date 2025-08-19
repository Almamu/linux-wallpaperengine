#include "CWallpaperApplication.h"

#include "Steam/FileSystem/FileSystem.h"
#include "WallpaperEngine/Application/CApplicationState.h"
#include "WallpaperEngine/Assets/CAssetLoadException.h"
#include "WallpaperEngine/Assets/CDirectory.h"
#include "WallpaperEngine/Assets/CVirtualContainer.h"
#include "WallpaperEngine/Audio/Drivers/Detectors/CPulseAudioPlayingDetector.h"
#include "WallpaperEngine/Logging/CLog.h"
#include "WallpaperEngine/Render/CRenderContext.h"
#include "WallpaperEngine/Render/Drivers/CVideoFactories.h"

#include "WallpaperEngine/Core/Projects/CProperty.h"

#include "WallpaperEngine/Data/Parsers/ProjectParser.h"
#include "WallpaperEngine/Data/Dumpers/StringPrinter.h"

#include "WallpaperEngine/Data/Model/Wallpaper.h"

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

using namespace WallpaperEngine::Application;
using namespace WallpaperEngine::Data::Model;

CWallpaperApplication::CWallpaperApplication (CApplicationContext& context) :
    m_context (context) {
    this->loadBackgrounds ();
    this->setupProperties ();
    this->setupBrowser();
}

ContainerUniquePtr CWallpaperApplication::setupContainer (const std::string& bg) const {
    auto container = std::make_unique <CCombinedContainer> ();
    const std::filesystem::path basepath = bg;

    container->add (std::make_unique<CDirectory> (basepath));
    container->addPkg (basepath / "scene.pkg");
    container->addPkg (basepath / "gifscene.pkg");

    try {
        container->add (std::make_unique <CDirectory> (this->m_context.settings.general.assets));
    } catch (CAssetLoadException&) {
        sLog.exception ("Cannot find a valid assets folder, resolved to ", this->m_context.settings.general.assets);
    }

    // TODO: move this somewhere else?
    auto virtualContainer = std::make_unique <CVirtualContainer> ();

    //
    // Had to get a little creative with the effects to achieve the same bloom effect without any custom code
    // these virtual files are loaded by an image in the scene that takes current _rt_FullFrameBuffer and
    // applies the bloom effect to render it out to the screen
    //

    // add the effect file for screen bloom

    // add some model for the image element even if it's going to waste rendering cycles
    virtualContainer->add (
        "effects/wpenginelinux/bloomeffect.json",
        {
            {"name", "camerabloom_wpengine_linux"},
            {"group", "wpengine_linux_camera"},
            {"dependencies", json::array ()},
            {"passes",
                json::array (
                    {
                        {
                            {"material", "materials/util/downsample_quarter_bloom.json"},
                            {"target", "_rt_4FrameBuffer"},
                            {
                                "bind",
                                json::array (
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
                                json::array (
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
                                json::array (
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
                                json::array (
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

    virtualContainer->add (
        "models/wpenginelinux.json",
        {
            {"material","materials/wpenginelinux.json"}
        }
    );

    virtualContainer->add(
        "materials/wpenginelinux.json",
        {
            {"passes", json::array (
                {
                    {
                        {"blending", "normal"},
                        {"cullmode", "nocull"},
                        {"depthtest", "disabled"},
                        {"depthwrite", "disabled"},
                        {"shader", "genericimage2"},
                        {"textures", json::array ({"_rt_FullFrameBuffer"})}
                    }
                }
            )}}
    );

    virtualContainer->add(
        "shaders/commands/copy.frag",
        "uniform sampler2D g_Texture0;\n"
        "in vec2 v_TexCoord;\n"
        "void main () {\n"
        "out_FragColor = texture (g_Texture0, v_TexCoord);\n"
        "}"
    );
    virtualContainer->add(
        "shaders/commands/copy.vert",
        "in vec3 a_Position;\n"
        "in vec2 a_TexCoord;\n"
        "out vec2 v_TexCoord;\n"
        "void main () {\n"
        "gl_Position = vec4 (a_Position, 1.0);\n"
        "v_TexCoord = a_TexCoord;\n"
        "}"
    );

    container->add (std::move(virtualContainer));

    return container;
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

ProjectUniquePtr CWallpaperApplication::loadBackground (const std::string& bg) {
    auto container = this->setupContainer (bg);
    auto json = WallpaperEngine::Data::JSON::JSON::parse (container->readFileAsString ("project.json"));

    return WallpaperEngine::Data::Parsers::ProjectParser::parse (json, std::move(container));
}

void CWallpaperApplication::setupPropertiesForProject (const Project& project) {
    // show properties if required
    for (const auto& [key, cur] : project.properties) {
        // update the value of the property
        auto override = this->m_context.settings.general.properties.find (key);

        if (override != this->m_context.settings.general.properties.end ()) {
            sLog.out ("Applying override value for ", key);

            cur->set (override->second);
        }

        if (this->m_context.settings.general.onlyListProperties)
            sLog.out (cur->dump ());
    }
}

void CWallpaperApplication::setupProperties () {
    for (const auto& [background, info] : this->m_backgrounds)
        this->setupPropertiesForProject (*info);
}

void CWallpaperApplication::setupBrowser () {
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

    this->m_browserContext = std::make_unique <WebBrowser::CWebBrowserContext> (*this);
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

    for (const auto& windowServer : sVideoFactories.getRegisteredDrivers ()) {
        sLog.debug("\t", windowServer);
    }

    this->m_videoDriver = sVideoFactories.createVideoDriver (
        this->m_context.settings.render.mode, XDG_SESSION_TYPE, this->m_context, *this);
    this->m_fullScreenDetector = sVideoFactories.createFullscreenDetector (XDG_SESSION_TYPE, this->m_context, *this->m_videoDriver);
}

void CWallpaperApplication::setupAudio () {
    // ensure audioprocessing is required by any background, and we have it enabled
    bool audioProcessingRequired = std::any_of (
        this->m_backgrounds.begin (), this->m_backgrounds.end (),
        [](const std::pair<const std::string, ProjectUniquePtr>& pair) -> bool {
            return pair.second->supportsAudioProcessing;
        }
    );

    if (audioProcessingRequired && this->m_context.settings.audio.audioprocessing) {
        this->m_audioRecorder = std::make_unique <WallpaperEngine::Audio::Drivers::Recorders::CPulseAudioPlaybackRecorder> ();
    } else {
        this->m_audioRecorder = std::make_unique <WallpaperEngine::Audio::Drivers::Recorders::CPlaybackRecorder> ();
    }

    if (this->m_context.settings.audio.automute) {
        m_audioDetector = std::make_unique <WallpaperEngine::Audio::Drivers::Detectors::CPulseAudioPlayingDetector> (this->m_context, *this->m_fullScreenDetector);
    } else {
        m_audioDetector = std::make_unique <WallpaperEngine::Audio::Drivers::Detectors::CAudioPlayingDetector> (this->m_context, *this->m_fullScreenDetector);
    }

    // initialize sdl audio driver
    m_audioDriver = std::make_unique <WallpaperEngine::Audio::Drivers::CSDLAudioDriver> (this->m_context, *this->m_audioDetector, *this->m_audioRecorder);
    // initialize audio context
    m_audioContext = std::make_unique <WallpaperEngine::Audio::CAudioContext> (*m_audioDriver);
}

void CWallpaperApplication::prepareOutputs () {
    // initialize render context
    m_renderContext = std::make_unique <WallpaperEngine::Render::CRenderContext> (*m_videoDriver, *this);
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

void CWallpaperApplication::show () {
    this->setupOutput ();
    this->setupAudio ();
    this->prepareOutputs ();

    static time_t seconds;
    static struct tm* timeinfo;

    if (this->m_context.settings.general.dumpStructure) {
        // TODO: REWRITE TO USE THE NEW DUMPER
        /*
        auto prettyPrinter = PrettyPrinter::CPrettyPrinter ();

        for (const auto& [background, info] : this->m_renderContext->getWallpapers ()) {
            prettyPrinter.printWallpaper (*info);
        }

        std::cout << prettyPrinter.str () << std::endl;
        */
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
    int elapsed_frames = 0;
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
        elapsed_frames ++;

        // wait for a full render cycle before actually starting
        // this gives some extra time for video and web decoders to set themselves up
        // because of size changes
        if (elapsed_frames > this->m_context.settings.render.maximumFPS) {
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

void CWallpaperApplication::update (Render::Drivers::Output::COutputViewport* viewport) {
    // render the scene
    m_renderContext->render (viewport);
}

void CWallpaperApplication::signal (int signal) {
    sLog.out ("Stop requested by signal ", signal);
    this->m_context.state.general.keepRunning = false;
}

const std::map<std::string, ProjectUniquePtr>& CWallpaperApplication::getBackgrounds () const {
    return this->m_backgrounds;
}

CApplicationContext& CWallpaperApplication::getContext () const {
    return this->m_context;
}

const WallpaperEngine::Render::Drivers::Output::COutput& CWallpaperApplication::getOutput () const {
    return this->m_renderContext->getOutput ();
}
