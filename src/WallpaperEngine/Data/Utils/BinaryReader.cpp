#include <iostream>

#include "BinaryReader.h"

#include <cstring>

using namespace WallpaperEngine::Data::Utils;

BinaryReader::BinaryReader (std::istream& file) : m_input (file) { }

uint32_t BinaryReader::nextUInt32 () {
    char buffer[4];

    this->m_input.read (buffer, 4);

    return (buffer [3] & 0xFF) << 24 |
           (buffer [2] & 0xFF) << 16 |
           (buffer [1] & 0xFF) << 8 |
           (buffer [0] & 0xFF);
}

int BinaryReader::nextInt () {
    char buffer[4];

    this->m_input.read (buffer, 4);

    return (buffer [3] & 0xFF) << 24 |
           (buffer [2] & 0xFF) << 16 |
           (buffer [1] & 0xFF) << 8 |
           (buffer [0] & 0xFF);
}


float BinaryReader::nextFloat () {
    float result {};
    uint32_t bytes = this->nextUInt32 ();

    memcpy (&result, &bytes, sizeof (result));

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

