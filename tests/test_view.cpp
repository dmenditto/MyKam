#include "doctest.h"

#include <cmath>
#include <string>

#include "controller.h"
#include "fake_sim.h"
#include "slot_store.h"
#include "view_control.h"

using mykam::AircraftState;
using mykam::CameraState;
using mykam::CommandPhase;
using mykam::Controller;
using mykam::Pose;
using mykam::SlotKind;
using mykam::ViewAction;
using mykam::ViewMotion;
using mykam::ViewSpeed;

namespace {

const char* const kCessnaFile = "plug/cameras/Cessna_172SP.slots.txt";

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

struct Rig {
    FakeSim sim;
    Controller controller;

    explicit Rig(const std::string& seconds) : controller(sim, "plug") {
        sim.files["plug/mykam.ini"] = "transition_seconds = " + seconds + "\n";
        controller.start();
        controller.onAircraftLoaded("Cessna_172SP.acf");
        sim.logs.clear();
    }

    // Save an external slot from the given world view, aircraft at the origin looking north.
    void saveExternal(int slot, const Pose& view) {
        sim.viewExternal = true;
        sim.view = view;
        controller.saveSlot(slot);
    }
    // Save a cockpit slot from the given head values.
    void saveCockpit(int slot, const Pose& head) {
        sim.viewExternal = false;
        sim.head = head;
        controller.saveSlot(slot);
    }
};

}  // namespace

// ---------------------------------------------------------------- cockpit slots

TEST_CASE("cockpit: saving in the internal view stores the pilot head values and the kind") {
    Rig rig("1");
    rig.saveCockpit(1, makePose(0.25, 0.5, -0.75, -7.0, 20.0, 2.0, 1.0));

    REQUIRE(rig.controller.slots().has(1));
    CHECK(rig.controller.slots().kind(1) == SlotKind::Cockpit);
    const Pose* p = rig.controller.slots().get(1);
    CHECK(p->x == doctest::Approx(0.25));
    CHECK(p->y == doctest::Approx(0.5));
    CHECK(p->z == doctest::Approx(-0.75));
    CHECK(p->pitch == doctest::Approx(-7.0));
    CHECK(p->heading == doctest::Approx(20.0));
    CHECK(p->roll == doctest::Approx(2.0));
    CHECK(rig.sim.readViewCalls == 0);  // the head datarefs are read, not the camera
    CHECK(rig.sim.takeCalls == 0);
    CHECK(rig.sim.files[kCessnaFile].find("view1.kind=cockpit") != std::string::npos);
    CHECK(rig.sim.logCount("cockpit view") == 1);
}

TEST_CASE("cockpit: saving in the external view stores an external slot") {
    Rig rig("1");
    rig.saveExternal(1, makePose(10, 5, 0, 0, 0, 0, 1.0));
    CHECK(rig.controller.slots().kind(1) == SlotKind::External);
    CHECK(rig.sim.files[kCessnaFile].find("view1.kind=external") != std::string::npos);
    CHECK(rig.sim.files[kCessnaFile].find("kind=cockpit") == std::string::npos);
}

