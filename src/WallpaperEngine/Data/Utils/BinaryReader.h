#pragma once

#include <cstdint>
#include <iosfwd>

namespace WallpaperEngine::Data::Utils {
class BinaryReader {
  public:
    explicit BinaryReader (std::istream& file);

    uint32_t nextUInt32 ();
    int nextInt ();
    float nextFloat ();
    std::string nextNullTerminatedString ();
    void next(char* out, size_t size);
    char next ();

  private:
    std::istream& m_input;
};
}
