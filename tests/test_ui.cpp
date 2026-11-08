#include "doctest.h"

#include <cmath>
#include <limits>
#include <string>
#include <vector>

#include "config.h"
#include "controller.h"
#include "fake_sim.h"
#include "slot_store.h"
#include "ui_model.h"
#include "view_commands.h"

using mykam::ApplyResult;
using mykam::CameraState;
using mykam::Config;
using mykam::Controller;
using mykam::HandoffView;
using mykam::Pose;
using mykam::QuickBarPosition;
using mykam::RenameResult;
using mykam::RowType;
using mykam::SavedView;
using mykam::UiModel;
using mykam::ViewGroup;
using mykam::ViewKind;
using mykam::ViewStore;

namespace {

const char* const kFolder = "plug";
const char* const kIniPath = "plug/mykam.ini";
const char* const kSlotFile = "plug/cameras/Cessna_172SP.slots.txt";

Pose somePose(double x) {
    Pose p;
    p.x = x;
    p.y = 1.0;
    p.z = -2.0;
    p.pitch = 3.0;
    p.heading = 4.0;
    p.roll = 0.0;
    p.zoom = 1.0;
    return p;
}

// A started controller (transition 0, so a go is an instant cut) with the Cessna loaded.
struct Rig {
    FakeSim sim;
    Controller controller;

    Rig() : controller(sim, kFolder) {
        sim.files[kIniPath] = "transition_seconds = 0\n";
        controller.start();
        controller.onAircraftLoaded("Cessna_172SP.acf");
        sim.logs.clear();
    }

    // Put X-Plane in a view the plugin can save (or not) and let the controller know, like the
    // before-handler for the view command does.
    void inFree(double x = 1.0) {
        sim.viewExternal = true;
        sim.view = somePose(x);
        controller.release();
        controller.onViewSelected("sim/view/free_camera");
    }
    void inChase(double x = 7.0) {
        sim.viewExternal = true;
        sim.view = somePose(x);
        controller.release();
        controller.onViewSelected("sim/view/chase");
    }
    void inCockpit(double x = 0.5) {
        sim.viewExternal = false;
        sim.head = somePose(x);
        controller.release();
    }
    void inCircle() {
        sim.viewExternal = true;
        controller.release();
        controller.onViewSelected("sim/view/circle");
    }

    // New views from the current X-Plane view; the id of the new view.
    int newFree(double x = 1.0) {
        inFree(x);
        return controller.saveCurrentAsNew();
    }
    int newChase(double x = 7.0) {
        inChase(x);
        return controller.saveCurrentAsNew();
    }
    int newCockpit(double x = 0.5) {
        inCockpit(x);
        return controller.saveCurrentAsNew();
    }

    const SavedView& view(int id) {
        const SavedView* v = controller.slots().find(id);
        REQUIRE(v != nullptr);
        return *v;
    }
    // The saved file as a store, parsed from what the fake sim holds.
    ViewStore fileStore() {
        ViewStore s;
        REQUIRE(sim.files.count(kSlotFile) == 1);
        s.parse(sim.files[kSlotFile]);
        return s;
    }
};

// A complete old-format slot as text.
std::string oldSlot(int n, const std::string& extra = std::string()) {
    const std::string p = "slot" + std::to_string(n) + ".";
    return p + "x=" + std::to_string(n) + "\n" + p + "y=2\n" + p + "z=3\n" + p + "pitch=4\n" + p + "heading=5\n" + p +
           "roll=6\n" + p + "zoom=1\n" + extra;
}

}  // namespace

// ---------------------------------------------------------------- config

TEST_CASE("config: quick bar and key hint settings parse, missing keys keep defaults quietly") {
    const mykam::ConfigResult plain = mykam::parseConfig("transition_seconds = 2\n");
    CHECK(plain.warning.empty());
    CHECK(plain.config.showQuickBar);
    CHECK(plain.config.quickBarPosition == QuickBarPosition::Bottom);
    CHECK(plain.config.keyHintGo == "Numpad {n}");
    CHECK(plain.config.keyHintSave == "Ctrl+Alt+Numpad {n}");

    const mykam::ConfigResult r = mykam::parseConfig(
        "transition_seconds = 2\nshow_quick_bar = 0\nquick_bar_position = TOP\nkey_hint_go = F{n}\nkey_hint_save =\n");
    CHECK(r.warning.empty());
    CHECK_FALSE(r.config.showQuickBar);
    CHECK(r.config.quickBarPosition == QuickBarPosition::Top);
    CHECK(r.config.keyHintGo == "F{n}");
    CHECK(r.config.keyHintSave.empty());  // an empty hint hides it
}

TEST_CASE("config: invalid quick bar and hint values give the defaults and one warning line") {
    const mykam::ConfigResult r = mykam::parseConfig(
        "transition_seconds = 2\nshow_quick_bar = maybe\nquick_bar_position = middle\n"
        "key_hint_go = 123456789012345678901234567890123\n");
    CHECK(r.config.showQuickBar);
    CHECK(r.config.quickBarPosition == QuickBarPosition::Bottom);
    CHECK(r.config.keyHintGo == "Numpad {n}");
    CHECK(r.warning.find("show_quick_bar") != std::string::npos);
    CHECK(r.warning.find("quick_bar_position") != std::string::npos);
    CHECK(r.warning.find("key_hint_go") != std::string::npos);
    CHECK(r.warning.find('\n') == std::string::npos);
}

TEST_CASE("config: validateConfig applies the same ranges") {
    Config c;
    std::string problem;
    CHECK(mykam::validateConfig(c, problem));
    c.transitionSeconds = 0.0;
    CHECK(mykam::validateConfig(c, problem));
    c.transitionSeconds = 10.0;
    CHECK(mykam::validateConfig(c, problem));
    c.transitionSeconds = 10.1;
    CHECK_FALSE(mykam::validateConfig(c, problem));
    CHECK(problem.find("transition") != std::string::npos);
    c.transitionSeconds = -0.1;
    CHECK_FALSE(mykam::validateConfig(c, problem));
    c.transitionSeconds = std::numeric_limits<double>::quiet_NaN();
    CHECK_FALSE(mykam::validateConfig(c, problem));
    c.transitionSeconds = 1.0;
    c.handoffView = static_cast<HandoffView>(7);
    CHECK_FALSE(mykam::validateConfig(c, problem));
    c.handoffView = HandoffView::Free;
    c.quickBarPosition = static_cast<QuickBarPosition>(5);
    CHECK_FALSE(mykam::validateConfig(c, problem));
    c.quickBarPosition = QuickBarPosition::Top;
    c.keyHintGo = "tab\t";
    CHECK_FALSE(mykam::validateConfig(c, problem));
}

TEST_CASE("config: updateConfigText keeps comments and unknown lines and never loses a valid key") {
    const std::string old =
        "# my notes\r\ntransition_seconds = 1.5\r\nfree_camera_handoff = 0\r\nkey_hint_go = F{n}\r\nsomething_else = 42\r\n";
    Config c = mykam::parseConfig(old).config;
    c.transitionSeconds = 3.2;
    c.handoffView = HandoffView::Free;
    c.showQuickBar = false;
    c.quickBarPosition = QuickBarPosition::Top;
    const std::string updated = mykam::updateConfigText(old, c);

    CHECK(updated.find("# my notes\n") != std::string::npos);
    CHECK(updated.find("free_camera_handoff = 0\n") != std::string::npos);  // old key kept
    CHECK(updated.find("key_hint_go = F{n}\n") != std::string::npos);
    CHECK(updated.find("something_else = 42\n") != std::string::npos);
    CHECK(updated.find("transition_seconds = 3.2\n") != std::string::npos);
    CHECK(updated.find("\r") == std::string::npos);

    const mykam::ConfigResult back = mykam::parseConfig(updated);
    CHECK(back.warning.empty());
    CHECK(back.config.transitionSeconds == doctest::Approx(3.2));
    CHECK(back.config.handoffView == HandoffView::Free);  // new key wins over free_camera_handoff
    CHECK_FALSE(back.config.showQuickBar);
    CHECK(back.config.quickBarPosition == QuickBarPosition::Top);
    CHECK(back.config.keyHintGo == "F{n}");

    // A second update changes lines in place and adds none.
    const std::string again = mykam::updateConfigText(updated, c);
    CHECK(again == updated);
}

TEST_CASE("config: updateConfigText on an empty file writes a header and all five settings") {
    const std::string text = mykam::updateConfigText("", Config());
    CHECK(text.rfind("# MyKam settings", 0) == 0);
    const mykam::ConfigResult r = mykam::parseConfig(text);
    CHECK(r.warning.empty());
    CHECK(r.config.transitionSeconds == doctest::Approx(1.5));
    CHECK(text.find("handoff_view = saved\n") != std::string::npos);
    CHECK(text.find("show_quick_bar = 1\n") != std::string::npos);
    CHECK(text.find("quick_bar_position = bottom\n") != std::string::npos);
    CHECK(text.find("quick_bar_views = cockpit,free,chase\n") != std::string::npos);
}

// ---------------------------------------------------------------- hotkey commands

