#include <filesystem>
#include <fstream>

#include "frontends/content.h"

#include "WallpaperEngine/Configuration.h"
#include "WallpaperEngine/Data/Parsers/EffectParser.h"

/**
 * Actual representation of the wp_background_list structure
 */
struct wp_background_list_impl {
	std::filesystem::directory_iterator it;
	std::optional<std::filesystem::path> current_path;
	std::optional<std::filesystem::path> preview_path;

	wp_background_list_entry current_entry;
};

wp_background_list* wp_background_list_open (wp_configuration* config) {
	const auto configPtr = static_cast<WallpaperEngine::Configuration*> (config);

	if (configPtr->backgrounds_dir.empty ()) {
		return nullptr;
	}

	if (!std::filesystem::exists (configPtr->backgrounds_dir)) {
		return nullptr;
	}

	if (!std::filesystem::is_directory (configPtr->backgrounds_dir)) {
		return nullptr;
	}

	// open directory and allocate structs
	return new wp_background_list_impl { .it = std::filesystem::directory_iterator (configPtr->backgrounds_dir),
		                                 .current_entry = { .path = nullptr, .preview_path = nullptr } };
}

wp_background_list_entry* wp_background_list_next (wp_background_list* list) {
	const auto listPtr = static_cast<wp_background_list_impl*> (list);

	do {
		if (listPtr->it == std::filesystem::directory_iterator ()) {
			// also update the entry to ensure it points to nothing
			listPtr->current_path = std::nullopt;
			listPtr->preview_path = std::nullopt;

			break;
		}

		if (!listPtr->it->is_directory ()) {
			continue;
		}

		listPtr->current_path = listPtr->it->path ();
		++listPtr->it;
		break;
	} while (true);

	if (listPtr->current_path.has_value ()) {
		// open the project.json file, get the preview, and return the contents
		// no need to use the full-blown parser just for this

		// parse file off the json directly
		const auto json = WallpaperEngine::Data::Parsers::JSON::parse (
			std::ifstream (listPtr->current_path.value () / "project.json")
		);

		if (const auto preview = json.optional ("preview"); preview.has_value ()) {
			listPtr->preview_path = listPtr->current_path.value () / preview.value ();
		} else {
			listPtr->preview_path = std::nullopt;
		}
	}

	if (listPtr->current_path.has_value ()) {
		listPtr->current_entry.path = listPtr->current_path.value ().c_str ();
	} else {
		listPtr->current_entry.path = nullptr;
	}

	if (listPtr->preview_path.has_value ()) {
		listPtr->current_entry.preview_path = listPtr->preview_path.value ().c_str ();
	} else {
		listPtr->current_entry.preview_path = nullptr;
	}

	if (listPtr->current_path.has_value () || listPtr->preview_path.has_value ()) {
		return &listPtr->current_entry;
	}

	return nullptr;
}

void wp_background_list_close (wp_background_list* list) { delete static_cast<wp_background_list_impl*> (list); }