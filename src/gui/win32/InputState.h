#pragma once

#include <windows.h>

namespace pm::gui::win32 {

// Input state captured from window messages.
struct InputState {
    POINT mouse       = { 0, 0 };
    bool  mouseDown   = false;
    bool  mouseInside = false;
    bool  shift       = false;
    bool  ctrl        = false;
};

} // namespace pm::gui::win32
