#pragma once

#include <string>

#include "imgui_includes.h"
#include "ui_theme.h"

// Small custom widgets drawn with ImDrawList in the mykam style (pill buttons, round icon buttons,
// toggle switches, sliders, segmented controls, chips, dashed card borders). Each one takes the
// cursor position like a normal ImGui item, so they mix with ordinary widgets. All colours come from
// ui_theme.h and go through ImGui::GetColorU32, so a window fade (style Alpha) applies to them too.

namespace mykam::widgets {

// A theme colour with the current style alpha applied.
ImU32 col(ImU32 colour, float alphaMul = 1.0f);
ImU32 withAlpha(ImU32 colour, float alpha);  // replaces the alpha (0..1)

// Text with a given font at its natural size (honours io.FontGlobalScale).
float fontPixels(ImFont* font);
ImVec2 textSize(ImFont* font, const char* text);
void drawText(ImDrawList* list, ImFont* font, ImVec2 pos, ImU32 colour, const char* text);
// `text` shortened with "..." so it fits `maxWidth`.
std::string ellipsize(ImFont* font, const std::string& text, float maxWidth);

enum class PillStyle { Filled, Outline };
// Pill-shaped button: filled cyan with dark text, or an outline. size.x == 0 sizes it to its content.
bool pillButton(const char* id, const char* label, const theme::Icon* icon, ImVec2 size, PillStyle style);

enum class IconStyle { Ghost, Soft, Filled, Danger };
// Round button with one icon.
bool iconButton(const char* id, const theme::Icon& icon, float diameter, IconStyle style);

// The favourite star (amber when on).
bool starButton(const char* id, bool on);

// Switch with a pill track and a round knob; cyan when on. True when clicked (value already flipped).
bool toggle(const char* id, bool* value);

// Pill group of exclusive choices. True when the selection changed; `hovered` gets the hovered segment or -1.
bool segmented(const char* id, const char* const* labels, int count, int* selected, int* hovered = nullptr);
float segmentedWidth(const char* const* labels, int count);

struct SliderResult {
    bool changed = false;   // the value moved this frame
    bool released = false;  // the mouse was let go after the value had moved
    bool active = false;    // the mouse is holding the slider
};
// Slider with a cyan filled track and a round knob, the value text on the right. `width` includes the text.
SliderResult slider(const char* id, float* value, float low, float high, float step, float width,
                    const char* valueText);

// A small pill with an icon and a label, drawn at `pos`.
float chipWidth(const theme::Icon& icon, const char* label);
void chip(ImDrawList* list, ImVec2 pos, const theme::Icon& icon, const char* label);

// A small pill with text only (a type chip, a key badge), drawn at `pos`, 22 px high. `filled` tints the
// pill with the accent; otherwise it is a dark inset. `textColour` is a theme colour.
float textChipWidth(const char* label);
void textChip(ImDrawList* list, ImVec2 pos, const char* label, ImU32 textColour, bool filled);

// A rounded rectangle border made of short segments (straight edges dashed, corners as short arcs).
void dashedRoundedRect(ImDrawList* list, ImVec2 min, ImVec2 max, float rounding, ImU32 colour, float thickness);
// A soft glow around a rounded rectangle.
void glowRoundedRect(ImDrawList* list, ImVec2 min, ImVec2 max, float rounding, ImU32 colour);
// The soft shadow of the main window.
void shadowRoundedRect(ImDrawList* list, ImVec2 min, ImVec2 max, float rounding, float spread);

// Tooltip in the glass style. Always pair a true Begin with End.
bool beginGlassTooltip();
void endGlassTooltip();
// One-line tooltip for the last item.
void tooltipText(const char* text);

}  // namespace mykam::widgets
