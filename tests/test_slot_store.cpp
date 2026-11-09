#include "doctest.h"

#include <cmath>
#include <string>
#include <vector>

#include "slot_store.h"
#include "view_commands.h"

using mykam::Pose;
using mykam::ViewGroup;
using mykam::ViewKind;
using mykam::ViewStore;

namespace {

Pose makePose(double x, double y, double z, double pitch, double heading, double roll, double zoom) {
    Pose p;
    p.x = x;
    p.y = y;
    p.z = z;
    p.pitch = pitch;
    p.heading = heading;
    p.roll = roll;
    p.zoom = zoom;
    return p;
}

Pose somePose(double x = 1.5) {
    return makePose(x, 2, 3, 4, 5, 6, 1);
}

// A complete valid old-format slot as text.
std::string slotText(int n, const std::string& x = "1.5", const std::string& zoom = "1") {
    const std::string p = "slot" + std::to_string(n) + ".";
    return p + "x=" + x + "\n" + p + "y=2\n" + p + "z=3\n" + p + "pitch=4\n" + p + "heading=5\n" + p + "roll=6\n" + p +
           "zoom=" + zoom + "\n";
}

// A complete valid new-format view as text.
std::string viewText(int id, const std::string& x = "1.5", const std::string& zoom = "1") {
    const std::string p = "view" + std::to_string(id) + ".";
    return p + "x=" + x + "\n" + p + "y=2\n" + p + "z=3\n" + p + "pitch=4\n" + p + "heading=5\n" + p + "roll=6\n" + p +
           "zoom=" + zoom + "\n";
}

const std::string kHeader = "format=2\n";

}  // namespace

// ---------------------------------------------------------------- new format

TEST_CASE("view store: the new format writes a version line, next_id and viewID.field lines") {
    ViewStore s;
    const int a = s.add(somePose(1.25), ViewKind::Cockpit, "", 3);
    const int b = s.add(somePose(2.5), ViewKind::External, "sim/view/chase");
    REQUIRE(a == 1);
    REQUIRE(b == 2);
    REQUIRE(s.setFavourite(b, true));
    REQUIRE(s.setName(a, "Left seat"));

    const std::string expected =
        "format=2\n"
        "next_id=3\n"
        "view1.x=1.25\nview1.y=2\nview1.z=3\nview1.pitch=4\nview1.heading=5\nview1.roll=6\nview1.zoom=1\n"
        "view1.kind=cockpit\nview1.name=Left seat\nview1.order=1\nview1.hotkey=3\n"
        "view2.x=2.5\nview2.y=2\nview2.z=3\nview2.pitch=4\nview2.heading=5\nview2.roll=6\nview2.zoom=1\n"
        "view2.kind=external\nview2.name=Outside view 1\nview2.order=1\nview2.view=sim/view/chase\nview2.favourite=1\n";
    CHECK(s.serialize() == expected);
}

TEST_CASE("view store: save, serialize and parse round trip keeps every field") {
    ViewStore a;
    REQUIRE(a.add(makePose(1.25, 2.5, -3.75, 10.0, 359.5, -12.0, 1.8), ViewKind::External, "sim/view/free_camera", 1) != 0);
    REQUIRE(a.add(makePose(-9999.0, 0.0, 9999.0, -89.0, 0.0, 180.0, 0.1), ViewKind::Cockpit, "", 9) != 0);
    const int third = a.add(somePose(), ViewKind::External);
    REQUIRE(a.setName(third, "Behind the tail"));
    REQUIRE(a.setFavourite(third, true));

    ViewStore b;
    const mykam::ViewParseResult r = b.parse(a.serialize());
    CHECK(r.loaded == 3);
    CHECK_FALSE(r.migrated);
    CHECK(r.problems.empty());
    REQUIRE(b.count() == 3);
    const mykam::SavedView* v1 = b.find(1);
    const mykam::SavedView* v2 = b.find(2);
    const mykam::SavedView* v3 = b.find(3);
    REQUIRE(v1 != nullptr);
    REQUIRE(v2 != nullptr);
    REQUIRE(v3 != nullptr);
    CHECK(v1->pose.x == 1.25);
    CHECK(v1->pose.heading == 359.5);
    CHECK(v1->pose.zoom == 1.8);
    CHECK(v1->viewCommand == "sim/view/free_camera");
    CHECK(v1->hotkey == 1);
    CHECK(v1->kind == ViewKind::External);
    CHECK(v2->kind == ViewKind::Cockpit);
    CHECK(v2->pose.z == 9999.0);
    CHECK(v2->hotkey == 9);
    CHECK(v3->name == "Behind the tail");
    CHECK(v3->favourite);
    CHECK(v3->hotkey == 0);
    CHECK(b.serialize() == a.serialize());
}

