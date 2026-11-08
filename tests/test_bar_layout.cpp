#include "doctest.h"

#include <cstddef>
#include <vector>

#include "bar_layout.h"

using mykam::BarElement;
using mykam::BarLayout;

namespace {

// The gaps the quick bar uses (kBarChipGap, kBarSectionGap in ui_windows.cpp) and the 30 px round button.
constexpr float kChipGap = 6.0f;
constexpr float kSectionGap = 10.0f;
constexpr float kButton = 30.0f;

float sumOf(const std::vector<BarElement>& elements) {
    float total = 0.0f;
    for (const BarElement& e : elements) {
        total += e.width + e.gapBefore;
    }
    return total;
}

// No two neighbours overlap, every gap is kept exactly, and the layout ends where the last element ends.
void checkSequence(const std::vector<BarElement>& elements, const BarLayout& layout) {
    REQUIRE(layout.slots.size() == elements.size());
    float previousRight = 0.0f;
    for (std::size_t i = 0; i < elements.size(); ++i) {
        INFO("element " << i);
        CHECK(layout.slots[i].x == doctest::Approx(previousRight + elements[i].gapBefore));
        CHECK(layout.slots[i].width == doctest::Approx(elements[i].width));
        CHECK(layout.slots[i].x >= previousRight - 0.0001f);  // never left of the previous element's edge
        previousRight = layout.slots[i].right();
    }
    CHECK(layout.total == doctest::Approx(previousRight));
}

}  // namespace

TEST_CASE("bar layout: the total is the sum of all widths and all gaps") {
    const std::vector<BarElement> elements = {{120.0f, 12.0f}, {260.0f, 10.0f}, {30.0f, 10.0f}, {9.0f, 10.0f},
                                              {30.0f, 10.0f},  {30.0f, 6.0f},   {30.0f, 6.0f},  {0.0f, 12.0f}};
    const BarLayout layout = mykam::layoutBar(elements);
    CHECK(layout.total == doctest::Approx(sumOf(elements)));
    checkSequence(elements, layout);
}

TEST_CASE("bar layout: elements follow each other with exactly their gap, so none overlaps") {
    std::vector<BarElement> elements;
    elements.push_back({60.5f, 12.0f});
    for (int i = 0; i < 25; ++i) {
        elements.push_back({kButton + static_cast<float>(i % 4) * 7.25f, i % 5 == 0 ? kSectionGap : kChipGap});
    }
    elements.push_back({0.0f, 12.0f});
    const BarLayout layout = mykam::layoutBar(elements);
    checkSequence(elements, layout);
    for (std::size_t i = 1; i < layout.slots.size(); ++i) {
        INFO("pair " << i);
        CHECK(layout.slots[i].x - layout.slots[i - 1].right() == doctest::Approx(elements[i].gapBefore));
        CHECK(layout.slots[i].x >= layout.slots[i - 1].right());
    }
}

TEST_CASE("bar layout: the first gap is the leading space, an empty list has no width") {
    std::vector<BarElement> single;
    single.push_back(BarElement{40.0f, 12.0f});
    const BarLayout one = mykam::layoutBar(single);
    REQUIRE(one.slots.size() == 1);
    CHECK(one.slots[0].x == doctest::Approx(12.0f));
    CHECK(one.total == doctest::Approx(52.0f));

    const BarLayout none = mykam::layoutBar(std::vector<BarElement>());
    CHECK(none.slots.empty());
    CHECK(none.total == doctest::Approx(0.0f));
}

TEST_CASE("bar layout: negative widths and gaps count as zero, so elements still never overlap") {
    const std::vector<BarElement> elements = {{-5.0f, -3.0f}, {20.0f, -8.0f}, {10.0f, 4.0f}};
    const BarLayout layout = mykam::layoutBar(elements);
    REQUIRE(layout.slots.size() == 3);
    CHECK(layout.slots[0].x == doctest::Approx(0.0f));
    CHECK(layout.slots[0].width == doctest::Approx(0.0f));
    CHECK(layout.slots[1].x == doctest::Approx(0.0f));
    CHECK(layout.slots[2].x == doctest::Approx(24.0f));
    CHECK(layout.total == doctest::Approx(34.0f));
}

