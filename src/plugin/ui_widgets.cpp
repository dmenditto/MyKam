#include "ui_widgets.h"

#include <algorithm>
#include <cfloat>
#include <cmath>

#include "core/bar_layout.h"

namespace mykam::widgets {

namespace {

constexpr float kPi = 3.14159265f;

float lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

}  // namespace

ImU32 col(ImU32 colour, float alphaMul) {
    return ImGui::GetColorU32(colour, alphaMul);
}

ImU32 withAlpha(ImU32 colour, float alpha) {
    const float clamped = std::min(std::max(alpha, 0.0f), 1.0f);
    return (colour & 0x00FFFFFFu) | (static_cast<ImU32>(clamped * 255.0f + 0.5f) << 24);
}

float fontPixels(ImFont* font) {
    return font->FontSize * ImGui::GetIO().FontGlobalScale;
}

ImVec2 textSize(ImFont* font, const char* text) {
    return font->CalcTextSizeA(fontPixels(font), FLT_MAX, 0.0f, text);
}

void drawText(ImDrawList* list, ImFont* font, ImVec2 pos, ImU32 colour, const char* text) {
    list->AddText(font, fontPixels(font), pos, col(colour), text);
}

std::string ellipsize(ImFont* font, const std::string& text, float maxWidth) {
    if (textSize(font, text.c_str()).x <= maxWidth) {
        return text;
    }
    std::string cut = text;
    while (!cut.empty()) {
        cut.pop_back();
        const std::string candidate = cut + "...";
        if (textSize(font, candidate.c_str()).x <= maxWidth) {
            return candidate;
        }
    }
    return std::string();
}

bool pillButton(const char* id, const char* label, const theme::Icon* icon, ImVec2 size, PillStyle style) {
    const theme::Fonts& f = theme::fonts();
    const std::string glyph = icon != nullptr ? theme::iconText(*icon) : std::string();
    const ImVec2 labelSize = textSize(f.body, label);
    const ImVec2 glyphSize = glyph.empty() ? ImVec2(0.0f, 0.0f) : textSize(f.iconSmall, glyph.c_str());
    const float gap = glyph.empty() ? 0.0f : 6.0f;
    const float contentW = glyphSize.x + gap + labelSize.x;
    if (size.x <= 0.0f) {
        size.x = contentW + 32.0f;
    }
    if (size.y <= 0.0f) {
        size.y = theme::kPillHeight;
    }
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const bool pressed = ImGui::InvisibleButton(id, size);
    const bool hovered = ImGui::IsItemHovered();
    const bool held = ImGui::IsItemActive();
    const ImVec2 max(pos.x + size.x, pos.y + size.y);
    const float radius = size.y * 0.5f;
    ImDrawList* list = ImGui::GetWindowDrawList();
    ImU32 textColour = theme::kOnCyan;
    if (style == PillStyle::Filled) {
        list->AddRectFilled(pos, max, col(held ? theme::kCyanPressed : hovered ? theme::kCyanHover : theme::kCyan),
                            radius);
    } else {
        if (hovered || held) {
            list->AddRectFilled(pos, max, col(held ? withAlpha(theme::kCyan, 0.28f) : theme::kCyanSoft), radius);
        }
        list->AddRect(pos, max, col(hovered ? theme::kCyanHover : theme::kCyan), radius, 0, 1.5f);
        textColour = hovered ? theme::kCyanHover : theme::kCyan;
    }
    float x = pos.x + (size.x - contentW) * 0.5f;
    if (!glyph.empty()) {
        drawText(list, f.iconSmall, ImVec2(x, pos.y + (size.y - glyphSize.y) * 0.5f), textColour, glyph.c_str());
        x += glyphSize.x + gap;
    }
    drawText(list, f.body, ImVec2(x, pos.y + (size.y - labelSize.y) * 0.5f), textColour, label);
    return pressed;
}

bool iconButton(const char* id, const theme::Icon& icon, float diameter, IconStyle style) {
    const theme::Fonts& f = theme::fonts();
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const bool pressed = ImGui::InvisibleButton(id, ImVec2(diameter, diameter));
    const bool hovered = ImGui::IsItemHovered();
    const bool held = ImGui::IsItemActive();
    const ImVec2 centre(pos.x + diameter * 0.5f, pos.y + diameter * 0.5f);
    // Every circle stays inside its item rectangle (1 px inset), so neighbours never touch or overlap.
    const float radius = barCircleRadius(diameter);
    ImDrawList* list = ImGui::GetWindowDrawList();
    ImU32 glyphColour = hovered ? theme::kText : theme::kMuted;
    switch (style) {
        case IconStyle::Ghost:
            if (hovered || held) {
                list->AddCircleFilled(centre, radius, col(theme::kHoverWash), 24);
            }
            break;
        case IconStyle::Soft:
            list->AddCircleFilled(centre, radius, col(held || hovered ? theme::kCardHover : theme::kCard), 32);
            list->AddCircle(centre, radius - 0.5f, col(theme::kHairline), 32, 1.0f);
            glyphColour = hovered ? theme::kCyanHover : theme::kText;
            break;
        case IconStyle::Filled:
            list->AddCircleFilled(centre, radius,
                                  col(held ? theme::kCyanPressed : hovered ? theme::kCyanHover : theme::kCyan), 32);
            glyphColour = theme::kOnCyan;
            break;
        case IconStyle::Danger:
            if (hovered || held) {
                list->AddCircleFilled(centre, radius, col(withAlpha(theme::kDanger, held ? 0.45f : 0.30f)), 24);
                glyphColour = theme::kDanger;
            }
            break;
    }
    const std::string glyph = theme::iconText(icon);
    ImFont* font = diameter >= 32.0f ? f.iconLarge : f.iconMedium;
    const ImVec2 size = textSize(font, glyph.c_str());
    drawText(list, font, ImVec2(centre.x - size.x * 0.5f, centre.y - size.y * 0.5f), glyphColour, glyph.c_str());
    return pressed;
}

bool starButton(const char* id, bool on) {
    const theme::Fonts& f = theme::fonts();
    constexpr float kSize = 26.0f;
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const bool pressed = ImGui::InvisibleButton(id, ImVec2(kSize, kSize));
    const bool hovered = ImGui::IsItemHovered();
    const std::string glyph = theme::iconText(on ? theme::kIconStar : theme::kIconStarOutline);
    const ImVec2 size = textSize(f.iconMedium, glyph.c_str());
    ImDrawList* list = ImGui::GetWindowDrawList();
    const ImU32 colour = on ? theme::kAmber : (hovered ? theme::kText : theme::kDim);
    drawText(list, f.iconMedium, ImVec2(pos.x + (kSize - size.x) * 0.5f, pos.y + (kSize - size.y) * 0.5f), colour,
             glyph.c_str());
    return pressed;
}

bool toggle(const char* id, bool* value) {
    constexpr float kWidth = 44.0f;
    constexpr float kHeight = 24.0f;
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const bool pressed = ImGui::InvisibleButton(id, ImVec2(kWidth, kHeight));
    if (pressed) {
        *value = !*value;
    }
    const bool hovered = ImGui::IsItemHovered();
    ImGuiStorage* storage = ImGui::GetStateStorage();
    const ImGuiID key = ImGui::GetItemID();
    const float target = *value ? 1.0f : 0.0f;
    float& t = *storage->GetFloatRef(key, target);
    t += (target - t) * std::min(1.0f, ImGui::GetIO().DeltaTime * 16.0f);
    if (std::fabs(target - t) < 0.01f) {
        t = target;
    }
    ImDrawList* list = ImGui::GetWindowDrawList();
    const ImVec2 max(pos.x + kWidth, pos.y + kHeight);
    const ImU32 off = hovered ? theme::kTrack : IM_COL32(0x24, 0x38, 0x44, 255);
    const ImU32 trackColour = t > 0.5f ? (hovered ? theme::kCyanHover : theme::kCyan) : off;
    list->AddRectFilled(pos, max, col(trackColour), kHeight * 0.5f);
    const float knobR = kHeight * 0.5f - 3.0f;
    const float knobX = lerp(pos.x + 3.0f + knobR, max.x - 3.0f - knobR, t);
    list->AddCircleFilled(ImVec2(knobX, pos.y + kHeight * 0.5f), knobR,
                          col(t > 0.5f ? IM_COL32(0xFF, 0xFF, 0xFF, 255) : theme::kMuted), 24);
    return pressed;
}

float segmentedWidth(const char* const* labels, int count) {
    const theme::Fonts& f = theme::fonts();
    float total = 8.0f;
    for (int i = 0; i < std::min(count, 8); ++i) {
        total += textSize(f.body, labels[i]).x + 32.0f;
    }
    return total;
}

bool segmented(const char* id, const char* const* labels, int count, int* selected, int* hovered) {
    const theme::Fonts& f = theme::fonts();
    constexpr float kHeight = 32.0f;
    constexpr float kPad = 16.0f;
    float widths[8] = {};
    count = std::min(count, 8);
    float total = 4.0f;
    for (int i = 0; i < count; ++i) {
        widths[i] = textSize(f.body, labels[i]).x + 2.0f * kPad;
        total += widths[i];
    }
    total += 4.0f;
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const bool pressed = ImGui::InvisibleButton(id, ImVec2(total, kHeight));
    const bool itemHovered = ImGui::IsItemHovered();
    const float mouseX = ImGui::GetIO().MousePos.x;
    int hit = -1;
    float x = pos.x + 4.0f;
    for (int i = 0; i < count; ++i) {
        if (itemHovered && mouseX >= x && mouseX < x + widths[i]) {
            hit = i;
        }
        x += widths[i];
    }
    if (hovered != nullptr) {
        *hovered = hit;
    }
    bool changed = false;
    if (pressed && hit >= 0 && hit != *selected) {
        *selected = hit;
        changed = true;
    }
    ImDrawList* list = ImGui::GetWindowDrawList();
    list->AddRectFilled(pos, ImVec2(pos.x + total, pos.y + kHeight), col(theme::kField), kHeight * 0.5f);
    x = pos.x + 4.0f;
    for (int i = 0; i < count; ++i) {
        const ImVec2 a(x, pos.y + 3.0f);
        const ImVec2 b(x + widths[i], pos.y + kHeight - 3.0f);
        const bool on = i == *selected;
        if (on) {
            list->AddRectFilled(a, b, col(theme::kCyan), (b.y - a.y) * 0.5f);
        } else if (i == hit) {
            list->AddRectFilled(a, b, col(theme::kHoverWash), (b.y - a.y) * 0.5f);
        }
        const ImVec2 size = textSize(f.body, labels[i]);
        drawText(list, f.body, ImVec2(x + (widths[i] - size.x) * 0.5f, pos.y + (kHeight - size.y) * 0.5f),
                 on ? theme::kOnCyan : (i == hit ? theme::kText : theme::kMuted), labels[i]);
        x += widths[i];
    }
    return changed;
}

SliderResult slider(const char* id, float* value, float low, float high, float step, float width,
                    const char* valueText) {
    const theme::Fonts& f = theme::fonts();
    constexpr float kHeight = 24.0f;
    constexpr float kValueWidth = 56.0f;
    constexpr float kKnob = 8.0f;
    const float trackWidth = std::max(48.0f, width - kValueWidth);
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton(id, ImVec2(trackWidth, kHeight));
    SliderResult result;
    result.active = ImGui::IsItemActive();
    const bool hovered = ImGui::IsItemHovered();
    ImGuiStorage* storage = ImGui::GetStateStorage();
    const ImGuiID key = ImGui::GetItemID();
    const float x0 = pos.x + kKnob;
    const float x1 = pos.x + trackWidth - kKnob;
    if (result.active) {
        float t = (ImGui::GetIO().MousePos.x - x0) / std::max(x1 - x0, 1.0f);
        t = std::min(std::max(t, 0.0f), 1.0f);
        float next = low + t * (high - low);
        if (step > 0.0f) {
            next = std::round(next / step) * step;
        }
        next = std::min(std::max(next, low), high);
        if (std::fabs(next - *value) > 1e-6f) {
            *value = next;
            result.changed = true;
            storage->SetBool(key, true);
        }
    }
    if (ImGui::IsItemDeactivated()) {
        result.released = storage->GetBool(key, false);
        storage->SetBool(key, false);
    }
    const float span = high - low;
    const float fraction = span > 0.0f ? std::min(std::max((*value - low) / span, 0.0f), 1.0f) : 0.0f;
    const float knobX = lerp(x0, x1, fraction);
    const float midY = pos.y + kHeight * 0.5f;
    ImDrawList* list = ImGui::GetWindowDrawList();
    list->AddRectFilled(ImVec2(pos.x, midY - 2.0f), ImVec2(pos.x + trackWidth, midY + 2.0f), col(theme::kTrack), 2.0f);
    list->AddRectFilled(ImVec2(pos.x, midY - 2.0f), ImVec2(knobX, midY + 2.0f), col(theme::kCyan), 2.0f);
    if (hovered || result.active) {
        list->AddCircleFilled(ImVec2(knobX, midY), kKnob + 4.0f, col(withAlpha(theme::kCyan, 0.20f)), 24);
    }
    list->AddCircleFilled(ImVec2(knobX, midY), kKnob, col(IM_COL32(0xFF, 0xFF, 0xFF, 255)), 24);
    list->AddCircle(ImVec2(knobX, midY), kKnob, col(theme::kCyan), 24, 2.0f);
    const ImVec2 size = textSize(f.body, valueText);
    drawText(list, f.body, ImVec2(pos.x + width - size.x, midY - size.y * 0.5f), theme::kText, valueText);
    return result;
}

float chipWidth(const theme::Icon& icon, const char* label) {
    const theme::Fonts& f = theme::fonts();
    const std::string glyph = theme::iconText(icon);
    return 10.0f + textSize(f.iconSmall, glyph.c_str()).x + 5.0f + textSize(f.small, label).x + 10.0f;
}

void chip(ImDrawList* list, ImVec2 pos, const theme::Icon& icon, const char* label) {
    const theme::Fonts& f = theme::fonts();
    constexpr float kHeight = 22.0f;
    const std::string glyph = theme::iconText(icon);
    const ImVec2 glyphSize = textSize(f.iconSmall, glyph.c_str());
    const ImVec2 labelSize = textSize(f.small, label);
    const float width = chipWidth(icon, label);
    list->AddRectFilled(pos, ImVec2(pos.x + width, pos.y + kHeight), col(theme::kCyanSoft), kHeight * 0.5f);
    drawText(list, f.iconSmall, ImVec2(pos.x + 10.0f, pos.y + (kHeight - glyphSize.y) * 0.5f), theme::kCyan,
             glyph.c_str());
    drawText(list, f.small, ImVec2(pos.x + 15.0f + glyphSize.x, pos.y + (kHeight - labelSize.y) * 0.5f), theme::kCyan,
             label);
}

float textChipWidth(const char* label) {
    const theme::Fonts& f = theme::fonts();
    return 20.0f + textSize(f.small, label).x;
}

void textChip(ImDrawList* list, ImVec2 pos, const char* label, ImU32 textColour, bool filled) {
    const theme::Fonts& f = theme::fonts();
    constexpr float kHeight = 22.0f;
    const ImVec2 labelSize = textSize(f.small, label);
    const float width = textChipWidth(label);
    list->AddRectFilled(pos, ImVec2(pos.x + width, pos.y + kHeight), col(filled ? theme::kCyanSoft : theme::kField),
                        kHeight * 0.5f);
    drawText(list, f.small, ImVec2(pos.x + 10.0f, pos.y + (kHeight - labelSize.y) * 0.5f), textColour, label);
}

void dashedRoundedRect(ImDrawList* list, ImVec2 min, ImVec2 max, float rounding, ImU32 colour, float thickness) {
    constexpr float kDash = 6.0f;
    constexpr float kGap = 4.0f;
    const ImU32 c = col(colour);
    const float r = std::min(rounding, std::min((max.x - min.x), (max.y - min.y)) * 0.5f);
    for (float x = min.x + r; x < max.x - r; x += kDash + kGap) {
        const float x2 = std::min(x + kDash, max.x - r);
        list->AddLine(ImVec2(x, min.y), ImVec2(x2, min.y), c, thickness);
        list->AddLine(ImVec2(x, max.y), ImVec2(x2, max.y), c, thickness);
    }
    for (float y = min.y + r; y < max.y - r; y += kDash + kGap) {
        const float y2 = std::min(y + kDash, max.y - r);
        list->AddLine(ImVec2(min.x, y), ImVec2(min.x, y2), c, thickness);
        list->AddLine(ImVec2(max.x, y), ImVec2(max.x, y2), c, thickness);
    }
    if (r > 0.5f) {
        list->PathArcTo(ImVec2(min.x + r, min.y + r), r, kPi, kPi * 1.5f, 8);
        list->PathStroke(c, 0, thickness);
        list->PathArcTo(ImVec2(max.x - r, min.y + r), r, kPi * 1.5f, kPi * 2.0f, 8);
        list->PathStroke(c, 0, thickness);
        list->PathArcTo(ImVec2(max.x - r, max.y - r), r, 0.0f, kPi * 0.5f, 8);
        list->PathStroke(c, 0, thickness);
        list->PathArcTo(ImVec2(min.x + r, max.y - r), r, kPi * 0.5f, kPi, 8);
        list->PathStroke(c, 0, thickness);
    }
}

void glowRoundedRect(ImDrawList* list, ImVec2 min, ImVec2 max, float rounding, ImU32 colour) {
    for (int i = 1; i <= 4; ++i) {
        const float grow = static_cast<float>(i) * 1.5f;
        const float alpha = 0.22f * (1.0f - static_cast<float>(i - 1) / 4.0f);
        list->AddRect(ImVec2(min.x - grow, min.y - grow), ImVec2(max.x + grow, max.y + grow),
                      col(withAlpha(colour, alpha)), rounding + grow, 0, 2.0f);
    }
}

void shadowRoundedRect(ImDrawList* list, ImVec2 min, ImVec2 max, float rounding, float spread) {
    // Stacked translucent rounded rectangles, wide and faint at the outside. Cheap: eight shapes.
    constexpr int kLayers = 8;
    for (int i = kLayers; i >= 1; --i) {
        const float grow = spread * static_cast<float>(i) / static_cast<float>(kLayers);
        list->AddRectFilled(ImVec2(min.x - grow, min.y - grow + 3.0f), ImVec2(max.x + grow, max.y + grow + 3.0f),
                            col(theme::kShadow), rounding + grow);
    }
}

bool beginGlassTooltip() {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 7.0f));
    ImGui::PushStyleColor(ImGuiCol_PopupBg, theme::vec4(theme::kGlassPopup));
    if (ImGui::BeginTooltip()) {
        return true;
    }
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
    return false;
}

void endGlassTooltip() {
    ImGui::EndTooltip();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
}

void tooltipText(const char* text) {
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip) && beginGlassTooltip()) {
        ImGui::TextUnformatted(text);
        endGlassTooltip();
    }
}

}  // namespace mykam::widgets