TEST_CASE("cockpit: going to a cockpit slot never takes the camera and writes head values over the transition") {
    Rig rig("2");
    rig.saveCockpit(1, makePose(0.4, 0.2, 0.0, 10.0, 90.0, 0.0, 1.0));
    rig.sim.head = makePose(0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0);  // the pilot looks elsewhere now
    rig.sim.viewExternal = false;

    rig.controller.goToSlot(1);
    CHECK(rig.sim.takeCalls == 0);
    CHECK(rig.controller.state() == CameraState::Idle);
    CHECK(rig.sim.cockpitFrameRequests == 1);
    CHECK(rig.sim.cockpitCommandCalls == 0);  // already in the cockpit
    CHECK(rig.controller.cockpitAnimating());

    REQUIRE(rig.controller.tickCockpit(0.0));  // first frame: still at the start
    CHECK(rig.sim.head.x == doctest::Approx(0.0));
    REQUIRE(rig.controller.tickCockpit(1.0));  // halfway, eased 0.5
    CHECK(rig.sim.head.x == doctest::Approx(0.2));
    CHECK(rig.sim.head.y == doctest::Approx(0.1));
    CHECK(rig.sim.head.pitch == doctest::Approx(5.0));
    CHECK(rig.sim.head.heading == doctest::Approx(45.0));
    CHECK_FALSE(rig.controller.tickCockpit(1.5));  // past the end: finished
    CHECK(rig.sim.head.x == doctest::Approx(0.4));
    CHECK(rig.sim.head.heading == doctest::Approx(90.0));
    CHECK_FALSE(rig.controller.cockpitAnimating());

    // Done: nothing is held and nothing more is written.
    const std::size_t writes = rig.sim.headWrites.size();
    CHECK_FALSE(rig.controller.tickCockpit(0.016));
    CHECK(rig.sim.headWrites.size() == writes);
    CHECK(rig.sim.takeCalls == 0);
    CHECK(rig.sim.releaseCalls == 0);
}

TEST_CASE("cockpit: the head turns the short way round and is written as -180 to 180") {
    Rig rig("2");
    rig.saveCockpit(1, makePose(0, 0, 0, 0, 350.0, 0, 1.0));
    rig.sim.head = makePose(0, 0, 0, 0, 10.0, 0, 1.0);
    rig.controller.goToSlot(1);
    REQUIRE(rig.controller.tickCockpit(1.0));  // halfway between 10 and 350 over the short way: 0
    CHECK(std::fabs(rig.sim.head.heading) < 1e-6);
    CHECK_FALSE(rig.controller.tickCockpit(2.0));
    CHECK(rig.sim.head.heading == doctest::Approx(-10.0));
}

TEST_CASE("cockpit: from an external view the X-Plane cockpit look command runs first") {
    Rig rig("1");
    rig.saveCockpit(1, makePose(0.1, 0.1, 0.1, 0, 0, 0, 1.0));
    rig.sim.viewExternal = true;

    rig.controller.goToSlot(1);
    CHECK(rig.sim.cockpitCommandCalls == 1);
    CHECK_FALSE(rig.sim.viewExternal);
    CHECK(rig.sim.takeCalls == 0);
    CHECK(rig.controller.cockpitAnimating());
}

TEST_CASE("cockpit: if the cockpit look command cannot run, nothing moves and one warning is logged") {
    Rig rig("1");
    rig.saveCockpit(1, makePose(0.1, 0.1, 0.1, 0, 0, 0, 1.0));
    rig.sim.viewExternal = true;
    rig.sim.cockpitCommandWorks = false;
    rig.sim.logs.clear();

    rig.controller.goToSlot(1);
    CHECK(rig.sim.headWrites.empty());
    CHECK(rig.sim.cockpitFrameRequests == 0);
    CHECK_FALSE(rig.controller.cockpitAnimating());
    CHECK(rig.sim.logCount("warning") == 1);
}

TEST_CASE("cockpit: a zero transition time writes the head at once without asking for frames") {
    Rig rig("0");
    rig.saveCockpit(1, makePose(0.3, 0.0, 0.0, 0, 0, 0, 1.0));
    rig.sim.head = makePose(0, 0, 0, 0, 0, 0, 1.0);
    rig.controller.goToSlot(1);
    CHECK(rig.sim.head.x == doctest::Approx(0.3));
    CHECK(rig.sim.cockpitFrameRequests == 0);
    CHECK_FALSE(rig.controller.cockpitAnimating());
}

