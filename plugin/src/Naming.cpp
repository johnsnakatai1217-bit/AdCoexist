#include "Naming.hpp"

#include <cctype>
#include <cstdio>
#include <set>

#include "Util.hpp"

namespace
{
struct PrefixRule
{
    std::string_view vanilla;
    std::string_view suffix; // appended to "adcoexist\<16 hex digits>" to keep the same length
};
constexpr PrefixRule PrefixRules[] = {
    {"base\\gameplay\\gui\\world\\adverts", "\\advt"}, // 31 characters
    {"ep1\\gameplay\\gui\\world\\adverts", "\\adv"},   // 30 characters
};

constexpr size_t MaxPathLength = 216; // same as ResourcePath
constexpr size_t IdSuffixLength = 6;  // length of the ID added when names collide
} // namespace

uint64_t Naming::PathHash(std::string_view aPath)
{
    // Same normalization as ResourcePath::HashSanitized in RED4ext.SDK:
    // strip leading quotes and separators, collapse repeated separators, turn / into \, lowercase letters,
    // stop at a quote, at most 216 characters
    std::string buffer;
    size_t i = 0;
    if (i < aPath.size() && (aPath[i] == '"' || aPath[i] == '\''))
        ++i;
    while (i < aPath.size() && (aPath[i] == '/' || aPath[i] == '\\'))
        ++i;
    while (i < aPath.size() && aPath[i] != '\0' && aPath[i] != '"' && aPath[i] != '\'')
    {
        if (aPath[i] == '/' || aPath[i] == '\\')
        {
            buffer += '\\';
            ++i;
            while (i < aPath.size() && (aPath[i] == '/' || aPath[i] == '\\'))
                ++i;
        }
        else
        {
            const char c = aPath[i++];
            buffer += (c >= 'A' && c <= 'Z') ? static_cast<char>(c + ('a' - 'A')) : c;
        }
        if (buffer.size() == MaxPathLength)
            break;
    }
    return buffer.empty() ? 0 : Util::Fnv1a64(buffer);
}

std::string Naming::ArchiveId(std::string_view aRelativePath)
{
    char id[17];
    std::snprintf(id, sizeof(id), "%016llx", static_cast<unsigned long long>(Util::Fnv1a64(Util::Lower(aRelativePath))));
    return id;
}

std::string Naming::NewPath(std::string_view aOriginal, std::string_view aId)
{
    for (const auto& rule : PrefixRules)
    {
        if (aOriginal.rfind(rule.vanilla, 0) != 0)
            continue;
        const auto prefix = "adcoexist\\" + std::string(aId) + std::string(rule.suffix);
        if (prefix.size() != rule.vanilla.size())
            return {};
        return prefix + std::string(aOriginal.substr(rule.vanilla.size()));
    }
    return {};
}

std::string Naming::RecordStem(std::string_view aArchiveFileStem)
{
    std::string stem;
    for (char c : aArchiveFileStem)
        stem += std::isalnum(static_cast<unsigned char>(c)) ? c : '_';
    return stem;
}

std::map<std::string, std::string> Naming::AssignRecordStems(const std::vector<ArchiveName>& aArchives)
{
    std::map<std::string, std::set<std::string>> archivesOfStem;
    for (const auto& archive : aArchives)
        archivesOfStem[archive.stem].insert(archive.relative);

    std::map<std::string, std::string> result;
    for (const auto& archive : aArchives)
    {
        const auto& sameStem = archivesOfStem[archive.stem];
        result[archive.relative] =
            sameStem.size() <= 1 ? archive.stem
                                 : archive.stem + "_" + ArchiveId(archive.relative).substr(0, IdSuffixLength);
    }
    return result;
}

std::string Naming::VariantRecordName(std::string_view aStem, std::string_view aOriginalRecord)
{
    const auto dot = aOriginalRecord.find('.');
    const auto shortName = dot == std::string_view::npos ? aOriginalRecord : aOriginalRecord.substr(dot + 1);
    return "Adverts.adcoexist_" + std::string(aStem) + "_" + std::string(shortName);
}