TEST_CASE("save_slot: creates a view with that hotkey, then saves into the same view and keeps its name and favourite") {
    Rig rig;
    rig.inFree(1.0);
    rig.controller.saveSlot(3);
    REQUIRE(rig.controller.slots().count() == 1);
    const int id = rig.controller.slots().hotkeyOwner(3);
    REQUIRE(id != 0);
    CHECK(rig.view(id).name == "Outside view 1");
    CHECK(rig.view(id).hotkey == 3);
    CHECK(rig.view(id).kind == ViewKind::External);
    CHECK(rig.view(id).viewCommand == "sim/view/free_camera");
    CHECK(rig.sim.logCount("saved camera to slot 3") == 1);
    CHECK(rig.sim.files[kSlotFile].rfind("format=2\n", 0) == 0);

    rig.controller.renameView(id, "Wing");
    rig.controller.setFavourite(id, true);
    rig.inChase(5.0);
    rig.controller.saveSlot(3);  // saves into the view that has hotkey 3
    REQUIRE(rig.controller.slots().count() == 1);
    CHECK(rig.view(id).name == "Wing");
    CHECK(rig.view(id).favourite);
    CHECK(rig.view(id).hotkey == 3);
    CHECK(rig.view(id).pose.x == doctest::Approx(5.0));
    CHECK(rig.view(id).viewCommand == "sim/view/chase");
    CHECK(rig.fileStore().find(id)->pose.x == doctest::Approx(5.0));
    CHECK(rig.fileStore().find(id)->name == "Wing");
}

TEST_CASE("save_slot: a hotkey on a view in the other group changes its kind but keeps name, favourite and hotkey") {
    Rig rig;
    const int id = rig.newFree();
    rig.controller.setHotkey(id, 2);
    rig.controller.setFavourite(id, true);
    rig.inCockpit(0.9);
    rig.controller.saveSlot(2);
    CHECK(rig.controller.slots().count() == 1);
    CHECK(rig.view(id).kind == ViewKind::Cockpit);
    CHECK(rig.view(id).name == "Outside view 1");
    CHECK(rig.view(id).favourite);
    CHECK(rig.view(id).viewCommand.empty());
}

TEST_CASE("save_slot: keeps the refusals, bad numbers and the empty hotkey logs") {
    Rig rig;
    rig.inCircle();
    rig.controller.saveSlot(2);
    CHECK(rig.controller.slots().count() == 0);
    CHECK(rig.sim.writeFileCalls == 0);
    REQUIRE(rig.sim.logs.size() == 1);
    CHECK(rig.sim.logs[0].find("slot 2 not saved: saving is not available in the sim/view/circle") != std::string::npos);

    rig.sim.logs.clear();
    rig.controller.saveSlot(0);
    rig.controller.saveSlot(10);
    rig.controller.goToSlot(0);
    rig.controller.goToSlot(10);
    CHECK(rig.sim.logs.empty());
}

TEST_CASE("go_slot: goes to the view with that hotkey, an empty hotkey logs one line") {
    Rig rig;
    rig.inFree();
    rig.controller.saveSlot(2);
    rig.inCockpit();
    rig.controller.saveSlot(4);
    const int free = rig.controller.slots().hotkeyOwner(2);
    const int cockpit = rig.controller.slots().hotkeyOwner(4);

    rig.controller.goToSlot(2);
    CHECK(rig.controller.activeView() == free);
    CHECK(rig.controller.state() == CameraState::Holding);
    rig.controller.goToSlot(4);
    CHECK(rig.controller.activeView() == cockpit);
    CHECK(rig.controller.state() == CameraState::Idle);  // a cockpit view never keeps the camera

    rig.sim.logs.clear();
    rig.controller.goToSlot(7);
    REQUIRE(rig.sim.logs.size() == 1);
    CHECK(rig.sim.logs[0].find("slot 7 is empty") != std::string::npos);
    CHECK(rig.controller.activeView() == cockpit);

    // Moving the key moves the command with it.
    rig.controller.setHotkey(free, 4);
    rig.controller.goToSlot(4);
    CHECK(rig.controller.activeView() == free);
    rig.sim.logs.clear();
    rig.controller.goToSlot(2);
    CHECK(rig.sim.logCount("slot 2 is empty") == 1);
}

// ---------------------------------------------------------------- new views from the window

TEST_CASE("save current: new views get default names by kind and the kind and view command of the current view") {
    Rig rig;
    const int free = rig.newFree();
    const int chase = rig.newChase();
    const int cockpit = rig.newCockpit();
    const int second = rig.newCockpit(0.6);
    CHECK(free == 1);
    CHECK(chase == 2);
    CHECK(cockpit == 3);
    CHECK(second == 4);
    CHECK(rig.view(free).name == "Outside view 1");
    CHECK(rig.view(free).viewCommand == "sim/view/free_camera");
    CHECK(rig.view(chase).name == "Outside view 2");
    CHECK(rig.view(chase).viewCommand == "sim/view/chase");
    CHECK(rig.view(cockpit).name == "Cockpit view 1");
    CHECK(rig.view(cockpit).kind == ViewKind::Cockpit);
    CHECK(rig.view(second).name == "Cockpit view 2");
    CHECK(rig.view(free).hotkey == 0);
    CHECK_FALSE(rig.view(free).favourite);
    CHECK(rig.sim.logCount("saved camera as 'Outside view 1'") == 1);
    CHECK(rig.fileStore().count() == 4);
}

TEST_CASE("save current: refused in the circle view and outside the accepted range, with one log line") {
    Rig rig;
    rig.inCircle();
    CHECK(rig.controller.saveCurrentAsNew() == 0);
    CHECK(rig.sim.writeFileCalls == 0);
    REQUIRE(rig.sim.logs.size() == 1);
    CHECK(rig.sim.logs[0].find("not saved") != std::string::npos);

    rig.sim.logs.clear();
    rig.inFree(50000.0);  // 50 km from the aircraft
    CHECK(rig.controller.saveCurrentAsNew() == 0);
    CHECK(rig.sim.writeFileCalls == 0);
    CHECK(rig.sim.logCount("warning") == 1);
    CHECK(rig.controller.slots().count() == 0);
}

TEST_CASE("save current: with 99 views the 100th is refused with one log line, hotkey replace still works") {
    Rig rig;
    rig.inFree();
    for (int i = 1; i <= mykam::kMaxViews; ++i) {
        REQUIRE(rig.controller.saveCurrentAsNew() == i);
    }
    REQUIRE(rig.controller.slots().count() == 99);
    CHECK(rig.sim.files[kSlotFile].size() < mykam::kMaxSlotFileBytes);

    rig.sim.logs.clear();
    const int writes = rig.sim.writeFileCalls;
    CHECK(rig.controller.saveCurrentAsNew() == 0);
    REQUIRE(rig.sim.logs.size() == 1);
    CHECK(rig.sim.logs[0].find("limit of 99") != std::string::npos);
    CHECK(rig.sim.writeFileCalls == writes);

    rig.sim.logs.clear();
    rig.controller.saveSlot(5);  // no view has key 5: it would have to create one
    REQUIRE(rig.sim.logs.size() == 1);
    CHECK(rig.sim.logs[0].find("limit of 99") != std::string::npos);
    CHECK(rig.sim.writeFileCalls == writes);
    CHECK(rig.controller.slots().count() == 99);

    CHECK(rig.controller.setHotkey(10, 5));
    rig.sim.logs.clear();
    rig.inFree(3.0);
    rig.controller.saveSlot(5);  // a view has key 5: replacing is fine when the list is full
    CHECK(rig.sim.logCount("saved camera to slot 5") == 1);
    CHECK(rig.controller.slots().count() == 99);
    CHECK(rig.view(10).pose.x == doctest::Approx(3.0));

    // The full file reads back completely.
    rig.controller.onAircraftLoaded("Cessna_172SP.acf");
    CHECK(rig.controller.slots().count() == 99);
}

TEST_CASE("replace: only with a current view that fits the group, name, favourite and hotkey stay") {
    Rig rig;
    const int ext = rig.newFree(1.0);
    rig.controller.setHotkey(ext, 6);
    rig.controller.renameView(ext, "Wing");
    rig.controller.setFavourite(ext, true);

    rig.inChase(9.0);
    CHECK(rig.controller.replaceView(ext));
    CHECK(rig.view(ext).pose.x == doctest::Approx(9.0));
    CHECK(rig.view(ext).viewCommand == "sim/view/chase");
    CHECK(rig.view(ext).name == "Wing");
    CHECK(rig.view(ext).favourite);
    CHECK(rig.view(ext).hotkey == 6);
    CHECK(rig.view(ext).kind == ViewKind::External);
    CHECK(rig.fileStore().find(ext)->pose.x == doctest::Approx(9.0));

    // A cockpit view does not fit an external view: nothing changes, one log line.
    rig.inCockpit(0.2);
    rig.sim.logs.clear();
    const int writes = rig.sim.writeFileCalls;
    CHECK_FALSE(rig.controller.replaceView(ext));
    CHECK(rig.sim.logs.size() == 1);
    CHECK(rig.sim.writeFileCalls == writes);
    CHECK(rig.view(ext).pose.x == doctest::Approx(9.0));

    // And the other way round.
    const int in = rig.newCockpit(0.3);
    rig.inFree(2.0);
    CHECK_FALSE(rig.controller.replaceView(in));
    CHECK(rig.view(in).pose.x == doctest::Approx(0.3));
    rig.inCockpit(0.9);
    CHECK(rig.controller.replaceView(in));
    CHECK(rig.view(in).pose.x == doctest::Approx(0.9));
    CHECK(rig.view(in).kind == ViewKind::Cockpit);

    rig.inCircle();
    CHECK_FALSE(rig.controller.replaceView(ext));
    CHECK_FALSE(rig.controller.replaceView(999));
}

