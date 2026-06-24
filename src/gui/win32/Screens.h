#pragma once

#include "Renderer.h"
#include "InputState.h"
#include "../BackendBridge.h"
#include "Theme.h"

namespace pm::gui::win32 {

class Screens {
public:
    // Render the active screen into the main content area.
    static void draw(Renderer& r, AppState& state, BackendBridge& bridge,
                     const InputState& input, float W, float H);
};

// Hit-test for clicks in the main content area.
bool ScreenHitTest(int x, int y, AppState& state, BackendBridge& bridge);

} // namespace pm::gui::win32
