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
	const std::streambuf::pos_type failed = std::streambuf::off_type (-1);
	if (!(which & std::ios_base::in)) {
	    return failed;
	}

	const std::streambuf::off_type length = egptr () - eback ();
	std::streambuf::off_type origin = 0;
	if (dir == std::ios_base::cur) {
	    origin = gptr () - eback ();
	} else if (dir == std::ios_base::end) {
	    origin = length;
	} else if (dir != std::ios_base::beg) {
	    return failed;
	}

	if (off < -origin || off > length - origin) {
	    return failed;
	}
	const auto position = origin + off;
	setg (eback (), eback () + position, egptr ());
	return position;
    }

    std::streambuf::pos_type seekpos (std::streambuf::pos_type pos, std::ios_base::openmode which) override {
	return seekoff (static_cast<std::streambuf::off_type> (pos), std::ios_base::beg, which);
    }

    [[nodiscard]] size_t size () const { return this->egptr () - this->eback (); }

    std::unique_ptr<char[]> m_buffer;
};

using MemoryStreamSharedPtr = std::shared_ptr<MemoryStream>;
using MemoryStreamUniquePtr = std::unique_ptr<MemoryStream>;
}
