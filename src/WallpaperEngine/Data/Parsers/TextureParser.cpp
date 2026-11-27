#include "WallpaperEngine/Assets/AssetLoadException.h"
#include "WallpaperEngine/Data/Model/Property.h"
#include "WallpaperEngine/Data/Utils/ScopeGuard.h"
#include <atomic>
#include <cerrno>
#include <cmath>
#include <cstring>
#include <limits>
#include <set>
#include <sstream>
#include <thread>

#include <lz4.h>
#include <stb_image.h>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>
}

#include "TextureParser.h"
#include "TextureJpegDecoder.h"
#include "WallpaperEngine/Data/Assets/Texture.h"
#include "WallpaperEngine/Logging/Log.h"

using namespace WallpaperEngine::Data::Assets;
using namespace WallpaperEngine::Data::Parsers;

void WallpaperEngine::Data::Assets::freeDecodedPixels (void* pixels) {
    stbi_image_free (pixels);
}

DecodedPixelsPtr TextureParser::decodeCompiledImage (FIF format, const char* data, int size, int& width, int& height) {
    if (!data || size <= 0) return nullptr;
    const auto* bytes = reinterpret_cast<const unsigned char*> (data);
    if (format == FIF_JPEG) {
	// Wallpaper Engine's accurate JPEG decode uses IJG's RGB reconstruction.
	// Both the private decoder and the bundled stb use malloc-owned pixels.
	if (DecodedPixelsPtr pixels { lwe_decode_tex_jpeg (bytes, size, &width, &height) }; pixels) {
	    return pixels;
	}
    }
    return DecodedPixelsPtr (stbi_load_from_memory (bytes, size, &width, &height, nullptr, 4));
}

