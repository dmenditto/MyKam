#include "doctest.h"

#include <string>
#include <vector>

#include "controller.h"
#include "fake_sim.h"
#include "slot_store.h"
#include "ui_model.h"

// Per-view label, custom key and order; the quick bar as the starred views; the edit dialog model.

using mykam::CustomKey;
using mykam::EditDraft;
using mykam::EditResult;
using mykam::KeyMode;
using mykam::Pose;
using mykam::UiModel;
using mykam::ViewGroup;
using mykam::ViewKind;
using mykam::ViewStore;
using mykam::WantedKey;

namespace {

Pose somePose(double x = 1.0) {
    Pose p;
    p.x = x;
    p.y = 2.0;
    p.z = 3.0;
    p.pitch = 4.0;
    p.heading = 5.0;
    p.roll = 6.0;
    p.zoom = 1.0;
    return p;
}

// A complete new-format view as text; `extra` is appended (more viewN.field lines).
std::string viewText(int id, const std::string& extra = std::string(), const std::string& kind = "external") {
    const std::string p = "view" + std::to_string(id) + ".";
    return p + "x=1\n" + p + "y=2\n" + p + "z=3\n" + p + "pitch=4\n" + p + "heading=5\n" + p + "roll=6\n" + p +
           "zoom=1\n" + p + "kind=" + kind + "\n" + extra;
}

CustomKey key(int vkey, int flags = 0) {
    CustomKey k;
    k.vkey = vkey;
    k.flags = flags;
    return k;
}

constexpr int kW = 87;  // XPLM_VK_W
constexpr int kQ = 81;  // XPLM_VK_Q

const char* const kSlotFile = "plug/cameras/Cessna_172SP.slots.txt";
const char* const kOtherFile = "plug/cameras/Piper.slots.txt";

struct Rig {
    FakeSim sim;
    mykam::Controller controller;
    Rig() : controller(sim, "plug") {
        sim.files["plug/mykam.ini"] = "transition_seconds = 0\n";
        controller.start();
        controller.onAircraftLoaded("Cessna_172SP.acf");
        sim.logs.clear();
    }
    void inFree(double x = 1.0) {
        sim.viewExternal = true;
        sim.view = somePose(x);
        controller.release();
        controller.onViewSelected("sim/view/free_camera");
    }
    void inCockpit(double x = 0.5) {
        sim.viewExternal = false;
        sim.head = somePose(x);
        controller.release();
    }
    int newFree(double x = 1.0) {
        inFree(x);
        return controller.saveCurrentAsNew();
    }
    int newCockpit(double x = 0.5) {
        inCockpit(x);
        return controller.saveCurrentAsNew();
    }
    const mykam::SavedView& view(int id) {
        const mykam::SavedView* v = controller.slots().find(id);
        REQUIRE(v != nullptr);
        return *v;
    }
    ViewStore fileStore(const char* path = kSlotFile) {
        ViewStore s;
        REQUIRE(sim.files.count(path) == 1);
        s.parse(sim.files[path]);
        return s;
    }
};

}  // namespace

// ---------------------------------------------------------------- label

TEST_CASE("label: 0 to 2 letters or digits, stored uppercase") {
    std::string out;
    CHECK(mykam::checkLabel("a", out) == mykam::LabelCheck::Ok);
    CHECK(out == "A");
    CHECK(mykam::checkLabel("b7", out) == mykam::LabelCheck::Ok);
    CHECK(out == "B7");
    CHECK(mykam::checkLabel("zz", out) == mykam::LabelCheck::Ok);
    CHECK(out == "ZZ");
    CHECK(mykam::checkLabel("5", out) == mykam::LabelCheck::Ok);
    CHECK(out == "5");
    CHECK(mykam::checkLabel("  q ", out) == mykam::LabelCheck::Ok);  // blanks around are ignored
    CHECK(out == "Q");
    CHECK(mykam::checkLabel("", out) == mykam::LabelCheck::Ok);  // none
    CHECK(out.empty());
    CHECK(mykam::checkLabel("abc", out) == mykam::LabelCheck::TooLong);
    CHECK(mykam::checkLabel("a!", out) == mykam::LabelCheck::BadCharacter);
    CHECK(mykam::checkLabel("a b", out) == mykam::LabelCheck::TooLong);  // three characters
    CHECK(mykam::checkLabel("\t", out) == mykam::LabelCheck::Ok);        // blanks only: no label
    CHECK(out.empty());
    CHECK(mykam::checkLabel("\xC3\xA9", out) == mykam::LabelCheck::BadCharacter);  // two bytes, not letters A-Z
    CHECK(mykam::checkLabel("\xC3", out) == mykam::LabelCheck::BadCharacter);
    CHECK(mykam::checkLabel("-", out) == mykam::LabelCheck::BadCharacter);
}

TEST_CASE("label: the store keeps it uppercase, clears it, refuses bad ones, and the file round trips it") {
    ViewStore s;
    const int a = s.add(somePose(), ViewKind::External);
    CHECK(s.find(a)->label.empty());  // default: none
    CHECK(s.setLabel(a, "k9"));
    CHECK(s.find(a)->label == "K9");
    CHECK_FALSE(s.setLabel(a, "toolong"));
    CHECK_FALSE(s.setLabel(a, "#"));
    CHECK_FALSE(s.setLabel(77, "A"));
    CHECK(s.find(a)->label == "K9");
    CHECK(s.serialize().find("view1.label=K9\n") != std::string::npos);

    ViewStore back;
    const mykam::ViewParseResult r = back.parse(s.serialize());
    CHECK(r.problems.empty());
    CHECK(back.find(a)->label == "K9");
    CHECK(back.serialize() == s.serialize());

    CHECK(s.setLabel(a, ""));
    CHECK(s.find(a)->label.empty());
    CHECK(s.serialize().find(".label") == std::string::npos);  // nothing written for no label
}

TEST_CASE("label: a bad label in a file is ignored with a problem, a lowercase one is stored uppercase") {
    ViewStore s;
    const mykam::ViewParseResult r = s.parse("format=2\n" + viewText(1, "view1.label=abc\n") +
                                             viewText(2, "view2.label=x#\n") + viewText(3, "view3.label=ab\n"));
    CHECK(r.loaded == 3);
    CHECK(r.problems.size() == 2);
    CHECK(s.find(1)->label.empty());
    CHECK(s.find(2)->label.empty());
    CHECK(s.find(3)->label == "AB");
}

