#include "CFBO.h"
#include "WallpaperEngine/Logging/Log.h"

#include <algorithm>
#include <bit>
#include <map>
#include <mutex>
#include <ranges>
#include <sstream>
#include <tuple>
#include <vector>

using namespace WallpaperEngine::Render;

std::atomic<size_t> CFBO::s_liveCount { 0 };
std::atomic<size_t> CFBO::s_liveGpuBytes { 0 };

namespace {
struct LiveFBOInfo {
    std::string name;
    uint32_t width;
    uint32_t height;
    size_t bytes;
};

std::mutex s_liveFBOsMutex;
std::map<const CFBO*, LiveFBOInfo> s_liveFBOs;

GLenum colorInternalFormat (const TextureFormat format) {
    switch (format) {
	case TextureFormat_R8: return GL_R8;
	case TextureFormat_R16f: return GL_R16F;
	case TextureFormat_RG1616f: return GL_RG16F;
	case TextureFormat_RGBA16161616f: return GL_RGBA16F;
	default: return GL_RGBA8;
    }
}

GLenum colorUploadType (const TextureFormat format) {
    switch (format) {
	case TextureFormat_R16f:
	case TextureFormat_RG1616f:
	case TextureFormat_RGBA16161616f: return GL_FLOAT;
	default: return GL_UNSIGNED_BYTE;
    }
}

size_t colorPixelBytes (const TextureFormat format) {
    switch (format) {
	case TextureFormat_R8: return 1;
	case TextureFormat_R16f: return 2;
	case TextureFormat_RGBA16161616f: return 8;
	default: return 4;
    }
}

uint32_t supportedSampleCount (const uint32_t requested, const bool withDepth, const GLenum colorFormat) {
    if (requested <= 1) return 1;

    GLint maxSamples = 0;
    glGetIntegerv (GL_MAX_SAMPLES, &maxSamples);
    if (requested > static_cast<uint32_t> (std::max (maxSamples, 0))) return 1;

    // GL 4.2 exposes exact per-format counts. Require the requested count for both
    // attachments instead of allowing the driver to round it to another quality.
    if (GLEW_VERSION_4_2 || GLEW_ARB_internalformat_query) {
	auto supports = [requested] (const GLenum format) {
	    GLint count = 0;
	    glGetInternalformativ (GL_RENDERBUFFER, format, GL_NUM_SAMPLE_COUNTS, 1, &count);
	    if (count <= 0) return false;
	    std::vector<GLint> samples (count);
	    glGetInternalformativ (GL_RENDERBUFFER, format, GL_SAMPLES, count, samples.data ());
	    return std::ranges::find (samples, static_cast<GLint> (requested)) != samples.end ();
	};
	if (!supports (colorFormat) || (withDepth && !supports (GL_DEPTH_COMPONENT32F))) return 1;
	return requested;
    }

    // GL 3.3 fallback: allocate disposable storage and accept it only when the
    // driver reports the exact requested count for every needed format.
    const auto probe = [requested] (const GLenum format) {
	GLuint renderbuffer = GL_NONE;
	glGenRenderbuffers (1, &renderbuffer);
	glBindRenderbuffer (GL_RENDERBUFFER, renderbuffer);
	glRenderbufferStorageMultisample (GL_RENDERBUFFER, requested, format, 1, 1);
	const GLenum error = glGetError ();
	GLint actual = 0;
	if (error == GL_NO_ERROR) glGetRenderbufferParameteriv (GL_RENDERBUFFER, GL_RENDERBUFFER_SAMPLES, &actual);
	glDeleteRenderbuffers (1, &renderbuffer);
	return error == GL_NO_ERROR && actual == static_cast<GLint> (requested);
    };
    return probe (colorFormat) && (!withDepth || probe (GL_DEPTH_COMPONENT32F)) ? requested : 1;
}
}

