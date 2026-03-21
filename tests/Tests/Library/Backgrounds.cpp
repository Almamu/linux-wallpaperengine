#include <catch2/generators/catch_generators.hpp>
#include <catch2/catch_test_macros.hpp>

#include "frontends/configuration.h"
#include "frontends/content.h"

#include "tests/Fixtures/BackgroundListFixture.h"

wp_configuration* basic_setup (const std::filesystem::path& root) {
	wp_configuration* config = wp_config_create ();

	wp_config_set_backgrounds_dir (config, root.c_str ());

	return config;
}

#define REQUIRE_PATH_VALUES(root, entry_get, background, hasPreview) \
	{ \
		wp_background_list_entry* entry = entry_get; \
		REQUIRE (entry != nullptr); \
		REQUIRE (entry->path != nullptr); \
		if (hasPreview) { REQUIRE (entry->preview_path != nullptr); } \
		REQUIRE (std::string_view (entry->path) == root / background); \
		if (hasPreview) { REQUIRE (std::string_view (entry->preview_path) == root / background / "preview.png"); } \
	}

TEST_CASE_METHOD (BackgroundListFixture, "Test") {
	addBackground (3050);
	addBackground (5241);
	addBackground (9999);
	addBackground (100849, false);

	wp_configuration* config = basic_setup (root);

	wp_background_list* list = wp_background_list_open (config);

	// TODO: THIS TEST SHOULD NOT TAKE INTO ACCOUNT ORDER, BUT THIS SHOULD BE ENOUGH FOR NOW
	REQUIRE_PATH_VALUES(root, wp_background_list_next (list), "100849", false);
	REQUIRE_PATH_VALUES(root, wp_background_list_next (list), "9999", true);
	REQUIRE_PATH_VALUES(root, wp_background_list_next (list), "5241", true);
	REQUIRE_PATH_VALUES(root, wp_background_list_next (list), "3050", true);

	wp_background_list_entry* entry = wp_background_list_next (list);

	REQUIRE (entry == nullptr);

	wp_background_list_close (list);

	wp_config_destroy (config);
}