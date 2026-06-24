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
#include <cstdio>

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int) {
    pm::Logger::instance().setLevel(pm::LogLevel::Info);
    pm::gui::win32::Application app;
    if (!app.init(hInstance, 1280, 800)) {
        std::fprintf(stderr, "Failed to initialize application.\n");
        return 1;
    }
    return app.run();
}
