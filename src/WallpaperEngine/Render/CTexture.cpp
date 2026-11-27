#include "CTexture.h"
#include "WallpaperEngine/Data/Utils/ScopeGuard.h"
#include "WallpaperEngine/Debug/RenderHealth.h"
#include "WallpaperEngine/Logging/Log.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <ranges>
#include <lz4.h>

#define STB_IMAGE_IMPLEMENTATION
#include "RenderContext.h"

#include <stb_image.h>

using namespace WallpaperEngine::Render;

CTexture::CTexture (
    RenderContext& context, TextureUniquePtr header, uint32_t textureReduction,
    Data::Parsers::TextureParser::VariantSelector selector
) : Helpers::ContextAware (context), m_header (std::move (header)), m_textureReduction (textureReduction),
    m_variantSelector (std::move (selector)) {
    if (this->m_variantSelector && this->m_header->variantSource) {
	this->m_variantSelection
	    = Data::Parsers::TextureParser::selectedVariants (*this->m_header, this->m_variantSelector);
	this->m_header = Data::Parsers::TextureParser::selectVariants (*this->m_header, this->m_variantSelector);
    }
    this->m_reduceMipmaps = this->m_textureReduction == 2
	&& !this->m_header->isVideoMp4 && !(this->m_header->flags & TextureFlags_Video);
    // Native source resolution uniforms remain authored. Animation UV normalization
    // separately accounts for rounded dimensions in the retained mip.
    if (this->m_header->isVolume ()) {
	// Complete CPU decoding before allocating GL objects. The encoded canvas is a
	// sequence of XY slices, not a tall 2D texture (native reader 14015c8d0).
	Data::Parsers::TextureParser::decodeMipmaps (*this->m_header);
	GLint maxSize = 0;
	glGetIntegerv (GL_MAX_3D_TEXTURE_SIZE, &maxSize);
	for (const auto& mipmaps : this->m_header->images | std::views::values) {
	    for (const auto& mipmap : mipmaps) {
		if (mipmap->width > static_cast<uint32_t> (maxSize)
		    || mipmap->height > static_cast<uint32_t> (maxSize) || mipmap->depth > static_cast<uint32_t> (maxSize)) {
		    sLog.exception ("Volume texture exceeds the graphics device's 3D texture limit");
		}
		if (this->m_header->freeImageFormat != FIF_UNKNOWN && !mipmap->decodedData) {
		    sLog.exception ("Cannot decode volume texture slices");
		}
	    }
	}
    }
    // ensure the header is parsed
    this->setupResolution ();
    const GLint internalFormat = this->setupInternalFormat ();

    if (this->m_header->isVideoMp4 || this->m_header->flags & TextureFlags_Video) {
	this->m_approximateGpuBytes
	    = static_cast<size_t> (this->m_header->textureWidth) * this->m_header->textureHeight * 4;
    } else {
	for (const auto& [index, mipmaps] : this->m_header->images) {
	    const size_t skipped = mipmaps.size () - this->getMipMapCount (index);
	    for (const auto& mipmap : mipmaps | std::views::drop (skipped)) {
		const size_t width = this->m_header->isVolume () ? mipmap->width
		    : mipmap->decodedWidth > 0 ? mipmap->decodedWidth : mipmap->width;
		const size_t height = this->m_header->isVolume () ? size_t (mipmap->height) * mipmap->depth
		    : mipmap->decodedHeight > 0 ? mipmap->decodedHeight : mipmap->height;

		if (this->m_header->freeImageFormat != FIF_UNKNOWN) {
		    this->m_approximateGpuBytes += width * height * 4;
		} else if (internalFormat == GL_R8) {
		    this->m_approximateGpuBytes += width * height;
		} else if (internalFormat == GL_RG8) {
		    this->m_approximateGpuBytes += width * height * 2;
		} else if (internalFormat == GL_RGBA8) {
		    this->m_approximateGpuBytes += width * height * 4;
		} else {
		    // Compressed uploads retain exactly the block payload supplied to GL.
		    this->m_approximateGpuBytes += static_cast<size_t> (mipmap->uncompressedSize);
		}
	    }
	}
    }

    // videos are a bit special, they only have one framebuffer, one mipmap
    if (this->m_header->isVideoMp4 || this->m_header->flags & TextureFlags_Video) {
	if (this->m_header->images.empty () || this->m_header->images.begin ()->second.empty ()) {
	    sLog.exception ("Cannot load video texture, no mipmaps found");
	}

	// generate the texture and set it up to be used by the player
	this->m_textureID = new GLuint[1];
	glGenTextures (1, this->m_textureID);
	this->setupOpenGLParameters (0);

	const auto mipmap = *this->m_header->images.begin ()->second.begin ();

	this->m_player = std::make_unique<GLPlayer> (
	    this->getContext (), this->m_textureID[0],
	    std::make_unique<MemoryStreamProtocol> (mipmap->uncompressedData.get (), mipmap->uncompressedSize),
	    this->m_header->textureWidth, this->m_header->textureHeight
	);
	// setup texture video player
	this->m_player->setMuted ();
	this->m_player->setVolume (0.0f);
	this->m_player->setNonBlocking ();
	// texture is ready, nothing else to do
	return;
    }

    // allocate texture ids list
    this->m_textureID = new GLuint[this->m_header->imageCount];
    // ask opengl for the correct amount of textures and framebuffers
    glGenTextures (this->m_header->imageCount, this->m_textureID);
    Data::Utils::ScopeGuard textureCleanup ([this] {
	glDeleteTextures (this->m_header->imageCount, this->m_textureID);
	delete[] this->m_textureID;
    });

    for (const auto& [index, mipmaps] : this->m_header->images) {
	this->setupOpenGLParameters (index);

	int level = 0;
	const size_t skipped = mipmaps.size () - this->getMipMapCount (index);
	if (skipped != 0) {
	    // Retain its dimensions for authored UVs, but discard the unused pixel buffers.
	    const auto& mipmap = mipmaps.front ();
	    mipmap->decodedData.reset ();
	    mipmap->uncompressedData.reset ();
	    mipmap->uncompressedSize = 0;
	}

	for (const auto& mipmap : mipmaps | std::views::drop (skipped)) {
	    DecodedPixelsPtr pixels;
	    const void* dataptr = mipmap->uncompressedData.get ();
	    int width = mipmap->width;
	    int height = mipmap->height;
	    const uint32_t bufferSize = mipmap->uncompressedSize;
	    GLenum textureFormat = GL_RGBA;

	    if (this->m_header->freeImageFormat != FIF_UNKNOWN) {
		if (mipmap->decodedData != nullptr) {
		    // pixels were pre-decoded off the render thread (TextureParser::decodeMipmaps),
		    // so the encoded image only needs to be uploaded
		    dataptr = mipmap->decodedData.get ();
		    width = mipmap->decodedWidth;
		    height = mipmap->decodedHeight;
		} else {
		    pixels = Data::Parsers::TextureParser::decodeCompiledImage (
			this->m_header->freeImageFormat, mipmap->uncompressedData.get (), mipmap->uncompressedSize,
			width, height
		    );
		    dataptr = pixels.get ();
		}
		if (dataptr == nullptr) {
		    sLog.exception ("Cannot decode texture image");
		}
	    } else {
		if (dataptr == nullptr) sLog.exception ("Cannot decode raw texture mip");
		// Texture payloads are tightly packed, with no per-row padding. GL defaults
		// GL_UNPACK_ALIGNMENT to 4, so for any format whose row is not a multiple of four
		// bytes the driver reads a padded row stride and runs off the end of the buffer.
		// RG88 at an odd width is the case that bites: 1181 * 2 = 2362 bytes per row, which
		// the driver rounds to 2364, overrunning a 1181x1168 payload by 2336 bytes and
		// faulting inside libnvidia-eglcore. Setting it once per upload also avoids relying
		// on the previous texture happening to have left it at 1.
		glPixelStorei (GL_UNPACK_ALIGNMENT, 1);

		if (this->m_header->format == TextureFormat_R8) {
		    textureFormat = GL_RED;
		} else if (this->m_header->format == TextureFormat_RG88) {
		    textureFormat = GL_RG;
		}
	    }

	    if (this->m_header->isVolume ()) {
		width = mipmap->width;
		height = mipmap->height;
	    }
	    switch (internalFormat) {
		case GL_RGBA8:
		case GL_RG8:
		case GL_R8:
		    if (this->m_header->isVolume ()) {
			glTexImage3D (GL_TEXTURE_3D, level, internalFormat, width, height, mipmap->depth, 0,
			    textureFormat, GL_UNSIGNED_BYTE, dataptr);
		    } else {
			glTexImage2D (
			    GL_TEXTURE_2D, level, internalFormat, width, height, 0, textureFormat, GL_UNSIGNED_BYTE, dataptr
			);
		    }
		    break;
		case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
		case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:
		case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
		    glCompressedTexImage2D (
			GL_TEXTURE_2D, level, internalFormat, width, height, 0, bufferSize, dataptr
		    );
		    break;
		default:
		    sLog.exception ("Cannot load texture, unknown format", this->m_header->format);
	    }

	    if (this->m_header->freeImageFormat != FIF_UNKNOWN) {
		// pre-decoded pixels are on the GPU now, don't keep them in RAM
		mipmap->decodedData.reset ();
		mipmap->decodedWidth = 0;
		mipmap->decodedHeight = 0;
	    }

	    // the raw file bytes are on the GPU now too; only the video path (which
	    // returns early above) keeps reading them after construction, so cached
	    // textures don't need to pin the CPU-side copy anymore
	    mipmap->uncompressedData.reset ();
	    mipmap->uncompressedSize = 0;

	    level++;
	}
    }
    textureCleanup.cancel ();
}