TEST_CASE("cockpit: going to a cockpit slot while holding an external camera gives the camera back") {
    Rig rig("0");
    rig.saveCockpit(1, makePose(0.1, 0.1, 0.1, 0, 0, 0, 1.0));
    rig.saveExternal(2, makePose(10, 5, 0, 0, 0, 0, 1.0));
    rig.controller.goToSlot(2);
    REQUIRE(rig.controller.state() == CameraState::Holding);
    REQUIRE(rig.sim.takeCalls == 1);

    rig.controller.goToSlot(1);
    CHECK(rig.controller.state() == CameraState::Idle);
    CHECK(rig.sim.releaseCalls == 1);
    CHECK(rig.sim.takeCalls == 1);  // no new take
    Pose out;
    CHECK_FALSE(rig.controller.tick(0.016, out));
}

TEST_CASE("cockpit: going to an external slot during a head animation ends the animation") {
    Rig rig("2");
    rig.saveCockpit(1, makePose(0.5, 0, 0, 0, 0, 0, 1.0));
    rig.saveExternal(2, makePose(10, 5, 0, 0, 0, 0, 1.0));
    rig.sim.head = makePose(0, 0, 0, 0, 0, 0, 1.0);
    rig.sim.viewExternal = false;
    rig.controller.goToSlot(1);
    REQUIRE(rig.controller.cockpitAnimating());

    rig.controller.goToSlot(2);
    CHECK_FALSE(rig.controller.cockpitAnimating());
    CHECK_FALSE(rig.controller.tickCockpit(0.5));
    CHECK(rig.sim.takeCalls == 1);
}

TEST_CASE("cockpit: release and an aircraft change stop a head animation") {
    Rig rig("2");
    rig.saveCockpit(1, makePose(0.5, 0, 0, 0, 0, 0, 1.0));
    rig.sim.head = makePose(0, 0, 0, 0, 0, 0, 1.0);
    rig.controller.goToSlot(1);
    REQUIRE(rig.controller.cockpitAnimating());
    rig.controller.release();
    CHECK_FALSE(rig.controller.tickCockpit(0.5));

    rig.controller.goToSlot(1);
    REQUIRE(rig.controller.cockpitAnimating());
    rig.controller.onAircraftLoaded("Baron_58.acf");
    CHECK_FALSE(rig.controller.tickCockpit(0.5));
}

TEST_CASE("cockpit: another plugin holding the camera blocks go to a cockpit slot too") {
    Rig rig("1");
    rig.saveCockpit(1, makePose(0.1, 0, 0, 0, 0, 0, 1.0));
    rig.sim.heldByOther = true;
    rig.sim.logs.clear();
    rig.controller.goToSlot(1);
    CHECK(rig.sim.headWrites.empty());
    CHECK(rig.sim.cockpitCommandCalls == 0);
    CHECK(rig.sim.logCount("another plugin") == 1);
}

TEST_CASE("cockpit: the head animation does no file access and writes no log") {
    Rig rig("1");
    rig.saveCockpit(1, makePose(0.5, 0, 0, 0, 0, 0, 1.0));
    rig.sim.head = makePose(0, 0, 0, 0, 0, 0, 1.0);
    rig.controller.goToSlot(1);
    rig.sim.logs.clear();
    const int ops = rig.sim.fileOps();
    for (int i = 0; i < 100; ++i) {
        rig.controller.tickCockpit(0.016);
    }
    CHECK(rig.sim.fileOps() == ops);
    CHECK(rig.sim.logs.empty());
}

TEST_CASE("slot file: a legacy file without kind loads as an external slot that takes the camera") {
    Rig rig("0");
    rig.sim.files[kCessnaFile] =
        "mykam_slots=1\nslot1.x=3\nslot1.y=2\nslot1.z=1\nslot1.pitch=0\nslot1.heading=0\nslot1.roll=0\nslot1.zoom=1\n";
    rig.controller.onAircraftLoaded("Cessna_172SP.acf");
    REQUIRE(rig.controller.slots().has(1));
    CHECK(rig.controller.slots().kind(1) == SlotKind::External);

    rig.sim.viewExternal = false;  // even from the cockpit, an external slot takes the camera
    rig.controller.goToSlot(1);
    CHECK(rig.sim.takeCalls == 1);
    CHECK(rig.controller.state() == CameraState::Holding);
    CHECK(rig.sim.cockpitCommandCalls == 0);
}

