// This code is a modification of the original projects that can be found at
// https://github.com/if1live/cef-gl-example
// https://github.com/andmcgregor/cefgui
#include "CWeb.h"
#include "WallpaperEngine/WebBrowser/CEF/WPSchemeHandlerFactory.h"

#include "WallpaperEngine/Audio/Drivers/Recorders/PlaybackRecorder.h"
#include "WallpaperEngine/Data/Model/Project.h"
#include "WallpaperEngine/Data/Model/Property.h"
#include "WallpaperEngine/Data/Model/Wallpaper.h"
#include "WallpaperEngine/Media/MediaSource.h"
#include "WallpaperEngine/Render/RenderContext.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <sstream>

#include <stb_image.h>

using namespace WallpaperEngine::Render;
using namespace WallpaperEngine::Render::Wallpapers;

using namespace WallpaperEngine::WebBrowser;
using namespace WallpaperEngine::WebBrowser::CEF;

namespace {
// Escape a UTF-8 string for embedding inside a JS double-quoted literal.
std::string jsEscape (const std::string& s) {
    std::string o;
    o.reserve (s.size () + 8);
    for (const char c : s) {
	switch (c) {
	    case '\\':
		o += "\\\\";
		break;
	    case '"':
		o += "\\\"";
		break;
	    case '\n':
		o += "\\n";
		break;
	    case '\r':
		o += "\\r";
		break;
	    case '\t':
		o += "\\t";
		break;
	    default:
		if (static_cast<unsigned char> (c) < 0x20) {
		    char b[8];
		    snprintf (b, sizeof (b), "\\u%04x", c);
		    o += b;
		} else {
		    o += c;
		}
	}
    }
    return o;
}

std::string base64 (const std::string& in) {
    static const char* T = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve (((in.size () + 2) / 3) * 4);
    int val = 0, bits = -6;
    for (const unsigned char c : in) {
	val = (val << 8) + c;
	bits += 8;
	while (bits >= 0) {
	    out.push_back (T[(val >> bits) & 0x3F]);
	    bits -= 6;
	}
    }
    if (bits > -6) {
	out.push_back (T[((val << 8) >> (bits + 8)) & 0x3F]);
    }
    while (out.size () % 4) {
	out.push_back ('=');
    }
    return out;
}

// Read an MPRIS artUrl (usually file://) into raw bytes; mimeOut gets the type.
std::string loadArtBytes (const std::string& artUrl, std::string& mimeOut) {
    std::string path;
    if (artUrl.rfind ("file://", 0) == 0) {
	path = artUrl.substr (7);
    } else if (!artUrl.empty () && artUrl[0] == '/') {
	path = artUrl;
    } else {
	return ""; // remote URLs: let the page load them directly if it wants
    }
    std::ifstream f (path, std::ios::binary);
    if (!f) {
	return "";
    }
    std::stringstream ss;
    ss << f.rdbuf ();
    mimeOut = (path.size () > 4 && path.substr (path.size () - 4) == ".png") ? "image/png" : "image/jpeg";
    return ss.str ();
}

std::string hex (int r, int g, int b) {
    char buf[8];
    snprintf (buf, sizeof (buf), "#%02x%02x%02x", r, g, b);
    return buf;
}

// A vibrant accent colour of the album art for page theming (WE's primaryColor): saturation- and
// brightness-weighted so it picks the cover's accent instead of a muddy average.
std::array<int, 3> dominantColor (const std::string& bytes) {
    int w = 0, h = 0, n = 0;
    stbi_uc* px = stbi_load_from_memory (
	reinterpret_cast<const stbi_uc*> (bytes.data ()), static_cast<int> (bytes.size ()), &w, &h, &n, 3
    );
    if (px == nullptr || w <= 0 || h <= 0) {
	if (px != nullptr) {
	    stbi_image_free (px);
	}
	return { 200, 200, 200 };
    }
    double rs = 0, gs = 0, bs = 0, wsum = 0;
    const int total = w * h;
    const int step = std::max (1, total / 4096);
    for (int i = 0; i < total; i += step) {
	const double r = px[i * 3], g = px[i * 3 + 1], b = px[i * 3 + 2];
	const double mx = std::max ({ r, g, b }), mn = std::min ({ r, g, b });
	const double sat = mx > 0 ? (mx - mn) / mx : 0;
	const double weight = sat * sat * (mx / 255.0) + 0.005;
	rs += r * weight;
	gs += g * weight;
	bs += b * weight;
	wsum += weight;
    }
    stbi_image_free (px);
    if (wsum <= 0) {
	return { 200, 200, 200 };
    }
    int r = static_cast<int> (rs / wsum), g = static_cast<int> (gs / wsum), b = static_cast<int> (bs / wsum);
    // Dark art yields a near-black accent; lift it (preserving hue) so it reads as a real colour.
    const int mx = std::max ({ r, g, b });
    if (mx > 0 && mx < 170) {
	const double f = 170.0 / mx;
	r = std::min (255, static_cast<int> (r * f));
	g = std::min (255, static_cast<int> (g * f));
	b = std::min (255, static_cast<int> (b * f));
    }
    return { r, g, b };
}
} // namespace