TEST_CASE("bar layout: a row of chips has the chip gap between neighbours and none at the ends") {
    const std::vector<float> widths = {30.0f, 30.0f, 71.5f, 30.0f, 88.25f};
    const BarLayout row = mykam::layoutRow(widths, kChipGap);
    REQUIRE(row.slots.size() == widths.size());
    float sum = 0.0f;
    for (float w : widths) {
        sum += w;
    }
    CHECK(row.total == doctest::Approx(sum + 4.0f * kChipGap));
    CHECK(row.slots[0].x == doctest::Approx(0.0f));
    for (std::size_t i = 1; i < row.slots.size(); ++i) {
        CHECK(row.slots[i].x - row.slots[i - 1].right() == doctest::Approx(kChipGap));
    }
    CHECK(row.slots.back().right() == doctest::Approx(row.total));

    CHECK(mykam::layoutRow(std::vector<float>(), kChipGap).total == doctest::Approx(0.0f));
    CHECK(mykam::layoutRow(std::vector<float>(1, 30.0f), kChipGap).total == doctest::Approx(30.0f));
}

TEST_CASE("bar layout: circles are drawn 1 px inside their item, so neighbouring circles keep a gap") {
    CHECK(mykam::kBarCircleInset >= 1.0f);
    CHECK(mykam::barCircleRadius(kButton) == doctest::Approx(14.0f));
    CHECK(mykam::barCircleRadius(26.0f) == doctest::Approx(12.0f));

    // Two round chips side by side: the circles are at least gap + 2 * inset apart.
    const BarLayout row = mykam::layoutRow({kButton, kButton}, kChipGap);
    const float centreA = row.slots[0].x + kButton * 0.5f;
    const float centreB = row.slots[1].x + kButton * 0.5f;
    const float clearance = (centreB - centreA) - 2.0f * mykam::barCircleRadius(kButton);
    CHECK(clearance >= kChipGap + 2.0f * mykam::kBarCircleInset - 0.0001f);
    // And a circle never reaches its item's edge.
    CHECK(centreA - mykam::barCircleRadius(kButton) >= row.slots[0].x + mykam::kBarCircleInset - 0.0001f);
    CHECK(centreB + mykam::barCircleRadius(kButton) <= row.slots[1].right() - mykam::kBarCircleInset + 0.0001f);
}

TEST_CASE("bar layout: a whole bar of round elements, with any number of view buttons, has no overlap and the right total") {
    for (int views = 0; views <= 10; ++views) {
        for (int chips = 0; chips <= 12; chips += 4) {
            INFO("views " << views << " chips " << chips);
            std::vector<float> chipWidths(static_cast<std::size_t>(chips), kButton);
            const BarLayout chipRow = mykam::layoutRow(chipWidths, kChipGap);

            std::vector<BarElement> bar;
            bar.push_back({150.0f, 12.0f});                // the switch
            bar.push_back({chipRow.total, kSectionGap});   // the chip area
            bar.push_back({kButton, kSectionGap});         // +
            bar.push_back({9.0f, kSectionGap});            // separator
            for (int i = 0; i < views; ++i) {
                bar.push_back({kButton, i == 0 ? kSectionGap : kChipGap});
            }
            bar.push_back({kButton, views == 0 ? kSectionGap : kChipGap});  // release
            bar.push_back({kButton, kChipGap});                             // menu
            bar.push_back({0.0f, 12.0f});
            const BarLayout layout = mykam::layoutBar(bar);
            checkSequence(bar, layout);
            CHECK(layout.total == doctest::Approx(sumOf(bar)));
            // Every round button: its circle stays inside its slot and clear of the next circle.
            for (std::size_t i = 4; i + 2 < layout.slots.size(); ++i) {
                const float rightCircle = layout.slots[i].x + kButton * 0.5f + mykam::barCircleRadius(kButton);
                const float nextLeftCircle = layout.slots[i + 1].x + kButton * 0.5f - mykam::barCircleRadius(kButton);
                CHECK(nextLeftCircle - rightCircle >= kChipGap + 2.0f - 0.0001f);
            }
        }
    }
}