// ---------------------------------------------------------------- custom key

TEST_CASE("key: encode writes vk and flag words in a fixed order") {
    CHECK(mykam::encodeKey(key(kW, mykam::kKeyShift | mykam::kKeyCtrl)) == "vk:87,flags:shift+ctrl");
    CHECK(mykam::encodeKey(key(kW, mykam::kKeyShift | mykam::kKeyCtrl | mykam::kKeyAlt)) == "vk:87,flags:shift+ctrl+alt");
    CHECK(mykam::encodeKey(key(kQ, mykam::kKeyAlt)) == "vk:81,flags:alt");
    CHECK(mykam::encodeKey(key(112)) == "vk:112,flags:none");
}

TEST_CASE("key: decode reads what encode writes, and common spellings") {
    CustomKey k;
    REQUIRE(mykam::decodeKey("vk:87,flags:shift+ctrl", k));
    CHECK(k == key(kW, mykam::kKeyShift | mykam::kKeyCtrl));
    REQUIRE(mykam::decodeKey("vk:87,flags:control+SHIFT", k));
    CHECK(k == key(kW, mykam::kKeyShift | mykam::kKeyCtrl));
    REQUIRE(mykam::decodeKey("vk:81,flags:option", k));
    CHECK(k == key(kQ, mykam::kKeyAlt));
    REQUIRE(mykam::decodeKey("  vk: 112 , flags: none ", k));
    CHECK(k == key(112));
    REQUIRE(mykam::decodeKey("vk:112", k));  // flags are optional
    CHECK(k == key(112));
    const CustomKey all = key(200, mykam::kKeyShift | mykam::kKeyCtrl | mykam::kKeyAlt);
    REQUIRE(mykam::decodeKey(mykam::encodeKey(all), k));
    CHECK(k == all);
}

TEST_CASE("key: invalid text and invalid keys are refused and leave the target alone") {
    const std::string bad[] = {"",           "87",           "vk:",          "vk:0",          "vk:256",
                               "vk:-1",      "vk:abc",       "vk:27",        "vk:8",          "vk:16",
                               "vk:17",      "vk:18",        "vk:87,shift",  "vk:87,flags:hyper",
                               "vk:87,flags:shift+", "vk:87,flags:shift++ctrl", "vk:87,flags:", "vk:1e2"};
    for (const std::string& text : bad) {
        INFO("text '" << text << "'");
        CustomKey k = key(5, 1);
        CHECK_FALSE(mykam::decodeKey(text, k));
        CHECK(k == key(5, 1));
    }
    CHECK_FALSE(mykam::isValidKey(key(0)));
    CHECK_FALSE(mykam::isValidKey(key(256)));
    CHECK_FALSE(mykam::isValidKey(key(27)));       // Escape cancels the capture field
    CHECK_FALSE(mykam::isValidKey(key(8)));        // Backspace clears it
    CHECK_FALSE(mykam::isValidKey(key(kW, 8)));    // unknown flag bit
    CHECK_FALSE(mykam::isValidKey(key(kW, -1)));
    CHECK(mykam::isValidKey(key(kW)));
    CHECK(mykam::isValidKey(key(255, 7)));
}

TEST_CASE("key: unique per aircraft, assigning a used combination moves it; the file round trips it") {
    ViewStore s;
    const int a = s.add(somePose(), ViewKind::External);
    const int b = s.add(somePose(), ViewKind::Cockpit);
    const CustomKey k = key(kW, mykam::kKeyShift | mykam::kKeyCtrl);
    CHECK(s.keyOwner(k) == 0);
    CHECK(s.setKey(a, k));
    CHECK(s.keyOwner(k) == a);
    CHECK(s.setKey(b, k));  // moves from a to b
    CHECK(s.keyOwner(k) == b);
    CHECK_FALSE(s.find(a)->key.set());
    CHECK(s.setKey(b, k));  // the same key again changes nothing
    CHECK(s.keyOwner(k) == b);
    CHECK(s.setKey(a, key(kW)));  // another modifier set is another combination
    CHECK(s.keyOwner(key(kW)) == a);
    CHECK(s.keyOwner(k) == b);

    CHECK_FALSE(s.setKey(a, key(27)));
    CHECK_FALSE(s.setKey(99, k));
    CHECK(s.keyOwner(key(0)) == 0);
    CHECK(s.find(a)->key == key(kW));

    const std::string text = s.serialize();
    CHECK(text.find("view2.key=vk:87,flags:shift+ctrl\n") != std::string::npos);
    CHECK(text.find("view1.key=vk:87,flags:none\n") != std::string::npos);
    ViewStore back;
    CHECK(back.parse(text).problems.empty());
    CHECK(back.find(b)->key == k);
    CHECK(back.serialize() == text);

    CHECK(s.setKey(b, key(0)));  // clear
    CHECK_FALSE(s.find(b)->key.set());
    CHECK(s.keyOwner(k) == 0);
}

TEST_CASE("key: the numpad hotkey stays and is independent of the custom key") {
    ViewStore s;
    const int a = s.add(somePose(), ViewKind::External, "", 3);
    CHECK(s.setKey(a, key(kW)));
    CHECK(s.find(a)->hotkey == 3);
    CHECK(s.hotkeyOwner(3) == a);
    CHECK(s.keyOwner(key(kW)) == a);
}

TEST_CASE("key: a bad or duplicate key in a file is ignored with a problem and the view still loads") {
    ViewStore s;
    const mykam::ViewParseResult r = s.parse("format=2\n" + viewText(1, "view1.key=vk:87,flags:shift\n") +
                                             viewText(2, "view2.key=vk:87,flags:shift\n") +  // same combination
                                             viewText(3, "view3.key=nonsense\n") +
                                             viewText(4, "view4.key=vk:87,flags:ctrl\n"));
    CHECK(r.loaded == 4);
    CHECK(r.problems.size() == 2);
    CHECK(s.find(1)->key == key(kW, mykam::kKeyShift));
    CHECK_FALSE(s.find(2)->key.set());
    CHECK_FALSE(s.find(3)->key.set());
    CHECK(s.find(4)->key == key(kW, mykam::kKeyCtrl));
}

// ---------------------------------------------------------------- order