CFBO::CFBO (
    std::string name, const TextureFormat format, const uint32_t flags, const float scale, uint32_t realWidth,
    uint32_t realHeight, uint32_t textureWidth, uint32_t textureHeight, bool withDepthBuffer, bool depthTexture,
    uint32_t requestedSamples, bool reverseDepth
) : m_depthTexture (depthTexture), m_withDepthBuffer (withDepthBuffer), m_scale (scale), m_name (std::move (name)),
    m_format (format), m_flags (flags) {
    m_reverseDepth = reverseDepth;
    // Hidden effect inputs can have an authored zero dimension (for example a
    // 64x0 audio buffer). Keep their logical size, but allocate complete GL storage.
    textureWidth = std::max (textureWidth, 1u);
    textureHeight = std::max (textureHeight, 1u);

    // NOTE: the framebuffer object itself is created lazily on first use (see
    // ensureFramebuffer): FBOs are not shared between GL contexts, so when this
    // constructor runs on the async switch worker only the shared objects
    // (texture, renderbuffer) can be created here
    this->m_samples = this->m_depthTexture
	? 1
	: supportedSampleCount (requestedSamples, withDepthBuffer, colorInternalFormat (this->m_format));
    this->m_multisampleRendering = this->m_samples > 1;

    // create the main texture
    glGenTextures (1, &this->m_texture);
    // bind the new texture to set settings on it
    glBindTexture (GL_TEXTURE_2D, this->m_texture);
    if (this->m_depthTexture) {
	glTexImage2D (
	    GL_TEXTURE_2D, 0, getDepthInternalFormat (), textureWidth, textureHeight, 0, GL_DEPTH_COMPONENT,
	    GL_FLOAT, nullptr
	);
	// Stock scene shaders declare the shadow atlas as sampler2DComparison. Hardware
	// comparison also lets their 9-tap PCF path work without a custom sampler object.
	glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
	glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, m_reverseDepth ? GL_GREATER : GL_LEQUAL);
	const GLfloat borderDepth[] = { getFarDepth (), getFarDepth (), getFarDepth (), getFarDepth () };
	glTexParameterfv (GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderDepth);
    } else {
	glTexImage2D (
	    GL_TEXTURE_2D, 0, colorInternalFormat (this->m_format), textureWidth, textureHeight, 0, GL_RGBA,
	    colorUploadType (this->m_format), nullptr
	);
    }
    this->configureMipmaps (textureWidth, textureHeight);
    // label stuff for debugging
#if !NDEBUG
    glObjectLabel (GL_TEXTURE, this->m_texture, -1, this->m_name.c_str ());