namespace {
struct GifImage {
    std::unique_ptr<unsigned char[]> pixels;
    float frametime;
};

std::vector<unsigned char> gifDecoderInput (
    const std::string& contents, size_t maxFrames, std::vector<int>& frameDelays
) {
    const auto* bytes = reinterpret_cast<const unsigned char*> (contents.data ());
    const size_t paletteSize = size_t (2) << (bytes[10] & 7);
    const size_t paletteBytes = (bytes[10] & 0x80) ? paletteSize * 3 : 0;
    const size_t background = bytes[11];
    if (paletteBytes > contents.size () - 13) throw std::runtime_error ("Truncated GIF palette");
    const bool normalizeBackground = paletteBytes != 0 && background < paletteSize
	&& (background == 0 || bytes[13 + background * 3] != bytes[15 + background * 3]);

    // Native uses BGR for the logical background, but RGB for drawn palette pixels.
    // Give global-palette frames their original palette locally before changing only
    // the decoder's background. A zero background index behaves like no global table.
    std::vector<size_t> globalFrames;
    size_t cursor = 13 + paletteBytes;
    size_t frames = 0;
    int delay = -1;
    const auto skip = [&] (size_t count) {
	if (count > contents.size () - cursor) throw std::runtime_error ("Truncated GIF block");
	cursor += count;
    };
    const auto skipBlocks = [&] {
	for (;;) {
	    skip (1);
	    const size_t count = bytes[cursor - 1];
	    if (count == 0) break;
	    skip (count);
	}
    };
    while (cursor < contents.size ()) {
	const unsigned char tag = bytes[cursor++];
	if (tag == 0x3b) break;
	if (tag == 0x21) {
	    skip (1); // Extension label, then length-prefixed subblocks.
	    if (bytes[cursor - 1] == 0xf9) {
		const size_t control = cursor;
		skip (5);
		if (bytes[control] != 4) throw std::runtime_error ("Invalid GIF control block");
		// FFmpeg's GIF parser replaces zero delays with 100 ms. Keep the
		// authored delay separately so the renderer can advance zero per update.
		delay = bytes[control + 2] | (bytes[control + 3] << 8);
	    }
	    skipBlocks ();
	} else if (tag == 0x2c) {
	    if (++frames > maxFrames) throw std::runtime_error ("GIF animation has too many frames");
	    frameDelays.push_back (delay);
	    delay = -1;
	    skip (9);
	    const size_t flags = cursor - 1;
	    if (bytes[flags] & 0x80) skip ((size_t (2) << (bytes[flags] & 7)) * 3);
	    else globalFrames.push_back (flags);
	    skip (1); // LZW minimum code size; pixel data remains untouched.
	    skipBlocks ();
	} else {
	    throw std::runtime_error ("Invalid GIF block");
	}
    }
    if (!normalizeBackground) return { contents.begin (), contents.end () };
    constexpr size_t maxInputBytes = std::numeric_limits<int>::max () - AV_INPUT_BUFFER_PADDING_SIZE;
    const size_t baseSize = contents.size () - (background == 0 ? paletteBytes : 0);
    if (globalFrames.size () > (maxInputBytes - baseSize) / paletteBytes) {
	throw std::runtime_error ("Source image is too large");
    }
    std::vector<unsigned char> input;
    input.reserve (baseSize + globalFrames.size () * paletteBytes + AV_INPUT_BUFFER_PADDING_SIZE);
    input.insert (input.end (), contents.begin (), contents.begin () + 13);
    if (background == 0) {
	input[10] &= 0x7f;
    } else {
	input.insert (input.end (), contents.begin () + 13, contents.begin () + 13 + paletteBytes);
	std::swap (input[13 + background * 3], input[15 + background * 3]);
    }
    size_t copied = 13 + paletteBytes;
    for (const size_t flags : globalFrames) {
	input.insert (input.end (), contents.begin () + copied, contents.begin () + flags);
	input.push_back ((bytes[flags] & 0x78) | 0x80 | (bytes[10] & 7));
	input.insert (input.end (), contents.begin () + 13, contents.begin () + 13 + paletteBytes);
	copied = flags + 1;
    }
    input.insert (input.end (), contents.begin () + copied, contents.end ());
    return input;
}

std::vector<GifImage> decodeGifImages (const std::string& contents, int& width, int& height) {
    // Match the packed texture payload/frame limits before allocating decoded canvases.
    constexpr size_t maxDecodedBytes = 1u << 30;
    constexpr size_t maxFrames = 16384;
    if (contents.size () < 13) {
	throw std::runtime_error ("Cannot decode GIF header");
    }
    const auto* header = reinterpret_cast<const unsigned char*> (contents.data ());
    width = header[6] | (header[7] << 8);
    height = header[8] | (header[9] << 8);
    const size_t frameBytes = static_cast<size_t> (width) * height * 4;
    if (frameBytes == 0 || frameBytes > maxDecodedBytes) {
	throw std::runtime_error ("Invalid or oversized GIF canvas");
    }

    const AVCodec* decoder = avcodec_find_decoder (AV_CODEC_ID_GIF);
    if (!decoder) {
	throw std::runtime_error ("GIF decoder is unavailable");
    }
    AVCodecContext* context = avcodec_alloc_context3 (decoder);
    AVCodecParserContext* parser = av_parser_init (AV_CODEC_ID_GIF);
    AVPacket* packet = av_packet_alloc ();
    AVFrame* frame = av_frame_alloc ();
    SwsContext* converter = nullptr;
    const WallpaperEngine::Data::Utils::ScopeGuard cleanup ([&] {
	sws_freeContext (converter);
	av_frame_free (&frame);
	av_packet_free (&packet);
	if (parser) {
	    av_parser_close (parser);
	}
	avcodec_free_context (&context);
    });
    if (!context || !parser || !packet || !frame) {
	throw std::runtime_error ("Cannot allocate GIF decoder");
    }
    context->max_pixels = maxDecodedBytes / 4;
    context->time_base = context->pkt_timebase = AVRational { 1, 100 };
    context->err_recognition = AV_EF_EXPLODE;
    if (av_opt_set_int (context->priv_data, "trans_color", 0, 0) < 0 || avcodec_open2 (context, decoder, nullptr) < 0) {
	throw std::runtime_error ("Cannot initialize GIF decoder");
    }

    std::vector<int> frameDelays;
    std::vector<unsigned char> input = gifDecoderInput (contents, maxFrames, frameDelays);
    const int inputSize = static_cast<int> (input.size ());
    input.resize (input.size () + AV_INPUT_BUFFER_PADDING_SIZE, 0);
    int position = 0;
    std::vector<GifImage> images;
    size_t packets = 0;
    for (;;) {
	unsigned char* packetData = nullptr;
	int packetSize = 0;
	const int consumed = av_parser_parse2 (
	    parser, context, &packetData, &packetSize, position < inputSize ? input.data () + position : nullptr,
	    inputSize - position, AV_NOPTS_VALUE, AV_NOPTS_VALUE, position
	);
	if (consumed < 0 || consumed > inputSize - position) {
	    throw std::runtime_error ("Cannot parse GIF frame");
	}
	position += consumed;
	if (packetSize == 0) {
	    if (consumed != 0) {
		continue;
	    }
	    if (position != inputSize) {
		throw std::runtime_error ("GIF parser made no progress");
	    }
	    break;
	}
	if (++packets > maxFrames) {
	    throw std::runtime_error ("GIF animation has too many frames");
	}
	// The parser can point into the next frame's bytes; each decoder packet needs zero padding.
	av_packet_unref (packet);
	if (av_new_packet (packet, packetSize) < 0) {
	    throw std::runtime_error ("Cannot allocate GIF frame packet");
	}
	std::memcpy (packet->data, packetData, packetSize);
	packet->duration = parser->duration;
	if (avcodec_send_packet (context, packet) < 0) {
	    throw std::runtime_error ("Cannot decode GIF frame");
	}
	for (;;) {
	    const int status = avcodec_receive_frame (context, frame);
	    if (status == AVERROR (EAGAIN)) {
		break;
	    }
	    if (status < 0 || frame->width != width || frame->height != height) {
		throw std::runtime_error ("Cannot decode GIF canvas");
	    }
	    if (images.size () >= maxFrames || images.size () >= maxDecodedBytes / frameBytes) {
		throw std::runtime_error ("GIF animation is too large");
	    }
	    converter = sws_getCachedContext (
		converter, width, height, static_cast<AVPixelFormat> (frame->format), width, height, AV_PIX_FMT_RGBA,
		SWS_POINT, nullptr, nullptr, nullptr
	    );
	    if (!converter) {
		throw std::runtime_error ("Cannot convert GIF frame");
	    }
	    if (images.size () >= frameDelays.size ()) throw std::runtime_error ("Unexpected GIF frame");
	    const auto delay = frameDelays[images.size ()];
	    GifImage image { std::make_unique<unsigned char[]> (frameBytes),
			     static_cast<float> (delay >= 0 ? delay : packet->duration > 0 ? packet->duration : 10) / 100.0f };
	    unsigned char* planes[4] = { image.pixels.get (), nullptr, nullptr, nullptr };
	    const int strides[4] = { width * 4, 0, 0, 0 };
	    if (sws_scale (converter, frame->data, frame->linesize, 0, height, planes, strides) != height) {
		throw std::runtime_error ("Cannot convert GIF canvas");
	    }
	    images.push_back (std::move (image));
	}
    }
    if (images.empty ()) {
	throw std::runtime_error ("GIF contains no decoded images");
    }
    return images;
}

size_t remainingBytes (const BinaryReader& file) {
    auto& input = file.base ();
    const auto position = input.tellg ();
    input.seekg (0, std::ios::end);
    const auto end = input.tellg ();
    input.seekg (static_cast<std::streamoff> (position), std::ios::beg);
    return static_cast<size_t> (end - position);
}

void checkPayloadSize (const BinaryReader& file, int stored, int unpacked) {
    if (stored <= 0 || unpacked <= 0 || unpacked > (1 << 30) || static_cast<size_t> (stored) > remainingBytes (file)) {
	throw std::runtime_error ("Invalid or truncated texture payload");
    }
}

void decodeMipmap (Mipmap& mipmap, const FIF format, const bool toleratePatchErrors = true) {
    const bool imageFormat = format != FIF_UNKNOWN;
    const auto decodeBase = [&] {
	if (mipmap.compressedData && !mipmap.uncompressedData) {
	// Publish only complete output. Optional LZ4/allocation failure leaves this
	// record incomplete for the render-thread error resource, never a null upload.
	try {
	    auto unpacked = std::make_unique<char[]> (mipmap.uncompressedSize);
	    if (LZ4_decompress_safe (mipmap.compressedData.get (), unpacked.get (),
		    mipmap.compressedSize, mipmap.uncompressedSize) != mipmap.uncompressedSize) return;
	    mipmap.uncompressedData = std::move (unpacked);
	    mipmap.compressedData.reset ();
	} catch (const std::bad_alloc&) {
	    return;
	}
	}
	if (!imageFormat || !mipmap.uncompressedData || mipmap.decodedData) return;
	int width = 0, height = 0;
	auto pixels = TextureParser::decodeCompiledImage (
	    format, mipmap.uncompressedData.get (), mipmap.uncompressedSize, width, height);

	// on failure leave the mipmap untouched, the render thread will retry and report
	if (pixels == nullptr) {
	return;
	}

	mipmap.decodedData = std::move (pixels);
	mipmap.decodedWidth = width;
	mipmap.decodedHeight = height;
    };
    decodeBase ();
    if (mipmap.variantPatches.empty ()) return;
    // Move work off the record while applying it: replacement images can decode
    // their base without recursively applying the same patch list.
    auto patches = std::move (mipmap.variantPatches);
    try {
	for (const auto& patch : patches) {
	    const auto& payload = patch.payload;
	    const auto x = patch.x, y = patch.y, width = patch.width, height = patch.height;
	    const auto size = static_cast<int> (payload.size ());
	    if (patch.flags & 2) {
		// A replacement payload discards the base and earlier patches.
		mipmap.decodedData.reset ();
		mipmap.uncompressedSize = size;
		mipmap.uncompressedData = std::make_unique<char[]> (size);
		std::memcpy (mipmap.uncompressedData.get (), payload.data (), size);
		continue;
	    }

	    const bool blocks = !imageFormat
		&& (mipmap.variantFormat == TextureFormat_DXT1 || mipmap.variantFormat == TextureFormat_DXT3
		    || mipmap.variantFormat == TextureFormat_DXT5 || mipmap.variantFormat == TextureFormat_BC7)
		&& static_cast<size_t> (mipmap.uncompressedSize)
		    != static_cast<size_t> (mipmap.width) * mipmap.height * 4;
	    const size_t unit = blocks                ? (mipmap.variantFormat == TextureFormat_DXT1 ? 8 : 16)
		: imageFormat                         ? 4
		: mipmap.variantFormat == TextureFormat_R8   ? 1
		: mipmap.variantFormat == TextureFormat_RG88 ? 2
						      : 4;
	    const size_t columns = blocks ? (width + 3) / 4 : width;
	    const size_t rows = blocks ? (height + 3) / 4 : height;
	    const size_t stride = (blocks ? (mipmap.width + 3) / 4 : mipmap.width) * unit;
	    const size_t patchSize = columns * rows * unit;
	    if (patchSize > (1 << 30) || (blocks && (x % 4 != 0 || y % 4 != 0))) {
		throw std::runtime_error ("Invalid texture variant block layout");
	    }
	    std::vector<char> unpacked;
	    DecodedPixelsPtr decoded;
	    const char* source = payload.data ();
	    size_t sourceSize = payload.size ();
	    if (mipmap.compression == 1) {
		unpacked.resize (patchSize);
		const int bytes
		    = LZ4_decompress_safe (payload.data (), unpacked.data (), size, static_cast<int> (patchSize));
		if (bytes != static_cast<int> (patchSize)) {
		    throw std::runtime_error ("Cannot decompress texture variant data");
		}
		source = unpacked.data ();
		sourceSize = unpacked.size ();
	    } else if (imageFormat) {
		int decodedWidth = 0, decodedHeight = 0, channels = 0;
		decoded.reset (stbi_load_from_memory (
		    reinterpret_cast<const unsigned char*> (source), size, &decodedWidth, &decodedHeight, &channels, 4
		));
		if (!decoded || decodedWidth != static_cast<int> (width)
		    || decodedHeight != static_cast<int> (height)) {
		    throw std::runtime_error ("Invalid texture variant image");
		}
		source = reinterpret_cast<const char*> (decoded.get ());
		sourceSize = patchSize;
	    }
	    char* target = mipmap.uncompressedData.get ();
	    size_t targetSize = mipmap.uncompressedSize;
	    if (imageFormat) {
		if (!mipmap.decodedData) {
		    decodeBase ();
		}
		if (!mipmap.decodedData || mipmap.decodedWidth != static_cast<int> (mipmap.width)
		    || mipmap.decodedHeight != static_cast<int> (mipmap.height)) {
		    throw std::runtime_error ("Invalid base image for texture variant");
		}
		target = reinterpret_cast<char*> (mipmap.decodedData.get ());
		targetSize = static_cast<size_t> (mipmap.width) * mipmap.height * 4;
	    }
	    const size_t offset = (blocks ? y / 4 : y) * stride + (blocks ? x / 4 : x) * unit;
	    if (sourceSize != patchSize || offset + (rows - 1) * stride + columns * unit > targetSize) {
		throw std::runtime_error ("Texture variant payload size does not match its rectangle");
	    }
	    for (size_t row = 0; row < rows; ++row) {
		auto* destination = reinterpret_cast<unsigned char*> (target + offset + row * stride);
		const auto* pixels = reinterpret_cast<const unsigned char*> (source + row * columns * unit);
		if (patch.flags & 1) {
		    if (unit != 4 || blocks) {
			throw std::runtime_error ("Texture variant blending requires RGBA pixels");
		    }
		    for (size_t pixel = 0; pixel < columns; ++pixel) {
			const int alpha = pixels[pixel * 4 + 3];
			for (size_t channel = 0; channel < 3; ++channel) {
			    const size_t at = pixel * 4 + channel;
			    destination[at] = destination[at] + ((pixels[at] - destination[at]) * alpha >> 8);
			}
			destination[pixel * 4 + 3] = std::max (destination[pixel * 4 + 3], pixels[pixel * 4 + 3]);
		    }
		} else {
		    std::memcpy (destination, pixels, columns * unit);
		}
	    }
	}
	decodeBase ();
    } catch (const std::exception&) {
	if (!toleratePatchErrors) throw;
	// Optional failure must leave an incomplete record for the error resource.
	mipmap.decodedData.reset ();
	mipmap.variantPatches = std::move (patches);
    }
}

/** Every mipmap of the texture that still has to be decoded, appended to out */
void collectDecodable (Texture& texture, std::vector<std::pair<Mipmap*, FIF>>& out) {
    // Videos are fed to the player as-is. Raw textures may still own packed LZ4.
    if (texture.isVideoMp4 || texture.flags & TextureFlags_Video) {
	return;
    }

    for (auto& [index, mipmaps] : texture.images) {
	for (const auto& mipmap : mipmaps) {
	    const bool imageFormat = texture.freeImageFormat != FIF_UNKNOWN;
	    if (mipmap->compressedData || (imageFormat && !mipmap->decodedData && mipmap->uncompressedData)) {
		out.emplace_back (mipmap.get (), texture.freeImageFormat);
	    }
	}
    }
}
} // namespace

