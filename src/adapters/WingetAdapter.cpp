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

// ---------------- Robust table parser for winget output ----------------

std::vector<PackageInfo> parseWingetTable(const std::string& table) {
    std::vector<PackageInfo> pkgs;
    std::istringstream iss(table);
    std::string line;
    
    struct ColBounds {
        size_t start = 0;
        size_t length = std::string::npos;
    };
    std::vector<ColBounds> cols;
    
    auto trim = [](const std::string& s) -> std::string {
        auto start = s.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) return {};
        auto end = s.find_last_not_of(" \t\r\n");
        return s.substr(start, end - start + 1);
    };

    std::vector<std::string> lines;
    while (std::getline(iss, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        lines.push_back(line);
    }

    // Step 1: Find separator line and header line
    size_t sepIdx = std::string::npos;
    for (size_t i = 0; i < lines.size(); ++i) {
        const auto& l = lines[i];
        if (l.empty()) continue;
        bool isSep = true;
        size_t hyphenCount = 0;
        for (char c : l) {
            if (c == '-') {
                hyphenCount++;
            } else if (c != ' ' && c != '\t') {
                isSep = false;
                break;
            }
        }
        if (isSep && hyphenCount > 10) {
            sepIdx = i;
            break;
        }
    }

    if (sepIdx == std::string::npos || sepIdx == 0) {
        // Fallback to basic regex splitting if no separator line found
        for (const auto& l : lines) {
            if (l.empty()) continue;
            if (l.find("Id") != std::string::npos && l.find("Version") != std::string::npos) continue;
            if (l[0] == '-' && l.find_first_not_of("- \t") == std::string::npos) continue;
            auto rowCols = adapters::splitTableRow(l);
            if (rowCols.size() < 2) continue;
            PackageInfo p;
            p.manager = PackageManager::Winget;
            p.name = rowCols[0];
            p.id = rowCols[1];
            if (rowCols.size() >= 3) p.installedVersion = rowCols[2];
            if (rowCols.size() == 4) {
                p.availableVersion = "";
            } else if (rowCols.size() >= 5) {
                p.availableVersion = rowCols[3];
            }
            if (p.availableVersion.empty() || p.availableVersion == p.installedVersion) {
                p.state = InstallState::UpToDate;
            } else {
                p.state = InstallState::Unknown;
            }
            pkgs.push_back(std::move(p));
        }
        return pkgs;
    }

    // Step 2: Parse column bounds from separator line
    const std::string& sepLine = lines[sepIdx];
    size_t pos = 0;
    while (pos < sepLine.size()) {
        size_t nextHyphen = sepLine.find('-', pos);
        if (nextHyphen == std::string::npos) break;
        size_t nextSpace = sepLine.find(' ', nextHyphen);
        ColBounds b;
        b.start = nextHyphen;
        if (nextSpace == std::string::npos) {
            b.length = std::string::npos;
            cols.push_back(b);
            break;
        } else {
            b.length = nextSpace - nextHyphen;
            cols.push_back(b);
            pos = nextSpace + 1;
        }
    }

    // Step 3: Map column headers
    const std::string& headerLine = lines[sepIdx - 1];
    int nameColIdx = 0;
    int idColIdx = 1;
    int verColIdx = 2;
    int availColIdx = -1;

    for (size_t i = 0; i < cols.size(); ++i) {
        std::string hText;
        if (cols[i].start < headerLine.size()) {
            hText = headerLine.substr(cols[i].start, cols[i].length);
        }
        hText = trim(hText);
        std::transform(hText.begin(), hText.end(), hText.begin(), [](unsigned char c){ return (char)std::tolower(c); });
        
        if (hText.find("id") != std::string::npos || hText.find("kimlik") != std::string::npos) {
            idColIdx = static_cast<int>(i);
        } else if (hText.find("available") != std::string::npos || hText.find("kullan") != std::string::npos || hText.find("mevcut") != std::string::npos) {
            availColIdx = static_cast<int>(i);
        } else if (hText.find("version") != std::string::npos || hText.find("sürüm") != std::string::npos || hText.find("surum") != std::string::npos) {
            verColIdx = static_cast<int>(i);
        } else if (hText.find("name") != std::string::npos || hText.find("ad") != std::string::npos) {
            nameColIdx = static_cast<int>(i);
        }
    }

    // Step 4: Parse data lines
    for (size_t i = sepIdx + 1; i < lines.size(); ++i) {
        const auto& l = lines[i];
        if (l.empty()) continue;
        if (l.find("upgrades available") != std::string::npos) continue;
        if (l.find("upgrade available") != std::string::npos) continue;

        auto getColValue = [&](int colIdx) -> std::string {
            if (colIdx < 0 || colIdx >= static_cast<int>(cols.size())) return {};
            const auto& b = cols[colIdx];
            if (b.start >= l.size()) return {};
            std::string val = l.substr(b.start, b.length);
            auto p = val.find('<');
            if (p != std::string::npos) val = val.substr(0, p);
            return trim(val);
        };

        PackageInfo p;
        p.manager = PackageManager::Winget;
        p.name = getColValue(nameColIdx);
        p.id = getColValue(idColIdx);
        p.installedVersion = getColValue(verColIdx);
        if (availColIdx >= 0) {
            p.availableVersion = getColValue(availColIdx);
        }
        
        if (p.id.empty()) continue;

        if (p.availableVersion.empty() || p.availableVersion == p.installedVersion) {
            p.state = InstallState::UpToDate;
        } else {
            p.state = InstallState::Unknown;
        }

        pkgs.push_back(std::move(p));
    }
    return pkgs;
}

