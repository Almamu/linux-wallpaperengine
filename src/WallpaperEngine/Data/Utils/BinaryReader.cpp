#include "BinaryReader.h"

#include <algorithm>
#include <bit>
#include <istream>
#include <stdexcept>
#include <utility>

using namespace WallpaperEngine::Data::Utils;

BinaryReader::BinaryReader (ReadStreamSharedPtr file) : m_input (std::move (file)) { }

uint32_t BinaryReader::nextUInt32 () const {
    unsigned char buffer[4];
    this->next (reinterpret_cast<char*> (buffer), sizeof (buffer));
    return static_cast<uint32_t> (buffer[0]) | (static_cast<uint32_t> (buffer[1]) << 8)
	| (static_cast<uint32_t> (buffer[2]) << 16) | (static_cast<uint32_t> (buffer[3]) << 24);
}

int BinaryReader::nextInt () const { return static_cast<int> (this->nextUInt32 ()); }

float BinaryReader::nextFloat () const {
    float result;
    static_assert (std::endian::native == std::endian::little, "Only little endian is supported for floats");

    this->next (reinterpret_cast<char*> (&result), sizeof (result));

    return result;
}

std::string BinaryReader::nextNullTerminatedString () const {
    std::string output;

    while (const auto c = this->next ()) {
	output += c;
    }

    return output;
}

std::string BinaryReader::nextSizedString () const {
    size_t remaining = this->nextUInt32 ();
    std::string output;
    char buffer[4096];

    // Grow only after a successful read, so a declared length alone cannot drive allocation.
    while (remaining > 0) {
	const auto count = std::min (remaining, sizeof (buffer));
	this->next (buffer, count);
	output.append (buffer, count);
	remaining -= count;
    }
    return output;
}

void BinaryReader::next (char* out, const size_t size) const {
    if (!std::in_range<std::streamsize> (size)) {
	throw std::runtime_error ("Binary read length is too large");
    }
    if (!this->m_input->read (out, static_cast<std::streamsize> (size))) {
	throw std::runtime_error ("Incomplete binary stream read");
    }
}

char BinaryReader::next () const {
    char buffer;
    this->next (&buffer, 1);
    return buffer;
}

std::istream& BinaryReader::base () const { return *this->m_input; }
