#include <filesystem>

#include "frontends/project.h"

#include "WallpaperEngine/Configuration.h"
#include "WallpaperEngine/Context.h"
#include "WallpaperEngine/Data/Parsers/ProjectParser.h"
#include "WallpaperEngine/Render/CWallpaper.h"

/**
 * The current active context in use
 *
 * There can be only one active at a time
 */
wp_context* wp_active_context = nullptr;

void* wp_context_call_gl_proc_address (const char* name) {
	if (wp_active_context == nullptr) {
		return nullptr;
	}

	const auto contextPtr = static_cast<WallpaperEngine::Context*> (wp_active_context);

	if (contextPtr->gl_proc_address == nullptr) {
		return nullptr;
	}

	if (contextPtr->gl_proc_address->get_proc_address == nullptr) {
		return nullptr;
	}

	return contextPtr->gl_proc_address->get_proc_address (contextPtr->gl_proc_address->user_parameter, name);
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

wp_project* wp_project_load (wp_context* context, wp_mouse_input* mouse_input, const std::filesystem::path& project) {
	try {
		if (!std::filesystem::exists (project) || !std::filesystem::is_directory (project)) {
			sLog.error ("Cannot find project file ", project);
			return nullptr;
		}

		if (wp_active_context == nullptr) {
			wp_active_context = context;
			// initialize glad with the proper loadgl function
			gladLoadGLLoader (wp_context_call_gl_proc_address);
		}

		if (wp_active_context != nullptr && context != wp_active_context) {
			sLog.exception ("Cannot load project with multiple contexts active at once");
		}

		auto contextPtr = static_cast<WallpaperEngine::Context*> (context);
		auto locator = wp_setup_asset_locator (contextPtr->config, project);
		auto json = JSON::parse (locator->readString ("project.json"));
		auto it = contextPtr->projects.insert (
			contextPtr->projects.end (),
			WallpaperEngine::Data::Parsers::ProjectParser::parse (json, std::move (locator))
		);
		auto result = new WallpaperEngine::LoadedProject {
			.ref = it,
			.render = std::make_unique<RenderContext> (*contextPtr, *(*it)->assetLocator),
			.context = *contextPtr,
		};

		result->wallpaper
			= CWallpaper::fromWallpaper (*(*it)->wallpaper, *result->render, *contextPtr->audio, mouse_input);

		return result;
	} catch (std::bad_alloc& error) {
		sLog.error ("Exception when loading background: ", error.what ());
	} catch (std::runtime_error& error) {
		sLog.error ("Exception when loading background: ", error.what ());
	} catch (std::exception& error) {
		sLog.error ("Exception when loading background: ", error.what ());
	} catch (...) {
		sLog.error ("Unknown exception when loading background");
	}

	return nullptr;
}

wp_project* wp_project_load_id (wp_context* context, wp_mouse_input* mouse_input, const int id) {
	return wp_project_load (
		context, mouse_input,
		static_cast<const WallpaperEngine::Context*> (context)->config->backgrounds_dir / std::to_string (id)
	);
}

wp_project* wp_project_load_folder (wp_context* context, wp_mouse_input* mouse_input, const char* folder) {
	return wp_project_load (context, mouse_input, std::filesystem::path (folder));
}

void wp_project_destroy (wp_project* project) {
	const auto projectPtr = static_cast<WallpaperEngine::LoadedProject*> (project);

	// remove the project off the list
	projectPtr->context.projects.erase (projectPtr->ref);

	if (projectPtr->context.projects.empty ()) {
		// context is not bound to anything anymore
		// erase active reference
		wp_active_context = nullptr;
	}

	// finally free LoadedProject info
	delete projectPtr;
}

int wp_project_get_width (wp_project* project) {
	return static_cast<WallpaperEngine::LoadedProject*> (project)->wallpaper->getWidth ();
}

int wp_project_get_height (wp_project* project) {
	return static_cast<WallpaperEngine::LoadedProject*> (project)->wallpaper->getHeight ();
}

void wp_project_set_output_framebuffer (wp_project* project, unsigned int framebuffer) {
	static_cast<WallpaperEngine::LoadedProject*> (project)->wallpaper->setDestinationFramebuffer (framebuffer);
}