CWeb::CWeb (
    const Wallpaper& wallpaper, RenderContext& context, AudioContext& audioContext, WebBrowserContext& browserContext,
    const WallpaperState::TextureUVsScaling& scalingMode, const uint32_t& clampMode
) : CWallpaper (wallpaper, context, audioContext, scalingMode, clampMode), m_browserContext (browserContext) {
    // setup framebuffers
    this->setupFramebuffers ();

    CefWindowInfo window_info;
    window_info.SetAsWindowless (0);

    this->m_renderHandler = new WebBrowser::CEF::RenderHandler (this);

    CefBrowserSettings browserSettings;
    // CEF uses integer microseconds between frames, so keep the interval positive.
    browserSettings.windowless_frame_rate
	= std::clamp (context.getApp ().getContext ().settings.render.maximumFPS, 1, 1'000'000);

    this->m_client = new WebBrowser::CEF::BrowserClient (m_renderHandler, browserContext);
    // use the custom scheme for the wallpaper's files; the workshop id is the host
    const std::string htmlURL
	= WPSchemeHandlerFactory::generateSchemeUrl (this->getWeb ().project.workshopId, this->getWeb ().filename);
    this->m_browser
	= CefBrowserHost::CreateBrowserSync (window_info, this->m_client, htmlURL, browserSettings, nullptr, nullptr);
}

void CWeb::setSize (const int width, const int height) {
    this->m_width = width > 0 ? width : this->m_width;
    this->m_height = height > 0 ? height : this->m_height;

    // do not refresh the texture if any of the sizes are invalid
    if (this->m_width <= 0 || this->m_height <= 0) {
	return;
    }

    // reconfigure the texture
    glBindTexture (GL_TEXTURE_2D, this->getWallpaperTexture ());
    glTexImage2D (
	GL_TEXTURE_2D, 0, GL_RGBA8, this->getWidth (), this->getHeight (), 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr
    );

    // Notify cef that it was resized(maybe it's not even needed)
    this->m_browser->GetHost ()->WasResized ();
}

void CWeb::renderFrame (const glm::ivec4& viewport) {
    // ensure the viewport matches the window size, and resize if needed
    if (viewport.z != this->getWidth () || viewport.w != this->getHeight ()) {
	this->setSize (viewport.z, viewport.w);
    }
    this->m_renderHandler->uploadTexture ();

    // ensure the virtual mouse position is up to date
    this->updateMouse (viewport);
    // use the scene's framebuffer by default
    glBindFramebuffer (GL_FRAMEBUFFER, this->getWallpaperFramebuffer ());
    // ensure we render over the whole framebuffer
    glViewport (0, 0, this->getWidth (), this->getHeight ());

    // CEF queues paints between output frames. Keep the previous image when
    // there is no new paint; clearing here would flicker above the browser FPS.
    this->pushBridgeData ();
}

