#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_set>
#include <vector>

// Reading and writing .archive files (RDAR version 12)
namespace Archive
{
struct Segment
{
    uint32_t size = 0;          // uncompressed size
    std::vector<uint8_t> data;  // stored bytes (zsize == data.size())
};

struct File
{
    uint64_t hash = 0;
    uint32_t inlineBuffers = 0;
    std::vector<Segment> segments;
};

// Reads an archive and returns the file segments as stored (still compressed).
// With aOnly, only the files with those hashes are read (only the index and their segments, so it works for large archives)
bool Read(const std::filesystem::path& aPath, std::vector<File>& aFiles, std::string& aError,
          const std::unordered_set<uint64_t>* aOnly = nullptr);

// Reads only the file hashes from the index (no data)
bool ReadHashes(const std::filesystem::path& aPath, std::vector<uint64_t>& aHashes, std::string& aError);

// Writes the files as an archive. The caller must avoid duplicate hashes
bool Write(const std::filesystem::path& aPath, std::vector<File> aFiles, std::string& aError);

// CRC-64 (index checksum)
uint64_t Crc64(const uint8_t* aData, size_t aSize);
} // namespace Archive
