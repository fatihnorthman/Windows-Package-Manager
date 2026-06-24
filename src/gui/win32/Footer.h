#pragma once

#include "Renderer.h"
#include "../BackendBridge.h"

namespace pm::gui::win32 {

class Footer {
public:
    static void draw(Renderer& r, AppState& state, BackendBridge& bridge, float windowW, float windowH);
};

} // namespace pm::gui::win32
