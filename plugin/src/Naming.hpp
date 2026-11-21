#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <vector>

// Naming rules for paths and records. Independent of the game and RED4ext.SDK, so tests can call them directly
namespace Naming
{
// Path hash (FNV-1a 64 after the same normalization as the game's ResourcePath)
uint64_t PathHash(std::string_view aPath);

// Makes a stable 16-digit ID from the archive path relative to the game directory
std::string ArchiveId(std::string_view aRelativePath);

// New path: replaces the known prefix with "adcoexist\<16 hex digits>..." of the same length. Empty if no rule matches
std::string NewPath(std::string_view aOriginal, std::string_view aId);

// The archive file name (without extension) used in record names, reduced to letters, digits and _
std::string RecordStem(std::string_view aArchiveFileStem);

struct ArchiveName
{
    std::string relative; // relative to the game directory
    std::string stem;     // result of RecordStem
};

// Relative path -> name used in record names. If several archives have the same stem, all of them get an ID suffix
std::map<std::string, std::string> AssignRecordStems(const std::vector<ArchiveName>& aArchives);

// Record name of an additional ad: Adverts.adcoexist_<name>_<original record name without Adverts.>
std::string VariantRecordName(std::string_view aStem, std::string_view aOriginalRecord);
} // namespace Naming