TEST_CASE("replace: while mykam holds the camera the held view is what gets stored") {
    Rig rig;
    const int a = rig.newFree(10.0);
    const int b = rig.newFree(-10.0);
    rig.controller.goToView(a);
    REQUIRE(rig.controller.state() != CameraState::Idle);
    CHECK(rig.controller.replaceView(b));
    CHECK(rig.view(b).pose.x == doctest::Approx(10.0));
}

// ---------------------------------------------------------------- rename, favourite, hotkey, delete

TEST_CASE("rename: keeps a valid name in the file and rejects bad ones") {
    Rig rig;
    const int id = rig.newFree();
    CHECK(rig.controller.renameView(id, "  Left wing ") == RenameResult::Renamed);
    CHECK(rig.view(id).name == "Left wing");
    CHECK(rig.fileStore().find(id)->name == "Left wing");

    const int writes = rig.sim.writeFileCalls;
    CHECK(rig.controller.renameView(id, std::string(33, 'n')) == RenameResult::TooLong);
    CHECK(rig.controller.renameView(id, "bad\tname") == RenameResult::NotPrintable);
    CHECK(rig.controller.renameView(id, "caf\xC3\xA9") == RenameResult::NotPrintable);
    CHECK(rig.controller.renameView(99, "nothing here") == RenameResult::UnknownView);
    CHECK(rig.sim.writeFileCalls == writes);  // rejected names write nothing
    CHECK(rig.view(id).name == "Left wing");

    CHECK(rig.controller.renameView(id, std::string(32, 'n')) == RenameResult::Renamed);  // exactly 32
    CHECK(rig.controller.renameView(id, "   ") == RenameResult::Reset);  // empty brings a default name back
    CHECK(rig.view(id).name == "Outside view 1");
    CHECK(rig.fileStore().find(id)->name == "Outside view 1");
    const int resetWrites = rig.sim.writeFileCalls;
    CHECK(rig.controller.renameView(id, "") == RenameResult::Reset);  // already the default: no write needed
    CHECK(rig.sim.writeFileCalls == resetWrites);
}

TEST_CASE("favourite: it is saved and shows in the file; unknown views are refused") {
    Rig rig;
    const int id = rig.newFree();
    CHECK(rig.controller.setFavourite(id, true));
    CHECK(rig.fileStore().find(id)->favourite);
    CHECK(rig.controller.setFavourite(id, false));
    CHECK_FALSE(rig.fileStore().find(id)->favourite);
    const int writes = rig.sim.writeFileCalls;
    CHECK(rig.controller.setFavourite(id, false));  // unchanged: nothing written
    CHECK(rig.sim.writeFileCalls == writes);
    CHECK_FALSE(rig.controller.setFavourite(42, true));
}

TEST_CASE("hotkey: assign, move from another view, clear; the file follows") {
    Rig rig;
    const int a = rig.newFree();
    const int b = rig.newCockpit();
    CHECK(rig.controller.setHotkey(a, 3));
    CHECK(rig.fileStore().find(a)->hotkey == 3);

    CHECK(rig.controller.setHotkey(b, 3));  // moves from a to b
    CHECK(rig.view(a).hotkey == 0);
    CHECK(rig.view(b).hotkey == 3);
    CHECK(rig.fileStore().find(a)->hotkey == 0);
    CHECK(rig.fileStore().find(b)->hotkey == 3);

    const int writes = rig.sim.writeFileCalls;
    CHECK(rig.controller.setHotkey(b, 3));  // unchanged
    CHECK(rig.sim.writeFileCalls == writes);

    CHECK(rig.controller.setHotkey(b, 0));  // clear
    CHECK(rig.view(b).hotkey == 0);
    CHECK_FALSE(rig.fileStore().find(b)->hotkey != 0);

    CHECK_FALSE(rig.controller.setHotkey(b, 10));
    CHECK_FALSE(rig.controller.setHotkey(b, -1));
    CHECK_FALSE(rig.controller.setHotkey(77, 1));
}

TEST_CASE("delete: removes the view from memory and from the file; ids are not reused") {
    Rig rig;
    const int a = rig.newFree();
    const int b = rig.newCockpit();
    rig.controller.goToView(a);
    REQUIRE(rig.controller.activeView() == a);

    CHECK(rig.controller.deleteView(a));
    CHECK(rig.controller.slots().find(a) == nullptr);
    CHECK(rig.controller.activeView() == 0);
    CHECK(rig.controller.state() != CameraState::Idle);
    CHECK(rig.controller.stateText() == "MyKam is holding the camera");
    CHECK(rig.fileStore().find(a) == nullptr);
    CHECK(rig.fileStore().find(b) != nullptr);
    CHECK(rig.sim.files[kSlotFile].find("view1.") == std::string::npos);
    CHECK_FALSE(rig.controller.deleteView(a));  // already gone
    CHECK_FALSE(rig.controller.deleteView(0));

    const int c = rig.newFree();  // a new id, never the one just deleted
    CHECK(c == 3);
    CHECK(rig.controller.deleteView(b));
    CHECK(rig.controller.deleteView(c));
    CHECK(rig.fileStore().count() == 0);  // an empty list still writes a valid file
    CHECK(rig.newFree() == 4);
}

// ---------------------------------------------------------------- next and previous, active view

TEST_CASE("next and previous: Internal then External, in group order, wrapping round") {
    Rig rig;
    const int e1 = rig.newFree(1.0);
    const int i1 = rig.newCockpit(0.1);
    const int e2 = rig.newChase(2.0);
    const int i2 = rig.newCockpit(0.2);
    rig.controller.release();
    // Step order: i1, i2, e1, e2.
    rig.controller.next();
    CHECK(rig.controller.activeView() == i1);
    rig.controller.next();
    CHECK(rig.controller.activeView() == i2);
    rig.controller.next();
    CHECK(rig.controller.activeView() == e1);
    rig.controller.next();
    CHECK(rig.controller.activeView() == e2);
    rig.controller.next();  // wraps
    CHECK(rig.controller.activeView() == i1);
    rig.controller.previous();  // wraps backwards
    CHECK(rig.controller.activeView() == e2);
    rig.controller.previous();
    CHECK(rig.controller.activeView() == e1);

    // A star no longer changes the order; moving a view does.
    rig.controller.setFavourite(e2, true);
    rig.controller.setFavourite(i2, true);
    rig.controller.release();
    rig.controller.next();
    CHECK(rig.controller.activeView() == i1);
    CHECK(rig.controller.moveView(i2, i1));  // i2 takes the first place of the Internal group
    rig.controller.moveView(e2, e1);
    // Step order: i2, i1, e2, e1.
    rig.controller.release();
    rig.controller.next();
    CHECK(rig.controller.activeView() == i2);
    rig.controller.next();
    CHECK(rig.controller.activeView() == i1);
    rig.controller.next();
    CHECK(rig.controller.activeView() == e2);
    rig.controller.next();
    CHECK(rig.controller.activeView() == e1);

    rig.controller.release();
    rig.controller.previous();  // no active view: the last
    CHECK(rig.controller.activeView() == e1);
}

TEST_CASE("next and previous with nothing saved only log; a single view steps to itself") {
    Rig rig;
    rig.controller.next();
    rig.controller.previous();
    CHECK(rig.controller.activeView() == 0);
    CHECK(rig.controller.state() == CameraState::Idle);
    CHECK(rig.sim.takeCalls == 0);
    CHECK(rig.sim.logCount("no saved cameras") == 2);

    const int only = rig.newFree();
    rig.controller.release();
    rig.controller.next();
    CHECK(rig.controller.activeView() == only);
    rig.controller.next();
    CHECK(rig.controller.activeView() == only);
    rig.controller.previous();
    CHECK(rig.controller.activeView() == only);
}

TEST_CASE("active view: it follows go, rename and delete and is cleared by release and X-Plane views") {
    Rig rig;
    const int free = rig.newFree();
    const int chase = rig.newChase();
    CHECK(rig.controller.activeView() == 0);
    CHECK(rig.controller.stateText() == "X-Plane in control");

    rig.controller.goToView(free);
    CHECK(rig.controller.activeView() == free);
    CHECK(rig.controller.stateText() == "Outside view 1 - Free camera");
    rig.controller.renameView(free, "Wing");
    CHECK(rig.controller.stateText() == "Wing - Free camera");
    rig.controller.goToView(chase);
    CHECK(rig.controller.stateText() == "Outside view 2 - Chase");

    rig.controller.release();
    CHECK(rig.controller.activeView() == 0);
    CHECK(rig.controller.stateText() == "X-Plane in control");

    rig.controller.goToView(chase);
    rig.controller.onViewSelected("sim/view/circle");
    CHECK(rig.controller.activeView() == 0);
    rig.controller.goToView(999);  // unknown: nothing happens
    CHECK(rig.controller.activeView() == 0);
}

