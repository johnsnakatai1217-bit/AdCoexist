#include "Archive.hpp"

#include <Windows.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <fstream>

#include "Util.hpp"

namespace
{
using Util::Get;
using Util::Set;

constexpr uint32_t Magic = 0x52414452; // "RDAR"
constexpr uint32_t Version = 12;
constexpr size_t HeaderAreaSize = 0xAC; // header (40) + custom data size (8) + zero padding
constexpr size_t PageSize = 0x1000;
constexpr size_t FileEntrySize = 56;
constexpr size_t SegmentEntrySize = 16;

template<typename T>
void Put(std::vector<uint8_t>& aOut, T aValue)
{
    const auto* bytes = reinterpret_cast<const uint8_t*>(&aValue);
    aOut.insert(aOut.end(), bytes, bytes + sizeof(T));
}

void PadToPage(std::vector<uint8_t>& aOut)
{
    aOut.resize((aOut.size() + PageSize - 1) / PageSize * PageSize, 0);
}

const std::array<uint64_t, 256>& Crc64Table()
{
    static const auto table = [] {
        std::array<uint64_t, 256> t{};
        for (uint64_t i = 0; i < 256; ++i)
        {
            uint64_t c = i;
            for (int k = 0; k < 8; ++k)
                c = (c & 1) ? (c >> 1) ^ 0xC96C5795D7870F42ull : c >> 1;
            t[i] = c;
        }
        return t;
    }();
    return table;
}
} // namespace

uint64_t Archive::Crc64(const uint8_t* aData, size_t aSize)
{
    const auto& table = Crc64Table();
    uint64_t crc = ~0ull;
    for (size_t i = 0; i < aSize; ++i)
        crc = (crc >> 8) ^ table[static_cast<uint8_t>(crc ^ aData[i])];
    return ~crc;
}

bool Archive::Read(const std::filesystem::path& aPath, std::vector<File>& aFiles, std::string& aError,
                   const std::unordered_set<uint64_t>* aOnly)
{
    std::ifstream in(aPath, std::ios::binary | std::ios::ate);
    if (!in)
    {
        aError = "cannot open";
        return false;
    }
    const auto fileSize = static_cast<uint64_t>(in.tellg());

    auto readAt = [&in](uint64_t aOffset, size_t aSize, std::vector<uint8_t>& aOut) {
        aOut.resize(aSize);
        in.seekg(static_cast<std::streamoff>(aOffset));
        in.read(reinterpret_cast<char*>(aOut.data()), static_cast<std::streamsize>(aSize));
        return static_cast<bool>(in);
    };

    std::vector<uint8_t> header;
    if (fileSize < HeaderAreaSize || !readAt(0, 40, header) || Get<uint32_t>(header, 0) != Magic)
    {
        aError = "not an RDAR archive";
        return false;
    }

    const auto indexPos = Get<uint64_t>(header, 8);
    const auto indexSize = Get<uint32_t>(header, 16);
    std::vector<uint8_t> index;
    if (indexSize < 28 || indexPos + indexSize > fileSize || !readAt(indexPos, indexSize, index))
    {
        aError = "index out of range";
        return false;
    }

    const auto fileCount = Get<uint32_t>(index, 16);
    const auto segmentCount = Get<uint32_t>(index, 20);
    const size_t fileBase = 28;
    const size_t segmentBase = fileBase + static_cast<size_t>(fileCount) * FileEntrySize;
    if (segmentBase + static_cast<size_t>(segmentCount) * SegmentEntrySize > index.size())
    {
        aError = "index tables out of range";
        return false;
    }

    aFiles.clear();
    for (uint32_t i = 0; i < fileCount; ++i)
    {
        const size_t o = fileBase + i * FileEntrySize;
        File file;
        file.hash = Get<uint64_t>(index, o);
        if (aOnly && !aOnly->contains(file.hash))
            continue;

        file.inlineBuffers = Get<uint32_t>(index, o + 16);
        const auto first = Get<uint32_t>(index, o + 20);
        const auto last = Get<uint32_t>(index, o + 24);
        for (uint32_t s = first; s < last && s < segmentCount; ++s)
        {
            const size_t so = segmentBase + s * SegmentEntrySize;
            const auto offset = Get<uint64_t>(index, so);
            const auto zsize = Get<uint32_t>(index, so + 8);
            Segment segment;
            segment.size = Get<uint32_t>(index, so + 12);
            if (offset + zsize > fileSize || !readAt(offset, zsize, segment.data))
            {
                aError = "segment out of range";
                return false;
            }
            file.segments.push_back(std::move(segment));
        }
        aFiles.push_back(std::move(file));
    }
    return true;
}

