#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

// Builds the content of the manifest file (variants.manifest) and decides whether to reuse the previous result.
// Reading and writing the file is up to the caller
namespace Manifest
{
struct ModStat
{
    std::string relative; // relative to the game directory
    uint64_t size = 0;
    int64_t time = 0; // last write time (file_time_type value)
};

struct Inputs
{
    int versionMajor = 0;
    int versionMinor = 0;
    int versionPatch = 0;
    std::string catalogGameVersion;
    uint32_t catalogTweakDbChecksum = 0;
    uint32_t catalogTweakDbEp1Checksum = 0;
    uint32_t tweakDbChecksum = 0;
    std::vector<ModStat> mods; // in load order
};

struct Outputs
{
    bool empty = false; // there was nothing to convert
    uint64_t archiveSize = 0;
    uint64_t yamlSize = 0;
};

enum class Reuse
{
    No,
    Generated, // use the previously generated files
    Nothing,   // there was nothing to convert last time either
};

// Turns the current inputs into the first part of the manifest
std::string Fingerprint(const Inputs& aInputs);

// The whole manifest (inputs + separator + sizes of the generated files)
std::string Compose(std::string_view aFingerprint, const Outputs& aOutputs);

// Decides whether to reuse the previous result from the manifest, the current inputs and the existing files
Reuse Decide(std::string_view aManifest, std::string_view aFingerprint, bool aGeneratedExists,
             const Outputs& aCurrentOutputs);
} // namespace Manifest
