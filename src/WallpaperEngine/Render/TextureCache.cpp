#include "TextureCache.h"

#include "WallpaperEngine/FileSystem/Container.h"

#include "CTexture.h"
#include "WallpaperEngine/Assets/AssetLoadException.h"
#include "WallpaperEngine/Render/Helpers/ContextAware.h"

#include "WallpaperEngine/Data/Model/Project.h"
#include "WallpaperEngine/Data/Parsers/TextureParser.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <poll.h>
#include <signal.h>
#include <spawn.h>
#include <stb_image.h>
#include <sys/wait.h>
#include <unistd.h>

using namespace WallpaperEngine::Render;
using namespace WallpaperEngine::FileSystem;
using namespace WallpaperEngine::Data::Parsers;
using namespace WallpaperEngine::Data::Assets;

extern char** environ;

namespace {
constexpr size_t kMaxThumbnailBytes = 16 * 1024 * 1024;

std::string trimTrailingNewlines (std::string value) {
    while (!value.empty () && (value.back () == '\n' || value.back () == '\r')) {
	value.pop_back ();
    }
    return value;
}

std::vector<char*> makeArgv (const std::string& program, const std::vector<std::string>& args) {
    std::vector<char*> argv;
    argv.reserve (args.size () + 2);
    argv.push_back (const_cast<char*> (program.c_str ()));
    for (const auto& arg : args) {
	argv.push_back (const_cast<char*> (arg.c_str ()));
    }
    argv.push_back (nullptr);
    return argv;
}

std::optional<pid_t>
spawnProcessWithStdout (const std::string& program, const std::vector<std::string>& args, int& readFd) {
    int outputPipe[2] {};
    if (pipe (outputPipe) != 0) {
	return std::nullopt;
    }

    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init (&actions);
    posix_spawn_file_actions_adddup2 (&actions, outputPipe[1], STDOUT_FILENO);
    posix_spawn_file_actions_addclose (&actions, outputPipe[0]);
    posix_spawn_file_actions_addclose (&actions, outputPipe[1]);

    auto argv = makeArgv (program, args);
    pid_t pid = 0;
    const int spawnResult = posix_spawnp (&pid, program.c_str (), &actions, nullptr, argv.data (), environ);
    posix_spawn_file_actions_destroy (&actions);
    close (outputPipe[1]);

    if (spawnResult != 0) {
	close (outputPipe[0]);
	return std::nullopt;
    }

    readFd = outputPipe[0];
    return pid;
}

bool waitForProcessUntil (pid_t pid, const std::chrono::steady_clock::time_point& deadline, int& status) {
    while (std::chrono::steady_clock::now () < deadline) {
	const pid_t waitResult = waitpid (pid, &status, WNOHANG);
	if (waitResult == pid) {
	    return true;
	}
	if (waitResult < 0) {
	    return false;
	}
	usleep (1000);
    }

    kill (pid, SIGKILL);
    waitpid (pid, &status, 0);
    return false;
}

bool pollReadable (int readFd, const std::chrono::steady_clock::time_point& deadline) {
    const auto remaining
	= std::chrono::duration_cast<std::chrono::milliseconds> (deadline - std::chrono::steady_clock::now ());
    if (remaining.count () <= 0) {
	return false;
    }

    pollfd readPoll {
	.fd = readFd,
	.events = POLLIN,
	.revents = 0,
    };
    const int pollResult = poll (&readPoll, 1, static_cast<int> (std::max (remaining.count (), int64_t { 1 })));
    return pollResult > 0 && !(readPoll.revents & (POLLERR | POLLNVAL)) && (readPoll.revents & (POLLIN | POLLHUP));
}

std::string processReadFirstLine (const std::string& program, const std::vector<std::string>& args) {
    constexpr auto timeout = std::chrono::milliseconds (300);
    const auto deadline = std::chrono::steady_clock::now () + timeout;

    int outputFd = -1;
    const auto pid = spawnProcessWithStdout (program, args, outputFd);
    if (!pid.has_value ()) {
	return {};
    }

    std::array<char, 512> buffer {};
    std::string output;
    while (std::chrono::steady_clock::now () < deadline && pollReadable (outputFd, deadline)) {
	const ssize_t bytesRead = read (outputFd, buffer.data (), buffer.size ());
	if (bytesRead <= 0) {
	    break;
	}
	output.append (buffer.data (), static_cast<size_t> (bytesRead));
	const auto newline = output.find ('\n');
	if (newline != std::string::npos) {
	    output.erase (newline + 1);
	    break;
	}
    }
    close (outputFd);

    int status = 0;
    if (!waitForProcessUntil (*pid, deadline, status)) {
	return {};
    }
    if (!WIFEXITED (status) || WEXITSTATUS (status) != 0) {
	return {};
    }

    return trimTrailingNewlines (output);
}

std::vector<char>
processReadBytes (const std::string& program, const std::vector<std::string>& args, std::chrono::milliseconds timeout) {
    const auto deadline = std::chrono::steady_clock::now () + timeout;

    int outputFd = -1;
    const auto pid = spawnProcessWithStdout (program, args, outputFd);
    if (!pid.has_value ()) {
	return {};
    }

    std::vector<char> output;
    std::array<char, 4096> buffer {};
    bool tooLarge = false;
    while (std::chrono::steady_clock::now () < deadline && pollReadable (outputFd, deadline)) {
	const ssize_t bytesRead = read (outputFd, buffer.data (), buffer.size ());
	if (bytesRead <= 0) {
	    break;
	}
	if (output.size () + static_cast<size_t> (bytesRead) > kMaxThumbnailBytes) {
	    tooLarge = true;
	    break;
	}
	output.insert (output.end (), buffer.begin (), buffer.begin () + bytesRead);
    }
    close (outputFd);

    if (tooLarge) {
	kill (*pid, SIGKILL);
	waitpid (*pid, nullptr, 0);
	return {};
    }

    int status = 0;
    if (!waitForProcessUntil (*pid, deadline, status)) {
	return {};
    }
    if (!WIFEXITED (status) || WEXITSTATUS (status) != 0) {
	return {};
    }

    return output;
}

std::vector<char> readFileBytes (const std::filesystem::path& path) {
    std::error_code sizeError;
    const auto size = std::filesystem::file_size (path, sizeError);
    if (!sizeError && size > kMaxThumbnailBytes) {
	return {};
    }

    std::ifstream stream (path, std::ios::binary);
    if (!stream) {
	return {};
    }

    std::vector<char> output;
    output.reserve (!sizeError ? static_cast<size_t> (std::min<uintmax_t> (size, kMaxThumbnailBytes)) : 0);
    std::array<char, 4096> buffer {};
    while (stream && output.size () < kMaxThumbnailBytes) {
	const auto remaining = kMaxThumbnailBytes - output.size ();
	stream.read (buffer.data (), static_cast<std::streamsize> (std::min (buffer.size (), remaining)));
	const auto bytesRead = stream.gcount ();
	if (bytesRead <= 0) {
	    break;
	}
	output.insert (output.end (), buffer.begin (), buffer.begin () + bytesRead);
    }
    if (stream && output.size () >= kMaxThumbnailBytes) {
	return {};
    }
    return output;
}

FIF detectImageFormat (const std::vector<char>& bytes) {
    const auto* data = reinterpret_cast<const unsigned char*> (bytes.data ());
    if (bytes.size () >= 4 && data[0] == 0x89 && data[1] == 'P' && data[2] == 'N' && data[3] == 'G') {
	return FIF_PNG;
    }
    if (bytes.size () >= 3 && data[0] == 0xFF && data[1] == 0xD8 && data[2] == 0xFF) {
	return FIF_JPEG;
    }
    if (bytes.size () >= 12 && std::memcmp (data, "RIFF", 4) == 0 && std::memcmp (data + 8, "WEBP", 4) == 0) {
	return FIF_WEBP;
    }
    return FIF_UNKNOWN;
}

int hexValue (char value) {
    if (value >= '0' && value <= '9') {
	return value - '0';
    }
    if (value >= 'a' && value <= 'f') {
	return value - 'a' + 10;
    }
    if (value >= 'A' && value <= 'F') {
	return value - 'A' + 10;
    }
    return -1;
}

std::optional<std::string> percentDecode (const std::string& value) {
    std::string result;
    result.reserve (value.size ());
    for (size_t index = 0; index < value.size (); index++) {
	if (value[index] != '%') {
	    result.push_back (value[index]);
	    continue;
	}
	if (index + 2 >= value.size ()) {
	    return std::nullopt;
	}
	const int high = hexValue (value[index + 1]);
	const int low = hexValue (value[index + 2]);
	if (high < 0 || low < 0) {
	    return std::nullopt;
	}
	result.push_back (static_cast<char> ((high << 4) | low));
	index += 2;
    }
    return result;
}

std::optional<std::filesystem::path> parseLocalFileUri (const std::string& uri) {
    if (uri.rfind ("file:", 0) != 0) {
	return std::nullopt;
    }

    std::string rest = uri.substr (5);
    std::string pathPart;
    if (rest.rfind ("//", 0) == 0) {
	rest = rest.substr (2);
	const auto slash = rest.find ('/');
	const std::string authority = slash == std::string::npos ? rest : rest.substr (0, slash);
	if (!authority.empty () && authority != "localhost") {
	    return std::nullopt;
	}
	pathPart = slash == std::string::npos ? std::string () : rest.substr (slash);
    } else {
	pathPart = rest;
    }

    auto decoded = percentDecode (pathPart);
    if (!decoded.has_value () || decoded->empty ()) {
	return std::nullopt;
    }
    return std::filesystem::path (*decoded);
}

std::shared_ptr<const TextureProvider> tryCreateImageTexture (RenderContext& context, const std::vector<char>& bytes) {
    if (bytes.empty ()) {
	return nullptr;
    }

    int width = 0;
    int height = 0;
    int channels = 0;
    if (!stbi_info_from_memory (
	    reinterpret_cast<const stbi_uc*> (bytes.data ()), static_cast<int> (bytes.size ()), &width, &height,
	    &channels
	)) {
	return nullptr;
    }

    auto mipmap = std::make_shared<Mipmap> ();
    mipmap->width = static_cast<uint32_t> (width);
    mipmap->height = static_cast<uint32_t> (height);
    mipmap->uncompressedSize = static_cast<int> (bytes.size ());
    mipmap->uncompressedData = std::make_unique<char[]> (bytes.size ());
    std::copy (bytes.begin (), bytes.end (), mipmap->uncompressedData.get ());

    auto texture = std::make_unique<Texture> ();
    texture->containerVersion = ContainerVersion_TEXB0001;
    texture->flags = TextureFlags_ClampUVs;
    texture->width = static_cast<uint32_t> (width);
    texture->height = static_cast<uint32_t> (height);
    texture->textureWidth = static_cast<uint32_t> (width);
    texture->textureHeight = static_cast<uint32_t> (height);
    texture->format = TextureFormat_ARGB8888;
    texture->freeImageFormat = detectImageFormat (bytes);
    texture->imageCount = 1;
    texture->images.emplace (0, MipmapList { mipmap });

    return std::make_shared<CTexture> (context, std::move (texture));
}

std::shared_ptr<const TextureProvider> tryLoadMediaThumbnailUrl (RenderContext& context, const std::string& artUrl) {
    std::vector<char> bytes;
    if (artUrl.rfind ("file:", 0) == 0) {
	if (const auto path = parseLocalFileUri (artUrl); path.has_value ()) {
	    bytes = readFileBytes (*path);
	}
    } else if (artUrl.rfind ("http://", 0) == 0 || artUrl.rfind ("https://", 0) == 0) {
	bytes = processReadBytes (
	    "curl", { "-L", "-s", "--max-time", "3", "--output", "-", artUrl }, std::chrono::milliseconds (3500)
	);
    }

    return tryCreateImageTexture (context, bytes);
}

struct MediaThumbnailState {
    RenderContext* context = nullptr;
    std::string currentArtUrl;
    std::shared_ptr<const TextureProvider> currentTexture = nullptr;
    std::shared_ptr<const TextureProvider> previousTexture = nullptr;
    std::chrono::steady_clock::time_point lastMetadataPoll = {};
    bool hasMetadataPoll = false;
};

MediaThumbnailState& mediaThumbnailState () {
    static MediaThumbnailState state;
    return state;
}

std::shared_ptr<const TextureProvider> tryLoadMediaThumbnail (RenderContext& context, bool previous) {
    auto& state = mediaThumbnailState ();
    if (state.context != &context) {
	state = MediaThumbnailState { .context = &context };
    }

    constexpr auto metadataPollInterval = std::chrono::milliseconds (750);
    const auto now = std::chrono::steady_clock::now ();
    if (state.hasMetadataPoll && now - state.lastMetadataPoll < metadataPollInterval) {
	return previous ? state.previousTexture : state.currentTexture;
    }
    state.lastMetadataPoll = now;
    state.hasMetadataPoll = true;

    const std::string artUrl = processReadFirstLine ("playerctl", { "metadata", "mpris:artUrl" });
    if (artUrl.empty ()) {
	return previous ? state.previousTexture : nullptr;
    }

    if (artUrl != state.currentArtUrl) {
	auto texture = tryLoadMediaThumbnailUrl (context, artUrl);
	if (texture != nullptr) {
	    state.previousTexture = state.currentTexture;
	    state.currentTexture = texture;
	    state.currentArtUrl = artUrl;
	}
    }

    return previous ? state.previousTexture : state.currentTexture;
}
}