void TextureParser::decodeMipmaps (Texture& texture) {
    std::vector<std::pair<Mipmap*, FIF>> pending;
    collectDecodable (texture, pending);

    for (const auto& [mipmap, format] : pending) {
	decodeMipmap (*mipmap, format);
    }
}

bool TextureParser::canStreamMipmaps (const Texture& texture) {
    return texture.containerVersion != ContainerVersion_UNKNOWN
	&& !texture.isVideoMp4 && !(texture.flags & TextureFlags_Video) && !texture.isAnimated ()
	&& !texture.isVolume () && texture.imageCount == 1
	&& ((texture.variants.empty () && !texture.variantSource)
	    || (texture.variantSelectionPrepared && texture.freeImageFormat != FIF_UNKNOWN
		&& std::ranges::none_of (texture.variants, [] (const auto& variant) { return variant.flags & 2; })))
	&& texture.images.contains (0) && texture.images.at (0).size () > 1
	// Native reader 14015c8d0 queues encoded/LZ4 work; plain raw chains are immediate.
	// Keep mixed raw chains eager until their partial-result installation is established.
	&& (texture.freeImageFormat != FIF_UNKNOWN || std::ranges::all_of (texture.images.at (0),
	    [] (const auto& mipmap) { return mipmap->compression == 1; }));
}

