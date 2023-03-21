#include "CWallpaperApplication.h"
#include "WallpaperEngine/Assets/CDirectory.h"
#include "WallpaperEngine/Assets/CVirtualContainer.h"
#include "WallpaperEngine/Audio/Drivers/CSDLAudioDriver.h"
#include "WallpaperEngine/Core/CVideo.h"
#include "WallpaperEngine/Logging/CLog.h"
#include "WallpaperEngine/Render/CRenderContext.h"
#include "WallpaperEngine/Render/Drivers/Output/CX11Output.h"
#include "WallpaperEngine/Render/Drivers/Output/CWindowOutput.h"
#include "Steam/FileSystem/FileSystem.h"

#include <unistd.h>


float g_Time;
float g_TimeLast;
bool g_KeepRunning = true;
bool g_AudioEnabled = true;
int g_AudioVolume = 128;

using namespace WallpaperEngine::Application;
using namespace WallpaperEngine::Core;

CWallpaperApplication::CWallpaperApplication (CApplicationContext& context) :
    m_context (context),
	m_defaultProject (nullptr)
{
    // copy state to global variables for now
    g_AudioVolume = context.audioVolume;
    g_AudioEnabled = context.audioEnabled;
    this->loadProjects ();
    this->setupProperties ();
}

void CWallpaperApplication::setupContainer (CCombinedContainer& container, const std::string& bg) const
{
	std::filesystem::path basepath = bg;

    container.add (new CDirectory (basepath));
	container.addPkg (basepath / "scene.pkg");
	container.addPkg (basepath / "gifscene.pkg");
	container.add (new CDirectory (this->m_context.assets));

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
		sLog.error ("WARNING: Shader patches directory cannot be found, this might make some backgrounds not work properly");

    // TODO: move this somewhere else?
    CVirtualContainer* virtualContainer = new CVirtualContainer ();

    //
    // Had to get a little creative with the effects to achieve the same bloom effect without any custom code
    // these virtual files are loaded by an image in the scene that takes current _rt_FullFrameBuffer and
    // applies the bloom effect to render it out to the screen
    //

    // add the effect file for screen bloom
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

    // add some model for the image element even if it's going to waste rendering cycles
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

void CWallpaperApplication::loadProjects ()
{
	for (const auto& it : this->m_context.screenSettings)
	{
		// ignore the screen settings if there was no background specified
		// the default will be used
		if (it.second.empty())
			continue;

		this->m_projects.insert_or_assign (
			it.first,
			this->loadProject (it.second)
		);
	}

	// load the default project if required
	if (!this->m_context.background.empty ())
		this->m_defaultProject = this->loadProject (this->m_context.background);
}

CProject* CWallpaperApplication::loadProject (const std::string& bg)
{
	CCombinedContainer* container = new CCombinedContainer ();

	this->setupContainer (*container, bg);

	// TODO: Change this to pointer instead of reference
    return CProject::fromFile ("project.json", container);
}

void CWallpaperApplication::setupPropertiesForProject (CProject* project)
{
	// show properties if required
	for (auto cur : project->getProperties ())
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

void CWallpaperApplication::setupProperties ()
{
	for (const auto& it : this->m_projects)
		this->setupPropertiesForProject (it.second);

	if (this->m_defaultProject != nullptr)
		this->setupPropertiesForProject (this->m_defaultProject);
}

void CWallpaperApplication::takeScreenshot (const Render::CRenderContext& context, const std::filesystem::path& filename, FREE_IMAGE_FORMAT format)
{
	// this should be getting called at the end of the frame, so the right thing should be bound already

	int width = context.getOutput ()->getFullWidth ();
	int height = context.getOutput ()->getFullHeight ();

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
    WallpaperEngine::Render::Drivers::COpenGLDriver videoDriver ("wallpaperengine");
    // initialize the input subsystem
    WallpaperEngine::Input::CInputContext inputContext (videoDriver);
	// output requested
	WallpaperEngine::Render::Drivers::Output::COutput* output;

	// initialize the requested output
	switch (this->m_context.windowMode)
	{
		case CApplicationContext::EXPLICIT_WINDOW:
		case CApplicationContext::NORMAL_WINDOW:
			output = new WallpaperEngine::Render::Drivers::Output::CWindowOutput (this->m_context, videoDriver);
			break;

		case CApplicationContext::X11_BACKGROUND:
			output = new WallpaperEngine::Render::Drivers::Output::CX11Output (this->m_context, videoDriver);
			break;
	}

    // initialize render context
    WallpaperEngine::Render::CRenderContext context (output, videoDriver, inputContext, *this);

	// set all the specific wallpapers required
	for (const auto& it : this->m_projects)
	{
		context.setWallpaper (
			it.first,
			WallpaperEngine::Render::CWallpaper::fromWallpaper (it.second->getWallpaper (), context, audioContext)
		);
	}

    // set the default rendering wallpaper if available
	if (this->m_defaultProject != nullptr)
		context.setDefaultWallpaper (
			WallpaperEngine::Render::CWallpaper::fromWallpaper (this->m_defaultProject->getWallpaper (), context, audioContext)
		);

    float startTime, endTime, minimumTime = 1.0f / this->m_context.maximumFPS;

    while (!videoDriver.closeRequested () && g_KeepRunning)
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

        if (this->m_context.takeScreenshot && videoDriver.getFrameCounter () == 5)
        {
            this->takeScreenshot (context, this->m_context.screenshot, this->m_context.screenshotFormat);
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

const std::map <std::string, CProject*>& CWallpaperApplication::getProjects () const
{
	return this->m_projects;
}

CProject* CWallpaperApplication::getDefaultProject () const
{
	return this->m_defaultProject;
}