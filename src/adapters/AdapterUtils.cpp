#include "AdapterUtils.h"
#include "../core/Logger.h"
#include <atomic>
#include <chrono>
#include <future>
#include <memory>
#include <thread>
#include <sstream>
#include <algorithm>
#include <cctype>

namespace pm::adapters {

namespace {
// Intentionally empty anonymous namespace (keeps TU self-contained).
}

void runAndParseAsync(
    ProcessOptions opt,
    std::function<std::vector<PackageInfo>(const std::string&)> parseFn,
    PackageListCallback cb) {
    try {
        auto promise = std::make_shared<std::promise<void>>();
        auto fut     = promise->get_future();
        auto runner  = std::make_shared<ProcessRunner>();
        runner->onComplete([cb, parseFn = std::move(parseFn), promise](const ProcessResult& res) mutable {
            try {
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
            } catch (const std::exception& e) {
                Logger::instance().error("Exception in runAndParseAsync callback: ", e.what());
            } catch (...) {
                Logger::instance().error("Unknown exception in runAndParseAsync callback");
            }
            try { promise->set_value(); } catch (...) {}
        });
        if (!runner->start(opt)) {
            if (cb) cb({}, "ProcessRunner busy");
            try { promise->set_value(); } catch (...) {}
            return;
        }
        fut.wait();
    } catch (const std::exception& e) {
        Logger::instance().error("Exception in runAndParseAsync: ", e.what());
        if (cb) cb({}, std::string("Internal exception: ") + e.what());
    } catch (...) {
        Logger::instance().error("Unknown exception in runAndParseAsync");
        if (cb) cb({}, "Unknown internal exception");
    }
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

std::string extractError(const ProcessResult& res) {
    if (!res.stderrText.empty()) {
        std::string err = res.stderrText;
        while (!err.empty() && (err.back() == '\n' || err.back() == '\r' || err.back() == ' ')) err.pop_back();
        return err;
    }
    if (res.stdoutText.empty()) return "Unknown error";

    std::istringstream iss(res.stdoutText);
    std::string line;
    std::string lastLine;
    std::string firstErrorLine;
    while (std::getline(iss, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        auto start = line.find_first_not_of(" \t");
        if (start == std::string::npos) continue;
        auto end = line.find_last_not_of(" \t");
        std::string trimmed = line.substr(start, end - start + 1);

        lastLine = trimmed;

        std::string lower = trimmed;
        std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) -> char { return static_cast<char>(std::tolower(c)); });
        if (firstErrorLine.empty() &&
            (lower.find("error") != std::string::npos ||
             lower.find("failed") != std::string::npos ||
             lower.find("baþarýsýz") != std::string::npos ||
             lower.find("basarisiz") != std::string::npos ||
             lower.find("yetki") != std::string::npos ||
             lower.find("yönetici") != std::string::npos ||
             lower.find("yonetici") != std::string::npos ||
             lower.find("denied") != std::string::npos ||
             lower.find("engellendi") != std::string::npos)) {
            firstErrorLine = trimmed;
        }
    }
    if (!firstErrorLine.empty()) return firstErrorLine;
    return lastLine;
}

} // namespace pm::adapters