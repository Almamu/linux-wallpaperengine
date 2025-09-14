#pragma once

#include <iostream>

namespace WallpaperEngine::Data::Utils {
struct MemoryStream : std::istream, private std::streambuf
{
    MemoryStream (std::unique_ptr <char[]> buffer, const size_t size) :
        std::istream (this),
        m_buffer (std::move (buffer))
    {
        this->setg(
            this->m_buffer.get (),
            this->m_buffer.get (),
            this->m_buffer.get () + size
        );
    }

    std::unique_ptr<char[]> m_buffer;
};

using MemoryStreamSharedPtr = std::shared_ptr<MemoryStream>;
}