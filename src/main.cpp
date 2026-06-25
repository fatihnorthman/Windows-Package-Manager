// Unified Windows Package Manager - native Win32 + Direct2D entry point
//
// Implements (per plan.md):
//   - Aşama 1: ProcessRunner (async winget invocations)
//   - Aşama 2: PackageInfo + WingetAdapter
//   - Aşama 4: TaskQueue (thread-safe, concurrency limit)
//   - Aşama 6: Native Win32 + Direct2D + DirectWrite GUI (no XAML, no web)

#include "core/Logger.h"
#include "gui/win32/Application.h"

#include <windows.h>
#include <dbghelp.h>
#include <cstdio>
#include <fstream>
#include <chrono>
#include <iomanip>
#include <algorithm>
#include <exception>

// Function pointer for MiniDumpWriteDump
typedef BOOL(WINAPI* MINIDUMPWRITEDUMP)(
    HANDLE hProcess,
    DWORD ProcessId,
    HANDLE hFile,
    MINIDUMP_TYPE DumpType,
    PMINIDUMP_EXCEPTION_INFORMATION ExceptionParam,
    PMINIDUMP_USER_STREAM_INFORMATION UserStreamParam,
    PMINIDUMP_CALLBACK_INFORMATION CallbackParam
);

LONG WINAPI CrashHandler(EXCEPTION_POINTERS* exceptionInfo) {
    // 1. Write crash log
    std::ofstream log("crash.log", std::ios::app);
    auto now = std::chrono::system_clock::now();
    auto t   = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
    localtime_s(&tm, &t);
    
    log << "========================================\n";
    log << "[" << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") << "] CRASH DETECTED!\n";
    
    if (exceptionInfo && exceptionInfo->ExceptionRecord) {
        DWORD code = exceptionInfo->ExceptionRecord->ExceptionCode;
        void* addr = exceptionInfo->ExceptionRecord->ExceptionAddress;
        log << "Exception Code: 0x" << std::hex << std::uppercase << code << "\n";
        log << "Exception Address: 0x" << addr << "\n";
        
        switch (code) {
            case EXCEPTION_ACCESS_VIOLATION:
                log << "Reason: EXCEPTION_ACCESS_VIOLATION (Access violation)\n";
                if (exceptionInfo->ExceptionRecord->NumberParameters >= 2) {
                    ULONG_PTR write = exceptionInfo->ExceptionRecord->ExceptionInformation[0];
                    ULONG_PTR target = exceptionInfo->ExceptionRecord->ExceptionInformation[1];
                    log << (write ? "Attempted to write to" : "Attempted to read from")
                        << " memory address 0x" << std::hex << target << "\n";
                }
                break;
            case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
                log << "Reason: EXCEPTION_ARRAY_BOUNDS_EXCEEDED (Array index out of bounds)\n";
                break;
            case EXCEPTION_BREAKPOINT:
                log << "Reason: EXCEPTION_BREAKPOINT (Breakpoint reached)\n";
                break;
            case EXCEPTION_DATATYPE_MISALIGNMENT:
                log << "Reason: EXCEPTION_DATATYPE_MISALIGNMENT\n";
                break;
            case EXCEPTION_FLT_DENORMAL_OPERAND:
                log << "Reason: EXCEPTION_FLT_DENORMAL_OPERAND\n";
                break;
            case EXCEPTION_FLT_DIVIDE_BY_ZERO:
                log << "Reason: EXCEPTION_FLT_DIVIDE_BY_ZERO (Floating point division by zero)\n";
                break;
            case EXCEPTION_INT_DIVIDE_BY_ZERO:
                log << "Reason: EXCEPTION_INT_DIVIDE_BY_ZERO (Integer division by zero)\n";
                break;
            case EXCEPTION_STACK_OVERFLOW:
                log << "Reason: EXCEPTION_STACK_OVERFLOW (Stack overflow)\n";
                break;
            default:
                log << "Reason: Unknown Exception Code\n";
                break;
        }
    } else {
        log << "No exception record available.\n";
    }
    
    // 2. Generate minidump
    HMODULE hDbgHelp = LoadLibraryA("DbgHelp.dll");
    if (hDbgHelp) {
        MINIDUMPWRITEDUMP pfnMiniDumpWriteDump = (MINIDUMPWRITEDUMP)GetProcAddress(hDbgHelp, "MiniDumpWriteDump");
        if (pfnMiniDumpWriteDump) {
            HANDLE hFile = CreateFileA("crash.dmp", GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (hFile != INVALID_HANDLE_VALUE) {
                MINIDUMP_EXCEPTION_INFORMATION mei;
                mei.ThreadId          = GetCurrentThreadId();
                mei.ExceptionPointers = exceptionInfo;
                mei.ClientPointers    = TRUE;
                
                BOOL success = pfnMiniDumpWriteDump(
                    GetCurrentProcess(),
                    GetCurrentProcessId(),
                    hFile,
                    MiniDumpNormal,
                    &mei,
                    nullptr,
                    nullptr
                );
                CloseHandle(hFile);
                if (success) {
                    log << "Minidump generated successfully as 'crash.dmp'.\n";
                } else {
                    log << "MiniDumpWriteDump failed (err=" << GetLastError() << ").\n";
                }
            } else {
                log << "Failed to create dump file 'crash.dmp' (err=" << GetLastError() << ").\n";
            }
        } else {
            log << "Failed to find MiniDumpWriteDump in DbgHelp.dll.\n";
        }
        FreeLibrary(hDbgHelp);
    } else {
        log << "Failed to load DbgHelp.dll.\n";
    }
    log << "========================================\n\n";
    log.close();
    
    // Show a messagebox to notify the user
    MessageBoxA(nullptr, 
        "Application crashed! A crash log ('crash.log') and dump file ('crash.dmp') have been generated. The application will now terminate.", 
        "Fatal Crash", 
        MB_ICONERROR | MB_OK);
        
    return EXCEPTION_EXECUTE_HANDLER;
}

void TerminateHandler() {
    std::ofstream log("crash.log", std::ios::app);
    auto now = std::chrono::system_clock::now();
    auto t   = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
    localtime_s(&tm, &t);
    
    log << "========================================\n";
    log << "[" << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") << "] UNCAUGHT C++ EXCEPTION DETECTED!\n";
    
    std::exception_ptr p = std::current_exception();
    if (p) {
        try {
            std::rethrow_exception(p);
        } catch (const std::exception& e) {
            log << "Exception Message: " << e.what() << "\n";
        } catch (...) {
            log << "Exception type is unknown (not derived from std::exception)\n";
        }
    } else {
        log << "No active exception ptr available.\n";
    }
    log << "========================================\n\n";
    log.close();
    
    MessageBoxA(nullptr, 
        "An uncaught C++ exception occurred! A details log has been written to 'crash.log'. The application will now terminate.", 
        "Fatal Exception", 
        MB_ICONERROR | MB_OK);
        
    std::abort();
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int) {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    SetUnhandledExceptionFilter(CrashHandler);
    std::set_terminate(TerminateHandler);
    
    pm::Logger::instance().setLevel(pm::LogLevel::Info);
    pm::gui::win32::Application app;
    if (!app.init(hInstance, 1280, 800)) {
        std::fprintf(stderr, "Failed to initialize application.\n");
        CoUninitialize();
        return 1;
    }
    int ret = app.run();
    CoUninitialize();
    return ret;
}
