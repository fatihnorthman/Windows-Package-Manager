#include "BackendBridge.h"
#include <shellapi.h>
#pragma comment(lib, "shell32.lib")
#include "../adapters/WingetAdapter.h"
#include "../adapters/ScoopAdapter.h"
#include "../adapters/ChocoAdapter.h"
#include "../core/Logger.h"
#include "../core/ProcessRunner.h"
#include <algorithm>
#include <memory>
#include <thread>
#include <future>
#include <filesystem>

namespace pm::gui {

BackendBridge::BackendBridge() : alive_(std::make_shared<bool>(true)) {}

BackendBridge::~BackendBridge() {
    *alive_ = false;
    ProcessRunner::cancelAll();
    // queue_'s destructor shuts down workers.
}

void BackendBridge::init() {
    adapters_.push_back(std::make_shared<WingetAdapter>());
    adapters_.push_back(std::make_shared<ScoopAdapter>());
    adapters_.push_back(std::make_shared<ChocoAdapter>());

    queue_ = std::make_unique<TaskQueue>(2);
    queue_->setStateCallback([this, alive = alive_](const Task& t) {
        if (!*alive) return;
        if (t.state == InstallState::Installed || t.state == InstallState::Failed) {
            unmarkInFlight(t.package.manager, t.package.id);
        }
        // Auto-refresh the package lists when a task completes so the UI
        // reflects the new install/upgrade/uninstall state without the
        // user having to press Refresh manually.
        if (t.state == InstallState::Installed) {
            refreshInstalled();
            refreshUpgradable();
        }
    });

    detectTools();

    Logger::instance().info("BackendBridge ready: ", adapters_.size(), " adapter(s) active");
}

namespace {

std::string queryPath(const std::string& exeName) {
    ProcessOptions opt;
    opt.executable = "where.exe";
    opt.arguments  = { exeName };
    auto runner = std::make_shared<ProcessRunner>();
    auto promise = std::make_shared<std::promise<std::string>>();
    auto future = promise->get_future();
    runner->onComplete([promise](const ProcessResult& res) {
        if (res.exitCode == 0 && !res.cancelled && !res.stdoutText.empty()) {
            std::string out = res.stdoutText;
            while (!out.empty() && (out.back() == '\n' || out.back() == '\r' || out.back() == ' ')) out.pop_back();
            auto firstNewline = out.find_first_of("\r\n");
            if (firstNewline != std::string::npos) {
                out = out.substr(0, firstNewline);
            }
            promise->set_value(out);
        } else {
            promise->set_value("Not found");
        }
    });
    if (!runner->start(opt)) return "Not found";
    if (future.wait_for(std::chrono::seconds(2)) == std::future_status::ready) {
        return future.get();
    } else {
        runner->cancel();
        return "Not found";
    }
}

std::string queryVersion(const std::string& exeName) {
    auto runQuery = [](const std::string& exe, const std::vector<std::string>& args) -> std::string {
        ProcessOptions opt;
        opt.executable = exe;
        opt.arguments  = args;
        auto runner = std::make_shared<ProcessRunner>();
        auto promise = std::make_shared<std::promise<std::string>>();
        auto future = promise->get_future();
        runner->onComplete([promise](const ProcessResult& res) {
            if (res.exitCode == 0 && !res.cancelled && !res.stdoutText.empty()) {
                std::string out = res.stdoutText;
                while (!out.empty() && (out.back() == '\n' || out.back() == '\r' || out.back() == ' ')) out.pop_back();
                auto firstNewline = out.find_first_of("\r\n");
                if (firstNewline != std::string::npos) {
                    out = out.substr(0, firstNewline);
                }
                promise->set_value(out);
            } else {
                promise->set_value("");
            }
        });
        if (!runner->start(opt)) return "";
        if (future.wait_for(std::chrono::seconds(2)) == std::future_status::ready) {
            return future.get();
        } else {
            runner->cancel();
            return "";
        }
    };
    
    std::string ver = runQuery(exeName, { "--version" });
    if (ver.empty()) {
        ver = runQuery(exeName, { "-v" });
    }
    if (ver.empty()) {
        ver = "Unknown";
    }
    return ver;
}

} // namespace

void BackendBridge::detectTools() {
    for (const auto& a : adapters_) {
        bool ok = false;
        try { ok = a->isAvailable(); }
        catch (const std::exception& e) {
            Logger::instance().warn("Adapter probe threw: ", e.what());
        }
        switch (a->manager()) {
            case PackageManager::Winget:     state_.wingetAvailable = ok; break;
            case PackageManager::Scoop:      state_.scoopAvailable  = ok; break;
            case PackageManager::Chocolatey: state_.chocoAvailable  = ok; break;
            default: break;
        }
    }

    state_.wingetPath = state_.wingetAvailable ? queryPath("winget") : "Not found";
    state_.wingetVer  = state_.wingetAvailable ? queryVersion("winget") : "Not found";

    state_.scoopPath  = state_.scoopAvailable ? queryPath("scoop") : "Not found";
    state_.scoopVer   = state_.scoopAvailable ? queryVersion("scoop") : "Not found";

    state_.chocoPath  = state_.chocoAvailable ? queryPath("choco") : "Not found";
    state_.chocoVer   = state_.chocoAvailable ? queryVersion("choco") : "Not found";

    if (queue_) {
        std::vector<std::shared_ptr<IPackageAdapter>> activeAdapters = adapters_;
        activeAdapters.erase(std::remove_if(activeAdapters.begin(), activeAdapters.end(),
            [this](const std::shared_ptr<IPackageAdapter>& a) {
                if (a->manager() == PackageManager::Winget)     return !state_.wingetAvailable;
                if (a->manager() == PackageManager::Scoop)      return !state_.scoopAvailable;
                if (a->manager() == PackageManager::Chocolatey) return !state_.chocoAvailable;
                return true;
            }), activeAdapters.end());
        queue_->setAdapters(activeAdapters);
    }
}

// Shared merge state for an in-flight parallel adapter load.
// Heap-allocated and captured by shared_ptr so the lambdas passed to the
// adapters can keep using it safely after the dispatcher thread returns.
// Previously these lived on the dispatcher's stack, which caused use-after-
// free crashes when the adapter invoked the callback asynchronously.
struct MergeCtx {
    std::vector<PackageInfo>  merged;
    std::string               firstError;
    std::atomic<int>          remaining{0};
    std::mutex                mergeMtx;
};

void BackendBridge::refreshInstalled() {
    if (state_.loadingInstalled.load()) return;
    state_.loadingInstalled = true;
    auto adapters = adapters_;
    AppState* st  = &state_;
    auto ctx      = std::make_shared<MergeCtx>();
    ctx->remaining = static_cast<int>(adapters.size());

    if (adapters.empty()) {
        state_.loadingInstalled = false;
        if (state_.hwnd) InvalidateRect(state_.hwnd, nullptr, FALSE);
        return;
    }

    for (const auto& a : adapters) {
        a->listInstalled([st, ctx, alive = alive_](std::vector<PackageInfo> pkgs, std::string err) {
            if (!*alive) return;
            try {
                {
                    std::lock_guard<std::mutex> lk(ctx->mergeMtx);
                    if (err.empty()) {
                        ctx->merged.insert(ctx->merged.end(),
                                           std::make_move_iterator(pkgs.begin()),
                                           std::make_move_iterator(pkgs.end()));
                    } else if (ctx->firstError.empty()) {
                        ctx->firstError = err;
                    }
                }
                if (--ctx->remaining == 0) {
                    {
                        std::lock_guard<std::recursive_mutex> lk(st->mtx);
                        if (!ctx->merged.empty()) {
                            st->installed = std::move(ctx->merged);
                            st->lastError.clear();
                        } else {
                            st->lastError = ctx->firstError.empty() ? "no installed packages found" : ctx->firstError;
                        }
                    }
                    st->loadingInstalled = false;
                    if (st->hwnd) {
                        InvalidateRect(st->hwnd, nullptr, FALSE);
                    }
                }
            } catch (const std::exception& e) {
                Logger::instance().error("Exception inside refreshInstalled list callback: ", e.what());
                st->loadingInstalled = false;
                if (st->hwnd) InvalidateRect(st->hwnd, nullptr, FALSE);
            } catch (...) {
                Logger::instance().error("Unknown exception inside refreshInstalled list callback");
                st->loadingInstalled = false;
                if (st->hwnd) InvalidateRect(st->hwnd, nullptr, FALSE);
            }
        });
    }
}

void BackendBridge::refreshUpgradable() {
    if (state_.loadingUpgradable.load()) return;
    state_.loadingUpgradable = true;
    auto adapters = adapters_;
    AppState* st  = &state_;
    auto ctx      = std::make_shared<MergeCtx>();
    ctx->remaining = static_cast<int>(adapters.size());

    if (adapters.empty()) {
        state_.loadingUpgradable = false;
        if (state_.hwnd) InvalidateRect(state_.hwnd, nullptr, FALSE);
        return;
    }

    for (const auto& a : adapters) {
        a->listUpgradable([st, ctx, alive = alive_](std::vector<PackageInfo> pkgs, std::string err) {
            if (!*alive) return;
            try {
                {
                    std::lock_guard<std::mutex> lk(ctx->mergeMtx);
                    if (err.empty()) {
                        ctx->merged.insert(ctx->merged.end(),
                                           std::make_move_iterator(pkgs.begin()),
                                           std::make_move_iterator(pkgs.end()));
                    } else if (ctx->firstError.empty()) {
                        ctx->firstError = err;
                    }
                }
                if (--ctx->remaining == 0) {
                    {
                        std::lock_guard<std::recursive_mutex> lk(st->mtx);
                        if (!ctx->merged.empty()) {
                            st->upgradable = std::move(ctx->merged);
                            st->lastError.clear();
                        } else {
                            if (ctx->merged.empty() && ctx->firstError.empty())
                                st->lastError.clear();
                            else
                                st->lastError = ctx->firstError;
                        }
                    }
                    st->loadingUpgradable = false;
                    if (st->hwnd) {
                        InvalidateRect(st->hwnd, nullptr, FALSE);
                    }
                }
            } catch (const std::exception& e) {
                Logger::instance().error("Exception inside refreshUpgradable list callback: ", e.what());
                st->loadingUpgradable = false;
                if (st->hwnd) InvalidateRect(st->hwnd, nullptr, FALSE);
            } catch (...) {
                Logger::instance().error("Unknown exception inside refreshUpgradable list callback");
                st->loadingUpgradable = false;
                if (st->hwnd) InvalidateRect(st->hwnd, nullptr, FALSE);
            }
        });
    }
}

void BackendBridge::runSearch(const std::string& query) {
    if (state_.loadingSearch.load()) return;
    state_.loadingSearch = true;
    {
        std::lock_guard<std::recursive_mutex> lk(state_.mtx);
        state_.searchQuery = query;
        state_.currentScreen = ScreenId::Discover;
    }
    auto adapters = adapters_;
    AppState* st  = &state_;
    auto ctx      = std::make_shared<MergeCtx>();
    ctx->remaining = static_cast<int>(adapters.size());

    if (adapters.empty()) {
        state_.loadingSearch = false;
        if (state_.hwnd) InvalidateRect(state_.hwnd, nullptr, FALSE);
        return;
    }

    for (const auto& a : adapters) {
        if (a->manager() == PackageManager::Winget) {
            auto winget = std::static_pointer_cast<WingetAdapter>(a);
            winget->setMsStoreSearchEnabled(state_.msStoreSearchEnabled);
        }
        a->search(query, [st, ctx, alive = alive_](std::vector<PackageInfo> pkgs, std::string err) {
            if (!*alive) return;
            try {
                {
                    std::lock_guard<std::mutex> lk(ctx->mergeMtx);
                    if (err.empty()) {
                        ctx->merged.insert(ctx->merged.end(),
                                           std::make_move_iterator(pkgs.begin()),
                                           std::make_move_iterator(pkgs.end()));
                    } else if (ctx->firstError.empty()) {
                        ctx->firstError = err;
                    }
                }
                if (--ctx->remaining == 0) {
                    {
                        std::lock_guard<std::recursive_mutex> lk(st->mtx);
                        if (!ctx->merged.empty()) {
                            st->searchResults = std::move(ctx->merged);
                        } else {
                            st->searchResults.clear();
                            st->lastError = ctx->firstError;
                        }
                    }
                    st->loadingSearch = false;
                    if (st->hwnd) {
                        InvalidateRect(st->hwnd, nullptr, FALSE);
                    }
                }
            } catch (const std::exception& e) {
                Logger::instance().error("Exception inside runSearch list callback: ", e.what());
                st->loadingSearch = false;
                if (st->hwnd) InvalidateRect(st->hwnd, nullptr, FALSE);
            } catch (...) {
                Logger::instance().error("Unknown exception inside runSearch list callback");
                st->loadingSearch = false;
                if (st->hwnd) InvalidateRect(st->hwnd, nullptr, FALSE);
            }
        });
    }
}

void BackendBridge::enqueueUpgradeAll() {
    std::vector<PackageInfo> toUpgrade;
    {
        std::lock_guard<std::recursive_mutex> lk(state_.mtx);
        toUpgrade = state_.upgradable;
    }
    for (const auto& p : toUpgrade) {
        enqueueUpgradeOne(p);
    }
}

void BackendBridge::enqueueUpgradeOne(const PackageInfo& pkg) {
    if (isInFlight(pkg.manager, pkg.id)) return;
    if (!findAdapter(pkg.manager)) {
        Logger::instance().warn("enqueueUpgradeOne: no adapter for manager=",
                                toString(pkg.manager));
        return;
    }
    markInFlight(pkg.manager, pkg.id);
    queue_->enqueue(pkg, TaskAction::Upgrade);
}

void BackendBridge::enqueueInstallOne(const PackageInfo& pkg) {
    if (isInFlight(pkg.manager, pkg.id)) return;
    if (!findAdapter(pkg.manager)) return;
    markInFlight(pkg.manager, pkg.id);
    queue_->enqueue(pkg, TaskAction::Install);
}

void BackendBridge::enqueueUninstallOne(const PackageInfo& pkg) {
    if (isInFlight(pkg.manager, pkg.id)) return;
    if (!findAdapter(pkg.manager)) {
        Logger::instance().warn("enqueueUninstallOne: no adapter for manager=",
                                toString(pkg.manager));
        return;
    }
    markInFlight(pkg.manager, pkg.id);
    queue_->enqueue(pkg, TaskAction::Uninstall);
}

std::vector<Task> BackendBridge::snapshotTasks() const {
    if (!queue_) return {};
    return queue_->snapshot();
}

int BackendBridge::pendingTasks() const { return queue_ ? queue_->pendingCount() : 0; }
int BackendBridge::activeTasks()  const { return queue_ ? queue_->activeCount()  : 0; }
int BackendBridge::doneTasks()    const { return queue_ ? queue_->doneCount()    : 0; }

void BackendBridge::markInFlight(PackageManager m, const std::string& packageId) {
    std::lock_guard<std::recursive_mutex> lk(state_.mtx);
    state_.inFlight.push_back({m, packageId});
}

void BackendBridge::unmarkInFlight(PackageManager m, const std::string& packageId) {
    std::lock_guard<std::recursive_mutex> lk(state_.mtx);
    auto it = std::find(state_.inFlight.begin(), state_.inFlight.end(),
                        AppState::InFlightKey{m, packageId});
    if (it != state_.inFlight.end()) state_.inFlight.erase(it);
}

bool BackendBridge::isInFlight(PackageManager m, const std::string& packageId) const {
    std::lock_guard<std::recursive_mutex> lk(state_.mtx);
    return std::find(state_.inFlight.begin(), state_.inFlight.end(),
                     AppState::InFlightKey{m, packageId}) != state_.inFlight.end();
}

std::shared_ptr<IPackageAdapter> BackendBridge::findAdapter(PackageManager m) const {
    for (const auto& a : adapters_) if (a->manager() == m) return a;
    return nullptr;
}

std::shared_ptr<IPackageAdapter> BackendBridge::findAdapterFor(const std::string& packageId) const {
    std::lock_guard<std::recursive_mutex> lk(state_.mtx);
    for (const auto& p : state_.upgradable) if (p.id == packageId) return findAdapter(p.manager);
    return nullptr;
}

void BackendBridge::setConcurrencyLimit(int limit) {
    std::lock_guard<std::recursive_mutex> lk(state_.mtx);
    state_.concurrencyLimit = std::max(1, limit);
    
    // Shut down the current queue (waits for active tasks to exit gracefully)
    if (queue_) {
        queue_->shutdown();
    }
    
    // Recreate the queue with the new concurrency limit
    queue_ = std::make_unique<TaskQueue>(state_.concurrencyLimit);
    
    std::vector<std::shared_ptr<IPackageAdapter>> activeAdapters = adapters_;
    activeAdapters.erase(std::remove_if(activeAdapters.begin(), activeAdapters.end(),
        [this](const std::shared_ptr<IPackageAdapter>& a) {
            if (a->manager() == PackageManager::Winget)     return !state_.wingetAvailable;
            if (a->manager() == PackageManager::Scoop)      return !state_.scoopAvailable;
            if (a->manager() == PackageManager::Chocolatey) return !state_.chocoAvailable;
            return true;
        }), activeAdapters.end());
    queue_->setAdapters(activeAdapters);

    queue_->setStateCallback([this, alive = alive_](const Task& t) {
        if (!*alive) return;
        if (t.state == InstallState::Installed || t.state == InstallState::Failed) {
            unmarkInFlight(t.package.manager, t.package.id);
        }
        if (t.state == InstallState::Installed) {
            refreshInstalled();
            refreshUpgradable();
        }
        if (state_.hwnd) {
            InvalidateRect(state_.hwnd, nullptr, FALSE);
        }
    });
    
    Logger::instance().info("TaskQueue concurrency updated dynamically to ", state_.concurrencyLimit);
}

void BackendBridge::clearCache() {
    std::lock_guard<std::recursive_mutex> lk(state_.mtx);
    
    // 1. Clear temporary winget export files
    try {
        auto tmpDir = std::filesystem::temp_directory_path();
        for (const auto& entry : std::filesystem::directory_iterator(tmpDir)) {
            if (entry.is_regular_file()) {
                auto name = entry.path().filename().string();
                if (name.find("winget_export_") == 0) {
                    std::error_code ec;
                    std::filesystem::remove(entry.path(), ec);
                }
            }
        }
    } catch (...) {}

    // 2. Clear package manager download caches safely
    auto clearDir = [](const std::wstring& envVar, const std::wstring& subPath) {
        wchar_t path[MAX_PATH];
        DWORD len = GetEnvironmentVariableW(envVar.c_str(), path, MAX_PATH);
        if (len > 0 && len < MAX_PATH) {
            std::filesystem::path dir = std::filesystem::path(path) / subPath;
            if (std::filesystem::exists(dir)) {
                std::error_code ec;
                std::filesystem::remove_all(dir, ec);
                std::filesystem::create_directories(dir, ec);
            }
        }
    };
    
    clearDir(L"LOCALAPPDATA", L"Temp\\WinGet");
    clearDir(L"USERPROFILE", L"Scoop\\cache");
    
    Logger::instance().info("Caches cleared successfully");
}

void BackendBridge::installTool(int toolIndex) {
    if (toolIndex == 0) {
        if (state_.installingWinget.load()) return;
        state_.installingWinget = true;
        
        std::thread([this, alive = alive_]() {
            SHELLEXECUTEINFOW sei = { sizeof(sei) };
            sei.cbSize = sizeof(sei);
            sei.fMask = SEE_MASK_NOCLOSEPROCESS;
            sei.lpVerb = L"open";
            sei.lpFile = L"powershell.exe";
            sei.lpParameters = L"-NoProfile -ExecutionPolicy Bypass -Command \"[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12; $ErrorActionPreference = 'Stop'; Write-Host 'Downloading Winget...'; try { Invoke-WebRequest -Uri 'https://github.com/microsoft/winget-cli/releases/latest/download/Microsoft.DesktopAppInstaller_8wekyb3d8bbwe.msixbundle' -OutFile '$env:TEMP\\winget.msixbundle'; Write-Host 'Installing Winget...'; Add-AppxPackage -Path '$env:TEMP\\winget.msixbundle' } catch { Write-Error $_ } Write-Host 'Process finished. Press Enter to close...'; Read-Host\"";
            sei.nShow = SW_SHOWNORMAL;
            if (ShellExecuteExW(&sei) && sei.hProcess) {
                WaitForSingleObject(sei.hProcess, INFINITE);
                CloseHandle(sei.hProcess);
            }
            if (*alive) {
                state_.installingWinget = false;
                detectTools();
                if (state_.hwnd) InvalidateRect(state_.hwnd, nullptr, FALSE);
            }
        }).detach();
    }
    else if (toolIndex == 1) {
        if (state_.installingScoop.load()) return;
        state_.installingScoop = true;
        
        std::thread([this, alive = alive_]() {
            SHELLEXECUTEINFOW sei = { sizeof(sei) };
            sei.cbSize = sizeof(sei);
            sei.fMask = SEE_MASK_NOCLOSEPROCESS;
            sei.lpVerb = L"open";
            sei.lpFile = L"powershell.exe";
            sei.lpParameters = L"-NoProfile -ExecutionPolicy Bypass -Command \"[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12; $ErrorActionPreference = 'Stop'; Write-Host 'Installing Scoop...'; try { iex (iwr -UseBasicParsing 'https://get.scoop.sh') } catch { Write-Error $_ } Write-Host 'Process finished. Press Enter to close...'; Read-Host\"";
            sei.nShow = SW_SHOWNORMAL;
            if (ShellExecuteExW(&sei) && sei.hProcess) {
                WaitForSingleObject(sei.hProcess, INFINITE);
                CloseHandle(sei.hProcess);
            }
            if (*alive) {
                state_.installingScoop = false;
                detectTools();
                if (state_.hwnd) InvalidateRect(state_.hwnd, nullptr, FALSE);
            }
        }).detach();
    }
    else if (toolIndex == 2) {
        if (state_.installingChoco.load()) return;
        state_.installingChoco = true;
        
        std::thread([this, alive = alive_]() {
            SHELLEXECUTEINFOW sei = { sizeof(sei) };
            sei.cbSize = sizeof(sei);
            sei.fMask = SEE_MASK_NOCLOSEPROCESS;
            sei.lpVerb = L"runas";
            sei.lpFile = L"powershell.exe";
            sei.lpParameters = L"-NoProfile -ExecutionPolicy Bypass -Command \"[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12; $ErrorActionPreference = 'Stop'; if (Test-Path 'C:\\ProgramData\\chocolatey') { if (-not (Test-Path 'C:\\ProgramData\\chocolatey\\bin\\choco.exe')) { Write-Host 'Leftover folder detected without choco.exe, cleaning up...'; Remove-Item -Path 'C:\\ProgramData\\chocolatey' -Recurse -Force } }; Write-Host 'Installing Chocolatey (requires Admin)...'; try { iex ((New-Object System.Net.WebClient).DownloadString('https://community.chocolatey.org/install.ps1')) } catch { Write-Error $_ } Write-Host 'Process finished. Press Enter to close...'; Read-Host\"";
            sei.nShow = SW_SHOWNORMAL;
            if (ShellExecuteExW(&sei) && sei.hProcess) {
                WaitForSingleObject(sei.hProcess, INFINITE);
                CloseHandle(sei.hProcess);
            }
            if (*alive) {
                state_.installingChoco = false;
                detectTools();
                if (state_.hwnd) InvalidateRect(state_.hwnd, nullptr, FALSE);
            }
        }).detach();
    }
}

} // namespace pm::gui