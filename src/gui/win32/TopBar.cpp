#include "TopBar.h"
#include "../i18n.h"
#include "Theme.h"
#include <string>

namespace pm::gui::win32 {

namespace mdl2 {
    constexpr const wchar_t* Search     = L"\uE721";
    constexpr const wchar_t* Filter     = L"\uE152";
    constexpr const wchar_t* Settings   = L"\uE713";
    constexpr const wchar_t* History    = L"\uE81C";
}

namespace {
constexpr float kTopBarX = theme::SIDEBAR_W;
constexpr float kTopBarY = 0.0f;
constexpr float kTopBarH = theme::TOPBAR_H;
constexpr float kPadX    = 24.0f;
constexpr float kPadY    = 14.0f;
constexpr float kSearchW = 320.0f;
constexpr float kSearchH = 36.0f;
constexpr float kUtilBtnSize = 36.0f;
constexpr float kUtilGap     = 4.0f;
}

void TopBar::draw(Renderer& r, AppState& state, const InputState& input, float windowW) {
    float barW = windowW - kTopBarX;

    // Background — semi-transparent
    r.fillRect({ kTopBarX, kTopBarY, barW, kTopBarH }, 0xB3131313);

    // Bottom border
    r.fillRect({ kTopBarX, kTopBarH - 1, barW, 1 }, theme::COL_OUTLINE_VARIANT);

    // Search box
    float sx = kTopBarX + kPadX;
    float sy = kTopBarY + kPadY;
    r.fillRoundedRect({ sx, sy, kSearchW, kSearchH }, theme::COL_SURFACE_CONTAINER_HIGH, 6.0f);
    // Search icon (left)
    r.drawText(std::wstring(mdl2::Search), { sx + 12, sy + 9, 18, 18 },
               theme::COL_ON_SURFACE_VARIANT, 14.0f, Renderer::Icon);
    // Placeholder text
    r.drawText(t(keys::top_search_ph), { sx + 36, sy + 9, kSearchW - 50, 18 },
               theme::COL_ON_SURFACE_VARIANT, 13.0f, Renderer::Regular);

    // Right utility buttons (right-aligned)
    float utilY = sy;
    float utilX = windowW - kPadX;
    const wchar_t* icons[] = { mdl2::Filter, mdl2::Settings, mdl2::History };
    for (int i = 2; i >= 0; --i) {
        utilX -= kUtilBtnSize + kUtilGap;
        RectF btn { utilX, utilY, kUtilBtnSize, kUtilBtnSize };
        bool hover = input.mouseInside
                  && input.mouse.x >= btn.x && input.mouse.x <= btn.x + btn.w
                  && input.mouse.y >= btn.y && input.mouse.y <= btn.y + btn.h;
        if (hover) r.fillRoundedRect(btn, theme::COL_SURFACE_CONTAINER_HIGH, 6.0f);
        r.drawText(std::wstring(icons[i]), { btn.x + 9, btn.y + 7, 18, 22 },
                   theme::COL_ON_SURFACE_VARIANT, 16.0f, Renderer::Icon);
    }
}

bool TopBar::hitTest(int x, int y, AppState& state) {
    if (y < 0 || y >= kTopBarH) return false;
    if (x < kTopBarX) return false;
    // Click is inside the top bar. No action wired up yet (search field,
    // filter/settings/history buttons are placeholder UI for now) — return
    // false so the click bubbles to the screen hit-test below.
    return false;
}

} // namespace pm::gui::win32