void TextureParser::decodeMipmaps (const std::vector<Texture*>& textures, const bool initialMipOnly) {
    std::vector<std::pair<Mipmap*, FIF>> pending;
    for (Texture* texture : textures) {
	if (initialMipOnly && canStreamMipmaps (*texture)) {
	    const auto& mipmaps = texture->images.at (0);
	    const auto& mipmap = mipmaps[std::min<size_t> (2, mipmaps.size () - 1)];
	    const bool imageFormat = texture->freeImageFormat != FIF_UNKNOWN;
	    if (mipmap->compressedData || (imageFormat && !mipmap->decodedData && mipmap->uncompressedData))
		pending.emplace_back (mipmap.get (), texture->freeImageFormat);
	} else {
	    collectDecodable (*texture, pending);
	}
    }

    if (pending.empty ()) {
	return;
    }

    std::atomic<size_t> next = 0;
    const auto worker = [&next, &pending] {
	for (size_t index = next++; index < pending.size (); index = next++) {
	    decodeMipmap (*pending[index].first, pending[index].second);
	}
    };

    const auto threads =
	std::min<size_t> (pending.size (), std::max (1u, std::thread::hardware_concurrency ()));
    std::vector<std::jthread> pool;
    pool.reserve (threads - 1);

    for (size_t thread = 1; thread < threads; thread++) {
	pool.emplace_back (worker);
    }

    // this thread takes work too instead of waiting on the pool
    worker ();
}

TextureUniquePtr TextureParser::parse (const BinaryReader& file) { return parse (file, VariantSelector {}); }

TextureUniquePtr TextureParser::parse (const BinaryReader& file, const VariantSelector& selector, const bool deferMipmaps) {
    // BinaryReader is also used by older parsers. Require complete reads here so a
    // truncated patch cannot turn an unchecked integer/string read into an allocation.
    auto& input = file.base ();
    const auto exceptions = input.exceptions ();
    input.exceptions (std::ios::failbit | std::ios::badbit);
    const ScopeGuard restoreExceptions ([&] { input.exceptions (exceptions); });
    auto result = std::make_unique<Texture> ();

    parseTextureHeader (*result, file);
    const auto selected = selectedVariants (*result, selector);
    if (result->imageCount == 0 || result->imageCount > 16384) {
	throw std::runtime_error ("Invalid texture image count");
    }

    // a rejected mipmap leaves the read position mid-payload, so nothing after it can be trusted
    bool truncated = false;

    for (uint32_t image = 0; image < result->imageCount && !truncated; image++) {
	const uint32_t mipmapCount = file.nextUInt32 ();
	if (mipmapCount == 0 || mipmapCount > 32) {
	    throw std::runtime_error ("Invalid texture mip count");
	}
	MipmapList mipmaps;
	const bool deferRaw = deferMipmaps && result->freeImageFormat == FIF_UNKNOWN
	    && !result->isVideoMp4 && !(result->flags & TextureFlags_Video) && !result->isAnimated ()
	    && !result->isVolume () && result->variants.empty () && result->imageCount == 1 && mipmapCount > 1;

	for (uint32_t mipmap = 0; mipmap < mipmapCount; mipmap++) {
	    auto parsed = parseMipmap (file, *result, deferRaw);

	    if (parsed == nullptr) {
		truncated = true;
		break;
	    }

	    if (!result->variants.empty ()) {
		const bool deferPatches = deferMipmaps && result->freeImageFormat != FIF_UNKNOWN
		    && !result->isVideoMp4 && !(result->flags & TextureFlags_Video) && !result->isAnimated ()
		    && !result->isVolume () && result->imageCount == 1 && mipmapCount > 1;
		parseVariantPatches (*parsed, *result, file, selected, deferPatches);
	    }
	    mipmaps.emplace_back (std::move (parsed));
	}

	if (mipmaps.empty ()) {
	    sLog.exception ("Cannot parse texture, the first mipmap's header is not valid");
	}

	result->images.emplace (image, mipmaps);
    }

    // Mixed raw chains and truncated single-level chains use ordinary construction.
    // Finish any retained LZ4 before returning them to that synchronous path.
    if (deferMipmaps && result->freeImageFormat == FIF_UNKNOWN && !canStreamMipmaps (*result))
	decodeMipmaps (*result);
    if (truncated || !result->isAnimated ()) {
	return result;
    }

    parseAnimations (*result, file);

    return result;
}