bool Archive::ReadHashes(const std::filesystem::path& aPath, std::vector<uint64_t>& aHashes, std::string& aError)
{
    std::ifstream in(aPath, std::ios::binary);
    if (!in)
    {
        aError = "cannot open";
        return false;
    }
    std::vector<uint8_t> header(40);
    in.read(reinterpret_cast<char*>(header.data()), 40);
    if (!in || Get<uint32_t>(header, 0) != Magic)
    {
        aError = "not an RDAR archive";
        return false;
    }
    const auto indexPos = Get<uint64_t>(header, 8);
    const auto indexSize = Get<uint32_t>(header, 16);
    std::vector<uint8_t> index(indexSize);
    in.seekg(static_cast<std::streamoff>(indexPos));
    in.read(reinterpret_cast<char*>(index.data()), indexSize);
    if (!in || indexSize < 28)
    {
        aError = "index out of range";
        return false;
    }
    const auto fileCount = Get<uint32_t>(index, 16);
    if (28 + static_cast<size_t>(fileCount) * FileEntrySize > index.size())
    {
        aError = "index tables out of range";
        return false;
    }
    aHashes.clear();
    for (uint32_t i = 0; i < fileCount; ++i)
        aHashes.push_back(Get<uint64_t>(index, 28 + i * FileEntrySize));
    return true;
}

bool Archive::Write(const std::filesystem::path& aPath, std::vector<File> aFiles, std::string& aError)
{
    std::sort(aFiles.begin(), aFiles.end(), [](const File& a, const File& b) { return a.hash < b.hash; });

    std::vector<uint8_t> out(HeaderAreaSize, 0);

    // Lay out the segment data and record the offsets
    struct SegmentEntry
    {
        uint64_t offset;
        uint32_t zsize;
        uint32_t size;
    };
    std::vector<SegmentEntry> segmentEntries;
    std::vector<std::pair<uint32_t, uint32_t>> ranges;
    for (const auto& file : aFiles)
    {
        const auto first = static_cast<uint32_t>(segmentEntries.size());
        for (const auto& segment : file.segments)
        {
            segmentEntries.push_back({out.size(), static_cast<uint32_t>(segment.data.size()), segment.size});
            out.insert(out.end(), segment.data.begin(), segment.data.end());
        }
        ranges.emplace_back(first, static_cast<uint32_t>(segmentEntries.size()));
    }
    PadToPage(out);

    // Index tables (from the file count to the dependencies)
    FILETIME now{};
    GetSystemTimeAsFileTime(&now);
    const uint64_t timestamp = (static_cast<uint64_t>(now.dwHighDateTime) << 32) | now.dwLowDateTime;

    std::vector<uint8_t> table;
    Put<uint32_t>(table, static_cast<uint32_t>(aFiles.size()));
    Put<uint32_t>(table, static_cast<uint32_t>(segmentEntries.size()));
    Put<uint32_t>(table, 0); // dependency count
    for (size_t i = 0; i < aFiles.size(); ++i)
    {
        Put<uint64_t>(table, aFiles[i].hash);
        Put<uint64_t>(table, timestamp);
        Put<uint32_t>(table, aFiles[i].inlineBuffers);
        Put<uint32_t>(table, ranges[i].first);
        Put<uint32_t>(table, ranges[i].second);
        Put<uint32_t>(table, 0); // dependency range
        Put<uint32_t>(table, 0);
        table.insert(table.end(), 20, 0); // SHA1 (apparently not used by the game)
    }
    for (const auto& segment : segmentEntries)
    {
        Put<uint64_t>(table, segment.offset);
        Put<uint32_t>(table, segment.zsize);
        Put<uint32_t>(table, segment.size);
    }

    const size_t indexPos = out.size();
    Put<uint32_t>(out, 8);
    Put<uint32_t>(out, static_cast<uint32_t>(table.size() + 8));
    Put<uint64_t>(out, Crc64(table.data(), table.size()));
    out.insert(out.end(), table.begin(), table.end());
    const size_t indexSize = out.size() - indexPos;
    PadToPage(out);

    // Header
    Set<uint32_t>(out, 0x00, Magic);
    Set<uint32_t>(out, 0x04, Version);
    Set<uint64_t>(out, 0x08, indexPos);
    Set<uint32_t>(out, 0x10, static_cast<uint32_t>(indexSize));
    Set<uint64_t>(out, 0x14, 0);
    Set<uint32_t>(out, 0x1C, 0);
    Set<uint64_t>(out, 0x20, out.size());
    Set<uint64_t>(out, 0x28, 0); // no custom data (LXRS)

    std::ofstream file(aPath, std::ios::binary | std::ios::trunc);
    if (!file)
    {
        aError = "cannot create";
        return false;
    }
    file.write(reinterpret_cast<const char*>(out.data()), static_cast<std::streamsize>(out.size()));
    if (!file)
    {
        aError = "write failed";
        return false;
    }
    return true;
}
