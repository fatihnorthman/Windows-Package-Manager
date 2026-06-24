#include "Sidebar.h"
#include "../i18n.h"
#include <cstdio>
#include <string>

namespace pm::gui::win32 {

// Segoe MDL2 Assets glyphs
namespace mdl2 {
    constexpr const wchar_t* Search     = L"\uE721";
    constexpr const wchar_t* Apps       = L"\uE71D";
    constexpr const wchar_t* Update     = L"\uE895";
    constexpr const wchar_t* CheckMark  = L"\uE73E";
    constexpr const wchar_t* Settings   = L"\uE713";
}

namespace {
struct NavItem {
    ScreenId       id;
    const wchar_t* icon;
    std::string_view key;
    const char*    badge;
};

constexpr NavItem kItems[] = {
    { ScreenId::Discover,  mdl2::Search,    keys::nav_discover,  nullptr },
    { ScreenId::Installed, mdl2::Apps,      keys::nav_installed, nullptr },
    { ScreenId::Updates,   mdl2::Update,    keys::nav_updates,   "5"   },
    { ScreenId::Tasks,     mdl2::CheckMark, keys::nav_tasks,     nullptr },
    { ScreenId::Settings,  mdl2::Settings,  keys::nav_settings,  nullptr },
};

// Layout (matches Stitch HTML: w-64, stack-md padding, etc.)
constexpr float kSidebarW    = theme::SIDEBAR_W;
constexpr float kTitleX      = 16.0f;
constexpr float kTitleY      = 18.0f;
constexpr float kItemX       = 12.0f;
constexpr float kItemY       = 92.0f;
constexpr float kItemW       = kSidebarW - 2 * kItemX;
constexpr float kItemH       = 40.0f;
constexpr float kItemGap     = 4.0f;
} // anonymous

void Sidebar::draw(Renderer& r, AppState& state, const InputState& input) {
    // Sidebar background — slightly lighter than main, like Stitch.
    RECT rcw; GetClientRect(r.hwnd(), &rcw);
    r.fillRect({ 0, 0, kSidebarW, static_cast<float>(rcw.bottom) }, theme::COL_SURFACE);

    // Right border separator
    r.fillRect({ kSidebarW - 1, 0, 1, static_cast<float>(rcw.bottom) }, theme::COL_OUTLINE_VARIANT);

    // App title (primary)
    r.drawText(t(keys::app_title), { kTitleX, kTitleY, kSidebarW - 32, 24 },
               theme::COL_PRIMARY, 18.0f, Renderer::Bold);
    r.drawText(t(keys::app_version), { kTitleX, kTitleY + 24, kSidebarW - 32, 16 },
               theme::COL_ON_SURFACE_VARIANT, 11.0f, Renderer::Regular);

    // Separator after title
    r.fillRect({ kTitleX, kTitleY + 50, kSidebarW - 32, 1 }, theme::COL_OUTLINE_VARIANT);

    // Nav items
    for (size_t i = 0; i < std::size(kItems); ++i) {
        const auto& item = kItems[i];
        float y = kItemY + static_cast<float>(i) * (kItemH + kItemGap);
        RectF itemRect { kItemX, y, kItemW, kItemH };
        bool isActive = (state.currentScreen == item.id);
        bool isHover  = !isActive && input.mouseInside
                        && input.mouse.x >= itemRect.x
                        && input.mouse.x <= itemRect.x + itemRect.w
                        && input.mouse.y >= itemRect.y
                        && input.mouse.y <= itemRect.y + itemRect.h;

        // Background
        uint32_t bg = isActive ? theme::COL_PRIMARY_CONTAINER
                     : isHover  ? theme::COL_SECONDARY_CONTAINER
                     : 0;  // transparent
        if (bg) r.fillRoundedRect(itemRect, bg, 8.0f);

        // Icon + label + (optional) badge
        uint32_t textColor = isActive ? theme::COL_ON_PRIMARY_CONTAINER : theme::COL_ON_SURFACE_VARIANT;
        r.drawText(std::wstring(item.icon), { itemRect.x + 12, itemRect.y + 8, 24, 24 },
                   textColor, 16.0f, Renderer::Icon);

        std::string label = t(item.key);
        r.drawText(label, { itemRect.x + 40, itemRect.y + 11, itemRect.w - 80, 20 },
                   textColor, 14.0f, isActive ? Renderer::Bold : Renderer::Regular);

        if (item.badge) {
            char buf[8];
            std::snprintf(buf, sizeof(buf), " %s ", item.badge);
            // Badge: small pill on the right
            float bw = 24.0f;
            r.fillRoundedRect({ itemRect.x + itemRect.w - bw - 8, itemRect.y + 9, bw, 22 },
                              isActive ? 0x33FFFFFF : theme::COL_SURFACE_CONTAINER_HIGH, 11.0f);
            r.drawText(buf, { itemRect.x + itemRect.w - bw - 8, itemRect.y + 12, bw, 18 },
                       isActive ? theme::COL_ON_PRIMARY_CONTAINER : theme::COL_ON_SURFACE_VARIANT,
                       11.0f, Renderer::Bold, true, true);
        }
    }
}

bool Sidebar::hitTest(int x, int y, AppState& state) {
    if (x < 0 || x >= kSidebarW) return false;
    for (size_t i = 0; i < std::size(kItems); ++i) {
        float iy = kItemY + static_cast<float>(i) * (kItemH + kItemGap);
        if (y >= iy && y <= iy + kItemH) {
            state.currentScreen = kItems[i].id;
            return true;
        }
    }
    return false;
}

} // namespace pm::gui::win32
