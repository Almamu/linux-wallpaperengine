#include "Project.h"
#include "Context.h"
#include "Data/Parsers/ProjectParser.h"

using namespace WallpaperEngine;

Context* active_context = nullptr;

void* wp_context_call_gl_proc_address (const char* name) {
	if (active_context == nullptr) {
		return nullptr;
	}

	if (active_context->gl_proc_address == nullptr) {
		return nullptr;
	}

	if (active_context->gl_proc_address->get_proc_address == nullptr) {
		return nullptr;
	}

	return active_context->gl_proc_address->get_proc_address (active_context->gl_proc_address->user_parameter, name);
}

/**
 * Builds an asset locator and sets up all the paths and virtual files that are needed for proper playback
 * TODO: MOVE THIS SOMEWHERE ELSE?
 *
 * @param config
 * @param project
 *
 * @return
 */
AssetLocatorUniquePtr wp_setup_asset_locator (const wp_configuration* config, const std::filesystem::path& project) {
	auto container = std::make_unique<Container> ();

	container->mount (project, "/");

	// exceptions from these mounts are expected and not a real problem, so ignore them for now
	try {
		container->mount (project / "scene.pkg", "/");
	} catch (std::runtime_error&) { }

	try {
		container->mount (project / "gifscene.pkg", "/");
	} catch (std::runtime_error&) { }

	try {
		container->mount (static_cast<const WallpaperEngine::Configuration*> (config)->assets_dir, "/");
	} catch (std::runtime_error&) { }

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
		{ { "name", "camerabloom_wpengine_linux" },
	      { "group", "wpengine_linux_camera" },
	      { "dependencies", JSON::array () },
	      {
			  "passes",
			  JSON::array (
				  { { { "material", "materials/util/downsample_quarter_bloom.json" },
	                  { "target", "_rt_4FrameBuffer" },
	                  { "bind", JSON::array ({ { { "name", "_rt_FullFrameBuffer" }, { "index", 0 } } }) } },
	                { { "material", "materials/util/downsample_eighth_blur_v.json" },
	                  { "target", "_rt_8FrameBuffer" },
	                  { "bind", JSON::array ({ { { "name", "_rt_4FrameBuffer" }, { "index", 0 } } }) } },
	                { { "material", "materials/util/blur_h_bloom.json" },
	                  { "target", "_rt_Bloom" },
	                  { "bind", JSON::array ({ { { "name", "_rt_8FrameBuffer" }, { "index", 0 } } }) } },
	                { { "material", "materials/util/combine.json" },
	                  { "target", "_rt_FullFrameBuffer" },
	                  { "bind",
	                    JSON::array (
							{ { { "name", "_rt_imageLayerComposite_-1_a" }, { "index", 0 } },
	                          { { "name", "_rt_Bloom" }, { "index", 1 } } }
						) } } }
			  ),
		  } }
	);

	vfs.add ("models/wpenginelinux.json", { { "material", "materials/wpenginelinux.json" } });

	vfs.add (
		"materials/wpenginelinux.json",
		{ { "passes",
	        JSON::array (
				{ { { "blending", "normal" },
	                { "cullmode", "nocull" },
	                { "depthtest", "disabled" },
	                { "depthwrite", "disabled" },
	                { "shader", "genericimage2" },
	                { "textures", JSON::array ({ "_rt_FullFrameBuffer" }) } } }
			) } }
	);

	vfs.add (
		"shaders/commands/copy.frag",
		"uniform sampler2D g_Texture0;\n"
		"in vec2 v_TexCoord;\n"
		"void main () {\n"
		"out_FragColor = texture (g_Texture0, v_TexCoord);\n"
		"}"
	);
	vfs.add (
		"shaders/commands/copy.vert",
		"in vec3 a_Position;\n"
		"in vec2 a_TexCoord;\n"
		"out vec2 v_TexCoord;\n"
		"void main () {\n"
		"gl_Position = vec4 (a_Position, 1.0);\n"
		"v_TexCoord = a_TexCoord;\n"
		"}"
	);

	return std::make_unique<AssetLocator> (std::move (container));
}

WallpaperEngine::Project::Project (
	Context* context, wp_mouse_input* mouse_input, const std::filesystem::path& project
) : context (*context) {
	if (!std::filesystem::exists (project) || !std::filesystem::is_directory (project)) {
		sLog.exception ("Cannot find project file");
	}

	if (active_context == nullptr) {
		active_context = context;
		// initialize glad with the proper loadgl function
		gladLoadGLLoader (wp_context_call_gl_proc_address);
	}

	if (active_context != nullptr && context != active_context) {
		sLog.exception ("Cannot load project with multiple contexts active at once");
	}

	auto contextPtr = static_cast<WallpaperEngine::Context*> (context);
	auto locator = wp_setup_asset_locator (contextPtr->config, project);
	auto json = JSON::parse (locator->readString ("project.json"));
	auto it = contextPtr->projects.insert (
		contextPtr->projects.end (), WallpaperEngine::Data::Parsers::ProjectParser::parse (json, std::move (locator))
	);

	this->ref = it;
	this->renderContext = std::make_unique<RenderContext> (*contextPtr, *(*it)->assetLocator);
	this->wallpaper
		= CWallpaper::fromWallpaper (*(*it)->wallpaper, *this->renderContext, *contextPtr->audio, mouse_input);
}

WallpaperEngine::Project::~Project () {
	this->context.projects.erase (this->ref);

	if (this->context.projects.empty ()) {
		active_context = nullptr;
	}
}

int WallpaperEngine::Project::getHeight () const { return this->wallpaper->getHeight (); }

int WallpaperEngine::Project::getWidth () const { return this->wallpaper->getWidth (); }

void WallpaperEngine::Project::setOutputFramebuffer (const unsigned int framebuffer) const {
	this->wallpaper->setDestinationFramebuffer (framebuffer);
}

void WallpaperEngine::Project::render () { this->wallpaper->render (); }

WallpaperEngine::Project*
WallpaperEngine::Project::loadId (Context* context, wp_mouse_input* mouse_input, const int id) {
	return new Project (context, mouse_input, context->config->backgrounds_dir / std::to_string (id));
}

WallpaperEngine::Project*
WallpaperEngine::Project::loadFolder (Context* context, wp_mouse_input* mouse_input, const char* folder) {
	return new Project (context, mouse_input, context->config->backgrounds_dir / folder);
}