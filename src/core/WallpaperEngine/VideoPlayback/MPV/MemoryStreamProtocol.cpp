#include "MemoryStreamProtocol.h"

#include "GLPlayer.h"

#include <mpv/stream_cb.h>

using namespace WallpaperEngine::VideoPlayback::MPV;

int64_t mem_seek (void* cookie, const int64_t offset) {
    const auto stream = static_cast<MemoryStreamProtocol*> (cookie);

    // sometimes the stream can get to fail state depending on what mpv did
    // seeking usually means we can ignore that and try seek
    // if the seek fails the failbit will be set again
    if (stream->fail ()) {
	stream->clear ();
    }

    stream->seekg (offset, std::ios_base::beg);

    return stream->tellg ();
}

int64_t mem_read (void* cookie, char* buf, uint64_t bytes) {
    return static_cast<MemoryStreamProtocol*> (cookie)->read (buf, bytes).gcount ();
}

int64_t mem_size (void* cookie) { return static_cast<MemoryStreamProtocol*> (cookie)->m_size; }

void mem_close (void* cookie) {
    // seek to the beginning again for the next play
    mem_seek (cookie, 0);
    // closing does nothing else because the life is managed
    // by whatever owns it

    // this also allows starting playback in another instance no problem,
    // but two instances won't be able to use the same stream
}

int mem_open (void* userdata, char* uri, struct mpv_stream_cb_info* info) {
    info->cookie = userdata;
    info->read_fn = mem_read;
    info->seek_fn = mem_seek;
    info->close_fn = mem_close;
    info->size_fn = mem_size;

    return 0;
}

void MemoryStreamProtocol::registerReadCallback (mpv_handle* handle) {
    if (mpv_stream_cb_add_ro (handle, "buffer", this, mem_open) < 0) {
	sLog.exception ("Cannot register memory stream protocol for mpv");
    }
}