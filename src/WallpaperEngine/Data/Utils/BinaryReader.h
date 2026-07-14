#pragma once

#include <cstdint>
#include <ios>
#include <iosfwd>
#include <memory>

namespace WallpaperEngine::Data::Utils {
using ReadStream = std::istream;
using ReadStreamSharedPtr = std::shared_ptr<ReadStream>;

class BinaryReader {
public:
    explicit BinaryReader (ReadStreamSharedPtr file);

    [[nodiscard]] uint32_t nextUInt32 () const;
    [[nodiscard]] int nextInt () const;
    [[nodiscard]] float nextFloat () const;
    [[nodiscard]] std::string nextNullTerminatedString () const;
    [[nodiscard]] std::string nextSizedString () const;
    void next (char* out, size_t size) const;
    [[nodiscard]] char next () const;

    /**
     * @return the number of bytes left between the current position and the end of the stream.
     *         Used to reject length/count fields from untrusted files that claim more data than
     *         the file actually contains before any allocation happens.
     */
    [[nodiscard]] std::streamsize remaining () const;

    [[nodiscard]] std::istream& base () const;

private:
    /**
     * Reads exactly @p size bytes into @p out, throwing if the stream ends early instead of
     * silently leaving the destination (partially) uninitialized.
     */
    void readExact (char* out, std::streamsize size) const;

    ReadStreamSharedPtr m_input;
};

using BinaryReaderUniquePtr = std::unique_ptr<BinaryReader>;
}
