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
    text += aInputs.restoreVanilla ? "restore-vanilla on\n" : "restore-vanilla off\n";
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

std::string Manifest::ExplainNo(std::string_view aManifest, std::string_view aFingerprint, bool aGeneratedExists,
                                const Outputs& aCurrentOutputs)
{
    if (aManifest.empty())
        return "no previous result";

    // Part of the manifest, or empty if the manifest is shorter
    const auto slice = [aManifest](size_t aPos, size_t aCount = std::string_view::npos) {
        return aPos <= aManifest.size() ? aManifest.substr(aPos, aCount) : std::string_view();
    };
    const auto withoutNewline = [](std::string_view aLine) {
        return std::string(!aLine.empty() && aLine.back() == '\n' ? aLine.substr(0, aLine.size() - 1) : aLine);
    };

    // The first line of the fingerprint that differs from the manifest
    size_t pos = 0;
    while (pos < aFingerprint.size())
    {
        const auto end = aFingerprint.find('\n', pos);
        const auto line = aFingerprint.substr(pos, end == std::string_view::npos ? end : end - pos + 1);
        if (slice(pos, line.size()) != line)
            return "changed: " + withoutNewline(line);
        pos += line.size();
    }
    const std::string_view separator = Separator;
    if (slice(pos, separator.size()) != separator)
        return "changed: a mod was removed";

    if (!aGeneratedExists)
        return "the generated files are missing";
    Outputs current = aCurrentOutputs;
    current.empty = false;
    return "the generated files changed (expected " + withoutNewline(slice(pos + separator.size())) + ", found " +
           withoutNewline(OutputLine(current)) + ")";
}