TEST_CASE("view store: 99 views with the longest names, commands and values stay well under 64 KB (NFR2.6)") {
    ViewStore s;
    const std::string longName(32, 'N');
    const std::string longCommand = "sim/view/" + std::string(55, 'a');
    REQUIRE(longCommand.size() == mykam::kMaxViewCommandNameLength);
    for (int i = 0; i < mykam::kMaxViews; ++i) {
        const int id = s.add(makePose(-9876.543210987654, -1234.5678901234567, 8765.432109876543, -89.99999999999999,
                                      359.1234567890123, -179.87654321098765, 1.2345678901234567),
                             ViewKind::External, longCommand, i < 9 ? i + 1 : 0, longName);
        REQUIRE(id != 0);
        REQUIRE(s.setFavourite(id, true));
    }
    const std::string text = s.serialize();
    INFO("size " << text.size());
    CHECK(text.size() < mykam::kMaxSlotFileBytes);
    CHECK(text.size() < 40 * 1024);

    ViewStore back;
    const mykam::ViewParseResult r = back.parse(text);
    CHECK(r.loaded == 99);
    CHECK(r.problems.empty());
    // Stored to 4 decimals: far inside the 1 cm and 0.1 degree budget.
    REQUIRE(back.find(99) != nullptr);
    CHECK(std::fabs(back.find(99)->pose.x - (-9876.543210987654)) < 0.001);
    CHECK(std::fabs(back.find(99)->pose.heading - 359.1234567890123) < 0.001);
}

TEST_CASE("view store: ids are stable and never reused, in memory and across a save and load") {
    ViewStore s;
    CHECK(s.add(somePose(), ViewKind::External) == 1);
    CHECK(s.add(somePose(), ViewKind::External) == 2);
    CHECK(s.add(somePose(), ViewKind::External) == 3);
    CHECK(s.nextId() == 4);

    CHECK(s.remove(2));  // a hole in the middle
    CHECK(s.add(somePose(), ViewKind::External) == 4);
    CHECK(s.remove(4));  // the highest id: the next view still gets a new id
    CHECK(s.add(somePose(), ViewKind::External) == 5);
    CHECK_FALSE(s.remove(2));  // already gone
    CHECK(s.find(2) == nullptr);

    // Delete the highest, write the file, read it back: the id is still not handed out again.
    REQUIRE(s.remove(5));
    ViewStore back;
    back.parse(s.serialize());
    CHECK(back.count() == 2);
    CHECK(back.nextId() == 6);
    CHECK(back.add(somePose(), ViewKind::External) == 6);

    // The ids of the views that are left did not change.
    CHECK(back.find(1) != nullptr);
    CHECK(back.find(3) != nullptr);
}

TEST_CASE("view store: a file keeps the highest id it has ever used even without next_id") {
    ViewStore s;
    s.parse(kHeader + viewText(7));
    CHECK(s.nextId() == 8);
    s.parse(kHeader + "next_id=50\n" + viewText(7));
    CHECK(s.nextId() == 50);
    s.parse(kHeader + "next_id=3\n" + viewText(7));  // a smaller next_id never lowers it
    CHECK(s.nextId() == 8);
    const mykam::ViewParseResult r = s.parse(kHeader + "next_id=zero\n" + viewText(2));
    CHECK(r.loaded == 1);
    CHECK(r.problems.size() == 1);
    CHECK(s.nextId() == 3);
}

TEST_CASE("view store: at most 99 views, the 100th is refused") {
    ViewStore s;
    for (int i = 1; i <= mykam::kMaxViews; ++i) {
        REQUIRE(s.add(somePose(), ViewKind::External) == i);
    }
    CHECK(s.full());
    CHECK(s.count() == 99);
    CHECK(s.add(somePose(), ViewKind::Cockpit) == 0);
    CHECK(s.count() == 99);
    CHECK(s.remove(40));
    CHECK_FALSE(s.full());
    CHECK(s.add(somePose(), ViewKind::Cockpit) == 100);  // a new id, not 40
}

TEST_CASE("view store: a file with more than 99 views loads the first 99 and reports the rest") {
    std::string text = kHeader;
    for (int id = 1; id <= 101; ++id) {
        text += viewText(id);
    }
    ViewStore s;
    const mykam::ViewParseResult r = s.parse(text);
    CHECK(r.loaded == 99);
    CHECK(s.count() == 99);
    CHECK(r.problems.size() == 2);
    CHECK(s.find(99) != nullptr);
    CHECK(s.find(100) == nullptr);
    CHECK(s.nextId() == 102);
}

// ---------------------------------------------------------------- hotkeys

TEST_CASE("view store: a hotkey is unique per aircraft; assigning one that is used moves it") {
    ViewStore s;
    const int a = s.add(somePose(), ViewKind::External);
    const int b = s.add(somePose(), ViewKind::Cockpit);
    CHECK(s.hotkeyOwner(3) == 0);
    CHECK(s.setHotkey(a, 3));
    CHECK(s.hotkeyOwner(3) == a);
    CHECK(s.has(3));
    CHECK(s.kind(3) == ViewKind::External);

    CHECK(s.setHotkey(b, 3));  // moves from a to b
    CHECK(s.hotkeyOwner(3) == b);
    CHECK(s.find(a)->hotkey == 0);
    CHECK(s.find(b)->hotkey == 3);
    CHECK(s.kind(3) == ViewKind::Cockpit);

    CHECK(s.setHotkey(b, 3));  // the same key again changes nothing
    CHECK(s.hotkeyOwner(3) == b);
    CHECK(s.setHotkey(b, 5));  // another key frees the old one
    CHECK(s.hotkeyOwner(3) == 0);
    CHECK(s.hotkeyOwner(5) == b);
    CHECK(s.setHotkey(b, 0));  // clear
    CHECK(s.find(b)->hotkey == 0);
    CHECK_FALSE(s.has(5));

    CHECK_FALSE(s.setHotkey(b, 11));
    CHECK_FALSE(s.setHotkey(b, -1));
    CHECK_FALSE(s.setHotkey(99, 1));  // unknown view
    CHECK(s.hotkeyOwner(0) == 0);
    CHECK(s.hotkeyOwner(11) == 0);
    CHECK(s.setHotkey(b, mykam::kHotkeyZero));  // numpad 0
    CHECK(s.hotkeyOwner(mykam::kHotkeyZero) == b);
}

