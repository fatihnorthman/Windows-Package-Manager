#pragma once

#include <cstdint>

namespace pm::gui::win32 {

// ---- Fluent Flux design tokens (matches Stitch HTML exactly) ----
namespace theme {

// Surface levels
constexpr uint32_t COL_BACKGROUND              = 0xFF0D0C15;  // Indigo-dark #0d0c15
constexpr uint32_t COL_SURFACE                 = 0xFF14131A;  // Dark surface
constexpr uint32_t COL_SURFACE_CONTAINER       = 0xFF1C1A24;  // Container surface
constexpr uint32_t COL_SURFACE_CONTAINER_HIGH  = 0xFF262330;  // High container
constexpr uint32_t COL_SURFACE_CONTAINER_HIGHEST = 0xFF312F3D; // Highest container
constexpr uint32_t COL_SURFACE_LOWEST          = 0xFF07060A;  // Lowest dark
constexpr uint32_t COL_SURFACE_DIM             = 0xFF0D0C15;  // Dim
constexpr uint32_t COL_SURFACE_BRIGHT          = 0xFF353147;  // Bright

// Mesh Background and Glass Colors
constexpr uint32_t COL_MESH_BG_TOP             = 0xFF0D0C15;  // Deep, rich dark indigo
constexpr uint32_t COL_MESH_BG_BOT             = 0xFF07080B;  // Near-black charcoal
constexpr uint32_t COL_GLOW_VIOLET             = 0x2B6C33A3;  // Glowing violet radial highlight (alpha ~17%)
constexpr uint32_t COL_GLOW_CYAN               = 0x1F00A389;  // Glowing cyan radial highlight (alpha ~12%)
constexpr uint32_t COL_GLASS_CARD_BG           = 0xCC18171F;  // Acrylic Glass: 80% opacity dark grey
constexpr uint32_t COL_GLASS_CARD_BORDER       = 0x22FFFFFF;  // Soft semi-transparent white border
constexpr uint32_t COL_GLASS_CARD_HOVER_BG     = 0x3DFFFFFF;  // Brighter glass hover fill
constexpr uint32_t COL_GLASS_CARD_HOVER_BORDER = 0x40FFFFFF;  // Brighter glass hover border

// Gradient stops — used for ambient background tints and surface depth.
// BG: very subtle warm-dark top to cooler-dark bottom.
constexpr uint32_t COL_BG_GRAD_TOP             = 0xFF0D0C15;  // Indigo
constexpr uint32_t COL_BG_GRAD_BOT             = 0xFF07080B;  // Charcoal
// Card: gentle vertical lift, slightly lighter at the top edge.
constexpr uint32_t COL_CARD_GRAD_TOP           = 0xCC1E1D26;  // Acrylic dark top
constexpr uint32_t COL_CARD_GRAD_BOT           = 0xCC14131A;  // Acrylic dark bottom
// Sidebar: ambient depth on the right edge so the sidebar reads as a
// recessed column when the content panel sits flush against it.
constexpr uint32_t COL_SIDEBAR_GRAD_L          = 0xFF14131A;
constexpr uint32_t COL_SIDEBAR_GRAD_R          = 0xFF0D0C15;
// Top bar: subtle primary-tinted highlight on the top edge fading down.
constexpr uint32_t COL_TOPBAR_GRAD_TOP         = 0xFF1C1A24;
constexpr uint32_t COL_TOPBAR_GRAD_BOT         = 0xFF14131A;
// Ambient halo (radial) used behind hero areas and the empty-state
// surfaces — gives the app a soft "lit from above" feel.
constexpr uint32_t COL_HALO_CENTER              = 0x220078D4;  // primary @ 13% alpha
constexpr uint32_t COL_HALO_EDGE                = 0x00000000;  // transparent

// Text
constexpr uint32_t COL_ON_SURFACE              = 0xFFF0EDEC;  // #f0edec
constexpr uint32_t COL_ON_SURFACE_VARIANT      = 0xFFC9D0DB;  // #c9d0db

// Primary
constexpr uint32_t COL_PRIMARY                 = 0xFFADC6FF;  // #adc6ff
constexpr uint32_t COL_PRIMARY_CONTAINER       = 0xFF1A5FB4;  // #1a5fb4
constexpr uint32_t COL_ON_PRIMARY              = 0xFF00224D;  // #00224d
constexpr uint32_t COL_ON_PRIMARY_CONTAINER    = 0xFFFFFFFF;  // #ffffff

// Secondary
constexpr uint32_t COL_SECONDARY               = 0xFFD2CFCE;  // #d2cfce
constexpr uint32_t COL_SECONDARY_CONTAINER     = 0xFF3E3C40;  // #3e3c40

// Tertiary (used for Scoop source color in Stitch)
constexpr uint32_t COL_TERTIARY                = 0xFFFFBCA1;  // #ffbca1
constexpr uint32_t COL_TERTIARY_CONTAINER      = 0xFFA74300;  // #a74300

// Error / Success
constexpr uint32_t COL_ERROR                   = 0xFFFFB3A9;  // #ffb3a9
constexpr uint32_t COL_SUCCESS                 = 0xFF81C784;  // soft green

// Borders
constexpr uint32_t COL_OUTLINE                 = 0xFF8F9AA9;  // #8f9aa9
constexpr uint32_t COL_OUTLINE_VARIANT         = 0xFF2A2D35;  // #2a2d35

// Geometry
constexpr float SIDEBAR_W   = 80.0f;       // Sleek compact navigation bar
constexpr float TOPBAR_H    = 64.0f;
constexpr float FOOTER_H    = 32.0f;
constexpr float ROW_H       = 64.0f;
constexpr float ROW_GAP     = 6.0f;
constexpr float CARD_RADIUS = 12.0f;      // Softer Fluent corners
constexpr float BTN_RADIUS  = 8.0f;       // Softer button corners
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
