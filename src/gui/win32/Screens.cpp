#include "Screens.h"
#include "ClickRects.h"
#include "../i18n.h"
#include "Theme.h"
#include <cstdio>
#include <string>
#include <vector>
#include <algorithm>

namespace pm::gui::win32 {

namespace mdl2 {
    constexpr const wchar_t* CheckMark  = L"\uE73E";
    constexpr const wchar_t* Search     = L"\uE721";
    constexpr const wchar_t* Refresh    = L"\uE72C";
    constexpr const wchar_t* Update     = L"\uE895";
    constexpr const wchar_t* Settings   = L"\uE713";
    constexpr const wchar_t* TaskAlt    = L"\uE73E";
}

namespace {

// Hit-test helper used by many draw functions.
bool RectContains(const RectF& r, int x, int y) {
    return x >= r.x && x <= r.x + r.w && y >= r.y && y <= r.y + r.h;
}

constexpr float kContentX     = theme::SIDEBAR_W;
constexpr float kContentY     = theme::TOPBAR_H;
constexpr float kContentPadX  = 32.0f;
constexpr float kContentPadY  = 24.0f;
constexpr float kColGap       = 12.0f;
constexpr float kRowH         = 64.0f;
constexpr float kRowGap       = 6.0f;
constexpr float kLogoSize     = 36.0f;

// Vertical scrollbar on the right edge of the list area. Only drawn when
// the list actually overflows (total > visible). Thumb height is
// proportional to the visible/total ratio; its position reflects the
// current scrollOffset (clamped to valid range by the caller).
void drawScrollbar(Renderer& r, float trackX, float trackY, float trackH,
                   int offset, int total, int visible) {
    if (total <= visible || total <= 0) return;
    int maxOff = total - visible;
    if (maxOff <= 0) return;
    r.fillRoundedRect({ trackX, trackY, 4.0f, trackH },
                      theme::COL_SURFACE_CONTAINER_HIGHEST, 2.0f);
    float thumbH = std::max(24.0f, (visible / (float)total) * trackH);
    float span   = trackH - thumbH;
    float thumbY = trackY + (offset / (float)maxOff) * span;
    r.fillRoundedRect({ trackX, thumbY, 4.0f, thumbH },
                      theme::COL_PRIMARY_CONTAINER, 2.0f);
}

// Clamp scrollOffset[screen] to a valid range for a list of `total` items
// showing `visible` at a time. Writes the clamped value back so the UI
// never sits at an out-of-range offset after a refresh that shrank the
// list.
int clampScrollOffset(AppState& state, ScreenId id, int total, int visible) {
    int idx    = static_cast<int>(id);
    int maxOff = std::max(0, total - visible);
    int off    = std::clamp(state.scrollOffset[idx], 0, maxOff);
    state.scrollOffset[idx] = off;
    return off;
}

// Track clickable button regions in the active screen so we can hit-test them.
struct ClickRectInternal {
    RectF  bounds;
    int    id;
    std::string payload;
};
std::vector<ClickRectInternal>& clickRects() {
    static std::vector<ClickRectInternal> v;
    return v;
}
void clearRects() { clickRects().clear(); }
static void pushRect(const RectF& r, int id, const std::string& payload = "") {
    clickRects().push_back({ r, id, payload });
}

// ---- helpers ----
void drawHeroHeader(Renderer& r, float x, float y, float w,
                    const std::string& title, const std::string& subtitle) {
    // A soft primary-tinted halo behind the title. The radial gradient
    // is offset to the left so it reads as if light is coming from
    // off-screen left, lifting the title out of the background.
    r.fillRectRadial({ x - 24, y - 16, w * 0.6f, 160.0f },
                     0.35f, 0.5f, 0.7f,
                     theme::COL_HALO_CENTER, theme::COL_HALO_EDGE);
    r.drawText(title, { x, y, w, 36 }, theme::COL_ON_SURFACE, 26.0f, Renderer::Bold);
    r.drawText(subtitle, { x, y + 38, w, 22 }, theme::COL_ON_SURFACE_VARIANT,
               13.0f, Renderer::Regular);
}

void drawSourceBadge(Renderer& r, const RectF& rect, PackageManager m) {
    const char* label = "WINGET";
    uint32_t textCol, bgCol;
    switch (m) {
        case PackageManager::Winget:
            textCol = theme::COL_PRIMARY;
            bgCol   = 0x1A0078D4;
            break;
        case PackageManager::Scoop:
            label = "SCOOP";
            textCol = theme::COL_TERTIARY;
            bgCol   = 0x33BC5B00;
            break;
        case PackageManager::Chocolatey:
            label = "CHOCO";
            textCol = theme::COL_ON_SURFACE_VARIANT;
            bgCol   = 0x80474746;
            break;
        default:
            label = "?"; textCol = theme::COL_ON_SURFACE_VARIANT;
            bgCol   = 0x80474746;
    }
    r.fillRoundedRect(rect, bgCol, 4.0f);
    r.drawText(label, rect, textCol, 10.0f, Renderer::Bold, true, true);
}

// Two-letter abbreviation: first letter + first letter after the last
// dot (publisher-style), or just first two letters if no dot.
std::string packageAbbrev(const std::string& id) {
    if (id.empty()) return "?";
    auto upper = [](char c) { return (char)std::toupper((unsigned char)c); };
    auto dot = id.find_last_of('.');
    if (dot != std::string::npos && dot + 1 < id.size()) {
        return std::string() + upper(id[0]) + upper(id[dot + 1]);
    }
    if (id.size() >= 2) {
        return std::string() + upper(id[0]) + upper(id[1]);
    }
    return std::string(1, upper(id[0]));
}

// Stable hash of the package id, used to pick a color so the same
// package always renders with the same avatar color.
uint32_t packageHash(const std::string& s) {
    uint32_t h = 2166136261u;
    for (char c : s) {
        h ^= (uint8_t)c;
        h *= 16777619u;
    }
    return h;
}

// Tasteful avatar palette (8 colors used by Microsoft Store / Fluent).
// Returns ARGB.
uint32_t avatarColor(uint32_t hash) {
    static const uint32_t palette[] = {
        0xFF0078D4, 0xFF8764B8, 0xFF038387, 0xFF00B294,
        0xFFE81123, 0xFFCA5010, 0xFFB146C2, 0xFF1B85C5,
    };
    return palette[hash % (sizeof(palette) / sizeof(palette[0]))];
}

// Darken an ARGB color by `amt` (0..1). Used for the bottom half of
// the avatar gradient to give a subtle vertical depth.
uint32_t darken(uint32_t c, float amt) {
    uint8_t a = (c >> 24) & 0xFF;
    uint8_t r = (uint8_t)(((c >> 16) & 0xFF) * (1.0f - amt));
    uint8_t g = (uint8_t)(((c >>  8) & 0xFF) * (1.0f - amt));
    uint8_t b = (uint8_t)(( c        & 0xFF) * (1.0f - amt));
    return (uint32_t)a << 24 | (uint32_t)r << 16 | (uint32_t)g << 8 | b;
}

void drawLogoPlaceholder(Renderer& r, const RectF& rect, const std::string& id, PackageManager m) {
    // Layered avatar: dark base, brand color on top, glossy highlight,
    // and outline. The highlight is a radial gradient anchored at the
    // top-left corner for a glassy "lit from above" finish.
    uint32_t hash = packageHash(id);
    uint32_t base = (m == PackageManager::Winget)     ? 0xFF0078D4u
                  : (m == PackageManager::Scoop)      ? 0xFFBC5B00u
                  : (m == PackageManager::Chocolatey) ? 0xFF6B4423u
                  :                                       avatarColor(hash);

    // Base + brand top half
    r.fillRoundedRect(rect, darken(base, 0.25f), 8.0f);
    r.fillRoundedRect({ rect.x, rect.y, rect.w, rect.h * 0.5f },
                      base, 8.0f);
    // Glossy highlight (radial, top-left)
    r.fillRectRadial(rect, 0.25f, 0.2f, 0.7f, 0x66FFFFFF, 0x00FFFFFF);
    // Outline
    r.strokeRect(rect, darken(base, 0.45f), 1.0f, 8.0f);

    std::string ab = packageAbbrev(id);
    r.drawText(ab, rect, 0xFFFFFFFF, 18.0f, Renderer::Bold, true, true);
}

void drawButton(Renderer& r, const RectF& rect, const std::string& label,
                bool primary, const InputState& input, bool hover) {
    (void)input;
    uint32_t bg = primary ? theme::COL_PRIMARY_CONTAINER : 0;
    uint32_t bd = primary ? 0 : theme::COL_PRIMARY;
    uint32_t tx = primary ? theme::COL_ON_PRIMARY_CONTAINER : theme::COL_PRIMARY;
    if (hover && !primary) bg = 0x330078D4;
    if (hover && primary)   bg = 0xFF0086EE;
    if (bg) {
        // Vertical gradient for the primary / hover state — adds depth
        // and makes the button look tappable.
        uint32_t top = primary
            ? (hover ? 0xFF0F95FFu : 0xFF0086EEu)
            : 0xFF1A8ADBu;
        uint32_t bot = bg;
        r.fillRectLinearV(rect, top, bot, 6.0f);
    }
    if (bd) r.strokeRect(rect, bd, 1.0f, 6.0f);
    r.drawText(label, rect, tx, 12.0f, Renderer::Bold, true, true);
}

// ---- Discover ----
void renderDiscover(Renderer& r, AppState& state, BackendBridge& bridge,
                    const InputState& input, float x, float y, float w, float h) {
    drawHeroHeader(r, x, y, w, t(keys::discover_title), t(keys::discover_subtitle));
    float sy = y + 90;

    // Search box + button. The box geometry is registered with
    // state.searchInput so the WndProc can route WM_CHAR into it and
    // handleMouseUp can give it focus on click.
    RectF sBox{ x, sy, w - 140.0f, 36.0f };
    state.searchInput.boxX = sBox.x;
    state.searchInput.boxY = sBox.y;
    state.searchInput.boxW = sBox.w;
    state.searchInput.boxH = sBox.h;
    state.searchInput.boxValid = (state.currentScreen == ScreenId::Discover);

    if (state.searchInput.focused) {
        r.fillRectLinearV(sBox, 0xFF353535, theme::COL_SURFACE_CONTAINER_HIGH, 8.0f);
        r.strokeRect(sBox, theme::COL_PRIMARY, 1.5f, 8.0f);
    } else {
        r.fillRectLinearV(sBox, 0xFF2F2F2F, theme::COL_SURFACE_CONTAINER_HIGH, 8.0f);
        r.strokeRect(sBox, theme::COL_OUTLINE_VARIANT, 1.0f, 8.0f);
    }
    r.drawText(std::wstring(mdl2::Search), { sBox.x + 14, sBox.y + 9, 18, 18 },
               theme::COL_ON_SURFACE_VARIANT, 14.0f, Renderer::Icon);
    if (state.searchInput.text.empty() && !state.searchInput.focused) {
        r.drawText(t(keys::discover_search_ph),
                   { sBox.x + 40, sBox.y + 9, sBox.w - 56, 18 },
                   theme::COL_ON_SURFACE_VARIANT, 13.0f, Renderer::Regular);
    } else {
        r.drawText(state.searchInput.text,
                   { sBox.x + 40, sBox.y + 9, sBox.w - 56, 18 },
                   theme::COL_ON_SURFACE, 13.0f, Renderer::Regular);
        if (state.searchInput.focused) {
            // Cheap text-width estimate: ~7px per char at 13pt. A real
            // measurement would call IDWriteTextLayout, but this is
            // close enough for the cursor to track the caret.
            float cursorX = sBox.x + 40 + 7.0f * (float)state.searchInput.text.size();
            r.fillRect({ cursorX, sBox.y + 8, 1.5f, 20 }, theme::COL_PRIMARY);
        }
    }

    RectF searchBtn{ x + w - 130, sy, 130, 36 };
    bool hover = input.mouseInside && RectContains(searchBtn, input.mouse.x, input.mouse.y);
    drawButton(r, searchBtn,
               state.loadingSearch.load() ? t(keys::common_loading) : t(keys::common_refresh),
               true, input, hover);
    pushRect(searchBtn, 100, "discover_search");

    // Results
    if (state.loadingSearch.load()) {
        r.drawText(t(keys::common_loading), { x, sy + 50, w, 20 },
                   theme::COL_ON_SURFACE_VARIANT, 13.0f, Renderer::Regular);
    } else     if (state.searchResults.empty()) {
        r.drawText(t(keys::discover_empty), { x, sy + 50, w, 30 },
                   theme::COL_ON_SURFACE_VARIANT, 13.0f, Renderer::Regular);
    } else {
        constexpr float kRowStride = 48.0f;
        float listY = sy + 50;
        float listH = std::max(0.0f, h - (listY - y) - 8.0f);
        int   total   = (int)state.searchResults.size();
        int   visible = std::max(1, (int)(listH / kRowStride));
        int   offset  = clampScrollOffset(state, ScreenId::Discover, total, visible);

        // Snapshot the tasks once for the whole list so we can match
        // each result row to its install/queue state without
        // re-locking per row.
        auto taskSnap = bridge.snapshotTasks();

        for (int i = 0; i < visible && (offset + i) < total; ++i) {
            const auto& p = state.searchResults[offset + i];
            float ry = listY + i * kRowStride;
            RectF row{ x, ry, w - 12.0f, 42.0f };
            r.fillRectLinearV(row,
                              theme::COL_CARD_GRAD_TOP, theme::COL_CARD_GRAD_BOT,
                              6.0f);
            r.strokeRect(row, theme::COL_OUTLINE_VARIANT, 1.0f, 6.0f);
            r.drawText(p.name, { row.x + 12, row.y + 9, 240, 18 },
                       theme::COL_ON_SURFACE, 13.0f, Renderer::Regular);
            r.drawText(p.id, { row.x + 12, row.y + 25, 240, 14 },
                       theme::COL_ON_SURFACE_VARIANT, 11.0f, Renderer::Regular);
            r.drawText(p.installedVersion.empty() ? std::string("--")
                                                    : p.installedVersion,
                       { row.x + 260, row.y + 9, 100, 18 },
                       theme::COL_ON_SURFACE_VARIANT, 12.0f, Renderer::Mono);

            RectF ibtn{ row.x + row.w - 100, row.y + 7, 90, 28 };

            // Look up the task state for this package so the button
            // can show progress / queued / done locally without
            // forcing the user to switch to the Tasks screen.
            bool       inflight    = false;
            int        liveProgress = 0;
            InstallState liveState  = InstallState::Unknown;
            {
                std::lock_guard<std::mutex> lk(state.mtx);
                for (const auto& key : state.inFlight) {
                    if (key.id == p.id && key.manager == p.manager) {
                        inflight = true;
                        break;
                    }
                }
            }
            for (const auto& tt : taskSnap) {
                if (tt.package.id == p.id && tt.package.manager == p.manager) {
                    liveProgress = tt.progress;
                    liveState    = tt.state;
                    break;
                }
            }

            if (inflight
                || liveState == InstallState::Installing
                || liveState == InstallState::Updating) {
                r.fillRoundedRect(ibtn, theme::COL_SURFACE_CONTAINER_HIGHEST, 4.0f);
                if (liveProgress > 0) {
                    float frac = std::clamp(liveProgress, 0, 100) / 100.0f;
                    r.fillRoundedRect({ ibtn.x, ibtn.y, ibtn.w * frac, ibtn.h },
                                      theme::COL_PRIMARY_CONTAINER, 4.0f);
                    char pct[16];
                    std::snprintf(pct, sizeof(pct), "%s %d%%",
                                  t(keys::discover_btn_installing).c_str(),
                                  liveProgress);
                    r.drawText(pct, ibtn, theme::COL_ON_SURFACE,
                               10.0f, Renderer::Bold, true, true);
                } else {
                    // Indeterminate: half-width fill, "Installing..." label
                    r.fillRoundedRect({ ibtn.x, ibtn.y, ibtn.w * 0.5f, ibtn.h },
                                      theme::COL_PRIMARY_CONTAINER, 4.0f);
                    r.drawText(t(keys::discover_btn_installing), ibtn,
                               theme::COL_ON_PRIMARY_CONTAINER,
                               10.0f, Renderer::Bold, true, true);
                }
            } else if (liveState == InstallState::Installed) {
                r.drawText(std::wstring(mdl2::CheckMark) + L"  " +
                           std::wstring(t(keys::updates_btn_done).begin(),
                                        t(keys::updates_btn_done).end()),
                           ibtn, theme::COL_SUCCESS,
                           11.0f, Renderer::Bold, true, true);
            } else if (inflight || liveState == InstallState::Queued) {
                r.drawText(t(keys::updates_btn_queued), ibtn,
                           theme::COL_ON_SURFACE_VARIANT,
                           11.0f, Renderer::Bold, true, true);
            } else {
                bool hov = input.mouseInside
                         && RectContains(ibtn, input.mouse.x, input.mouse.y);
                drawButton(r, ibtn, t(keys::discover_btn_install), false, input, hov);
                pushRect(ibtn, 200,
                         std::string("install:") + std::string(toString(p.manager)) + ":" + p.id);
            }
        }
        drawScrollbar(r, x + w - 4.0f, listY, listH, offset, total, visible);
    }
}

// ---- Installed ----
void renderInstalled(Renderer& r, AppState& state, BackendBridge& bridge,
                     const InputState& input, float x, float y, float w, float h) {
    drawHeroHeader(r, x, y, w, t(keys::installed_title), t(keys::installed_subtitle));
    float sy = y + 90;

    RectF refreshBtn { x, sy, 130, 36 };
    bool hover = input.mouseInside && RectContains(refreshBtn, input.mouse.x, input.mouse.y);
    drawButton(r, refreshBtn, t(keys::common_refresh), true, input, hover);
    pushRect(refreshBtn, 101, "installed_refresh");

    if (state.loadingInstalled.load()) {
        r.drawText(t(keys::common_loading), { x + 150, sy + 9, 200, 18 },
                   theme::COL_ON_SURFACE_VARIANT, 13.0f, Renderer::Regular);
    } else {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%d %s",
                      (int)state.installed.size(),
                      currentLang() == Lang::En ? "packages" : "paket");
        r.drawText(buf, { x + 150, sy + 9, 200, 18 },
                   theme::COL_ON_SURFACE_VARIANT, 13.0f, Renderer::Regular);
    }

