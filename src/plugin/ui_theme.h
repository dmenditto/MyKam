#pragma once

#include <cstddef>
#include <string>

#include "imgui_includes.h"

// The one place for the look of the mykam windows: colours, radii, sizes, the icon glyphs, the fonts
// and the ImGuiStyle. Nothing else in the plugin hard-codes a colour or a size. Style: dark
// translucent glass, cyan accent, rounded shapes, Inter text and Material icons.

namespace mykam::theme {

// ---------------------------------------------------------------------------------------- colours
// Packed ABGR via IM_COL32(r, g, b, a). Alpha-bearing colours are used through ImGui::GetColorU32 by
// the widgets, so a window fade (style Alpha) applies to everything we draw.
inline constexpr ImU32 kGlass = IM_COL32(0x0E, 0x1A, 0x22, 235);       // #0E1A22 at about 92 percent
inline constexpr ImU32 kGlassPopup = IM_COL32(0x12, 0x22, 0x2B, 248);
inline constexpr ImU32 kSidebar = IM_COL32(0x08, 0x12, 0x18, 120);
inline constexpr ImU32 kCyan = IM_COL32(0x2F, 0xC4, 0xC4, 255);        // #2FC4C4
inline constexpr ImU32 kCyanHover = IM_COL32(0x57, 0xD6, 0xD6, 255);
inline constexpr ImU32 kCyanPressed = IM_COL32(0x22, 0xA0, 0xA0, 255);
inline constexpr ImU32 kBorder = IM_COL32(0x2F, 0xC4, 0xC4, 89);       // cyan at 35 percent
inline constexpr ImU32 kCyanSoft = IM_COL32(0x2F, 0xC4, 0xC4, 32);     // chip / hover tint
inline constexpr ImU32 kCyanGlow = IM_COL32(0x2F, 0xC4, 0xC4, 26);
inline constexpr ImU32 kCard = IM_COL32(0x17, 0x28, 0x32, 255);        // #172832
inline constexpr ImU32 kCardHover = IM_COL32(0x1D, 0x33, 0x40, 255);   // #1D3340
inline constexpr ImU32 kCardEmpty = IM_COL32(0x12, 0x20, 0x29, 170);
inline constexpr ImU32 kNavSelected = IM_COL32(0x1B, 0x30, 0x3D, 255);
inline constexpr ImU32 kField = IM_COL32(0x0B, 0x16, 0x1D, 255);       // dark inset: segmented track, inputs
inline constexpr ImU32 kTrack = IM_COL32(0x2A, 0x40, 0x4D, 255);       // slider / toggle / segmented track
inline constexpr ImU32 kText = IM_COL32(0xEA, 0xF2, 0xF5, 255);
inline constexpr ImU32 kMuted = IM_COL32(0x8A, 0xA0, 0xAB, 255);
inline constexpr ImU32 kDim = IM_COL32(0x5B, 0x71, 0x7C, 255);
inline constexpr ImU32 kOnCyan = IM_COL32(0x07, 0x1A, 0x20, 255);      // dark text on a cyan fill
inline constexpr ImU32 kAmber = IM_COL32(0xF4, 0xC5, 0x42, 255);       // #F4C542 favourite star
inline constexpr ImU32 kGreen = IM_COL32(0x3D, 0xDC, 0x84, 255);
inline constexpr ImU32 kDanger = IM_COL32(0xFF, 0x73, 0x66, 255);
inline constexpr ImU32 kHairline = IM_COL32(0xFF, 0xFF, 0xFF, 20);
inline constexpr ImU32 kShadow = IM_COL32(0, 0, 0, 22);
inline constexpr ImU32 kHoverWash = IM_COL32(0xFF, 0xFF, 0xFF, 18);

inline ImVec4 vec4(ImU32 colour) {
    return ImGui::ColorConvertU32ToFloat4(colour);
}

// ----------------------------------------------------------------------------------------- sizes
inline constexpr float kWindowRounding = 14.0f;
inline constexpr float kCardRounding = 10.0f;
inline constexpr float kShadowMargin = 10.0f;  // transparent border inside the X-Plane window, for the shadow
inline constexpr float kHeaderHeight = 52.0f;
inline constexpr float kSidebarWidth = 180.0f;
inline constexpr float kContentPad = 24.0f;
inline constexpr float kNavItemHeight = 40.0f;
inline constexpr float kCardHeight = 160.0f;
inline constexpr float kMinCardWidth = 190.0f;
inline constexpr float kFooterHeight = 68.0f;
inline constexpr float kRoundButton = 36.0f;
inline constexpr float kPillHeight = 32.0f;
inline constexpr float kFontBody = 14.0f;
inline constexpr float kFontSmall = 12.0f;
inline constexpr float kFontCard = 16.0f;
inline constexpr float kFontSection = 20.0f;
inline constexpr float kFontNumber = 28.0f;

// ------------------------------------------------------------------------------------------ icons
// A Material Icons codepoint as UTF-8 text, plus a plain fallback shown when the icon font is missing.
struct Icon {
    unsigned codepoint;
    const char* fallback;
};

inline constexpr Icon kIconCamera{0xE3B0, "o"};        // camera_alt
inline constexpr Icon kIconGrid{0xE9B0, "#"};          // grid_view
inline constexpr Icon kIconEye{0xE8F4, "o"};           // visibility
inline constexpr Icon kIconSettings{0xE8B8, "*"};      // settings
inline constexpr Icon kIconStar{0xE838, "*"};          // star
inline constexpr Icon kIconStarOutline{0xE83A, "*"};   // star_border
inline constexpr Icon kIconClose{0xE5CD, "x"};         // close
inline constexpr Icon kIconMinimise{0xE931, "-"};      // minimize
inline constexpr Icon kIconRestore{0xE3C6, "+"};       // crop_square
inline constexpr Icon kIconPopOut{0xE89E, "^"};        // open_in_new
inline constexpr Icon kIconPrevious{0xE045, "<"};      // skip_previous
inline constexpr Icon kIconNext{0xE044, ">"};          // skip_next
inline constexpr Icon kIconCockpit{0xE871, "C"};       // dashboard
inline constexpr Icon kIconFree{0xE412, "F"};          // photo_camera
inline constexpr Icon kIconChase{0xE539, "H"};         // flight
inline constexpr Icon kIconRelease{0xE879, "R"};       // exit_to_app
inline constexpr Icon kIconMenu{0xE5D2, "M"};          // menu
inline constexpr Icon kIconCircle{0xE577, "O"};        // 360
inline constexpr Icon kIconTower{0xEBBA, "T"};         // cell_tower
inline constexpr Icon kIconRunway{0xE904, "R"};        // flight_land
inline constexpr Icon kIconLinear{0xE501, "L"};        // moving
inline constexpr Icon kIconStill{0xE3B4, "S"};         // center_focus_strong
inline constexpr Icon kIconRide{0xE637, "A"};          // airline_seat_recline_normal
inline constexpr Icon kIconWingman{0xE7EF, "W"};       // group
inline constexpr Icon kIconAdd{0xE145, "+"};           // add
inline constexpr Icon kIconChevronLeft{0xE5CB, "<"};   // chevron_left
inline constexpr Icon kIconChevronRight{0xE5CC, ">"};  // chevron_right
inline constexpr Icon kIconUp{0xE316, "^"};            // keyboard_arrow_up
inline constexpr Icon kIconDown{0xE313, "v"};          // keyboard_arrow_down
inline constexpr Icon kIconXpr{0xE62D, "X"};           // vibration (XPRealistic effects)

// Every icon above, for the atlas glyph range.
inline constexpr Icon kAllIcons[] = {
    kIconCamera,  kIconGrid,   kIconEye,     kIconSettings, kIconStar,   kIconStarOutline, kIconClose,
    kIconMinimise, kIconRestore, kIconPopOut, kIconPrevious, kIconNext,   kIconCockpit,     kIconFree,
    kIconChase,   kIconRelease, kIconMenu,   kIconCircle,   kIconTower,  kIconRunway,      kIconLinear,
    kIconStill,   kIconRide,   kIconWingman, kIconAdd, kIconChevronLeft, kIconChevronRight,
    kIconUp,      kIconDown,   kIconXpr,
};

// ------------------------------------------------------------------------------------------ fonts
struct Fonts {
    ImFont* body = nullptr;     // Inter Regular 14
    ImFont* small = nullptr;    // Inter Regular 12
    ImFont* card = nullptr;     // Inter SemiBold 16
    ImFont* section = nullptr;  // Inter SemiBold 20
    ImFont* number = nullptr;   // Inter SemiBold 28 (digits only)
    ImFont* iconSmall = nullptr;   // Material Icons 14
    ImFont* iconMedium = nullptr;  // Material Icons 18
    ImFont* iconLarge = nullptr;   // Material Icons 24
    bool hasIcons = false;
    bool fellBack = false;  // some font file was missing: the built-in ImGui font stands in
};

// Adds every font to io.Fonts from `fontDirectory` (the plugin's fonts folder). `atlasScale` is 1 or 2:
// the atlas is built that many times larger and io.FontGlobalScale is set to its inverse. A missing or
// unreadable file never fails: the built-in ImGui font stands in and one warning goes to the log
// (once per X-Plane session).
Fonts buildFonts(ImGuiIO& io, const std::string& fontDirectory, float atlasScale);

// The fonts of the window being drawn. The window code sets this at the start of every frame.
void setActiveFonts(const Fonts* fonts);
const Fonts& fonts();

// The icon as text for the current font set: the glyph, or the plain fallback without the icon font.
std::string iconText(const Icon& icon);

// Fills the whole ImGuiStyle (rounding, spacing, colours).
void applyStyle(ImGuiStyle& style);

}  // namespace mykam::theme
