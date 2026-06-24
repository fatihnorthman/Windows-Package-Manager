#pragma once

#include "../core/PackageInfo.h"
#include "../adapters/IPackageAdapter.h"
#include <string>
#include <vector>
#include <functional>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>
#include <memory>
#include <unordered_map>

namespace pm {

using TaskId = std::uint64_t;

struct Task {
    TaskId         id        = 0;
    PackageInfo    package;             // contains PackageManager (dispatch key)
    TaskAction     action    = TaskAction::Install;
    InstallState   state     = InstallState::Queued;
    int            progress  = 0;
    std::string    message;
};

using TaskStateCallback = std::function<void(const Task&)>;

class TaskQueue {
public:
    // concurrency = max parallel tasks. 0 = no tasks processed.
    explicit TaskQueue(int concurrency = 2);
    ~TaskQueue();

    TaskQueue(const TaskQueue&)            = delete;
    TaskQueue& operator=(const TaskQueue&) = delete;

    // Register adapters for dispatch. Tasks are routed to the adapter whose
    // manager() matches Task::package.manager. A manager without a bound
    // adapter will fail the task with "no adapter bound" instead of crashing.
    void setAdapters(std::vector<std::shared_ptr<IPackageAdapter>> adapters);
    void setStateCallback(TaskStateCallback cb);

    // Enqueue a new task. Returns the assigned id.
    TaskId enqueue(PackageInfo pkg, TaskAction action);

    // Snapshot of all known tasks (queued + completed).
    std::vector<Task> snapshot() const;

    // Stop workers, drop pending work.
    void shutdown();

    int  pendingCount() const;
    int  activeCount()  const;
    int  doneCount()    const;

private:
    void workerLoop();
    void emit(const Task& t);
    void emitLocked(const Task& t);   // caller already holds mtx_
    std::shared_ptr<IPackageAdapter> resolveAdapter(const Task& t) const;

    int                                  concurrency_;
    std::unordered_map<int, std::shared_ptr<IPackageAdapter>> adapters_; // by (int)PackageManager
    TaskStateCallback                    stateCb_;

    mutable std::mutex                   mtx_;
    std::condition_variable              cv_;
    std::queue<Task>                     queue_;        // pending
    std::vector<Task>                    all_;          // every task ever enqueued
    std::atomic<bool>                    stop_{false};
    std::vector<std::thread>             workers_;
    int                                  active_ = 0;   // protected by mtx_
};

} // namespace pm