    if (state.installed.empty() && !state.loadingInstalled.load()) {
        r.drawText(t(keys::installed_empty), { x, sy + 60, w, 30 },
                   theme::COL_ON_SURFACE_VARIANT, 13.0f, Renderer::Regular);
    } else {
        const float kRowStride = 50.0f;   // logo (36) + padding
        float listY = sy + 60;
        float listH = std::max(0.0f, h - (listY - y) - 8.0f);
        int   total   = (int)state.installed.size();
        int   visible = std::max(1, (int)(listH / kRowStride));
        int   offset  = clampScrollOffset(state, ScreenId::Installed, total, visible);

        // Snapshot tasks once for the entire list draw to match
        // each installed row to its uninstall state.
        auto taskSnap = bridge.snapshotTasks();

        for (int i = 0; i < visible && (offset + i) < total; ++i) {
            const auto& p = state.installed[offset + i];
            float ry = listY + i * kRowStride;
            RectF row{ x, ry, w - 12.0f, 44.0f };
            r.fillRectLinearV(row,
                              theme::COL_CARD_GRAD_TOP, theme::COL_CARD_GRAD_BOT,
                              theme::CARD_RADIUS);
            bool rowHover = input.mouseInside
                         && input.mouse.x >= row.x && input.mouse.x <= row.x + row.w
                         && input.mouse.y >= row.y && input.mouse.y <= row.y + row.h;
            if (rowHover) {
                r.fillRectLinearV(row, 0x14FFFFFF, 0x06FFFFFF, theme::CARD_RADIUS);
            }
            r.strokeRect(row, theme::COL_OUTLINE_VARIANT, 1.0f, theme::CARD_RADIUS);

            // 3px left accent stripe in the source's brand color.
            uint32_t stripe = (p.manager == PackageManager::Winget)     ? theme::COL_PRIMARY
                           : (p.manager == PackageManager::Scoop)      ? theme::COL_TERTIARY
                           : (p.manager == PackageManager::Chocolatey) ? 0xFF6B4423u
                           :                                            theme::COL_OUTLINE;
            r.fillRectLinearV({ row.x, row.y + 6, 3.0f, row.h - 12.0f },
                              stripe, stripe, 1.5f);

            // Logo
            RectF logoRect{ row.x + 18, row.y + 4, 36, 36 };
            drawLogoPlaceholder(r, logoRect, p.id, p.manager);

            // Name + id
            r.drawText(p.name, { logoRect.x + logoRect.w + 12, row.y + 8, 280, 18 },
                       theme::COL_ON_SURFACE, 13.0f, Renderer::Regular);
            r.drawText(p.id, { logoRect.x + logoRect.w + 12, row.y + 26, 280, 14 },
                       theme::COL_ON_SURFACE_VARIANT, 11.0f, Renderer::Regular);

            // Version (right-aligned, shifted left to make room for Uninstall button)
            r.drawText(p.installedVersion, { row.x + row.w - 260, row.y + 14, 140, 16 },
                       theme::COL_ON_SURFACE_VARIANT, 12.0f, Renderer::Mono, true, true);

            // Uninstall button
            RectF ubtn{ row.x + row.w - 100, row.y + 8, 90, 28 };

            // Check if this package has an in-flight uninstall task.
            bool       inflight     = false;
            int        liveProgress = 0;
            InstallState liveState  = InstallState::Unknown;
            {
                std::lock_guard<std::mutex> lk(state.mtx);
                for (const auto& key : state.inFlight) {
                    if (key.id == p.id && key.manager == p.manager) {
                        inflight = true;
                        break;
                    }
                }
            }
            for (const auto& tt : taskSnap) {
                if (tt.package.id == p.id && tt.package.manager == p.manager
                    && tt.action == TaskAction::Uninstall) {
                    liveProgress = tt.progress;
                    liveState    = tt.state;
                    break;
                }
            }

            if (inflight
                || liveState == InstallState::Installing
                || liveState == InstallState::Updating) {
                r.fillRoundedRect(ubtn, theme::COL_SURFACE_CONTAINER_HIGHEST, 4.0f);
                if (liveProgress > 0) {
                    float frac = std::clamp(liveProgress, 0, 100) / 100.0f;
                    r.fillRoundedRect({ ubtn.x, ubtn.y, ubtn.w * frac, ubtn.h },
                                      theme::COL_ERROR, 4.0f);
                    char pct[16];
                    std::snprintf(pct, sizeof(pct), "%d%%", liveProgress);
                    r.drawText(pct, ubtn, theme::COL_ON_SURFACE,
                               10.0f, Renderer::Bold, true, true);
                } else {
                    r.fillRoundedRect({ ubtn.x, ubtn.y, ubtn.w * 0.5f, ubtn.h },
                                      theme::COL_ERROR, 4.0f);
                    r.drawText(t(keys::installed_btn_uninstalling), ubtn,
                               theme::COL_ON_SURFACE,
                               10.0f, Renderer::Bold, true, true);
                }
            } else if (liveState == InstallState::Installed) {
                // "Installed" in TaskQueue means "completed successfully"
                // for uninstall that means "removed".
                r.drawText(t(keys::installed_btn_uninstall_done), ubtn,
                           theme::COL_SUCCESS,
                           11.0f, Renderer::Bold, true, true);
            } else {
                bool hov = input.mouseInside
                         && RectContains(ubtn, input.mouse.x, input.mouse.y);
                // Uninstall uses a subtle danger styling (error color border).
                uint32_t bg = hov ? 0x33E81123 : 0;
                uint32_t bd = theme::COL_ERROR;
                if (bg) r.fillRoundedRect(ubtn, bg, 6.0f);
                r.strokeRect(ubtn, bd, 1.0f, 6.0f);
                r.drawText(t(keys::installed_btn_uninstall), ubtn,
                           theme::COL_ERROR, 12.0f, Renderer::Bold, true, true);
                pushRect(ubtn, 400,
                         std::string("uninstall:") + std::string(toString(p.manager)) + ":" + p.id);
            }
        }
        drawScrollbar(r, x + w - 4.0f, listY, listH, offset, total, visible);
    }
}

// ---- Updates (the main screen) ----
void renderUpdates(Renderer& r, AppState& state, BackendBridge& bridge,
                   const InputState& input, float x, float y, float w, float h) {
    // Hero
    drawHeroHeader(r, x, y, w, t(keys::updates_title), t(keys::updates_subtitle));

    // Action buttons (right-aligned)
    float btnY = y + 4;
    RectF refreshBtn { x + w - 360, btnY, 130, 36 };
    RectF updateAllBtn { x + w - 220, btnY, 220, 36 };
    bool hovRefresh = input.mouseInside && RectContains(refreshBtn, input.mouse.x, input.mouse.y);
    bool hovUpdateAll = input.mouseInside && RectContains(updateAllBtn, input.mouse.x, input.mouse.y);
    drawButton(r, refreshBtn, t(keys::common_refresh), false, input, hovRefresh);
    pushRect(refreshBtn, 110, "updates_refresh");
    drawButton(r, updateAllBtn, std::string("\uE895  ") + t(keys::updates_update_all),
               true, input, hovUpdateAll);
    pushRect(updateAllBtn, 111, "updates_update_all");

    // Status line
    char statusBuf[96];
    std::snprintf(statusBuf, sizeof(statusBuf), "%d %s",
                  (int)state.upgradable.size(),
                  currentLang() == Lang::En ? "updates available" : "guncelleme mevcut");
    r.drawText(statusBuf, { x, y + 64, 400, 18 },
               theme::COL_ON_SURFACE_VARIANT, 12.0f, Renderer::Regular);

    // Table header (12-col grid)
    float tableY = y + 100;
    float colW[5] = { w * 0.42f, 110, 110, 90, 160 };
    float cursorX = x;
    auto headerCol = [&](const char* txt, float w, bool right = false) {
        if (right) r.drawText(txt, { cursorX, tableY, w, 14 },
                                  theme::COL_ON_SURFACE_VARIANT, 11.0f, Renderer::Bold, false, false);
        else       r.drawText(txt, { cursorX, tableY, w, 14 },
                                  theme::COL_ON_SURFACE_VARIANT, 11.0f, Renderer::Bold, false, false);
        cursorX += w;
    };
    headerCol(t(keys::updates_col_name).c_str(),   colW[0]);
    headerCol(t(keys::updates_col_ver).c_str(),    colW[1]);
    headerCol(t(keys::updates_col_avail).c_str(),  colW[2]);
    headerCol("SOURCE",                              colW[3]);
    headerCol(t(keys::updates_col_action).c_str(), colW[4]);

    // Header bottom border
    r.fillRect({ x, tableY + 18, w, 1 }, theme::COL_OUTLINE_VARIANT);

    // Empty state
    if (state.upgradable.empty() && !state.loadingUpgradable.load()) {
        r.drawText(t(keys::updates_empty), { x, tableY + 40, w, 20 },
                   theme::COL_ON_SURFACE_VARIANT, 13.0f, Renderer::Regular);
        return;
    }

    // Table rows
    constexpr float kRowStride = kRowH + kRowGap;
    float rowY = tableY + 30;
    float listH = std::max(0.0f, h - (rowY - y) - 8.0f);
    int   total   = (int)state.upgradable.size();
    int   visible = std::max(1, (int)(listH / kRowStride));
    int   offset  = clampScrollOffset(state, ScreenId::Updates, total, visible);

    // Snapshot tasks once before the loop — avoids locking the queue
    // mutex on every row iteration (previously 200 pkgs * 60fps = 12000/sec).
    auto updatesTaskSnap = bridge.snapshotTasks();

    for (int i = 0; i < visible && (offset + i) < total; ++i) {
        const auto& p = state.upgradable[offset + i];
        float yRow = rowY + i * kRowStride;

        // Card background — gentle top-to-bottom gradient that fakes a
        // small amount of light from above, plus a 1px outline. The
        // left edge gets a 3px accent stripe in the source's brand
        // color so rows scan as belonging together even before you
        // read the badge column.
        RectF rowBg{ x, yRow, w - 12.0f, kRowH };
        r.fillRectLinearV(rowBg,
                          theme::COL_CARD_GRAD_TOP, theme::COL_CARD_GRAD_BOT,
                          theme::CARD_RADIUS);

        // Hover highlight — a subtle vertical gradient overlay applied
        // on top of the base card. Cheap visual feedback that the row
        // is interactive without animating anything.
        bool rowHover = input.mouseInside
                     && input.mouse.x >= rowBg.x && input.mouse.x <= rowBg.x + rowBg.w
                     && input.mouse.y >= rowBg.y && input.mouse.y <= rowBg.y + rowBg.h;
        if (rowHover) {
            r.fillRectLinearV(rowBg,
                              0x14FFFFFF,  // white @ ~8% alpha
                              0x06FFFFFF,  // white @ ~2% alpha
                              theme::CARD_RADIUS);
        }

        r.strokeRect(rowBg, theme::COL_OUTLINE_VARIANT, 1.0f, theme::CARD_RADIUS);

        // Left accent stripe (3px) in the source's brand color.
        uint32_t stripe = (p.manager == PackageManager::Winget)     ? theme::COL_PRIMARY
                       : (p.manager == PackageManager::Scoop)      ? theme::COL_TERTIARY
                       : (p.manager == PackageManager::Chocolatey) ? 0xFF6B4423u
                       :                                            theme::COL_OUTLINE;
        r.fillRectLinearV({ rowBg.x, rowBg.y + 6, 3.0f, rowBg.h - 12.0f },
                          stripe, stripe, 1.5f);

        // Col 0: Logo + name + publisher
        RectF logoRect{ x + 22, yRow + (kRowH - kLogoSize) / 2.0f, kLogoSize, kLogoSize };
        drawLogoPlaceholder(r, logoRect, p.id, p.manager);
        std::string publisher = "Winget";
        if (auto dot = p.id.find('.'); dot != std::string::npos) publisher = p.id.substr(0, dot);
        r.drawText(p.name, { x + 22 + kLogoSize + 14, yRow + 12, colW[0] - kLogoSize - 28, 20 },
                   theme::COL_ON_SURFACE, 15.0f, Renderer::Regular);
        r.drawText(publisher, { x + 22 + kLogoSize + 14, yRow + 34, colW[0] - kLogoSize - 28, 16 },
                   theme::COL_ON_SURFACE_VARIANT, 11.0f, Renderer::Regular);

        // Col 1: Current
        r.drawText(p.installedVersion, { x + colW[0], yRow + 22, colW[1], 20 },
                   theme::COL_ON_SURFACE_VARIANT, 13.0f, Renderer::Mono);
        // Col 2: New
        r.drawText(p.availableVersion, { x + colW[0] + colW[1], yRow + 22, colW[2], 20 },
                   theme::COL_PRIMARY, 13.0f, Renderer::Mono);

        // Col 3: Source badge
        RectF srcRect{ x + colW[0] + colW[1] + colW[2] + 4, yRow + (kRowH - 18) / 2.0f, 80, 18 };
        drawSourceBadge(r, srcRect, p.manager);

        // Col 4: Action
        RectF actRect{ x + w - 140, yRow + (kRowH - 32) / 2.0f, 130, 32 };
        bool inflight = false;
        int  liveProgress = 0;
        InstallState liveState = InstallState::Unknown;
        {
            std::lock_guard<std::mutex> lk(state.mtx);
            for (const auto& key : state.inFlight) {
                if (key.id == p.id && key.manager == p.manager) { inflight = true; break; }
            }
        }
        for (const auto& tt : updatesTaskSnap) {
            if (tt.package.id == p.id) {
                liveProgress = tt.progress;
                liveState    = tt.state;
                break;
            }
        }

        if (inflight || liveState == InstallState::Installing || liveState == InstallState::Updating) {
            r.fillRoundedRect(actRect, theme::COL_SURFACE_CONTAINER_HIGHEST, 4.0f);
            // If we don't have a real percentage yet (e.g. winget --silent
            // suppressed its own progress output, or the install just
            // started), render a thin indeterminate stripe rather than
            // a static "0%" so the user sees that something is happening.
            if (liveProgress > 0) {
                float frac = std::clamp(liveProgress, 0, 100) / 100.0f;
                r.fillRoundedRect({ actRect.x, actRect.y, actRect.w * frac, actRect.h },
                                  theme::COL_PRIMARY_CONTAINER, 4.0f);
                char pct[8]; std::snprintf(pct, sizeof(pct), "%d%%", liveProgress);
                r.drawText(pct, actRect, theme::COL_ON_SURFACE, 11.0f, Renderer::Bold, true, true);
            } else {
                r.fillRoundedRect({ actRect.x, actRect.y, actRect.w * 0.5f, actRect.h },
                                  theme::COL_PRIMARY_CONTAINER, 4.0f);
                r.drawText(t(keys::discover_btn_installing), actRect,
                           theme::COL_ON_PRIMARY_CONTAINER,
                           10.0f, Renderer::Bold, true, true);
            }
        } else if (liveState == InstallState::Installed) {
            r.drawText(std::wstring(mdl2::CheckMark) + L"  " +
                       std::wstring(t(keys::updates_btn_done).begin(),
                                    t(keys::updates_btn_done).end()),
                       actRect, theme::COL_SUCCESS, 12.0f, Renderer::Bold, true, true);
        } else {
            bool hov = input.mouseInside && RectContains(actRect, input.mouse.x, input.mouse.y);
            drawButton(r, actRect, t(keys::updates_btn_update), false, input, hov);
            pushRect(actRect, 200,
                     std::string("update:") + std::string(toString(p.manager)) + ":" + p.id);
        }
    }
    drawScrollbar(r, x + w - 4.0f, rowY, listH, offset, total, visible);
}

// ---- Tasks ----
void renderTasks(Renderer& r, AppState& state, BackendBridge& bridge,
                 const InputState& input, float x, float y, float w, float h) {
    (void)input;
    drawHeroHeader(r, x, y, w, t(keys::tasks_title), t(keys::tasks_subtitle));
    float sy = y + 90;

    char buf[128];
    std::snprintf(buf, sizeof(buf), t(keys::tasks_summary).c_str(),
                  bridge.pendingTasks(), bridge.activeTasks(), bridge.doneTasks());
    r.drawText(buf, { x, sy, 400, 18 }, theme::COL_ON_SURFACE_VARIANT, 12.0f, Renderer::Regular);

    auto snap = bridge.snapshotTasks();
    if (snap.empty()) {
        r.drawText(t(keys::tasks_empty), { x, sy + 40, w, 20 },
                   theme::COL_ON_SURFACE_VARIANT, 13.0f, Renderer::Regular);
    } else {
        constexpr float kRowStride = 42.0f;
        float listY = sy + 40;
        float listH = std::max(0.0f, h - (listY - y) - 8.0f);
        int   total   = (int)snap.size();
        int   visible = std::max(1, (int)(listH / kRowStride));
        int   offset  = clampScrollOffset(state, ScreenId::Tasks, total, visible);

        for (int i = 0; i < visible && (offset + i) < total; ++i) {
            const auto& task = snap[offset + i];
            float ry = listY + i * kRowStride;
            RectF row{ x, ry, w - 12.0f, 36.0f };
            r.fillRoundedRect(row, theme::COL_SURFACE_CONTAINER, 6.0f);
            char id[16]; std::snprintf(id, sizeof(id), "%llu", (unsigned long long)task.id);
            r.drawText(id, { row.x + 12, row.y + 9, 50, 18 },
                       theme::COL_ON_SURFACE, 13.0f, Renderer::Regular);
            r.drawText(std::string(toString(task.action).data()),
                       { row.x + 70, row.y + 9, 80, 18 },
                       theme::COL_ON_SURFACE_VARIANT, 13.0f, Renderer::Regular);
            r.drawText(task.package.id,
                       { row.x + 160, row.y + 9, 200, 18 },
                       theme::COL_ON_SURFACE, 13.0f, Renderer::Regular);
            r.drawText(std::string(toString(task.state).data()),
                       { row.x + 370, row.y + 9, 100, 18 },
                       theme::COL_PRIMARY, 13.0f, Renderer::Regular);
            // Progress bar or Error message
            if (task.state == InstallState::Failed) {
                r.drawText(task.message, { row.x + 480, row.y + 9, w - 12 - 480 - 10, 18 },
                           theme::COL_ERROR, 11.0f, Renderer::Regular);
            } else {
                RectF pb{ row.x + 480, row.y + 13, 200, 10 };
                r.fillRoundedRect(pb, theme::COL_SURFACE_CONTAINER_HIGHEST, 5.0f);
                float frac = task.progress / 100.0f;
                if (frac > 0.0f) r.fillRoundedRect({ pb.x, pb.y, pb.w * frac, pb.h },
                                                    theme::COL_PRIMARY_CONTAINER, 5.0f);
            }
        }
        drawScrollbar(r, x + w - 4.0f, listY, listH, offset, total, visible);
    }
}

// ---- Settings ----
void renderSettings(Renderer& r, AppState& state, BackendBridge& bridge,
                    const InputState& input, float x, float y, float w, float h) {
    (void)state;
    (void)bridge;
    (void)h;
    drawHeroHeader(r, x, y, w, t(keys::settings_title), t(keys::settings_subtitle));
    float sy = y + 90;

    // Language card
    RectF card { x, sy, 480, 100 };
    r.fillRoundedRect(card, 0xB2201F1F, theme::CARD_RADIUS);
    r.strokeRect(card, 0x33404752, 1.0f, theme::CARD_RADIUS);
    r.drawText(t(keys::settings_lang_label), { card.x + 20, card.y + 18, 200, 20 },
               theme::COL_ON_SURFACE, 14.0f, Renderer::Regular);

    bool isEn = currentLang() == Lang::En;
    bool isTr = currentLang() == Lang::Tr;
    RectF enBtn { card.x + 20, card.y + 50, 100, 36 };
    RectF trBtn { card.x + 140, card.y + 50, 100, 36 };
    if (isEn) r.fillRoundedRect(enBtn, theme::COL_PRIMARY_CONTAINER, 999.0f);
    else      r.strokeRect(enBtn, theme::COL_OUTLINE_VARIANT, 1.0f, 999.0f);
    if (isTr) r.fillRoundedRect(trBtn, theme::COL_PRIMARY_CONTAINER, 999.0f);
    else      r.strokeRect(trBtn, theme::COL_OUTLINE_VARIANT, 1.0f, 999.0f);
    r.drawText(t(keys::settings_lang_en), enBtn,
               isEn ? theme::COL_ON_PRIMARY_CONTAINER : theme::COL_ON_SURFACE_VARIANT,
               12.0f, Renderer::Bold, true, true);
    r.drawText(t(keys::settings_lang_tr), trBtn,
               isTr ? theme::COL_ON_PRIMARY_CONTAINER : theme::COL_ON_SURFACE_VARIANT,
               12.0f, Renderer::Bold, true, true);

    (void)input;
    pushRect(enBtn, 300, "lang_en");
    pushRect(trBtn, 300, "lang_tr");
}

} // anonymous

