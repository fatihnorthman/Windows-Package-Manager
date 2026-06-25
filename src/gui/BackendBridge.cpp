#include "BackendBridge.h"
#include "../adapters/WingetAdapter.h"
#include "../adapters/ScoopAdapter.h"
#include "../adapters/ChocoAdapter.h"
#include "../core/Logger.h"
#include "../core/ProcessRunner.h"
#include <algorithm>
#include <memory>
#include <thread>

namespace pm::gui {

BackendBridge::BackendBridge() = default;

BackendBridge::~BackendBridge() {
    // queue_'s destructor shuts down workers.
}

void BackendBridge::init() {
    // Construct every adapter; we'll prune the ones whose tool isn't on PATH
    // *before* giving the list to the TaskQueue, otherwise the queue would
    // still try to dispatch to a missing executable.
    adapters_.push_back(std::make_shared<WingetAdapter>());
    adapters_.push_back(std::make_shared<ScoopAdapter>());
    adapters_.push_back(std::make_shared<ChocoAdapter>());

    detectTools();
    adapters_.erase(std::remove_if(adapters_.begin(), adapters_.end(),
        [this](const std::shared_ptr<IPackageAdapter>& a) {
            if (a->manager() == PackageManager::Winget)     return !state_.wingetAvailable;
            if (a->manager() == PackageManager::Scoop)      return !state_.scoopAvailable;
            if (a->manager() == PackageManager::Chocolatey) return !state_.chocoAvailable;
            return true;
        }), adapters_.end());

    queue_ = std::make_unique<TaskQueue>(2);
    queue_->setAdapters(adapters_);
    queue_->setStateCallback([this](const Task& t) {
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

    Logger::instance().info("BackendBridge ready: ", adapters_.size(), " adapter(s) active");
}

void BackendBridge::detectTools() {
    // Probe by running each adapter's own isAvailable() — same code path the
    // adapter will use, so we get consistent results (e.g. winget's --version
    // vs scoop's --version may differ in startup cost).
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
        a->listInstalled([st, ctx](std::vector<PackageInfo> pkgs, std::string err) {
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
        a->listUpgradable([st, ctx](std::vector<PackageInfo> pkgs, std::string err) {
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
        a->search(query, [st, ctx](std::vector<PackageInfo> pkgs, std::string err) {
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

} // namespace pm::gui