TEST_CASE("view store: adding a view with a used hotkey takes the key away from the other view") {
    ViewStore s;
    const int a = s.add(somePose(), ViewKind::External, "", 2);
    const int b = s.add(somePose(), ViewKind::External, "", 2);
    CHECK(s.find(a)->hotkey == 0);
    CHECK(s.find(b)->hotkey == 2);
    const int c = s.add(somePose(), ViewKind::External, "", 12);  // not a hotkey: none
    CHECK(s.find(c)->hotkey == 0);
}

TEST_CASE("view store: the hotkey round trips and a duplicate in a file is ignored for the later view") {
    ViewStore s;
    const mykam::ViewParseResult r =
        s.parse(kHeader + viewText(1) + "view1.hotkey=4\n" + viewText(2) + "view2.hotkey=4\n" + viewText(3) +
                "view3.hotkey=7\n" + viewText(4) + "view4.hotkey=10\n" + viewText(5) + "view5.hotkey=x\n" + viewText(6) +
                "view6.hotkey=0\n");
    CHECK(r.loaded == 6);
    CHECK(s.find(1)->hotkey == 4);
    CHECK(s.find(2)->hotkey == 0);
    CHECK(s.find(3)->hotkey == 7);
    CHECK(s.find(4)->hotkey == 0);
    CHECK(s.find(5)->hotkey == 0);
    CHECK(s.find(6)->hotkey == mykam::kHotkeyZero);  // 0 in the file is numpad 0
    CHECK(r.problems.size() == 3);  // the duplicate, 10 and x
    CHECK(s.hotkeyOwner(4) == 1);
    CHECK(s.hotkeyOwner(mykam::kHotkeyZero) == 6);
}

TEST_CASE("view store: numpad 0 is stored as hotkey=0 and read back") {
    ViewStore s;
    const int id = s.add(somePose(), ViewKind::External, std::string(), mykam::kHotkeyZero);
    REQUIRE(id != 0);
    CHECK(s.find(id)->hotkey == mykam::kHotkeyZero);
    const std::string text = s.serialize();
    CHECK(text.find("view" + std::to_string(id) + ".hotkey=0\n") != std::string::npos);
    ViewStore back;
    back.parse(text);
    CHECK(back.find(id)->hotkey == mykam::kHotkeyZero);
    CHECK(mykam::hotkeyText(mykam::kHotkeyZero) == "0");
    CHECK(mykam::hotkeyText(7) == "7");
}

// ---------------------------------------------------------------- names, defaults, order

TEST_CASE("view store: default names are the lowest free 'Cockpit view N' or 'Outside view N'") {
    ViewStore s;
    CHECK(s.defaultName(ViewKind::Cockpit) == "Cockpit view 1");
    CHECK(s.defaultName(ViewKind::External) == "Outside view 1");
    const int c1 = s.add(somePose(), ViewKind::Cockpit);
    const int c2 = s.add(somePose(), ViewKind::Cockpit);
    const int e1 = s.add(somePose(), ViewKind::External);
    CHECK(s.find(c1)->name == "Cockpit view 1");
    CHECK(s.find(c2)->name == "Cockpit view 2");
    CHECK(s.find(e1)->name == "Outside view 1");
    CHECK(s.defaultName(ViewKind::Cockpit) == "Cockpit view 3");

    CHECK(s.remove(c1));  // the lowest number is free again
    CHECK(s.defaultName(ViewKind::Cockpit) == "Cockpit view 1");
    CHECK(s.setName(c2, "Cockpit view 1"));  // a name the user typed counts as used
    CHECK(s.defaultName(ViewKind::Cockpit) == "Cockpit view 2");
}

TEST_CASE("view store: setName takes only a trimmed, printable name of 1 to 32 characters") {
    ViewStore s;
    const int id = s.add(somePose(), ViewKind::External);
    CHECK(s.setName(id, "Left wing"));
    CHECK(s.find(id)->name == "Left wing");
    CHECK_FALSE(s.setName(id, " padded "));  // callers pass the trimmed name
    CHECK_FALSE(s.setName(id, ""));
    CHECK_FALSE(s.setName(id, std::string(33, 'a')));
    CHECK_FALSE(s.setName(id, "tab\there"));
    CHECK_FALSE(s.setName(99, "nobody"));
    CHECK(s.find(id)->name == "Left wing");
    CHECK(s.setName(id, std::string(32, 'a')));

    std::string out;
    CHECK(mykam::checkSlotName("  Left wing  ", out) == mykam::NameCheck::Ok);
    CHECK(out == "Left wing");
    CHECK(mykam::checkSlotName("", out) == mykam::NameCheck::Empty);
    CHECK(mykam::checkSlotName("caf\xC3\xA9", out) == mykam::NameCheck::NotPrintable);
    CHECK(mykam::checkSlotName(std::string(33, 'a'), out) == mykam::NameCheck::TooLong);
}

