#pragma once

#include <cstdint>
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
    void next(char* out, size_t size) const;
    [[nodiscard]] char next () const;

    [[nodiscard]] std::istream& base () const;

  private:
    ReadStreamSharedPtr m_input;
};

using BinaryReaderUniquePtr = std::unique_ptr<BinaryReader>;
}
