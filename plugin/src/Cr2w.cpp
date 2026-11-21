#include "Cr2w.hpp"

#include <algorithm>
#include <array>
#include <cstring>

#include "Util.hpp"

namespace
{
constexpr uint32_t Magic = 0x57325243; // "CR2W"
constexpr uint32_t DeadBeef = 0xDEADBEEF;
constexpr size_t HeaderSize = 40;   // magic (4) + file header (36)
constexpr size_t TableCount = 10;
constexpr size_t TableSize = 12;    // offset, itemCount, crc32
constexpr size_t BufferInfoSize = 24; // flags, index, offset, diskSize, memSize, crc32
constexpr size_t StringsTable = 0;
constexpr size_t BuffersTable = 5;

uint32_t Read32(const std::vector<uint8_t>& aData, size_t aOffset)
{
    return Util::Get<uint32_t>(aData, aOffset);
}

void Write32(std::vector<uint8_t>& aData, size_t aOffset, uint32_t aValue)
{
    Util::Set<uint32_t>(aData, aOffset, aValue);
}

int ReplaceAll(std::vector<uint8_t>& aData, const std::vector<Cr2w::Replacement>& aReplacements)
{
    int count = 0;
    for (const auto& [from, to] : aReplacements)
    {
        auto it = aData.begin();
        while ((it = std::search(it, aData.end(), from.begin(), from.end())) != aData.end())
        {
            std::copy(to.begin(), to.end(), it);
            it += static_cast<std::ptrdiff_t>(to.size());
            ++count;
        }
    }
    return count;
}

size_t TableOffset(size_t aIndex)
{
    return HeaderSize + aIndex * TableSize;
}
} // namespace

uint32_t Cr2w::Crc32(const uint8_t* aData, size_t aSize)
{
    static const auto table = [] {
        std::array<uint32_t, 256> t{};
        for (uint32_t i = 0; i < 256; ++i)
        {
            uint32_t c = i;
            for (int k = 0; k < 8; ++k)
                c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            t[i] = c;
        }
        return t;
    }();
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < aSize; ++i)
        crc = table[(crc ^ aData[i]) & 0xFF] ^ (crc >> 8);
    return crc ^ 0xFFFFFFFFu;
}

int Cr2w::ReplacePaths(std::vector<uint8_t>& aMain, std::vector<std::vector<uint8_t>>& aBuffers,
                       const std::vector<Replacement>& aReplacements, std::string& aError)
{
    for (const auto& [from, to] : aReplacements)
    {
        if (from.size() != to.size())
        {
            aError = "replacement length differs: " + from;
            return -1;
        }
    }
    if (aMain.size() < HeaderSize + TableCount * TableSize || Read32(aMain, 0) != Magic)
    {
        aError = "not a CR2W file";
        return -1;
    }

    const uint32_t objectsEnd = Read32(aMain, 24);
    const uint32_t stringsOffset = Read32(aMain, TableOffset(StringsTable));
    const uint32_t stringsSize = Read32(aMain, TableOffset(StringsTable) + 4);
    const uint32_t bufferTableOffset = Read32(aMain, TableOffset(BuffersTable));
    const uint32_t bufferCount = Read32(aMain, TableOffset(BuffersTable) + 4);
    if (stringsOffset + stringsSize > aMain.size() || bufferTableOffset + bufferCount * BufferInfoSize > aMain.size() ||
        bufferCount != aBuffers.size())
    {
        aError = "unexpected table layout";
        return -1;
    }

    // Replace the paths in the main data and the buffers (both in the string table and in the object data)
    int count = ReplaceAll(aMain, aReplacements);
    for (auto& buffer : aBuffers)
        count += ReplaceAll(buffer, aReplacements);

    // String table checksum
    Write32(aMain, TableOffset(StringsTable) + 8, Crc32(aMain.data() + stringsOffset, stringsSize));

    // Buffer table: stored uncompressed, so diskSize = memSize = size, and the checksum is of the stored bytes
    uint32_t position = objectsEnd;
    for (uint32_t i = 0; i < bufferCount; ++i)
    {
        const size_t entry = bufferTableOffset + i * BufferInfoSize;
        const auto size = static_cast<uint32_t>(aBuffers[i].size());
        Write32(aMain, entry + 8, position);
        Write32(aMain, entry + 12, size);
        Write32(aMain, entry + 16, size);
        Write32(aMain, entry + 20, Crc32(aBuffers[i].data(), size));
        position += size;
    }
    if (bufferCount > 0)
    {
        Write32(aMain, TableOffset(BuffersTable) + 8,
                Crc32(aMain.data() + bufferTableOffset, bufferCount * BufferInfoSize));
    }
    Write32(aMain, 28, position); // buffersEnd

    // Header checksum: magic, version, flags, timeStamp, buildVersion, objectsEnd, buffersEnd, 0xDEADBEEF,
    // numChunks, 10 tables
    std::vector<uint8_t> header;
    auto put = [&header](const void* aData, size_t aSize) {
        const auto* bytes = static_cast<const uint8_t*>(aData);
        header.insert(header.end(), bytes, bytes + aSize);
    };
    put(aMain.data(), 32); // magic to buffersEnd
    put(&DeadBeef, 4);
    put(aMain.data() + 36, 4); // numChunks
    put(aMain.data() + HeaderSize, TableCount * TableSize);
    Write32(aMain, 32, Crc32(header.data(), header.size()));

    return count;
}
