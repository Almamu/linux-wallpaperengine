#pragma once

#include <map>
#include <string>

#include "Types.h"
namespace WallpaperEngine::Data::Model {
enum PlaylistMode {
	PlaylistMode_Unknown = -1,
	PlaylistMode_Timer = 0,
	PlaylistMode_Logon = 1,
	PlaylistMode_Daytime = 2,
	PlaylistMode_Weekday = 3,
	PlaylsitMode_Never = 4,
};

enum PlaylistOrder {
	PlaylistOrder_Unknown = -1,
	PlaylistOrder_Random = 0,
	PlaylistOrder_Sorted = 1,
};

enum PlaylistTransition {
	PlaylistTransition_Unknown = -1,
	PlaylistTransition_None = 0, // none
	PlaylistTransition_Fade = 1, // 0
	PlaylistTransition_NoneReducedFlicker = 2, // -2
	// random with no transitionpool is all,
	// otherwise the values are the same as the ones used to parse these
	PlaylistTransition_Random = 3, // random
	PlaylistTransition_FadeToBlack = 4, // 18
	PlaylistTransition_Mosaic = 5, // 1
	PlaylistTransition_Diffuse = 6, // 2
	PlaylistTransition_HorizontalSlide = 7, // 3
	PlaylistTransition_VerticalSlide = 8, // 4
	PlaylistTransition_HorizontalFade = 9, // 5
	PlaylistTransition_VerticalFade = 10, // 6
	PlaylistTransition_Clouds = 11, // 7
	PlaylistTransition_BurnPaper = 12, // 8
	PlaylistTransition_Circular = 13, // 9
	PlaylistTransition_Zipper = 14, // 10
	PlaylistTransition_Door = 15, // 11
	PlaylistTransition_Lines = 16, // 12
	PlaylistTransition_RadialWipe = 17, // 22
	PlaylistTransition_Zoom = 18, // 13
	PlaylistTransition_Twister = 19, // 19
	PlaylistTransition_Drip = 20, // 14
	PlaylistTransition_Pixelate = 21, // 15
	PlaylistTransition_Bricks = 22, // 16
	PlaylistTransition_Paint = 23, // 17
	PlaylistTransition_BlackHole = 24, // 20
	PlaylistTransition_CRT = 25, // 21
	PlaylistTransition_GlassShatter = 26, // 23
	PlaylistTransition_Bullets = 27, // 24
	PlaylistTransition_Ice = 28, // 25
	PlaylistTransition_Boilover = 29, // 26
};

struct PlaylistItem {
	float daytimeend;
	std::string path;
};

struct Playlist {
	std::string name;
	PlaylistMode mode;
	PlaylistOrder order;
	int delay;
	PlaylistTransition transition;
	int transitiontime;
	std::vector<PlaylistItemUniquePtr> items;
};

struct Config {
	std::string installDirectory;
	PlaylistMap playlists;
};
}