#pragma once

#include <iostream>
#include <memory>

namespace WallpaperEngine::Data::Utils {
struct MemoryStream : std::istream, private std::streambuf {
    MemoryStream (std::unique_ptr<char[]> buffer, const size_t size) :
	std::istream (this), m_buffer (std::move (buffer)) {
	this->setg (this->m_buffer.get (), this->m_buffer.get (), this->m_buffer.get () + size);
    }

    std::streambuf::pos_type
    seekoff (std::streambuf::off_type off, std::ios_base::seekdir dir, std::ios_base::openmode which) override {
	char* const begin = this->eback ();
	char* const end = this->egptr ();
	char* target = nullptr;

	if (dir == std::ios_base::cur) {
	    target = this->gptr () + off;
	} else if (dir == std::ios_base::end) {
	    target = end + off;
	} else if (dir == std::ios_base::beg) {
	    target = begin + off;
	} else {
	    return std::streambuf::pos_type (std::streambuf::off_type (-1));
	}

	// reject seeks that would move the get pointer outside [begin, end]; a malformed offset from
	// an untrusted package must fail the seek, not leave gptr dangling for the next read
	if (target < begin || target > end) {
	    return std::streambuf::pos_type (std::streambuf::off_type (-1));
	}

	this->setg (begin, target, end);
	return target - begin;
    }

    std::streambuf::pos_type
    seekpos (std::streambuf::pos_type pos, std::ios_base::openmode which) override {
	return this->seekoff (pos, std::ios_base::beg, which);
    }

    std::unique_ptr<char[]> m_buffer;
};

using MemoryStreamSharedPtr = std::shared_ptr<MemoryStream>;
using MemoryStreamUniquePtr = std::unique_ptr<MemoryStream>;
}