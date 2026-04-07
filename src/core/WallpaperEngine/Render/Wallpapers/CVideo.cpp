#include "CVideo.h"

#include "WallpaperEngine/Audio/AudioPlayingDetector.h"
#include "WallpaperEngine/Context.h"
#include "WallpaperEngine/Data/Model/Project.h"
#include "WallpaperEngine/Data/Model/Wallpaper.h"
#include "WallpaperEngine/VideoPlayback/MPV/GLPlayer.h"

using namespace WallpaperEngine;
using namespace WallpaperEngine::Render;
using namespace WallpaperEngine::Render::Wallpapers;
using namespace WallpaperEngine::VideoPlayback::MPV;

CVideo::CVideo (const Wallpaper& wallpaper, RenderContext& context, AudioContext& audioContext) :
	CWallpaper (wallpaper, context, audioContext, nullptr) {
	// setup framebuffers
	this->setupFramebuffers ();

	const std::filesystem::path videopath
		= this->getVideo ().project.assetLocator->physicalPath (this->getVideo ().filename);

	// create a player with a small framebuffer
	// this will be changed after mpv starts playback and sees the video resolution
	this->m_player = std::make_unique<GLPlayer> (
		this->getContext (), this->CWallpaper::getWallpaperTexture (), videopath, 64, 64,
		this->CWallpaper::getWallpaperFramebuffer ()
	);
	this->m_player->setVolume (this->getContext ().getContext ().config->volume * 100.0 / 128.0);
	// make sure the video has at least one usage marked; this ensures the video plays
	this->m_player->incrementUsageCount ();
}

CVideo::~CVideo () { this->m_player->decrementUsageCount (); }

void CVideo::renderFrame () {
	// ensure the video's audio follows audio detection rules
	if (this->getContext ().getContext ().config->enableAudio
	    && this->m_muted != this->getContext ().getContext ().audio->getDetector ().anythingPlaying ()) {
		this->m_muted = !this->m_muted;

		if (this->m_muted) {
			this->m_player->setMuted ();
		} else {
			this->m_player->clearMuted ();
		}
	}

	this->m_player->render ();
}

const Data::Model::Video& CVideo::getVideo () const { return *this->getWallpaperData ().as<Data::Model::Video> (); }

void CVideo::setPause (bool newState) {
	if (newState) {
		this->m_player->setPaused ();
	} else {
		this->m_player->clearPaused ();
	}
}

int CVideo::getWidth () const { return this->m_player ? this->m_player->getWidth () : 16; }

int CVideo::getHeight () const { return this->m_player ? this->m_player->getHeight () : 16; }
