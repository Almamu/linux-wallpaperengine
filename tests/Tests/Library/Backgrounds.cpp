#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "frontends/configuration.h"
#include "frontends/content.h"

#include "Fixtures/BackgroundListFixture.h"

#define REQUIRE_PATH_VALUES(root, entry_get, values, hasPreviews) \
	{ \
		wp_background_list_entry* entry = entry_get; \
		REQUIRE (entry != nullptr); \
		REQUIRE (entry->path != nullptr); \
		std::filesystem::path path = std::filesystem::path (entry->path).lexically_proximate(root); \
		if (hasPreviews.at (path)) { \
			REQUIRE (entry->preview_path != nullptr); \
			REQUIRE (std::string_view (entry->preview_path) == root / path / "preview.png"); \
		} else { \
			REQUIRE (entry->preview_path == nullptr); \
		} \
		REQUIRE (path.empty () == false); \
		values.insert_or_assign (*path.begin(), true); \
	}

TEST_CASE_METHOD (BackgroundListFixture, "Background list behaviour") {
	addBackground (3050);
	addBackground (5241);
	addBackground (9999);
	addBackground (100849, false);

	std::map<std::string, bool> values = {
		{"3050", false},
		{"5241", false},
		{"9999", false},
		{"100849", false}
	};

	std::map<std::string, bool> hasPreviews = {
		{"3050", true},
		{"5241", true},
		{"9999", true},
		{"100849", false}
	};

	wp_configuration* config = wp_config_create ();

	REQUIRE(wp_config_set_backgrounds_dir (config, root.c_str ()) == true);

	wp_background_list* list = wp_background_list_open (config);

	REQUIRE_PATH_VALUES(root, wp_background_list_next (list), values, hasPreviews);
	REQUIRE_PATH_VALUES(root, wp_background_list_next (list), values, hasPreviews);
	REQUIRE_PATH_VALUES(root, wp_background_list_next (list), values, hasPreviews);
	REQUIRE_PATH_VALUES(root, wp_background_list_next (list), values, hasPreviews);
	// ensure all the values were found
	std::ranges::for_each (values, [](auto& pair) -> void {
		REQUIRE(pair.second == true);
	});
	// cleanup the values map so it can be filled with just one again
	// logic was already validated, so the only thing we'll care about after
	// is that one entry with true is present
	values.clear ();
	// validate that after the last result we get a nullptr
	REQUIRE(wp_background_list_next (list) == nullptr);
	// but calling it again starts off the beginning again
	REQUIRE_PATH_VALUES(root, wp_background_list_next (list), values, hasPreviews);
	REQUIRE(values.size () == 1);
	REQUIRE(values.begin()->second == true);

	wp_background_list_close (list);

	wp_config_destroy (config);
}