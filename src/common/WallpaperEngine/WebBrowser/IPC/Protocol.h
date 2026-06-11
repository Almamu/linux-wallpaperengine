#pragma once

#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

namespace WallpaperEngine::WebBrowser::IPC {

// 1-99: core → webhelper   100-199: webhelper → core
enum class MessageType : uint32_t {
    CreateBrowser = 1,
    DestroyBrowser = 2,
    MouseMove = 3,
    MouseButton = 4,
    Resize = 5,
    AssetResponse = 6,

    FrameReady = 100,
    AssetRequest = 101,
};

// Wire format per message: [uint32 type][uint32 payload_size][payload bytes...]
// All integers are little-endian.
//
// Payload field encoding:
//   string  : [uint32 length][utf-8 bytes]
//   bytes   : [uint32 length][raw bytes]
//   int32   : 4 bytes LE
//   uint32  : 4 bytes LE
//   uint8   : 1 byte
//   float   : 4 bytes IEEE-754 LE

struct MessageWriter {
    std::vector<uint8_t> data;

    void writeU8 (uint8_t v) { data.push_back (v); }

    void writeU32 (uint32_t v) {
	data.push_back (static_cast<uint8_t> (v));
	data.push_back (static_cast<uint8_t> (v >> 8));
	data.push_back (static_cast<uint8_t> (v >> 16));
	data.push_back (static_cast<uint8_t> (v >> 24));
    }

    void writeI32 (int32_t v) { writeU32 (static_cast<uint32_t> (v)); }

    void writeFloat (float v) {
	uint32_t tmp;
	std::memcpy (&tmp, &v, 4);
	writeU32 (tmp);
    }

    void writeString (const std::string& s) {
	writeU32 (static_cast<uint32_t> (s.size ()));
	data.insert (data.end (), s.begin (), s.end ());
    }

    void writeBytes (const void* ptr, size_t len) {
	writeU32 (static_cast<uint32_t> (len));
	if (len > 0) {
	    const auto* p = static_cast<const uint8_t*> (ptr);
	    data.insert (data.end (), p, p + len);
	}
    }
};

struct MessageReader {
    const uint8_t* data;
    size_t size;
    size_t pos = 0;

    MessageReader (const uint8_t* d, size_t s) : data (d), size (s) { }

    uint8_t readU8 () {
	if (pos >= size) {
	    throw std::runtime_error ("IPC: read past end of message");
	}
	return data[pos++];
    }

    uint32_t readU32 () {
	if (pos + 4 > size) {
	    throw std::runtime_error ("IPC: read past end of message");
	}
	uint32_t v = static_cast<uint32_t> (data[pos]) | (static_cast<uint32_t> (data[pos + 1]) << 8)
	    | (static_cast<uint32_t> (data[pos + 2]) << 16) | (static_cast<uint32_t> (data[pos + 3]) << 24);
	pos += 4;
	return v;
    }

    int32_t readI32 () { return static_cast<int32_t> (readU32 ()); }

    float readFloat () {
	const uint32_t tmp = readU32 ();
	float v;
	std::memcpy (&v, &tmp, 4);
	return v;
    }

    std::string readString () {
	const uint32_t len = readU32 ();
	if (pos + len > size) {
	    throw std::runtime_error ("IPC: read past end of message");
	}
	std::string s (reinterpret_cast<const char*> (data + pos), len);
	pos += len;
	return s;
    }

    std::vector<uint8_t> readBytes () {
	const uint32_t len = readU32 ();
	if (pos + len > size) {
	    throw std::runtime_error ("IPC: read past end of message");
	}
	std::vector<uint8_t> v (data + pos, data + pos + len);
	pos += len;
	return v;
    }
};

} // namespace WallpaperEngine::WebBrowser::IPC
