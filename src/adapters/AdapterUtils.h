#pragma once

#include "../core/ProcessRunner.h"
#include "../core/PackageInfo.h"
// PackageListCallback is defined in IPackageAdapter.h; including it here lets
// adapter implementations forward runAndParseAsync callbacks without taking
// a second dependency on the adapter interface header.
#include "IPackageAdapter.h"
#include <functional>
#include <regex>
#include <string>
#include <vector>

namespace pm::adapters {

// Split a winget/scoop-style table row on 2+ consecutive spaces.
inline std::vector<std::string> splitTableRow(const std::string& line) {
    static const std::regex colSplitter(R"(\s{2,})");
    std::vector<std::string> cols;
    auto begin = std::sregex_token_iterator(line.begin(), line.end(), colSplitter, -1);
    auto end   = std::sregex_token_iterator();
    for (auto it = begin; it != end; ++it) {
        std::string col = it->str();
        auto a = col.find_first_not_of(" \t\r\n");
        auto b = col.find_last_not_of(" \t\r\n");
        if (a == std::string::npos) continue;
        cols.push_back(col.substr(a, b - a + 1));
    }
    return cols;
}

inline bool isSeparatorLine(const std::string& line) {
    for (char c : line) if (c != '-' && c != ' ' && c != '\t') return false;
    return line.size() > 4;
}

// Run an async command and pipe its stdout through a parser function.
// The callback is invoked exactly once on a worker thread with the parsed
// packages (possibly empty) and an error message (empty on success).
void runAndParseAsync(
    ProcessOptions opt,
    std::function<std::vector<PackageInfo>(const std::string&)> parseFn,
    PackageListCallback cb);

// Synchronously probe an executable on PATH by running it with `--version`.
// Used by adapter `isAvailable()` implementations. Blocks the calling thread
// for at most ~1s.
bool probeVersion(const std::string& executable);

// Extract a clean error string from ProcessResult (checking stderr, and if empty,
// searching stdout for error keywords or the last non-empty line).
std::string extractError(const ProcessResult& res);

} // namespace pm::adapters