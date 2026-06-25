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
    { ScreenId::Updates,   mdl2::Update,    keys::nav_updates,   nullptr },  // badge set dynamically
    { ScreenId::Tasks,     mdl2::CheckMark, keys::nav_tasks,     nullptr },
    { ScreenId::Settings,  mdl2::Settings,  keys::nav_settings,  nullptr },
};

// Layout for the floating compact sidebar dock
constexpr float kSidebarW    = theme::SIDEBAR_W; // 80.0f
constexpr float kDockX       = 12.0f;
constexpr float kDockW       = kSidebarW - 2 * kDockX; // 56.0f
constexpr float kItemX       = 18.0f;
constexpr float kItemW       = 44.0f;
constexpr float kItemH       = 44.0f;
constexpr float kItemY       = 80.0f;
constexpr float kItemGap     = 12.0f;
} // anonymous

void Sidebar::draw(Renderer& r, AppState& state, const InputState& input) {
    RECT rcw; GetClientRect(r.hwnd(), &rcw);
    float H = static_cast<float>(rcw.bottom);

    // Floating Glass Sidebar Dock
    RectF dockRect{ kDockX, 12.0f, kDockW, H - 24.0f };
    r.fillRoundedRect(dockRect, theme::COL_GLASS_CARD_BG, 16.0f);
    r.strokeRect(dockRect, theme::COL_GLASS_CARD_BORDER, 1.0f, 16.0f);

    // Top Brand Logo Icon
    RectF logoRect{ kDockX + 8.0f, 24.0f, 40.0f, 40.0f };
    r.fillRoundedRect(logoRect, theme::COL_PRIMARY_CONTAINER, 12.0f);
    r.strokeRect(logoRect, theme::COL_PRIMARY, 1.0f, 12.0f);
    // Draw "PM" in bold
    r.drawText(L"PM", logoRect, theme::COL_ON_PRIMARY_CONTAINER, 14.0f, Renderer::Bold, true, true);

    // Nav items
    for (size_t i = 0; i < std::size(kItems); ++i) {
        const auto& item = kItems[i];
        float y = kItemY + static_cast<float>(i) * (kItemH + kItemGap);
        RectF itemRect { kItemX, y, kItemW, kItemH };
        bool isActive = (state.currentScreen == item.id);
        bool isHover  = !isActive && input.mouseInside
                        && static_cast<float>(input.mouse.x) >= itemRect.x
                        && static_cast<float>(input.mouse.x) <= itemRect.x + itemRect.w
                        && static_cast<float>(input.mouse.y) >= itemRect.y
                        && static_cast<float>(input.mouse.y) <= itemRect.y + itemRect.h;

        // Glass background with vertical linear gradient
        if (isActive) {
            r.fillRectLinearV(itemRect, 0xFF0086EE, theme::COL_PRIMARY_CONTAINER, 12.0f);
            r.strokeRect(itemRect, theme::COL_PRIMARY, 1.0f, 12.0f);
        } else if (isHover) {
            r.fillRoundedRect(itemRect, theme::COL_GLASS_CARD_HOVER_BG, 12.0f);
            r.strokeRect(itemRect, theme::COL_GLASS_CARD_HOVER_BORDER, 1.0f, 12.0f);
        }

        // Centered Nav Icon
        uint32_t textColor = isActive ? theme::COL_ON_PRIMARY_CONTAINER : theme::COL_ON_SURFACE_VARIANT;
        r.drawText(std::wstring(item.icon), itemRect, textColor, 16.0f, Renderer::Icon, true, true);

        // Notification dot/badge in top-right corner of the icon
        int count = 0;
        if (item.id == ScreenId::Updates) {
            count = static_cast<int>(state.upgradable.size());
        }
        
        if (count > 0) {
            float dotSize = 8.0f;
            RectF dotRect { itemRect.x + itemRect.w - dotSize - 4.0f, itemRect.y + 4.0f, dotSize, dotSize };
            r.fillRoundedRect(dotRect, theme::COL_ERROR, 999.0f);
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
