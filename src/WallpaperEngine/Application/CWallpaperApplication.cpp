#include "CWallpaperApplication.h"
#include "WallpaperEngine/Assets/CDirectory.h"
#include "WallpaperEngine/Assets/CVirtualContainer.h"
#include "WallpaperEngine/Audio/Drivers/CSDLAudioDriver.h"
#include "WallpaperEngine/Core/CVideo.h"
#include "WallpaperEngine/Logging/CLog.h"
#include "WallpaperEngine/Render/CRenderContext.h"

#include <unistd.h>

float g_Time;
float g_TimeLast;
bool g_KeepRunning = true;
bool g_AudioEnabled = true;
int g_AudioVolume = 128;

using namespace WallpaperEngine::Application;
using namespace WallpaperEngine::Core;

CWallpaperApplication::CWallpaperApplication (CApplicationContext& context) :
    m_context (context)
{
    // copy state to global variables for now
    g_AudioVolume = context.audioVolume;
    g_AudioEnabled = context.audioEnabled;
    this->setupContainer ();
    this->loadProject ();
    this->setupProperties ();
}

void CWallpaperApplication::setupContainer ()
{
    this->m_vfs.add (new CDirectory (this->m_context.background));
    this->m_vfs.addPkg (std::filesystem::path (this->m_context.background) / "scene.pkg");
    this->m_vfs.addPkg (std::filesystem::path (this->m_context.background) / "gifscene.pkg");
    this->m_vfs.add (new CDirectory (this->m_context.assets));

    // TODO: move this somewhere else?
    CVirtualContainer* container = new CVirtualContainer ();

    //
    // Had to get a little creative with the effects to achieve the same bloom effect without any custom code
    // these virtual files are loaded by an image in the scene that takes current _rt_FullFrameBuffer and
    // applies the bloom effect to render it out to the screen
    //

    // add the effect file for screen bloom
    container->add (
        "effects/wpenginelinux/bloomeffect.json",
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
        "}"
    );

    // add some model for the image element even if it's going to waste rendering cycles
    container->add (
        "models/wpenginelinux.json",
        "{"
        "\t\"material\":\"materials/wpenginelinux.json\""
        "}"
    );

    // models require materials, so add that too
    container->add (
        "materials/wpenginelinux.json",
        "{"
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
        "}"
    );

    this->m_vfs.add (container);
}

void CWallpaperApplication::loadProject ()
{
    this->m_project = CProject::fromFile ("project.json", this->m_vfs);
    // go to the right folder so the videos will play
    // TODO: stop doing chdir and use full path
    if (this->m_project->getWallpaper ()->is <WallpaperEngine::Core::CVideo> () == true)
        chdir (this->m_context.background.c_str ());
}

void CWallpaperApplication::setupProperties ()
{
    // show properties if required
    for (auto cur : this->m_project->getProperties ())
    {
        // update the value of the property
        auto override = this->m_context.properties.find (cur->getName ());

        if (override != this->m_context.properties.end ())
        {
            sLog.out ("Applying override value for ", cur->getName ());

            cur->update (override->second);
        }

        if (this->m_context.onlyListProperties)
            sLog.out (cur->dump ());
    }
}

void CWallpaperApplication::takeScreenshot (WallpaperEngine::Render::CWallpaper* wp, const std::filesystem::path& filename, FREE_IMAGE_FORMAT format)
{
    GLint width, height;

    // bind texture and get the size
    glBindFramebuffer (GL_FRAMEBUFFER, wp->getWallpaperFramebuffer ());
    glBindTexture (GL_TEXTURE_2D, wp->getWallpaperTexture ());
    glGetTexLevelParameteriv (GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &width);
    glGetTexLevelParameteriv (GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &height);

    // make room for storing the pixel data
    uint8_t* buffer = new uint8_t [width * height * sizeof (uint8_t) * 3];
    uint8_t* pixel = buffer;

    // read the image into the buffer
    glReadPixels (0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, buffer);

    // build the output file with FreeImage
    FIBITMAP* bitmap = FreeImage_Allocate (width, height, 24);
    RGBQUAD color;

    // now get access to the pixels
    for (int y = height; y > 0; y --)
    {
        for (int x = 0; x < width; x ++)
        {
            color.rgbRed = *pixel ++;
            color.rgbGreen = *pixel ++;
            color.rgbBlue = *pixel ++;

            // set the pixel in the destination
            FreeImage_SetPixelColor (bitmap, x, y, &color);
        }
    }

    // finally save the file
    FreeImage_Save (format, bitmap, filename.c_str (), 0);

    // free all the used memory
    delete[] buffer;

    FreeImage_Unload (bitmap);

    // unbind the textures
    glBindTexture (GL_TEXTURE_2D, GL_NONE);
}

void CWallpaperApplication::show ()
{
    // initialize sdl audio driver
    WallpaperEngine::Audio::Drivers::CSDLAudioDriver audioDriver;
    // initialize audio context
    WallpaperEngine::Audio::CAudioContext audioContext (audioDriver);
    // initialize OpenGL driver
    WallpaperEngine::Render::Drivers::COpenGLDriver videoDriver (this->m_project->getTitle ().c_str ());
    // initialize the input subsystem
    WallpaperEngine::Input::CInputContext inputContext (videoDriver);
    // initialize render context
    WallpaperEngine::Render::CRenderContext context (this->m_context.screens, videoDriver, inputContext, this->m_vfs, *this);
    // ensure the context knows what wallpaper to render
    context.setWallpaper (
        WallpaperEngine::Render::CWallpaper::fromWallpaper (this->m_project->getWallpaper (), context, audioContext)
    );

    float startTime, endTime, minimumTime = 1.0f / this->m_context.maximumFPS;

    while (videoDriver.closeRequested () == false && g_KeepRunning == true)
    {
        // update input information
        inputContext.update ();
        // keep track of the previous frame's time
        g_TimeLast = g_Time;
        // calculate the current time value
        g_Time = videoDriver.getRenderTime ();
        // get the start time of the frame
        startTime = g_Time;
        // render the scene
        context.render ();
        // get the end time of the frame
        endTime = videoDriver.getRenderTime ();

        // ensure the frame time is correct to not overrun FPS
        if ((endTime - startTime) < minimumTime)
            usleep ((minimumTime - (endTime - startTime)) * CLOCKS_PER_SEC);

        if (this->m_context.takeScreenshot == true && videoDriver.getFrameCounter () == 5)
        {
            this->takeScreenshot (context.getWallpaper (), this->m_context.screenshot, this->m_context.screenshotFormat);
            // disable screenshot just in case the counter overflows
            this->m_context.takeScreenshot = false;
        }
    }

    // ensure this is updated as sometimes it might not come from a signal
    g_KeepRunning = false;

    sLog.out ("Stop requested");

    SDL_Quit ();
}

void CWallpaperApplication::signal (int signal)
{
    g_KeepRunning = false;
}