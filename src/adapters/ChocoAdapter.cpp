#include "ChocoAdapter.h"
#include "AdapterUtils.h"
#include "../core/ProcessRunner.h"
#include "../core/Logger.h"

#include <future>
#include <memory>
#include <sstream>
#include <string>
#include <thread>
#include <vector>
#include <windows.h>

namespace pm {

namespace {

// Choco's official installer drops choco.exe at
// C:\ProgramData\chocolatey\bin and adds that folder to the SYSTEM
// PATH. New shells inherit that path, but a session that was already
// open (or an app launched before the install) won't see it until
// restart. We probe the default location explicitly so the user
// doesn't have to relaunch the app after installing choco.
std::string resolveChocoPath() {
    static const std::string defaultPath = "C:\\ProgramData\\chocolatey\\bin\\choco.exe";
    DWORD attr = GetFileAttributesA(defaultPath.c_str());
    if (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY)) {
        return defaultPath;
    }
    return "choco";
}

// ---------------- Choco parsers ----------------
//
// Chocolatey output is pipe-separated:  Name|Version
// `choco list -i --limit-output` (or just `choco list`) emits:
//   7zip|23.01
//   git|2.43.0
//
// `choco outdated --limit-output` (default) emits:
//   googlechrome|120.0.6099.110|120.0.6099.130|false
// = name|installed|available|pinned

std::vector<std::string> splitPipe(const std::string& line) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : line) {
        if (c == '|') { out.push_back(cur); cur.clear(); }
        else if (c != '\r' && c != '\n') cur += c;
    }
    if (!cur.empty()) out.push_back(cur);
    return out;
}

std::vector<PackageInfo> parseChocoList(const std::string& text) {
    std::vector<PackageInfo> pkgs;
    std::istringstream iss(text);
    std::string line;
    while (std::getline(iss, line)) {
        if (line.empty()) continue;
        if (line.find("Chocolatey") != std::string::npos) continue;  // skip banner
        if (line.find("packages installed") != std::string::npos) continue;
        auto parts = splitPipe(line);
        if (parts.size() < 2) continue;
        PackageInfo p;
        p.manager          = PackageManager::Chocolatey;
        p.id               = parts[0];
        p.name             = parts[0]; // choco uses package id as the display name
        p.installedVersion = parts[1];
        p.state            = InstallState::UpToDate;
        if (!p.id.empty()) pkgs.push_back(std::move(p));
    }
    return pkgs;
}

std::vector<PackageInfo> parseChocoOutdated(const std::string& text) {
    std::vector<PackageInfo> pkgs;
    std::istringstream iss(text);
    std::string line;
    while (std::getline(iss, line)) {
        if (line.empty()) continue;
        if (line.find("Chocolatey") != std::string::npos) continue;
        if (line.find("packages outdated") != std::string::npos) continue;
        if (line.find("packages found") != std::string::npos) continue;
        auto parts = splitPipe(line);
        if (parts.size() < 3) continue;
        PackageInfo p;
        p.manager          = PackageManager::Chocolatey;
        p.id               = parts[0];
        p.name             = parts[0];
        p.installedVersion = parts[1];
        p.availableVersion = parts[2];
        p.state            = InstallState::Updating;
        if (!p.id.empty()) pkgs.push_back(std::move(p));
    }
    return pkgs;
}

// `choco search <q>` emits verbose multi-line blocks; using `--limit-output`
// gives the simpler Name|Version list, which is enough for our UI.
std::vector<PackageInfo> parseChocoSearch(const std::string& text) {
    std::vector<PackageInfo> pkgs;
    std::istringstream iss(text);
    std::string line;
    while (std::getline(iss, line)) {
        if (line.empty()) continue;
        if (line.find("Chocolatey") != std::string::npos) continue;
        auto parts = splitPipe(line);
        if (parts.size() < 2) continue;
        PackageInfo p;
        p.manager = PackageManager::Chocolatey;
        p.id      = parts[0];
        p.name    = parts[0];
        p.installedVersion = parts[1];
        p.state   = InstallState::Unknown;
        if (!p.id.empty()) pkgs.push_back(std::move(p));
    }
    return pkgs;
}

} // anonymous