TEST_CASE("slot file: a cockpit slot survives a reload with its kind") {
    Rig rig("0");
    rig.saveCockpit(3, makePose(0.1, 0.2, 0.3, 1.0, 2.0, 3.0, 1.0));
    rig.controller.onAircraftLoaded("Cessna_172SP.acf");
    REQUIRE(rig.controller.slots().has(3));
    CHECK(rig.controller.slots().kind(3) == SlotKind::Cockpit);
    CHECK(rig.controller.slots().get(3)->heading == doctest::Approx(2.0));
}

// ---------------------------------------------------------------- external hold: view commands

TEST_CASE("external hold: move commands move the held camera in its own frame") {
    Rig rig("0");
    rig.saveExternal(1, makePose(0, 0, 0, 0, 0, 0, 1.0));  // looking north: forward is -z
    rig.controller.goToSlot(1);
    REQUIRE(rig.controller.state() == CameraState::Holding);

    CHECK(rig.controller.onViewCommand(ViewAction::Right, ViewSpeed::Normal, CommandPhase::Begin));
    Pose out;
    REQUIRE(rig.controller.tick(1.0, out));
    CHECK(out.x == doctest::Approx(mykam::kMoveMetresPerSecond));
    CHECK(rig.controller.onViewCommand(ViewAction::Right, ViewSpeed::Normal, CommandPhase::Continue));
    REQUIRE(rig.controller.tick(1.0, out));
    CHECK(out.x == doctest::Approx(2.0 * mykam::kMoveMetresPerSecond));
    CHECK(rig.controller.onViewCommand(ViewAction::Right, ViewSpeed::Normal, CommandPhase::End));

    REQUIRE(rig.controller.tick(1.0, out));  // released: no more movement
    CHECK(out.x == doctest::Approx(2.0 * mykam::kMoveMetresPerSecond));

    CHECK(rig.controller.onViewCommand(ViewAction::Forward, ViewSpeed::Normal, CommandPhase::Begin));
    REQUIRE(rig.controller.tick(1.0, out));
    CHECK(out.z == doctest::Approx(-mykam::kMoveMetresPerSecond));  // north is -z
    CHECK(rig.controller.onViewCommand(ViewAction::Forward, ViewSpeed::Normal, CommandPhase::End));
}

TEST_CASE("external hold: rotate and zoom commands turn and zoom the held camera") {
    Rig rig("0");
    rig.saveExternal(1, makePose(0, 0, 0, 0, 0, 0, 1.0));
    rig.controller.goToSlot(1);
    Pose out;

    CHECK(rig.controller.onViewCommand(ViewAction::RotRight, ViewSpeed::Normal, CommandPhase::Begin));
    REQUIRE(rig.controller.tick(0.5, out));
    CHECK(out.heading == doctest::Approx(0.5 * mykam::kTurnDegreesPerSecond));
    rig.controller.onViewCommand(ViewAction::RotRight, ViewSpeed::Normal, CommandPhase::End);

    CHECK(rig.controller.onViewCommand(ViewAction::RotUp, ViewSpeed::Normal, CommandPhase::Begin));
    REQUIRE(rig.controller.tick(0.5, out));
    CHECK(out.pitch == doctest::Approx(0.5 * mykam::kTurnDegreesPerSecond));
    rig.controller.onViewCommand(ViewAction::RotUp, ViewSpeed::Normal, CommandPhase::End);

    CHECK(rig.controller.onViewCommand(ViewAction::ZoomIn, ViewSpeed::Normal, CommandPhase::Begin));
    REQUIRE(rig.controller.tick(1.0, out));
    CHECK(out.zoom == doctest::Approx(std::pow(2.0, mykam::kZoomDoublingsPerSecond)));
    rig.controller.onViewCommand(ViewAction::ZoomIn, ViewSpeed::Normal, CommandPhase::End);
}

