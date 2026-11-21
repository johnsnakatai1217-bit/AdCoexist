#include "Oodle.hpp"

#include <Windows.h>

#include <cstring>
#include <filesystem>

namespace
{
constexpr uint32_t Kark = 0x4B52414B; // "KARK"

using Decompress_t = int64_t(__stdcall*)(const void*, int64_t, void*, int64_t, int, int, int, void*, int64_t, void*,
                                         void*, void*, int64_t, int);

Decompress_t GetDecompress()
{
    static const Decompress_t func = [] {
        // Use the DLL if the game already loaded it, otherwise load it from the exe's directory
        HMODULE module = GetModuleHandleW(L"oo2ext_7_win64.dll");
        if (!module)
        {
            wchar_t exe[MAX_PATH]{};
            GetModuleFileNameW(nullptr, exe, MAX_PATH);
            const auto dll = std::filesystem::path(exe).parent_path() / L"oo2ext_7_win64.dll";
            module = LoadLibraryW(dll.c_str());
        }
        return module ? reinterpret_cast<Decompress_t>(GetProcAddress(module, "OodleLZ_Decompress")) : nullptr;
    }();
    return func;
}
} // namespace

bool Oodle::DecompressSegment(const std::vector<uint8_t>& aStored, uint32_t aSize, std::vector<uint8_t>& aOut,
                              std::string& aError)
{
    if (aStored.size() == aSize)
    {
        aOut = aStored;
        return true;
    }
    if (aStored.size() < 8 || std::memcmp(aStored.data(), &Kark, 4) != 0)
    {
        aError = "not a KARK segment";
        return false;
    }
    const auto decompress = GetDecompress();
    if (!decompress)
    {
        aError = "OodleLZ_Decompress not available";
        return false;
    }
    aOut.resize(aSize);
    const auto got = decompress(aStored.data() + 8, static_cast<int64_t>(aStored.size() - 8), aOut.data(), aSize, 1, 0,
                                0, nullptr, 0, nullptr, nullptr, nullptr, 0, 3);
    if (got != aSize)
    {
        aError = "decompressed size mismatch";
        return false;
    }
    return true;
}
