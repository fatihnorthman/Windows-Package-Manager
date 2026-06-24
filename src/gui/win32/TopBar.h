#pragma once

#include "Renderer.h"
#include "InputState.h"
#include "../BackendBridge.h"
#include "Theme.h"

namespace pm::gui::win32 {

// Semantic identifier for any clickable element in the top bar — utility
// buttons AND window controls (minimize / maximize / close). The
// Application dispatcher switches on this to decide what to do.
enum class TopBarButton {
    None,
    Minimize,
    Maximize,
    Close,
    Filter,
    Settings,
    History,
};

class TopBar {
public:
    // Draw the title bar / toolbar / window controls in one pass.
    static void draw(Renderer& r, AppState& state, const InputState& input,
                     float windowW, bool maximized);

    // Hit-test a click in the top bar. Returns the semantic button the
    // click landed on, or None.
    static TopBarButton hitTest(int x, int y, AppState& state, float windowW);
};

// Wrapper to expose the public hit-test function.
inline TopBarButton TopBarHitTest(int x, int y, AppState& state, float windowW) {
    return TopBar::hitTest(x, y, state, windowW);
}

} // namespace pm::gui::win32
