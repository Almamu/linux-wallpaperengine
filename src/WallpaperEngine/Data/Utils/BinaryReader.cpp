#include <bit>
#include <iostream>

#include "BinaryReader.h"

#include "WallpaperEngine/Logging/Log.h"

#include <cstring>

using namespace WallpaperEngine::Data::Utils;

BinaryReader::BinaryReader (ReadStreamSharedPtr file) : m_input (std::move (file)) { }

void BinaryReader::readExact (char* out, std::streamsize size) const {
    this->m_input->read (out, size);

    if (this->m_input->gcount () != size) {
	sLog.exception ("Unexpected end of stream: wanted ", size, " bytes, got ", this->m_input->gcount ());
    }
}

std::streamsize BinaryReader::remaining () const {
    const std::streampos current = this->m_input->tellg ();

    if (current < 0) {
	// non-seekable or errored stream: we cannot bound safely, report nothing left
	return 0;
    }

    this->m_input->seekg (0, std::ios::end);
    const std::streampos end = this->m_input->tellg ();
    this->m_input->seekg (current, std::ios::beg);

    return end - current;
}

uint32_t BinaryReader::nextUInt32 () const {
    char buffer[4] = {0};

    this->readExact (buffer, 4);

    if constexpr (std::endian::native == std::endian::little) {
	return (buffer[3] & 0xFF) << 24 | (buffer[2] & 0xFF) << 16 | (buffer[1] & 0xFF) << 8 | (buffer[0] & 0xFF);
    } else {
	return (buffer[0] & 0xFF) << 24 | (buffer[1] & 0xFF) << 16 | (buffer[2] & 0xFF) << 8 | (buffer[3] & 0xFF);
    }
}

int BinaryReader::nextInt () const { return static_cast<int> (this->nextUInt32 ()); }

float BinaryReader::nextFloat () const {
    float result;
    static_assert (std::endian::native == std::endian::little, "Only little endian is supported for floats");

    this->readExact (reinterpret_cast<char*> (&result), sizeof (result));

    return result;
}

std::string BinaryReader::nextNullTerminatedString () const {
    std::string output;
    char c;

    // read one byte at a time until the terminator or end of stream; get() reports EOF via the
    // stream state so a missing terminator can no longer spin forever on an indeterminate byte
    while (this->m_input->get (c)) {
	if (c == '\0') {
	    break;
	}

	output += c;
    }

    return output;
}

std::string BinaryReader::nextSizedString () const {
    const uint32_t length = this->nextUInt32 ();

    if (static_cast<std::streamsize> (length) > this->remaining ()) {
	sLog.exception ("Sized string length ", length, " exceeds remaining stream size ", this->remaining ());
    }

    std::string output (length, '\0');

    this->readExact (output.data (), length);

    return output;
}

void BinaryReader::next (char* out, size_t size) const {
    this->readExact (out, static_cast<std::streamsize> (size));
}

char BinaryReader::next () const {
    char buffer = 0;
    this->readExact (&buffer, 1);
    return buffer;
}

std::istream& BinaryReader::base () const { return *this->m_input; }