TEST_CASE("view store: groups list by the order field only, a favourite gets no special place; cockpit is Internal") {
    ViewStore s;
    const int i1 = s.add(somePose(), ViewKind::Cockpit);
    const int e1 = s.add(somePose(), ViewKind::External);
    const int i2 = s.add(somePose(), ViewKind::Cockpit);
    const int e2 = s.add(somePose(), ViewKind::External);
    const int i3 = s.add(somePose(), ViewKind::Cockpit);
    const int e3 = s.add(somePose(), ViewKind::External);
    CHECK(s.count(ViewGroup::Internal) == 3);
    CHECK(s.count(ViewGroup::External) == 3);
    CHECK(s.ordered(ViewGroup::Internal) == std::vector<int>({i1, i2, i3}));
    CHECK(s.ordered(ViewGroup::External) == std::vector<int>({e1, e2, e3}));

    s.setFavourite(i3, true);
    s.setFavourite(e2, true);
    s.setFavourite(e3, true);
    CHECK(s.ordered(ViewGroup::Internal) == std::vector<int>({i1, i2, i3}));  // stars do not reorder
    CHECK(s.ordered(ViewGroup::External) == std::vector<int>({e1, e2, e3}));
    CHECK(s.allOrdered() == std::vector<int>({i1, i2, i3, e1, e2, e3}));
    CHECK(s.favourites(ViewGroup::Internal) == std::vector<int>({i3}));  // the quick bar list, in group order
    CHECK(s.favourites(ViewGroup::External) == std::vector<int>({e2, e3}));
    CHECK(mykam::groupOf(ViewKind::Cockpit) == ViewGroup::Internal);
    CHECK(mykam::groupOf(ViewKind::External) == ViewGroup::External);
}

TEST_CASE("step view: goes through Internal then External, wraps, and handles no position") {
    ViewStore s;
    CHECK(mykam::stepView(s, 0, 1) == 0);  // nothing saved
    const int e1 = s.add(somePose(), ViewKind::External);
    const int i1 = s.add(somePose(), ViewKind::Cockpit);
    const int e2 = s.add(somePose(), ViewKind::External);
    // Order: i1, e1, e2
    CHECK(mykam::stepView(s, i1, 1) == e1);
    CHECK(mykam::stepView(s, e1, 1) == e2);
    CHECK(mykam::stepView(s, e2, 1) == i1);  // wraps forwards
    CHECK(mykam::stepView(s, i1, -1) == e2);  // wraps backwards
    CHECK(mykam::stepView(s, e2, -1) == e1);
    CHECK(mykam::stepView(s, 0, 1) == i1);   // no position: first
    CHECK(mykam::stepView(s, 0, -1) == e2);  // no position: last
    CHECK(mykam::stepView(s, 77, 1) == i1);  // unknown id counts as no position
    ViewStore one;
    const int only = one.add(somePose(), ViewKind::External);
    CHECK(mykam::stepView(one, only, 1) == only);
    CHECK(mykam::stepView(one, only, -1) == only);
}

TEST_CASE("view store: update keeps name, favourite and hotkey; kind and view command follow the new view") {
    ViewStore s;
    const int id = s.add(somePose(1.0), ViewKind::External, "sim/view/chase", 4);
    s.setName(id, "Keep me");
    s.setFavourite(id, true);
    REQUIRE(s.update(id, somePose(9.0), ViewKind::Cockpit, "sim/view/chase"));
    const mykam::SavedView* v = s.find(id);
    REQUIRE(v != nullptr);
    CHECK(v->pose.x == 9.0);
    CHECK(v->kind == ViewKind::Cockpit);
    CHECK(v->viewCommand.empty());  // a cockpit view keeps no view command
    CHECK(v->name == "Keep me");
    CHECK(v->favourite);
    CHECK(v->hotkey == 4);
    CHECK_FALSE(s.update(id, makePose(99999, 0, 0, 0, 0, 0, 1), ViewKind::External));  // invalid pose
    CHECK_FALSE(s.update(500, somePose(), ViewKind::External));
    CHECK(s.find(id)->pose.x == 9.0);
}

TEST_CASE("view store: values exactly on the limits are accepted, others refused") {
    ViewStore s;
    CHECK(s.add(makePose(10000, -10000, 10000, 360, -360, 360, 10), ViewKind::External) != 0);
    CHECK(s.add(makePose(0, 0, 0, 0, 0, 0, 0.1), ViewKind::External) != 0);
    CHECK(s.add(makePose(10000.01, 0, 0, 0, 0, 0, 1), ViewKind::External) == 0);
    CHECK(s.add(makePose(0, 0, 0, 0, 0, 0, 0.09), ViewKind::External) == 0);
    CHECK(s.count() == 2);
}