TEST_CASE("active view: a cockpit view is active too, and a hand-over keeps its view active") {
    Rig rig;
    const int cockpit = rig.newCockpit();
    rig.controller.goToView(cockpit);
    CHECK(rig.controller.activeView() == cockpit);
    CHECK(rig.controller.stateText() == "Cockpit view 1 - Cockpit");

    const int free = rig.newFree();
    rig.controller.goToView(free);
    Pose world;
    rig.controller.tick(0.1, world);  // arrived: the hand-over is pending
    rig.controller.performHandoff();
    CHECK(rig.controller.state() == CameraState::Idle);
    CHECK(rig.controller.activeView() == free);  // X-Plane holds the view now, but it is this view's spot
    rig.controller.onCameraLost();               // somebody else took the camera
    CHECK(rig.controller.activeView() == 0);
}

TEST_CASE("an X-Plane view button releases the camera and runs only listed commands") {
    Rig rig;
    const int free = rig.newFree();
    rig.controller.goToView(free);
    REQUIRE(rig.controller.state() != CameraState::Idle);

    CHECK(rig.controller.selectXPlaneView("sim/view/chase"));
    CHECK(rig.controller.state() == CameraState::Idle);
    REQUIRE_FALSE(rig.sim.viewCommandsRun.empty());
    CHECK(rig.sim.viewCommandsRun.back() == "sim/view/chase");
    CHECK(rig.controller.lastViewCommand() == "sim/view/chase");
    CHECK(rig.controller.activeView() == 0);

    const std::size_t runs = rig.sim.viewCommandsRun.size();
    CHECK_FALSE(rig.controller.selectXPlaneView("sim/operation/quit"));
    CHECK_FALSE(rig.controller.selectXPlaneView(""));
    CHECK(rig.sim.viewCommandsRun.size() == runs);

    rig.sim.missingCommands.insert("sim/view/tower");
    CHECK_FALSE(rig.controller.selectXPlaneView("sim/view/tower"));
    CHECK(rig.sim.logCount("not found") == 1);
}

// ---------------------------------------------------------------- migration

TEST_CASE("migration: an old slot file loads as views with hotkeys, is not rewritten on load, and is converted on the next save") {
    Rig rig;
    rig.sim.files[kSlotFile] = "mykam_slots=1\n" + oldSlot(1, "slot1.name=Wing\nslot1.favourite=1\nslot1.view=sim/view/chase\n") +
                               oldSlot(3, "slot3.kind=cockpit\n") + oldSlot(6);
    const std::string original = rig.sim.files[kSlotFile];
    rig.controller.onAircraftLoaded("Cessna_172SP.acf");

    CHECK(rig.sim.logCount("3 saved camera") == 1);
    CHECK(rig.sim.logCount("converted 3 view(s)") == 1);
    CHECK(rig.sim.logCount("warning") == 0);
    CHECK(rig.controller.slots().count() == 3);
    CHECK(rig.sim.writeFileCalls == 0);
    CHECK(rig.sim.files[kSlotFile] == original);  // the old file is untouched until something is saved

    const int wing = rig.controller.slots().hotkeyOwner(1);
    const int panel = rig.controller.slots().hotkeyOwner(3);
    REQUIRE(wing == 1);
    REQUIRE(panel == 3);
    CHECK(rig.view(wing).name == "Wing");
    CHECK(rig.view(wing).favourite);
    CHECK(rig.view(wing).viewCommand == "sim/view/chase");
    CHECK(rig.view(panel).kind == ViewKind::Cockpit);
    CHECK(rig.view(panel).name == "Cockpit view 1");
    CHECK(rig.view(rig.controller.slots().hotkeyOwner(6)).name == "Outside view 1");

    // The old commands still work through the hotkeys.
    rig.controller.goToSlot(3);
    CHECK(rig.controller.activeView() == panel);
    CHECK(rig.sim.writeFileCalls == 0);

    rig.controller.setFavourite(wing, false);  // the first change writes the new format
    CHECK(rig.sim.writeFileCalls == 1);
    const std::string& now = rig.sim.files[kSlotFile];
    CHECK(now.rfind("format=2\n", 0) == 0);
    CHECK(now.find("slot") == std::string::npos);
    ViewStore back = rig.fileStore();
    CHECK(back.count() == 3);
    CHECK(back.find(3)->kind == ViewKind::Cockpit);
    CHECK(back.find(1)->name == "Wing");
    CHECK_FALSE(back.find(1)->favourite);
}

TEST_CASE("migration: a broken slot in an old file is skipped with one warning, the rest load") {
    Rig rig;
    rig.sim.files[kSlotFile] = "mykam_slots=1\n" + oldSlot(2) + "slot5.x=oops\n";
    rig.controller.onAircraftLoaded("Cessna_172SP.acf");
    CHECK(rig.controller.slots().count() == 1);
    CHECK(rig.controller.slots().has(2));
    CHECK(rig.sim.logCount("warning") == 1);
}

TEST_CASE("migration: a file that is already in the new format is not reported as converted") {
    Rig rig;
    const int id = rig.newFree();
    (void)id;
    rig.sim.logs.clear();
    rig.controller.onAircraftLoaded("Cessna_172SP.acf");
    CHECK(rig.sim.logCount("1 saved camera") == 1);
    CHECK(rig.sim.logCount("converted") == 0);
    CHECK(rig.controller.slots().count() == 1);
}

// ---------------------------------------------------------------- settings through the controller

TEST_CASE("applySettings writes mykam.ini, keeps other lines, rejects invalid values") {
    Rig rig;
    rig.sim.files[kIniPath] = "# keep me\ntransition_seconds = 0\nmystery = 1\n";
    Config c = rig.controller.config();
    c.transitionSeconds = 2.5;
    c.handoffView = HandoffView::Off;
    c.showQuickBar = false;
    c.quickBarPosition = QuickBarPosition::Top;
    CHECK(rig.controller.applySettings(c) == ApplyResult::Saved);
    CHECK(rig.controller.transitionSeconds() == doctest::Approx(2.5));
    CHECK(rig.controller.config().handoffView == HandoffView::Off);

    const std::string& ini = rig.sim.files[kIniPath];
    CHECK(ini.find("# keep me\n") != std::string::npos);
    CHECK(ini.find("mystery = 1\n") != std::string::npos);
    const mykam::ConfigResult back = mykam::parseConfig(ini);
    CHECK(back.warning.empty());
    CHECK(back.config.transitionSeconds == doctest::Approx(2.5));
    CHECK(back.config.handoffView == HandoffView::Off);
    CHECK_FALSE(back.config.showQuickBar);
    CHECK(back.config.quickBarPosition == QuickBarPosition::Top);

    // The new transition time is used by the next go-to.
    const int id = rig.newFree();
    rig.controller.goToView(id);
    CHECK(rig.controller.state() == CameraState::Transitioning);

    // Invalid values: nothing applied, nothing written.
    const std::string before = rig.sim.files[kIniPath];
    const int writes = rig.sim.writeFileCalls;
    Config bad = rig.controller.config();
    bad.transitionSeconds = 11.0;
    CHECK(rig.controller.applySettings(bad) == ApplyResult::Invalid);
    bad.transitionSeconds = std::numeric_limits<double>::infinity();
    CHECK(rig.controller.applySettings(bad) == ApplyResult::Invalid);
    bad = rig.controller.config();
    bad.handoffView = static_cast<HandoffView>(9);
    CHECK(rig.controller.applySettings(bad) == ApplyResult::Invalid);
    CHECK(rig.controller.transitionSeconds() == doctest::Approx(2.5));
    CHECK(rig.sim.files[kIniPath] == before);
    CHECK(rig.sim.writeFileCalls == writes);
}

TEST_CASE("applySettings creates a missing mykam.ini and reports write problems") {
    FakeSim sim;
    Controller c(sim, kFolder);
    c.start();  // no mykam.ini yet
    Config cfg = c.config();
    cfg.transitionSeconds = 4.0;
    CHECK(c.applySettings(cfg) == ApplyResult::Saved);
    REQUIRE(sim.files.count(kIniPath) == 1);
    CHECK(mykam::parseConfig(sim.files[kIniPath]).config.transitionSeconds == doctest::Approx(4.0));

    sim.failWrites = true;
    cfg.transitionSeconds = 5.0;
    CHECK(c.applySettings(cfg) == ApplyResult::NotSaved);
    CHECK(c.transitionSeconds() == doctest::Approx(5.0));  // active even though it could not be saved
    CHECK(sim.logCount("could not be written") == 1);

    sim.failWrites = false;
    sim.forcedStatus[kIniPath] = mykam::FileStatus::Error;  // unreadable: never overwritten
    const std::string before = sim.files[kIniPath];
    cfg.transitionSeconds = 6.0;
    CHECK(c.applySettings(cfg) == ApplyResult::NotSaved);
    CHECK(sim.files[kIniPath] == before);
}

