#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

// Replaces paths in a CR2W file with other paths of the same length.
// Offsets don't change, so only the checksums (string table, buffer table, header) and the buffer table are updated.
namespace Cr2w
{
using Replacement = std::pair<std::string, std::string>; // original path, new path (same length)

// aMain: the CR2W data (decompressed). aBuffers: the buffers (decompressed, in buffer table order).
// The buffer table is rebuilt assuming the buffers are stored uncompressed afterwards.
// Returns the number of replaced paths, or -1 on failure
int ReplacePaths(std::vector<uint8_t>& aMain, std::vector<std::vector<uint8_t>>& aBuffers,
                 const std::vector<Replacement>& aReplacements, std::string& aError);

uint32_t Crc32(const uint8_t* aData, size_t aSize);
} // namespace Cr2w
