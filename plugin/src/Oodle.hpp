#pragma once

#include <cstdint>
#include <string>
#include <vector>

// Oodle (Kraken) decompression, using the game's bin\x64\oo2ext_7_win64.dll
namespace Oodle
{
// Decompresses an archive segment (raw if zsize == size, otherwise KARK + size + compressed data)
bool DecompressSegment(const std::vector<uint8_t>& aStored, uint32_t aSize, std::vector<uint8_t>& aOut,
                       std::string& aError);
} // namespace Oodle
