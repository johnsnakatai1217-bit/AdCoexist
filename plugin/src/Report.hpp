#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

// Builds the short report for bug reports (red4ext\logs\adcoexist-report.txt). It fits in one Nexus comment
// (5000 characters) and contains no folder paths. Reading the environment and writing the files is up to the caller
namespace Report
{
constexpr size_t MaxLength = 5000;      // the whole report
constexpr size_t SummaryLength = 3000;  // the summary of the last conversion (variants.summary)
constexpr size_t ProblemsLength = 1000; // the problems of this start that aren't in the summary
constexpr size_t LineLength = 300;      // one line of a list

// Joins as many lines as fit in aBudget characters (each line is indented and ends with "\n").
// Long lines are cut. If some lines don't fit, the last line says how many were left out
std::string FitLines(const std::vector<std::string>& aLines, size_t aBudget);

// Replaces every occurrence of aDirectory (ignoring case) with "<game>"
std::string HidePath(std::string_view aText, std::string_view aDirectory);

struct ConvertedMod
{
    std::string archive; // relative to the game directory
    unsigned ads = 0;
};

// The summary of a conversion, written to variants.summary. At most SummaryLength characters
std::string Summary(std::string_view aTime, const std::vector<ConvertedMod>& aMods,
                    const std::vector<std::string>& aProblems);

struct Start
{
    std::string time;
    std::string adcoexist;  // version
    std::string game;       // version
    std::string catalog;    // e.g. "2.50, matches the game"
    std::string red4ext;    // version or "not loaded"
    std::string tweakXl;    // version or "not loaded"
    std::string modManager; // "MO2", "Vortex" or "manual"
    unsigned modArchives = 0;
    std::string result;                // what this start did
    std::vector<std::string> problems; // warnings and errors of this start that aren't in the summary
};

// The whole report. aSummary is the content of variants.summary, or empty when there is none.
// At most MaxLength characters
std::string Compose(const Start& aStart, std::string_view aSummary);
} // namespace Report