CTexture::~CTexture () {
    if (this->m_mipStream) this->m_mipStream->cancelled.store (true, std::memory_order_relaxed);
    // first release the player to prevent using null references
    this->m_player.reset ();

    if (this->m_header->isVideoMp4 || this->m_header->flags & TextureFlags_Video) {
	glDeleteTextures (1, this->m_textureID);
    } else {
	glDeleteTextures (this->m_header->imageCount, this->m_textureID);
    }

    delete[] this->m_textureID;
}

bool CTexture::isBlockCompressedPayload (
    const TextureFormat format, const uint32_t width, const uint32_t height, const size_t byteCount
) {
    size_t bytesPerBlock = 0;

    switch (format) {
	case TextureFormat_DXT1: bytesPerBlock = 8; break;
	case TextureFormat_DXT3:
	case TextureFormat_DXT5:
	case TextureFormat_BC7: bytesPerBlock = 16; break;
	default: return false;
    }

    const size_t blocksWide = std::max<size_t> (1, (static_cast<size_t> (width) + 3) / 4);
    const size_t blocksHigh = std::max<size_t> (1, (static_cast<size_t> (height) + 3) / 4);
    return byteCount == blocksWide * blocksHigh * bytesPerBlock;
}

void CTexture::setupResolution () {
    if (this->m_header->freeImageFormat != FIF_UNKNOWN) {
	// Encoded images upload their mip dimensions, including animated atlases
	// whose TEXI storage dimensions still describe power-of-two padding.
	const auto& mipmap = this->m_header->images.at (0).front ();
	this->m_resolution = { mipmap->width, mipmap->height, this->m_header->width, this->m_header->height };
    } else {
	// Shader resolution describes the atlas; getRealWidth/Height describe its animation canvas.
	this->m_resolution = { this->m_header->textureWidth, this->m_header->textureHeight, this->m_header->width,
			       this->m_header->height };
    }
}

