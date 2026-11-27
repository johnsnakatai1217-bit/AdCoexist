#include "Report.hpp"

#include "Util.hpp"

// Lengths are counted in bytes. A UTF-8 character is one or more bytes, so this never exceeds the character limit
namespace
{
constexpr std::string_view Indent = "  ";

// Cuts aText to at most aLength bytes without splitting a UTF-8 character
std::string Cut(std::string_view aText, size_t aLength)
{
    if (aText.size() <= aLength)
        return std::string(aText);
    constexpr std::string_view Ellipsis = "...";
    if (aLength < Ellipsis.size())
        return {};
    size_t end = aLength - Ellipsis.size();
    while (end > 0 && (static_cast<unsigned char>(aText[end]) & 0xC0) == 0x80)
        --end;
    return std::string(aText.substr(0, end)) + std::string(Ellipsis);
}

std::string Section(std::string_view aTitle, const std::vector<std::string>& aLines, size_t aBudget)
{
    std::string out(aTitle);
    if (aLines.empty())
        return out + " none\n";
    out += "\n";
    return out + Report::FitLines(aLines, aBudget > out.size() ? aBudget - out.size() : 0);
}
} // namespace

std::string Report::FitLines(const std::vector<std::string>& aLines, size_t aBudget)
{
    std::vector<std::string> lines;
    size_t total = 0;
    for (const auto& line : aLines)
    {
        lines.push_back(std::string(Indent) + Cut(line, LineLength) + "\n");
        total += lines.back().size();
    }
    if (total <= aBudget)
    {
        std::string out;
        for (const auto& line : lines)
            out += line;
        return out;
    }

    // Leave room for the line that tells how many were left out
    constexpr size_t MoreLength = 32;
    std::string out;
    size_t count = 0;
    while (count < lines.size() && out.size() + lines[count].size() + MoreLength <= aBudget)
        out += lines[count++];
    if (out.size() + MoreLength <= aBudget)
        out += std::string(Indent) + "... and " + std::to_string(lines.size() - count) + " more\n";
    return out;
}

std::string Report::HidePath(std::string_view aText, std::string_view aDirectory)
{
    if (aDirectory.empty())
        return std::string(aText);
    const auto text = Util::Lower(aText);
    const auto directory = Util::Lower(aDirectory);
    std::string out;
    size_t pos = 0;
    for (auto found = text.find(directory); found != std::string::npos; found = text.find(directory, pos))
    {
        out += aText.substr(pos, found - pos);
        out += "<game>";
        pos = found + directory.size();
    }
    out += aText.substr(pos);
    return out;
}

std::string Report::Summary(std::string_view aTime, const std::vector<ConvertedMod>& aMods,
                            const std::vector<ConvertedMod>& aSkipped, const std::vector<std::string>& aProblems,
                            const std::vector<ConvertedMod>& aKept)
{
    constexpr size_t ModsLength = 1400;
    constexpr size_t SkippedLength = 500;
    constexpr size_t KeptLength = 500;
    std::string out = "Last conversion: " + std::string(aTime) + "\n";

    const auto lines = [](const std::vector<ConvertedMod>& aList) {
        std::vector<std::string> out;
        for (const auto& mod : aList)
            out.push_back(mod.archive + ": " + std::to_string(mod.ads) + " ad(s)");
        return out;
    };
    out += Section("Converted mods:", lines(aMods), ModsLength);
    if (!aKept.empty())
        out += Section("Kept in the original place:", lines(aKept), KeptLength);
    out += Section("Not converted (only change how the game's own pictures are shown):", lines(aSkipped),
                   SkippedLength);
    out += Section("Problems at that conversion:", aProblems, SummaryLength - out.size());
    return out;
}

std::string Report::Compose(const Start& aStart, std::string_view aSummary)
{
    std::string out = "AdCoexist report (contains no folder paths)\n";
    out += "Written: " + aStart.time + "\n";
    out += "AdCoexist " + aStart.adcoexist + ", game " + aStart.game + "\n";
    out += "Ad catalog: " + aStart.catalog + "\n";
    out += "RED4ext: " + aStart.red4ext + "\n";
    out += "TweakXL: " + aStart.tweakXl + "\n";
    out += "Installed with: " + aStart.modManager + "\n";
    out += "Mod archives: " + std::to_string(aStart.modArchives) + "\n";
    out += std::string("Option restore-vanilla: ") + (aStart.restoreVanilla ? "on" : "off") + "\n";
    out += "This start: " + aStart.result + "\n";
    out += Section("Problems at this start:", aStart.problems, ProblemsLength);
    out += "\n";

    // The summary is read from a file, so cut it here too in case it was changed by hand
    const auto summary = aSummary.empty() ? std::string("Last conversion: not available\n") : std::string(aSummary);
    const auto room = MaxLength > out.size() ? MaxLength - out.size() : 0;
    out += Cut(summary, room);
    return Cut(out, MaxLength);
}
