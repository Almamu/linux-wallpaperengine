#include <iostream>
#include <bit>

#include "BinaryReader.h"

#include <cstring>

using namespace WallpaperEngine::Data::Utils;

BinaryReader::BinaryReader (std::istream& file) : m_input (file) { }

uint32_t BinaryReader::nextUInt32 () {
    char buffer[4];

    this->m_input.read (buffer, 4);

    if constexpr (std::endian::native == std::endian::little) {
        return (buffer [3] & 0xFF) << 24 |
               (buffer [2] & 0xFF) << 16 |
               (buffer [1] & 0xFF) << 8 |
               (buffer [0] & 0xFF);
    } else {
        return (buffer [0] & 0xFF) << 24 |
               (buffer [1] & 0xFF) << 16 |
               (buffer [2] & 0xFF) << 8 |
               (buffer [3] & 0xFF);
    }
}

int BinaryReader::nextInt () {
    char buffer[4];

    this->m_input.read (buffer, 4);

    if constexpr (std::endian::native == std::endian::little) {
        return (buffer [3] & 0xFF) << 24 |
               (buffer [2] & 0xFF) << 16 |
               (buffer [1] & 0xFF) << 8 |
               (buffer [0] & 0xFF);
    } else {
        return (buffer [0] & 0xFF) << 24 |
               (buffer [1] & 0xFF) << 16 |
               (buffer [2] & 0xFF) << 8 |
               (buffer [3] & 0xFF);
    }
}

float BinaryReader::nextFloat () {
    float result;
    static_assert (std::endian::native == std::endian::little, "Only little endian is supported for floats");

    this->m_input.read (reinterpret_cast<char*>(&result), sizeof (result));

    return result;
}


std::string BinaryReader::nextNullTerminatedString () {
    std::string output;

    while (const auto c = this->next ()) {
        output += c;
    }

    return output;
}

void BinaryReader::next (char* out, size_t size) {
    this->m_input.read (out, size);
}

char BinaryReader::next () {
    char buffer;
    this->m_input.read (&buffer, 1);
    return buffer;
}

