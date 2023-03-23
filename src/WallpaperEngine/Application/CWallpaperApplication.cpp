#include "CWallpaperApplication.h"

#include "Steam/FileSystem/FileSystem.h"
#include "WallpaperEngine/Assets/CDirectory.h"
#include "WallpaperEngine/Assets/CVirtualContainer.h"
#include "WallpaperEngine/Audio/Drivers/CSDLAudioDriver.h"
#include "WallpaperEngine/Core/CVideo.h"
#include "WallpaperEngine/Logging/CLog.h"
#include "WallpaperEngine/Render/CRenderContext.h"
#include "WallpaperEngine/Render/Drivers/Output/CGLFWWindowOutput.h"
#include "WallpaperEngine/Render/Drivers/Output/CX11Output.h"
#include "WallpaperEngine/Application/CApplicationState.h"

#include <unistd.h>

float g_Time;
float g_TimeLast;

namespace WallpaperEngine::Application
{
    CWallpaperApplication::CWallpaperApplication (CApplicationContext& context) :
        m_context (context),
        m_defaultBackground (nullptr)
    {
        this->loadBackgrounds ();
        this->setupProperties ();
    }

    void CWallpaperApplication::setupContainer (CCombinedContainer& container, const std::string& bg) const
    {
        std::filesystem::path basepath = bg;

        container.add (new CDirectory (basepath));
        container.addPkg (basepath / "scene.pkg");
        container.addPkg (basepath / "gifscene.pkg");
        container.add (new CDirectory (this->m_context.settings.general.assets));

        // add two possible patches directories to the container
        // hopefully one sticks
        bool relative = true;
        bool absolute = true;

        try
        {
            container.add (new CDirectory ("../share/"));
        }
        catch (std::runtime_error& ex)
        {
            relative = false;
        }

        try
        {
            container.add (new CDirectory (DATADIR));
        }
        catch (std::runtime_error& ex)
        {
            absolute = false;
        }

        if (!relative && !absolute)
            sLog.error (
                "WARNING: Shader patches directory cannot be found, this might make some backgrounds not work "
                "properly"
            );

        // TODO: move this somewhere else?
        CVirtualContainer* virtualContainer = new CVirtualContainer ();

        //
        // Had to get a little creative with the effects to achieve the same bloom effect without any custom code
        // these virtual files are loaded by an image in the scene that takes current _rt_FullFrameBuffer and
        // applies the bloom effect to render it out to the screen
        //

        // add the effect file for screen bloom

        // add some model for the image element even if it's going to waste rendering cycles
        virtualContainer->add (
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

        virtualContainer->add (
            "models/wpenginelinux.json",
            "{"
            "\t\"material\":\"materials/wpenginelinux.json\""
            "}"
        );

        // models require materials, so add that too
        virtualContainer->add (
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

        container.add (virtualContainer);
    }

    void CWallpaperApplication::loadBackgrounds ()
    {
        for (const auto& it : this->m_context.settings.general.screenBackgrounds)
        {
            // ignore the screen settings if there was no background specified
            // the default will be used
            if (it.second.empty ())
                continue;

            this->m_backgrounds[it.first] = this->loadBackground (it.second);
        }

        // load the default project if required
        if (!this->m_context.settings.general.defaultBackground.empty ())
            this->m_defaultBackground = this->loadBackground (this->m_context.settings.general.defaultBackground);
    }

    Core::CProject* CWallpaperApplication::loadBackground (const std::string& bg)
    {
        CCombinedContainer* container = new CCombinedContainer ();

        this->setupContainer (*container, bg);

        return Core::CProject::fromFile ("project.json", container);
    }

    void CWallpaperApplication::setupPropertiesForProject (Core::CProject* project)
    {
        // show properties if required
        for (auto cur : project->getProperties ())
        {
            // update the value of the property
            auto override = this->m_context.settings.general.properties.find (cur->getName ());

            if (override != this->m_context.settings.general.properties.end ())
            {
                sLog.out ("Applying override value for ", cur->getName ());

                cur->update (override->second);
            }

            if (this->m_context.settings.general.onlyListProperties)
                sLog.out (cur->dump ());
        }
    }

    void CWallpaperApplication::setupProperties ()
    {
        for (const auto& it : this->m_backgrounds)
            this->setupPropertiesForProject (it.second);

        if (this->m_defaultBackground != nullptr)
            this->setupPropertiesForProject (this->m_defaultBackground);
    }

    void CWallpaperApplication::takeScreenshot (
        const Render::CRenderContext& context, const std::filesystem::path& filename, FREE_IMAGE_FORMAT format
    )
    {
        // this should be getting called at the end of the frame, so the right thing should be bound already

        int width = context.getOutput ()->getFullWidth ();
        int height = context.getOutput ()->getFullHeight ();

        // make room for storing the pixel data
        uint8_t* buffer = new uint8_t[width * height * sizeof (uint8_t) * 3];
        uint8_t* pixel = buffer;

        // read the image into the buffer
        glReadPixels (0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, buffer);

        // build the output file with FreeImage
        FIBITMAP* bitmap = FreeImage_Allocate (width, height, 24);
        RGBQUAD color;

        // now get access to the pixels
        for (int y = height; y > 0; y--)
        {
            for (int x = 0; x < width; x++)
            {
                color.rgbRed = *pixel++;
                color.rgbGreen = *pixel++;
                color.rgbBlue = *pixel++;

                // set the pixel in the destination
                FreeImage_SetPixelColor (bitmap, x, y, &color);
            }
        }

        // finally save the file
        FreeImage_Save (format, bitmap, filename.c_str (), 0);

        // free all the used memory
        delete[] buffer;

        FreeImage_Unload (bitmap);
    }

    void CWallpaperApplication::show ()
    {
        // initialize sdl audio driver
        WallpaperEngine::Audio::Drivers::CSDLAudioDriver audioDriver (this->m_context);
        // initialize audio context
        WallpaperEngine::Audio::CAudioContext audioContext (audioDriver);
        // initialize OpenGL driver
        WallpaperEngine::Render::Drivers::COpenGLDriver videoDriver ("wallpaperengine");
        // initialize the input subsystem
        WallpaperEngine::Input::CInputContext inputContext (videoDriver);
        // output requested
        WallpaperEngine::Render::Drivers::Output::COutput* output;

        // initialize the requested output
        switch (this->m_context.settings.render.mode)
        {
            case CApplicationContext::EXPLICIT_WINDOW:
            case CApplicationContext::NORMAL_WINDOW:
                output = new WallpaperEngine::Render::Drivers::Output::CGLFWWindowOutput (this->m_context, videoDriver);
                break;

            case CApplicationContext::X11_BACKGROUND:
                output = new WallpaperEngine::Render::Drivers::Output::CX11Output (this->m_context, videoDriver);
                break;
        }

        // initialize render context
        WallpaperEngine::Render::CRenderContext context (output, videoDriver, inputContext, *this);

        // set all the specific wallpapers required
        for (const auto& it : this->m_backgrounds)
            context.setWallpaper (
                it.first,
                WallpaperEngine::Render::CWallpaper::fromWallpaper (it.second->getWallpaper (), context, audioContext)
            );

        // set the default rendering wallpaper if available
        if (this->m_defaultBackground != nullptr)
            context.setDefaultWallpaper (WallpaperEngine::Render::CWallpaper::fromWallpaper (
                this->m_defaultBackground->getWallpaper (), context, audioContext
            ));

        float startTime, endTime, minimumTime = 1.0f / this->m_context.settings.render.maximumFPS;

        while (!videoDriver.closeRequested () && this->m_context.state.general.keepRunning)
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

            if (!this->m_context.settings.screenshot.take || videoDriver.getFrameCounter () != 5)
                continue;

            this->takeScreenshot (context, this->m_context.settings.screenshot.path, this->m_context.settings.screenshot.format);
            // disable screenshot just in case the counter overflows
            this->m_context.settings.screenshot.take = false;
        }

        // ensure this is updated as sometimes it might not come from a signal
        this->m_context.state.general.keepRunning = false;

        sLog.out ("Stop requested");

        SDL_Quit ();
    }

    void CWallpaperApplication::signal (int signal)
    {
        this->m_context.state.general.keepRunning = false;
    }

    const std::map<std::string, Core::CProject*>& CWallpaperApplication::getBackgrounds () const
    {
        return this->m_backgrounds;
    }

    Core::CProject* CWallpaperApplication::getDefaultBackground () const
    {
        return this->m_defaultBackground;
    }

    CApplicationContext& CWallpaperApplication::getContext () const
    {
        return this->m_context;
    }
}