MipmapSharedPtr TextureParser::parseMipmap (const BinaryReader& file, const Texture& header, const bool deferDecompression) {
    auto result = std::make_shared<Mipmap> ();
    const bool isVideo = (header.flags & TextureFlags_Video) != 0;

    result->width = file.nextUInt32 ();
    result->height = file.nextUInt32 ();
    if (header.isVolume ()) {
	result->depth = file.nextUInt32 ();
	// Native reader 14015c8d0 bounds all three axes before decoding its slice canvas.
	if (result->width == 0 || result->height == 0 || result->depth == 0
	    || result->width > 8192 || result->height > 8192 || result->depth > 128
	    || uint64_t (result->width) * result->height * result->depth * 4 > (1u << 30)) {
	    throw std::runtime_error ("Invalid volume texture dimensions");
	}
    }

    if (header.containerVersion == ContainerVersion_TEXB0004 || header.containerVersion == ContainerVersion_TEXB0003
	|| header.containerVersion == ContainerVersion_TEXB0002) {
	result->compression = file.nextUInt32 ();
	result->uncompressedSize = file.nextInt ();
    }

    result->compressedSize = file.nextInt ();

    // Packed video textures carry a raw MP4 payload. Its preceding uncompressed-size
    // field is zero, so the following compressed-size field is the authoritative byte
    // count even though the payload itself is not compressed.
    if (isVideo) {
	if (result->compressedSize <= 0) {
	    return nullptr;
	}

	result->uncompressedSize = result->compressedSize;
	result->compression = 0;
	checkPayloadSize (file, result->compressedSize, result->uncompressedSize);
	result->uncompressedData = std::make_unique<char[]> (result->uncompressedSize);
	file.next (result->uncompressedData.get (), result->uncompressedSize);
	return result;
    }

    if (result->compression == 0) {
	// this might be better named as mipmap_bytes_size instead of compressedSize
	// as in uncompressed files this variable actually holds the file length
	result->uncompressedSize = result->compressedSize;
    }

    if (result->width == 0 || result->height == 0 || result->compression > 1 || result->compressedSize <= 0
	|| result->uncompressedSize <= 0) {
	return nullptr;
    }

    checkPayloadSize (file, result->compressedSize, result->uncompressedSize);
    if (deferDecompression && result->compression == 1) {
	result->compressedData = std::make_unique<char[]> (result->compressedSize);
	file.next (result->compressedData.get (), result->compressedSize);
	return result;
    }
    result->uncompressedData = std::unique_ptr<char[]> (new char[result->uncompressedSize]);

    if (result->compression == 1) {
	result->compressedData = std::unique_ptr<char[]> (new char[result->compressedSize]);
	// read the compressed data into the buffer
	file.next (result->compressedData.get (), result->compressedSize);
	// finally decompress it
	int bytes = LZ4_decompress_safe (
	    result->compressedData.get (), result->uncompressedData.get (), result->compressedSize,
	    result->uncompressedSize
	);

	if (bytes != result->uncompressedSize) {
	    sLog.exception ("Cannot decompress texture data, LZ4_decompress_safe returned an error");
	}

	// nothing reads the compressed copy again, and the texture cache holds mipmaps for the
	// lifetime of the process, so keeping it would retain the whole packed payload for nothing
	result->compressedData.reset ();
    } else {
	file.next (result->uncompressedData.get (), result->uncompressedSize);
    }

    if (header.isVolume ()) {
	if (header.freeImageFormat != FIF_UNKNOWN) {
	    int width, height, channels;
	    if (!stbi_info_from_memory (reinterpret_cast<const unsigned char*> (result->uncompressedData.get ()),
		result->uncompressedSize, &width, &height, &channels)
		|| width != static_cast<int> (result->width)
		|| height != static_cast<int> (result->height * result->depth)) {
		throw std::runtime_error ("Volume image must contain width by height*depth stacked slices");
	    }
	} else {
	    const uint32_t channels = header.format == TextureFormat_ARGB8888 ? 4
		: header.format == TextureFormat_RG88 ? 2 : header.format == TextureFormat_R8 ? 1 : 0;
	    if (channels == 0) throw std::runtime_error ("Unsupported raw volume texture format");
	    if (uint64_t (result->width) * result->height * result->depth * channels
		!= static_cast<uint64_t> (result->uncompressedSize)) {
		throw std::runtime_error ("Invalid volume texture payload size");
	    }
	}
    }

    return result;
}

FrameSharedPtr TextureParser::parseFrameV1 (const BinaryReader& file) {
    auto result = std::make_shared<Frame> ();

    result->frameNumber = file.nextUInt32 ();
    result->frametime = file.nextFloat ();
    result->x = static_cast<float> (file.nextInt ());
    result->y = static_cast<float> (file.nextInt ());
    result->width1 = static_cast<float> (file.nextInt ());
    result->width2 = static_cast<float> (file.nextInt ());
    result->height2 = static_cast<float> (file.nextInt ());
    result->height1 = static_cast<float> (file.nextInt ());

    return result;
}

FrameSharedPtr TextureParser::parseFrame (const BinaryReader& file) {
    auto result = std::make_shared<Frame> ();

    result->frameNumber = file.nextUInt32 ();
    result->frametime = file.nextFloat ();
    result->x = file.nextFloat ();
    result->y = file.nextFloat ();
    result->width1 = file.nextFloat ();
    result->width2 = file.nextFloat ();
    result->height2 = file.nextFloat ();
    result->height1 = file.nextFloat ();

    return result;
}

TextureMap TextureParser::parseTextureMap (const JSON& it) {
    if (!it.is_array ()) {
	return {};
    }

    TextureMap result = {};
    int textureIndex = -1;

    for (const auto& cur : it) {
	textureIndex++;

	if (cur.is_null ()) {
	    continue;
	}

	if (cur.is_object ()) {
	    const auto nameIt = cur.find ("name");
	    if (nameIt != cur.end () && nameIt->is_string ()) {
		result.emplace (textureIndex, nameIt->get<std::string> ());
	    }
	} else if (cur.is_string ()) {
	    std::string texName = cur;
	    if (!texName.empty ()) {
		result.emplace (textureIndex, texName);
	    }
	}
    }

    return result;
}

TextureFormat TextureParser::parseTextureFormat (uint32_t value) {
    switch (value) {
	case TextureFormat_UNKNOWN:
	case TextureFormat_ARGB8888:
	case TextureFormat_RGB888:
	case TextureFormat_RGB565:
	case TextureFormat_DXT5:
	case TextureFormat_DXT3:
	case TextureFormat_DXT1:
	case TextureFormat_RG88:
	case TextureFormat_R8:
	case TextureFormat_RG1616f:
	case TextureFormat_R16f:
	case TextureFormat_BC7:
	case TextureFormat_RGBa1010102:
	case TextureFormat_RGBA16161616f:
	case TextureFormat_RGB161616f:
	    return static_cast<TextureFormat> (value);

	default:
	    sLog.exception ("unknown texture format: ", value);
    }
}

void TextureParser::parseTextureHeader (Texture& header, const BinaryReader& file) {
    char magic[9] = { 0 };

    file.next (magic, 9);

    const bool legacy = strncmp (magic, "TEXV0004", 9) == 0;
    if (!legacy && strncmp (magic, "TEXV0005", 9) != 0) {
	sLog.exception ("unexpected texture container type: ", std::string_view (magic, 9));
    }

    if (!legacy) {
	file.next (magic, 9);

	if (strncmp (magic, "TEXI0001", 9) != 0) {
	    sLog.exception ("unexpected texture sub-container type: ", std::string_view (magic, 9));
	}
    }

    header.format = parseTextureFormat (file.nextUInt32 ());
    header.flags = parseTextureFlags (file.nextUInt32 ());
    header.textureWidth = file.nextUInt32 ();
    header.textureHeight = file.nextUInt32 ();
    header.width = file.nextUInt32 ();
    header.height = file.nextUInt32 ();
    if (header.isVolume ()) {
	// Native TEXI reader 14015c760 places depth before the existing versioned word.
	header.textureDepth = file.nextUInt32 ();
	if (header.textureDepth == 0 || header.textureDepth > 128) {
	    throw std::runtime_error ("Invalid volume texture depth");
	}
	if (header.flags & (TextureFlags_IsGif | TextureFlags_Video)) {
	    throw std::runtime_error ("Animated and video volume textures are not supported");
	}
    }

    if (legacy) {
	// TEXV0004 has no TEXI/TEXB sections or image count. One image's mip
	// count follows these six fields, using the same raw records as TEXB0001.
	header.containerVersion = ContainerVersion_TEXB0001;
	header.imageCount = 1;
	return;
    }

    // ignore some more bytes
    std::ignore = file.nextUInt32 ();
    parseContainer (header, file);
    if (header.isVolume () && !header.variants.empty ()) {
	throw std::runtime_error ("Conditional volume texture variants are not supported");
    }
}