TEST_CASE("order: new views go to the end of their group; groups number separately") {
    ViewStore s;
    const int i1 = s.add(somePose(), ViewKind::Cockpit);
    const int e1 = s.add(somePose(), ViewKind::External);
    const int i2 = s.add(somePose(), ViewKind::Cockpit);
    const int e2 = s.add(somePose(), ViewKind::External);
    CHECK(s.find(i1)->order == 1);
    CHECK(s.find(i2)->order == 2);
    CHECK(s.find(e1)->order == 1);
    CHECK(s.find(e2)->order == 2);
    CHECK(s.ordered(ViewGroup::Internal) == std::vector<int>({i1, i2}));
    CHECK(s.ordered(ViewGroup::External) == std::vector<int>({e1, e2}));
    CHECK(s.remove(e1));
    const int e3 = s.add(somePose(), ViewKind::External);
    CHECK(s.find(e3)->order == 3);  // after the highest, whatever was deleted
    CHECK(s.ordered(ViewGroup::External) == std::vector<int>({e2, e3}));
}

TEST_CASE("order: moveTo puts a view in the place of another and renumbers the group 1..n") {
    ViewStore s;
    const int a = s.add(somePose(), ViewKind::External);
    const int b = s.add(somePose(), ViewKind::External);
    const int c = s.add(somePose(), ViewKind::External);
    const int d = s.add(somePose(), ViewKind::External);
    const int cockpit = s.add(somePose(), ViewKind::Cockpit);

    CHECK(s.moveTo(d, a));  // to the front
    CHECK(s.ordered(ViewGroup::External) == std::vector<int>({d, a, b, c}));
    CHECK(s.find(d)->order == 1);
    CHECK(s.find(c)->order == 4);
    CHECK(s.moveTo(d, c));  // to the back
    CHECK(s.ordered(ViewGroup::External) == std::vector<int>({a, b, c, d}));
    CHECK(s.moveTo(a, c));  // forwards, jumping over b
    CHECK(s.ordered(ViewGroup::External) == std::vector<int>({b, c, a, d}));
    CHECK(s.moveTo(a, b));  // backwards
    CHECK(s.ordered(ViewGroup::External) == std::vector<int>({a, b, c, d}));

    CHECK_FALSE(s.moveTo(a, a));
    CHECK_FALSE(s.moveTo(a, cockpit));  // other group
    CHECK_FALSE(s.moveTo(a, 99));
    CHECK_FALSE(s.moveTo(99, a));
    CHECK(s.ordered(ViewGroup::External) == std::vector<int>({a, b, c, d}));
    CHECK(s.ordered(ViewGroup::Internal) == std::vector<int>({cockpit}));

    // Gaps left by a delete are closed by the next move.
    CHECK(s.remove(b));
    CHECK(s.moveTo(d, a));
    CHECK(s.find(d)->order == 1);
    CHECK(s.find(a)->order == 2);
    CHECK(s.find(c)->order == 3);
}

TEST_CASE("order: neighbours, with and without skipping views that are not starred") {
    ViewStore s;
    const int a = s.add(somePose(), ViewKind::External);
    const int b = s.add(somePose(), ViewKind::External);
    const int c = s.add(somePose(), ViewKind::External);
    const int d = s.add(somePose(), ViewKind::External);
    s.setFavourite(a, true);
    s.setFavourite(c, true);
    s.setFavourite(d, true);
    CHECK(s.neighbour(a, -1, false) == 0);
    CHECK(s.neighbour(a, 1, false) == b);
    CHECK(s.neighbour(a, 1, true) == c);   // b is not starred
    CHECK(s.neighbour(c, -1, true) == a);
    CHECK(s.neighbour(d, 1, true) == 0);
    CHECK(s.neighbour(d, 1, false) == 0);
    CHECK(s.neighbour(99, 1, false) == 0);
    CHECK(s.neighbour(b, 0, false) == 0);
    CHECK(s.favourites(ViewGroup::External) == std::vector<int>({a, c, d}));
}

TEST_CASE("order: changing the group of a view (update) sends it to the end of the new group") {
    ViewStore s;
    const int e1 = s.add(somePose(), ViewKind::External);
    const int e2 = s.add(somePose(), ViewKind::External);
    const int c1 = s.add(somePose(), ViewKind::Cockpit);
    REQUIRE(s.update(e1, somePose(), ViewKind::Cockpit));
    CHECK(s.ordered(ViewGroup::Internal) == std::vector<int>({c1, e1}));
    CHECK(s.ordered(ViewGroup::External) == std::vector<int>({e2}));
    REQUIRE(s.update(c1, somePose(9.0), ViewKind::Cockpit));  // same group: the place stays
    CHECK(s.ordered(ViewGroup::Internal) == std::vector<int>({c1, e1}));
}

TEST_CASE("order: the order is written, read back and used to sort the groups") {
    ViewStore a;
    const int v1 = a.add(somePose(), ViewKind::External);
    const int v2 = a.add(somePose(), ViewKind::External);
    const int v3 = a.add(somePose(), ViewKind::External);
    REQUIRE(a.moveTo(v3, v1));
    const std::string text = a.serialize();
    CHECK(text.find("view3.order=1\n") != std::string::npos);
    CHECK(text.find("view1.order=2\n") != std::string::npos);
    ViewStore b;
    CHECK(b.parse(text).problems.empty());
    CHECK(b.ordered(ViewGroup::External) == std::vector<int>({v3, v1, v2}));
    CHECK(b.serialize() == text);

    // Explicit orders in a hand-written file, out of id order and with gaps, are honoured and renumbered.
    ViewStore c;
    c.parse("format=2\n" + viewText(1, "view1.order=30\n") + viewText(2, "view2.order=5\n") +
            viewText(3, "view3.order=10\n"));
    CHECK(c.ordered(ViewGroup::External) == std::vector<int>({2, 3, 1}));
    CHECK(c.find(2)->order == 1);
    CHECK(c.find(3)->order == 2);
    CHECK(c.find(1)->order == 3);
    // Equal orders keep id order.
    c.parse("format=2\n" + viewText(1, "view1.order=2\n") + viewText(2, "view2.order=2\n") + viewText(3, "view3.order=1\n"));
    CHECK(c.ordered(ViewGroup::External) == std::vector<int>({3, 1, 2}));
}

