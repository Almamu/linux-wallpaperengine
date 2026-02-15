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
	if (dir == std::ios_base::cur) {
	    gbump (off);
	} else if (dir == std::ios_base::end) {
	    setg (eback (), egptr () + off, egptr ());
	} else if (dir == std::ios_base::beg) {
	    setg (eback (), eback () + off, egptr ());
	}
	return gptr () - eback ();
    }

    std::unique_ptr<char[]> m_buffer;
};

using MemoryStreamSharedPtr = std::shared_ptr<MemoryStream>;
}