// ---------------------------------------------------------------- parsing problems

TEST_CASE("view store: empty content means no views (FR5.2) and parse replaces what was there") {
    ViewStore s;
    s.add(somePose(), ViewKind::External);
    const mykam::ViewParseResult r = s.parse("");
    CHECK(r.loaded == 0);
    CHECK_FALSE(r.migrated);
    CHECK(r.problems.empty());
    CHECK(s.count() == 0);
    CHECK(s.nextId() == 1);
    CHECK(s.find(0) == nullptr);
    CHECK_FALSE(s.has(0));
    CHECK_FALSE(s.has(10));
    CHECK(s.get(3) == nullptr);
}

TEST_CASE("view store: a broken view is skipped and the others still load") {
    const std::string text = kHeader + viewText(1) +
                             viewText(2, "abc") +                // not a number
                             viewText(3, "1", "99") +            // zoom out of range
                             "view4.x=1\nview4.y=2\n" +          // fields missing
                             viewText(5) + viewText(6) + "view6.kind=helicopter\n" +  // unknown kind
                             "garbage line\n" + "viewX.x=1\n" + "view7.colour=red\n" + "# a comment\n\n";
    ViewStore s;
    const mykam::ViewParseResult r = s.parse(text);
    CHECK(r.loaded == 2);
    CHECK(s.find(1) != nullptr);
    CHECK(s.find(2) == nullptr);
    CHECK(s.find(3) == nullptr);
    CHECK(s.find(4) == nullptr);
    CHECK(s.find(5) != nullptr);
    CHECK(s.find(6) == nullptr);
    // view 2, 3, 4, 6, "garbage line", "viewX.x" and "view7.colour"
    CHECK(r.problems.size() == 7);
}

TEST_CASE("view store: out-of-range, non-finite and non-numeric values are rejected per view (NFR2.5)") {
    struct Case {
        const char* field;
        const char* value;
    };
    const Case bad[] = {
        {"x", "10000.5"}, {"y", "-20000"}, {"z", "nan"},   {"pitch", "361"}, {"heading", "-361"},
        {"roll", "inf"},  {"zoom", "0.05"}, {"zoom", "10.5"}, {"x", ""},       {"y", "1e999"},
        {"z", "12abc"},   {"x", "0x10"},
    };
    for (const Case& c : bad) {
        std::string text = kHeader + viewText(1) + viewText(2);
        const std::string key = std::string("view1.") + c.field + "=";
        const std::size_t at = text.find(key);
        REQUIRE(at != std::string::npos);
        const std::size_t eol = text.find('\n', at);
        text.replace(at, eol - at, key + c.value);
        ViewStore s;
        const mykam::ViewParseResult r = s.parse(text);
        INFO("field " << c.field << " value '" << c.value << "'");
        CHECK(r.loaded == 1);
        CHECK(s.find(1) == nullptr);
        CHECK(s.find(2) != nullptr);  // the other view is not affected
        CHECK(r.problems.size() == 1);
    }
}

TEST_CASE("view store: a bad name, favourite or view command is ignored and the view still loads") {
    ViewStore s;
    const mykam::ViewParseResult r =
        s.parse(kHeader + viewText(1) + "view1.name=" + std::string(40, 'x') + "\nview1.favourite=yes\nview1.view=chase\n" +
                viewText(2) + "view2.name=caf\xC3\xA9\n" + viewText(3) + "view3.name=  First  \nview3.name = Second\n" +
                "view3.favourite = 1\nview3.view=sim/view/chase\n");
    CHECK(r.loaded == 3);
    CHECK(r.problems.size() == 4);  // long name, favourite, view command, non-ASCII name
    CHECK(s.find(1)->name == "Outside view 1");
    CHECK_FALSE(s.find(1)->favourite);
    CHECK(s.find(1)->viewCommand.empty());
    CHECK(s.find(2)->name == "Outside view 2");
    CHECK(s.find(3)->name == "Second");  // the last name line wins, blanks trimmed
    CHECK(s.find(3)->favourite);
    CHECK(s.find(3)->viewCommand == "sim/view/chase");
}

TEST_CASE("view store: a view without a kind is external; kind is case-insensitive; a lone kind line stores nothing") {
    ViewStore s;
    const mykam::ViewParseResult r = s.parse(kHeader + viewText(1) + viewText(2) + "view2.kind=Cockpit\n" + viewText(3) +
                                             "view3.kind=EXTERNAL\n" + "view4.kind=cockpit\nview4.name=ghost\n");
    CHECK(r.loaded == 3);
    CHECK(r.problems.empty());
    CHECK(s.find(1)->kind == ViewKind::External);
    CHECK(s.find(2)->kind == ViewKind::Cockpit);
    CHECK(s.find(3)->kind == ViewKind::External);
    CHECK(s.find(4) == nullptr);
}

