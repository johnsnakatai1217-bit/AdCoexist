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

// Whether one mod's version of one ad is converted, and why
struct Decision
{
    bool convert = false;
    bool readError = false; // the mod's files couldn't be read (converted, as before)
    std::string reason;
};

// Converts only mods that change the picture: the mod replaces an image, or one of its .inkatlas and .inkwidget
// files references a file added by mods. Mods that only change how vanilla pictures are shown (quality, animation,
// layout fixes) aren't converted. aIsAdded tells whether a path hash is in a mod archive but not in the vanilla
// archives (references to files that exist nowhere are mistakes in the mod, not new pictures)
Decision Decide(const std::vector<const AdCatalog::File*>& aProvided, const FileReader& aReadMod,
                const std::function<bool(uint64_t)>& aIsAdded);

// Without the restore-vanilla option, one mod per ad keeps the original place instead of being converted.
// aAds has the ad of every conversion in load order, so it is the first one. Returns true for the conversions
// that keep the original place
std::vector<bool> ChooseOwners(const std::vector<const AdCatalog::Ad*>& aAds);

// Removes the files an owner provides from the files restored to vanilla, so the owner's versions stay in the
// original place
void KeepOwnerFiles(const std::vector<const AdCatalog::File*>& aProvided, std::unordered_set<uint64_t>& aRestore);

// Districts for ads that aren't in any district list (the 18 from the custom advertisements guide)
const std::vector<const char*>& AllDistricts();

bool Build(const Request& aRequest, const FileReader& aReadMod, const FileReader& aReadVanilla, Result& aResult,
           std::string& aError);
} // namespace Variant
