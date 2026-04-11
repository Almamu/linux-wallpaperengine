#include "Project.h"
#include "Context.h"
#include "Data/Dumpers/StringPrinter.h"
#include "Data/Model/Property.h"
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

	auto locator = wp_setup_asset_locator (context->config, project);
	auto json = JSON::parse (locator->readString ("project.json"));
	this->ref = context->projects.insert (
		context->projects.end (), WallpaperEngine::Data::Parsers::ProjectParser::parse (json, std::move (locator))
	);

	this->current_property = nullptr;
	this->property_it = (*this->ref)->properties.begin ();
	this->mouse_input = mouse_input;
}

WallpaperEngine::Project::~Project () {
	if (this->context.projects.empty ()) {
		active_context = nullptr;
	}
}

int WallpaperEngine::Project::getHeight () const {
	if (this->wallpaper == nullptr) {
		return 1;
	}

	return this->wallpaper->getHeight ();
}

int WallpaperEngine::Project::getWidth () const {
	if (this->wallpaper == nullptr) {
		return 1;
	}

	return this->wallpaper->getWidth ();
}

void WallpaperEngine::Project::setOutputFramebuffer (const GLuint newFramebuffer) {
	this->framebuffer = newFramebuffer;

	if (this->wallpaper != nullptr) {
		this->wallpaper->setDestinationFramebuffer (newFramebuffer);
	}
}

void WallpaperEngine::Project::render () {
	if (active_context != &this->context) {
		active_context = &this->context;

		gladLoadGLLoader (wp_context_call_gl_proc_address);
	}

	if (this->renderContext == nullptr) {
		// initialize render if not available
		this->renderContext = std::make_unique<RenderContext> (this->context, *(*this->ref)->assetLocator);
		this->wallpaper = CWallpaper::fromWallpaper (
			*(*this->ref)->wallpaper, *this->renderContext, *this->context.audio, this->mouse_input
		);
		this->wallpaper->setDestinationFramebuffer (this->framebuffer);
	}

	this->wallpaper->render ();
}

wp_project_property* WallpaperEngine::Project::propertyListNext () {
	if (this->property_it == (*this->ref)->properties.end ()) {
		this->property_it = (*this->ref)->properties.begin ();
		return nullptr;
	}

	// get current property's info and copy it over
	const auto& property = this->property_it++;

	if (const auto boolean = property->second->asOrNull<PropertyBoolean> ()) {
		this->current_property = reinterpret_cast<wp_project_property*> (new wp_project_property_boolean {
			.base = { .type = wp_property_type_boolean, .name = boolean->name.c_str () },
			.value = boolean->getBool () });
	} else if (const auto color = property->second->asOrNull<PropertyColor> ()) {
		this->current_property = reinterpret_cast<wp_project_property*> (new wp_project_property_color {
			.base = { .type = wp_property_type_color, .name = color->name.c_str () },
			.r = color->getIVec4 ().r,
			.g = color->getIVec4 ().g,
			.b = color->getIVec4 ().b,
			.a = color->getIVec4 ().a,
		});
	} else if (const auto text = property->second->asOrNull<PropertyText> ()) {
		this->current_property = reinterpret_cast<wp_project_property*> (new wp_project_property_text {
			.base = { .type = wp_property_type_text, .name = text->name.c_str () },
			.value = text->getString ().c_str () });
	} else if (const auto slider = property->second->asOrNull<PropertySlider> ()) {
		this->current_property = reinterpret_cast<wp_project_property*> (new wp_project_property_slider {
			.base = { .type = wp_property_type_slider, .name = slider->name.c_str () },
			.min = slider->min,
			.max = slider->max,
			.step = slider->step,
			.value = slider->getFloat () });
	} else if (const auto combo = property->second->asOrNull<PropertyCombo> ()) {
		auto result = new wp_project_property_combo {
			.base = { .type = wp_property_type_combo, .name = combo->name.c_str () },
			.value_count = combo->values.size (),
			.values = new wp_project_property_combo_value[combo->values.size ()],
		};

		int current = 0;

		for (const auto& value : combo->values) {
			result->values[current++]
				= wp_project_property_combo_value { .key = value.first.c_str (), .value = value.second.c_str () };
		}

		this->current_property = reinterpret_cast<wp_project_property*> (result);
	} else if (const auto file = property->second->asOrNull<PropertyFile> ()) {
		this->current_property = reinterpret_cast<wp_project_property*> (new wp_project_property_file {
			.base = { .type = wp_property_type_file, .name = file->name.c_str () },
			.path = file->getString ().c_str () });
	} else if (const auto sceneTexture = property->second->asOrNull<PropertySceneTexture> ()) {
		this->current_property = reinterpret_cast<wp_project_property*> (new wp_project_property_scene_texture {
			.base = { .type = wp_property_type_scene_texture, .name = sceneTexture->name.c_str () },
			.value = sceneTexture->getString ().c_str () });
	} else {
		sLog.exception ("Unknown property type");
	}

	return this->current_property;
}

