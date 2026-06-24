#include "TaskDrawer.h"
#include "../i18n.h"
#include "Theme.h"
#include <algorithm>
#include <cstdio>
#include <string>

namespace pm::gui::win32 {

namespace mdl2 {
    constexpr const wchar_t* CheckMark = L"\uE73E";
    constexpr const wchar_t* ChevronUp = L"\uE70E";
    constexpr const wchar_t* ChevronDn = L"\uE70D";
    constexpr const wchar_t* Cancel    = L"\uE894";
}

namespace {

bool RectContains(const RectF& r, int x, int y) {
    return x >= r.x && x <= r.x + r.w && y >= r.y && y <= r.y + r.h;
}

// Single row representation: package id, manager badge, action, progress.
struct Row {
    TaskId          id;
    std::string     managerLabel; // WINGET / SCOOP / CHOCO
    std::string     action;       // install / upgrade / uninstall
    std::string     package;
    InstallState    state;
    int             progress;
};

const char* managerBadge(PackageManager m) {
    switch (m) {
        case PackageManager::Winget:     return "WINGET";
        case PackageManager::Scoop:      return "SCOOP";
        case PackageManager::Chocolatey: return "CHOCO";
        default:                          return "PKG";
    }
}

uint32_t managerBadgeBg(PackageManager m) {
    switch (m) {
        case PackageManager::Winget:     return 0x1A0078D4;
        case PackageManager::Scoop:      return 0x33BC5B00;
        case PackageManager::Chocolatey: return 0x80474746;
        default:                          return 0x80474746;
    }
}

uint32_t managerBadgeText(PackageManager m) {
    switch (m) {
        case PackageManager::Winget:     return theme::COL_PRIMARY;
        case PackageManager::Scoop:      return theme::COL_TERTIARY;
        default:                          return theme::COL_ON_SURFACE_VARIANT;
    }
}

uint32_t stateColor(InstallState s) {
    switch (s) {
        case InstallState::Failed:    return theme::COL_ERROR;
        case InstallState::Installed: return theme::COL_SUCCESS;
        case InstallState::Updating:
        case InstallState::Installing:return theme::COL_PRIMARY;
        default:                      return theme::COL_ON_SURFACE_VARIANT;
    }
}

} // anonymous

bool TaskDrawer::shouldShow(const AppState& state, const BackendBridge& bridge) {
    if (state.loadingInstalled.load() || state.loadingUpgradable.load() || state.loadingSearch.load())
        return true;
    if (bridge.pendingTasks() > 0 || bridge.activeTasks() > 0) return true;
    auto snap = bridge.snapshotTasks();
    return !snap.empty(); // hide after user clears all completed
}

float TaskDrawer::draw(Renderer& r, AppState& state, BackendBridge& bridge,
                       const InputState& input, float W, float H) {
    if (!shouldShow(state, bridge)) return H - theme::FOOTER_H;

    const float headerH = state.tasksDrawerOpen ? kExpandedH : kCollapsedH;
    const float drawX   = W - kDrawerW - kMargin;
    const float drawY   = H - theme::FOOTER_H - headerH - kMargin;

    // Acrylic-ish background card (matches mockup).
    r.fillRoundedRect({ drawX, drawY, kDrawerW, headerH }, 0xD9201F1F, theme::CARD_RADIUS);
    r.strokeRect({ drawX, drawY, kDrawerW, headerH }, 0x33404752, 1.0f, theme::CARD_RADIUS);

    // --- Header (always drawn; clickable to toggle) ---
    RectF hdr { drawX, drawY, kDrawerW, kCollapsedH };
    r.drawText(std::wstring(mdl2::CheckMark),
               { hdr.x + 16, hdr.y + 16, 18, 18 },
               theme::COL_PRIMARY, 14.0f, Renderer::Icon);

    int active = bridge.activeTasks();
    int pending = bridge.pendingTasks();
    char hdrText[96];
    if (active > 0)
        std::snprintf(hdrText, sizeof(hdrText),
                      currentLang() == Lang::En ? "Task Queue (%d Active)" : "Gorev Kuyrugu (%d Aktif)",
                      active);
    else if (pending > 0)
        std::snprintf(hdrText, sizeof(hdrText),
                      currentLang() == Lang::En ? "Task Queue (%d Queued)" : "Gorev Kuyrugu (%d Kuyrukta)",
                      pending);
    else
        std::snprintf(hdrText, sizeof(hdrText),
                      currentLang() == Lang::En ? "Task Queue (Idle)" : "Gorev Kuyrugu (Bosta)");
    r.drawText(hdrText, { hdr.x + 40, hdr.y + 18, kDrawerW - 110, 20 },
               theme::COL_ON_SURFACE, 13.0f, Renderer::Bold);

    // Toggle chevron (right side)
    const wchar_t* chev = state.tasksDrawerOpen ? mdl2::ChevronDn : mdl2::ChevronUp;
    r.drawText(std::wstring(chev),
               { drawX + kDrawerW - 32, hdr.y + 18, 18, 20 },
               theme::COL_ON_SURFACE_VARIANT, 14.0f, Renderer::Icon);

    // --- Expanded body ---
    if (state.tasksDrawerOpen) {
        float bodyY = drawY + kCollapsedH + 4;
        float bodyH = headerH - kCollapsedH - 4;
        auto snap = bridge.snapshotTasks();

        if (snap.empty()) {
            r.drawText(currentLang() == Lang::En ? "No tasks yet." : "Henuz gorev yok.",
                       { drawX + 16, bodyY + 12, kDrawerW - 32, 18 },
                       theme::COL_ON_SURFACE_VARIANT, 12.0f, Renderer::Regular);
        } else {
            // Show up to 8 most recent rows.
            int shown = std::min<int>((int)snap.size(), 8);
            float rowH = (bodyH - 36.0f) / shown;
            int startIdx = std::max(0, (int)snap.size() - shown);
            for (int i = startIdx; i < (int)snap.size(); ++i) {
                const auto& t = snap[i];
                float ry = bodyY + (i - startIdx) * rowH;
                RectF row { drawX + 8, ry, kDrawerW - 16, rowH - 4.0f };
                r.fillRoundedRect(row, theme::COL_SURFACE_CONTAINER, 4.0f);

                // Manager badge (left)
                RectF bg { row.x + 8, row.y + (row.h - 16) / 2.0f, 56, 16 };
                r.fillRoundedRect(bg, managerBadgeBg(t.package.manager), 4.0f);
                r.drawText(managerBadge(t.package.manager), bg,
                           managerBadgeText(t.package.manager),
                           9.0f, Renderer::Bold, true, true);

                // Package id
                r.drawText(t.package.id,
                           { row.x + 72, row.y + 4, kDrawerW - 200, 16 },
                           theme::COL_ON_SURFACE, 12.0f, Renderer::Regular);

                // Action label (small caps)
                std::string actLabel(toString(t.action).data());
                r.drawText(actLabel,
                           { row.x + 72, row.y + row.h - 18, 80, 14 },
                           theme::COL_ON_SURFACE_VARIANT, 10.0f, Renderer::Regular);

                // Progress bar (right portion)
                RectF pb { row.x + kDrawerW - 130, row.y + (row.h - 8) / 2.0f, 60, 8 };
                r.fillRoundedRect(pb, theme::COL_SURFACE_CONTAINER_HIGHEST, 4.0f);
                if (t.progress > 0) {
                    float frac = std::clamp(t.progress, 0, 100) / 100.0f;
                    r.fillRoundedRect({ pb.x, pb.y, pb.w * frac, pb.h },
                                      stateColor(t.state), 4.0f);
                }
                // State + percent
                char pct[16];
                std::snprintf(pct, sizeof(pct), "%s %d%%",
                              std::string(toString(t.state)).c_str(), t.progress);
                r.drawText(pct,
                           { row.x + kDrawerW - 66, row.y + 4, 60, 16 },
                           stateColor(t.state), 10.0f, Renderer::Bold);
            }
        }

        // Cancel-all footer
        RectF cancelBtn { drawX + 12, drawY + headerH - 32, kDrawerW - 24, 24 };
        r.drawText(std::wstring(mdl2::Cancel),
                   { cancelBtn.x + 6, cancelBtn.y + 4, 14, 16 },
                   theme::COL_ERROR, 12.0f, Renderer::Icon);
        r.drawText(currentLang() == Lang::En ? "Cancel All" : "Tumunu Iptal Et",
                   { cancelBtn.x + 24, cancelBtn.y + 4, 200, 16 },
                   theme::COL_ERROR, 12.0f, Renderer::Bold);
    }

    return H - theme::FOOTER_H; // content area unaffected; drawer overlays
}

bool TaskDrawer::hitTest(int x, int y, AppState& state, BackendBridge& bridge,
                         float W, float H) {
    if (!shouldShow(state, bridge)) return false;
    const float headerH = state.tasksDrawerOpen ? kExpandedH : kCollapsedH;
    const float drawX   = W - kDrawerW - kMargin;
    const float drawY   = H - theme::FOOTER_H - headerH - kMargin;
    RectF card { drawX, drawY, kDrawerW, headerH };
    if (!RectContains(card, x, y)) return false;

    // Click anywhere on the header toggles expansion.
    RectF hdr { drawX, drawY, kDrawerW, kCollapsedH };
    if (RectContains(hdr, x, y)) {
        state.tasksDrawerOpen = !state.tasksDrawerOpen;
        return true;
    }
    // Expanded body: any click is "consumed" but does nothing for now
    // (no per-row cancel wired up — would need a TaskQueue::cancel API).
    return true;
}

} // namespace pm::gui::win32