TextureCache::TextureCache (RenderContext& context) : Helpers::ContextAware (context) { }

std::shared_ptr<const TextureProvider> TextureCache::resolve (const std::string& filename) {
    // Media thumbnails are dynamic, so avoid caching the texture by name.
    if (filename == "$mediaThumbnail" || filename == "$mediaPreviousThumbnail") {
	auto texture = tryLoadMediaThumbnail (this->getContext (), filename == "$mediaPreviousThumbnail");
	if (texture == nullptr) {
	    texture = this->resolve ("util/white");
	}
	return texture;
    }

    if (const auto found = this->m_textureCache.find (filename); found != this->m_textureCache.end ()) {
	return found->second;
    }

    // search for the texture in all the different containers just in case
    for (const auto& project : this->getContext ().getApp ().getBackgrounds () | std::views::values) {
	try {
	    const auto contents = project->assetLocator->texture (filename);
	    auto stream = BinaryReader (contents);

	    // Create metadata loader lambda that captures the assetLocator
	    // so we need to construct the full path here
	    auto metadataLoader = [&project] (const std::string& metaFilename) -> std::string {
		std::filesystem::path fullPath = std::filesystem::path ("materials") / metaFilename;
		return project->assetLocator->readString (fullPath);
	    };

	    auto parsedTexture = TextureParser::parse (stream, filename, metadataLoader);
	    auto texture = std::make_shared<CTexture> (this->getContext (), std::move (parsedTexture));

	    this->store (filename, texture);

	    return texture;
	} catch (AssetLoadException&) {
	    // ignored, this happens if we're looking at the wrong background
	}
    }

    throw AssetLoadException ("Cannot find file", filename, std::error_code ());
}

void TextureCache::store (const std::string& name, std::shared_ptr<const TextureProvider> texture) {
    this->m_textureCache.insert_or_assign (name, texture);
}
