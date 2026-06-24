#include "TaskQueue.h"
#include "../core/Logger.h"
#include <algorithm>
#include <future>

namespace pm {

namespace {
std::atomic<TaskId> gNextId{1};
}

TaskQueue::TaskQueue(int concurrency) : concurrency_(std::max(1, concurrency)) {
    workers_.reserve(concurrency_);
    for (int i = 0; i < concurrency_; ++i) {
        workers_.emplace_back([this] { workerLoop(); });
    }
    Logger::instance().info("TaskQueue started with concurrency=", concurrency_);
}

TaskQueue::~TaskQueue() { shutdown(); }

void TaskQueue::setAdapters(std::vector<std::shared_ptr<IPackageAdapter>> adapters) {
    std::lock_guard<std::mutex> lk(mtx_);
    adapters_.clear();
    for (auto& a : adapters) {
        if (a) adapters_[static_cast<int>(a->manager())] = a;
    }
}

void TaskQueue::setStateCallback(TaskStateCallback cb) {
    std::lock_guard<std::mutex> lk(mtx_);
    stateCb_ = std::move(cb);
}

TaskId TaskQueue::enqueue(PackageInfo pkg, TaskAction action) {
    Task t;
    t.id      = gNextId.fetch_add(1);
    t.package = std::move(pkg);
    t.action  = action;
    t.state   = InstallState::Queued;
    {
        std::lock_guard<std::mutex> lk(mtx_);
        all_.push_back(t);
        queue_.push(t);
    }
    Logger::instance().info("Task enqueued id=", t.id, " mgr=", toString(t.package.manager),
                            " pkg=", t.package.id, " action=", toString(action));
    emit(t);
    cv_.notify_one();
    return t.id;
}

std::vector<Task> TaskQueue::snapshot() const {
    std::lock_guard<std::mutex> lk(mtx_);
    return all_;
}

int TaskQueue::pendingCount() const {
    std::lock_guard<std::mutex> lk(mtx_);
    return static_cast<int>(queue_.size());
}

int TaskQueue::activeCount() const {
    std::lock_guard<std::mutex> lk(mtx_);
    return active_;
}

int TaskQueue::doneCount() const {
    std::lock_guard<std::mutex> lk(mtx_);
    int n = 0;
    for (const auto& t : all_)
        if (t.state == InstallState::Installed || t.state == InstallState::Failed || t.state == InstallState::UpToDate)
            ++n;
    return n;
}

std::shared_ptr<IPackageAdapter> TaskQueue::resolveAdapter(const Task& t) const {
    auto it = adapters_.find(static_cast<int>(t.package.manager));
    return it == adapters_.end() ? nullptr : it->second;
}

void TaskQueue::workerLoop() {
    auto findSlot = [this](TaskId id) -> Task* {
        for (auto& t : all_) if (t.id == id) return &t;
        return nullptr;
    };

    while (true) {
        try {
            Task t;
            {
                std::unique_lock<std::mutex> lk(mtx_);
                cv_.wait(lk, [this] { return stop_.load() || !queue_.empty(); });
                if (stop_.load() && queue_.empty()) return;
                t = queue_.front();
                queue_.pop();
                ++active_;

                if (auto* slot = findSlot(t.id)) {
                    slot->state    = (t.action == TaskAction::Upgrade   ? InstallState::Updating
                                   : t.action == TaskAction::Uninstall ? InstallState::Installing
                                   :                                     InstallState::Installing);
                    slot->progress = 0;
                    t = *slot;
                }
            }
            Logger::instance().info("Task started id=", t.id, " pkg=", t.package.id);
            emit(t);

            auto adapter = resolveAdapter(t);
            if (!adapter) {
                std::lock_guard<std::mutex> lk(mtx_);
                if (auto* slot = findSlot(t.id)) {
                    slot->state   = InstallState::Failed;
                    slot->message = std::string("no adapter bound for manager=") + std::string(toString(t.package.manager));
                    t = *slot;
                }
                --active_;
                Logger::instance().error("Task id=", t.id, " failed: no adapter for manager=", toString(t.package.manager));
                emit(t);
                cv_.notify_one();
                continue;
            }

            try {
                auto doneFlag = std::make_shared<std::promise<bool>>();
                auto fut      = doneFlag->get_future();

                adapter->performAction(
                    t.package,
                    t.action,
                    [this, id = t.id](int percent) {
                        try {
                            Task updatedTask;
                            bool shouldEmit = false;
                            {
                                std::lock_guard<std::mutex> lk(mtx_);
                                if (auto* slot = [this, id]() -> Task* {
                                    for (auto& x : all_) if (x.id == id) return &x;
                                    return nullptr;
                                }()) {
                                    if (percent > slot->progress) {
                                        slot->progress = percent;
                                        updatedTask = *slot;
                                        shouldEmit = true;
                                    }
                                }
                            }
                            if (shouldEmit) {
                                emit(updatedTask);
                            }
                        } catch (const std::exception& e) {
                            Logger::instance().error("Exception inside TaskQueue progress callback: ", e.what());
                        } catch (...) {
                            Logger::instance().error("Unknown exception inside TaskQueue progress callback");
                        }
                    },
                    [this, id = t.id, doneFlag](bool ok, std::string msg) {
                        try {
                            Task updatedTask;
                            bool shouldEmit = false;
                            {
                                std::lock_guard<std::mutex> lk(mtx_);
                                for (auto& x : all_) {
                                    if (x.id == id) {
                                        x.state   = ok ? InstallState::Installed : InstallState::Failed;
                                        x.message = std::move(msg);
                                        if (ok) x.progress = 100;
                                        if (!ok) {
                                            Logger::instance().error("Task id=", x.id, " pkg=", x.package.id, " action=", toString(x.action), " FAILED: ", x.message);
                                        } else {
                                            Logger::instance().info("Task id=", x.id, " pkg=", x.package.id, " action=", toString(x.action), " completed successfully");
                                        }
                                        updatedTask = x;
                                        shouldEmit = true;
                                        break;
                                    }
                                }
                            }
                            if (shouldEmit) {
                                emit(updatedTask);
                            }
                            doneFlag->set_value(ok);
                        } catch (const std::exception& e) {
                            Logger::instance().error("Exception inside TaskQueue done callback: ", e.what());
                            try { doneFlag->set_value(false); } catch (...) {}
                        } catch (...) {
                            Logger::instance().error("Unknown exception inside TaskQueue done callback");
                            try { doneFlag->set_value(false); } catch (...) {}
                        }
                    });

                fut.wait();
            } catch (const std::exception& e) {
                Logger::instance().error("Exception executing task id=", t.id, ": ", e.what());
                {
                    std::lock_guard<std::mutex> lk(mtx_);
                    if (auto* slot = findSlot(t.id)) {
                        slot->state   = InstallState::Failed;
                        slot->message = std::string("Execution exception: ") + e.what();
                        t = *slot;
                    }
                }
                emit(t);
            } catch (...) {
                Logger::instance().error("Unknown exception executing task id=", t.id);
                {
                    std::lock_guard<std::mutex> lk(mtx_);
                    if (auto* slot = findSlot(t.id)) {
                        slot->state   = InstallState::Failed;
                        slot->message = "Unknown execution exception";
                        t = *slot;
                    }
                }
                emit(t);
            }

            {
                std::lock_guard<std::mutex> lk(mtx_);
                --active_;
            }
            cv_.notify_one();
        } catch (const std::exception& e) {
            Logger::instance().error("Exception in TaskQueue workerLoop: ", e.what());
        } catch (...) {
            Logger::instance().error("Unknown exception in TaskQueue workerLoop");
        }
    }
}

void TaskQueue::emit(const Task& t) {
    TaskStateCallback cb;
    {
        std::lock_guard<std::mutex> lk(mtx_);
        cb = stateCb_;
    }
    if (cb) {
        try { cb(t); } catch (const std::exception& e) {
            Logger::instance().error("TaskQueue state callback threw: ", e.what());
        }
    }
}


void TaskQueue::shutdown() {
    {
        std::lock_guard<std::mutex> lk(mtx_);
        stop_ = true;
    }
    cv_.notify_all();
    for (auto& t : workers_) if (t.joinable()) t.join();
    workers_.clear();
}

} // namespace pm