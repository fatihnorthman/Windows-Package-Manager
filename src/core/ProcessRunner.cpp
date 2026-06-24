#include "ProcessRunner.h"
#include "Logger.h"

#include <windows.h>
#include <regex>
#include <string>
#include <thread>
#include <atomic>

namespace pm {

namespace {

struct Pipes {
    HANDLE read  = INVALID_HANDLE_VALUE;
    HANDLE write = INVALID_HANDLE_VALUE;
    bool   ok() const { return read != INVALID_HANDLE_VALUE && write != INVALID_HANDLE_VALUE; }
};

Pipes makePipe() {
    Pipes p;
    SECURITY_ATTRIBUTES sa{};
    sa.nLength        = sizeof(sa);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = nullptr;
    if (!CreatePipe(&p.read, &p.write, &sa, 0)) return p;
    SetHandleInformation(p.read, HANDLE_FLAG_INHERIT, 0);
    return p;
}

std::wstring utf8ToWide(const std::string& s) {
    if (s.empty()) return {};
    int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    if (len <= 0) return {};
    std::wstring out(static_cast<size_t>(len - 1), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, out.data(), len);
    return out;
}

// Strip trailing \r (winget uses \r for live progress on a single line)
void rstripCarriageReturn(std::string& line) {
    while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) line.pop_back();
}

} // anonymous

ProcessRunner::ProcessRunner() = default;

ProcessRunner::~ProcessRunner() {
    cancel();
    if (worker_.joinable()) {
        if (worker_.get_id() == std::this_thread::get_id()) {
            worker_.detach();
        } else {
            worker_.join();
        }
    }
}

bool ProcessRunner::start(const ProcessOptions& opts) {
    if (running_.load()) return false;
    // Join any leftover worker from a previous run so we don't call
    // std::thread::operator= on a joinable thread (which is UB / terminate).
    if (worker_.joinable()) worker_.join();
    opts_   = opts;
    cancel_ = false;
    running_ = true;
    worker_  = std::thread([this] { run(); });
    return true;
}