TEST_CASE("order: a bad order line is ignored with a problem and the view gets the next free place") {
    ViewStore s;
    const mykam::ViewParseResult r = s.parse("format=2\n" + viewText(1, "view1.order=2\n") +
                                             viewText(2, "view2.order=zero\n") + viewText(3, "view3.order=0\n") +
                                             viewText(4, "view4.order=1\n"));
    CHECK(r.loaded == 4);
    CHECK(r.problems.size() == 2);  // zero and 0
    CHECK(s.ordered(ViewGroup::External) == std::vector<int>({4, 1, 2, 3}));
}

TEST_CASE("migration: views without an order line get their place from the old display order (favourites first)") {
    ViewStore s;
    // Old behaviour: external favourites first (view 3, then view 2... by creation), then the rest.
    const mykam::ViewParseResult r =
        s.parse("format=2\n" + viewText(1) + viewText(2, "view2.favourite=1\n") + viewText(3) +
                viewText(4, "view4.favourite=1\n") + viewText(5, "", "cockpit") +
                viewText(6, "view6.favourite=1\n", "cockpit") + viewText(7, "", "cockpit"));
    CHECK(r.loaded == 7);
    CHECK(r.problems.empty());
    CHECK(s.ordered(ViewGroup::External) == std::vector<int>({2, 4, 1, 3}));
    CHECK(s.ordered(ViewGroup::Internal) == std::vector<int>({6, 5, 7}));
    CHECK(s.find(2)->order == 1);
    CHECK(s.find(3)->order == 4);
    CHECK(s.find(6)->order == 1);

    // Writing it down makes the order permanent.
    ViewStore back;
    back.parse(s.serialize());
    CHECK(back.ordered(ViewGroup::External) == std::vector<int>({2, 4, 1, 3}));
    CHECK(back.ordered(ViewGroup::Internal) == std::vector<int>({6, 5, 7}));
}

TEST_CASE("migration: an old slot file also gets the old display order") {
    const std::string slot1 = "slot1.x=1\nslot1.y=2\nslot1.z=3\nslot1.pitch=4\nslot1.heading=5\nslot1.roll=6\nslot1.zoom=1\n";
    const std::string slot2 = "slot2.x=1\nslot2.y=2\nslot2.z=3\nslot2.pitch=4\nslot2.heading=5\nslot2.roll=6\nslot2.zoom=1\n";
    const std::string slot3 = "slot3.x=1\nslot3.y=2\nslot3.z=3\nslot3.pitch=4\nslot3.heading=5\nslot3.roll=6\nslot3.zoom=1\n";
    ViewStore s;
    const mykam::ViewParseResult r =
        s.parse("mykam_slots=1\n" + slot1 + slot2 + "slot2.favourite=1\n" + slot3);
    CHECK(r.migrated);
    CHECK(s.ordered(ViewGroup::External) == std::vector<int>({2, 1, 3}));
    CHECK(s.find(2)->label.empty());
    CHECK_FALSE(s.find(1)->key.set());
}

TEST_CASE("migration: a view with an order and views without mix: the ones without go after the highest") {
    ViewStore s;
    s.parse("format=2\n" + viewText(1) + viewText(2, "view2.order=1\n") + viewText(3, "view3.favourite=1\n"));
    CHECK(s.ordered(ViewGroup::External) == std::vector<int>({2, 3, 1}));  // 3 is starred, so before 1
}

TEST_CASE("migration: old files without label, key or order load unchanged") {
    ViewStore s;
    const mykam::ViewParseResult r = s.parse("format=2\nnext_id=3\n" + viewText(1, "view1.name=Wing\nview1.hotkey=2\n") +
                                             viewText(2, "view2.name=Tail\nview2.favourite=1\n"));
    CHECK(r.loaded == 2);
    CHECK(r.problems.empty());
    CHECK(s.find(1)->name == "Wing");
    CHECK(s.find(1)->hotkey == 2);
    CHECK(s.find(1)->label.empty());
    CHECK_FALSE(s.find(1)->key.set());
    CHECK(s.find(2)->favourite);
    CHECK(s.nextId() == 3);
}

// ---------------------------------------------------------------- wanted custom keys

TEST_CASE("wanted keys: one per view that has a key, described as 'MyKam: <name>'") {
    ViewStore s;
    const int a = s.add(somePose(), ViewKind::External, "", 0, "Wing");
    const int b = s.add(somePose(), ViewKind::Cockpit, "", 0, "Panel");
    s.add(somePose(), ViewKind::External);  // no key
    s.setKey(a, key(kW, mykam::kKeyShift));
    s.setKey(b, key(kQ));
    const std::vector<WantedKey> wanted = s.wantedKeys();
    REQUIRE(wanted.size() == 2);
    CHECK(wanted[0].viewId == a);
    CHECK(wanted[0].key == key(kW, mykam::kKeyShift));
    CHECK(wanted[0].description == "MyKam: Wing");
    CHECK(wanted[1].viewId == b);
    CHECK(wanted[1].description == "MyKam: Panel");
}

TEST_CASE("wanted keys: the difference lists what to add and what to remove (added, changed, removed)") {
    ViewStore s;
    const int a = s.add(somePose(), ViewKind::External, "", 0, "Wing");
    const int b = s.add(somePose(), ViewKind::External, "", 0, "Tail");
    s.setKey(a, key(kW));
    const std::vector<WantedKey> first = s.wantedKeys();

    // Nothing changed: nothing to do.
    mykam::KeyDiff diff = mykam::diffKeys(first, s.wantedKeys());
    CHECK(diff.add.empty());
    CHECK(diff.remove.empty());

    // From nothing: everything is added (aircraft load, plugin enable).
    diff = mykam::diffKeys({}, first);
    CHECK(diff.add.size() == 1);
    CHECK(diff.remove.empty());

    // Added.
    s.setKey(b, key(kQ, mykam::kKeyCtrl));
    diff = mykam::diffKeys(first, s.wantedKeys());
    REQUIRE(diff.add.size() == 1);
    CHECK(diff.add[0].viewId == b);
    CHECK(diff.remove.empty());

    // Changed: another combination for the same view is one removal and one addition.
    const std::vector<WantedKey> two = s.wantedKeys();
    s.setKey(a, key(kW, mykam::kKeyShift));
    diff = mykam::diffKeys(two, s.wantedKeys());
    REQUIRE(diff.remove.size() == 1);
    REQUIRE(diff.add.size() == 1);
    CHECK(diff.remove[0].key == key(kW));
    CHECK(diff.add[0].key == key(kW, mykam::kKeyShift));
    CHECK(diff.add[0].viewId == a);

    // A rename changes the description, so the key is registered again with the new text.
    const std::vector<WantedKey> before = s.wantedKeys();
    s.setName(a, "Left wing");
    diff = mykam::diffKeys(before, s.wantedKeys());
    CHECK(diff.remove.size() == 1);
    CHECK(diff.add.size() == 1);
    CHECK(diff.add[0].description == "MyKam: Left wing");

    // Removed: delete a view, or clear its key.
    const std::vector<WantedKey> withBoth = s.wantedKeys();
    s.remove(b);
    diff = mykam::diffKeys(withBoth, s.wantedKeys());
    REQUIRE(diff.remove.size() == 1);
    CHECK(diff.remove[0].viewId == b);
    CHECK(diff.add.empty());

    // Another aircraft: its own set replaces the old one.
    ViewStore other;
    const int x = other.add(somePose(), ViewKind::External, "", 0, "Rear");
    other.setKey(x, key(kQ, mykam::kKeyCtrl));
    diff = mykam::diffKeys(withBoth, other.wantedKeys());
    CHECK(diff.remove.size() == 2);
    CHECK(diff.add.size() == 1);
}

