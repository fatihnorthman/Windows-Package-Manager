#include "TopBar.h"
#include "../i18n.h"
#include "Theme.h"
#include "../BackendBridge.h"
#include <cstdio>
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

struct ScreenMeta {
    std::string_view titleKey;
    int              count;
};

ScreenMeta screenMeta(const AppState& s) {
    switch (s.currentScreen) {
        case ScreenId::Discover:  return { keys::discover_title,  (int)s.searchResults.size() };
        case ScreenId::Installed: return { keys::installed_title, (int)s.installed.size()    };
        case ScreenId::Updates:   return { keys::updates_title,   (int)s.upgradable.size()   };
        case ScreenId::Tasks:     return { keys::tasks_title,     0 };
        case ScreenId::Settings:  return { keys::settings_title,  0 };
    }
    return { keys::app_title, 0 };
}
} // anonymous

void TopBar::draw(Renderer& r, AppState& state, const InputState& input, float windowW) {
    float barW = windowW - kTopBarX;

    // 1. Solid surface background — matches sidebar (COL_SURFACE) so the
    //    top bar reads as part of the same horizontal band, not a separate
    //    translucent overlay.
    r.fillRect({ kTopBarX, kTopBarY, barW, kTopBarH }, theme::COL_SURFACE);

    // 2. Thin primary accent line at the very top (2px). Subtle but gives
    //    the bar a defined leading edge that echoes the COL_PRIMARY
    //    highlight used on the active sidebar item.
    r.fillRect({ kTopBarX, 0, barW, 2 }, theme::COL_PRIMARY_CONTAINER);

    // 3. Bottom border separating bar from content.
    r.fillRect({ kTopBarX, kTopBarH - 1, barW, 1 }, theme::COL_OUTLINE_VARIANT);

    // ---- Left: search box ----
    float sx = kTopBarX + kPadX;
    float sy = kTopBarY + kPadY;
    RectF sBox{ sx, sy, kSearchW, kSearchH };
    r.fillRoundedRect(sBox, theme::COL_SURFACE_CONTAINER_HIGH, 8.0f);
    r.strokeRect(sBox, theme::COL_OUTLINE_VARIANT, 1.0f, 8.0f);
    r.drawText(std::wstring(mdl2::Search), { sx + 14, sy + 9, 18, 18 },
               theme::COL_ON_SURFACE_VARIANT, 14.0f, Renderer::Icon);
    r.drawText(t(keys::top_search_ph), { sx + 40, sy + 9, kSearchW - 56, 18 },
               theme::COL_ON_SURFACE_VARIANT, 13.0f, Renderer::Regular);

    // ---- Center: screen title + count pill ----
    //    Placed in the empty band between the search box and the utility
    //    buttons so the bar never feels lopsided.
    auto meta = screenMeta(state);
    float utilStripW = 3.0f * kUtilBtnSize + 2.0f * kUtilGap;
    float availLeft  = sx + kSearchW + kPadX;
    float availRight = windowW - kPadX - utilStripW;

    std::string title = t(meta.titleKey);
    // Title sits slightly above the bar's vertical center for optical
    // balance (lowercase x-height reads as too low when text-aligned middle).
    r.drawText(title, { availLeft, kTopBarY + 16, availRight - availLeft, 20 },
               theme::COL_ON_SURFACE, 14.0f, Renderer::Bold, true, false);

    // Count pill — only when there is something to count, otherwise skip
    // (e.g. on the Settings/Tasks screens where count is 0).
    if (meta.count > 0) {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%d", meta.count);
        float pw = (meta.count < 100) ? 36.0f : 48.0f;
        RectF pill{ availRight - pw - 4, kTopBarY + 16, pw, 20 };
        r.fillRoundedRect(pill, theme::COL_PRIMARY_CONTAINER, 10.0f);
        r.drawText(buf, pill, theme::COL_ON_PRIMARY_CONTAINER,
                   11.0f, Renderer::Bold, true, true);
    } else {
        // Status indicator: subtle "ready" dot on the right of the title band.
        RectF dot{ availRight - 8, kTopBarY + 24, 6, 6 };
        r.fillRoundedRect(dot, theme::COL_SUCCESS, 3.0f);
    }

    // ---- Right: utility buttons ----
    float utilY = sy;
    float utilX = windowW - kPadX;
    const wchar_t* icons[] = { mdl2::Filter, mdl2::Settings, mdl2::History };
    for (int i = 2; i >= 0; --i) {
        utilX -= kUtilBtnSize + kUtilGap;
        RectF btn{ utilX, utilY, kUtilBtnSize, kUtilBtnSize };
        bool hover = input.mouseInside
                  && input.mouse.x >= btn.x && input.mouse.x <= btn.x + btn.w
                  && input.mouse.y >= btn.y && input.mouse.y <= btn.y + btn.h;
        if (hover) r.fillRoundedRect(btn, theme::COL_SURFACE_CONTAINER_HIGHEST, 8.0f);
        r.drawText(std::wstring(icons[i]), { btn.x + 9, btn.y + 7, 18, 22 },
                   hover ? theme::COL_ON_SURFACE : theme::COL_ON_SURFACE_VARIANT,
                   16.0f, Renderer::Icon);
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