GLint CTexture::setupInternalFormat () const {
    if (this->m_header->freeImageFormat != FIF_UNKNOWN) {
	return GL_RGBA8;
    }

    const auto firstImage = this->m_header->images.find (0);
    if (firstImage == this->m_header->images.end () || firstImage->second.empty ()) {
	sLog.exception ("Cannot determine texture format without mipmap data");
    }

    const auto& firstMipmap = *firstImage->second.front ();
    const bool blockCompressed = isBlockCompressedPayload (
	this->m_header->format, firstMipmap.width, firstMipmap.height,
	static_cast<size_t> (std::max (firstMipmap.uncompressedSize, 0))
    );

    const bool dxtAuthored = this->m_header->format == TextureFormat_DXT1
	|| this->m_header->format == TextureFormat_DXT3 || this->m_header->format == TextureFormat_DXT5;
    const size_t rawRgbaBytes
	= static_cast<size_t> (firstMipmap.width) * static_cast<size_t> (firstMipmap.height) * 4;
    if (dxtAuthored && !blockCompressed
	&& static_cast<size_t> (std::max (firstMipmap.uncompressedSize, 0)) != rawRgbaBytes) {
	sLog.exception (
	    "Unsupported DXT texture payload size: ", firstMipmap.uncompressedSize,
	    " for ", firstMipmap.width, "x", firstMipmap.height
	);
    }

    // detect the image format and hand it to openGL to be used
    switch (this->m_header->format) {
	case TextureFormat_DXT5:
	    return blockCompressed ? GL_COMPRESSED_RGBA_S3TC_DXT5_EXT : GL_RGBA8;
	case TextureFormat_DXT3:
	    return blockCompressed ? GL_COMPRESSED_RGBA_S3TC_DXT3_EXT : GL_RGBA8;
	case TextureFormat_DXT1:
	    return blockCompressed ? GL_COMPRESSED_RGBA_S3TC_DXT1_EXT : GL_RGBA8;
	case TextureFormat_ARGB8888:
	    return GL_RGBA8;
	case TextureFormat_R8:
	    return GL_R8;
	case TextureFormat_RG88:
	    return GL_RG8;
	default:
	    sLog.exception ("Cannot determine texture format");
    }
}

