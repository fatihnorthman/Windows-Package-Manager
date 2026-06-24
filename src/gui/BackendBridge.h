#pragma once

#include "../core/PackageInfo.h"
#include "../adapters/IPackageAdapter.h"
#include "../services/TaskQueue.h"
#include <array>
#include <memory>
#include <mutex>
#include <vector>
#include <string>
#include <atomic>

namespace pm::gui {

enum class ScreenId {
    Discover = 0,
    Installed,
    Updates,
    Tasks,
    Settings
};

struct AppState {
    mutable std::mutex               mtx;

    ScreenId                         currentScreen = ScreenId::Updates;

    // Loaded package lists
    std::vector<PackageInfo>         installed;
    std::vector<PackageInfo>         upgradable;
    std::vector<PackageInfo>         searchResults;
    std::string                      searchQuery;

    // Loading flags
    std::atomic<bool>                loadingInstalled{false};
    std::atomic<bool>                loadingUpgradable{false};
    std::atomic<bool>                loadingSearch{false};

    // Error messages (cleared on next successful load)
    std::string                      lastError;

    // Package IDs that have an in-flight task in the queue (for button state).
    // Stored as (manager, id) pairs so we don't collide between tools that
    // happen to share an id (e.g. both winget and scoop ship "git").
    struct InFlightKey {
        PackageManager manager;
        std::string    id;
        bool operator==(const InFlightKey& o) const {
            return manager == o.manager && id == o.id;
        }
    };
    mutable std::vector<InFlightKey> inFlight;

    // Tool availability (set by detectTools() at startup).
    bool                             wingetAvailable = false;
    bool                             scoopAvailable  = false;
    bool                             chocoAvailable  = false;

    // Bottom task-queue drawer UI state (collapsed vs expanded).
    bool                             tasksDrawerOpen = false;

    // Per-screen scroll offset (rows). One slot per ScreenId; populated by
    // WM_MOUSEWHEEL, consumed by Screens::draw. We keep one slot per screen
    // so navigating away and back doesn't lose the user's position.
    std::array<int, 5>               scrollOffset{0, 0, 0, 0, 0};

    // Single-line text input. The Discover search box and the top-bar
    // search both bind to this; whichever gets a click takes focus, and
    // WM_CHAR feeds characters into the buffer. Pressing Enter triggers
    // bridge.runSearch() with the current text.
    struct TextInput {
        std::string   text;
        bool          focused = false;
        // Geometry of the currently focused field, refreshed by the
        // drawer each frame. Hit-test uses these to figure out whether
        // a click landed in the field.
        float         boxX = 0, boxY = 0, boxW = 0, boxH = 0;
        bool          boxValid = false;
    };
    TextInput                        searchInput;
};

// BackendBridge owns the adapters and queue. UI calls into bridge methods;
// bridge spawns detached threads and updates AppState under its own mutex.
class BackendBridge {
public:
    BackendBridge();
    ~BackendBridge();

    void init();  // Set up adapters and queue with default concurrency.

    // Probe PATH for winget, scoop, chocolatey and store results in AppState.
    void detectTools();

    // Trigger async loads. Returns immediately. Results land in AppState.
    void refreshInstalled();
    void refreshUpgradable();
    void runSearch(const std::string& query);

    // Enqueue tasks. Caller must pass the full PackageInfo (so we know which
    // adapter to dispatch to).
    void enqueueUpgradeAll();
    void enqueueUpgradeOne(const PackageInfo& pkg);
    void enqueueInstallOne(const PackageInfo& pkg);
    void enqueueUninstallOne(const PackageInfo& pkg);

    // Snapshot of current tasks for the UI.
    std::vector<Task> snapshotTasks() const;

    int  pendingTasks() const;
    int  activeTasks()  const;
    int  doneTasks()    const;

    // Read shared state (caller must hold AppState.mtx if mutating).
    AppState& state() { return state_; }
    const AppState& state() const { return state_; }

private:
    void markInFlight(PackageManager m, const std::string& packageId);
    void unmarkInFlight(PackageManager m, const std::string& packageId);
    bool isInFlight(PackageManager m, const std::string& packageId) const;

    // Find the adapter that owns `id` in the in-memory snapshot list (state_).
    // Used when only an id is known (e.g. "Update All" button).
    std::shared_ptr<IPackageAdapter> findAdapterFor(const std::string& packageId) const;
    std::shared_ptr<IPackageAdapter> findAdapter(PackageManager m) const;

    AppState                                state_;
    std::vector<std::shared_ptr<IPackageAdapter>> adapters_;
    std::unique_ptr<TaskQueue>              queue_;
};

} // namespace pm::gui