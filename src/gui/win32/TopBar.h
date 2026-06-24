#pragma once

#include "Renderer.h"
#include "InputState.h"
#include "../BackendBridge.h"

namespace pm::gui::win32 {

class TopBar {
public:
    static void draw(Renderer& r, AppState& state, const InputState& input, float windowW);
    static bool hitTest(int x, int y, AppState& state);
};

inline bool TopBarHitTest(int x, int y, AppState& state) {
    return TopBar::hitTest(x, y, state);
}

} // namespace pm::gui::win32