bool ChocoAdapter::isAvailable() const {
    return adapters::probeVersion(resolveChocoPath());
}

void ChocoAdapter::listInstalled(PackageListCallback cb) {
    ProcessOptions opt;
    opt.executable = resolveChocoPath();
    opt.arguments  = { "list", "--limit-output" };
    std::thread([cb = std::move(cb), opt]() mutable {
        try {
            adapters::runAndParseAsync(opt, parseChocoList, std::move(cb));
        } catch (const std::exception& e) {
            Logger::instance().error("Exception inside ChocoAdapter listInstalled thread: ", e.what());
            if (cb) cb({}, std::string("Internal exception: ") + e.what());
        } catch (...) {
            Logger::instance().error("Unknown exception inside ChocoAdapter listInstalled thread");
            if (cb) cb({}, "Unknown internal exception");
        }
    }).detach();
}

void ChocoAdapter::listUpgradable(PackageListCallback cb) {
    ProcessOptions opt;
    opt.executable = resolveChocoPath();
    opt.arguments  = { "outdated", "--limit-output" };
    std::thread([cb = std::move(cb), opt]() mutable {
        try {
            adapters::runAndParseAsync(opt, parseChocoOutdated, std::move(cb));
        } catch (const std::exception& e) {
            Logger::instance().error("Exception inside ChocoAdapter listUpgradable thread: ", e.what());
            if (cb) cb({}, std::string("Internal exception: ") + e.what());
        } catch (...) {
            Logger::instance().error("Unknown exception inside ChocoAdapter listUpgradable thread");
            if (cb) cb({}, "Unknown internal exception");
        }
    }).detach();
}

void ChocoAdapter::search(const std::string& query, PackageListCallback cb) {
    ProcessOptions opt;
    opt.executable = resolveChocoPath();
    opt.arguments  = { "search", query, "--limit-output" };
    std::thread([cb = std::move(cb), opt]() mutable {
        try {
            adapters::runAndParseAsync(opt, parseChocoSearch, std::move(cb));
        } catch (const std::exception& e) {
            Logger::instance().error("Exception inside ChocoAdapter search thread: ", e.what());
            if (cb) cb({}, std::string("Internal exception: ") + e.what());
        } catch (...) {
            Logger::instance().error("Unknown exception inside ChocoAdapter search thread");
            if (cb) cb({}, "Unknown internal exception");
        }
    }).detach();
}

void ChocoAdapter::performAction(const PackageInfo& pkg,
                                 TaskAction action,
                                 std::function<void(int percent)> progressCb,
                                 ActionCallback done) {
    std::vector<std::string> args;
    switch (action) {
        case TaskAction::Install:
            args = { "install", pkg.id, "-y", "--no-progress" };
            break;
        case TaskAction::Upgrade:
            args = { "upgrade", pkg.id, "-y", "--no-progress" };
            break;
        case TaskAction::Uninstall:
            args = { "uninstall", pkg.id, "-y", "--no-progress" };
            break;
    }
    ProcessOptions opt;
    opt.executable = resolveChocoPath();
    opt.arguments  = std::move(args);
    (void)progressCb;

    auto runner = std::make_shared<ProcessRunner>();
    runner->onComplete([done, runner](const ProcessResult& res) {
        bool ok = (res.exitCode == 0) && !res.cancelled;
        std::string msg = ok ? "OK (" + std::to_string(res.exitCode) + ")"
                             : "FAILED (" + std::to_string(res.exitCode) + "): " + adapters::extractError(res);
        if (done) done(ok, std::move(msg));
    });
    runner->start(opt);
}

} // namespace pm