TEST_CASE("wanted keys: the controller reports a change when the set changes, never otherwise") {
    Rig rig;
    int calls = 0;
    std::vector<WantedKey> seen;
    rig.controller.setOnKeysChanged([&]() {
        ++calls;
        seen = rig.controller.wantedKeys();
    });

    const int a = rig.newFree();
    CHECK(calls == 0);  // a new view has no key

    CHECK(rig.controller.setViewKey(a, key(kW, mykam::kKeyShift | mykam::kKeyCtrl)));
    CHECK(calls == 1);
    REQUIRE(seen.size() == 1);
    CHECK(seen[0].viewId == a);
    CHECK(seen[0].description == "MyKam: Outside view 1");

    CHECK(rig.controller.setViewKey(a, key(kW, mykam::kKeyShift | mykam::kKeyCtrl)));  // same key
    CHECK(calls == 1);
    CHECK(rig.controller.setFavourite(a, true));  // not about keys
    CHECK(rig.controller.setLabel(a, "w"));
    CHECK(calls == 1);

    CHECK(rig.controller.renameView(a, "Wing") == mykam::RenameResult::Renamed);  // the description changes
    CHECK(calls == 2);
    CHECK(seen[0].description == "MyKam: Wing");

    CHECK(rig.controller.setViewKey(a, key(kQ)));  // changed
    CHECK(calls == 3);
    CHECK(seen[0].key == key(kQ));

    const int b = rig.newCockpit();
    CHECK(rig.controller.setViewKey(b, key(kQ)));  // moves from a to b
    CHECK(calls == 4);
    REQUIRE(seen.size() == 1);
    CHECK(seen[0].viewId == b);
    CHECK_FALSE(rig.view(a).key.set());

    CHECK(rig.controller.deleteView(b));  // delete
    CHECK(calls == 5);
    CHECK(seen.empty());
    CHECK(rig.controller.deleteView(a));  // no key: nothing to report
    CHECK(calls == 5);
}

TEST_CASE("wanted keys: an aircraft change reports the new aircraft's keys and the old ones go away") {
    Rig rig;
    rig.sim.files[kSlotFile] = "format=2\n" + viewText(1, "view1.key=vk:87,flags:shift\nview1.name=Wing\n");
    rig.sim.files[kOtherFile] = "format=2\n" + viewText(1, "view1.key=vk:81,flags:ctrl\nview1.name=Rear\n");
    int calls = 0;
    rig.controller.setOnKeysChanged([&]() { ++calls; });

    rig.controller.onAircraftLoaded("Cessna_172SP.acf");
    CHECK(calls == 1);
    REQUIRE(rig.controller.wantedKeys().size() == 1);
    CHECK(rig.controller.wantedKeys()[0].key == key(kW, mykam::kKeyShift));

    rig.controller.onAircraftLoaded("Piper.acf");
    CHECK(calls == 2);
    REQUIRE(rig.controller.wantedKeys().size() == 1);
    CHECK(rig.controller.wantedKeys()[0].key == key(kQ, mykam::kKeyCtrl));
    CHECK(rig.controller.wantedKeys()[0].description == "MyKam: Rear");

    rig.controller.onAircraftLoaded("Empty.acf");  // no file: no keys
    CHECK(calls == 3);
    CHECK(rig.controller.wantedKeys().empty());
    rig.controller.onAircraftLoaded("Empty2.acf");  // still none: nothing to report
    CHECK(calls == 3);

    // Setting the callback again forgets what was reported, so the plugin gets everything after an enable.
    rig.controller.onAircraftLoaded("Piper.acf");
    CHECK(calls == 4);
    rig.controller.setOnKeysChanged([&]() { ++calls; });
    rig.controller.onAircraftLoaded("Piper.acf");
    CHECK(calls == 5);
    rig.controller.setOnKeysChanged(nullptr);  // disable: no callback, no crash
    rig.controller.onAircraftLoaded("Cessna_172SP.acf");
    CHECK(calls == 5);
}

TEST_CASE("key text: modifiers and X-Plane's name for the key; Num N for a numpad hotkey") {
    Rig rig;
    CHECK(rig.controller.keyText(key(kW, mykam::kKeyShift | mykam::kKeyCtrl)) == "Ctrl+Shift+W");
    CHECK(rig.controller.keyText(key(kW, mykam::kKeyAlt)) == "Alt+W");
    CHECK(rig.controller.keyText(key(kW, mykam::kKeyShift | mykam::kKeyCtrl | mykam::kKeyAlt)) == "Ctrl+Shift+Alt+W");
    CHECK(rig.controller.keyText(key(kW)) == "W");
    CHECK(rig.controller.keyText(key(0)).empty());

    const int a = rig.newFree();
    CHECK(rig.controller.viewKeyText(rig.view(a)).empty());
    rig.controller.setHotkey(a, 4);
    CHECK(rig.controller.viewKeyText(rig.view(a)) == "Num 4");
    rig.controller.setViewKey(a, key(kQ, mykam::kKeyCtrl));
    CHECK(rig.controller.viewKeyText(rig.view(a)) == "Ctrl+Q");  // the custom key wins in the list
    UiModel model(rig.controller);
    CHECK(model.rows(ViewGroup::External)[0].keyText == "Ctrl+Q");
    CHECK(model.rows(ViewGroup::External)[0].customKey);
    CHECK(model.customKeyText(key(kQ, mykam::kKeyCtrl)) == "Ctrl+Q");
}