// External entry point: another TU (e.g. TaskDrawer) can register a
// clickable rectangle without taking a dependency on Screens internals.
// Forwards into the same clickRects() vector that the dispatcher
// below iterates.
void pushClickRect(const ClickRect& r) {
    clickRects().push_back({ {r.x, r.y, r.w, r.h}, r.id, r.payload });
}

void Screens::draw(Renderer& r, AppState& state, BackendBridge& bridge,
                   const InputState& input, float W, float H) {
    clearRects();

    float x = kContentX + kContentPadX;
    float y = kContentY + kContentPadY;
    float w = W - kContentX - 2 * kContentPadX;
    float h = H - kContentY - theme::FOOTER_H - kContentPadY;

    switch (state.currentScreen) {
        case ScreenId::Discover:  renderDiscover(r, state, bridge, input, x, y, w, h); break;
        case ScreenId::Installed: renderInstalled(r, state, bridge, input, x, y, w, h); break;
        case ScreenId::Updates:   renderUpdates(r, state, bridge, input, x, y, w, h);  break;
        case ScreenId::Tasks:     renderTasks(r, state, bridge, input, x, y, w, h);    break;
        case ScreenId::Settings:  renderSettings(r, state, bridge, input, x, y, w, h); break;
    }
}

bool ScreenHitTest(int x, int y, AppState& state, BackendBridge& bridge) {
    for (const auto& r : clickRects()) {
        if (x >= r.bounds.x && x <= r.bounds.x + r.bounds.w &&
            y >= r.bounds.y && y <= r.bounds.y + r.bounds.h) {
            switch (r.id) {
                case 100:  // discover search
                    bridge.runSearch(state.searchInput.text);
                    return true;
                case 101:  // installed refresh
                    bridge.refreshInstalled();
                    return true;
                case 110:  // updates refresh
                    bridge.refreshUpgradable();
                    return true;
                case 111:  // update all
                    bridge.enqueueUpgradeAll();
                    return true;
                case 200: {  // install / upgrade a specific package.
                            // payload format: "<action>:<manager>:<id>"
                            // e.g. "update:winget:Microsoft.VisualStudioCode"
                            auto p1 = r.payload.find(':');
                            auto p2 = r.payload.find(':', p1 == std::string::npos ? 0 : p1 + 1);
                            if (p1 == std::string::npos || p2 == std::string::npos) return true;
                            std::string action  = r.payload.substr(0, p1);
                            std::string manager = r.payload.substr(p1 + 1, p2 - p1 - 1);
                            std::string id      = r.payload.substr(p2 + 1);
                            PackageInfo pkg;
                            pkg.id      = id;
                            pkg.name    = id;
                            pkg.manager = managerFromString(manager);
                            if (action == "update")       bridge.enqueueUpgradeOne(pkg);
                            else if (action == "install") bridge.enqueueInstallOne(pkg);
                            return true;
                        }
                case 300: {  // language toggle OR task retry (payload "lang_en" / "retry:<id>")
                    if (r.payload == "lang_en") { setLang(Lang::En); return true; }
                    if (r.payload == "lang_tr") { setLang(Lang::Tr); return true; }
                    if (r.payload.rfind("retry:", 0) == 0) {
                        // Find the failed task and re-enqueue its action.
                        TaskId id = std::stoull(r.payload.substr(6));
                        auto tasks = bridge.snapshotTasks();
                        for (const auto& t : tasks) {
                            if (t.id == id && t.state == InstallState::Failed) {
                                if (t.action == TaskAction::Upgrade) {
                                    bridge.enqueueUpgradeOne(t.package);
                                } else if (t.action == TaskAction::Install) {
                                    bridge.enqueueInstallOne(t.package);
                                }
                                return true;
                            }
                        }
                    }
                    return true;
                }
                case 400: {  // uninstall a specific package.
                            // payload format: "uninstall:<manager>:<id>"
                            auto p1 = r.payload.find(':');
                            auto p2 = r.payload.find(':', p1 == std::string::npos ? 0 : p1 + 1);
                            if (p1 == std::string::npos || p2 == std::string::npos) return true;
                            std::string manager = r.payload.substr(p1 + 1, p2 - p1 - 1);
                            std::string id      = r.payload.substr(p2 + 1);
                            PackageInfo pkg;
                            pkg.id      = id;
                            pkg.name    = id;
                            pkg.manager = managerFromString(manager);
                            bridge.enqueueUninstallOne(pkg);
                            return true;
                        }
            }
        }
    }
    return false;
}

} // namespace pm::gui::win32