TEST_CASE("no file access when only reading state") {
    Rig rig;
    rig.newFree();
    const int ops = rig.sim.fileOps();
    (void)rig.controller.stateText();
    (void)rig.controller.revision();
    UiModel model(rig.controller);
    model.refresh();
    model.refresh();
    (void)model.canSaveCurrent(ViewGroup::Internal);
    CHECK(rig.sim.fileOps() == ops);
}

// ---------------------------------------------------------------- ui model: group lists

TEST_CASE("ui model: each group lists its views with type, name, key badge, favourite and active, in group order") {
    Rig rig;
    const int free = rig.newFree();
    const int chase = rig.newChase();
    const int cockpit = rig.newCockpit();
    rig.controller.setHotkey(free, 1);
    rig.controller.setHotkey(cockpit, 7);
    rig.controller.renameView(chase, "Behind");
    rig.controller.setFavourite(chase, true);
    rig.controller.goToView(free);

    UiModel model(rig.controller);
    const std::vector<mykam::ViewRow>& external = model.rows(ViewGroup::External);
    const std::vector<mykam::ViewRow>& internal = model.rows(ViewGroup::Internal);
    REQUIRE(external.size() == 2);
    REQUIRE(internal.size() == 1);
    CHECK(model.rowCount(ViewGroup::External) == 2);
    CHECK(model.rowCount(ViewGroup::Internal) == 1);

    // Group order is the order field: the star does not move the chase view to the front any more.
    CHECK(external[0].id == free);
    CHECK(external[0].type == RowType::FreeCamera);
    CHECK(external[0].typeLabel == "Free camera");
    CHECK_FALSE(external[0].favourite);
    CHECK(external[0].active);
    CHECK(external[0].hotkey == 1);
    CHECK(external[0].keyBadge == "Numpad 1");
    CHECK(external[0].keyText == "Num 1");
    CHECK(external[0].label.empty());

    CHECK(external[1].id == chase);
    CHECK(external[1].name == "Behind");
    CHECK(external[1].type == RowType::Chase);
    CHECK(external[1].typeLabel == "Chase");
    CHECK(external[1].favourite);
    CHECK_FALSE(external[1].active);
    CHECK(external[1].hotkey == 0);
    CHECK(external[1].keyBadge.empty());
    CHECK(external[1].keyText.empty());

    CHECK(internal[0].id == cockpit);
    CHECK(internal[0].type == RowType::Cockpit);
    CHECK(internal[0].typeLabel == "Cockpit");
    CHECK(internal[0].keyBadge == "Numpad 7");
    CHECK(internal[0].name == "Cockpit view 1");

    CHECK(model.headerState() == "Outside view 1 - Free camera");
    CHECK(model.activeName() == "Outside view 1");
    CHECK(model.activeTypeLabel() == "Free camera");
    CHECK(model.activeView() == free);
    CHECK(model.anySaved());
}

TEST_CASE("ui model: an empty group has no rows; the cache follows changes and stays untouched otherwise") {
    Rig rig;
    UiModel model(rig.controller);
    CHECK_FALSE(model.anySaved());
    CHECK(model.rows(ViewGroup::Internal).empty());
    CHECK(model.rows(ViewGroup::External).empty());
    CHECK(model.headerState() == "X-Plane in control");
    CHECK(model.activeName().empty());
    CHECK(model.activeTypeLabel().empty());

    const unsigned revision = rig.controller.revision();
    model.refresh();
    CHECK(rig.controller.revision() == revision);

    const int id = rig.newFree();
    model.refresh();
    CHECK(model.rowCount(ViewGroup::External) == 1);
    CHECK(model.rowCount(ViewGroup::Internal) == 0);
    model.go(id);
    model.refresh();
    CHECK(model.rows(ViewGroup::External)[0].active);
    model.release();
    model.refresh();
    CHECK_FALSE(model.rows(ViewGroup::External)[0].active);
    CHECK(model.headerState() == "X-Plane in control");
}

TEST_CASE("ui model: key badges come from mykam.ini and can be hidden") {
    Rig rig;
    rig.sim.files[kIniPath] = "transition_seconds = 0\nkey_hint_go = Num {n}\n";
    rig.controller.start();
    const int id = rig.newFree();
    rig.controller.setHotkey(id, 3);
    UiModel model(rig.controller);
    CHECK(model.rows(ViewGroup::External)[0].keyBadge == "Num 3");

    rig.sim.files[kIniPath] = "transition_seconds = 0\nkey_hint_go =\n";
    rig.controller.start();
    model.refresh();
    CHECK(model.rows(ViewGroup::External)[0].hotkey == 3);
    CHECK(model.rows(ViewGroup::External)[0].keyBadge.empty());
}

TEST_CASE("ui model: the nine hotkey buttons are lit when a view has the key, highlighted when active") {
    Rig rig;
    const int a = rig.newFree();
    const int b = rig.newCockpit();
    rig.controller.setHotkey(a, 2);
    rig.controller.setHotkey(b, 9);
    rig.controller.renameView(a, "Behind");
    rig.controller.goToView(a);
    UiModel model(rig.controller);
    const auto& q = model.hotkeyButtons();
    REQUIRE(q.size() == 9);
    CHECK_FALSE(q[0].assigned);
    CHECK(q[0].tooltip == "Key 1 (free)");
    CHECK(q[0].viewId == 0);
    CHECK(q[1].assigned);
    CHECK(q[1].active);
    CHECK(q[1].viewId == a);
    CHECK(q[1].tooltip == "Behind");
    CHECK(q[8].assigned);
    CHECK_FALSE(q[8].active);
    CHECK(q[8].tooltip == "Cockpit view 1");
    CHECK(q[2].hotkey == 3);
    CHECK_FALSE(q[2].assigned);
}

TEST_CASE("ui model: quick bar left click goes to the hotkey view, right click saves into the hotkey") {
    Rig rig;
    UiModel model(rig.controller);
    rig.inFree(3.0);
    model.saveHotkey(5);  // right-click on a free key creates the view
    model.refresh();
    CHECK(model.hotkeyButtons()[4].assigned);
    CHECK(rig.fileStore().count() == 1);
    CHECK(rig.fileStore().get(5) != nullptr);
    model.goHotkey(5);
    CHECK(model.activeView() == model.hotkeyButtons()[4].viewId);
    const int active = model.activeView();
    model.goHotkey(6);  // free: nothing happens
    CHECK(model.activeView() == active);

    rig.inFree(8.0);
    model.saveHotkey(5);  // right-click on a lit key replaces the camera
    CHECK(rig.controller.slots().count() == 1);
    CHECK(rig.controller.slots().get(5)->x == doctest::Approx(8.0));
}

// ---------------------------------------------------------------- ui model: the quick bar group lists

TEST_CASE("quick bar: the group follows cockpit and outside views") {
    Rig rig;
    UiModel model(rig.controller);

    rig.inCockpit();
    model.updateBar();
    CHECK(model.barGroup() == ViewGroup::Internal);
    CHECK_FALSE(model.barManual());

    rig.inFree();
    model.updateBar();
    CHECK(model.barGroup() == ViewGroup::External);

    rig.inChase();
    model.updateBar();
    CHECK(model.barGroup() == ViewGroup::External);

    rig.inCircle();  // an outside view mykam cannot save is still an outside view
    model.updateBar();
    CHECK(model.barGroup() == ViewGroup::External);

    rig.inCockpit();
    model.updateBar();
    CHECK(model.barGroup() == ViewGroup::Internal);
}

TEST_CASE("quick bar: mykam holding or handing over an outside view shows External, animating a cockpit view Internal") {
    Rig rig;
    const int outside = rig.newFree(2.0);
    const int cockpit = rig.newCockpit(0.5);
    UiModel model(rig.controller);

    // Holding an outside camera while X-Plane still reports a cockpit view.
    rig.inCockpit();
    model.updateBar();
    REQUIRE(model.barGroup() == ViewGroup::Internal);
    model.go(outside);
    REQUIRE(rig.controller.state() == CameraState::Holding);
    model.updateBar();
    CHECK(model.barGroup() == ViewGroup::External);

    // The hand-over is requested on the next frame: still an outside view until it has run.
    Pose world;
    REQUIRE(rig.controller.tick(0.0, world));
    REQUIRE(rig.controller.handoffPending());
    model.updateBar();
    CHECK(model.barGroup() == ViewGroup::External);

    // A running cockpit head animation is a cockpit view, even if X-Plane still reports an outside one.
    REQUIRE(model.setTransitionSeconds(1.0) == ApplyResult::Saved);
    rig.inFree();
    model.updateBar();
    REQUIRE(model.barGroup() == ViewGroup::External);
    rig.sim.head = somePose(3.0);
    model.go(cockpit);
    REQUIRE(rig.controller.cockpitAnimating());
    model.updateBar();
    CHECK(model.barGroup() == ViewGroup::Internal);
}