TEST_CASE("view store: a file over 64 KB is ignored with a warning (NFR2.6)") {
    std::string text = kHeader + viewText(1);
    text += std::string(mykam::kMaxSlotFileBytes, '#');  // padding pushes it over the limit
    CHECK(text.size() > mykam::kMaxSlotFileBytes);
    ViewStore s;
    const mykam::ViewParseResult r = s.parse(text);
    CHECK(r.loaded == 0);
    CHECK(s.count() == 0);
    REQUIRE(r.problems.size() == 1);
    CHECK(r.problems[0].find("64 KB") != std::string::npos);
}

TEST_CASE("view store: an unknown format version ignores the whole file") {
    ViewStore s;
    mykam::ViewParseResult r = s.parse("format=3\n" + viewText(1));
    CHECK(r.loaded == 0);
    CHECK(r.problems.size() == 1);
    r = s.parse("format=abc\n" + viewText(1));
    CHECK(r.loaded == 0);
    CHECK(r.problems.size() == 1);
    r = s.parse("mykam_slots=2\n" + slotText(1));  // the old header with another version
    CHECK(r.loaded == 0);
    CHECK(r.problems.size() == 1);
}

TEST_CASE("view store: CRLF line endings and blank padding are accepted") {
    std::string text = "format=2\r\n";
    const std::string body = viewText(2);
    for (char c : body) {
        if (c == '\n') {
            text += "\r\n";
        } else {
            text += c;
        }
    }
    ViewStore s;
    const mykam::ViewParseResult r = s.parse(text);
    CHECK(r.loaded == 1);
    CHECK(s.find(2) != nullptr);
}

TEST_CASE("view store: old slot keys inside a format 2 file are not views") {
    ViewStore s;
    const mykam::ViewParseResult r = s.parse(kHeader + slotText(1) + viewText(2));
    CHECK(r.loaded == 1);
    CHECK_FALSE(r.problems.empty());
    CHECK(s.find(2) != nullptr);
}

// ---------------------------------------------------------------- migration of old slot files

TEST_CASE("migration: old slots become views with hotkey N, keeping kind, name, favourite and view command") {
    const std::string old = "mykam_slots=1\n" + slotText(1, "1") + "slot1.name=Left wing\nslot1.favourite=1\n" +
                            "slot1.view=sim/view/chase\n" + slotText(2, "2") + "slot2.kind=cockpit\nslot2.name=Panel\n" +
                            slotText(3, "3") +  // a legacy slot with no kind and no name
                            slotText(5, "5") + "slot5.kind=cockpit\n" + slotText(9, "9") + "slot9.view=sim/view/free_camera\n";
    ViewStore s;
    const mykam::ViewParseResult r = s.parse(old);
    CHECK(r.loaded == 5);
    CHECK(r.migrated);
    CHECK(r.problems.empty());
    REQUIRE(s.count() == 5);

    const mykam::SavedView* v1 = s.find(1);
    REQUIRE(v1 != nullptr);
    CHECK(v1->hotkey == 1);
    CHECK(v1->kind == ViewKind::External);
    CHECK(v1->name == "Left wing");
    CHECK(v1->favourite);
    CHECK(v1->viewCommand == "sim/view/chase");
    CHECK(v1->pose.x == 1.0);
    CHECK(v1->pose.y == 2.0);
    CHECK(v1->pose.z == 3.0);
    CHECK(v1->pose.pitch == 4.0);
    CHECK(v1->pose.heading == 5.0);
    CHECK(v1->pose.roll == 6.0);
    CHECK(v1->pose.zoom == 1.0);

    const mykam::SavedView* v2 = s.find(2);
    REQUIRE(v2 != nullptr);
    CHECK(v2->hotkey == 2);
    CHECK(v2->kind == ViewKind::Cockpit);
    CHECK(v2->name == "Panel");
    CHECK_FALSE(v2->favourite);
    CHECK(v2->viewCommand.empty());

    const mykam::SavedView* v3 = s.find(3);
    REQUIRE(v3 != nullptr);
    CHECK(v3->hotkey == 3);
    CHECK(v3->kind == ViewKind::External);  // a legacy slot without kind is external
    CHECK(v3->name == "Outside view 1");    // no name: a default one
    CHECK_FALSE(v3->favourite);
    CHECK(v3->viewCommand.empty());

    const mykam::SavedView* v5 = s.find(5);
    REQUIRE(v5 != nullptr);
    CHECK(v5->hotkey == 5);
    CHECK(v5->kind == ViewKind::Cockpit);
    CHECK(v5->name == "Cockpit view 1");

    const mykam::SavedView* v9 = s.find(9);
    REQUIRE(v9 != nullptr);
    CHECK(v9->hotkey == 9);
    CHECK(v9->name == "Outside view 2");
    CHECK(v9->viewCommand == "sim/view/free_camera");

    // The hotkey lookup finds them by their old slot number, and ids are never reused.
    CHECK(s.get(3) != nullptr);
    CHECK(s.get(4) == nullptr);
    CHECK(s.nextId() == 10);

    // Migrating writes nothing by itself; the next serialize is the new format.
    const std::string text = s.serialize();
    CHECK(text.rfind("format=2\n", 0) == 0);
    CHECK(text.find("slot") == std::string::npos);
    ViewStore again;
    const mykam::ViewParseResult r2 = again.parse(text);
    CHECK_FALSE(r2.migrated);
    CHECK(r2.loaded == 5);
    CHECK(again.serialize() == text);
}