#endif /* DEBUG */
    // set filtering parameters, otherwise the texture is not rendered
    if (this->m_depthTexture) {
	glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    } else if (flags & TextureFlags_ClampUVs) {
	glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    } else if (flags & TextureFlags_ClampUVsBorder) {
	glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    } else {
	glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    }

    if (flags & TextureFlags_NoInterpolation) {
	glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    } else {
	glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	// Wallpaper Engine reserves mip generation for its dedicated
	// _rt_MipMappedFrameBuffer. Ordinary image-layer composites must stay on level 0:
	// their transparent RGB is not premultiplied, so averaging it into generated mip
	// levels produces bright fringes around translucent artwork.
	glTexParameteri (
	    GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, this->hasMipmaps () ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR
	);
    }

    glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY, 1.0f);

    // 3D scenes depth-test their models, so the scene framebuffer needs a depth attachment
    // (renderbuffers are shared objects, so this is safe on the worker context too)
    if (withDepthBuffer && !this->m_depthTexture && this->m_samples == 1) {
	glGenRenderbuffers (1, &this->m_depthbuffer);
	glBindRenderbuffer (GL_RENDERBUFFER, this->m_depthbuffer);
	glRenderbufferStorage (GL_RENDERBUFFER, getDepthInternalFormat (), textureWidth, textureHeight);
    }
    if (this->m_samples > 1) {
	glGenRenderbuffers (1, &this->m_multisampleColor);
	glBindRenderbuffer (GL_RENDERBUFFER, this->m_multisampleColor);
	glRenderbufferStorageMultisample (
	    GL_RENDERBUFFER, this->m_samples, colorInternalFormat (this->m_format), textureWidth, textureHeight
	);
	GLint colorSamples = 0;
	glGetRenderbufferParameteriv (GL_RENDERBUFFER, GL_RENDERBUFFER_SAMPLES, &colorSamples);
	bool allocated = glGetError () == GL_NO_ERROR && colorSamples == static_cast<GLint> (this->m_samples);
	if (withDepthBuffer) {
	    glGenRenderbuffers (1, &this->m_multisampleDepth);
	    glBindRenderbuffer (GL_RENDERBUFFER, this->m_multisampleDepth);
	    glRenderbufferStorageMultisample (
		GL_RENDERBUFFER, this->m_samples, GL_DEPTH_COMPONENT32F, textureWidth, textureHeight
	    );
	    GLint depthSamples = 0;
	    glGetRenderbufferParameteriv (GL_RENDERBUFFER, GL_RENDERBUFFER_SAMPLES, &depthSamples);
	    allocated = allocated && glGetError () == GL_NO_ERROR
		&& depthSamples == static_cast<GLint> (this->m_samples);
	}
	if (!allocated) {
	    glDeleteRenderbuffers (1, &this->m_multisampleColor);
	    glDeleteRenderbuffers (1, &this->m_multisampleDepth);
	    this->m_multisampleColor = this->m_multisampleDepth = GL_NONE;
	    this->m_samples = 1;
	    this->m_multisampleRendering = false;
	    if (withDepthBuffer) {
		glGenRenderbuffers (1, &this->m_depthbuffer);
		glBindRenderbuffer (GL_RENDERBUFFER, this->m_depthbuffer);
		glRenderbufferStorage (GL_RENDERBUFFER, getDepthInternalFormat (), textureWidth, textureHeight);
	    }
	}
    }
    if (this->hasMipmaps ()) {
	// Reflection textures can be copied or sampled before their context-local
	// framebuffer is first requested. Initialize the shared storage now.
	this->clearTextureStorage ();
    }

    this->m_resolution = { textureWidth, textureHeight, realWidth, realHeight };
    this->m_approximateGpuBytes = this->calculateStorageBytes (textureWidth, textureHeight);
    s_liveCount.fetch_add (1, std::memory_order_relaxed);
    s_liveGpuBytes.fetch_add (this->m_approximateGpuBytes, std::memory_order_relaxed);
    {
	std::lock_guard lock (s_liveFBOsMutex);
	s_liveFBOs.emplace (
	    this,
	    LiveFBOInfo {
		.name = this->m_name.empty () ? "<unnamed>" : this->m_name,
		.width = textureWidth,
		.height = textureHeight,
		.bytes = this->m_approximateGpuBytes,
	    }
	);
    }

    // create the textureframe entries
    const auto frame = std::make_shared<Frame> ();

    frame->frameNumber = 0;
    frame->frametime = 0;
    frame->height1 = textureHeight;
    frame->height2 = realHeight;
    frame->width1 = textureWidth;
    frame->width2 = realWidth;
    frame->x = 0;
    frame->y = 0;

    this->m_frames.push_back (frame);
}

