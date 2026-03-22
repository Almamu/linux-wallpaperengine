#include <catch2/generators/catch_generators.hpp>
#include <catch2/catch_test_macros.hpp>

#include "frontends/configuration.h"
#include "frontends/playlists.h"

#include "tests/Fixtures/PlaylistsFixture.h"

TEST_CASE_METHOD (PlaylistsFixture, "Test") {
	addPlaylist (
		"test",
		PlaylistMode_Daytime,
		PlaylistOrder_Random,
		PlaylistTransition_None,
		1,
		1500,
		{
			"3326873240",
			"2317494988",
			"2931252198"
		}
	);

	createConfigFile ();

	wp_configuration* config = wp_config_create ();

	REQUIRE (wp_config_set_assets_dir (config, (root / "assets").c_str ()) == true);

	wp_playlists* lists	= wp_playlists_load (config);

	REQUIRE (lists != nullptr);

	wp_playlist_entry* entry = wp_playlists_next (lists);

	REQUIRE (entry != nullptr);
	REQUIRE ("test" == std::string (entry->name));
	REQUIRE (entry->item_count == 3);
	REQUIRE (entry->items != nullptr);
	REQUIRE (entry->daytimeend != nullptr);

	// validate that after the last result we get a nullptr
	REQUIRE (wp_playlists_next (lists) == nullptr);
	// but calling it again starts off the beginning again
	REQUIRE (wp_playlists_next (lists) != nullptr);

	wp_playlists_destroy (lists);
	wp_config_destroy (config);
}