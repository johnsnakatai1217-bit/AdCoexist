#pragma once

#include <cstdint>
#include <filesystem>
#include <fstream>

// Constants of the game's tweakdb.bin format and reading its header
namespace TweakDbFile
{
constexpr uint32_t Magic = 0x0BB1DB47;
constexpr int32_t BlobVersion = 8;   // game 2.50
constexpr int32_t ParserVersion = 4; // game 2.50

// Reads the record checksum from the header. Returns 0 if it can't be read
inline uint32_t ReadChecksum(const std::filesystem::path& aPath)
{
    std::ifstream in(aPath, std::ios::binary);
    uint32_t header[4]{};
    in.read(reinterpret_cast<char*>(header), sizeof(header));
    return in && header[0] == Magic ? header[3] : 0;
}
} // namespace TweakDbFile