void CFBO::resize (uint32_t width, uint32_t height) {
    width = std::max (width, 1u);
    height = std::max (height, 1u);
    if (m_resolution == glm::vec4 (width, height, width, height)) {
	return;
    }
    glBindTexture (GL_TEXTURE_2D, m_texture);
    glTexImage2D (
	GL_TEXTURE_2D, 0, m_depthTexture ? getDepthInternalFormat () : colorInternalFormat (m_format), width, height, 0,
	m_depthTexture ? GL_DEPTH_COMPONENT : GL_RGBA, m_depthTexture ? GL_FLOAT : colorUploadType (m_format),
	nullptr
    );
    this->configureMipmaps (width, height);
    if (m_depthbuffer != GL_NONE) {
	glBindRenderbuffer (GL_RENDERBUFFER, m_depthbuffer);
	glRenderbufferStorage (GL_RENDERBUFFER, getDepthInternalFormat (), width, height);
    }
    if (m_multisampleColor != GL_NONE) {
	glBindRenderbuffer (GL_RENDERBUFFER, m_multisampleColor);
	glRenderbufferStorageMultisample (
	    GL_RENDERBUFFER, m_samples, colorInternalFormat (m_format), width, height
	);
    }
    if (m_multisampleDepth != GL_NONE) {
	glBindRenderbuffer (GL_RENDERBUFFER, m_multisampleDepth);
	glRenderbufferStorageMultisample (GL_RENDERBUFFER, m_samples, GL_DEPTH_COMPONENT32F, width, height);
    }
    if (m_samples > 1) {
	GLint colorSamples = 0, depthSamples = static_cast<GLint> (m_samples);
	glBindRenderbuffer (GL_RENDERBUFFER, m_multisampleColor);
	glGetRenderbufferParameteriv (GL_RENDERBUFFER, GL_RENDERBUFFER_SAMPLES, &colorSamples);
	bool allocated = glGetError () == GL_NO_ERROR && colorSamples == static_cast<GLint> (m_samples);
	if (m_multisampleDepth != GL_NONE) {
	    glBindRenderbuffer (GL_RENDERBUFFER, m_multisampleDepth);
	    glGetRenderbufferParameteriv (GL_RENDERBUFFER, GL_RENDERBUFFER_SAMPLES, &depthSamples);
	    allocated = allocated && glGetError () == GL_NO_ERROR && depthSamples == static_cast<GLint> (m_samples);
	}
	if (!allocated) {
	    glDeleteFramebuffers (1, &m_multisampleFramebuffer);
	    glDeleteRenderbuffers (1, &m_multisampleColor);
	    glDeleteRenderbuffers (1, &m_multisampleDepth);
	    m_multisampleFramebuffer = m_multisampleColor = m_multisampleDepth = GL_NONE;
	    m_samples = 1;
	    m_multisampleRendering = false;
	    if (m_withDepthBuffer) {
		glGenRenderbuffers (1, &m_depthbuffer);
		glBindRenderbuffer (GL_RENDERBUFFER, m_depthbuffer);
		glRenderbufferStorage (GL_RENDERBUFFER, getDepthInternalFormat (), width, height);
		if (m_framebuffer != GL_NONE) {
		    GLint previousDraw = GL_NONE, previousRead = GL_NONE;
		    glGetIntegerv (GL_DRAW_FRAMEBUFFER_BINDING, &previousDraw);
		    glGetIntegerv (GL_READ_FRAMEBUFFER_BINDING, &previousRead);
		    glBindFramebuffer (GL_FRAMEBUFFER, m_framebuffer);
		    glFramebufferRenderbuffer (GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_depthbuffer);
		    if (glCheckFramebufferStatus (GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
			sLog.exception ("Single-sample fallback framebuffer is not properly set");
		    glBindFramebuffer (GL_DRAW_FRAMEBUFFER, previousDraw);
		    glBindFramebuffer (GL_READ_FRAMEBUFFER, previousRead);
		}
	    }
	}
    }
    this->clearTextureStorage ();
    if (this->m_samples > 1) {
	this->ensureMultisampleFramebuffer ();
	GLint previousDraw = GL_NONE, previousRead = GL_NONE;
	GLboolean colorMask[4], depthMask;
	glGetIntegerv (GL_DRAW_FRAMEBUFFER_BINDING, &previousDraw);
	glGetIntegerv (GL_READ_FRAMEBUFFER_BINDING, &previousRead);
	glGetBooleanv (GL_COLOR_WRITEMASK, colorMask);
	glGetBooleanv (GL_DEPTH_WRITEMASK, &depthMask);
	const GLboolean scissor = glIsEnabled (GL_SCISSOR_TEST);
	glBindFramebuffer (GL_FRAMEBUFFER, this->m_multisampleFramebuffer);
	glDisable (GL_SCISSOR_TEST);
	glColorMask (GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	glDepthMask (GL_TRUE);
	const GLfloat zero[4] = {};
	const GLfloat farDepth = getFarDepth ();
	glClearBufferfv (GL_COLOR, 0, zero);
	if (this->m_multisampleDepth != GL_NONE) glClearBufferfv (GL_DEPTH, 0, &farDepth);
	glColorMask (colorMask[0], colorMask[1], colorMask[2], colorMask[3]);
	glDepthMask (depthMask);
	if (scissor) glEnable (GL_SCISSOR_TEST);
	glBindFramebuffer (GL_DRAW_FRAMEBUFFER, previousDraw);
	glBindFramebuffer (GL_READ_FRAMEBUFFER, previousRead);
    }
    m_resolution = { width, height, width, height };
    m_frames.front ()->width1 = m_frames.front ()->width2 = width;
    m_frames.front ()->height1 = m_frames.front ()->height2 = height;
    s_liveGpuBytes.fetch_sub (m_approximateGpuBytes, std::memory_order_relaxed);
    m_approximateGpuBytes = this->calculateStorageBytes (width, height);
    s_liveGpuBytes.fetch_add (m_approximateGpuBytes, std::memory_order_relaxed);
    {
	std::lock_guard lock (s_liveFBOsMutex);
	auto& info = s_liveFBOs.at (this);
	info.width = width;
	info.height = height;
	info.bytes = m_approximateGpuBytes;
    }
}

void CFBO::configureMipmaps (const uint32_t width, const uint32_t height) {
    const uint32_t previousCount = this->m_mipMapCount;
    const uint32_t exponent = std::bit_width (std::min (width, height) - 1);
    this->m_mipMapCount = this->hasMipmaps () && exponent > 3 ? exponent - 3 : 1;
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, this->m_mipMapCount - 1);
    glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAX_LOD, this->m_mipMapCount - 1);
    // Shrinking a mutable texture leaves old mip storage allocated unless it is
    // explicitly released. The new chain is populated after clearing level zero.
    for (uint32_t level = this->m_mipMapCount; level < previousCount; ++level) {
	glTexImage2D (
	    GL_TEXTURE_2D, level, colorInternalFormat (this->m_format), 0, 0, 0, GL_RGBA,
	    colorUploadType (this->m_format), nullptr
	);
    }
}

size_t CFBO::calculateStorageBytes (uint32_t width, uint32_t height) const {
    const size_t levelZeroPixels = static_cast<size_t> (width) * height;
    const size_t texturePixelBytes = this->m_depthTexture ? 4 : colorPixelBytes (this->m_format);
    size_t bytes = this->m_depthbuffer != GL_NONE
	? levelZeroPixels * (this->getDepthInternalFormat () == GL_DEPTH_COMPONENT16 ? 2 : 4) : 0;
    for (uint32_t level = 0; level < this->m_mipMapCount; ++level) {
	bytes += static_cast<size_t> (width) * height * texturePixelBytes;
	width = std::max (width / 2, 1u);
	height = std::max (height / 2, 1u);
    }
    if (this->m_samples > 1) {
	bytes += levelZeroPixels * colorPixelBytes (this->m_format) * this->m_samples;
	if (this->m_multisampleDepth != GL_NONE)
	    bytes += levelZeroPixels * 4 * this->m_samples;
    }
    return bytes;
}

void CFBO::clear (const glm::vec4& color) const { clearTextureStorage (color); }

void CFBO::clearTextureStorage (const glm::vec4& color) const {
    GLint previousDraw = GL_NONE, previousRead = GL_NONE;
    GLboolean colorMask[4], depthMask;
    glGetIntegerv (GL_DRAW_FRAMEBUFFER_BINDING, &previousDraw);
    glGetIntegerv (GL_READ_FRAMEBUFFER_BINDING, &previousRead);
    glGetBooleanv (GL_COLOR_WRITEMASK, colorMask);
    glGetBooleanv (GL_DEPTH_WRITEMASK, &depthMask);
    const GLboolean scissor = glIsEnabled (GL_SCISSOR_TEST);

    // Use a temporary container in the current context: construction can run on
    // the shared-context worker, while the permanent framebuffer stays lazy.
    GLuint framebuffer = GL_NONE;
    glGenFramebuffers (1, &framebuffer);
    glBindFramebuffer (GL_FRAMEBUFFER, framebuffer);
    if (this->m_depthTexture) {
	glFramebufferTexture2D (GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, this->m_texture, 0);
	glDrawBuffer (GL_NONE);
	glReadBuffer (GL_NONE);
    } else {
	glFramebufferTexture2D (GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, this->m_texture, 0);
    }
    if (this->m_depthbuffer != GL_NONE) {
	glFramebufferRenderbuffer (GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, this->m_depthbuffer);
    }
    glDisable (GL_SCISSOR_TEST);
    glColorMask (GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDepthMask (GL_TRUE);
    const GLfloat farDepth = getFarDepth ();
    if (!this->m_depthTexture) glClearBufferfv (GL_COLOR, 0, &color.x);
    if (this->m_depthTexture || this->m_depthbuffer != GL_NONE) glClearBufferfv (GL_DEPTH, 0, &farDepth);
    glColorMask (colorMask[0], colorMask[1], colorMask[2], colorMask[3]);
    glDepthMask (depthMask);
    if (scissor) glEnable (GL_SCISSOR_TEST);
    glBindFramebuffer (GL_DRAW_FRAMEBUFFER, previousDraw);
    glBindFramebuffer (GL_READ_FRAMEBUFFER, previousRead);
    glDeleteFramebuffers (1, &framebuffer);

    m_storageCleared = true;
    if (this->m_mipMapCount > 1) {
	GLint previousTexture;
	glGetIntegerv (GL_TEXTURE_BINDING_2D, &previousTexture);
	glBindTexture (GL_TEXTURE_2D, m_texture);
	glGenerateMipmap (GL_TEXTURE_2D);
	glBindTexture (GL_TEXTURE_2D, previousTexture);
    }
}

CFBO::~CFBO () {
    // free opengl texture and framebuffer
    glDeleteTextures (1, &this->m_texture);
    glDeleteFramebuffers (1, &this->m_framebuffer);
    glDeleteFramebuffers (1, &this->m_multisampleFramebuffer);

    if (this->m_depthbuffer != GL_NONE) {
	glDeleteRenderbuffers (1, &this->m_depthbuffer);
    }
    if (this->m_multisampleColor != GL_NONE) glDeleteRenderbuffers (1, &this->m_multisampleColor);
    if (this->m_multisampleDepth != GL_NONE) glDeleteRenderbuffers (1, &this->m_multisampleDepth);

    {
	std::lock_guard lock (s_liveFBOsMutex);
	s_liveFBOs.erase (this);
    }

    s_liveCount.fetch_sub (1, std::memory_order_relaxed);
    s_liveGpuBytes.fetch_sub (this->m_approximateGpuBytes, std::memory_order_relaxed);
}

std::string CFBO::getLiveDebugSummary (const size_t limit) {
    struct Group {
	std::string name;
	uint32_t width;
	uint32_t height;
	size_t count = 0;
	size_t bytes = 0;
    };

    std::map<std::tuple<std::string, uint32_t, uint32_t>, Group> grouped;
    {
	std::lock_guard lock (s_liveFBOsMutex);
	for (const auto& [instance, info] : s_liveFBOs) {
	    auto& group = grouped[std::make_tuple (info.name, info.width, info.height)];
	    group.name = info.name;
	    group.width = info.width;
	    group.height = info.height;
	    group.count++;
	    group.bytes += info.bytes;
	}
    }

    std::vector<Group> sorted;
    sorted.reserve (grouped.size ());
    for (auto& group : grouped | std::views::values) {
	sorted.push_back (std::move (group));
    }
    std::ranges::sort (sorted, [] (const Group& left, const Group& right) { return left.bytes > right.bytes; });

    std::ostringstream out;
    for (size_t index = 0; index < std::min (limit, sorted.size ()); index++) {
	if (index > 0) {
	    out << ';';
	}
	auto name = sorted[index].name;
	std::ranges::replace (name, ' ', '_');
	out << name << '@' << sorted[index].width << 'x' << sorted[index].height << 'x' << sorted[index].count << '='
	    << sorted[index].bytes;
    }
    return out.str ();
}

void CFBO::ensureFramebuffer () const {
    if (this->m_framebuffer != GL_NONE) {
	return;
    }

    // this can trigger mid-frame (e.g. from getTextureID while another FBO is bound),
    // so the caller's framebuffer bindings must be preserved
    GLint previousDraw = GL_NONE;
    GLint previousRead = GL_NONE;
    glGetIntegerv (GL_DRAW_FRAMEBUFFER_BINDING, &previousDraw);
    glGetIntegerv (GL_READ_FRAMEBUFFER_BINDING, &previousRead);

    glGenFramebuffers (1, &this->m_framebuffer);
    glBindFramebuffer (GL_FRAMEBUFFER, this->m_framebuffer);

    if (this->m_depthTexture) {
	glFramebufferTexture2D (GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, this->m_texture, 0);
	glDrawBuffer (GL_NONE);
	glReadBuffer (GL_NONE);
    } else {
	constexpr GLenum drawBuffers[1] = { GL_COLOR_ATTACHMENT0 };
	glFramebufferTexture2D (GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, this->m_texture, 0);
	glDrawBuffers (1, drawBuffers);
    }

    if (this->m_depthbuffer != GL_NONE) {
	glFramebufferRenderbuffer (GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, this->m_depthbuffer);
    }

    // ensure first framebuffer is okay
    if (glCheckFramebufferStatus (GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
	sLog.exception ("Framebuffers are not properly set");
    }

    if (this->m_depthTexture) {
	// Unrendered atlas texels represent the far plane and therefore compare as lit.
	this->clearTextureStorage ();
    } else if (!this->hasMipmaps () && !m_storageCleared) {
	// Layer framebuffers must start transparent. The scene clear color is often opaque,
	// and using it here makes empty layer areas render as solid rectangles.
	GLfloat previousClearColor[4] = {};
	glGetFloatv (GL_COLOR_CLEAR_VALUE, previousClearColor);
	glClearColor (0.0f, 0.0f, 0.0f, 0.0f);
	glClear (GL_COLOR_BUFFER_BIT);
	glClearColor (previousClearColor[0], previousClearColor[1], previousClearColor[2], previousClearColor[3]);
    }

    glBindFramebuffer (GL_DRAW_FRAMEBUFFER, static_cast<GLuint> (previousDraw));
    glBindFramebuffer (GL_READ_FRAMEBUFFER, static_cast<GLuint> (previousRead));
}

void CFBO::ensureMultisampleFramebuffer () const {
    if (this->m_samples == 1 || this->m_multisampleFramebuffer != GL_NONE) return;
    GLint previousDraw = GL_NONE, previousRead = GL_NONE;
    glGetIntegerv (GL_DRAW_FRAMEBUFFER_BINDING, &previousDraw);
    glGetIntegerv (GL_READ_FRAMEBUFFER_BINDING, &previousRead);
    glGenFramebuffers (1, &this->m_multisampleFramebuffer);
    glBindFramebuffer (GL_FRAMEBUFFER, this->m_multisampleFramebuffer);
    glFramebufferRenderbuffer (GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, this->m_multisampleColor);
    if (this->m_multisampleDepth != GL_NONE)
	glFramebufferRenderbuffer (GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, this->m_multisampleDepth);
    constexpr GLenum drawBuffers[] = { GL_COLOR_ATTACHMENT0 };
    glDrawBuffers (1, drawBuffers);
    if (glCheckFramebufferStatus (GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
	sLog.exception ("Multisample framebuffer is not properly set");

    GLboolean colorMask[4], depthMask;
    glGetBooleanv (GL_COLOR_WRITEMASK, colorMask);
    glGetBooleanv (GL_DEPTH_WRITEMASK, &depthMask);
    const GLboolean scissor = glIsEnabled (GL_SCISSOR_TEST);
    glDisable (GL_SCISSOR_TEST);
    glColorMask (GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDepthMask (GL_TRUE);
    const GLfloat zero[4] = {};
    const GLfloat farDepth = getFarDepth ();
    glClearBufferfv (GL_COLOR, 0, zero);
    if (this->m_multisampleDepth != GL_NONE) glClearBufferfv (GL_DEPTH, 0, &farDepth);
    glColorMask (colorMask[0], colorMask[1], colorMask[2], colorMask[3]);
    glDepthMask (depthMask);
    if (scissor) glEnable (GL_SCISSOR_TEST);
    glBindFramebuffer (GL_DRAW_FRAMEBUFFER, previousDraw);
    glBindFramebuffer (GL_READ_FRAMEBUFFER, previousRead);
}

void CFBO::resolveMultisample () const {
    if (this->m_samples == 1 || !this->m_multisampleRendering) return;
    this->ensureFramebuffer ();
    this->ensureMultisampleFramebuffer ();
    GLint previousDraw = GL_NONE, previousRead = GL_NONE;
    glGetIntegerv (GL_DRAW_FRAMEBUFFER_BINDING, &previousDraw);
    glGetIntegerv (GL_READ_FRAMEBUFFER_BINDING, &previousRead);
    const GLboolean scissor = glIsEnabled (GL_SCISSOR_TEST);
    glDisable (GL_SCISSOR_TEST);
    glBindFramebuffer (GL_READ_FRAMEBUFFER, this->m_multisampleFramebuffer);
    glBindFramebuffer (GL_DRAW_FRAMEBUFFER, this->m_framebuffer);
    glBlitFramebuffer (
	0, 0, this->m_resolution.x, this->m_resolution.y, 0, 0, this->m_resolution.x, this->m_resolution.y,
	GL_COLOR_BUFFER_BIT, GL_NEAREST
    );
    if (scissor) glEnable (GL_SCISSOR_TEST);
    glBindFramebuffer (GL_DRAW_FRAMEBUFFER, previousDraw);
    glBindFramebuffer (GL_READ_FRAMEBUFFER, previousRead);
}

bool CFBO::hasMipmaps () const {
    return !this->m_depthTexture && targetUsesMipmaps (this->m_name);
}

bool CFBO::targetUsesMipmaps (const std::string_view name) { return name == "_rt_MipMappedFrameBuffer"; }

void CFBO::generateMipmaps () const {
    if (!this->hasMipmaps ()) {
	return;
    }

    glBindTexture (GL_TEXTURE_2D, this->m_texture);
    glGenerateMipmap (GL_TEXTURE_2D);
    glBindTexture (GL_TEXTURE_2D, 0);
}

const std::string& CFBO::getName () const { return this->m_name; }

const float& CFBO::getScale () const { return this->m_scale; }

TextureFormat CFBO::getFormat () const { return this->m_format; }

uint32_t CFBO::getFlags () const { return this->m_flags; }

GLuint CFBO::getFramebuffer () const {
    this->ensureFramebuffer ();
    this->resolveMultisample ();
    return this->m_framebuffer;
}

GLuint CFBO::getDrawFramebuffer () const {
    if (this->m_multisampleRendering) {
	this->ensureMultisampleFramebuffer ();
	return this->m_multisampleFramebuffer;
    }
    this->ensureFramebuffer ();
    return this->m_framebuffer;
}

GLuint CFBO::getDepthbuffer () const {
    return this->m_samples > 1 ? this->m_multisampleDepth : this->m_depthbuffer;
}

uint32_t CFBO::getSamples () const { return this->m_samples; }

GLenum CFBO::getDepthInternalFormat () const {
    if (this->m_name == "_rt_Reflection" && this->m_samples == 1) return GL_DEPTH_COMPONENT16;
    return this->m_reverseDepth || this->m_samples > 1 ? GL_DEPTH_COMPONENT32F : GL_DEPTH_COMPONENT24;
}

void CFBO::setMultisampleRendering (const bool enabled) {
    if (!enabled) this->resolveMultisample ();
    this->m_multisampleRendering = enabled && this->m_samples > 1;
}

GLuint CFBO::getTextureID (uint32_t imageIndex) const {
    // ensure the FBO exists (and its initial transparent clear ran) before anything
    // samples from this texture, otherwise the first frame reads undefined memory
    this->ensureFramebuffer ();
    this->resolveMultisample ();
    return this->m_texture;
}

uint32_t CFBO::getTextureWidth (uint32_t imageIndex) const { return this->m_resolution.x; }

uint32_t CFBO::getTextureHeight (uint32_t imageIndex) const { return this->m_resolution.y; }

uint32_t CFBO::getMipMapCount (uint32_t imageIndex) const { return this->m_mipMapCount; }

uint32_t CFBO::getRealWidth () const { return this->m_resolution.z; }

uint32_t CFBO::getRealHeight () const { return this->m_resolution.w; }

const std::vector<FrameSharedPtr>& CFBO::getFrames () const { return this->m_frames; }

const glm::vec4* CFBO::getResolution () const { return &this->m_resolution; }

bool CFBO::isAnimated () const { return false; }

size_t CFBO::getApproximateGpuBytes () const { return this->m_approximateGpuBytes; }

uint32_t CFBO::getSpritesheetCols () const {
    return 0; // FBOs don't have spritesheets
}

uint32_t CFBO::getSpritesheetRows () const {
    return 0; // FBOs don't have spritesheets
}

uint32_t CFBO::getSpritesheetFrames () const {
    return 0; // FBOs don't have spritesheets
}

float CFBO::getSpritesheetDuration () const {
    return 0.0f; // FBOs don't have spritesheets
}

void CFBO::incrementUsageCount () const { }
void CFBO::decrementUsageCount () const { }
void CFBO::update () const { }
// FBOs are always ready
bool CFBO::isReady () const { return true; }

size_t CFBO::getLiveCount () { return s_liveCount.load (std::memory_order_relaxed); }

size_t CFBO::getLiveGpuBytes () { return s_liveGpuBytes.load (std::memory_order_relaxed); }
