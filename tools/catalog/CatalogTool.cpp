// Development tool that builds the ad catalog. It only reads the game directory and never writes to it.
//
// Usage:
//   AdCoexistCatalogTool --game <game dir> --usedhashes <usedhashes.kark> --tweakdbstr <tweakdbstr.kark>
//                        --game-version <e.g. 2.50> --wolvenkit-ref <commit> --out <AdCatalog.inc> --report <report.txt>
//
// Inputs:
//   WolvenKit name lists (usedhashes.kark: paths, tweakdbstr.kark: TweakDB names)
//   the game's r6\cache\tweakdb.bin and tweakdb_ep1.bin (record resources and district lists)
//   the indexes of the archives in archive\pc\content and ep1 (to check that the files exist)
#include <Windows.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "Archive.hpp"
#include "Cr2w.hpp"
#include "Naming.hpp"
#include "Oodle.hpp"
#include "TweakDbFile.hpp"
#include "Util.hpp"

namespace fs = std::filesystem;

namespace
{
constexpr std::string_view AdvertFolders[] = {
    "base\\gameplay\\gui\\world\\adverts\\",
    "ep1\\gameplay\\gui\\world\\adverts\\",
};
constexpr std::string_view DistrictPrefix = "Adverts.";
constexpr std::string_view DistrictSuffix = ".advertisements";
constexpr uint64_t TweakDbKeyMask = 0xFFFFFFFFFFull;

using Util::EndsWith;
using Util::Get;
using Util::ReadVlq;

std::vector<uint8_t> ReadFile(const fs::path& aPath)
{
    std::ifstream in(aPath, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

// Low 40 bits of a TweakDBID used for matching (CRC32 of the name and its length)
uint64_t TweakDbKey(std::string_view aName)
{
    const auto crc = Cr2w::Crc32(reinterpret_cast<const uint8_t*>(aName.data()), aName.size());
    return (static_cast<uint64_t>(aName.size() & 0xFF) << 32) | crc;
}

// Decompresses a WolvenKit .kark file (KARK + size + Oodle)
bool Decompress(const fs::path& aPath, std::vector<uint8_t>& aOut)
{
    const auto stored = ReadFile(aPath);
    if (stored.size() < 8)
    {
        std::fprintf(stderr, "%s: too small\n", aPath.string().c_str());
        return false;
    }
    std::string error;
    if (!Oodle::DecompressSegment(stored, Get<uint32_t>(stored, 4), aOut, error))
    {
        std::fprintf(stderr, "%s: %s\n", aPath.string().c_str(), error.c_str());
        return false;
    }
    return true;
}

// usedhashes: paths separated by newlines
std::vector<std::string> ParseLines(const std::vector<uint8_t>& aData)
{
    std::vector<std::string> lines;
    std::string line;
    for (uint8_t c : aData)
    {
        if (c == '\n' || c == '\r' || c == 0)
        {
            if (!line.empty())
                lines.push_back(std::move(line));
            line.clear();
        }
        else
        {
            line += static_cast<char>(c);
        }
    }
    if (!line.empty())
        lines.push_back(line);
    return lines;
}

// tweakdbstr: a 20-byte header, then length (variable-length integer) + string
std::vector<std::string> ParseTweakDbNames(const std::vector<uint8_t>& aData)
{
    std::vector<std::string> names;
    size_t pos = 20;
    while (pos < aData.size())
    {
        // In the game's string format, a negative length means one byte per character. All names use this form
        const auto signedLength = ReadVlq(aData, pos);
        const auto length = static_cast<size_t>(signedLength < 0 ? -signedLength : signedLength);
        if (pos + length > aData.size())
        {
            std::fprintf(stderr, "tweakdbstr: entry %zu overruns (pos=%zu length=%zu size=%zu)\n", names.size(), pos,
                         length, aData.size());
            break;
        }
        names.emplace_back(reinterpret_cast<const char*>(aData.data() + pos), length);
        pos += length;
    }
    return names;
}

// Reads the sections of the two needed types from the flats in tweakdb.bin
struct TweakDb
{
    uint32_t checksum = 0;
    std::unordered_map<uint64_t, uint64_t> resources;             // flat ID -> path hash
    std::unordered_map<uint64_t, std::vector<uint64_t>> idArrays; // flat ID -> array of TweakDBIDs
};

bool ReadTweakDb(const fs::path& aPath, TweakDb& aDb)
{
    const auto data = ReadFile(aPath);
    if (data.size() < 32 || Get<uint32_t>(data, 0) != TweakDbFile::Magic)
    {
        std::fprintf(stderr, "%s: not a tweakdb\n", aPath.string().c_str());
        return false;
    }
    if (Get<int32_t>(data, 4) != TweakDbFile::BlobVersion || Get<int32_t>(data, 8) != TweakDbFile::ParserVersion)
    {
        std::fprintf(stderr, "%s: unsupported version %d/%d\n", aPath.string().c_str(), Get<int32_t>(data, 4),
                     Get<int32_t>(data, 8));
        return false;
    }
    aDb.checksum = Get<uint32_t>(data, 12);

    const auto arrayType = Util::Fnv1a64("array:TweakDBID");
    const auto resourceType = Util::Fnv1a64("raRef:CResource");
    size_t pos = static_cast<size_t>(Get<int32_t>(data, 16));
    const auto typeCount = Get<int32_t>(data, pos);
    pos += 4;
    for (int32_t t = 0; t < typeCount; ++t, pos += 20)
    {
        const auto typeHash = Get<uint64_t>(data, pos);
        if (typeHash != arrayType && typeHash != resourceType)
            continue;

        size_t p = Get<uint32_t>(data, pos + 16);
        const auto valueCount = Get<uint32_t>(data, p);
        p += 4;
        std::vector<std::vector<uint64_t>> arrays;
        std::vector<uint64_t> resources;
        for (uint32_t v = 0; v < valueCount; ++v)
        {
            if (typeHash == resourceType)
            {
                resources.push_back(Get<uint64_t>(data, p));
                p += 8;
            }
            else
            {
                const auto count = ReadVlq(data, p);
                std::vector<uint64_t> ids;
                for (int64_t i = 0; i < count; ++i, p += 8)
                    ids.push_back(Get<uint64_t>(data, p) & TweakDbKeyMask);
                arrays.push_back(std::move(ids));
            }
        }
        const auto keyCount = Get<uint32_t>(data, p);
        p += 4;
        for (uint32_t k = 0; k < keyCount; ++k, p += 12)
        {
            const auto key = Get<uint64_t>(data, p) & TweakDbKeyMask;
            const auto index = Get<int32_t>(data, p + 8);
            if (typeHash == resourceType)
                aDb.resources[key] = resources[index];
            else
                aDb.idArrays[key] = arrays[index];
        }
    }
    return true;
}

const char* KindOf(std::string_view aPath)
{
    if (EndsWith(aPath, ".xbm"))
        return "Image";
    if (EndsWith(aPath, ".inkatlas"))
        return "Atlas";
    if (EndsWith(aPath, ".inkwidget"))
        return "Widget";
    if (EndsWith(aPath, ".inkanim"))
        return "Animation";
    return "Other";
}

std::string CString(std::string_view aText)
{
    std::string out = "\"";
    for (char c : aText)
    {
        if (c == '\\' || c == '"')
            out += '\\';
        out += c;
    }
    return out + "\"";
}

struct RecordInfo
{
    std::string name;
    std::string widget;
    std::vector<std::string> districts;
};

struct AdInfo
{
    std::string folder;
    std::vector<std::string> files;
    std::vector<RecordInfo> records;
};
} // namespace

int Run(int argc, char** argv);

int main(int argc, char** argv)
{
    try
    {
        return Run(argc, argv);
    }
    catch (const std::exception& e)
    {
        std::fprintf(stderr, "exception: %s\n", e.what());
        return 1;
    }
}

int Run(int argc, char** argv)
{
    std::map<std::string, std::string> args;
    for (int i = 1; i + 1 < argc; i += 2)
        args[argv[i]] = argv[i + 1];
    for (const char* required :
         {"--game", "--usedhashes", "--tweakdbstr", "--game-version", "--wolvenkit-ref", "--out", "--report"})
    {
        if (!args.contains(required))
        {
            std::fprintf(stderr, "missing %s\n", required);
            return 2;
        }
    }
    const fs::path gameDir = args["--game"];

    // Oodle comes from the game's DLL (only loaded). Oodle.cpp looks for an already loaded one
    if (!LoadLibraryW((gameDir / L"bin" / L"x64" / L"oo2ext_7_win64.dll").c_str()))
    {
        std::fprintf(stderr, "cannot load oo2ext_7_win64.dll\n");
        return 1;
    }

    // 1. Ad file paths (by folder)
    std::fprintf(stderr, "stage 1\n");
    std::vector<uint8_t> raw;
    if (!Decompress(args["--usedhashes"], raw))
        return 1;
    std::map<std::string, AdInfo> ads; // folder -> ad
    std::unordered_map<uint64_t, std::string> pathOfHash;
    for (auto& path : ParseLines(raw))
    {
        for (const auto folderRoot : AdvertFolders)
        {
            if (path.rfind(folderRoot, 0) != 0)
                continue;
            const auto rest = std::string_view(path).substr(folderRoot.size());
            const auto slash = rest.find('\\');
            if (slash == std::string_view::npos)
                continue; // files outside an ad folder are skipped
            const auto folder = path.substr(0, folderRoot.size() + slash + 1);
            pathOfHash[Naming::PathHash(path)] = path;
            ads[folder].folder = folder;
            ads[folder].files.push_back(path);
        }
    }

    // 2. TweakDB names: ad record candidates and district lists
    std::fprintf(stderr, "stage 2\n");
    if (!Decompress(args["--tweakdbstr"], raw))
        return 1;
    std::vector<std::string> recordNames;
    std::vector<std::string> districtNames;
    for (auto& name : ParseTweakDbNames(raw))
    {
        if (name.rfind(DistrictPrefix, 0) != 0)
            continue;
        if (EndsWith(name, DistrictSuffix) &&
            std::count(name.begin(), name.end(), '.') == 2)
            districtNames.push_back(name);
        else if (std::count(name.begin(), name.end(), '.') == 1)
            recordNames.push_back(name);
    }

    // 3. tweakdb: look up record resources and districts in the Phantom Liberty version (which includes the base game ads)
    std::fprintf(stderr, "stage 3\n");
    TweakDb tweakDb;
    const auto cacheDir = gameDir / L"r6" / L"cache";
    if (!ReadTweakDb(cacheDir / L"tweakdb_ep1.bin", tweakDb))
        return 1;
    const auto baseChecksum = TweakDbFile::ReadChecksum(cacheDir / L"tweakdb.bin");

    std::unordered_map<uint64_t, std::vector<std::string>> districtsOfRecord;
    for (const auto& district : districtNames)
    {
        const auto it = tweakDb.idArrays.find(TweakDbKey(district));
        if (it == tweakDb.idArrays.end())
            continue;
        for (auto id : it->second)
            districtsOfRecord[id].push_back(district);
    }

    size_t recordsOutside = 0;
    for (const auto& name : recordNames)
    {
        const auto res = tweakDb.resources.find(TweakDbKey(name + ".resource"));
        if (res == tweakDb.resources.end())
            continue; // no resource (not an ad record)
        const auto widget = pathOfHash.find(res->second);
        if (widget == pathOfHash.end() || !EndsWith(widget->second, ".inkwidget"))
        {
            ++recordsOutside; // points outside the ad folders (not a billboard ad)
            continue;
        }
        const auto folder = widget->second.substr(0, widget->second.rfind('\\') + 1);
        RecordInfo record{name, widget->second, districtsOfRecord[TweakDbKey(name)]};
        std::sort(record.districts.begin(), record.districts.end());
        ads[folder].records.push_back(std::move(record));
    }

    // 4. Check that the files exist in the vanilla archives
    std::fprintf(stderr, "stage 4\n");
    std::unordered_set<uint64_t> vanilla;
    for (const auto* dir : {L"content", L"ep1"})
    {
        std::error_code ec;
        for (const auto& entry : fs::directory_iterator(gameDir / L"archive" / L"pc" / dir, ec))
        {
            if (entry.path().extension() != L".archive")
                continue;
            std::vector<uint64_t> hashes;
            std::string error;
            if (Archive::ReadHashes(entry.path(), hashes, error))
                vanilla.insert(hashes.begin(), hashes.end());
        }
    }

    // 5. Output (only ads that have records)
    std::fprintf(stderr, "stage 5\n");
    std::ostringstream inc;
    std::ostringstream report;
    inc << "// Generated file. Rebuild it with tools/catalog/build-catalog.ps1 instead of editing it.\n";
    inc << "// Game " << args["--game-version"] << ", WolvenKit name lists " << args["--wolvenkit-ref"] << "\n\n";
    size_t adCount = 0, fileCount = 0, missingFiles = 0, recordCount = 0, noDistrict = 0;
    std::ostringstream adTable;
    for (auto& [folder, ad] : ads)
    {
        if (ad.records.empty())
        {
            report << "no record: " << folder << "\n";
            continue;
        }
        std::sort(ad.files.begin(), ad.files.end());
        std::sort(ad.records.begin(), ad.records.end(),
                  [](const RecordInfo& a, const RecordInfo& b) { return a.name < b.name; });
        const auto id = std::to_string(adCount);

        inc << "static const AdCatalog::File Files" << id << "[] = {\n";
        size_t kept = 0;
        for (const auto& path : ad.files)
        {
            if (!vanilla.contains(Naming::PathHash(path)))
            {
                ++missingFiles;
                report << "missing in vanilla archives: " << path << "\n";
                continue;
            }
            inc << "    {" << CString(path) << ", AdCatalog::FileKind::" << KindOf(path) << "},\n";
            ++kept;
        }
        inc << "};\n";
        fileCount += kept;

        for (size_t r = 0; r < ad.records.size(); ++r)
        {
            const auto& record = ad.records[r];
            inc << "static const char* const Districts" << id << "_" << r << "[] = {";
            for (const auto& d : record.districts)
                inc << CString(d) << ", ";
            if (record.districts.empty())
            {
                inc << "nullptr";
                ++noDistrict;
                report << "no district: " << record.name << "\n";
            }
            inc << "};\n";
        }
        inc << "static const AdCatalog::Record Records" << id << "[] = {\n";
        for (size_t r = 0; r < ad.records.size(); ++r)
        {
            const auto& record = ad.records[r];
            inc << "    {" << CString(record.name) << ", " << CString(record.widget) << ", Districts" << id << "_" << r
                << ", " << record.districts.size() << "},\n";
        }
        inc << "};\n\n";
        recordCount += ad.records.size();

        adTable << "    {" << CString(folder) << ", Files" << id << ", " << kept << ", Records" << id << ", "
                << ad.records.size() << "},\n";
        ++adCount;
    }
    inc << "static const AdCatalog::Ad Ads[] = {\n" << adTable.str() << "};\n\n";
    inc << "static const AdCatalog::Catalog CatalogData = {" << CString(args["--game-version"]) << ", 0x" << std::hex
        << baseChecksum << ", 0x" << tweakDb.checksum << std::dec << ", Ads, " << adCount << "};\n";

    report << "\nads=" << adCount << " files=" << fileCount << " records=" << recordCount
           << " districtLists=" << districtNames.size() << " recordsWithoutDistrict=" << noDistrict
           << " filesMissingInVanilla=" << missingFiles << " recordsOutsideAdvertFolders=" << recordsOutside << "\n";
    report << std::hex << "tweakdb.bin checksum=0x" << baseChecksum << " tweakdb_ep1.bin checksum=0x"
           << tweakDb.checksum << "\n";

    std::ofstream(args["--out"], std::ios::binary) << inc.str();
    std::ofstream(args["--report"], std::ios::binary) << report.str();
    std::printf("ads=%zu files=%zu records=%zu (see %s)\n", adCount, fileCount, recordCount,
                args["--report"].c_str());
    return 0;
}