// ---------------------------------------------------------------- controller: reorder, edit, write count

TEST_CASE("reorder: moves persist at once and the order survives a reload") {
    Rig rig;
    const int a = rig.newFree(1.0);
    const int b = rig.newFree(2.0);
    const int c = rig.newFree(3.0);
    const int writes = rig.sim.writeFileCalls;

    CHECK(rig.controller.moveView(c, a));
    CHECK(rig.sim.writeFileCalls == writes + 1);
    CHECK(rig.fileStore().ordered(ViewGroup::External) == std::vector<int>({c, a, b}));
    CHECK(rig.controller.moveViewStep(c, 1, false));
    CHECK(rig.fileStore().ordered(ViewGroup::External) == std::vector<int>({a, c, b}));
    CHECK_FALSE(rig.controller.moveViewStep(a, -1, false));  // already first: nothing written
    CHECK(rig.sim.writeFileCalls == writes + 2);
    CHECK_FALSE(rig.controller.moveView(a, 99));

    rig.controller.onAircraftLoaded("Cessna_172SP.acf");
    UiModel model(rig.controller);
    CHECK(model.rows(ViewGroup::External)[0].id == a);
    CHECK(model.rows(ViewGroup::External)[1].id == c);
    CHECK(model.rows(ViewGroup::External)[2].id == b);
}

TEST_CASE("edit: Save applies every field with one write; a bad field changes nothing") {
    Rig rig;
    const int a = rig.newFree();
    const int other = rig.newFree(2.0);
    rig.controller.setHotkey(other, 5);
    const int writes = rig.sim.writeFileCalls;

    mykam::ViewEdit edit;
    edit.name = "  Left wing ";
    edit.label = "lw";
    edit.numpad = 5;  // taken from `other`
    edit.favourite = true;
    CHECK(rig.controller.editView(a, edit) == EditResult::Saved);
    CHECK(rig.sim.writeFileCalls == writes + 1);
    CHECK(rig.view(a).name == "Left wing");
    CHECK(rig.view(a).label == "LW");
    CHECK(rig.view(a).hotkey == 5);
    CHECK(rig.view(a).favourite);
    CHECK(rig.view(other).hotkey == 0);

    edit.numpad = 0;
    edit.key = key(kW, mykam::kKeyCtrl);
    CHECK(rig.controller.editView(a, edit) == EditResult::Saved);
    CHECK(rig.view(a).hotkey == 0);
    CHECK(rig.view(a).key == key(kW, mykam::kKeyCtrl));
    CHECK(rig.fileStore().find(a)->key == key(kW, mykam::kKeyCtrl));
    CHECK(rig.fileStore().find(a)->label == "LW");

    const int afterSaves = rig.sim.writeFileCalls;
    CHECK(rig.controller.editView(a, edit) == EditResult::Saved);  // nothing changed: nothing written
    CHECK(rig.sim.writeFileCalls == afterSaves);

    mykam::ViewEdit bad = edit;
    bad.name = "";
    CHECK(rig.controller.editView(a, bad) == EditResult::BadName);
    bad = edit;
    bad.name = "caf\xC3\xA9";
    CHECK(rig.controller.editView(a, bad) == EditResult::BadName);
    bad = edit;
    bad.name = std::string(33, 'n');
    CHECK(rig.controller.editView(a, bad) == EditResult::BadName);
    bad = edit;
    bad.label = "abc";
    CHECK(rig.controller.editView(a, bad) == EditResult::BadLabel);
    bad = edit;
    bad.numpad = 11;
    CHECK(rig.controller.editView(a, bad) == EditResult::BadNumpad);
    bad = edit;
    bad.key = key(27);
    CHECK(rig.controller.editView(a, bad) == EditResult::BadKey);
    CHECK(rig.controller.editView(99, edit) == EditResult::UnknownView);
    CHECK(rig.sim.writeFileCalls == afterSaves);
    CHECK(rig.view(a).name == "Left wing");
}

TEST_CASE("edit: a custom key taken from another view moves it") {
    Rig rig;
    const int a = rig.newFree();
    const int b = rig.newFree(2.0);
    rig.controller.setViewKey(a, key(kW));
    mykam::ViewEdit edit;
    edit.name = rig.view(b).name;
    edit.key = key(kW);
    CHECK(rig.controller.editView(b, edit) == EditResult::Saved);
    CHECK(rig.view(b).key == key(kW));
    CHECK_FALSE(rig.view(a).key.set());
}

// ---------------------------------------------------------------- quick bar: starred views, +, update, reorder

TEST_CASE("quick bar: '+' saves the current view as a new starred view that appears in the bar") {
    Rig rig;
    UiModel model(rig.controller);
    rig.inFree(4.0);
    model.updateBar();
    REQUIRE(model.barGroup() == ViewGroup::External);
    CHECK(model.barRows().empty());
    const int writes = rig.sim.writeFileCalls;

    const int id = model.saveToBar();
    REQUIRE(id != 0);
    CHECK(rig.sim.writeFileCalls == writes + 1);  // one write: the view and its star together
    CHECK(rig.view(id).favourite);
    CHECK(rig.fileStore().find(id)->favourite);
    model.refresh();
    REQUIRE(model.barRows().size() == 1);
    CHECK(model.barRows()[0].id == id);
    CHECK(model.barRows()[0].favourite);

    // The window's own save button does not star.
    const int plain = model.saveCurrent(ViewGroup::External);
    REQUIRE(plain != 0);
    CHECK_FALSE(rig.view(plain).favourite);
    model.refresh();
    CHECK(model.barRows().size() == 1);
    CHECK(model.rowCount(ViewGroup::External) == 2);

    // Another '+' goes to the end of the group and of the bar.
    const int third = model.saveToBar();
    model.refresh();
    REQUIRE(model.barRows().size() == 2);
    CHECK(model.barRows()[1].id == third);
    CHECK(rig.view(third).order > rig.view(plain).order);
}