TEST_CASE("external hold: fast and slow variants scale the rate") {
    Rig rig("0");
    rig.saveExternal(1, makePose(0, 0, 0, 0, 0, 0, 1.0));
    rig.controller.goToSlot(1);
    Pose out;
    rig.controller.onViewCommand(ViewAction::Right, ViewSpeed::Fast, CommandPhase::Begin);
    REQUIRE(rig.controller.tick(1.0, out));
    CHECK(out.x == doctest::Approx(mykam::kMoveMetresPerSecond * mykam::kFastFactor));
    rig.controller.onViewCommand(ViewAction::Right, ViewSpeed::Fast, CommandPhase::End);

    rig.controller.onViewCommand(ViewAction::Left, ViewSpeed::Slow, CommandPhase::Begin);
    REQUIRE(rig.controller.tick(1.0, out));
    CHECK(out.x == doctest::Approx(mykam::kMoveMetresPerSecond * (mykam::kFastFactor - mykam::kSlowFactor)));
}

TEST_CASE("external hold: movement is applied in the aircraft frame so the camera follows the aircraft") {
    Rig rig("0");
    rig.sim.aircraft.heading = 90.0;  // east
    rig.saveExternal(1, makePose(0, 0, 0, 0, 90.0, 0, 1.0));
    rig.controller.goToSlot(1);
    rig.controller.onViewCommand(ViewAction::Right, ViewSpeed::Normal, CommandPhase::Begin);
    Pose out;
    REQUIRE(rig.controller.tick(1.0, out));
    rig.controller.onViewCommand(ViewAction::Right, ViewSpeed::Normal, CommandPhase::End);
    // Facing along the aircraft nose (east), right of the camera is south (+z).
    CHECK(out.x == doctest::Approx(0.0).epsilon(1e-6));
    CHECK(out.z == doctest::Approx(mykam::kMoveMetresPerSecond));

    // The aircraft flies on: the adjusted offset goes with it.
    rig.sim.aircraft.x = 1000.0;
    REQUIRE(rig.controller.tick(0.0, out));
    CHECK(out.x == doctest::Approx(1000.0));
    CHECK(out.z == doctest::Approx(mykam::kMoveMetresPerSecond));
}

TEST_CASE("external hold: commands pass through when nothing is held") {
    Rig rig("0");
    // Idle.
    for (CommandPhase ph : {CommandPhase::Begin, CommandPhase::Continue, CommandPhase::End}) {
        CHECK_FALSE(rig.controller.onViewCommand(ViewAction::Left, ViewSpeed::Normal, ph));
    }
    // After a release.
    rig.saveExternal(1, makePose(0, 0, 0, 0, 0, 0, 1.0));
    rig.controller.goToSlot(1);
    rig.controller.release();
    CHECK_FALSE(rig.controller.onViewCommand(ViewAction::Left, ViewSpeed::Normal, CommandPhase::Begin));

    // While a cockpit slot is animating we hold nothing either.
    rig.saveCockpit(2, makePose(0.1, 0, 0, 0, 0, 0, 1.0));
    rig.controller.goToSlot(2);
    CHECK_FALSE(rig.controller.onViewCommand(ViewAction::Left, ViewSpeed::Normal, CommandPhase::Begin));
}

TEST_CASE("external hold: a command that began before we held the camera is left to X-Plane") {
    Rig rig("0");
    rig.saveExternal(1, makePose(0, 0, 0, 0, 0, 0, 1.0));
    rig.controller.goToSlot(1);
    // We never saw the Begin, so the Continue and End belong to X-Plane.
    CHECK_FALSE(rig.controller.onViewCommand(ViewAction::Left, ViewSpeed::Normal, CommandPhase::Continue));
    CHECK_FALSE(rig.controller.onViewCommand(ViewAction::Left, ViewSpeed::Normal, CommandPhase::End));
}

