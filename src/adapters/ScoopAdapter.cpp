#include "ScoopAdapter.h"
#include "AdapterUtils.h"
#include "../core/ProcessRunner.h"
#include "../core/Logger.h"

#include <future>
#include <memory>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace pm {

namespace {

// Scoop installs itself as a "shim" in C:\Users\<u>\scoop\shims. The
// shim directory contains `scoop` (a tiny unix-style launcher), the
// real `scoop.cmd` (a batch wrapper around the PowerShell core), and
// `scoop.ps1`. CreateProcessW with the bare name "scoop" finds the
// 343-byte shim file, tries to run it as a PE, and fails. The actual
// entry point Windows must launch is `scoop.cmd`, which is what
// shells expand to when the user types `scoop`. We hard-code that
// extension so ProcessRunner can find it.
constexpr const char* kScoopExe = "scoop.cmd";

std::vector<PackageInfo> parseScoopList(const std::string& table) {
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
        p.manager = PackageManager::Scoop;
        p.name    = cols[0];
        p.id      = cols[0];
        if (cols.size() >= 2) p.installedVersion = cols[1];
        if (p.id.empty()) continue;
        p.state = InstallState::UpToDate;
        pkgs.push_back(std::move(p));
    }
    return pkgs;
}

std::vector<PackageInfo> parseScoopStatus(const std::string& table) {
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
        p.manager          = PackageManager::Scoop;
        p.name             = cols[0];
        p.id               = cols[0];
        p.installedVersion = cols[1];
        if (cols.size() >= 3) p.availableVersion = cols[2];
        if (p.id.empty()) continue;
        p.state = (!p.availableVersion.empty() && p.availableVersion != p.installedVersion)
                  ? InstallState::Updating : InstallState::UpToDate;
        pkgs.push_back(std::move(p));
    }
    return pkgs;
}

std::vector<PackageInfo> parseScoopSearch(const std::string& table) {
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
        p.manager = PackageManager::Scoop;
        p.name    = cols[0];
        p.id      = cols[0];
        if (cols.size() >= 2) p.installedVersion = cols[1];
        p.state   = InstallState::Unknown;
        pkgs.push_back(std::move(p));
    }
    return pkgs;
}

} // anonymous

bool ScoopAdapter::isAvailable() const {
    return adapters::probeVersion(kScoopExe);
}

void ScoopAdapter::listInstalled(PackageListCallback cb) {
    ProcessOptions opt;
    opt.executable = kScoopExe;
    opt.arguments  = { "list" };
    std::thread([cb = std::move(cb), opt]() mutable {
        try {
            adapters::runAndParseAsync(opt, parseScoopList, std::move(cb));
        } catch (const std::exception& e) {
            Logger::instance().error("Exception inside ScoopAdapter listInstalled thread: ", e.what());
            if (cb) cb({}, std::string("Internal exception: ") + e.what());
        } catch (...) {
            Logger::instance().error("Unknown exception inside ScoopAdapter listInstalled thread");
            if (cb) cb({}, "Unknown internal exception");
        }
    }).detach();
}

void ScoopAdapter::listUpgradable(PackageListCallback cb) {
    ProcessOptions opt;
    opt.executable = kScoopExe;
    opt.arguments  = { "status" };
    std::thread([cb = std::move(cb), opt]() mutable {
        try {
            adapters::runAndParseAsync(opt, parseScoopStatus, std::move(cb));
        } catch (const std::exception& e) {
            Logger::instance().error("Exception inside ScoopAdapter listUpgradable thread: ", e.what());
            if (cb) cb({}, std::string("Internal exception: ") + e.what());
        } catch (...) {
            Logger::instance().error("Unknown exception inside ScoopAdapter listUpgradable thread");
            if (cb) cb({}, "Unknown internal exception");
        }
    }).detach();
}

void ScoopAdapter::search(const std::string& query, PackageListCallback cb) {
    ProcessOptions opt;
    opt.executable = kScoopExe;
    opt.arguments  = { "search", query };
    std::thread([cb = std::move(cb), opt]() mutable {
        try {
            adapters::runAndParseAsync(opt, parseScoopSearch, std::move(cb));
        } catch (const std::exception& e) {
            Logger::instance().error("Exception inside ScoopAdapter search thread: ", e.what());
            if (cb) cb({}, std::string("Internal exception: ") + e.what());
        } catch (...) {
            Logger::instance().error("Unknown exception inside ScoopAdapter search thread");
            if (cb) cb({}, "Unknown internal exception");
        }
    }).detach();
}

void ScoopAdapter::performAction(const PackageInfo& pkg,
                                 TaskAction action,
                                 std::function<void(int percent)> progressCb,
                                 ActionCallback done) {
    std::vector<std::string> args;
    switch (action) {
        case TaskAction::Install:
            args = { "install", pkg.id };
            break;
        case TaskAction::Upgrade:
            args = { "update", pkg.id };
            break;
        case TaskAction::Uninstall:
            args = { "uninstall", pkg.id };
            break;
    }
    ProcessOptions opt;
    opt.executable = kScoopExe;
    opt.arguments  = std::move(args);

    auto runner = std::make_shared<ProcessRunner>();
    runner->onProgress([progressCb](int p) {
        if (progressCb) progressCb(p);
    });
    runner->onComplete([done, runner](const ProcessResult& res) {
        bool ok = (res.exitCode == 0) && !res.cancelled;
        std::string msg = ok ? "OK (" + std::to_string(res.exitCode) + ")"
                             : "FAILED (" + std::to_string(res.exitCode) + "): " + adapters::extractError(res);
        if (done) done(ok, std::move(msg));
    });
    runner->start(opt);
}

} // namespace pm