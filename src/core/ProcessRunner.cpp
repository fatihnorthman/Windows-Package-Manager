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
    if (worker_.joinable()) worker_.join();
}

bool ProcessRunner::start(const ProcessOptions& opts) {
    if (running_.load()) return false;
    opts_   = opts;
    cancel_ = false;
    running_ = true;
    worker_  = std::thread([this] { run(); });
    return true;
}

void ProcessRunner::run() {
    auto outP = makePipe();
    auto errP = makePipe();
    if (!outP.ok() || !errP.ok()) {
        Logger::instance().error("ProcessRunner: pipe creation failed");
        running_ = false;
        if (completeCb_) completeCb_(ProcessResult{});
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
        if (completeCb_) completeCb_(ProcessResult{});
        return;
    }

    processHandle_ = pi.hProcess;
    CloseHandle(pi.hThread);
    CloseHandle(outP.write);
    CloseHandle(errP.write);

    // Shared collected text + simple progress percent
    std::string outBuf, errBuf;
    int          lastPercent = -1;
    const std::regex percentRe(R"((\d{1,3})\s*%)");

    auto drain = [&](HANDLE h, bool isErr) {
        char  buf[4096];
        DWORD bytesRead = 0;
        std::string partial;
        while (!cancel_.load()) {
            DWORD avail = 0;
            if (!PeekNamedPipe(h, nullptr, 0, nullptr, &avail, nullptr)) break;
            if (avail == 0) {
                DWORD waitRes = WaitForSingleObject(processHandle_, 50);
                if (waitRes == WAIT_OBJECT_0) {
                    // Process ended — drain remaining bytes.
                    if (PeekNamedPipe(h, nullptr, 0, nullptr, &avail, nullptr) && avail > 0) {
                        if (ReadFile(h, buf, sizeof(buf) - 1, &bytesRead, nullptr) && bytesRead > 0) {
                            buf[bytesRead] = '\0';
                            (isErr ? errBuf : outBuf).append(buf, bytesRead);
                            partial.append(buf, bytesRead);
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
            }
        }
        // Flush any remaining partial line
        if (!partial.empty()) {
            rstripCarriageReturn(partial);
            if (lineCb_) lineCb_(partial, isErr);
            if (progressCb_) {
                std::smatch m;
                if (std::regex_search(partial, m, percentRe)) {
                    int p = std::stoi(m[1].str());
                    if (p != lastPercent && p >= 0 && p <= 100) {
                        lastPercent = p;
                        progressCb_(p);
                    }
                }
            }
        }
    };

    std::thread outThread([&] { drain(outP.read, false); });
    std::thread errThread([&] { drain(errP.read, true);  });

    WaitForSingleObject(processHandle_, INFINITE);
    outThread.join();
    errThread.join();

    DWORD exitCode = 0;
    GetExitCodeProcess(processHandle_, &exitCode);
    CloseHandle(processHandle_);
    CloseHandle(outP.read);
    CloseHandle(errP.read);
    processHandle_ = nullptr;

    running_ = false;

    ProcessResult res;
    res.exitCode  = static_cast<int>(exitCode);
    res.stdoutText = std::move(outBuf);
    res.stderrText = std::move(errBuf);
    res.cancelled  = cancel_.load();

    if (completeCb_) completeCb_(res);
}

void ProcessRunner::wait() {
    if (worker_.joinable()) worker_.join();
}

void ProcessRunner::cancel() {
    cancel_ = true;
    HANDLE h = static_cast<HANDLE>(processHandle_);
    if (h) TerminateProcess(h, 1);
}

} // namespace pm