void CTexture::setupOpenGLParameters (const uint32_t textureID) const {
    const GLenum target = this->getTextureTarget ();
    // TODO: LABEL ELEMENTS TOO
    // bind the texture to assign information to it
    glBindTexture (target, this->m_textureID[textureID]);

    // set mipmap levels
    glTexParameteri (target, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri (target, GL_TEXTURE_MAX_LEVEL, this->getMipMapCount (textureID) - 1);

    // setup texture wrapping and filtering
    if (this->m_header->flags & TextureFlags_ClampUVs) {
	glTexParameteri (target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri (target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    } else {
	glTexParameteri (target, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri (target, GL_TEXTURE_WRAP_T, GL_REPEAT);
    }

    if (this->m_header->isVolume ()) {
	glTexParameteri (target, GL_TEXTURE_WRAP_R,
	    this->m_header->flags & TextureFlags_ClampUVs ? GL_CLAMP_TO_EDGE : GL_REPEAT);
    }

    if (this->m_header->flags & TextureFlags_NoInterpolation) {
	glTexParameteri (target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri (target, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
    } else {
	glTexParameteri (target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri (target, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    }

    glTexParameterf (target, GL_TEXTURE_MAX_ANISOTROPY,
	this->m_header->flags & TextureFlags_NoInterpolation ? 1.0f : this->m_header->anisotropy);
}

GLenum CTexture::getTextureTarget () const {
    return this->m_header->isVolume () ? GL_TEXTURE_3D : GL_TEXTURE_2D;
}

GLuint CTexture::getTextureID (const uint32_t imageIndex) const {
    if (m_userAnimation) {
	return m_textureID[m_header->frames[m_userAnimation->frame]->frameNumber];
    }
    // ensure we do not go out of bounds
    if (imageIndex >= this->m_header->imageCount) {
	return this->m_textureID[0];
    }

    return this->m_textureID[imageIndex];
}

void CTexture::startUserAnimation (const double sceneTime, const uint32_t sceneFrame) {
    if (!isAnimated () || m_header->frames.empty ()) return;
    m_userAnimation = UserAnimation { .update = sceneFrame, .frameTime = sceneTime };
    for (const auto& frame : m_header->frames) {
	if (frame->frametime <= 0.0f) {
	    // Zero-delay frames must be displayed for one update, even after a hitch.
	    m_userAnimation->duration = 0.0;
	    return;
	}
	m_userAnimation->duration += frame->frametime;
    }
}

void CTexture::updateUserAnimation (const double sceneTime, const uint32_t sceneFrame) const {
    if (!m_userAnimation || m_userAnimation->update == sceneFrame) return;
    auto& animation = *m_userAnimation;
    animation.update = sceneFrame;
    const auto& frames = m_header->frames;
    if (frames[animation.frame]->frametime <= 0.0f) {
	animation.frame = (animation.frame + 1) % frames.size ();
	animation.frameTime = sceneTime;
	return;
    }
    if (animation.duration > 0.0 && sceneTime > animation.frameTime) {
	// Bound catch-up work after long pauses without skipping a zero-delay frame.
	animation.frameTime += std::floor ((sceneTime - animation.frameTime) / animation.duration)
	    * animation.duration;
    }
    while (sceneTime - animation.frameTime >= frames[animation.frame]->frametime) {
	animation.frameTime += frames[animation.frame]->frametime;
	animation.frame = (animation.frame + 1) % frames.size ();
	if (frames[animation.frame]->frametime <= 0.0f) {
	    animation.frameTime = sceneTime;
	    break;
	}
    }
}

uint32_t CTexture::getTextureWidth (const uint32_t imageIndex) const {
    if (imageIndex >= this->m_header->imageCount) {
	return this->getHeader ().textureWidth;
    }

    return (*this->m_header->images[imageIndex].begin ())->width;
}

uint32_t CTexture::getTextureHeight (const uint32_t imageIndex) const {
    if (imageIndex >= this->m_header->imageCount) {
	return this->getHeader ().textureHeight;
    }

    return (*this->m_header->images[imageIndex].begin ())->height;
}

glm::vec2 CTexture::getAnimationUVSize (const uint32_t imageIndex) const {
    if (this->m_reduceMipmaps && imageIndex < this->m_header->imageCount) {
	const auto& mipmaps = this->m_header->images.at (imageIndex);
	if (mipmaps.size () > 1) {
	    // Native halves frame coordinates before dividing by the retained mip.
	    // An odd original extent is not exactly twice its next mip's extent.
	    return 2.0f * glm::vec2 (mipmaps[1]->width, mipmaps[1]->height);
	}
    }
    return TextureProvider::getAnimationUVSize (imageIndex);
}

uint32_t CTexture::getMipMapCount (const uint32_t imageIndex) const {
    if (this->m_streamingTemporary) return 1;
    if (this->m_player != nullptr) {
	return 1;
    }
    // Match getTextureID's fallback for a static texture bound alongside animation.
    const uint32_t index = imageIndex < this->m_header->imageCount ? imageIndex : 0;
    const uint32_t count = this->m_header->images.at (index).size ();
    return count - (this->m_reduceMipmaps && count > 1 ? 1 : 0);
}

uint32_t CTexture::getRealWidth () const {
    return this->isAnimated () ? this->getHeader ().gifWidth : this->getHeader ().width;
}

uint32_t CTexture::getRealHeight () const {
    return this->isAnimated () ? this->getHeader ().gifHeight : this->getHeader ().height;
}

TextureFormat CTexture::getFormat () const { return this->getHeader ().format; }

uint32_t CTexture::getFlags () const { return this->getHeader ().flags; }

const Texture& CTexture::getHeader () const { return *this->m_header; }

const glm::vec4* CTexture::getResolution () const { return &this->m_resolution; }

const std::vector<FrameSharedPtr>& CTexture::getFrames () const { return this->getHeader ().frames; }

bool CTexture::isAnimated () const { return this->getHeader ().isAnimated (); }

uint32_t CTexture::getSpritesheetCols () const { return this->getHeader ().spritesheetCols; }

uint32_t CTexture::getSpritesheetRows () const { return this->getHeader ().spritesheetRows; }

uint32_t CTexture::getSpritesheetFrames () const { return this->getHeader ().spritesheetFrames; }

float CTexture::getSpritesheetDuration () const { return this->getHeader ().spritesheetDuration; }

size_t CTexture::getRetainedCpuBytes () const {
    if (this->m_mipStream) return this->m_streamCpuBytes;
    // image textures free their CPU-side buffers after the GL upload in the constructor,
    // but the video path keeps the whole file pinned so mpv can stream from memory
    size_t bytes = this->m_header->variantSource ? this->m_header->variantSource->size () : 0;

    for (const auto& [index, mipmaps] : this->m_header->images) {
	for (const auto& mipmap : mipmaps) {
	    bytes += mipmap->uncompressedSize;
	}
    }

    return bytes;
}

size_t CTexture::getApproximateGpuBytes () const { return this->m_approximateGpuBytes; }

void CTexture::incrementUsageCount () const {
    if (this->m_player) {
	this->m_player->incrementUsageCount ();
    }
}

void CTexture::decrementUsageCount () const {
    if (this->m_player) {
	this->m_player->decrementUsageCount ();
    }
}

void CTexture::update () const {
    if (this->m_mipStream) const_cast<CTexture*> (this)->refreshMipStream ();
    // Providers are exposed as const to render passes; mutation stays on the GL
    // thread, just as it does for the video player below.
    if (this->m_variantSelector) {
	const_cast<CTexture*> (this)->refreshVariant ();
    }
    if (this->m_player) {
	this->m_player->render ();
    }
}

void CTexture::refreshVariant () {
    const auto selection = Data::Parsers::TextureParser::selectedVariants (*this->m_header, this->m_variantSelector);
    if (selection == this->m_variantSelection) {
	return;
    }
    this->m_variantSelection = selection;
    try {
	auto parsed = Data::Parsers::TextureParser::selectVariants (*this->m_header, this->m_variantSelector);
	CTexture replacement (this->getContext (), std::move (parsed), this->m_textureReduction);
	// A completed new selection owns the display; an older worker may finish but
	// can no longer publish its optional levels over this replacement.
	if (this->m_mipStream) this->m_mipStream->cancelled.store (true, std::memory_order_relaxed);
	this->m_mipStream.reset ();
	this->m_streamCpuBytes = 0;
	this->m_streamingTemporary = false;
	std::swap (this->m_header, replacement.m_header);
	std::swap (this->m_textureID, replacement.m_textureID);
	std::swap (this->m_resolution, replacement.m_resolution);
	std::swap (this->m_reduceMipmaps, replacement.m_reduceMipmaps);
	std::swap (this->m_approximateGpuBytes, replacement.m_approximateGpuBytes);
	Debug::RenderHealth::record ("texture.variant_update", std::to_string (selection.size ()));
    } catch (const std::exception& error) {
	// Preserve the last complete texture and retry when the property changes again.
	sLog.error ("Cannot select texture variant: ", error.what ());
    }
}

// CTextures are always ready to be rendered at all times
bool CTexture::isReady () const { return true; }

std::shared_ptr<CTexture> CTexture::createStreaming (
    RenderContext& context, TextureUniquePtr header, const uint32_t textureReduction,
    std::vector<std::shared_ptr<MipStream>>& streams,
    Data::Parsers::TextureParser::VariantSelector selector
) {
    if (!Data::Parsers::TextureParser::canStreamMipmaps (*header)) {
	return std::make_shared<CTexture> (context, std::move (header), textureReduction, std::move (selector));
    }
    const auto& mipmaps = header->images.at (0);
    const auto& required = mipmaps[std::min<size_t> (2, mipmaps.size () - 1)];
    const bool imageFormat = header->freeImageFormat != FIF_UNKNOWN;
    if (imageFormat ? !required->decodedData : !required->uncompressedData)
	sLog.exception ("Cannot decode required texture mip");

    auto temporary = std::make_unique<Texture> (*header);
    const auto& first = mipmaps.front ();
    if (!imageFormat && (header->format == TextureFormat_DXT1 || header->format == TextureFormat_DXT3
	    || header->format == TextureFormat_DXT5)
	&& size_t (first->uncompressedSize) == size_t (first->width) * first->height * 4
	&& !isBlockCompressedPayload (header->format, first->width, first->height, first->uncompressedSize)) {
	// Tiny raw RGBA mips can have the same byte count as one DXT block.
	// Preserve the complete chain's format decision for its temporary level.
	temporary->format = TextureFormat_ARGB8888;
    }
    auto mipmap = std::make_shared<Mipmap> ();
    mipmap->width = required->width;
    mipmap->height = required->height;
    mipmap->decodedWidth = required->decodedWidth;
    mipmap->decodedHeight = required->decodedHeight;
    if (imageFormat) {
	if (header->variantSelectionPrepared) {
	    // The required mip already includes selected patches. Keep those pixels
	    // in the worker-owned full chain instead of decoding its unpatched base again.
	    const size_t size = size_t (required->decodedWidth) * required->decodedHeight * 4;
	    mipmap->decodedData.reset (static_cast<unsigned char*> (std::malloc (size)));
	    if (!mipmap->decodedData) throw std::bad_alloc ();
	    std::memcpy (mipmap->decodedData.get (), required->decodedData.get (), size);
	} else {
	    mipmap->decodedData = std::move (required->decodedData);
	}
    } else {
	// The complete raw chain still needs this level; copy only the small required mip.
	mipmap->uncompressedSize = required->uncompressedSize;
	mipmap->uncompressedData = std::make_unique<char[]> (required->uncompressedSize);
	std::memcpy (mipmap->uncompressedData.get (), required->uncompressedData.get (), required->uncompressedSize);
    }
    temporary->images[0] = { mipmap };
    auto texture = std::make_shared<CTexture> (context, std::move (temporary), textureReduction);

    // Rendering keeps authored dimensions, while the worker owns all mutable pixel data.
    auto metadata = std::make_unique<Texture> (*header);
    size_t bytes = header->variantSource ? header->variantSource->size () : 0;
    for (auto& entry : metadata->images.at (0)) {
	bytes += size_t (entry->uncompressedSize) + (entry->compressedData ? entry->compressedSize : 0)
	    + (imageFormat ? size_t (entry->width) * entry->height * 4 : 0);
	for (const auto& patch : entry->variantPatches) bytes += patch.payload.size ();
	auto dimensions = std::make_shared<Mipmap> ();
	dimensions->width = entry->width;
	dimensions->height = entry->height;
	entry = std::move (dimensions);
    }
    texture->m_header = std::move (metadata);
    texture->m_variantSelector = std::move (selector);
    if (texture->m_variantSelector)
	texture->m_variantSelection = Data::Parsers::TextureParser::selectedVariants (*texture->m_header, texture->m_variantSelector);
    texture->setupResolution ();
    texture->m_streamingTemporary = true;
    texture->m_streamCpuBytes = bytes;
    texture->m_mipStream = std::make_shared<MipStream> ();
    texture->m_mipStream->texture = std::move (header);
    streams.push_back (texture->m_mipStream);
    Debug::RenderHealth::record ("texture.stream_temporary", std::to_string (required->width));
    return texture;
}

void CTexture::completeMipStreams (const std::vector<std::shared_ptr<MipStream>>& streams) {
    try {
	std::vector<Texture*> textures;
	for (const auto& stream : streams) {
	    if (!stream->cancelled.load (std::memory_order_relaxed)) textures.push_back (stream->texture.get ());
	}
	Data::Parsers::TextureParser::decodeMipmaps (textures);
    } catch (const std::exception& error) {
	// Decode-pool setup can fail too. Publish after its workers have joined so
	// incomplete mip chains reach the same fallback as a failed image decode.
	sLog.error ("Cannot decode texture stream: ", error.what ());
    }
    for (const auto& stream : streams) stream->ready.store (true, std::memory_order_release);
}

void CTexture::refreshMipStream () {
    if (!this->m_mipStream->ready.load (std::memory_order_acquire)) return;
    auto header = std::move (this->m_mipStream->texture);
    const auto& mipmaps = header->images.at (0);
    const size_t first = this->m_reduceMipmaps && mipmaps.size () > 1 ? 1 : 0;
    const bool imageFormat = header->freeImageFormat != FIF_UNKNOWN;
    const bool failed = std::ranges::any_of (mipmaps | std::views::drop (first),
	[imageFormat] (const auto& mipmap) { return imageFormat ? !mipmap->decodedData : !mipmap->uncompressedData; });
    if (failed) {
	// Native 1400eef70 publishes a 6x6 yellow/black error texture on a failed result.
	auto source = std::move (header);
	header = std::make_unique<Texture> ();
	header->variants = std::move (source->variants);
	header->variantSource = std::move (source->variantSource);
	header->spritesheetCols = source->spritesheetCols;
	header->spritesheetRows = source->spritesheetRows;
	header->spritesheetFrames = source->spritesheetFrames;
	header->spritesheetDuration = source->spritesheetDuration;
	header->imageCount = 1;
	header->width = header->height = header->textureWidth = header->textureHeight = 6;
	header->format = TextureFormat_ARGB8888;
	header->flags = TextureFlags_NoInterpolation;
	auto mipmap = std::make_shared<Mipmap> ();
	mipmap->width = mipmap->height = 6;
	mipmap->uncompressedSize = 6 * 6 * 4;
	mipmap->uncompressedData = std::make_unique<char[]> (mipmap->uncompressedSize);
	for (size_t pixel = 0; pixel < 36; ++pixel) {
	    const char value = (pixel / 6 + pixel) % 2 ? char (255) : 0;
	    auto* rgba = mipmap->uncompressedData.get () + pixel * 4;
	    rgba[0] = value; rgba[1] = value; rgba[2] = 0; rgba[3] = char (255);
	}
	header->images[0] = { mipmap };
    }
    // Construction owns cleanup on failure; only replace the displayed object after upload.
    try {
	CTexture replacement (this->getContext (), std::move (header), this->m_textureReduction);
	std::swap (this->m_header, replacement.m_header);
	std::swap (this->m_textureID, replacement.m_textureID);
	std::swap (this->m_resolution, replacement.m_resolution);
	std::swap (this->m_reduceMipmaps, replacement.m_reduceMipmaps);
	std::swap (this->m_approximateGpuBytes, replacement.m_approximateGpuBytes);
	this->m_streamingTemporary = false;
	Debug::RenderHealth::record (failed ? "texture.stream_failed" : "texture.stream_complete", "");
    } catch (const std::exception& error) {
	sLog.error ("Cannot complete texture stream: ", error.what ());
    }
    this->m_mipStream.reset ();
    this->m_streamCpuBytes = 0;
}