void CWeb::pushBridgeData () {
    if (!this->m_browser) {
	return;
    }
    const CefRefPtr<CefFrame> frame = this->m_browser->GetMainFrame ();
    if (!frame) {
	return;
    }
    this->m_frame++;
    const std::string url = frame->GetURL ();

    // Audio visualizer: 128 values (two channels) from the FFT, every frame.
    const auto& recorder = this->getAudioContext ().getRecorder ();
    std::string audio = "window.__wpAudio&&window.__wpAudio([";
    audio.reserve (1300);
    for (const float* channel : { recorder.rawAudio64Left, recorder.rawAudio64Right }) {
	for (int i = 0; i < 64; i++) {
	    char b[16];
	    snprintf (b, sizeof (b), "%.4f,", channel[i]);
	    audio += b;
	}
    }
    audio += "])";
    frame->ExecuteJavaScript (audio, url, 0);

    // Deliver settings after each main document loads, including reloads. The bridge queues
    // them if the page installs its listener asynchronously after the load event.
    if (this->m_client->getLoadGeneration () != this->m_propertiesGeneration) {
	this->m_propertiesGeneration = this->m_client->getLoadGeneration ();
	frame->ExecuteJavaScript (
	    "window.__wpApplyGeneral&&window.__wpApplyGeneral({fps:"
		+ std::to_string (this->getContext ().getApp ().getContext ().settings.render.maximumFPS) + "})",
	    url, 0
	);
	std::string props = "{";
	bool first = true;
	for (const auto& [name, prop] : this->getWeb ().project.properties) {
	    std::string value;
	    try {
		if (dynamic_cast<const Data::Model::PropertyColor*> (prop.get ()) != nullptr) {
		    const glm::vec3 v = prop->getVec3 ();
		    char b[64];
		    snprintf (b, sizeof (b), "\"%.4f %.4f %.4f\"", v.x, v.y, v.z);
		    value = b;
		} else if (dynamic_cast<const Data::Model::PropertyBoolean*> (prop.get ()) != nullptr) {
		    value = prop->getBool () ? "true" : "false";
		} else if (dynamic_cast<const Data::Model::PropertySlider*> (prop.get ()) != nullptr) {
		    char b[32];
		    snprintf (b, sizeof (b), "%.6g", prop->getFloat ());
		    value = b;
		} else if (dynamic_cast<const Data::Model::PropertyText*> (prop.get ()) != nullptr) {
		    continue; // text properties are UI labels, not values
		} else {
		    const auto* combo = dynamic_cast<const Data::Model::PropertyCombo*> (prop.get ());
		    const auto* literal = combo != nullptr ? combo->getSelectedWebLiteral () : nullptr;
		    value = literal != nullptr ? *literal : "\"" + jsEscape (prop->getString ()) + "\"";
		}
	    } catch (...) {
		continue;
	    }
	    if (!first) {
		props += ",";
	    }
	    first = false;
	    props += "\"" + jsEscape (name) + "\":{\"value\":" + value + "}";
	}
	props += "}";
	frame->ExecuteJavaScript ("window.__wpApplyProps&&window.__wpApplyProps(" + props + ")", url, 0);
    }

    // Media (now-playing) from the native MPRIS MediaSource, delivered to the page ~twice a second.
    if (this->m_frame % 30 == 0) {
	const auto& info = this->getContext ().getMediaSource ().getMediaInfo ();
	if (info.available) {
	    frame->ExecuteJavaScript (
		"window.__wpMediaProps&&window.__wpMediaProps({title:\"" + jsEscape (info.title) + "\",artist:\""
		    + jsEscape (info.artist) + "\",album:\"" + jsEscape (info.album) + "\"})",
		url, 0
	    );
	    frame->ExecuteJavaScript (
		"window.__wpMediaPlayback&&window.__wpMediaPlayback({state:"
		    + std::to_string (static_cast<int> (info.playbackState)) + "})",
		url, 0
	    );
	    // MPRIS position/duration are microseconds; the page's timeline expects seconds.
	    frame->ExecuteJavaScript (
		"window.__wpMediaTimeline&&window.__wpMediaTimeline({position:"
		    + std::to_string (info.position / 1000000.0)
		    + ",duration:" + std::to_string (info.duration / 1000000.0) + "})",
		url, 0
	    );
	    const std::string artUrl = info.url.value_or ("");
	    if (artUrl != this->m_lastArtSent) {
		this->m_lastArtSent = artUrl;
		std::string mime;
		const std::string bytes = loadArtBytes (artUrl, mime);
		if (!bytes.empty ()) {
		    const std::string dataUrl = "data:" + mime + ";base64," + base64 (bytes);
		    // WE thumbnail events carry primary+secondary+text colours; all are required or the
		    // page's primary->secondary gradient breaks and falls back to its default theme.
		    const auto c = dominantColor (bytes);
		    const std::string primary = hex (c[0], c[1], c[2]);
		    const std::string secondary = hex (c[0] * 4 / 10, c[1] * 4 / 10, c[2] * 4 / 10);
		    const double luma = 0.2126 * c[0] + 0.7152 * c[1] + 0.0722 * c[2];
		    const char* text = luma < 150 ? "#ffffff" : "#101010";
		    frame->ExecuteJavaScript (
			"window.__wpMediaThumb&&window.__wpMediaThumb({thumbnail:\"" + dataUrl + "\",primaryColor:\""
			    + primary + "\",secondaryColor:\"" + secondary + "\",textColor:\"" + text + "\"})",
			url, 0
		    );
		} else if (artUrl.rfind ("http", 0) == 0) {
		    // Remote art (e.g. Spotify https): no HTTP client here, so pass the URL through for the
		    // page to load. Send a neutral palette since theme colours need the pixels.
		    frame->ExecuteJavaScript (
			"window.__wpMediaThumb&&window.__wpMediaThumb({thumbnail:\"" + jsEscape (artUrl)
			    + "\",primaryColor:\"#5a7d9a\",secondaryColor:\"#24323e\",textColor:\"#ffffff\"})",
			url, 0
		    );
		}
	    }
	}
    }
}

