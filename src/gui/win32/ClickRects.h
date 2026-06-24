#pragma once

#include <string>
#include <vector>

namespace pm::gui::win32 {

// A clickable rectangle in the current frame, with a semantic id
// and an arbitrary string payload. The Application dispatcher
// (ScreenHitTest in Screens.cpp) iterates these every frame and
// matches on the id + payload to invoke a behavior. Cleared at
// the start of every Screens::draw and TaskDrawer::draw pass.
struct ClickRect {
    float       x, y, w, h;
    int         id;
    std::string payload;
};

// Push a rect into the global list. The list is owned by Screens.cpp
// but exposed here so other screens (TaskDrawer, etc.) can publish
// rects without taking a dependency on Screens internals.
void pushClickRect(const ClickRect& r);

} // namespace pm::gui::win32