void TextureParser::parseContainer (Texture& header, const BinaryReader& file) {
    char magic[9] = { 0 };

    file.next (magic, 9);

    header.imageCount = file.nextUInt32 ();

    if (strncmp (magic, "TEXB0004", 9) == 0) {
	header.containerVersion = ContainerVersion_TEXB0004;
	header.freeImageFormat = parseFIF (file.nextUInt32 ());
	// number of conditional image variants stored ahead of the base image; each one is an
	// alternate picked by a user property (3737268876 ships Link's tunic as three
	// 'tuniccolor' alternates on top of the default green base image)
	const uint32_t conditionalImages = file.nextUInt32 ();
	// mp4 containers report a single entry too, so only trust that when the header also
	// flags video; parseMipmap needs TextureFlags_Video to read the payload anyway
	header.isVideoMp4 = conditionalImages == 1 && (header.flags & TextureFlags_Video) != 0;

	if (header.freeImageFormat == FIF_UNKNOWN && header.isVideoMp4) {
	    header.freeImageFormat = FIF_MP4;
	}

	if (!(header.flags & TextureFlags_Video)) {
	    if (conditionalImages > 16384) {
		throw std::runtime_error ("Invalid texture variant count");
	    }
	    for (uint32_t variant = 0; variant < conditionalImages; variant++) {
		TextureVariant entry;
		entry.group = file.nextUInt32 ();
		entry.id = file.nextUInt32 ();
		entry.flags = file.nextUInt32 ();
		const auto metadata = JSON::parse (file.nextNullTerminatedString ());
		entry.condition = metadata.value ("condition", JSON ());
		header.variants.emplace_back (std::move (entry));
	    }
	}
    } else if (strncmp (magic, "TEXB0003", 9) == 0) {
	header.containerVersion = ContainerVersion_TEXB0003;
	header.freeImageFormat = parseFIF (file.nextUInt32 ());
    } else if (strncmp (magic, "TEXB0002", 9) == 0) {
	header.containerVersion = ContainerVersion_TEXB0002;
    } else if (strncmp (magic, "TEXB0001", 9) == 0) {
	header.containerVersion = ContainerVersion_TEXB0001;
    } else {
	sLog.exception ("unknown texture format type: ", std::string_view (magic, 9));
    }
}

void TextureParser::parseAnimations (Texture& header, const BinaryReader& file) {
    char magic[9] = { 0 };

    // image is animated, keep parsing the rest of the image info
    file.next (magic, 9);

    if (strncmp (magic, "TEXS0001", 9) == 0) {
	header.animatedVersion = AnimatedVersion_TEXS0001;
    } else if (strncmp (magic, "TEXS0002", 9) == 0) {
	header.animatedVersion = AnimatedVersion_TEXS0002;
    } else if (strncmp (magic, "TEXS0003", 9) == 0) {
	header.animatedVersion = AnimatedVersion_TEXS0003;
    } else {
	sLog.exception ("found animation information of unknown type: ", std::string_view (magic, 9));
    }

    uint32_t frameCount = file.nextUInt32 ();

    if (header.animatedVersion == AnimatedVersion_TEXS0003) {
	header.gifWidth = file.nextUInt32 ();
	header.gifHeight = file.nextUInt32 ();
    }

    while (frameCount-- > 0) {
	if (header.animatedVersion == AnimatedVersion_TEXS0001) {
	    header.frames.push_back (parseFrameV1 (file));
	} else {
	    header.frames.push_back (parseFrame (file));
	}
    }

    // TEXS0001 and TEXS0002 use the authored TEXI dimensions, independent of the frame's atlas basis.
    if (header.animatedVersion == AnimatedVersion_TEXS0001 || header.animatedVersion == AnimatedVersion_TEXS0002) {
	header.gifWidth = header.width;
	header.gifHeight = header.height;
    }

    // Calculate spritesheet grid dimensions from animation frames
    // Spritesheets are grid-based textures where each frame is at a specific position
    if (!header.frames.empty () && header.width > 0 && header.height > 0) {
	auto& firstFrame = *header.frames.front ();
	float frameWidth = firstFrame.width1;
	float frameHeight = firstFrame.height1;

	if (frameWidth > 0.0f && frameHeight > 0.0f) {
	    const uint32_t cols = static_cast<uint32_t> (std::round (static_cast<double> (header.width) / frameWidth));
	    const uint32_t rows
		= static_cast<uint32_t> (std::round (static_cast<double> (header.height) / frameHeight));
	    const uint32_t frameCount = static_cast<uint32_t> (header.frames.size ());

	    // Only populate spritesheet metadata if the inferred grid can actually hold all frames
	    // This prevents GIFs (where frameWidth == textureWidth) from being treated as 1×1 spritesheets
	    if (cols > 0 && rows > 0 && cols * rows >= frameCount) {
		header.spritesheetCols = cols;
		header.spritesheetRows = rows;
		header.spritesheetFrames = frameCount;

		float totalDuration = 0.0f;
		for (const auto& frame : header.frames) {
		    totalDuration += frame->frametime;
		}
		header.spritesheetDuration = totalDuration;
	    }
	}
    }
}

uint32_t TextureParser::parseTextureFlags (uint32_t value) {
    // Texture flags are a bitmask, not a bounded enum. Newer Wallpaper Engine
    // assets set metadata bits that this renderer does not consume (for example
    // the 0x200000/0x800000 bits used by several model material masks). Preserve
    // every bit so the known sampling flags still work instead of rejecting an
    // otherwise valid texture.
    return value;
}

FIF TextureParser::parseFIF (uint32_t value) {
    switch (value) {
	case FIF_UNKNOWN:
	case FIF_BMP:
	case FIF_ICO:
	case FIF_JPEG:
	case FIF_JNG:
	case FIF_KOALA:
	case FIF_LBM:
	case FIF_MNG:
	case FIF_PBM:
	case FIF_PBMRAW:
	case FIF_PCD:
	case FIF_PCX:
	case FIF_PGM:
	case FIF_PGMRAW:
	case FIF_PNG:
	case FIF_PPM:
	case FIF_PPMRAW:
	case FIF_RAS:
	case FIF_TARGA:
	case FIF_TIFF:
	case FIF_WBMP:
	case FIF_PSD:
	case FIF_CUT:
	case FIF_XBM:
	case FIF_XPM:
	case FIF_DDS:
	case FIF_GIF:
	case FIF_HDR:
	case FIF_FAXG3:
	case FIF_SGI:
	case FIF_EXR:
	case FIF_J2K:
	case FIF_JP2:
	case FIF_PFM:
	case FIF_PICT:
	case FIF_RAW:
	case FIF_WEBP:
	case FIF_JXR:
	    return static_cast<FIF> (value);

	default:
	    sLog.exception ("unknown free image format: ", value);
    }
}