TEST_CASE("external hold: a release or camera loss forgets held commands") {
    Rig rig("0");
    rig.saveExternal(1, makePose(0, 0, 0, 0, 0, 0, 1.0));
    rig.controller.goToSlot(1);
    rig.controller.onViewCommand(ViewAction::Left, ViewSpeed::Normal, CommandPhase::Begin);
    rig.controller.onCameraLost();
    CHECK_FALSE(rig.controller.onViewCommand(ViewAction::Left, ViewSpeed::Normal, CommandPhase::Continue));

    rig.controller.goToSlot(1);
    rig.controller.onViewCommand(ViewAction::Left, ViewSpeed::Normal, CommandPhase::Begin);
    rig.controller.release();
    rig.controller.goToSlot(1);  // holding again, key not pressed any more
    Pose out;
    REQUIRE(rig.controller.tick(1.0, out));
    CHECK(out.x == doctest::Approx(0.0));
}

TEST_CASE("external hold: a command during a transition takes over from the in-between view") {
    Rig rig("2");
    rig.saveExternal(1, makePose(10, 0, 0, 0, 0, 0, 1.0));
    rig.sim.view = makePose(0, 0, 0, 0, 0, 0, 1.0);
    rig.controller.goToSlot(1);
    Pose out;
    REQUIRE(rig.controller.tick(1.0, out));  // halfway: x = 5
    REQUIRE(rig.controller.state() == CameraState::Transitioning);

    CHECK(rig.controller.onViewCommand(ViewAction::Up, ViewSpeed::Normal, CommandPhase::Begin));
    CHECK(rig.controller.state() == CameraState::Holding);
    REQUIRE(rig.controller.tick(1.0, out));
    CHECK(out.x == doctest::Approx(5.0));  // the blend stopped; it does not carry on to 10
    CHECK(out.y == doctest::Approx(mykam::kMoveMetresPerSecond));
}

TEST_CASE("external hold: saving after adjusting stores the adjusted pose") {
    Rig rig("0");
    rig.saveExternal(1, makePose(2, 1, 0, 0, 0, 0, 1.0));
    rig.controller.goToSlot(1);
    rig.controller.onViewCommand(ViewAction::Right, ViewSpeed::Normal, CommandPhase::Begin);
    Pose out;
    REQUIRE(rig.controller.tick(1.0, out));
    rig.controller.onViewCommand(ViewAction::Right, ViewSpeed::Normal, CommandPhase::End);
    rig.controller.onViewCommand(ViewAction::RotRight, ViewSpeed::Normal, CommandPhase::Begin);
    REQUIRE(rig.controller.tick(0.5, out));
    rig.controller.onViewCommand(ViewAction::RotRight, ViewSpeed::Normal, CommandPhase::End);

    rig.sim.viewExternal = false;  // whatever the view dataref says, a held camera saves as external
    rig.controller.saveSlot(2);
    REQUIRE(rig.controller.slots().has(2));
    CHECK(rig.controller.slots().kind(2) == SlotKind::External);
    const Pose* p = rig.controller.slots().get(2);
    CHECK(p->x == doctest::Approx(2.0 + mykam::kMoveMetresPerSecond));
    CHECK(p->y == doctest::Approx(1.0));
    CHECK(p->heading == doctest::Approx(0.5 * mykam::kTurnDegreesPerSecond));
    CHECK(rig.sim.files[kCessnaFile].find("view2.x=7") != std::string::npos);
}

TEST_CASE("external hold: the per-frame tick with a held command does no file access and writes no log") {
    Rig rig("0");
    rig.saveExternal(1, makePose(0, 0, 0, 0, 0, 0, 1.0));
    rig.controller.goToSlot(1);
    rig.controller.onViewCommand(ViewAction::Backward, ViewSpeed::Normal, CommandPhase::Begin);
    rig.sim.logs.clear();
    const int ops = rig.sim.fileOps();
    Pose out;
    for (int i = 0; i < 200; ++i) {
        rig.controller.tick(0.016, out);
    }
    CHECK(rig.sim.fileOps() == ops);
    CHECK(rig.sim.logs.empty());
}

