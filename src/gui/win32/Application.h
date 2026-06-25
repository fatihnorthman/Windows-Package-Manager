#pragma once

#include "Renderer.h"
#include "InputState.h"
#include "../BackendBridge.h"
#include <windows.h>

namespace pm::gui::win32 {

class Application {
public:
    Application();
    ~Application();

    bool init(HINSTANCE hInstance, int width = 1280, int height = 800);
    int  run();

    BackendBridge& bridge() { return bridge_; }
    InputState& input() { return input_; }
    Renderer& renderer() { return renderer_; }

private:
    bool initWindow(HINSTANCE hInstance, int w, int h);
    void handleResize(UINT w, UINT h);

public:  // Called from the WndProc in the same translation unit.
    void handleMouseUp(int x, int y);
    void handleMouseMove(int x, int y);
    void renderFrame();
    void updateTimer();

    HWND          hwnd_ = nullptr;
    Renderer      renderer_;
    InputState    input_;
    BackendBridge bridge_;
    bool          running_ = false;
    bool          maximized_ = false;
    bool          timerActive_ = false;
};

// WndProc routed through static function. Friend so it can call private
// mouse handlers.
class Application;
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

} // namespace pm::gui::win32
