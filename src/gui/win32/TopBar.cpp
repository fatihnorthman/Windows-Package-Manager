#include "TopBar.h"
#include "../i18n.h"
#include "Theme.h"
#include <cstdio>
#include <string>

namespace pm::gui::win32 {

namespace mdl2 {
    constexpr const wchar_t* Search       = L"\uE721";
    constexpr const wchar_t* Filter       = L"\uE152";
    constexpr const wchar_t* Settings     = L"\uE713";
    constexpr const wchar_t* History      = L"\uE81C";
    constexpr const wchar_t* ChromeMin    = L"\uE921";
    constexpr const wchar_t* ChromeMax    = L"\uE922";
    constexpr const wchar_t* ChromeRest   = L"\uE923";
    constexpr const wchar_t* ChromeClose  = L"\uE8BB";
}

namespace {
constexpr float kTopBarX     = theme::SIDEBAR_W;
constexpr float kPadX        = 24.0f;
constexpr float kPadY        = 14.0f;
constexpr float kSearchW     = 320.0f;
constexpr float kSearchH     = 36.0f;
constexpr float kUtilBtnSize = 36.0f;
constexpr float kUtilGap     = 4.0f;
// Window control buttons are intentionally a bit taller so they read as a
// distinct "title bar strip" along the top.
constexpr float kWinBtnW     = 46.0f;
constexpr float kWinBtnH     = 32.0f;
constexpr float kWinBtnGap   = 0.0f;

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

// Pre-compute the rect for each window control button. Always positioned
// at the far right of the window (x = windowW - n*W - (n-1)*gap).
struct WinBtnRects {
    RectF minimize;
    RectF maximize;
    RectF close;
};

WinBtnRects winBtnRects(float windowW) {
    WinBtnRects r;
    float y = 0.0f;
    r.close.x    = windowW - kWinBtnW;             r.close.y    = y;
    r.maximize.x = r.close.x    - kWinBtnW - kWinBtnGap; r.maximize.y = y;
    r.minimize.x = r.maximize.x - kWinBtnW - kWinBtnGap; r.minimize.y = y;
    r.minimize.w = r.maximize.w = r.close.w = kWinBtnW;
    r.minimize.h = r.maximize.h = r.close.h = kWinBtnH;
    return r;
}

void drawWinButton(Renderer& r, const RectF& rect, const wchar_t* glyph,
                   uint32_t hoverColor, const InputState& input, bool danger = false) {
    bool hover = input.mouseInside
              && input.mouse.x >= rect.x && input.mouse.x <= rect.x + rect.w
              && input.mouse.y >= rect.y && input.mouse.y <= rect.y + rect.h;
    if (hover) {
        uint32_t bg = danger ? 0xFFC42B1C : hoverColor;  // close hover = red
        r.fillRect(rect, bg);
    }
    r.drawText(std::wstring(glyph),
               { rect.x, rect.y + (rect.h - 16) * 0.5f, rect.w, 16 },
               theme::COL_ON_SURFACE, 11.0f, Renderer::Icon,
               true, true);
}
} // anonymous

void TopBar::draw(Renderer& r, AppState& state, const InputState& input,
                  float windowW, bool maximized) {
    (void)windowW;
    (void)maximized;

    // We let the background mesh gradient flow underneath, keeping the top bar completely transparent.

    // ---- Search box (left, after sidebar) ----
    float sx = kTopBarX + kPadX;
    float sy = kPadY;
    RectF sBox{ sx, sy, kSearchW, kSearchH };
    state.searchInput.tbX = sx;
    state.searchInput.tbY = sy;
    state.searchInput.tbW = kSearchW;
    state.searchInput.tbH = kSearchH;
    state.searchInput.tbValid = true;

    bool isTbFocused = state.searchInput.focused;
    if (isTbFocused) {
        r.fillRoundedRect(sBox, theme::COL_GLASS_CARD_HOVER_BG, 8.0f);
        r.strokeRect(sBox, theme::COL_PRIMARY, 1.5f, 8.0f);
    } else {
        r.fillRoundedRect(sBox, theme::COL_GLASS_CARD_BG, 8.0f);
        r.strokeRect(sBox, theme::COL_GLASS_CARD_BORDER, 1.0f, 8.0f);
    }
    r.drawText(std::wstring(mdl2::Search), { sx + 14, sy + 9, 18, 18 },
               theme::COL_ON_SURFACE_VARIANT, 14.0f, Renderer::Icon);

    if (state.searchInput.text.empty() && !isTbFocused) {
        r.drawText(t(keys::top_search_ph), { sx + 40, sy + 9, kSearchW - 56, 18 },
                   theme::COL_ON_SURFACE_VARIANT, 13.0f, Renderer::Regular);
    } else {
        r.drawText(state.searchInput.text, { sx + 40, sy + 9, kSearchW - 56, 18 },
                   theme::COL_ON_SURFACE, 13.0f, Renderer::Regular);
        if (isTbFocused) {
            float cursorX = sx + 40 + r.measureTextWidth(state.searchInput.text, 13.0f, Renderer::Regular);
            r.fillRect({ cursorX, sy + 8, 1.5f, 20 }, theme::COL_PRIMARY);
        }
    }

    // ---- Center: screen title + count pill ----
    auto meta = screenMeta(state);
    float winStripW = 3.0f * kWinBtnW + 2.0f * kWinBtnGap;
    float utilStripW = 3.0f * kUtilBtnSize + 2.0f * kUtilGap;
    float availLeft  = sx + kSearchW + kPadX;
    float availRight = windowW - kPadX - utilStripW - kPadX - winStripW;
    if (availRight < availLeft) availRight = availLeft + 100;

    std::string title = t(meta.titleKey);
    r.drawText(title, { availLeft, kPadY + 2, availRight - availLeft, 20 },
               theme::COL_ON_SURFACE, 14.0f, Renderer::Bold, true, false);

    if (meta.count > 0) {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%d", meta.count);
        float pw = (meta.count < 100) ? 36.0f : 48.0f;
        RectF pill{ availRight - pw - 4, kPadY + 4, pw, 20 };
        r.fillRoundedRect(pill, theme::COL_PRIMARY_CONTAINER, 10.0f);
        r.drawText(buf, pill, theme::COL_ON_PRIMARY_CONTAINER,
                   11.0f, Renderer::Bold, true, true);
    } else {
        RectF dot{ availRight - 8, kPadY + 12, 6, 6 };
        r.fillRoundedRect(dot, theme::COL_SUCCESS, 3.0f);
    }

    // ---- Utility buttons (right of title, left of window controls) ----
    float utilY = sy;
    float utilX = windowW - kPadX - winStripW;
    const wchar_t* icons[] = { mdl2::Filter, mdl2::Settings, mdl2::History };
    for (int i = 2; i >= 0; --i) {
        utilX -= kUtilBtnSize + kUtilGap;
        RectF btn{ utilX, utilY, kUtilBtnSize, kUtilBtnSize };
        bool hover = input.mouseInside
                  && input.mouse.x >= btn.x && input.mouse.x <= btn.x + btn.w
                  && input.mouse.y >= btn.y && input.mouse.y <= btn.y + btn.h;
        
        if (hover) {
            r.fillRoundedRect(btn, theme::COL_GLASS_CARD_HOVER_BG, 8.0f);
            r.strokeRect(btn, theme::COL_GLASS_CARD_HOVER_BORDER, 1.0f, 8.0f);
        } else {
            r.fillRoundedRect(btn, theme::COL_GLASS_CARD_BG, 8.0f);
            r.strokeRect(btn, theme::COL_GLASS_CARD_BORDER, 1.0f, 8.0f);
        }
        
        r.drawText(std::wstring(icons[i]), { btn.x + 9, btn.y + 7, 18, 22 },
                   hover ? theme::COL_ON_SURFACE : theme::COL_ON_SURFACE_VARIANT,
                   16.0f, Renderer::Icon);
    }

    // ---- Window controls (far right) ----
    auto wbr = winBtnRects(windowW);
    drawWinButton(r, wbr.minimize, mdl2::ChromeMin,
                  theme::COL_GLASS_CARD_HOVER_BG, input);
    drawWinButton(r, wbr.maximize, maximized ? mdl2::ChromeRest : mdl2::ChromeMax,
                  theme::COL_GLASS_CARD_HOVER_BG, input);
    drawWinButton(r, wbr.close, mdl2::ChromeClose,
                  theme::COL_GLASS_CARD_HOVER_BG, input, /*danger*/ true);
}

TopBarButton TopBar::hitTest(int x, int y, AppState& state, float windowW) {
    (void)state;
    if (y < 0 || y >= theme::TOPBAR_H) return TopBarButton::None;

    // Check window controls first (their height is kWinBtnH = 32)
    if (y < kWinBtnH) {
        auto wbr = winBtnRects(windowW);
        if (x >= wbr.close.x && x <= wbr.close.x + wbr.close.w)    return TopBarButton::Close;
        if (x >= wbr.maximize.x && x <= wbr.maximize.x + wbr.maximize.w) return TopBarButton::Maximize;
        if (x >= wbr.minimize.x && x <= wbr.minimize.x + wbr.minimize.w) return TopBarButton::Minimize;
    }

    // Check utility buttons (Filter, Settings, History)
    float winStripW = 3.0f * kWinBtnW + 2.0f * kWinBtnGap;
    float startX = windowW - kPadX - winStripW;
    float utilY = kPadY;

    // History
    float hX = startX - kUtilBtnSize - kUtilGap;
    if (x >= hX && x <= hX + kUtilBtnSize && y >= utilY && y <= utilY + kUtilBtnSize) {
        return TopBarButton::History;
    }

    // Settings
    float sX = hX - kUtilBtnSize - kUtilGap;
    if (x >= sX && x <= sX + kUtilBtnSize && y >= utilY && y <= utilY + kUtilBtnSize) {
        return TopBarButton::Settings;
    }

    // Filter
    float fX = sX - kUtilBtnSize - kUtilGap;
    if (x >= fX && x <= fX + kUtilBtnSize && y >= utilY && y <= utilY + kUtilBtnSize) {
        return TopBarButton::Filter;
    }

    return TopBarButton::None;
}

} // namespace pm::gui::win32
