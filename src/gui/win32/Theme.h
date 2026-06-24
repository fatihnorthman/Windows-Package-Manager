#pragma once

#include <cstdint>

namespace pm::gui::win32 {

// ---- Fluent Flux design tokens (matches Stitch HTML exactly) ----
namespace theme {

// Surface levels
constexpr uint32_t COL_BACKGROUND              = 0xFF131313;  // #131313
constexpr uint32_t COL_SURFACE                 = 0xFF1C1B1B;  // #1c1b1b
constexpr uint32_t COL_SURFACE_CONTAINER       = 0xFF201F1F;  // #201f1f
constexpr uint32_t COL_SURFACE_CONTAINER_HIGH  = 0xFF2A2A2A;  // #2a2a2a
constexpr uint32_t COL_SURFACE_CONTAINER_HIGHEST = 0xFF353534; // #353534
constexpr uint32_t COL_SURFACE_LOWEST          = 0xFF0E0E0E;  // #0e0e0e
constexpr uint32_t COL_SURFACE_DIM             = 0xFF131313;  // #131313
constexpr uint32_t COL_SURFACE_BRIGHT          = 0xFF393939;  // #393939

// Gradient stops — used for ambient background tints and surface depth.
// BG: very subtle warm-dark top to cooler-dark bottom.
constexpr uint32_t COL_BG_GRAD_TOP             = 0xFF181410;  // warm-tinted near-black
constexpr uint32_t COL_BG_GRAD_BOT             = 0xFF0C0E12;  // cool-tinted near-black
// Card: gentle vertical lift, slightly lighter at the top edge.
constexpr uint32_t COL_CARD_GRAD_TOP           = 0xFF242220;  // +4 over surface_container
constexpr uint32_t COL_CARD_GRAD_BOT           = 0xFF1C1A19;  // -4 under surface_container
// Sidebar: ambient depth on the right edge so the sidebar reads as a
// recessed column when the content panel sits flush against it.
constexpr uint32_t COL_SIDEBAR_GRAD_L          = 0xFF1A1919;
constexpr uint32_t COL_SIDEBAR_GRAD_R          = 0xFF222020;
// Top bar: subtle primary-tinted highlight on the top edge fading down.
constexpr uint32_t COL_TOPBAR_GRAD_TOP         = 0xFF1F2026;  // +3 over surface, +blue tint
constexpr uint32_t COL_TOPBAR_GRAD_BOT         = 0xFF1A1919;  // = surface
// Ambient halo (radial) used behind hero areas and the empty-state
// surfaces — gives the app a soft "lit from above" feel.
constexpr uint32_t COL_HALO_CENTER              = 0x140078D4;  // primary @ 8% alpha
constexpr uint32_t COL_HALO_EDGE                = 0x00000000;  // transparent

// Text
constexpr uint32_t COL_ON_SURFACE              = 0xFFE5E2E1;  // #e5e2e1
constexpr uint32_t COL_ON_SURFACE_VARIANT      = 0xFFC0C7D4;  // #c0c7d4

// Primary
constexpr uint32_t COL_PRIMARY                 = 0xFFA3C9FF;  // #a3c9ff
constexpr uint32_t COL_PRIMARY_CONTAINER       = 0xFF0078D4;  // #0078d4
constexpr uint32_t COL_ON_PRIMARY              = 0xFF00315C;  // #00315c
constexpr uint32_t COL_ON_PRIMARY_CONTAINER    = 0xFFFFFFFF;  // #ffffff

// Secondary
constexpr uint32_t COL_SECONDARY               = 0xFFC8C6C5;  // #c8c6c5
constexpr uint32_t COL_SECONDARY_CONTAINER     = 0xFF474746;  // #474746

// Tertiary (used for Scoop source color in Stitch)
constexpr uint32_t COL_TERTIARY                = 0xFFFFB689;  // #ffb689
constexpr uint32_t COL_TERTIARY_CONTAINER      = 0xFFBC5B00;  // #bc5b00

// Error / Success
constexpr uint32_t COL_ERROR                   = 0xFFFFB4AB;  // #ffb4ab
constexpr uint32_t COL_SUCCESS                 = 0xFF66BB6A;  // soft green

// Borders
constexpr uint32_t COL_OUTLINE                 = 0xFF8A919E;  // #8a919e
constexpr uint32_t COL_OUTLINE_VARIANT         = 0xFF404752;  // #404752

// Geometry
constexpr float SIDEBAR_W   = 256.0f;
constexpr float TOPBAR_H    = 64.0f;
constexpr float FOOTER_H    = 32.0f;
constexpr float ROW_H       = 64.0f;
constexpr float ROW_GAP     = 6.0f;
constexpr float CARD_RADIUS = 8.0f;
constexpr float BTN_RADIUS  = 6.0f;
constexpr float PILL_RADIUS = 999.0f;

// Spacing (matches Tailwind stack-* in the Stitch HTML)
constexpr float SPACE_1 = 4.0f;
constexpr float SPACE_2 = 8.0f;
constexpr float SPACE_3 = 12.0f;
constexpr float SPACE_4 = 16.0f;
constexpr float SPACE_5 = 24.0f;
constexpr float SPACE_6 = 32.0f;

// Text sizes
constexpr float FONT_DISPLAY = 28.0f;
constexpr float FONT_HEADING = 22.0f;
constexpr float FONT_BODY_LG = 16.0f;
constexpr float FONT_BODY    = 14.0f;
constexpr float FONT_LABEL    = 12.0f;
constexpr float FONT_MONO     = 13.0f;
constexpr float FONT_ICON_LG  = 20.0f;
constexpr float FONT_ICON     = 16.0f;

} // namespace theme
} // namespace pm::gui::win32