void CWeb::updateMouse (const glm::ivec4& viewport) {
    // update virtual mouse position first
    auto& input = this->getContext ().getInputContext ().getMouseInput ();

    const glm::dvec2 position = input.position ();
    const auto leftClick = input.leftClick ();
    const auto rightClick = input.rightClick ();

    CefMouseEvent evt;
    // Set mouse current position. Maybe clamps are not needed
    evt.x = std::clamp (static_cast<int> (position.x - viewport.x), 0, viewport.z);
    // Convert from OpenGL coordinates (Y=0 at bottom) to CEF coordinates (Y=0 at top)
    evt.y = viewport.w - std::clamp (static_cast<int> (position.y - viewport.y), 0, viewport.w);
    // Send mouse position to cef
    this->m_browser->GetHost ()->SendMouseMoveEvent (evt, false);

    // TODO: ANY OTHER MOUSE EVENTS TO SEND?
    if (leftClick != this->m_leftClick) {
	this->m_browser->GetHost ()->SendMouseClickEvent (
	    evt, CefBrowserHost::MouseButtonType::MBT_LEFT,
	    leftClick == WallpaperEngine::Input::MouseClickStatus::Released, 1
	);
    }

    if (rightClick != this->m_rightClick) {
	this->m_browser->GetHost ()->SendMouseClickEvent (
	    evt, CefBrowserHost::MouseButtonType::MBT_RIGHT,
	    rightClick == WallpaperEngine::Input::MouseClickStatus::Released, 1
	);
    }

    this->m_leftClick = leftClick;
    this->m_rightClick = rightClick;
}

CWeb::~CWeb () {
    // CEF may deliver queued OnPaint callbacks after CloseBrowser. Detach the
    // handler first so those callbacks cannot dereference this destroyed CWeb.
    if (this->m_renderHandler != nullptr) {
	this->m_renderHandler->detach ();
    }

    if (this->m_browser != nullptr) {
	this->m_browser->GetHost ()->CloseBrowser (true);
    }
}
