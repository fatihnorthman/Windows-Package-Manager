#pragma once

#include "Renderer.h"
#include "InputState.h"
#include "../BackendBridge.h"
#include "Theme.h"

namespace pm::gui::win32 {

class Sidebar {
public:
    // Draw the sidebar (full-height, left).
    static void draw(Renderer& r, AppState& state, const InputState& input);

    // Hit-test: returns true if the click was handled by the sidebar.
    static bool hitTest(int x, int y, AppState& state);
};

// Wrapper to expose the public hit-test function.
inline bool SidebarHitTest(int x, int y, AppState& state) {
    return Sidebar::hitTest(x, y, state);
}

} // namespace pm::gui::win32