// ---------------------------------------------------------------- ViewMotion and pose maths

TEST_CASE("view motion: opposite commands cancel and a zero or negative frame time changes nothing") {
    ViewMotion m;
    m.press(ViewAction::Left, ViewSpeed::Normal);
    m.press(ViewAction::Right, ViewSpeed::Normal);
    const Pose start = makePose(1, 2, 3, 4, 5, 6, 1.5);
    const Pose a = m.apply(start, 1.0);
    CHECK(a.x == doctest::Approx(1.0));
    CHECK(a.zoom == doctest::Approx(1.5));

    m.clear();
    CHECK_FALSE(m.any());
    m.press(ViewAction::Up, ViewSpeed::Normal);
    CHECK(m.apply(start, 0.0).y == doctest::Approx(2.0));
    CHECK(m.apply(start, -1.0).y == doctest::Approx(2.0));
    CHECK(m.release(ViewAction::Up, ViewSpeed::Normal));
    CHECK_FALSE(m.release(ViewAction::Up, ViewSpeed::Normal));
}

TEST_CASE("view motion: zoom and position stay inside the range the slot file accepts") {
    ViewMotion m;
    m.press(ViewAction::ZoomIn, ViewSpeed::Fast);
    Pose p = makePose(0, 0, 0, 0, 0, 0, 1.0);
    for (int i = 0; i < 100; ++i) {
        p = m.apply(p, 1.0);
    }
    CHECK(p.zoom == doctest::Approx(mykam::kMaxZoom));
    m.clear();
    m.press(ViewAction::ZoomOut, ViewSpeed::Fast);
    for (int i = 0; i < 100; ++i) {
        p = m.apply(p, 1.0);
    }
    CHECK(p.zoom == doctest::Approx(mykam::kMinZoom));

    m.clear();
    m.press(ViewAction::Right, ViewSpeed::Fast);
    for (int i = 0; i < 2000; ++i) {
        p = m.apply(p, 1.0);
    }
    CHECK(p.x == doctest::Approx(mykam::kMaxPositionMetres));
    CHECK(mykam::isValidPose(p));
}

TEST_CASE("pose: moving along the camera's own axes follows its heading and pitch") {
    // Camera looking east: forward is +x, right is +z (south), up stays +y.
    Pose east = makePose(0, 0, 0, 0, 90.0, 0, 1.0);
    Pose f = mykam::moveInCameraFrame(east, 0.0, 0.0, 2.0);
    CHECK(f.x == doctest::Approx(2.0));
    CHECK(std::fabs(f.z) < 1e-9);
    Pose r = mykam::moveInCameraFrame(east, 3.0, 0.0, 0.0);
    CHECK(r.z == doctest::Approx(3.0));

    // Camera pitched up 90 degrees looking at the sky: forward is +y.
    Pose sky = makePose(0, 0, 0, 90.0, 0, 0, 1.0);
    Pose s = mykam::moveInCameraFrame(sky, 0.0, 0.0, 1.0);
    CHECK(s.y == doctest::Approx(1.0));
}

TEST_CASE("pose: rotating about the camera's own axes") {
    Pose p = makePose(1, 2, 3, 0, 350.0, 0, 2.0);
    Pose r = mykam::rotateInCameraFrame(p, 20.0, 0.0);
    CHECK(r.heading == doctest::Approx(10.0));  // wraps through north
    CHECK(r.pitch == doctest::Approx(0.0).epsilon(1e-6));
    CHECK(r.x == doctest::Approx(1.0));
    CHECK(r.zoom == doctest::Approx(2.0));

    Pose up = mykam::rotateInCameraFrame(makePose(0, 0, 0, 0, 0, 0, 1.0), 0.0, 30.0);
    CHECK(up.pitch == doctest::Approx(30.0));
    CHECK(up.heading == doctest::Approx(0.0).epsilon(1e-6));
}