TextureUniquePtr TextureParser::parse (
    const BinaryReader& file, const std::string& filename,
    std::function<std::string (const std::string&)> metadataLoader, const bool deferMipmaps
) {
    // Parse the binary .tex file first
    auto result = parse (file, VariantSelector {}, deferMipmaps);

    // Try to load optional .tex-json metadata for spritesheet data
    if (metadataLoader) {
	parseSpritesheetMetadata (*result, filename, metadataLoader);
    }

    return result;
}

void TextureParser::parseSpritesheetMetadata (
    Texture& header, const std::string& filename, std::function<std::string (const std::string&)> metadataLoader
) {
    try {
	std::string texJsonContent = metadataLoader (filename + ".tex-json");
	JSON texJson = WallpaperEngine::Data::JSON::parseCompatible (texJsonContent, filename + ".tex-json");

	// Check for spritesheet sequences
	if (texJson.contains ("spritesheetsequences") && texJson["spritesheetsequences"].is_array ()) {
	    auto& sequences = texJson["spritesheetsequences"];
	    if (!sequences.empty ()) {
		auto& firstSeq = sequences[0];
		int frames = firstSeq.value ("frames", 0);
		float frameWidth = firstSeq.value ("width", 0.0f);
		float frameHeight = firstSeq.value ("height", 0.0f);
		float duration = firstSeq.value ("duration", 1.0f);

		if (frames > 0 && frameWidth > 0.0f && frameHeight > 0.0f && header.width > 0 && header.height > 0) {
		    // Calculate grid dimensions from texture size and frame size
		    header.spritesheetCols = static_cast<uint32_t> (std::round (header.width / frameWidth));
		    header.spritesheetRows = static_cast<uint32_t> (std::round (header.height / frameHeight));
		    header.spritesheetFrames = static_cast<uint32_t> (frames);
		    header.spritesheetDuration = duration;
		}
	    }
	}
    } catch (const std::exception&) {
	// .tex-json file is optional, only used for spritesheet data
    }
}

bool TextureParser::matchesCondition (const JSON& condition, const Properties& properties) {
    const std::string name = condition.is_string () ? condition.get<std::string> ()
	: condition.is_object ()                    ? condition.value ("name", std::string {})
						    : std::string {};
    const auto property = properties.find (name);
    if (property == properties.end ()) {
	return false;
    }
    if (condition.is_string ()) {
	return property->second->getBool ();
    }
    const auto expected = condition.find ("condition");
    return expected != condition.end () && expected->is_string ()
	&& property->second->toString () == expected->get<std::string> ();
}

std::vector<uint32_t> TextureParser::selectedVariants (const Texture& texture, const VariantSelector& selector) {
    std::set<uint32_t> groups;
    std::vector<uint32_t> selected;
    if (selector) {
	for (const auto& variant : texture.variants) {
	    // Native TEXB4 selects the first matching condition in each group.
	    if (!groups.contains (variant.group) && selector (variant.condition)) {
		groups.insert (variant.group);
		selected.push_back (variant.id);
	    }
	}
    }
    return selected;
}

void TextureParser::parseVariantPatches (
    Mipmap& mipmap, const Texture& header, const BinaryReader& file, const std::vector<uint32_t>& selected, const bool deferPatches
) {
    // TEXB4: base mip payload, group count, then a patch count per group.
    // Each patch has version/id/x/y/width/height/FIF/byte count, followed by bytes.
    // LZ4 compression is inherited from the base mip, not encoded in the patch.
    const auto groups = file.nextUInt32 ();
    if (groups > 16384) {
	throw std::runtime_error ("Invalid texture patch group count");
    }
    for (uint32_t group = 0; group < groups; ++group) {
	const auto count = file.nextUInt32 ();
	if (count > remainingBytes (file) / 32) {
	    throw std::runtime_error ("Truncated texture patch table");
	}
	for (uint32_t index = 0; index < count; ++index) {
	    std::ignore = file.nextUInt32 (); // record version (1 in authored assets)
	    const auto id = file.nextUInt32 ();
	    const auto x = file.nextUInt32 ();
	    const auto y = file.nextUInt32 ();
	    const auto width = file.nextUInt32 ();
	    const auto height = file.nextUInt32 ();
	    std::ignore = file.nextUInt32 (); // patch FIF; stbi identifies image bytes itself
	    const auto size = file.nextInt ();
	    if (size < 0 || static_cast<size_t> (size) > remainingBytes (file)) {
		throw std::runtime_error ("Truncated texture variant payload");
	    }
	    if (std::ranges::find (selected, id) == selected.end () || size == 0) {
		file.base ().seekg (size, std::ios::cur);
		continue;
	    }
	    const auto variant = std::ranges::find (header.variants, id, &TextureVariant::id);
	    if (variant == header.variants.end ()) {
		throw std::runtime_error ("Unknown texture variant id");
	    }
	    if (width == 0 || height == 0 || x > mipmap.width || width > mipmap.width - x || y > mipmap.height
		|| height > mipmap.height - y) {
		throw std::runtime_error ("Texture variant patch lies outside the mipmap");
	    }
	    std::string payload (size, '\0');
	    file.next (payload.data (), payload.size ());
	    mipmap.variantPatches.push_back ({ variant->flags, x, y, width, height, std::move (payload) });
	}
    }
    mipmap.variantFormat = header.format;
    if (!deferPatches && !mipmap.variantPatches.empty ())
	decodeMipmap (mipmap, header.freeImageFormat, false);
}

TextureUniquePtr TextureParser::selectVariants (
    const Texture& texture, const VariantSelector& selector, const bool deferMipmaps
) {
    if (!texture.variantSource) {
	throw std::runtime_error ("Texture has no retained variant source");
    }
    const auto input = std::make_shared<std::istringstream> (*texture.variantSource, std::ios::binary);
    auto result = parse (BinaryReader (input), selector, deferMipmaps);
    result->variantSelectionPrepared = deferMipmaps;
    result->variantSource = texture.variantSource;
    result->spritesheetCols = texture.spritesheetCols;
    result->spritesheetRows = texture.spritesheetRows;
    result->spritesheetFrames = texture.spritesheetFrames;
    result->spritesheetDuration = texture.spritesheetDuration;
    if (!deferMipmaps || !canStreamMipmaps (*result)) decodeMipmaps (*result);
    return result;
}