TEST_CASE("migration: a legacy file without the header line is migrated, a broken slot is skipped") {
    ViewStore s;
    const mykam::ViewParseResult r = s.parse(slotText(1) + slotText(2, "abc") + slotText(4) + "slot4.view=nonsense\n" +
                                             "slot10.x=1\n" + "garbage\n");
    CHECK(r.migrated);
    CHECK(r.loaded == 2);
    CHECK(s.find(1) != nullptr);
    CHECK(s.find(2) == nullptr);
    CHECK(s.find(4) != nullptr);
    CHECK(s.find(4)->viewCommand.empty());
    CHECK(r.problems.size() == 4);  // slot 2, bad view on slot 4, slot10.x and the garbage line
}

TEST_CASE("migration: a cockpit slot never keeps a stored view command") {
    ViewStore s;
    s.parse("mykam_slots=1\n" + slotText(3) + "slot3.kind=cockpit\nslot3.view=sim/view/chase\n");
    REQUIRE(s.find(3) != nullptr);
    CHECK(s.find(3)->kind == ViewKind::Cockpit);
    CHECK(s.find(3)->viewCommand.empty());
}

TEST_CASE("migration: an old file with no slots is not reported as migrated") {
    ViewStore s;
    mykam::ViewParseResult r = s.parse("mykam_slots=1\n");
    CHECK_FALSE(r.migrated);
    CHECK(r.loaded == 0);
    r = s.parse("# only a comment\n");
    CHECK_FALSE(r.migrated);
}

TEST_CASE("migration: names, favourites and kinds survive migration and a second round trip") {
    ViewStore s;
    s.parse("mykam_slots=1\n" + slotText(7) + "slot7.name=Ahead\nslot7.favourite=1\nslot7.kind=cockpit\n");
    ViewStore back;
    back.parse(s.serialize());
    REQUIRE(back.find(7) != nullptr);
    CHECK(back.find(7)->name == "Ahead");
    CHECK(back.find(7)->favourite);
    CHECK(back.find(7)->kind == ViewKind::Cockpit);
    CHECK(back.find(7)->hotkey == 7);
}

// ---------------------------------------------------------------- stored view command

TEST_CASE("view command: it round trips and only valid names are stored") {
    ViewStore a;
    REQUIRE(a.add(somePose(), ViewKind::External, "sim/view/chase") == 1);
    REQUIRE(a.add(somePose(), ViewKind::External) == 2);
    REQUIRE(a.add(somePose(), ViewKind::External, "not a command") == 3);
    const std::string text = a.serialize();
    CHECK(text.find("view1.view=sim/view/chase\n") != std::string::npos);
    CHECK(text.find("view2.view") == std::string::npos);
    CHECK(text.find("view3.view") == std::string::npos);

    ViewStore b;
    b.parse(text);
    CHECK(b.find(1)->viewCommand == "sim/view/chase");
    CHECK(b.find(2)->viewCommand.empty());
    CHECK(b.serialize() == text);
}

TEST_CASE("view command: an invalid view field is ignored and the view still loads") {
    const std::string tooLong = "sim/view/" + std::string(60, 'a');  // 69 characters
    REQUIRE(tooLong.size() > mykam::kMaxViewCommandNameLength);
    const std::string bad[] = {"chase",
                               "sim/general/left",
                               "sim/view/",
                               "",
                               "sim/view/chase;rm",
                               "sim/view/ch ase",
                               "sim/view/../x.y",
                               "sim/view/track-ir_toggle",
                               "/sim/view/chase",
                               "SIM/view/chase",
                               tooLong};
    for (const std::string& value : bad) {
        ViewStore s;
        INFO(value);
        const mykam::ViewParseResult r = s.parse(kHeader + viewText(3) + "view3.view=" + value + "\n");
        CHECK(r.loaded == 1);
        REQUIRE(s.find(3) != nullptr);
        CHECK(s.find(3)->viewCommand.empty());
        CHECK(r.problems.size() == 1);
        CHECK(s.serialize().find(".view=") == std::string::npos);
    }
}

TEST_CASE("view command: accepted names are letters, digits, underscore and slash, up to 64 characters") {
    CHECK(mykam::isValidViewCommandName("sim/view/chase"));
    CHECK(mykam::isValidViewCommandName("sim/view/3d_cockpit_cmnd_look"));
    CHECK(mykam::isValidViewCommandName("sim/view/Some/Sub_Name9"));
    CHECK(mykam::isValidViewCommandName("sim/view/" + std::string(55, 'a')));        // exactly 64
    CHECK_FALSE(mykam::isValidViewCommandName("sim/view/" + std::string(56, 'a')));  // 65
    CHECK_FALSE(mykam::isValidViewCommandName("sim/view/a-b"));
    CHECK_FALSE(mykam::isValidViewCommandName("sim/view/a\tb"));
    CHECK_FALSE(mykam::isValidViewCommandName(std::string("sim/view/a") + '\0' + "b"));
    CHECK_FALSE(mykam::isValidViewCommandName(""));
}

