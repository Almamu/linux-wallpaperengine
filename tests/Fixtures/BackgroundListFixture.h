#pragma once

#define STB_IMAGE_WRITE_IMPLEMENTATION

#include "WallpaperEngine/Data/JSON.h"
#include "WallpaperEngine/Utils/UUID.h"
#include "stb_image_write.h"

#include <filesystem>
#include <fstream>

inline std::array<uint32_t, 256 * 256> redSquare = [] {
	std::array<uint32_t, 256 * 256> data = {};

	for (int i = 0; i < 256 * 256; i++) {
		data[i] = 0xFF000000;
	}

	return data;
} ();

struct BackgroundListFixture {
	BackgroundListFixture() {
		// uuidv4 should be more than enough
		root = std::filesystem::temp_directory_path () / WallpaperEngine::Utils::UUID::UUIDv4 ();
	}

	~BackgroundListFixture() {
		std::filesystem::remove_all (root);
	}

	void addBackground (int id, bool hasPreview = true) {
		this->addBackground (std::to_string (id).c_str (), hasPreview);
	}

	void addBackground (const char* id, bool hasPreview = true) {
		const auto path = root / id;
		const auto project = path / "project.json";
		const auto preview = path / "preview.png";

		std::filesystem::create_directories (path);

		WallpaperEngine::Data::JSON::JSON json = {};

		if (hasPreview) {
			json["preview"] = "preview.png";
		}

		std::ofstream contents (preview);
		std::ofstream (project) << json;

		// create a red square image of 256x256
		stbi_write_png_to_func ([] (void* context, void* data, int size) -> void {
			*static_cast<std::ofstream*> (context) << std::string (static_cast<const char*> (data), size);
		}, &contents, 256, 256, 4, redSquare.data (), 0);
	}

	std::filesystem::path root;
};
