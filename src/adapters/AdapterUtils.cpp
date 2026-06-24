#include "AdapterUtils.h"
#include "../core/Logger.h"
#include <atomic>
#include <chrono>
#include <future>
#include <memory>
#include <thread>

namespace pm::adapters {

namespace {
std::string utf8ToWide_unused; // keep file self-contained
}

void runAndParseAsync(
    ProcessOptions opt,
    std::function<std::vector<PackageInfo>(const std::string&)> parseFn,
    PackageListCallback cb) {
    auto promise = std::make_shared<std::promise<void>>();
    auto fut     = promise->get_future();
    auto runner  = std::make_shared<ProcessRunner>();
    runner->onComplete([cb, parseFn = std::move(parseFn), promise](const ProcessResult& res) mutable {
        std::vector<PackageInfo> pkgs;
        std::string err;
        if (res.cancelled) {
            err = "cancelled";
        } else if (res.exitCode != 0) {
            err = "exited with code " + std::to_string(res.exitCode);
            if (!res.stderrText.empty()) err += ": " + res.stderrText.substr(0, 200);
        } else {
            try { pkgs = parseFn(res.stdoutText); }
            catch (const std::exception& e) { err = std::string("parse: ") + e.what(); }
        }
        if (cb) cb(std::move(pkgs), std::move(err));
        promise->set_value();
    });
    if (!runner->start(opt)) {
        if (cb) cb({}, "ProcessRunner busy");
        promise->set_value();
        return;
    }
    fut.wait();
}

bool probeVersion(const std::string& executable) {
    ProcessOptions opt;
    opt.executable = executable;
    opt.arguments  = { "--version" };
    ProcessRunner r;
    std::atomic<int> done{0};
    bool ok = false;
    r.onComplete([&](const ProcessResult& res) {
        ok = (res.exitCode == 0);
        done.store(1);
    });
    if (!r.start(opt)) return false;
    // Cap wait at 2s so a hung tool doesn't stall boot forever.
    for (int i = 0; i < 40 && done.load() == 0; ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    return ok && done.load() == 1;
}

} // namespace pm::adapters