TEST_CASE("view command: a cockpit view never keeps one, from add, update or a file") {
    ViewStore a;
    REQUIRE(a.add(somePose(), ViewKind::Cockpit, "sim/view/chase") == 1);
    CHECK(a.find(1)->viewCommand.empty());
    CHECK(a.serialize().find(".view=") == std::string::npos);

    ViewStore b;
    const mykam::ViewParseResult r = b.parse(kHeader + viewText(3) + "view3.kind=cockpit\nview3.view=sim/view/chase\n");
    CHECK(r.loaded == 1);
    CHECK(b.find(3)->kind == ViewKind::Cockpit);
    CHECK(b.find(3)->viewCommand.empty());
}

TEST_CASE("view command: the tracked list holds only well-formed, unique names and the hand-off targets are outside views") {
    for (std::size_t i = 0; i < mykam::kViewCommandCount; ++i) {
        INFO(mykam::kViewCommands[i].name);
        CHECK(mykam::isValidViewCommandName(mykam::kViewCommands[i].name));
        for (std::size_t j = i + 1; j < mykam::kViewCommandCount; ++j) {
            CHECK(std::string(mykam::kViewCommands[i].name) != mykam::kViewCommands[j].name);
        }
    }
    CHECK(mykam::isHandoffViewCommand("sim/view/free_camera"));
    CHECK(mykam::isHandoffViewCommand("sim/view/chase"));
    CHECK_FALSE(mykam::isHandoffViewCommand("sim/view/default_view"));
    CHECK_FALSE(mykam::isHandoffViewCommand("sim/view/quick_look_1"));
    CHECK_FALSE(mykam::isHandoffViewCommand(""));
}

TEST_CASE("view type label: Cockpit, Chase or Free camera") {
    ViewStore s;
    s.add(somePose(), ViewKind::Cockpit);
    s.add(somePose(), ViewKind::External, "sim/view/chase");
    s.add(somePose(), ViewKind::External, "sim/view/free_camera");
    s.add(somePose(), ViewKind::External);
    CHECK(std::string(mykam::viewTypeLabel(*s.find(1))) == "Cockpit");
    CHECK(std::string(mykam::viewTypeLabel(*s.find(2))) == "Chase");
    CHECK(std::string(mykam::viewTypeLabel(*s.find(3))) == "Free camera");
    CHECK(std::string(mykam::viewTypeLabel(*s.find(4))) == "Free camera");
}

// ---------------------------------------------------------------- file names

TEST_CASE("file names: ordinary aircraft names keep their letters (NFR2.7)") {
    CHECK(mykam::sanitizeAircraftName("Cessna_172SP.acf") == "Cessna_172SP");
    CHECK(mykam::sanitizeAircraftName("B738-800.ACF") == "B738-800");
    CHECK(mykam::slotFileName("Cessna_172SP.acf") == "Cessna_172SP.slots.txt");
}

TEST_CASE("file names: slashes, dots and colons can never leave the folder (NFR2.7)") {
    const char* hostile[] = {"../../evil.acf", "..\\..\\evil.acf", "C:\\Windows\\x.acf", "a/b\\c:d*e?f.acf",
                             "....", "/etc/passwd", "name with spaces.acf", "caf\xC3\xA9.acf"};
    for (const char* name : hostile) {
        const std::string out = mykam::slotFileName(name);
        INFO("input " << name << " output " << out);
        CHECK(out.find('/') == std::string::npos);
        CHECK(out.find('\\') == std::string::npos);
        CHECK(out.find(':') == std::string::npos);
        CHECK(out.find(' ') == std::string::npos);
        CHECK(out[0] != '.');
        for (char c : out) {
            const unsigned char u = static_cast<unsigned char>(c);
            CHECK(u < 128);
        }
    }
    CHECK(mykam::sanitizeAircraftName("a/b\\c:d") == "a_b_c_d");
}

TEST_CASE("file names: empty names and Windows device names are made safe") {
    CHECK(mykam::sanitizeAircraftName("") == "unnamed");
    CHECK(mykam::sanitizeAircraftName(".acf") == "unnamed");
    CHECK(mykam::sanitizeAircraftName("CON.acf") == "_CON");
    CHECK(mykam::sanitizeAircraftName("nul") == "_nul");
    CHECK(mykam::sanitizeAircraftName("Prn.acf") == "_Prn");
    CHECK(mykam::sanitizeAircraftName("AUX") == "_AUX");
    CHECK(mykam::sanitizeAircraftName("COM1.acf") == "_COM1");
    CHECK(mykam::sanitizeAircraftName("LPT9") == "_LPT9");
    CHECK(mykam::sanitizeAircraftName("CON.foo.acf") == "_CON.foo");
    // Names that only look similar stay as they are.
    CHECK(mykam::sanitizeAircraftName("COM0") == "COM0");
    CHECK(mykam::sanitizeAircraftName("Console") == "Console");
    CHECK(mykam::sanitizeAircraftName("COM10") == "COM10");
}

TEST_CASE("file names: an over-long name is capped") {
    const std::string longName = std::string(300, 'a') + ".acf";
    const std::string stem = mykam::sanitizeAircraftName(longName);
    CHECK(stem.size() == 64);
    CHECK(mykam::slotFileName(longName).size() == 64 + std::string(".slots.txt").size());
}