TEST_CASE("quick bar: a manual choice is held until the view kind changes, then auto-follow resumes") {
    Rig rig;
    UiModel model(rig.controller);
    rig.inCockpit();
    model.updateBar();
    REQUIRE(model.barGroup() == ViewGroup::Internal);

    model.setBarGroup(ViewGroup::External);  // click on the External segment while in the cockpit
    CHECK(model.barGroup() == ViewGroup::External);
    CHECK(model.barManual());
    model.updateBar();  // same view kind: the choice holds, frame after frame
    model.updateBar();
    CHECK(model.barGroup() == ViewGroup::External);
    CHECK(model.barManual());

    rig.inFree();  // the view kind changed: auto-follow resumes
    model.updateBar();
    CHECK(model.barGroup() == ViewGroup::External);
    CHECK_FALSE(model.barManual());

    model.setBarGroup(ViewGroup::Internal);  // override while outside
    rig.inChase();                           // still outside: held
    model.updateBar();
    CHECK(model.barGroup() == ViewGroup::Internal);
    CHECK(model.barManual());
    rig.inCockpit();  // back in the cockpit: auto-follow shows Internal again
    model.updateBar();
    CHECK(model.barGroup() == ViewGroup::Internal);
    CHECK_FALSE(model.barManual());

    model.setBarGroup(ViewGroup::Internal);  // clicking the segment already shown changes nothing
    CHECK_FALSE(model.barManual());
}

TEST_CASE("quick bar: chips list only the starred views of the shown group, in group order, with the active flag") {
    Rig rig;
    const int e1 = rig.newFree(1.0);
    const int e2 = rig.newChase(2.0);
    const int e3 = rig.newFree(3.0);
    const int c1 = rig.newCockpit(0.1);
    rig.controller.renameView(e1, "A very long outside view name");
    rig.controller.setFavourite(e3, true);
    rig.controller.setFavourite(e1, true);
    rig.controller.setFavourite(e2, true);
    rig.controller.setHotkey(e2, 4);
    rig.controller.setHotkey(c1, 9);
    rig.controller.setFavourite(c1, true);
    rig.controller.goToView(e2);
    UiModel model(rig.controller);

    model.updateBar();  // the held chase camera is an outside view
    REQUIRE(model.barGroup() == ViewGroup::External);
    const auto& chips = model.barRows();
    REQUIRE(chips.size() == 3);
    CHECK(chips[0].id == e1);  // group order, not star order
    CHECK(chips[1].id == e2);
    CHECK(chips[2].id == e3);
    CHECK(chips[1].hotkey == 4);
    CHECK(chips[1].active);
    CHECK_FALSE(chips[0].active);
    CHECK_FALSE(chips[2].active);
    CHECK(chips[1].type == RowType::Chase);
    CHECK(chips[2].type == RowType::FreeCamera);

    // Names over 14 characters are cut with an ellipsis; short ones stay as they are.
    CHECK(chips[0].name == "A very long outside view name");
    CHECK(chips[0].shortName == "A very long...");
    CHECK(chips[0].shortName.size() == mykam::kChipNameChars);  // 11 characters + "..."
    CHECK(chips[2].shortName == chips[2].name);
    CHECK(UiModel::chipName("exactly14chars") == "exactly14chars");
    CHECK(UiModel::chipName("fifteen chars!!").size() == mykam::kChipNameChars);

    // A view that is not starred is not on the bar, but stays in the window list.
    rig.controller.setFavourite(e3, false);
    model.refresh();
    REQUIRE(model.barRows().size() == 2);
    CHECK(model.barRows()[0].id == e1);
    CHECK(model.barRows()[1].id == e2);
    CHECK(model.rowCount(ViewGroup::External) == 3);

    model.setBarGroup(ViewGroup::Internal);
    const auto& inside = model.barRows();
    REQUIRE(inside.size() == 1);
    CHECK(inside[0].id == c1);
    CHECK(inside[0].hotkey == 9);
    CHECK(inside[0].type == RowType::Cockpit);
    CHECK_FALSE(inside[0].active);
}

TEST_CASE("quick bar: the + button is enabled when the current view fits the shown group") {
    Rig rig;
    UiModel model(rig.controller);

    rig.inCockpit();
    model.updateBar();
    REQUIRE(model.barGroup() == ViewGroup::Internal);
    CHECK(model.canSaveToBar());
    model.setBarGroup(ViewGroup::External);  // showing the other group while in the cockpit
    CHECK_FALSE(model.canSaveToBar());
    CHECK(model.saveToBar() == 0);
    CHECK(rig.controller.slots().count() == 0);

    rig.inFree();
    model.updateBar();
    REQUIRE(model.barGroup() == ViewGroup::External);
    CHECK(model.canSaveToBar());
    model.setBarGroup(ViewGroup::Internal);
    CHECK_FALSE(model.canSaveToBar());

    rig.inCircle();  // saving is refused in circle view, whichever group is shown
    model.updateBar();
    model.setBarGroup(ViewGroup::External);
    CHECK_FALSE(model.canSaveToBar());
    model.setBarGroup(ViewGroup::Internal);
    CHECK_FALSE(model.canSaveToBar());

    rig.inCockpit();  // the view kind changes, so auto-follow takes over from the manual choice
    model.updateBar();
    rig.inFree(5.0);
    model.updateBar();
    REQUIRE(model.barGroup() == ViewGroup::External);
    const int id = model.saveToBar();  // saves into the shown group
    REQUIRE(id != 0);
    model.refresh();
    REQUIRE(model.barRows().size() == 1);
    CHECK(model.barRows()[0].id == id);
    CHECK(rig.view(id).kind == ViewKind::External);
}

TEST_CASE("quick bar: toggling the group switches it like a click on the other segment") {
    Rig rig;
    UiModel model(rig.controller);
    rig.inCockpit();
    // No frame was drawn yet: the toggle still starts from the current view side.
    model.toggleBarGroup();
    CHECK(model.barGroup() == ViewGroup::External);
    CHECK(model.barManual());
    model.toggleBarGroup();
    CHECK(model.barGroup() == ViewGroup::Internal);
    model.toggleBarGroup();
    CHECK(model.barGroup() == ViewGroup::External);
    model.updateBar();
    CHECK(model.barGroup() == ViewGroup::External);  // held while the view kind stays the same
    rig.inFree();
    model.updateBar();
    CHECK_FALSE(model.barManual());
    model.toggleBarGroup();
    CHECK(model.barGroup() == ViewGroup::Internal);
}

TEST_CASE("quick bar: an empty group has no chips and a group's views stay out of the other group") {
    Rig rig;
    UiModel model(rig.controller);
    rig.inCockpit();
    model.updateBar();
    CHECK(model.barRows().empty());
    CHECK(model.canSaveToBar());  // the + button still works in an empty group

    const int c = rig.newCockpit();
    rig.controller.setFavourite(c, true);  // only starred views are chips
    model.refresh();
    model.updateBar();
    REQUIRE(model.barRows().size() == 1);
    CHECK(model.barRows()[0].id == c);
    model.setBarGroup(ViewGroup::External);
    CHECK(model.barRows().empty());  // the cockpit view does not appear under External

    rig.controller.deleteView(c);
    model.refresh();
    model.setBarGroup(ViewGroup::Internal);
    CHECK(model.barRows().empty());
}

// ---------------------------------------------------------------- ui model: the save button

TEST_CASE("ui model: the save button is enabled for the group that fits the current X-Plane view") {
    Rig rig;
    UiModel model(rig.controller);

    rig.inCockpit();
    CHECK(model.canSaveCurrent(ViewGroup::Internal));
    CHECK_FALSE(model.canSaveCurrent(ViewGroup::External));

    rig.inFree();
    CHECK_FALSE(model.canSaveCurrent(ViewGroup::Internal));
    CHECK(model.canSaveCurrent(ViewGroup::External));

    rig.inChase();
    CHECK_FALSE(model.canSaveCurrent(ViewGroup::Internal));
    CHECK(model.canSaveCurrent(ViewGroup::External));

    const char* const noSave[] = {"sim/view/circle", "sim/view/tower", "sim/view/runway", "sim/view/linear_spot",
                                  "sim/view/still_spot", "sim/view/ridealong", "sim/view/wingman"};
    for (const char* view : noSave) {
        INFO(view);
        rig.sim.viewExternal = true;
        rig.controller.release();
        rig.controller.onViewSelected(view);
        CHECK_FALSE(model.canSaveCurrent(ViewGroup::Internal));
        CHECK_FALSE(model.canSaveCurrent(ViewGroup::External));
    }

    // While mykam holds an outside camera, that camera is what would be saved.
    rig.inFree();
    const int id = model.saveCurrent(ViewGroup::External);
    REQUIRE(id != 0);
    rig.controller.onViewSelected("sim/view/circle");
    model.go(id);
    REQUIRE(rig.controller.state() != CameraState::Idle);
    CHECK(model.canSaveCurrent(ViewGroup::External));
    CHECK_FALSE(model.canSaveCurrent(ViewGroup::Internal));
}