void WallpaperEngine::Project::propertyListReset () {
	this->property_it = (*this->ref)->properties.begin ();

	if (const auto combo = WP_PROPERTY_AS_COMBO (this->current_property)) {
		delete combo->values;
	}

	delete this->current_property;
	this->current_property = nullptr;
}

void WallpaperEngine::Project::propertySet (const std::string& key, bool value) {
	const auto it = (*this->ref)->properties.find (key);

	if (it == (*this->ref)->properties.end ()) {
		sLog.exception ("Property not found");
	}

	it->second->update (value);
}

void WallpaperEngine::Project::propertySet (const std::string& key, const std::string& value) {
	const auto it = (*this->ref)->properties.find (key);

	if (it == (*this->ref)->properties.end ()) {
		sLog.exception ("Property not found");
	}

	it->second->update (value);
}

void WallpaperEngine::Project::propertySet (const std::string& key, float value) {
	const auto it = (*this->ref)->properties.find (key);

	if (it == (*this->ref)->properties.end ()) {
		sLog.exception ("Property not found");
	}

	it->second->update (value);
}

void WallpaperEngine::Project::propertySet (const std::string& key, glm::vec4 value) {
	const auto it = (*this->ref)->properties.find (key);

	if (it == (*this->ref)->properties.end ()) {
		sLog.exception ("Property not found");
	}

	it->second->update (value);
}

void WallpaperEngine::Project::propertySet (const wp_project_property* property, float value) {
	if (property != this->current_property) {
		return;
	}

	this->propertySet (property->name, value);
}

void WallpaperEngine::Project::propertySet (const wp_project_property* property, const std::string& value) {
	if (property != this->current_property) {
		sLog.exception ("Property not found");
	}

	this->propertySet (property->name, value);
}

void WallpaperEngine::Project::propertySet (const wp_project_property* property, glm::vec4 value) {
	if (property != this->current_property) {
		sLog.exception ("Property not found");
	}

	this->propertySet (property->name, value);
}

void WallpaperEngine::Project::propertySet (const wp_project_property* property, bool value) {
	if (property != this->current_property) {
		sLog.exception ("Property not found");
	}

	this->propertySet (property->name, value);
}

void WallpaperEngine::Project::describe (wp_describe_callback* callback) {
	// TODO: CHANGE PRETTYPRINTER TO TAKE IN AN OSTREAM THAT WILL CALL THE CALLBACK INSTEAD OF PARSING EVERYTHING
	// UPFRONT
	auto prettyPrinter = Data::Dumpers::StringPrinter ();
	prettyPrinter.printWallpaper (*this->ref->get ()->wallpaper.get ());

	const auto result = prettyPrinter.str ();
	constexpr unsigned long chunkSize = 1024;

	for (unsigned long offset = 0; offset < result.size (); offset += chunkSize) {
		callback->write (
			callback->user_parameter, &result.c_str ()[offset], std::min (chunkSize, result.size () - offset)
		);
	}
}

WallpaperEngine::Project*
WallpaperEngine::Project::loadId (Context* context, wp_mouse_input* mouse_input, const int id) {
	return new Project (context, mouse_input, context->config->backgrounds_dir / std::to_string (id));
}

WallpaperEngine::Project*
WallpaperEngine::Project::loadId (Context* context, wp_mouse_input* mouse_input, const std::string& id) {
	return new Project (context, mouse_input, context->config->backgrounds_dir / id);
}

WallpaperEngine::Project*
WallpaperEngine::Project::loadFolder (Context* context, wp_mouse_input* mouse_input, const char* folder) {
	return new Project (context, mouse_input, folder);
}