#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "AdCatalog.hpp"
#include "Archive.hpp"

// Converts one ad of one replacer mod into an additional ad.
// File reading is passed in by the caller as functions, so it can be tested without the game
namespace Variant
{
// Reads the files with the given hashes. Returns false if any is missing
using FileReader =
    std::function<bool(const std::unordered_set<uint64_t>& aHashes, std::unordered_map<uint64_t, Archive::File>& aOut)>;

struct Request
{
    const AdCatalog::Ad* ad = nullptr;
    std::vector<const AdCatalog::File*> provided; // catalog files the mod has (images, .inkatlas, .inkwidget)
    std::string archiveId;                        // 16-digit ID used in the new paths
    std::string recordStem;                       // name used in the record names
};

struct Result
{
    std::vector<Archive::File> files;    // files at the new paths (go into the generated archive)
    std::unordered_set<uint64_t> restore; // files restored to vanilla at the original paths
    std::string yaml;                     // records and district entries added to TweakDB (YAML)
    std::vector<std::string> recordNames;
};

// Whether a catalog file kind is converted
bool IsConvertible(AdCatalog::FileKind aKind);

// Districts for ads that aren't in any district list (the 18 from the custom advertisements guide)
const std::vector<const char*>& AllDistricts();

bool Build(const Request& aRequest, const FileReader& aReadMod, const FileReader& aReadVanilla, Result& aResult,
           std::string& aError);
} // namespace Variant