TEST_CASE("ui model: the disabled save button says which view to switch to") {
    const std::string internal = UiModel::saveDisabledReason(ViewGroup::Internal);
    const std::string external = UiModel::saveDisabledReason(ViewGroup::External);
    CHECK(internal.find("cockpit") != std::string::npos);
    CHECK(external.find("free camera") != std::string::npos);
    CHECK(external.find("chase") != std::string::npos);
}

TEST_CASE("ui model: saveCurrent saves only into the group that fits and changes nothing otherwise") {
    Rig rig;
    UiModel model(rig.controller);

    rig.inFree();
    rig.sim.logs.clear();
    const int writes = rig.sim.writeFileCalls;
    CHECK(model.saveCurrent(ViewGroup::Internal) == 0);  // a free camera is not a cockpit view
    CHECK(rig.sim.writeFileCalls == writes);
    CHECK(rig.sim.logs.empty());
    CHECK(rig.controller.slots().count() == 0);

    const int ext = model.saveCurrent(ViewGroup::External);
    REQUIRE(ext != 0);
    CHECK(rig.view(ext).kind == ViewKind::External);

    rig.inCockpit();
    CHECK(model.saveCurrent(ViewGroup::External) == 0);
    const int in = model.saveCurrent(ViewGroup::Internal);
    REQUIRE(in != 0);
    CHECK(rig.view(in).kind == ViewKind::Cockpit);
    model.refresh();
    CHECK(model.rowCount(ViewGroup::Internal) == 1);
    CHECK(model.rowCount(ViewGroup::External) == 1);

    rig.inCircle();
    CHECK(model.saveCurrent(ViewGroup::External) == 0);
    CHECK(model.saveCurrent(ViewGroup::Internal) == 0);
    CHECK(rig.controller.slots().count() == 2);
}

// ---------------------------------------------------------------- ui model: row actions

TEST_CASE("ui model: rename, favourite, replace and delete go through to the controller") {
    Rig rig;
    const int id = rig.newFree(1.0);
    UiModel model(rig.controller);
    CHECK(model.rename(id, "Wing") == RenameResult::Renamed);
    CHECK(std::string(UiModel::renameProblem(RenameResult::Renamed)).empty());
    CHECK(std::string(UiModel::renameProblem(RenameResult::Reset)).empty());
    CHECK_FALSE(std::string(UiModel::renameProblem(RenameResult::TooLong)).empty());
    CHECK_FALSE(std::string(UiModel::renameProblem(RenameResult::NotPrintable)).empty());
    CHECK_FALSE(std::string(UiModel::renameProblem(RenameResult::UnknownView)).empty());
    CHECK(model.rename(id, std::string(40, 'a')) == RenameResult::TooLong);

    CHECK(model.toggleFavourite(id));
    model.refresh();
    CHECK(model.rows(ViewGroup::External)[0].favourite);
    CHECK(model.toggleFavourite(id));
    model.refresh();
    CHECK_FALSE(model.rows(ViewGroup::External)[0].favourite);
    CHECK_FALSE(model.toggleFavourite(99));

    rig.inFree(6.0);
    CHECK(model.replace(id));
    CHECK(rig.view(id).pose.x == doctest::Approx(6.0));
    rig.inCockpit();
    CHECK_FALSE(model.replace(id));  // a cockpit view does not fit an external row

    CHECK(model.remove(id));
    model.refresh();
    CHECK(model.rows(ViewGroup::External).empty());
    CHECK_FALSE(model.anySaved());
}

TEST_CASE("ui model: assigning a key shows who has it and moves it") {
    Rig rig;
    const int a = rig.newFree();
    const int b = rig.newChase();
    rig.controller.renameView(a, "Wing");
    UiModel model(rig.controller);

    CHECK(model.keyOwnerName(4, b).empty());  // free
    CHECK(model.assignKey(a, 4));
    CHECK(model.keyOwnerName(4, b) == "Wing");  // used by another view
    CHECK(model.keyOwnerName(4, a).empty());    // it is this view's own key
    CHECK(model.assignKey(b, 4));               // chosen anyway: it moves
    CHECK(rig.view(a).hotkey == 0);
    CHECK(rig.view(b).hotkey == 4);
    CHECK(model.keyOwnerName(4, a) == "Outside view 2");

    model.refresh();
    CHECK(model.hotkeyButtons()[3].viewId == b);
    CHECK(model.assignKey(b, 0));  // None
    CHECK(model.keyOwnerName(4, a).empty());
    CHECK_FALSE(model.assignKey(b, 11));
}

TEST_CASE("ui model: next and previous follow the group order") {
    Rig rig;
    const int e = rig.newFree();
    const int i = rig.newCockpit();
    rig.controller.release();
    UiModel model(rig.controller);
    model.next();
    CHECK(model.activeView() == i);
    model.next();
    CHECK(model.activeView() == e);
    model.next();
    CHECK(model.activeView() == i);
    model.previous();
    CHECK(model.activeView() == e);
}

// ---------------------------------------------------------------- ui model: settings and X-Plane views

TEST_CASE("ui model: settings are validated, rounded to 0.1 s and saved") {
    Rig rig;
    UiModel model(rig.controller);
    CHECK(model.setTransitionSeconds(2.34) == ApplyResult::Saved);
    CHECK(model.settings().transitionSeconds == doctest::Approx(2.3));
    CHECK(model.setTransitionSeconds(10.0) == ApplyResult::Saved);
    CHECK(model.setTransitionSeconds(0.0) == ApplyResult::Saved);
    CHECK(model.setTransitionSeconds(10.04) == ApplyResult::Saved);    // rounds to 10.0
    CHECK(model.setTransitionSeconds(10.06) == ApplyResult::Invalid);  // rounds to 10.1
    CHECK(model.setTransitionSeconds(-1.0) == ApplyResult::Invalid);
    CHECK(model.setTransitionSeconds(std::numeric_limits<double>::quiet_NaN()) == ApplyResult::Invalid);
    CHECK(model.settings().transitionSeconds == doctest::Approx(10.0));

    CHECK(model.setHandoffView(HandoffView::Free) == ApplyResult::Saved);
    CHECK(model.setShowQuickBar(false) == ApplyResult::Saved);
    CHECK(model.setQuickBarPosition(QuickBarPosition::Top) == ApplyResult::Saved);
    CHECK_FALSE(model.quickBarVisible());
    CHECK(model.quickBarAtTop());

    const mykam::ConfigResult back = mykam::parseConfig(rig.sim.files[kIniPath]);
    CHECK(back.warning.empty());
    CHECK(back.config.transitionSeconds == doctest::Approx(10.0));
    CHECK(back.config.handoffView == HandoffView::Free);
    CHECK_FALSE(back.config.showQuickBar);
    CHECK(back.config.quickBarPosition == QuickBarPosition::Top);
}

TEST_CASE("ui model: the X-Plane view buttons are the ten expected views, all on the tracked list") {
    REQUIRE(mykam::kXPlaneViewButtonCount == 10);
    const char* const expected[] = {"sim/view/3d_cockpit_cmnd_look", "sim/view/free_camera", "sim/view/chase",
                                    "sim/view/circle",               "sim/view/tower",       "sim/view/runway",
                                    "sim/view/linear_spot",          "sim/view/still_spot",  "sim/view/ridealong",
                                    "sim/view/wingman"};
    for (std::size_t i = 0; i < mykam::kXPlaneViewButtonCount; ++i) {
        INFO(mykam::kXPlaneViewButtons[i].command);
        CHECK(std::string(mykam::kXPlaneViewButtons[i].command) == expected[i]);
        CHECK(mykam::isTrackedViewCommand(mykam::kXPlaneViewButtons[i].command));
        CHECK(std::string(mykam::kXPlaneViewButtons[i].label).size() > 0);
    }
    CHECK(std::string(mykam::kXPlaneViewButtons[mykam::kQuickViewCockpit].command) == "sim/view/3d_cockpit_cmnd_look");
    CHECK(std::string(mykam::kXPlaneViewButtons[mykam::kQuickViewFree].command) == "sim/view/free_camera");
    CHECK(std::string(mykam::kXPlaneViewButtons[mykam::kQuickViewChase].command) == "sim/view/chase");

    Rig rig;
    UiModel model(rig.controller);
    CHECK(model.selectXPlaneView(2));
    CHECK(rig.sim.viewCommandsRun.back() == "sim/view/chase");
    CHECK_FALSE(model.selectXPlaneView(mykam::kXPlaneViewButtonCount));
}

// ---------------------------------------------------------------- quick bar: the X-Plane view buttons

namespace {

std::vector<std::string> buttonNames(const UiModel& model) {
    std::vector<std::string> names;
    for (const mykam::QuickViewButton& b : model.quickViewButtons()) {
        names.push_back(b.name);
    }
    return names;
}

using Names = std::vector<std::string>;

}  // namespace

