#include "CVideo.h"

#include "WallpaperEngine/Data/Model/Project.h"
#include "WallpaperEngine/Data/Model/Property.h"
#include "WallpaperEngine/Data/Model/Wallpaper.h"
#include "WallpaperEngine/VideoPlayback/MPV/GLPlayer.h"
#include "WallpaperEngine/Logging/Log.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <exception>
#include <utility>

using namespace WallpaperEngine;
using namespace WallpaperEngine::Render;
using namespace WallpaperEngine::Render::Wallpapers;
using namespace WallpaperEngine::VideoPlayback::MPV;

CVideo::CVideo (
    const Wallpaper& wallpaper, RenderContext& context, AudioContext& audioContext,
    const WallpaperState::TextureUVsScaling& scalingMode, const uint32_t& clampMode
) : CWallpaper (wallpaper, context, audioContext, scalingMode, clampMode) {
    // setup framebuffers
    this->setupFramebuffers ();
    this->setupColorLut ();

    const std::filesystem::path videopath
	= this->getVideo ().project.assetLocator->physicalPath (this->getVideo ().filename);

    // Match the allocated attachment until mpv reports the decoded dimensions.
    this->m_player = std::make_unique<GLPlayer> (
	this->getContext (), this->CWallpaper::getWallpaperTexture (), videopath, this->getWidth (), this->getHeight (),
	this->CWallpaper::getWallpaperFramebuffer ()
    );
    // The output driver paces presentation. Waiting for an MP4 frame here also
    // stalls every cooperative scene-loading slice while this video is outgoing.
    this->m_player->setNonBlocking ();
    this->m_player->enableHDR ();
    // --silent disables audio (audio.enabled = false) and must mute video wallpapers too;
    // a volume of 0 mutes the mpv backend (matching what --volume 0 already does).
    const auto& audioSettings = this->getContext ().getApp ().getContext ().settings.audio;
    this->m_player->setVolume (audioSettings.enabled ? audioSettings.volume * 100.0 / 128.0 : 0.0);
    // make sure the video has at least one usage marked, this ensures the video plays
    this->m_player->incrementUsageCount ();
}

void CVideo::setupColorLut () {
    const auto& properties = this->getVideo ().project.properties;
    const auto& overrides = this->getContext ().getApp ().getContext ().settings.general.properties;
    const auto readOverride = [&] (const std::string& key) -> const std::string* {
	if (const auto live = this->m_liveColorProperties.find (key); live != this->m_liveColorProperties.end ())
	    return &live->second;
	const auto override = overrides.find (key);
	return override != overrides.end () ? &override->second : nullptr;
    };
    std::string selection;
    if (const auto property = properties.find ("wcc_v"); property != properties.end ()
	&& property->second->getType () == Data::Model::DynamicValue::String) {
	selection = property->second->getString ();
    }
    if (const auto* override = readOverride ("wcc_v")) {
	selection = *override;
    }
    float amount = 100.0f;
    if (const auto property = properties.find ("wcc_amt"); property != properties.end ()) {
	const auto type = property->second->getType ();
	if (type == Data::Model::DynamicValue::Float || type == Data::Model::DynamicValue::Int)
	    amount = property->second->getFloat ();
    }
    if (const auto* override = readOverride ("wcc_amt")) {
	const auto& text = *override;
	float parsed;
	const auto result = std::from_chars (text.data (), text.data () + text.size (), parsed);
	if (result.ec == std::errc {} && result.ptr == text.data () + text.size () && std::isfinite (parsed))
	    amount = parsed;
    }
    const float strength = amount / 100.0f;
    if (selection.empty () || strength <= 0.0f) {
	this->m_colorLut.reset ();
	this->m_colorLutStrength = strength;
	return;
    }
    auto colorLut = this->getContext ().resolveTexture ("lut/" + selection, this->getAssetLocator (), 1);
    if (colorLut->getTextureTarget () != GL_TEXTURE_3D)
	sLog.exception ("Video image filter requires a 3D texture");
    this->m_colorLut = std::move (colorLut);
    this->m_colorLutStrength = strength;
}

std::pair<GLuint, float> CVideo::getColorLut () const {
    if (!this->m_colorLut) return {};
    return { this->m_colorLut->getTextureID (0), this->m_colorLutStrength };
}

CVideo::~CVideo () { this->m_player->decrementUsageCount (); }

bool CVideo::hasHDRSource () const { return this->m_player->hasHDRSource (); }

void CVideo::prepareHDRPresentation (const bool enabled) { this->m_player->setHDRPresentation (enabled); }

float CVideo::getHDRSourceScale () const {
    // mpv vo_gpu's linear reference is203 nits; Windows-scRGB's unit is80 nits.
    return this->m_player->isHDRFrame () ? 203.0f / 80.0f : 0.0f;
}