TextureUniquePtr TextureParser::parseImage (const std::string& contents, const JSON& metadata) {
    if (contents.size () > static_cast<size_t> (std::numeric_limits<int>::max () - AV_INPUT_BUFFER_PADDING_SIZE)) {
	throw std::runtime_error ("Source image is too large");
    }
    auto result = std::make_unique<Texture> ();
    int width = 0, height = 0, channels = 0;
    std::vector<GifImage> gifImages;
    if (contents.starts_with ("GIF87a") || contents.starts_with ("GIF89a")) {
	gifImages = decodeGifImages (contents, width, height);
    }
    DecodedPixelsPtr staticPixels;
    if (gifImages.size () <= 1) {
	// Preserve native/static stb background handling for single-frame GIFs.
	gifImages.clear ();
	staticPixels.reset (stbi_load_from_memory (
	    reinterpret_cast<const unsigned char*> (contents.data ()), static_cast<int> (contents.size ()), &width,
	    &height, &channels, 4
	));
	if (!staticPixels || width <= 0 || height <= 0) {
	    throw std::runtime_error ("Cannot decode source image");
	}
    }
    result->format = TextureFormat_ARGB8888;
    const auto format = metadata.value ("format", std::string ("rgba8888"));
    size_t components = 4;
    if (format == "rg88" || format == "rg88n") {
	result->format = TextureFormat_RG88;
	components = 2;
    } else if (format == "r8") {
	result->format = TextureFormat_R8;
	components = 1;
    } else if (format != "rgba8888") {
	throw std::runtime_error ("Unsupported source texture format " + format);
    }
    result->width = result->textureWidth = width;
    result->height = result->textureHeight = height;
    result->imageCount = gifImages.empty () ? 1 : gifImages.size ();
    if (!gifImages.empty ()) {
	result->flags |= TextureFlags_IsGif;
	result->gifWidth = width;
	result->gifHeight = height;
    }
    if (metadata.value ("nointerpolation", false)) {
	result->flags |= TextureFlags_NoInterpolation;
    }
    if (metadata.value ("clampuvs", false)) {
	result->flags |= TextureFlags_ClampUVs;
    }
    for (uint32_t image = 0; image < result->imageCount; ++image) {
	const auto* pixels = gifImages.empty () ? staticPixels.get () : gifImages[image].pixels.get ();
	auto mipmap = std::make_shared<Mipmap> ();
	mipmap->width = width;
	mipmap->height = height;
	mipmap->uncompressedSize = static_cast<int> (static_cast<size_t> (width) * height * components);
	mipmap->uncompressedData = std::make_unique<char[]> (mipmap->uncompressedSize);
	for (size_t pixel = 0; pixel < static_cast<size_t> (width) * height; ++pixel) {
	    std::memcpy (mipmap->uncompressedData.get () + pixel * components, pixels + pixel * 4, components);
	}
	if (!gifImages.empty ()) {
	    gifImages[image].pixels.reset ();
	    auto frame = std::make_shared<Frame> ();
	    frame->frameNumber = image;
	    frame->frametime = gifImages[image].frametime;
	    frame->width1 = width;
	    frame->height1 = height;
	    result->frames.push_back (std::move (frame));
	}
	result->images[image].push_back (mipmap);
	// The shipped sources request nomip. For other sources, create the chain
	// on the CPU so the ordinary uploader sees the same complete mip layout.
	if (!metadata.value ("nomip", false)) {
	    while (mipmap->width > 1 || mipmap->height > 1) {
		auto next = std::make_shared<Mipmap> ();
		next->width = std::max (1u, mipmap->width / 2);
		next->height = std::max (1u, mipmap->height / 2);
		next->uncompressedSize = static_cast<int> (next->width * next->height * components);
		next->uncompressedData = std::make_unique<char[]> (next->uncompressedSize);
		for (uint32_t y = 0; y < next->height; ++y) {
		    for (uint32_t x = 0; x < next->width; ++x) {
			for (size_t channel = 0; channel < components; ++channel) {
			    unsigned sum = 0;
			    for (uint32_t dy = 0; dy < 2; ++dy) {
				for (uint32_t dx = 0; dx < 2; ++dx) {
				    const size_t at = (std::min (y * 2 + dy, mipmap->height - 1) * mipmap->width
						       + std::min (x * 2 + dx, mipmap->width - 1))
					    * components
					+ channel;
				    sum += static_cast<unsigned char> (mipmap->uncompressedData[at]);
				}
			    }
			    next->uncompressedData[(y * next->width + x) * components + channel]
				= static_cast<char> (sum / 4);
			}
		    }
		}
		result->images[image].push_back (next);
		mipmap = std::move (next);
	    }
	}
    }
    return result;
}

TextureUniquePtr TextureParser::loadHeader (const WallpaperEngine::Assets::AssetLocator& locator, const std::string& filename) {
    auto input = locator.texture (filename);
    const auto exceptions = input->exceptions ();
    input->exceptions (std::ios::failbit | std::ios::badbit);
    const ScopeGuard restoreExceptions ([&] { input->exceptions (exceptions); });
    auto result = std::make_unique<Texture> ();
    parseTextureHeader (*result, BinaryReader (input));
    return result;
}

TextureUniquePtr
TextureParser::load (const WallpaperEngine::Assets::AssetLocator& locator, const std::string& filename, const bool deferMipmaps) {
    const auto metadataLoader = [&locator] (const std::string& name) {
	return locator.readString (std::filesystem::path ("materials") / name);
    };
    ReadStreamSharedPtr compiled;
    try {
	compiled = locator.texture (filename);
    } catch (const WallpaperEngine::Assets::AssetLoadException&) {
	// An editor source needs its import settings. Do not reinterpret an arbitrary
	// PNG (or a corrupt .tex) as a compiled texture merely because it exists.
	const auto metadata = WallpaperEngine::Data::JSON::parseCompatible (
	    metadataLoader (filename + ".tex-json"), filename + ".tex-json"
	);
	const auto contents = locator.readString (std::filesystem::path ("materials") / (filename + ".png"));
	auto result = parseImage (contents, metadata);
	parseSpritesheetMetadata (*result, filename, [&metadata] (const std::string&) { return metadata.dump (); });
	return result;
    }
    auto result = parse (BinaryReader (compiled), filename, metadataLoader, deferMipmaps);
    if (!result->variants.empty ()) {
	compiled->seekg (0, std::ios::beg);
	result->variantSource = std::make_shared<const std::string> (
	    std::istreambuf_iterator<char> (*compiled), std::istreambuf_iterator<char> ()
	);
    }
    return result;
}