TEST_CASE("quick view names: ten names, each the command of the X-Plane views page tile at the same place") {
    REQUIRE(mykam::kQuickViewNameCount == mykam::kXPlaneViewButtonCount);
    for (std::size_t i = 0; i < mykam::kQuickViewNameCount; ++i) {
        INFO(mykam::kQuickViewNames[i].name);
        CHECK(std::string(mykam::kQuickViewNames[i].command) == mykam::kXPlaneViewButtons[i].command);
        CHECK(mykam::quickViewIndex(mykam::kQuickViewNames[i].name) == static_cast<int>(i));
        CHECK(mykam::isTrackedViewCommand(mykam::kQuickViewNames[i].command));
    }
    CHECK(std::string(mykam::kQuickViewNames[0].name) == "cockpit");
    CHECK(std::string(mykam::kQuickViewNames[0].command) == "sim/view/3d_cockpit_cmnd_look");
    CHECK(mykam::quickViewIndex("nope") == -1);
    CHECK(mykam::quickViewIndex("Chase") == -1);  // names are lowercase
    CHECK(mykam::quickViewIndex("") == -1);
}

TEST_CASE("quick bar views: by default cockpit, free and chase, with their full names and commands") {
    Rig rig;
    UiModel model(rig.controller);
    const std::vector<mykam::QuickViewButton>& buttons = model.quickViewButtons();
    REQUIRE(buttons.size() == 3);
    CHECK(buttons[0].name == "cockpit");
    CHECK(std::string(buttons[0].label) == "3D cockpit");
    CHECK(std::string(buttons[0].command) == "sim/view/3d_cockpit_cmnd_look");
    CHECK(buttons[0].index == mykam::kQuickViewCockpit);
    CHECK(buttons[1].name == "free");
    CHECK(std::string(buttons[1].label) == "Free camera");
    CHECK(buttons[2].name == "chase");
    CHECK(std::string(buttons[2].label) == "Chase");
    CHECK(std::string(buttons[2].command) == "sim/view/chase");
    for (const mykam::QuickViewButton& b : buttons) {
        CHECK_FALSE(b.active);  // no X-Plane view chosen yet
    }
}

TEST_CASE("quick bar views: the button of the active X-Plane view is marked, and none while a saved view is active") {
    Rig rig;
    const int saved = rig.newFree();
    UiModel model(rig.controller);
    REQUIRE(model.setQuickBarViews({"chase", "tower", "cockpit"}) == ApplyResult::Saved);

    REQUIRE(model.selectXPlaneView(mykam::kQuickViewChase));
    {
        const std::vector<mykam::QuickViewButton>& b = model.quickViewButtons();
        REQUIRE(b.size() == 3);
        CHECK(b[0].active);
        CHECK_FALSE(b[1].active);
        CHECK_FALSE(b[2].active);
    }
    REQUIRE(model.selectXPlaneView(4));  // tower
    {
        const std::vector<mykam::QuickViewButton>& b = model.quickViewButtons();
        CHECK_FALSE(b[0].active);
        CHECK(b[1].active);
    }
    // Going to a saved view hands the camera to mykam: no X-Plane button is active.
    model.go(saved);
    REQUIRE(model.activeView() == saved);
    for (const mykam::QuickViewButton& b : model.quickViewButtons()) {
        CHECK_FALSE(b.active);
    }
    // Choosing an X-Plane view again marks it.
    REQUIRE(model.selectXPlaneView(mykam::kQuickViewCockpit));
    CHECK(model.quickViewButtons()[2].active);
}

TEST_CASE("quick bar views: settings apply at once, keep the order, and are written to mykam.ini") {
    Rig rig;
    UiModel model(rig.controller);
    const int writes = rig.sim.writeFileCalls;
    CHECK(model.setQuickBarViews({"wingman", "cockpit", "runway"}) == ApplyResult::Saved);
    CHECK(rig.sim.writeFileCalls == writes + 1);
    CHECK(buttonNames(model) == Names({"wingman", "cockpit", "runway"}));
    CHECK(model.settings().quickBarViews == Names({"wingman", "cockpit", "runway"}));
    CHECK(rig.sim.files[kIniPath].find("quick_bar_views = wingman,cockpit,runway\n") != std::string::npos);
    CHECK(rig.sim.files[kIniPath].find("transition_seconds = 0") != std::string::npos);  // other lines stay

    const mykam::ConfigResult back = mykam::parseConfig(rig.sim.files[kIniPath]);
    CHECK(back.warning.empty());
    CHECK(back.config.quickBarViews == Names({"wingman", "cockpit", "runway"}));

    // An empty list is allowed: no X-Plane view buttons, and the file says so.
    CHECK(model.setQuickBarViews({}) == ApplyResult::Saved);
    CHECK(model.quickViewButtons().empty());
    CHECK(rig.sim.files[kIniPath].find("quick_bar_views =\n") != std::string::npos);
    CHECK(mykam::parseConfig(rig.sim.files[kIniPath]).config.quickBarViews.empty());
}

TEST_CASE("quick bar views: unknown or repeated names are refused and change nothing") {
    Rig rig;
    UiModel model(rig.controller);
    const int writes = rig.sim.writeFileCalls;
    CHECK(model.setQuickBarViews({"chase", "nope"}) == ApplyResult::Invalid);
    CHECK(model.setQuickBarViews({"chase", "tower", "chase"}) == ApplyResult::Invalid);
    CHECK(model.toggleQuickBarView("nope") == ApplyResult::Invalid);
    CHECK(model.moveQuickBarView("tower", 1) == ApplyResult::Invalid);   // not shown
    CHECK(model.moveQuickBarView("cockpit", -1) == ApplyResult::Invalid);  // already first
    CHECK(model.moveQuickBarView("chase", 1) == ApplyResult::Invalid);     // already last
    CHECK(model.moveQuickBarView("free", 2) == ApplyResult::Invalid);      // only one place at a time
    CHECK(rig.sim.writeFileCalls == writes);
    CHECK(buttonNames(model) == Names({"cockpit", "free", "chase"}));
}

TEST_CASE("quick bar views: a toggle adds a view at the end and removes it again") {
    Rig rig;
    UiModel model(rig.controller);
    CHECK(model.quickViewShown("chase"));
    CHECK_FALSE(model.quickViewShown("circle"));

    CHECK(model.toggleQuickBarView("circle") == ApplyResult::Saved);
    CHECK(buttonNames(model) == Names({"cockpit", "free", "chase", "circle"}));
    CHECK(model.quickViewShown("circle"));
    CHECK(rig.sim.files[kIniPath].find("quick_bar_views = cockpit,free,chase,circle\n") != std::string::npos);

    CHECK(model.toggleQuickBarView("free") == ApplyResult::Saved);
    CHECK(buttonNames(model) == Names({"cockpit", "chase", "circle"}));
    CHECK_FALSE(model.quickViewShown("free"));

    CHECK(model.toggleQuickBarView("free") == ApplyResult::Saved);  // back, at the end now
    CHECK(buttonNames(model) == Names({"cockpit", "chase", "circle", "free"}));

    // Taking every view away leaves an empty bar section; release and the menu button are not in the list.
    for (const std::string& name : Names({"cockpit", "chase", "circle", "free"})) {
        CHECK(model.toggleQuickBarView(name) == ApplyResult::Saved);
    }
    CHECK(model.quickViewButtons().empty());
}

TEST_CASE("quick bar views: up and down move a view one place and the file follows") {
    Rig rig;
    UiModel model(rig.controller);
    CHECK(model.moveQuickBarView("chase", -1) == ApplyResult::Saved);
    CHECK(buttonNames(model) == Names({"cockpit", "chase", "free"}));
    CHECK(model.moveQuickBarView("chase", -1) == ApplyResult::Saved);
    CHECK(buttonNames(model) == Names({"chase", "cockpit", "free"}));
    CHECK(model.moveQuickBarView("cockpit", 1) == ApplyResult::Saved);
    CHECK(buttonNames(model) == Names({"chase", "free", "cockpit"}));
    CHECK(rig.sim.files[kIniPath].find("quick_bar_views = chase,free,cockpit\n") != std::string::npos);
    CHECK(mykam::parseConfig(rig.sim.files[kIniPath]).config.quickBarViews == Names({"chase", "free", "cockpit"}));
}

TEST_CASE("quick bar views: a file that cannot be written still applies the change and reports NotSaved") {
    Rig rig;
    UiModel model(rig.controller);
    rig.sim.failWrites = true;
    CHECK(model.toggleQuickBarView("tower") == ApplyResult::NotSaved);
    CHECK(buttonNames(model) == Names({"cockpit", "free", "chase", "tower"}));
}

TEST_CASE("quick bar views: a saved mykam.ini decides the buttons at start, unknown names ignored") {
    FakeSim sim;
    sim.files[kIniPath] = "transition_seconds = 0\nquick_bar_views = circle, bogus, wingman, circle\n";
    Controller controller(sim, kFolder);
    controller.start();
    UiModel model(controller);
    CHECK(buttonNames(model) == Names({"circle", "wingman"}));
    CHECK(sim.logCount("quick_bar_views") == 1);  // one warning line for the unknown name
}
