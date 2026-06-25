#include "Application.h"
#include "Sidebar.h"
#include "TopBar.h"
#include "Footer.h"
#include "Screens.h"
#include "TaskDrawer.h"
#include "../i18n.h"
#include "../../core/Logger.h"
#include <dwmapi.h>
#include <algorithm>
#include <windowsx.h>

#pragma comment(lib, "dwmapi.lib")

namespace pm::gui::win32 {

namespace {
Application* g_app = nullptr;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (!g_app) return ::DefWindowProc(hwnd, msg, wParam, lParam);

    switch (msg) {
        case WM_CREATE: return 0;

        case WM_SIZE: {
            UINT w = LOWORD(lParam);
            UINT h = HIWORD(lParam);
            g_app->maximized_ = (wParam == SIZE_MAXIMIZED);
            g_app->renderer().resize(w, h);
            return 0;
        }

        case WM_PAINT: {
            PAINTSTRUCT ps;
            BeginPaint(hwnd, &ps);
            g_app->renderFrame();
            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_TIMER: {
            if (wParam == 1) {
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;
        }

        case WM_MOUSEMOVE: {
            int x = GET_X_LPARAM(lParam);
            int y = GET_Y_LPARAM(lParam);
            g_app->input().mouse.x = x;
            g_app->input().mouse.y = y;
            g_app->input().mouseInside = true;
            g_app->handleMouseMove(x, y);
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }

        case WM_MOUSELEAVE:
            g_app->input().mouseInside = false;
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;

        case WM_LBUTTONDOWN:
            g_app->input().mouseDown = true;
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;

        case WM_LBUTTONUP: {
            g_app->input().mouseDown = false;
            int x = GET_X_LPARAM(lParam);
            int y = GET_Y_LPARAM(lParam);
            g_app->handleMouseUp(x, y);
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }

        case WM_CHAR: {
            // Feed printable characters / backspace / Enter into the
            // focused TextInput. Enter triggers the bound action
            // (currently runSearch on the Discover query).
            auto& s = g_app->bridge().state();
            std::lock_guard<std::recursive_mutex> lk(s.mtx);
            if (!s.searchInput.focused) return 0;
            wchar_t wc = (wchar_t)wParam;
            if (wc == L'\r' || wc == L'\n') {
                g_app->bridge().runSearch(s.searchInput.text);
            } else if (wc == L'\b') {
                if (!s.searchInput.text.empty()) {
                    s.searchInput.text.pop_back();
                }
            } else if (wc >= 32 && wc != 127) {
                // Simple ASCII fast path; for the rest we just drop it.
                char c = (char)wc;
                s.searchInput.text += c;
            }
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }

        case WM_LBUTTONDBLCLK: {
            // Double-click on the title bar region toggles maximize.
            POINT pt{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            if (pt.y >= 0 && pt.y < 32) {
                if (g_app->maximized_) ShowWindow(hwnd, SW_RESTORE);
                else                   ShowWindow(hwnd, SW_MAXIMIZE);
            }
            return 0;
        }

        case WM_KEYDOWN:
            if (wParam == VK_SHIFT) g_app->input().shift = true;
            if (wParam == VK_CONTROL) g_app->input().ctrl = true;
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;

        case WM_KEYUP:
            if (wParam == VK_SHIFT) g_app->input().shift = false;
            if (wParam == VK_CONTROL) g_app->input().ctrl = false;
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;

        case WM_MOUSEWHEEL: {
            int delta  = GET_WHEEL_DELTA_WPARAM(wParam);
            int rows   = delta / 60;
            if (rows == 0) rows = (delta > 0) ? 1 : -1;
            auto& s    = g_app->bridge().state();
            std::lock_guard<std::recursive_mutex> lk(s.mtx);
            int  idx   = static_cast<int>(s.currentScreen);
            int  next  = s.scrollOffset[idx] - rows;
            if (next < 0) next = 0;
            s.scrollOffset[idx] = next;
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }

        case WM_NCHITTEST: {
            LRESULT def = ::DefWindowProc(hwnd, msg, wParam, lParam);
            if (def != HTCLIENT) return def;
            POINT pt{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            ScreenToClient(hwnd, &pt);
            // Reserve the rightmost 150px of the title strip for our
            // custom min/max/close buttons. The rest of the top 32px
            // is the drag region. Returning HTCAPTION for the button
            // area would make clicks there start a window drag instead
            // of reaching WM_LBUTTONUP -> TopBar::hitTest -> Close.
            RECT rc; GetClientRect(hwnd, &rc);
            if (pt.y >= 0 && pt.y < 32 && pt.x < rc.right - 150) {
                return HTCAPTION;
            }
            return HTCLIENT;
        }

        case WM_NCCALCSIZE: {
            // Strip the standard 1px client border that WS_THICKFRAME
            // normally adds on top of the non-client area; we want a
            // seamless edge-to-edge look for the custom title bar.
            if (wParam == TRUE) {
                NCCALCSIZE_PARAMS* p = reinterpret_cast<NCCALCSIZE_PARAMS*>(lParam);
                p->rgrc[0].top    = p->rgrc[1].top;
                p->rgrc[0].left   = p->rgrc[1].left;
                p->rgrc[0].right  = p->rgrc[1].right;
                p->rgrc[0].bottom = p->rgrc[1].bottom;
            }
            return 0;
        }

        case WM_NCPAINT:
            return 0;  // we paint the whole frame; skip non-client paint

        case WM_ERASEBKGND:
            return 1;

        case WM_GETMINMAXINFO: {
            MINMAXINFO* m = reinterpret_cast<MINMAXINFO*>(lParam);
            m->ptMinTrackSize.x = 800;
            m->ptMinTrackSize.y = 560;
            return 0;
        }

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return ::DefWindowProc(hwnd, msg, wParam, lParam);
}

Application::Application() = default;
Application::~Application() = default;

bool Application::init(HINSTANCE hInstance, int w, int h) {
    if (!initWindow(hInstance, w, h)) return false;
    if (!renderer_.init(hwnd_)) {
        Logger::instance().error("D2D1 init failed");
        return false;
    }
    bridge_.init();
    bridge_.state().hwnd = hwnd_;
    bridge_.refreshInstalled();
    bridge_.refreshUpgradable();
    return true;
}

bool Application::initWindow(HINSTANCE hInstance, int w, int h) {
    WNDCLASSEXW wc = {};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInstance;
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"PackageManagerClass";
    if (!RegisterClassExW(&wc)) {
        DWORD err = GetLastError();
        if (err != ERROR_CLASS_ALREADY_EXISTS) {
            Logger::instance().error("RegisterClassExW failed: ", err);
            return false;
        }
    }

    g_app = this;

    // Borderless window: no system caption, but keep WS_THICKFRAME for
    // resizing and WS_MINIMIZEBOX/MAXIMIZEBOX for taskbar restore.
    DWORD style = WS_POPUP | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX;
    RECT rc = {0, 0, w, h};
    AdjustWindowRect(&rc, style, FALSE);

    hwnd_ = CreateWindowExW(
        0, wc.lpszClassName, L"Windows Package Manager",
        style,
        CW_USEDEFAULT, CW_USEDEFAULT,
        rc.right - rc.left, rc.bottom - rc.top,
        nullptr, nullptr, hInstance, nullptr);
    if (!hwnd_) {
        Logger::instance().error("CreateWindowExW failed: ", GetLastError());
        return false;
    }

    ShowWindow(hwnd_, SW_SHOW);
    UpdateWindow(hwnd_);
    SetForegroundWindow(hwnd_);
    SetFocus(hwnd_);
    return true;
}

void Application::handleMouseUp(int x, int y) {
    AppState& state = bridge_.state();
    std::lock_guard<std::recursive_mutex> lk(state.mtx);
    RECT rc; GetClientRect(hwnd_, &rc);
    const float W = static_cast<float>(rc.right);
    const float H = static_cast<float>(rc.bottom);

    // Focus management: clicking inside the currently-bound text-input
    // box gives it focus; clicking outside releases it.
    if (state.searchInput.boxValid) {
        bool inside = x >= state.searchInput.boxX
                   && x <= state.searchInput.boxX + state.searchInput.boxW
                   && y >= state.searchInput.boxY
                   && y <= state.searchInput.boxY + state.searchInput.boxH;
        state.searchInput.focused = inside;
    }

    if (auto btn = TopBar::hitTest(x, y, state, W); btn != TopBarButton::None) {
        switch (btn) {
            case TopBarButton::Minimize:
                ShowWindow(hwnd_, SW_MINIMIZE);
                return;
            case TopBarButton::Maximize:
                if (maximized_) ShowWindow(hwnd_, SW_RESTORE);
                else            ShowWindow(hwnd_, SW_MAXIMIZE);
                return;
            case TopBarButton::Close:
                PostMessage(hwnd_, WM_CLOSE, 0, 0);
                return;
            default: break;
        }
    }

    if (SidebarHitTest(x, y, state)) return;
    if (ScreenHitTest(x, y, state, bridge_)) return;
    if (TaskDrawer::hitTest(x, y, state, bridge_, W, H)) return;
}

void Application::handleMouseMove(int x, int y) {
    // Reserved for hover tracking that needs to update global state.
    (void)x; (void)y;
}

void Application::renderFrame() {
    AppState& state = bridge_.state();
    std::lock_guard<std::recursive_mutex> lk(state.mtx);

    renderer_.beginFrame();
    renderer_.clear(theme::COL_BACKGROUND);

    RECT rc; GetClientRect(hwnd_, &rc);
    const float W = static_cast<float>(rc.right);
    const float H = static_cast<float>(rc.bottom);

    // Ambient background — a very gentle warm-to-cool vertical fade
    // instead of flat black. Sets the mood for the whole window before
    // any UI surfaces paint over it.
    renderer_.fillRectLinearV({ 0, 0, W, H },
                              theme::COL_BG_GRAD_TOP, theme::COL_BG_GRAD_BOT);

    // A soft primary-tinted halo behind the top-left corner, fading to
    // transparent. Gives the impression of light spilling from the
    // title bar across the rest of the window.
    renderer_.fillRectRadial({ 0, 0, W, H * 0.6f },
                             0.15f, 0.25f, 0.85f,
                             theme::COL_HALO_CENTER, theme::COL_HALO_EDGE);

    Sidebar::draw(renderer_, state, input_);
    TopBar::draw(renderer_, state, input_, W, maximized_);
    Screens::draw(renderer_, state, bridge_, input_, W, H);
    Footer::draw(renderer_, state, bridge_, W, H);
    TaskDrawer::draw(renderer_, state, bridge_, input_, W, H);

    renderer_.endFrame();

    updateTimer();
}

void Application::updateTimer() {
    bool needsTimer = bridge_.state().loadingInstalled.load() ||
                      bridge_.state().loadingUpgradable.load() ||
                      bridge_.state().loadingSearch.load() ||
                      bridge_.activeTasks() > 0 ||
                      bridge_.pendingTasks() > 0;
    if (needsTimer) {
        if (!timerActive_) {
            SetTimer(hwnd_, 1, 33, nullptr);
            timerActive_ = true;
        }
    } else {
        if (timerActive_) {
            KillTimer(hwnd_, 1);
            timerActive_ = false;
        }
    }
}

int Application::run() {
    MSG msg = {};
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return static_cast<int>(msg.wParam);
}

} // namespace pm::gui::win32
