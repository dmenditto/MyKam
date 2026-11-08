#pragma once

#include <cstddef>
#include <vector>

// The horizontal layout of the quick bar as plain numbers: where every element sits, given the width of
// each element and the gap before it. The drawing code and the window width both read the same layout,
// so what is measured is exactly what is drawn. Pure code, no ImGui and no X-Plane calls.

namespace mykam {

// A round element (a chip circle, a round button) is drawn this many pixels inside its item rectangle on
// every side, so neighbouring circles, hover rings and the clip edge never touch.
inline constexpr float kBarCircleInset = 1.0f;

// The radius of a circle drawn in a square item of `size`.
inline float barCircleRadius(float size) {
    return size * 0.5f - kBarCircleInset;
}

struct BarElement {
    float width = 0.0f;
    float gapBefore = 0.0f;  // empty space left of the element; for the first element, the leading space
};

struct BarSlot {
    float x = 0.0f;  // left edge, measured from the start of the layout
    float width = 0.0f;
    float right() const { return x + width; }
};

struct BarLayout {
    std::vector<BarSlot> slots;  // one per element, in order
    float total = 0.0f;          // from the start to the right edge of the last element
};

// Places the elements left to right. Negative widths and gaps count as 0. `total` is the sum of all
// widths and all gaps; an element ends exactly where the next gap begins, so no two overlap.
inline BarLayout layoutBar(const std::vector<BarElement>& elements) {
    BarLayout layout;
    layout.slots.reserve(elements.size());
    float x = 0.0f;
    for (const BarElement& element : elements) {
        const float gap = element.gapBefore > 0.0f ? element.gapBefore : 0.0f;
        const float width = element.width > 0.0f ? element.width : 0.0f;
        x += gap;
        layout.slots.push_back(BarSlot{x, width});
        x += width;
    }
    layout.total = x;
    return layout;
}

// A row of items with the same gap between neighbours and none at the ends (the chips).
inline BarLayout layoutRow(const std::vector<float>& widths, float gap) {
    std::vector<BarElement> elements;
    elements.reserve(widths.size());
    for (std::size_t i = 0; i < widths.size(); ++i) {
        elements.push_back(BarElement{widths[i], i == 0 ? 0.0f : gap});
    }
    return layoutBar(elements);
}

}  // namespace mykam
