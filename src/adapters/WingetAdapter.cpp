#include "WingetAdapter.h"
#include "AdapterUtils.h"
#include "../core/ProcessRunner.h"
#include "../core/Logger.h"

#include <future>
#include <fstream>
#include <regex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>
#include <filesystem>
#include <cstdio>

namespace pm {

namespace {

// ---------------- JSON helpers (sufficient for winget output) ---------------

std::string extractJsonString(const std::string& json, const std::string& key) {
    std::string pattern = "\"" + key + "\"\\s*:\\s*\"((?:\\\\.|[^\"\\\\])*)\"";
    std::regex re(pattern);
    std::smatch m;
    if (!std::regex_search(json, m, re)) return {};
    std::string v = m[1].str();
    std::string out;
    out.reserve(v.size());
    for (size_t i = 0; i < v.size(); ++i) {
        if (v[i] == '\\' && i + 1 < v.size()) {
            char n = v[i + 1];
            switch (n) {
                case 'n': out += '\n'; break;
                case 'r': out += '\r'; break;
                case 't': out += '\t'; break;
                case '"': out += '"';  break;
                case '\\': out += '\\'; break;
                default:  out += n;
            }
            ++i;
        } else {
            out += v[i];
        }
    }
    return out;
}

// Walk top-level JSON objects ({...}). Tracks brace depth for nested cases.
std::vector<std::string> extractTopLevelObjects(const std::string& json) {
    std::vector<std::string> result;
    size_t pos = 0;
    while (true) {
        size_t open = json.find('{', pos);
        if (open == std::string::npos) break;
        int depth = 0;
        size_t close = std::string::npos;
        for (size_t i = open; i < json.size(); ++i) {
            if (json[i] == '{') ++depth;
            else if (json[i] == '}') { --depth; if (depth == 0) { close = i; break; } }
        }
        if (close == std::string::npos) break;
        result.push_back(json.substr(open, close - open + 1));
        pos = close + 1;
    }
    return result;
}

// ---------------- Table parser for `winget list` / `upgrade` ----------------

// Parse `winget list` (default table output) — winget v1.28 produces 4 cols:
//   Name   Version   Available   Id
std::vector<PackageInfo> parseListTable(const std::string& table) {
    std::vector<PackageInfo> pkgs;
    std::istringstream iss(table);
    std::string line;
    bool pastHeader = false;
    while (std::getline(iss, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        if (adapters::isSeparatorLine(line)) { pastHeader = true; continue; }
        if (!pastHeader) continue;
        auto cols = adapters::splitTableRow(line);
        if (cols.size() < 2) continue;
        PackageInfo p;
        p.manager = PackageManager::Winget;
        if (cols.size() >= 5) {
            // Layout: Name, Id, Version, Available, Source
            p.name    = cols[0];
            p.id      = cols[1];
            p.installedVersion = cols[2];
            p.availableVersion = cols[3];
        } else if (cols.size() == 4) {
            p.name    = cols[0];
            p.id      = cols[1];
            p.installedVersion = cols[2];
            p.availableVersion = cols[3];
        } else if (cols.size() == 3) {
            p.name    = cols[0];
            p.id      = cols[1];
            p.installedVersion = cols[2];
        } else {
            p.id   = cols[0];
            p.name = cols[1];
        }
        if (p.id.empty()) continue;
        if (p.availableVersion.empty() || p.availableVersion == p.installedVersion) {
            p.state = InstallState::UpToDate;
        } else {
            p.state = InstallState::Updating;
        }
        pkgs.push_back(std::move(p));
    }
    return pkgs;
}

std::vector<PackageInfo> parseUpgradeTable(const std::string& table) {
    return parseListTable(table); // same shape
}

std::vector<PackageInfo> parseSearchTable(const std::string& table, const std::string& query) {
    (void)query;
    std::vector<PackageInfo> pkgs;
    std::istringstream iss(table);
    std::string line;
    bool pastHeader = false;
    while (std::getline(iss, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        if (adapters::isSeparatorLine(line)) { pastHeader = true; continue; }
        if (!pastHeader) continue;
        auto cols = adapters::splitTableRow(line);
        if (cols.size() < 2) continue;
        PackageInfo p;
        p.manager = PackageManager::Winget;
        if (cols.size() >= 3) {
            p.name = cols[0];
            p.id   = cols[1];
            p.installedVersion = cols[2];
        } else {
            p.id   = cols[0];
            p.name = cols[1];
        }
        if (p.id.empty()) continue;
        p.state = InstallState::Unknown;
        pkgs.push_back(std::move(p));
    }
    return pkgs;
}

// ---------------- File-based runner for `winget export` --------------------

std::string runAndReadFile(const std::vector<std::string>& args, const std::filesystem::path& file) {
    auto promise = std::make_shared<std::promise<bool>>();
    auto fut     = promise->get_future();
    std::string captured;

    ProcessOptions opt;
    opt.executable = "winget";
    opt.arguments  = args;

    auto runner = std::make_shared<ProcessRunner>();
    runner->onComplete([runner, &captured, promise](const ProcessResult& res) {
        captured = res.stdoutText + res.stderrText;
        promise->set_value(res.exitCode == 0 && !res.cancelled);
    });
    if (!runner->start(opt)) return {};
    fut.wait();
    if (!std::filesystem::exists(file)) return {};
    std::ifstream in(file);
    std::stringstream ss; ss << in.rdbuf();
    return ss.str();
}

std::filesystem::path makeTempJson() {
    auto p = std::filesystem::temp_directory_path() /
             (std::string("winget_export_") +
              std::to_string(std::chrono::system_clock::now().time_since_epoch().count()) +
              ".json");
    return p;
}

std::vector<PackageInfo> parseExportJson(const std::string& json) {
    std::vector<PackageInfo> result;
    for (const auto& obj : extractTopLevelObjects(json)) {
        std::regex re("\"PackageIdentifier\"\\s*:\\s*\"([^\"]+)\"");
        auto begin = std::sregex_iterator(obj.begin(), obj.end(), re);
        auto end   = std::sregex_iterator();
        for (auto it = begin; it != end; ++it) {
            PackageInfo p;
            p.id      = (*it)[1].str();
            p.name    = p.id;
            p.manager = PackageManager::Winget;
            p.state   = InstallState::UpToDate;
            if (!p.id.empty()) result.push_back(std::move(p));
        }
    }
    return result;
}

} // anonymous

bool WingetAdapter::isAvailable() const {
    return adapters::probeVersion("winget");
}

void WingetAdapter::listInstalled(PackageListCallback cb) {
    auto tmp = makeTempJson();
    std::thread([cb = std::move(cb), tmp]() mutable {
        std::vector<std::string> args = {"export", "-o", tmp.string(), "--accept-source-agreements"};
        std::string json = runAndReadFile(args, tmp);
        std::error_code ec; std::filesystem::remove(tmp, ec);
        std::vector<PackageInfo> pkgs;
        std::string err;
        if (json.empty()) {
            err = "winget export produced no JSON file";
        } else {
            try { pkgs = parseExportJson(json); }
            catch (const std::exception& e) { err = std::string("parse: ") + e.what(); }
        }
        if (cb) cb(std::move(pkgs), std::move(err));
    }).detach();
}

void WingetAdapter::listUpgradable(PackageListCallback cb) {
    ProcessOptions opt;
    opt.executable = "winget";
    opt.arguments  = {"upgrade", "--accept-source-agreements"};
    std::thread([cb = std::move(cb), opt]() mutable {
        adapters::runAndParseAsync(opt, parseUpgradeTable, std::move(cb));
    }).detach();
}

void WingetAdapter::search(const std::string& query, PackageListCallback cb) {
    ProcessOptions opt;
    opt.executable = "winget";
    opt.arguments  = {"search", query, "--accept-source-agreements"};
    std::thread([cb = std::move(cb), opt, query]() mutable {
        adapters::runAndParseAsync(opt,
            [query](const std::string& s) { return parseSearchTable(s, query); },
            std::move(cb));
    }).detach();
}

void WingetAdapter::performAction(const PackageInfo& pkg,
                                  TaskAction action,
                                  std::function<void(int percent)> progressCb,
                                  ActionCallback done) {
    std::vector<std::string> args;
    switch (action) {
        case TaskAction::Install:   args = {"install",   pkg.id, "--accept-source-agreements", "--accept-package-agreements"}; break;
        case TaskAction::Upgrade:   args = {"upgrade",   "--id", pkg.id, "--accept-source-agreements", "--accept-package-agreements"}; break;
        case TaskAction::Uninstall: args = {"uninstall", "--id", pkg.id, "--accept-source-agreements"}; break;
    }
    ProcessOptions opt;
    opt.executable = "winget";
    opt.arguments  = std::move(args);

    auto runner = std::make_shared<ProcessRunner>();
    runner->onProgress([progressCb](int p) {
        if (progressCb) progressCb(p);
    });
    runner->onComplete([runner, done](const ProcessResult& res) {
        bool ok = (res.exitCode == 0) && !res.cancelled;
        std::string msg = ok ? "OK (" + std::to_string(res.exitCode) + ")"
                             : "FAILED (" + std::to_string(res.exitCode) + "): " + res.stderrText;
        if (done) done(ok, std::move(msg));
    });
    runner->start(opt);
}

} // namespace pm