#pragma once
#include "WallpaperEngine/Data/Utils/MemoryStream.h"

#include <mpv/client.h>

#include <cstring>
#include <memory>

namespace WallpaperEngine::VideoPlayback::MPV {
class GLPlayer;
struct MemoryStreamProtocol : Data::Utils::MemoryStream {
    explicit MemoryStreamProtocol (const char* pointer, const size_t size) :
	MemoryStream (std::unique_ptr<char[]> (new char[size]), size), m_size (size) {
	memcpy (this->m_buffer.get (), pointer, size);
    }

    void registerReadCallback (mpv_handle* handle);

    size_t m_size;
};

using MemoryStreamProtocolUniquePtr = std::unique_ptr<MemoryStreamProtocol>;
using MemoryStreamProtocolSharedPtr = std::shared_ptr<MemoryStreamProtocol>;
};