TEST_CASE("quick bar: '+' in a group the current view does not fit saves nothing") {
    Rig rig;
    UiModel model(rig.controller);
    rig.inCockpit();
    model.updateBar();
    model.setBarGroup(ViewGroup::External);
    const int writes = rig.sim.writeFileCalls;
    CHECK(model.saveToBar() == 0);
    CHECK(rig.sim.writeFileCalls == writes);
    CHECK(rig.controller.slots().count() == 0);
}

TEST_CASE("quick bar: Update to current view works from a chip only when the current view fits the group") {
    Rig rig;
    const int ext = rig.newFree(1.0);
    rig.controller.setFavourite(ext, true);
    UiModel model(rig.controller);

    rig.inFree(8.0);
    CHECK(model.canReplace(ext));
    CHECK(model.replace(ext));
    CHECK(rig.view(ext).pose.x == doctest::Approx(8.0));
    CHECK(rig.view(ext).favourite);

    rig.inCockpit(0.3);
    CHECK_FALSE(model.canReplace(ext));
    const int writes = rig.sim.writeFileCalls;
    CHECK_FALSE(model.replace(ext));
    CHECK(rig.sim.writeFileCalls == writes);
    CHECK(rig.view(ext).pose.x == doctest::Approx(8.0));

    rig.sim.viewExternal = true;
    rig.controller.release();
    rig.controller.onViewSelected("sim/view/circle");  // saving is refused here
    CHECK_FALSE(model.canReplace(ext));
    CHECK_FALSE(model.canReplace(999));
}

TEST_CASE("quick bar: Move left and right jump over views that are not starred; drag and drop takes the place") {
    Rig rig;
    const int a = rig.newFree(1.0);
    const int b = rig.newFree(2.0);
    const int c = rig.newFree(3.0);
    const int d = rig.newFree(4.0);
    rig.controller.setFavourite(a, true);
    rig.controller.setFavourite(c, true);
    rig.controller.setFavourite(d, true);
    UiModel model(rig.controller);
    model.refresh();
    model.setBarGroup(ViewGroup::External);
    // Group order a b c d; the bar shows a c d.
    REQUIRE(model.barRows().size() == 3);
    CHECK(model.barRows()[0].id == a);

    CHECK_FALSE(model.canMove(a, -1, true));
    CHECK(model.canMove(a, 1, true));
    CHECK(model.moveRight(a));  // a takes c's place: b c a d? c moves to the front part
    model.refresh();
    CHECK(rig.controller.slots().ordered(ViewGroup::External) == std::vector<int>({b, c, a, d}));
    REQUIRE(model.barRows().size() == 3);
    CHECK(model.barRows()[0].id == c);
    CHECK(model.barRows()[1].id == a);
    CHECK(model.barRows()[2].id == d);

    CHECK(model.moveLeft(d));
    model.refresh();
    CHECK(model.barRows()[1].id == d);
    CHECK(model.barRows()[2].id == a);
    CHECK_FALSE(model.moveRight(a));  // last chip
    CHECK_FALSE(model.moveLeft(c));   // first chip

    CHECK(model.dropOn(a, c));  // drag the last chip onto the first
    model.refresh();
    CHECK(model.barRows()[0].id == a);
    CHECK(model.barRows()[1].id == c);
    CHECK(model.barRows()[2].id == d);
    CHECK(rig.fileStore().ordered(ViewGroup::External) == rig.controller.slots().ordered(ViewGroup::External));
    CHECK_FALSE(model.dropOn(a, a));
    CHECK_FALSE(model.dropOn(a, rig.newCockpit()));  // other group

    // Row moves in the window use the whole group.
    model.refresh();
    const std::vector<int> before = rig.controller.slots().ordered(ViewGroup::External);
    CHECK(model.moveDown(before[0]));
    CHECK(rig.controller.slots().ordered(ViewGroup::External)[1] == before[0]);
    CHECK(model.moveUp(before[0]));
    CHECK(rig.controller.slots().ordered(ViewGroup::External) == before);
    CHECK_FALSE(model.moveUp(before[0]));
    (void)b;
}

TEST_CASE("quick bar: Remove from quick bar clears the star only; the view stays in its list") {
    Rig rig;
    const int a = rig.newFree();
    rig.controller.setFavourite(a, true);
    UiModel model(rig.controller);
    model.refresh();
    model.setBarGroup(ViewGroup::External);
    REQUIRE(model.barRows().size() == 1);
    CHECK(model.removeFromBar(a));
    model.refresh();
    CHECK(model.barRows().empty());
    CHECK(model.rowCount(ViewGroup::External) == 1);
    CHECK_FALSE(rig.fileStore().find(a)->favourite);
}

TEST_CASE("quick bar: chips carry the label and the key text for the tooltip") {
    Rig rig;
    const int a = rig.newFree();
    mykam::ViewEdit edit;
    edit.name = "Wing";
    edit.label = "w1";
    edit.key = key(kW, mykam::kKeyShift);
    edit.favourite = true;
    REQUIRE(rig.controller.editView(a, edit) == EditResult::Saved);
    UiModel model(rig.controller);
    model.refresh();
    model.setBarGroup(ViewGroup::External);
    REQUIRE(model.barRows().size() == 1);
    CHECK(model.barRows()[0].label == "W1");
    CHECK(model.barRows()[0].keyText == "Shift+W");
    CHECK(model.barRows()[0].shortName == "Wing");
}

// ---------------------------------------------------------------- edit dialog model

TEST_CASE("edit dialog: a draft starts from the view; key choice follows custom, then numpad, then none") {
    Rig rig;
    const int a = rig.newFree();
    UiModel model(rig.controller);

    EditDraft d = model.beginEdit(a);
    CHECK(d.viewId == a);
    CHECK(d.name == "Outside view 1");
    CHECK(d.label.empty());
    CHECK(d.keyMode == KeyMode::None);
    CHECK_FALSE(d.favourite);
    CHECK(model.beginEdit(999).viewId == 0);

    rig.controller.setHotkey(a, 6);
    d = model.beginEdit(a);
    CHECK(d.keyMode == KeyMode::Numpad);
    CHECK(d.numpad == 6);

    rig.controller.setViewKey(a, key(kW, mykam::kKeyCtrl));
    rig.controller.setFavourite(a, true);
    d = model.beginEdit(a);
    CHECK(d.keyMode == KeyMode::Custom);
    CHECK(d.custom == key(kW, mykam::kKeyCtrl));
    CHECK(d.favourite);
}