void CVideo::renderFrame (const glm::ivec4& viewport) {
    // Texture creation must run after the output's graphics context becomes current.
    if (this->m_colorLutDirty) {
	try {
	    this->setupColorLut ();
	} catch (const std::exception& error) {
	    sLog.error ("Cannot update video image filter: ", error.what ());
	}
	this->m_colorLutDirty = false;
    }

    // ensure the video's audio follows audio detection rules
    if (this->getContext ().getApp ().getContext ().settings.audio.enabled
	&& this->m_muted != this->getAudioContext ().getDriver ().getAudioDetector ().anythingPlaying ()) {
	this->m_muted = !this->m_muted;

	if (this->m_muted) {
	    this->m_player->setMuted ();
	} else {
	    this->m_player->clearMuted ();
	}
    }

    this->m_player->render (this->getContext ().getApp ().getContext ().settings.screenshot.take);
}

bool CVideo::isReadyForScreenshot () const { return this->m_player->hasScreenshotFrame (); }

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

WallpaperState::TextureUVs CVideo::getPresentationUVs () const {
    auto uvs = CWallpaper::getPresentationUVs ();
    const auto& properties = this->getVideo ().project.properties;
    const auto property = properties.find ("alignmentfliph");
    bool flip = property != properties.end ()
	&& property->second->getType () == Data::Model::DynamicValue::Boolean
	&& property->second->getBool ();

    // Native adds this built-in setting even when project.json omits it.
    const auto& overrides = this->getContext ().getApp ().getContext ().settings.general.properties;
    if (const auto override = overrides.find ("alignmentfliph"); override != overrides.end ()) {
	flip = override->second == "true" || override->second == "1";
    }
    if (const auto live = this->m_liveColorProperties.find ("alignmentfliph");
	live != this->m_liveColorProperties.end ()) {
	flip = live->second == "true" || live->second == "1";
    }
    if (flip) {
	std::swap (uvs.ustart, uvs.uend);
    }
    return uvs;
}

bool CVideo::setColorProperty (const std::string& key, const std::string& value) {
    if (key != "alignmentfliph" && key != "wec_e" && key != "wec_brs" && key != "wec_con" && key != "wec_sa"
	&& key != "wec_hue" && key != "wcc_v" && key != "wcc_amt")
	return false;
    if (key == "wec_e" && value != "true" && value != "false" && value != "1" && value != "0")
	return true;
    if (key == "wcc_amt" || key == "wec_brs" || key == "wec_con" || key == "wec_sa" || key == "wec_hue") {
	float parsed;
	const auto result = std::from_chars (value.data (), value.data () + value.size (), parsed);
	if (result.ec != std::errc {} || result.ptr != value.data () + value.size () || !std::isfinite (parsed))
	    return true;
    }
    this->m_liveColorProperties.insert_or_assign (key, value);
    if (key == "wcc_v" || key == "wcc_amt") this->m_colorLutDirty = true;
    return true;
}

glm::vec4 CVideo::getColorCorrection () const {
    const auto& properties = this->getVideo ().project.properties;
    const auto& overrides = this->getContext ().getApp ().getContext ().settings.general.properties;
    const auto readOverride = [&] (const std::string& key) -> const std::string* {
	if (const auto live = this->m_liveColorProperties.find (key); live != this->m_liveColorProperties.end ())
	    return &live->second;
	const auto override = overrides.find (key);
	return override != overrides.end () ? &override->second : nullptr;
    };
    const auto property = properties.find ("wec_e");
    bool enabled = property != properties.end ()
	&& property->second->getType () == Data::Model::DynamicValue::Boolean
	&& property->second->getBool ();
    if (const auto* override = readOverride ("wec_e")) {
	enabled = *override == "true" || *override == "1";
    }
    if (!enabled) return { 1.0f, 1.0f, 1.0f, 0.0f };

    const auto read = [&] (const std::string& name) {
	float value = 50.0f;
	if (const auto setting = properties.find (name); setting != properties.end ()) {
	    const auto type = setting->second->getType ();
	    if (type == Data::Model::DynamicValue::Float || type == Data::Model::DynamicValue::Int)
		value = setting->second->getFloat ();
	}
	if (const auto* override = readOverride (name)) {
	    const auto& text = *override;
	    float parsed;
	    const auto result = std::from_chars (text.data (), text.data () + text.size (), parsed);
	    if (result.ec == std::errc {} && result.ptr == text.data () + text.size () && std::isfinite (parsed))
		value = parsed;
	}
	return value;
    };
    // Native Scene DX11 maps 0..100 sliders to ccsimple's brightness,
    // contrast, HSV saturation and hue parameters; their neutral value is 50.
    return { std::pow (read ("wec_brs") / 50.0f, 2.0f),
	     std::pow (read ("wec_con") / 50.0f, 0.5f),
	     std::pow (read ("wec_sa") / 50.0f, 0.5f), read ("wec_hue") / 100.0f - 0.5f };
}
