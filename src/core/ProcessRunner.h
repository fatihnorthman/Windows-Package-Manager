#pragma once

#include <string>
#include <vector>
#include <functional>
#include <atomic>
#include <thread>
#include <mutex>

namespace pm {

struct ProcessOptions {
    std::string                executable;
    std::vector<std::string>   arguments;
    std::string                workingDir;
    bool                       showWindow = false;
};

struct ProcessResult {
    int          exitCode = -1;
    std::string  stdoutText;
    std::string  stderrText;
    bool         cancelled = false;
};

using LineCallback  = std::function<void(const std::string& line, bool isStderr)>;
using ProgressCb    = std::function<void(int percent)>;
using CompleteCb    = std::function<void(const ProcessResult&)>;

class ProcessRunner {
public:
    ProcessRunner();
    ~ProcessRunner();

    ProcessRunner(const ProcessRunner&)            = delete;
    ProcessRunner& operator=(const ProcessRunner&) = delete;

    // Start an async process. Returns false if one is already running.
    bool start(const ProcessOptions& opts);

    // Subscribers
    void onLine(LineCallback cb)   { lineCb_     = std::move(cb); }
    void onProgress(ProgressCb cb) { progressCb_ = std::move(cb); }
    void onComplete(CompleteCb cb) { completeCb_ = std::move(cb); }

    // Block until the process finishes.
    void wait();

    // Best-effort termination of the running process.
    void cancel();

    bool isRunning() const { return running_.load(); }

private:
    void run();

    ProcessOptions opts_;
    LineCallback   lineCb_;
    ProgressCb     progressCb_;
    CompleteCb     completeCb_;

    std::atomic<bool> running_{false};
    std::atomic<bool> cancel_{false};
    std::thread       worker_;
    void*             processHandle_ = nullptr; // HANDLE
};

} // namespace pm
