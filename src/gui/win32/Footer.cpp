#include "Footer.h"
#include "../i18n.h"
#include "Theme.h"
#include <cstdio>
#include <string>

namespace pm::gui::win32 {

namespace mdl2 {
    constexpr const wchar_t* CheckMark = L"\uE73E";
    constexpr const wchar_t* Cancel    = L"\uE894";
}

// Build a "Winget: OK  |  Scoop: --  |  Choco: --" string with real
// detection results.
static std::string buildToolStrip(const AppState& state) {
    auto fmt = [](const char* name, bool ok) {
        std::string s = name;
        s += ": ";
        s += ok ? "OK" : "--";
        return s;
    };
    return fmt("Winget", state.wingetAvailable) + "   |   " +
           fmt("Scoop",  state.scoopAvailable)  + "   |   " +
           fmt("Choco",  state.chocoAvailable);
}

void Footer::draw(Renderer& r, AppState& state, BackendBridge& bridge, float W, float H) {
    (void)bridge;
    float y = H - theme::FOOTER_H;

    // Background
    r.fillRect({ theme::SIDEBAR_W, y, W - theme::SIDEBAR_W, theme::FOOTER_H },
               theme::COL_SURFACE_LOWEST);
    // Top border
    r.fillRect({ theme::SIDEBAR_W, y, W - theme::SIDEBAR_W, 1 }, theme::COL_OUTLINE_VARIANT);

    // Left: check_circle + "Background scan complete: N updates found"
    int nUpdates = (int)state.upgradable.size();
    char left[128];
    std::snprintf(left, sizeof(left), t("footer_scan_complete").c_str(), nUpdates);
    r.drawText(std::wstring(mdl2::CheckMark), { theme::SIDEBAR_W + 16, y + 8, 16, 16 },
               theme::COL_PRIMARY, 14.0f, Renderer::Icon);
    r.drawText(left, { theme::SIDEBAR_W + 36, y + 8, 280, 16 },
               theme::COL_PRIMARY, 12.0f, Renderer::Regular);

    // Divider
    r.fillRect({ theme::SIDEBAR_W + 340, y + 8, 1, 16 }, theme::COL_OUTLINE_VARIANT);

    // Middle: live tool status (auto-detected)
    std::string mid = buildToolStrip(state);
    r.drawText(mid, { theme::SIDEBAR_W + 360, y + 8, 320, 16 },
               theme::COL_ON_SURFACE_VARIANT, 12.0f, Renderer::Regular);

    // Right: System Status: Idle
    std::string right = t("footer_status_idle");
    float rw = 160;
    r.drawText(right, { W - rw - 16, y + 8, rw, 16 },
               (theme::COL_ON_SURFACE_VARIANT & 0x00FFFFFF) | 0x80000000, 11.0f, Renderer::Regular);
}

} // namespace pm::gui::win32

