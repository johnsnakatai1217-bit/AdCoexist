#include "Manifest.hpp"

#include <cstdio>

namespace
{
constexpr const char* Format = "adcoexist-manifest 1";
constexpr const char* Separator = "--\n";

std::string OutputLine(const Manifest::Outputs& aOutputs)
{
    if (aOutputs.empty)
        return "output none\n";
    char line[128];
    std::snprintf(line, sizeof(line), "output %llu %llu\n", static_cast<unsigned long long>(aOutputs.archiveSize),
                  static_cast<unsigned long long>(aOutputs.yamlSize));
    return line;
}
} // namespace

std::string Manifest::Fingerprint(const Inputs& aInputs)
{
    char line[256];
    std::string text = std::string(Format) + "\n";
    std::snprintf(line, sizeof(line), "adcoexist %d.%d.%d\n", aInputs.versionMajor, aInputs.versionMinor,
                  aInputs.versionPatch);
    text += line;
    std::snprintf(line, sizeof(line), "catalog %s %08x %08x\n", aInputs.catalogGameVersion.c_str(),
                  aInputs.catalogTweakDbChecksum, aInputs.catalogTweakDbEp1Checksum);
    text += line;
    std::snprintf(line, sizeof(line), "tweakdb %08x\n", aInputs.tweakDbChecksum);
    text += line;
    for (const auto& mod : aInputs.mods)
    {
        std::snprintf(line, sizeof(line), "%llu %lld ", static_cast<unsigned long long>(mod.size),
                      static_cast<long long>(mod.time));
        text += "mod " + std::string(line) + mod.relative + "\n";
    }
    return text;
}

std::string Manifest::Compose(std::string_view aFingerprint, const Outputs& aOutputs)
{
    return std::string(aFingerprint) + Separator + OutputLine(aOutputs);
}

Manifest::Reuse Manifest::Decide(std::string_view aManifest, std::string_view aFingerprint, bool aGeneratedExists,
                                 const Outputs& aCurrentOutputs)
{
    const auto prefix = std::string(aFingerprint) + Separator;
    if (aManifest.substr(0, prefix.size()) != prefix)
        return Reuse::No;
    const auto rest = aManifest.substr(prefix.size());

    Outputs none;
    none.empty = true;
    if (rest == OutputLine(none))
        return Reuse::Nothing;

    Outputs current = aCurrentOutputs;
    current.empty = false;
    if (aGeneratedExists && rest == OutputLine(current))
        return Reuse::Generated;
    return Reuse::No;
}