// ---------------- File-based runner for `winget export` --------------------

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
    ProcessOptions opt;
    opt.executable = "winget";
    opt.arguments  = {"export", "-o", tmp.string(), "--accept-source-agreements"};

    auto runner = std::make_shared<ProcessRunner>();
    runner->onComplete([cb = std::move(cb), tmp, runner](const ProcessResult& res) mutable {
        std::vector<PackageInfo> pkgs;
        std::string err;
        try {
            if (res.exitCode == 0 && !res.cancelled && std::filesystem::exists(tmp)) {
                std::ifstream in(tmp);
                std::stringstream ss; ss << in.rdbuf();
                std::string json = ss.str();
                in.close();
                std::error_code ec; std::filesystem::remove(tmp, ec);

                try { pkgs = parseExportJson(json); }
                catch (const std::exception& e) { err = std::string("parse: ") + e.what(); }
            } else {
                std::error_code ec; std::filesystem::remove(tmp, ec);
                err = "winget export failed or cancelled";
                if (!res.stderrText.empty()) err += ": " + res.stderrText;
            }
        } catch (const std::exception& e) {
            err = std::string("Internal error: ") + e.what();
        }
        if (cb) cb(std::move(pkgs), std::move(err));
    });

    if (!runner->start(opt)) {
        runner->onComplete(nullptr);
        std::error_code ec; std::filesystem::remove(tmp, ec);
        if (cb) cb({}, "ProcessRunner busy");
    }
}

void WingetAdapter::listUpgradable(PackageListCallback cb) {
    ProcessOptions opt;
    opt.executable = "winget";
    opt.arguments  = {"upgrade", "--accept-source-agreements"};
    adapters::runAndParseAsync(opt, parseWingetTable, std::move(cb));
}

void WingetAdapter::search(const std::string& query, PackageListCallback cb) {
    ProcessOptions opt;
    opt.executable = "winget";
    if (msStoreSearchEnabled_) {
        opt.arguments  = {"search", query, "--accept-source-agreements"};
    } else {
        opt.arguments  = {"search", query, "-s", "winget", "--accept-source-agreements"};
    }
    adapters::runAndParseAsync(opt, parseWingetTable, std::move(cb));
}

void WingetAdapter::performAction(const PackageInfo& pkg,
                                  TaskAction action,
                                  std::function<void(int percent)> progressCb,
                                  ActionCallback done) {
    std::vector<std::string> args;
    // --silent asks winget to suppress the package's own installer UI
    // wherever the underlying installer honours the flag. Combined with
    // --accept-package-agreements and a hidden console, this keeps the
    // whole flow in-app: the user sees a progress bar instead of a
    // "Next > Next > Finish" wizard.
    // --exact prevents matching multiple packages and prompting or failing.
    switch (action) {
        case TaskAction::Install:
            args = { "install", "--id", pkg.id,
                     "--exact",
                     "--accept-source-agreements",
                     "--accept-package-agreements",
                     "--silent" };
            break;
        case TaskAction::Upgrade:
            args = { "upgrade", "--id", pkg.id,
                     "--exact",
                     "--accept-source-agreements",
                     "--accept-package-agreements",
                     "--silent" };
            break;
        case TaskAction::Uninstall:
            args = { "uninstall", "--id", pkg.id,
                     "--exact",
                     "--accept-source-agreements",
                     "--silent" };
            break;
    }
    ProcessOptions opt;
    opt.executable = "winget";
    opt.arguments  = std::move(args);

    auto runner = std::make_shared<ProcessRunner>();
    runner->onProgress([progressCb](int p) {
        if (progressCb) progressCb(p);
    });
    // Capture `runner` to keep it alive for the duration of execution.
    // At the end of execution, we break the circular reference by setting
    // the callback pointers to nullptr.
    runner->onComplete([done, runner](const ProcessResult& res) {
        bool ok = (res.exitCode == 0) && !res.cancelled;
        std::string msg = ok ? "OK (" + std::to_string(res.exitCode) + ")"
                             : "FAILED (" + std::to_string(res.exitCode) + "): " + adapters::extractError(res);
        if (done) done(ok, std::move(msg));
    });
    runner->start(opt);
}

} // namespace pm