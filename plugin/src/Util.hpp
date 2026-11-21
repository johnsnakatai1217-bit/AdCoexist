#pragma once

#include <cctype>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

// Small helpers shared by the plugin, the tools and the tests
namespace Util
{
// Read and write a value at an offset in a byte buffer (little endian)
template<typename T>
T Get(const std::vector<uint8_t>& aData, size_t aOffset)
{
    T value{};
    std::memcpy(&value, aData.data() + aOffset, sizeof(T));
    return value;
}

template<typename T>
void Set(std::vector<uint8_t>& aData, size_t aOffset, T aValue)
{
    std::memcpy(aData.data() + aOffset, &aValue, sizeof(T));
}

// FNV-1a 64 (no normalization). Use Naming::PathHash for path hashes
inline uint64_t Fnv1a64(std::string_view aText)
{
    uint64_t hash = 0xCBF29CE484222325ull;
    for (unsigned char c : aText)
    {
        hash ^= c;
        hash *= 0x100000001B3ull;
    }
    return hash;
}

// Variable-length integer used in game files (first byte: bit 7 sign, bit 6 continue, low 6 bits; then: bit 7 continue, low 7 bits)
inline int64_t ReadVlq(const std::vector<uint8_t>& aData, size_t& aPos)
{
    uint8_t c = aData[aPos++];
    const bool negative = (c & 0x80) != 0;
    int64_t value = c & 0x3F;
    int shift = 6;
    bool more = (c & 0x40) != 0;
    while (more)
    {
        c = aData[aPos++];
        value |= static_cast<int64_t>(c & 0x7F) << shift;
        shift += 7;
        more = (c & 0x80) != 0;
    }
    return negative ? -value : value;
}

inline std::string Lower(std::string_view aText)
{
    std::string out;
    for (char c : aText)
        out += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return out;
}

inline bool EndsWith(std::string_view aText, std::string_view aSuffix)
{
    return aText.size() >= aSuffix.size() && aText.substr(aText.size() - aSuffix.size()) == aSuffix;
}
} // namespace Util
