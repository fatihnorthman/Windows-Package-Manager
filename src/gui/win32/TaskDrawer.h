#pragma once

#include "Renderer.h"
#include "InputState.h"
#include "../BackendBridge.h"

namespace pm::gui::win32 {

// Bottom-right "task queue" drawer matching the Stitch Fluent Flux mockup.
// Always visible (slim grabber bar) when at least one task has ever been
// enqueued or a scan is running; collapses/expands on click.
//
// Layout (anchored to bottom-right above the footer):
//   collapsed:  W x 56  ─ grabber + "Task Queue (N active)" + chevron
//   expanded:   W x 360 ─ header + scrollable task list + "Cancel All"
class TaskDrawer {
public:
    static constexpr float kCollapsedH = 56.0f;
    static constexpr float kExpandedH  = 360.0f;
    static constexpr float kDrawerW    = 420.0f;
    static constexpr float kMargin     = 16.0f;

    // Draw the drawer. Returns the y-coordinate where the main content area
    // should end (i.e. the top of the drawer) so callers can shrink the
    // screen content if they want to make room for the expanded drawer.
    static float draw(Renderer& r, AppState& state, BackendBridge& bridge,
                      const InputState& input, float W, float H);

    // Hit-test: handles clicks on the drawer header (toggle) and rows.
    static bool hitTest(int x, int y, AppState& state, BackendBridge& bridge,
                        float W, float H);

    // True if any task is queued or running — used to decide whether the
    // drawer is visible at all.
    static bool shouldShow(const AppState& state, const BackendBridge& bridge);
};

} // namespace pm::gui::win32