void ProcessRunner::run() {
    try {
        auto outP = makePipe();
        auto errP = makePipe();
        if (!outP.ok() || !errP.ok()) {
            Logger::instance().error("ProcessRunner: pipe creation failed");
            running_ = false;
            CompleteCb cb = std::move(completeCb_);
            progressCb_ = nullptr;
            lineCb_     = nullptr;
            if (cb) cb(ProcessResult{});
            return;
        }

        STARTUPINFOW si{};
        si.cb         = sizeof(si);
        si.dwFlags    = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
        si.hStdInput  = GetStdHandle(STD_INPUT_HANDLE);
        si.hStdOutput = outP.write;
        si.hStdError  = errP.write;
        si.wShowWindow = SW_HIDE;

        std::wstring cmd = L"\"" + utf8ToWide(opts_.executable) + L"\"";
        for (const auto& a : opts_.arguments) {
            cmd += L" \"" + utf8ToWide(a) + L"\"";
        }
        std::vector<wchar_t> cmdBuf(cmd.begin(), cmd.end());
        cmdBuf.push_back(L'\0');

        std::wstring workDir = opts_.workingDir.empty() ? L"" : utf8ToWide(opts_.workingDir);

        PROCESS_INFORMATION pi{};
        BOOL ok = CreateProcessW(
            nullptr,
            cmdBuf.data(),
            nullptr, nullptr,
            TRUE,
            opts_.showWindow ? 0 : CREATE_NO_WINDOW,
            nullptr,
            workDir.empty() ? nullptr : workDir.c_str(),
            &si,
            &pi
        );
        if (!ok) {
            DWORD err = GetLastError();
            Logger::instance().error("CreateProcess failed (err=", err, "): ", opts_.executable);
            CloseHandle(outP.read);  CloseHandle(outP.write);
            CloseHandle(errP.read);  CloseHandle(errP.write);
            running_ = false;
            CompleteCb cb = std::move(completeCb_);
            progressCb_ = nullptr;
            lineCb_     = nullptr;
            if (cb) cb(ProcessResult{});
            return;
        }

        {
            std::lock_guard<std::mutex> lk(handleMtx_);
            processHandle_ = pi.hProcess;
        }
        CloseHandle(pi.hThread);
        CloseHandle(outP.write);
        CloseHandle(errP.write);

        // Shared collected text + simple progress percent
        std::string outBuf, errBuf;
        std::atomic<int> lastPercent{-1};
        const std::regex percentRe(R"((\d{1,3})\s*%)");

        // Process a single complete line: notify line callback and extract
        // progress percentage if present.
        auto processLine = [&](const std::string& line, bool isErr) {
            try {
                std::string trimmed = line;
                rstripCarriageReturn(trimmed);
                if (trimmed.empty()) return;
                if (lineCb_) lineCb_(trimmed, isErr);
                if (progressCb_) {
                    std::smatch m;
                    if (std::regex_search(trimmed, m, percentRe)) {
                        int p = std::stoi(m[1].str());
                        int expected = lastPercent.load();
                        while (p > expected && p >= 0 && p <= 100) {
                            if (lastPercent.compare_exchange_weak(expected, p)) {
                                progressCb_(p);
                                break;
                            }
                        }
                    }
                }
            } catch (const std::exception& e) {
                Logger::instance().error("ProcessRunner exception inside processLine: ", e.what());
            } catch (...) {
                Logger::instance().error("ProcessRunner unknown exception inside processLine");
            }
        };

        auto drain = [&](HANDLE h, bool isErr) {
            char  buf[4096];
            DWORD bytesRead = 0;
            std::string partial;
            HANDLE procH = nullptr;
            {
                std::lock_guard<std::mutex> lk(handleMtx_);
                procH = static_cast<HANDLE>(processHandle_);
            }
            while (!cancel_.load()) {
                DWORD avail = 0;
                if (!PeekNamedPipe(h, nullptr, 0, nullptr, &avail, nullptr)) break;
                if (avail == 0) {
                    DWORD waitRes = WaitForSingleObject(procH, 50);
                    if (waitRes == WAIT_OBJECT_0) {
                        // Process ended — drain all remaining bytes.
                        while (PeekNamedPipe(h, nullptr, 0, nullptr, &avail, nullptr) && avail > 0) {
                            DWORD toRead = (avail < sizeof(buf) - 1) ? avail : (sizeof(buf) - 1);
                            if (ReadFile(h, buf, toRead, &bytesRead, nullptr) && bytesRead > 0) {
                                buf[bytesRead] = '\0';
                                (isErr ? errBuf : outBuf).append(buf, bytesRead);
                                partial.append(buf, bytesRead);
                            } else {
                                break;
                            }
                        }
                        break;
                    }
                    continue;
                }
                if (ReadFile(h, buf, sizeof(buf) - 1, &bytesRead, nullptr) && bytesRead > 0) {
                    buf[bytesRead] = '\0';
                    (isErr ? errBuf : outBuf).append(buf, bytesRead);
                    partial.append(buf, bytesRead);
                    // Parse complete lines from the accumulated partial buffer.
                    size_t pos = 0;
                    size_t nl;
                    while ((nl = partial.find('\n', pos)) != std::string::npos) {
                        processLine(partial.substr(pos, nl - pos), isErr);
                        pos = nl + 1;
                    }
                    if (pos > 0) partial = partial.substr(pos);
                }
            }
            // Flush any remaining partial line
            if (!partial.empty()) {
                processLine(partial, isErr);
            }
        };

        std::thread outThread([&] {
            try {
                drain(outP.read, false);
            } catch (const std::exception& e) {
                Logger::instance().error("ProcessRunner stdout drain thread exception: ", e.what());
            } catch (...) {
                Logger::instance().error("ProcessRunner stdout drain thread unknown exception");
            }
        });
        std::thread errThread([&] {
            try {
                drain(errP.read, true);
            } catch (const std::exception& e) {
                Logger::instance().error("ProcessRunner stderr drain thread exception: ", e.what());
            } catch (...) {
                Logger::instance().error("ProcessRunner stderr drain thread unknown exception");
            }
        });

        WaitForSingleObject(processHandle_, INFINITE);
        outThread.join();
        errThread.join();

        DWORD exitCode = 0;
        {
            std::lock_guard<std::mutex> lk(handleMtx_);
            GetExitCodeProcess(processHandle_, &exitCode);
            CloseHandle(processHandle_);
            processHandle_ = nullptr;
        }
        CloseHandle(outP.read);
        CloseHandle(errP.read);

        running_ = false;

        ProcessResult res;
        res.exitCode  = static_cast<int>(exitCode);
        res.stdoutText = std::move(outBuf);
        res.stderrText = std::move(errBuf);
        res.cancelled  = cancel_.load();

        CompleteCb cb = std::move(completeCb_);
        progressCb_ = nullptr;
        lineCb_     = nullptr;
        if (cb) cb(res);
    } catch (const std::exception& e) {
        Logger::instance().error("ProcessRunner exception in run(): ", e.what());
        running_ = false;
        CompleteCb cb = std::move(completeCb_);
        progressCb_ = nullptr;
        lineCb_     = nullptr;
        if (cb) {
            ProcessResult res;
            res.exitCode = -1;
            res.stderrText = std::string("Internal exception: ") + e.what();
            cb(res);
        }
    } catch (...) {
        Logger::instance().error("ProcessRunner unknown exception in run()");
        running_ = false;
        CompleteCb cb = std::move(completeCb_);
        progressCb_ = nullptr;
        lineCb_     = nullptr;
        if (cb) {
            ProcessResult res;
            res.exitCode = -1;
            res.stderrText = "Unknown internal exception";
            cb(res);
        }
    }
}

void ProcessRunner::wait() {
    if (worker_.joinable()) worker_.join();
}

void ProcessRunner::cancel() {
    cancel_ = true;
    std::lock_guard<std::mutex> lk(handleMtx_);
    HANDLE h = static_cast<HANDLE>(processHandle_);
    if (h) TerminateProcess(h, 1);
}

} // namespace pm