TEST_CASE("edit dialog: name, label and key are validated and Save is enabled only when all are fine") {
    Rig rig;
    const int a = rig.newFree();
    UiModel model(rig.controller);
    EditDraft d = model.beginEdit(a);
    CHECK(model.checkEdit(d).ok());
    CHECK(model.canSaveEdit(d));

    d.name = "";
    CHECK_FALSE(model.checkEdit(d).name.empty());
    CHECK_FALSE(model.canSaveEdit(d));
    d.name = "   ";
    CHECK_FALSE(model.canSaveEdit(d));
    d.name = std::string(33, 'x');
    CHECK(model.checkEdit(d).name.find("32") != std::string::npos);
    CHECK_FALSE(model.canSaveEdit(d));
    d.name = "tab\there";
    CHECK_FALSE(model.canSaveEdit(d));
    d.name = std::string(32, 'x');
    CHECK(model.canSaveEdit(d));
    d.name = " Wing ";
    CHECK(model.canSaveEdit(d));

    d.label = "abc";
    CHECK_FALSE(model.checkEdit(d).label.empty());
    CHECK_FALSE(model.canSaveEdit(d));
    d.label = "a#";
    CHECK_FALSE(model.canSaveEdit(d));
    d.label = "a1";
    CHECK(model.canSaveEdit(d));
    d.label = "";
    CHECK(model.canSaveEdit(d));

    d.keyMode = KeyMode::Numpad;
    d.numpad = 0;
    CHECK_FALSE(model.checkEdit(d).key.empty());
    CHECK_FALSE(model.canSaveEdit(d));
    d.numpad = 11;
    CHECK_FALSE(model.canSaveEdit(d));
    d.numpad = mykam::kHotkeyZero;  // numpad 0
    CHECK(model.canSaveEdit(d));
    d.numpad = 9;
    CHECK(model.canSaveEdit(d));

    d.keyMode = KeyMode::Custom;
    CHECK_FALSE(model.canSaveEdit(d));  // nothing captured yet
    d.custom = key(27);
    CHECK_FALSE(model.canSaveEdit(d));
    d.custom = key(kW, mykam::kKeyShift);
    CHECK(model.canSaveEdit(d));
    d.keyMode = KeyMode::None;
    d.custom = key(0);
    CHECK(model.canSaveEdit(d));

    d.viewId = 0;
    CHECK_FALSE(model.canSaveEdit(d));
}

TEST_CASE("edit dialog: Save writes the draft once; an invalid draft is refused with the matching result") {
    Rig rig;
    const int a = rig.newFree();
    const int b = rig.newFree(2.0);
    rig.controller.setHotkey(b, 2);
    UiModel model(rig.controller);

    EditDraft d = model.beginEdit(a);
    d.name = "Wing";
    d.label = "w";
    d.keyMode = KeyMode::Numpad;
    d.numpad = 2;
    d.favourite = true;
    CHECK(model.editKeyConflict(d) == rig.view(b).name);  // b has numpad 2: saving moves it
    const int writes = rig.sim.writeFileCalls;
    CHECK(model.saveEdit(d) == EditResult::Saved);
    CHECK(rig.sim.writeFileCalls == writes + 1);
    CHECK(rig.view(a).name == "Wing");
    CHECK(rig.view(a).label == "W");
    CHECK(rig.view(a).hotkey == 2);
    CHECK(rig.view(a).favourite);
    CHECK(rig.view(b).hotkey == 0);
    CHECK(model.editKeyConflict(model.beginEdit(a)).empty());  // its own key is no conflict

    d = model.beginEdit(a);
    d.keyMode = KeyMode::Custom;
    d.custom = key(kQ, mykam::kKeyAlt);
    CHECK(model.editKeyConflict(d).empty());
    CHECK(model.saveEdit(d) == EditResult::Saved);
    CHECK(rig.view(a).hotkey == 0);  // one key kind at a time: the numpad slot is given up
    CHECK(rig.view(a).key == key(kQ, mykam::kKeyAlt));
    d.keyMode = KeyMode::None;
    CHECK(model.saveEdit(d) == EditResult::Saved);
    CHECK_FALSE(rig.view(a).key.set());

    const int afterSaves = rig.sim.writeFileCalls;
    d = model.beginEdit(a);
    d.name = "";
    CHECK(model.saveEdit(d) == EditResult::BadName);
    d = model.beginEdit(a);
    d.label = "toolong";
    CHECK(model.saveEdit(d) == EditResult::BadLabel);
    d = model.beginEdit(a);
    d.keyMode = KeyMode::Numpad;
    d.numpad = 0;
    CHECK(model.saveEdit(d) == EditResult::BadNumpad);
    d = model.beginEdit(a);
    d.keyMode = KeyMode::Custom;
    CHECK(model.saveEdit(d) == EditResult::BadKey);
    d = model.beginEdit(a);
    d.viewId = 999;
    CHECK(model.saveEdit(d) == EditResult::UnknownView);
    CHECK(rig.sim.writeFileCalls == afterSaves);
}

TEST_CASE("edit dialog: the label preview is uppercase and empty for a bad label; the note warns about X-Plane keys") {
    CHECK(UiModel::labelPreview("ab") == "AB");
    CHECK(UiModel::labelPreview("7") == "7");
    CHECK(UiModel::labelPreview("").empty());
    CHECK(UiModel::labelPreview("abc").empty());
    CHECK(UiModel::labelPreview("#").empty());
    const std::string note = UiModel::customKeyNote();
    CHECK(note.find("X-Plane does not tell plugins which keys it uses") != std::string::npos);
    CHECK(note.find("X-Plane binding may stop working while MyKam uses it.") != std::string::npos);
}

TEST_CASE("edit dialog: opening the model and checking a draft never touches the files") {
    Rig rig;
    const int a = rig.newFree();
    UiModel model(rig.controller);
    const int ops = rig.sim.fileOps();
    EditDraft d = model.beginEdit(a);
    (void)model.checkEdit(d);
    (void)model.canSaveEdit(d);
    (void)model.editKeyConflict(d);
    (void)model.canReplace(a);
    (void)model.canMove(a, 1, true);
    model.refresh();
    CHECK(rig.sim.fileOps() == ops);
}
