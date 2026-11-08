#include "doctest.h"

#include <cmath>
#include <string>

#include "controller.h"
#include "fake_sim.h"
#include "file_io.h"

using mykam::AircraftState;
using mykam::CameraState;
using mykam::Controller;
using mykam::Pose;

namespace {

const char* const kFolder = "plug";
const char* const kIniPath = "plug/mykam.ini";
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

AircraftState makeAircraft(double x, double y, double z, double pitch, double heading, double roll) {
    AircraftState a;
    a.x = x;
    a.y = y;
    a.z = z;
    a.pitch = pitch;
    a.heading = heading;
    a.roll = roll;
    return a;
}

// A controller started with the given transition time and the Cessna loaded.
struct Rig {
    FakeSim sim;
    Controller controller;

    explicit Rig(const std::string& seconds, bool loadAircraft = true) : controller(sim, kFolder) {
        sim.files[kIniPath] = "transition_seconds = " + seconds + "\n";
        controller.start();
        if (loadAircraft) {
            controller.onAircraftLoaded("Cessna_172SP.acf");
        }
        sim.logs.clear();
    }
};

}  // namespace

TEST_CASE("controller: start reads mykam.ini and logs a prefixed line (FR7, NFR5)") {
    Rig rig("0.5", false);
    CHECK(rig.controller.transitionSeconds() == doctest::Approx(0.5));

    FakeSim sim;  // no mykam.ini at all
    Controller c(sim, kFolder);
    c.start();
    CHECK(c.transitionSeconds() == doctest::Approx(1.5));
    REQUIRE(sim.logs.size() == 2);  // the start line and one fallback warning
    for (const std::string& line : sim.logs) {
        CHECK(line.rfind("[MyKam] ", 0) == 0);
        CHECK(line.back() == '\n');
    }
    CHECK(sim.logs[0].find("started") != std::string::npos);
    CHECK(sim.logs[1].find("warning") != std::string::npos);
}

TEST_CASE("controller: save a camera, move the aircraft, go to it, and it follows the aircraft (FR2, FR3)") {
    Rig rig("0");  // instant cut
    rig.sim.aircraft = makeAircraft(1000.0, 200.0, -500.0, 0.0, 0.0, 0.0);
    rig.sim.view = makePose(1010.0, 205.0, -490.0, -10.0, 30.0, 0.0, 2.0);
    rig.controller.saveSlot(1);

    REQUIRE(rig.sim.writtenPaths.size() == 1);
    CHECK(rig.sim.writtenPaths[0] == kCessnaFile);
    CHECK(rig.sim.logCount("saved camera to slot 1") == 1);
    CHECK(rig.controller.slots().has(1));
    CHECK(rig.controller.state() == CameraState::Idle);

    // The aircraft flies on and the user looks somewhere else.
    rig.sim.aircraft = makeAircraft(1100.0, 250.0, -450.0, 0.0, 0.0, 0.0);
    rig.sim.view = makePose(0, 0, 0, 0, 0, 0, 1.0);
    rig.controller.goToSlot(1);
    CHECK(rig.sim.takeCalls == 1);
    CHECK(rig.controller.state() == CameraState::Holding);

    Pose out;
    REQUIRE(rig.controller.tick(0.016, out));
    CHECK(out.x == doctest::Approx(1110.0));
    CHECK(out.y == doctest::Approx(255.0));
    CHECK(out.z == doctest::Approx(-440.0));
    CHECK(out.pitch == doctest::Approx(-10.0));
    CHECK(out.heading == doctest::Approx(30.0));
    CHECK(out.zoom == doctest::Approx(2.0));
}

TEST_CASE("controller: going to a slot blends over the configured time and then holds (FR4)") {
    Rig rig("2");
    rig.sim.aircraft = makeAircraft(0, 0, 0, 0, 0, 0);
    rig.sim.view = makePose(10.0, 0.0, 0.0, 0, 0, 0, 1.0);
    rig.controller.saveSlot(1);
    rig.sim.view = makePose(0.0, 0.0, 0.0, 0, 0, 0, 1.0);
    rig.controller.goToSlot(1);
    CHECK(rig.controller.state() == CameraState::Transitioning);

    Pose out;
    REQUIRE(rig.controller.tick(0.0, out));
    CHECK(out.x == doctest::Approx(0.0));
    REQUIRE(rig.controller.tick(1.0, out));  // halfway: eased 0.5
    CHECK(out.x == doctest::Approx(5.0));
    CHECK(rig.controller.state() == CameraState::Transitioning);
    REQUIRE(rig.controller.tick(1.5, out));  // past the end
    CHECK(out.x == doctest::Approx(10.0));
    CHECK(rig.controller.state() == CameraState::Holding);
}

TEST_CASE("controller: an empty slot does nothing and logs one line (FR2.2)") {
    Rig rig("1");
    rig.controller.goToSlot(4);
    CHECK(rig.controller.state() == CameraState::Idle);
    CHECK(rig.sim.takeCalls == 0);
    REQUIRE(rig.sim.logs.size() == 1);
    CHECK(rig.sim.logs[0].find("slot 4 is empty") != std::string::npos);

    rig.sim.logs.clear();
    rig.controller.goToSlot(0);   // not a slot
    rig.controller.goToSlot(10);  // not a slot
    rig.controller.saveSlot(0);
    rig.controller.saveSlot(10);
    CHECK(rig.sim.logs.empty());
    CHECK(rig.sim.writeFileCalls == 0);
}

TEST_CASE("controller: release returns control to X-Plane (FR2.3)") {
    Rig rig("0");
    rig.sim.view = makePose(5, 5, 5, 0, 0, 0, 1.0);
    rig.controller.saveSlot(2);
    rig.controller.goToSlot(2);
    REQUIRE(rig.controller.state() == CameraState::Holding);

    rig.controller.release();
    CHECK(rig.controller.state() == CameraState::Idle);
    CHECK(rig.sim.releaseCalls == 1);
    Pose out;
    CHECK_FALSE(rig.controller.tick(0.016, out));

    rig.controller.release();  // already idle: nothing more to give back
    CHECK(rig.sim.releaseCalls == 1);
}

TEST_CASE("controller: when another plugin holds the camera, go does nothing and logs one line") {
    Rig rig("0");
    rig.sim.view = makePose(5, 5, 5, 0, 0, 0, 1.0);
    rig.controller.saveSlot(1);
    rig.sim.logs.clear();

    rig.sim.heldByOther = true;
    rig.controller.goToSlot(1);
    CHECK(rig.controller.state() == CameraState::Idle);
    CHECK(rig.sim.takeCalls == 0);
    REQUIRE(rig.sim.logs.size() == 1);
    CHECK(rig.sim.logs[0].find("another plugin") != std::string::npos);

    // Saving still works: it only reads the view.
    rig.controller.saveSlot(2);
    CHECK(rig.controller.slots().has(2));

    // If taking the camera fails for another reason, we stay idle too.
    rig.sim.heldByOther = false;
    rig.sim.takeResult = false;
    rig.sim.logs.clear();
    rig.controller.goToSlot(1);
    CHECK(rig.controller.state() == CameraState::Idle);
    CHECK(rig.sim.logs.size() == 1);
}

TEST_CASE("controller: an aircraft change releases, cancels the transition and reloads slots (NFR2.4)") {
    Rig rig("5");
    rig.sim.view = makePose(5, 5, 5, 0, 0, 0, 1.0);
    rig.controller.saveSlot(1);
    rig.controller.goToSlot(1);
    REQUIRE(rig.controller.state() == CameraState::Transitioning);

    // The other aircraft already has one saved camera in slot 3.
    mykam::ViewStore other;
    other.add(makePose(1, 2, 3, 0, 90, 0, 1.5), mykam::ViewKind::External, "", 3);
    rig.sim.files["plug/cameras/Baron_58.slots.txt"] = other.serialize();

    rig.controller.onAircraftLoaded("Baron_58.acf");
    CHECK(rig.sim.releaseCalls == 1);
    CHECK(rig.controller.state() == CameraState::Idle);
    Pose out;
    CHECK_FALSE(rig.controller.tick(0.1, out));
    CHECK_FALSE(rig.controller.slots().has(1));
    CHECK(rig.controller.slots().has(3));
    CHECK(rig.controller.aircraftName() == "Baron_58.acf");
    CHECK(rig.sim.logCount("1 saved camera") == 1);

    // Coming back to the first aircraft reloads its saved file.
    rig.controller.onAircraftLoaded("Cessna_172SP.acf");
    CHECK(rig.controller.slots().has(1));
    CHECK_FALSE(rig.controller.slots().has(3));
}

TEST_CASE("controller: a broken, oversized or unreadable slot file loads what it can and warns once") {
    Rig rig("1", false);
    SUBCASE("partly broken") {
        rig.sim.files[kCessnaFile] =
            "mykam_slots=1\nslot1.x=1\nslot1.y=1\nslot1.z=1\nslot1.pitch=0\nslot1.heading=0\nslot1.roll=0\n"
            "slot1.zoom=1\nslot2.x=oops\n";
        rig.controller.onAircraftLoaded("Cessna_172SP.acf");
        CHECK(rig.controller.slots().has(1));
        CHECK_FALSE(rig.controller.slots().has(2));
        CHECK(rig.sim.logCount("warning") == 1);
        CHECK(rig.sim.logCount("1 saved camera") == 1);
    }
    SUBCASE("larger than 64 KB") {
        rig.sim.files[kCessnaFile] = std::string(70 * 1024, 'x');
        rig.controller.onAircraftLoaded("Cessna_172SP.acf");
        CHECK(rig.controller.slots().count() == 0);
        CHECK(rig.sim.logCount("warning") == 1);
    }
    SUBCASE("cannot be read") {
        rig.sim.forcedStatus[kCessnaFile] = mykam::FileStatus::Error;
        rig.controller.onAircraftLoaded("Cessna_172SP.acf");
        CHECK(rig.controller.slots().count() == 0);
        CHECK(rig.sim.logCount("warning") == 1);
    }
    SUBCASE("a hostile aircraft name stays inside the cameras folder and in one log line") {
        rig.controller.onAircraftLoaded("../../x\ny.acf");
        for (const std::string& line : rig.sim.logs) {
            CHECK(line.find('\n') == line.size() - 1);
        }
        rig.sim.view = makePose(1, 1, 1, 0, 0, 0, 1.0);
        rig.controller.saveSlot(1);
        REQUIRE(rig.sim.writtenPaths.size() == 1);
        CHECK(rig.sim.writtenPaths[0].rfind("plug/cameras/", 0) == 0);
        // Only the fixed folder separators remain; the hostile name became a single file name.
        const std::string rest = rig.sim.writtenPaths[0].substr(std::string("plug/cameras/").size());
        CHECK(rest.find('/') == std::string::npos);
        CHECK(rest.find('\\') == std::string::npos);
        CHECK(rest[0] != '.');
    }
}

TEST_CASE("controller: saving during a transition stores the in-between view (FR2.1)") {
    Rig rig("2");
    rig.sim.aircraft = makeAircraft(0, 0, 0, 0, 0, 0);
    rig.sim.view = makePose(10.0, 0.0, 0.0, 0, 0, 0, 1.0);
    rig.controller.saveSlot(1);
    rig.sim.view = makePose(0.0, 0.0, 0.0, 0, 0, 0, 1.0);
    rig.controller.goToSlot(1);

    Pose out;
    REQUIRE(rig.controller.tick(1.0, out));  // halfway
    rig.controller.saveSlot(2);
    REQUIRE(rig.controller.slots().has(2));
    CHECK(rig.controller.slots().get(2)->x == doctest::Approx(5.0));
    CHECK(rig.controller.state() == CameraState::Transitioning);  // saving does not stop the move
}

TEST_CASE("controller: a new go during a transition continues from the in-between view (FR4.3)") {
    Rig rig("2");
    rig.sim.aircraft = makeAircraft(0, 0, 0, 0, 0, 0);
    rig.sim.view = makePose(10.0, 0.0, 0.0, 0, 0, 0, 1.0);
    rig.controller.saveSlot(1);
    rig.sim.view = makePose(-10.0, 0.0, 0.0, 0, 0, 0, 1.0);
    rig.controller.saveSlot(2);
    rig.sim.view = makePose(0.0, 0.0, 0.0, 0, 0, 0, 1.0);

    rig.controller.goToSlot(1);
    Pose before;
    REQUIRE(rig.controller.tick(1.0, before));  // x = 5
    rig.controller.goToSlot(2);
    CHECK(rig.sim.takeCalls == 1);  // camera is already ours
    Pose after;
    REQUIRE(rig.controller.tick(0.0, after));
    CHECK(after.x == doctest::Approx(before.x));  // no jump
    REQUIRE(rig.controller.tick(5.0, after));
    CHECK(after.x == doctest::Approx(-10.0));
}

TEST_CASE("controller: the transition target follows a moving aircraft") {
    Rig rig("2");
    rig.sim.aircraft = makeAircraft(0, 0, 0, 0, 90.0, 0);  // heading east
    rig.sim.view = makePose(0.0, 0.0, 20.0, 0, 90.0, 0, 1.0);  // 20 m south = 20 m to the right
    rig.controller.saveSlot(1);
    rig.sim.view = makePose(0.0, 50.0, 0.0, 0, 0, 0, 1.0);
    rig.controller.goToSlot(1);

    // The aircraft moves east and turns south while the transition runs.
    Pose out;
    rig.sim.aircraft = makeAircraft(500.0, 0, 0, 0, 180.0, 0);
    REQUIRE(rig.controller.tick(10.0, out));  // finished
    // Heading south: right of the aircraft is west, so 20 m right = x 500 - 20.
    CHECK(out.x == doctest::Approx(480.0));
    CHECK(out.z == doctest::Approx(0.0).epsilon(1e-6));
    CHECK(std::fabs(out.z) < 1e-6);
}

TEST_CASE("controller: the per-frame tick does no file access and writes no log (NFR1.3, NFR5.3)") {
    Rig rig("1");
    rig.sim.view = makePose(3, 2, 1, 0, 0, 0, 1.0);
    rig.controller.saveSlot(1);
    rig.controller.goToSlot(1);
    rig.sim.logs.clear();

    const int ops = rig.sim.fileOps();
    Pose out;
    for (int i = 0; i < 200; ++i) {
        rig.controller.tick(0.016, out);
    }
    CHECK(rig.sim.fileOps() == ops);
    CHECK(rig.sim.logs.empty());
    CHECK(rig.sim.readViewCalls == 2);  // only the save and the go read the view, never a tick
}

TEST_CASE("controller: a failed write keeps the slot in memory and logs one warning (FR5.2)") {
    Rig rig("0");
    rig.sim.failWrites = true;
    rig.sim.view = makePose(7, 7, 7, 0, 0, 0, 1.0);
    rig.controller.saveSlot(1);
    CHECK(rig.controller.slots().has(1));
    REQUIRE(rig.sim.logs.size() == 1);
    CHECK(rig.sim.logs[0].find("warning") != std::string::npos);
    CHECK(rig.sim.logs[0].find("memory only") != std::string::npos);

    rig.controller.goToSlot(1);  // still usable this session
    CHECK(rig.controller.state() == CameraState::Holding);
}

TEST_CASE("controller: a view outside the accepted range is not stored") {
    Rig rig("0");
    rig.sim.view = makePose(50000.0, 0, 0, 0, 0, 0, 1.0);  // 50 km from the aircraft
    rig.controller.saveSlot(1);
    CHECK_FALSE(rig.controller.slots().has(1));
    CHECK(rig.sim.writeFileCalls == 0);
    CHECK(rig.sim.logCount("warning") == 1);
}

TEST_CASE("controller: losing the camera to X-Plane goes idle without giving it back twice") {
    Rig rig("0");
    rig.sim.view = makePose(1, 1, 1, 0, 0, 0, 1.0);
    rig.controller.saveSlot(1);
    rig.controller.goToSlot(1);
    REQUIRE(rig.controller.state() == CameraState::Holding);

    rig.controller.onCameraLost();
    CHECK(rig.controller.state() == CameraState::Idle);
    CHECK(rig.sim.releaseCalls == 0);
    Pose out;
    CHECK_FALSE(rig.controller.tick(0.016, out));
}

TEST_CASE("controller: stop releases a held camera and logs") {
    Rig rig("0");
    rig.sim.view = makePose(1, 1, 1, 0, 0, 0, 1.0);
    rig.controller.saveSlot(1);
    rig.controller.goToSlot(1);
    rig.controller.stop();
    CHECK(rig.sim.releaseCalls == 1);
    CHECK(rig.controller.state() == CameraState::Idle);
    CHECK(rig.sim.logCount("stopped") == 1);
}

// ---- Hand-over to X-Plane's free camera (free_camera_handoff) ----

namespace {

struct HandoffRig {
    FakeSim sim;
    Controller controller;

    // iniLines is the text after transition_seconds, e.g. "free_camera_handoff = 0\n" or "".
    HandoffRig(const std::string& seconds, const std::string& iniLines) : controller(sim, kFolder) {
        sim.files[kIniPath] = "transition_seconds = " + seconds + "\n" + iniLines;
        controller.start();
        controller.onAircraftLoaded("Cessna_172SP.acf");
        sim.aircraft = makeAircraft(0, 0, 0, 0, 0, 0);
        sim.view = makePose(10.0, 5.0, 0.0, 0, 0, 0, 1.0);
        controller.saveSlot(2);
        sim.view = makePose(0.0, 0.0, 0.0, 0, 0, 0, 1.0);
        sim.logs.clear();
    }
};

}  // namespace

TEST_CASE("handoff: requested only after the transition has ended") {
    HandoffRig rig("2", "");  // handoff missing from the ini means on
    rig.controller.goToSlot(2);
    CHECK(rig.sim.handoffFrameRequests == 0);

    Pose out;
    REQUIRE(rig.controller.tick(1.0, out));  // halfway
    CHECK(rig.sim.handoffFrameRequests == 0);
    CHECK_FALSE(rig.controller.handoffPending());
    CHECK(rig.controller.state() == CameraState::Transitioning);

    REQUIRE(rig.controller.tick(1.5, out));  // past the end: the camera is placed at the saved spot
    CHECK(out.x == doctest::Approx(10.0));
    CHECK(rig.controller.state() == CameraState::Holding);
    CHECK(rig.sim.handoffFrameRequests == 1);
    CHECK(rig.controller.handoffPending());
    // Nothing is handed over inside the camera callback.
    CHECK(rig.sim.freeCameraCommandCalls == 0);
    CHECK(rig.sim.releaseCalls == 0);
    CHECK(rig.sim.owned);

    REQUIRE(rig.controller.tick(0.016, out));  // further frames before the flight loop do not ask again
    CHECK(rig.sim.handoffFrameRequests == 1);
}

TEST_CASE("handoff: a zero transition time requests it on the first frame at the spot") {
    HandoffRig rig("0", "free_camera_handoff = 1\n");
    rig.controller.goToSlot(2);
    CHECK(rig.controller.state() == CameraState::Holding);
    CHECK(rig.sim.freeCameraCommandCalls == 0);  // never from goToSlot itself

    Pose out;
    REQUIRE(rig.controller.tick(0.016, out));  // this frame renders at the saved spot
    CHECK(out.x == doctest::Approx(10.0));
    CHECK(rig.sim.handoffFrameRequests == 1);
    CHECK(rig.controller.handoffPending());
    CHECK(rig.sim.freeCameraCommandCalls == 0);
}

TEST_CASE("handoff: performing it runs the command, gives the camera back, goes idle and logs one line") {
    HandoffRig rig("0", "");
    rig.controller.goToSlot(2);
    Pose out;
    REQUIRE(rig.controller.tick(0.016, out));
    REQUIRE(rig.sim.mouseCapture);
    const int fileOps = rig.sim.fileOps();

    rig.controller.performHandoff();
    CHECK(rig.sim.freeCameraCommandCalls == 1);
    CHECK(rig.sim.ownedAtFreeCameraCommand);  // the command ran first, then the camera was released
    CHECK(rig.sim.releaseCalls == 1);
    CHECK_FALSE(rig.sim.owned);
    CHECK(rig.controller.state() == CameraState::Idle);
    CHECK_FALSE(rig.sim.mouseCapture);
    CHECK_FALSE(rig.controller.handoffPending());
    CHECK_FALSE(rig.controller.tick(0.016, out));
    CHECK(rig.sim.fileOps() == fileOps);
    REQUIRE(rig.sim.logs.size() == 1);
    CHECK(rig.sim.logs[0].find("handed over to the X-Plane sim/view/free_camera view for 'Outside view 1'") !=
          std::string::npos);
    CHECK(rig.sim.logs[0].rfind("[MyKam] ", 0) == 0);

    rig.controller.performHandoff();  // nothing pending any more: no second command, no second line
    CHECK(rig.sim.freeCameraCommandCalls == 1);
    CHECK(rig.sim.logs.size() == 1);
}

TEST_CASE("handoff: nothing happens when performHandoff is called with no request") {
    HandoffRig rig("2", "");
    rig.controller.performHandoff();  // idle
    rig.controller.goToSlot(2);
    rig.controller.performHandoff();  // still transitioning, nothing requested
    CHECK(rig.sim.freeCameraCommandCalls == 0);
    CHECK(rig.controller.state() == CameraState::Transitioning);
    CHECK(rig.sim.logs.empty());
}

TEST_CASE("handoff: with free_camera_handoff = 0 the camera is held exactly as before") {
    HandoffRig rig("0", "free_camera_handoff = 0\n");
    rig.controller.goToSlot(2);
    Pose out;
    for (int i = 0; i < 5; ++i) {
        REQUIRE(rig.controller.tick(0.016, out));
    }
    CHECK(rig.sim.handoffFrameRequests == 0);
    CHECK_FALSE(rig.controller.handoffPending());
    rig.controller.performHandoff();
    CHECK(rig.sim.freeCameraCommandCalls == 0);
    CHECK(rig.controller.state() == CameraState::Holding);
    CHECK(rig.sim.owned);
    CHECK(rig.sim.mouseCapture);
    CHECK(rig.controller.onMouseWheel(1));  // mouse look-around, wheel and view keys still work
    CHECK(rig.controller.onViewCommand(mykam::ViewAction::Forward, mykam::ViewSpeed::Normal, mykam::CommandPhase::Begin));
}

TEST_CASE("handoff: a cockpit slot never hands off") {
    HandoffRig rig("0", "");
    rig.sim.viewExternal = false;
    rig.sim.head = makePose(0.1, 0.2, 0.3, 0, 10.0, 0, 1.0);
    rig.controller.saveSlot(1);
    rig.controller.goToSlot(1);
    Pose out;
    CHECK_FALSE(rig.controller.tick(0.016, out));
    CHECK(rig.sim.handoffFrameRequests == 0);
    rig.controller.performHandoff();
    CHECK(rig.sim.freeCameraCommandCalls == 0);
    CHECK(rig.sim.takeCalls == 0);
}

TEST_CASE("handoff: a missing free camera command logs one warning and keeps holding") {
    HandoffRig rig("0", "");
    rig.sim.freeCameraCommandWorks = false;
    rig.controller.goToSlot(2);
    Pose out;
    REQUIRE(rig.controller.tick(0.016, out));
    rig.controller.performHandoff();

    CHECK(rig.sim.freeCameraCommandCalls == 1);
    CHECK(rig.controller.state() == CameraState::Holding);
    CHECK(rig.sim.owned);
    CHECK(rig.sim.releaseCalls == 0);
    CHECK(rig.sim.mouseCapture);
    REQUIRE(rig.sim.logs.size() == 1);
    CHECK(rig.sim.logs[0].find("warning") != std::string::npos);
    CHECK(rig.sim.logs[0].find("sim/view/free_camera") != std::string::npos);

    // It does not try again every frame.
    REQUIRE(rig.controller.tick(0.016, out));
    CHECK(rig.sim.handoffFrameRequests == 1);
    rig.controller.performHandoff();
    CHECK(rig.sim.freeCameraCommandCalls == 1);
    CHECK(rig.sim.logs.size() == 1);
}

TEST_CASE("handoff: release, an aircraft change or losing the camera cancel a pending hand-over") {
    HandoffRig rig("0", "");
    Pose out;
    SUBCASE("release") {
        rig.controller.goToSlot(2);
        REQUIRE(rig.controller.tick(0.016, out));
        rig.controller.release();
    }
    SUBCASE("aircraft change") {
        rig.controller.goToSlot(2);
        REQUIRE(rig.controller.tick(0.016, out));
        rig.controller.onAircraftLoaded("Cessna_172SP.acf");
    }
    SUBCASE("camera lost to another plugin") {
        rig.controller.goToSlot(2);
        REQUIRE(rig.controller.tick(0.016, out));
        rig.controller.onCameraLost();
    }
    SUBCASE("release before the first frame") {
        rig.controller.goToSlot(2);
        rig.controller.release();
        CHECK_FALSE(rig.controller.tick(0.016, out));
    }
    CHECK_FALSE(rig.controller.handoffPending());
    rig.controller.performHandoff();
    CHECK(rig.sim.freeCameraCommandCalls == 0);
    CHECK(rig.controller.state() == CameraState::Idle);
}

TEST_CASE("handoff: the user grabbing the camera before it arrives cancels the hand-over") {
    HandoffRig rig("2", "");
    rig.controller.goToSlot(2);
    Pose out;
    REQUIRE(rig.controller.tick(0.5, out));
    REQUIRE(rig.controller.onMouseWheel(1));  // the user moves the camera mid-transition
    REQUIRE(rig.controller.tick(0.016, out));
    CHECK(rig.sim.handoffFrameRequests == 0);
    rig.controller.performHandoff();
    CHECK(rig.sim.freeCameraCommandCalls == 0);
    CHECK(rig.controller.state() == CameraState::Holding);
}

TEST_CASE("handoff: saving after the hand-over reads the X-Plane view, like any camera we do not hold") {
    HandoffRig rig("0", "");
    rig.controller.goToSlot(2);
    Pose out;
    REQUIRE(rig.controller.tick(0.016, out));
    rig.controller.performHandoff();
    REQUIRE(rig.controller.state() == CameraState::Idle);

    rig.sim.view = makePose(0.0, 20.0, 30.0, 0, 0, 0, 1.0);  // where X-Plane's free camera is now
    const int reads = rig.sim.readViewCalls;
    rig.controller.saveSlot(3);
    CHECK(rig.sim.readViewCalls == reads + 1);
    REQUIRE(rig.controller.slots().has(3));
    CHECK(rig.controller.slots().get(3)->y == doctest::Approx(20.0));
}

TEST_CASE("handoff: a second go to a slot after the hand-over takes the camera and hands over again") {
    HandoffRig rig("0", "");
    rig.controller.goToSlot(2);
    Pose out;
    REQUIRE(rig.controller.tick(0.016, out));
    rig.controller.performHandoff();
    rig.controller.goToSlot(2);
    CHECK(rig.sim.takeCalls == 2);
    REQUIRE(rig.controller.tick(0.016, out));
    CHECK(rig.sim.handoffFrameRequests == 2);
    rig.controller.performHandoff();
    CHECK(rig.sim.freeCameraCommandCalls == 2);
    CHECK(rig.sim.logCount("handed over") == 2);
}

TEST_CASE("handoff: the per-frame tick does no file access and writes no log when a hand-over is requested") {
    HandoffRig rig("1", "");
    rig.controller.goToSlot(2);
    rig.sim.logs.clear();
    const int fileOps = rig.sim.fileOps();
    Pose out;
    for (int i = 0; i < 100; ++i) {
        REQUIRE(rig.controller.tick(0.016, out));
    }
    CHECK(rig.sim.fileOps() == fileOps);
    CHECK(rig.sim.logs.empty());
}

// ---- Hand-over to the saved X-Plane view type (handoff_view) ----

#include "view_commands.h"

namespace {

// Like HandoffRig, but plays X-Plane's before-handler: every view command that runs reaches
// Controller::onViewSelected, and `seen` keeps the controller's record right after each call.
struct ViewRig {
    FakeSim sim;
    Controller controller;
    std::vector<std::string> seen;

    explicit ViewRig(const std::string& iniLines = "") : controller(sim, kFolder) {
        sim.files[kIniPath] = "transition_seconds = 0\n" + iniLines;
        sim.onCommand = [this](const std::string& name) {
            controller.onViewSelected(name);
            seen.push_back(controller.lastViewCommand());
        };
        controller.start();
        controller.onAircraftLoaded("Cessna_172SP.acf");
        sim.aircraft = makeAircraft(0, 0, 0, 0, 0, 0);
        sim.view = makePose(10.0, 5.0, 0.0, 0, 0, 0, 1.0);
        sim.logs.clear();
    }

    // The user picks `view` (empty = no view command yet) and saves an outside camera in `slot`.
    void saveIn(int slot, const std::string& view) {
        if (!view.empty()) {
            controller.onViewSelected(view);
        }
        controller.saveSlot(slot);
    }

    // Go to the slot, let the camera arrive, then run the flight-loop hand-over.
    void goAndHandOver(int slot) {
        sim.view = makePose(0.0, 0.0, 0.0, 0, 0, 0, 1.0);
        controller.goToSlot(slot);
        Pose out;
        REQUIRE(controller.tick(0.016, out));
        controller.performHandoff();
    }
};

}  // namespace

TEST_CASE("view tracking: every tracked command is remembered as the last view command") {
    for (const mykam::ViewCommandInfo& info : mykam::kViewCommands) {
        ViewRig rig;
        INFO(info.name);
        CHECK(rig.controller.lastViewCommand().empty());
        rig.controller.onViewSelected(info.name);
        CHECK(rig.controller.lastViewCommand() == info.name);
    }
}

TEST_CASE("view tracking: the latest command wins and untracked names are ignored") {
    ViewRig rig;
    rig.controller.onViewSelected("sim/view/chase");
    rig.controller.onViewSelected("sim/view/circle");
    CHECK(rig.controller.lastViewCommand() == "sim/view/circle");
    const char* ignored[] = {"sim/view/quick_look_3", "sim/view/3d_cockpit_toggle", "sim/general/left",
                             "mykam/go_slot_1", "sim/view/", "sim/view/chase ", ""};
    for (const char* name : ignored) {
        INFO(name);
        rig.controller.onViewSelected(name);
        CHECK(rig.controller.lastViewCommand() == "sim/view/circle");
    }
}

TEST_CASE("view tracking: the commands mykam runs itself do not overwrite the record") {
    SUBCASE("hand-over") {
        ViewRig rig;
        rig.saveIn(2, "sim/view/chase");
        rig.controller.onViewSelected("sim/view/tower");  // the user moved on to the tower view
        rig.goAndHandOver(2);
        REQUIRE(rig.sim.viewCommandsRun.size() == 1);
        CHECK(rig.sim.viewCommandsRun[0] == "sim/view/chase");
        REQUIRE(rig.seen.size() == 1);
        CHECK(rig.seen[0] == "sim/view/tower");  // X-Plane reported chase back; the record did not move
        CHECK(rig.controller.lastViewCommand() == "sim/view/chase");  // now the view really is chase
    }
    SUBCASE("switch to the cockpit view for a cockpit slot") {
        ViewRig rig;
        rig.saveIn(2, "sim/view/chase");
        rig.sim.viewExternal = false;
        rig.sim.head = makePose(0.1, 0.2, 0.3, 0, 10.0, 0, 1.0);
        rig.controller.saveSlot(1);
        rig.sim.viewExternal = true;
        rig.controller.onViewSelected("sim/view/runway");
        rig.controller.goToSlot(1);
        REQUIRE(rig.sim.cockpitCommandCalls == 1);
        REQUIRE(rig.seen.size() == 1);
        CHECK(rig.seen[0] == "sim/view/runway");
        CHECK(rig.controller.lastViewCommand() == "sim/view/runway");
    }
    SUBCASE("a command the user runs after the hand-over is recorded again") {
        ViewRig rig;
        rig.saveIn(2, "sim/view/chase");
        rig.goAndHandOver(2);
        rig.controller.onViewSelected("sim/view/wingman");
        CHECK(rig.controller.lastViewCommand() == "sim/view/wingman");
    }
}

TEST_CASE("view save: an outside slot stores the last view command and writes slotN.view") {
    ViewRig rig;
    rig.saveIn(2, "sim/view/chase");
    CHECK(rig.controller.slots().viewCommand(2) == "sim/view/chase");
    const std::string& file = rig.sim.files[kCessnaFile];
    CHECK(file.find("view1.view=sim/view/chase\n") != std::string::npos);

    rig.saveIn(3, "sim/view/free_camera");
    CHECK(rig.controller.slots().viewCommand(3) == "sim/view/free_camera");
    CHECK(rig.controller.slots().viewCommand(2) == "sim/view/chase");  // other slots keep theirs
}

TEST_CASE("view save: no stored view without a user command, for cockpit-type commands or for cockpit slots") {
    SUBCASE("no view command run yet") {
        ViewRig rig;
        rig.saveIn(2, "");
        CHECK(rig.controller.slots().has(2));
        CHECK(rig.controller.slots().viewCommand(2).empty());
        CHECK(rig.sim.files[kCessnaFile].find(".view=") == std::string::npos);
    }
    SUBCASE("the last command was a cockpit or behaviour command") {
        ViewRig rig;
        rig.saveIn(2, "sim/view/default_view");
        rig.saveIn(3, "sim/view/cinema_verite");
        CHECK(rig.controller.slots().viewCommand(2).empty());
        CHECK(rig.controller.slots().viewCommand(3).empty());
    }
    SUBCASE("cockpit slot") {
        ViewRig rig;
        rig.controller.onViewSelected("sim/view/chase");
        rig.sim.viewExternal = false;
        rig.sim.head = makePose(0.1, 0.2, 0.3, 0, 10.0, 0, 1.0);
        rig.controller.saveSlot(4);
        REQUIRE(rig.controller.slots().has(4));
        CHECK(rig.controller.slots().viewCommand(4).empty());
        CHECK(rig.sim.files[kCessnaFile].find(".view=") == std::string::npos);
    }
}

TEST_CASE("view save: saving while mykam holds the camera also stores the last view command") {
    ViewRig rig("handoff_view = off\n");
    rig.saveIn(2, "sim/view/chase");
    rig.sim.view = makePose(0.0, 0.0, 0.0, 0, 0, 0, 1.0);
    rig.controller.goToSlot(2);
    Pose out;
    REQUIRE(rig.controller.tick(0.016, out));
    REQUIRE(rig.controller.state() == CameraState::Holding);
    rig.controller.saveSlot(5);
    CHECK(rig.controller.slots().viewCommand(5) == "sim/view/chase");
}

TEST_CASE("view save: refused in circle, tower, spot and the other views X-Plane places itself") {
    const char* const views[] = {"sim/view/circle",   "sim/view/tower",     "sim/view/linear_spot",
                                 "sim/view/still_spot", "sim/view/runway",  "sim/view/ridealong",
                                 "sim/view/wingman",  "sim/view/track_weapon"};
    for (const char* view : views) {
        ViewRig rig;
        INFO(view);
        rig.sim.logs.clear();
        rig.saveIn(2, view);
        CHECK_FALSE(rig.controller.slots().has(2));
        CHECK(rig.sim.files.count(kCessnaFile) == 0);  // nothing written
        REQUIRE(rig.sim.logs.size() == 1);
        CHECK(rig.sim.logs[0].find("slot 2 not saved: saving is not available in the " + std::string(view)) !=
              std::string::npos);
    }
}

TEST_CASE("view save: a refused save keeps the slot that was there before") {
    ViewRig rig;
    rig.saveIn(2, "sim/view/chase");
    REQUIRE(rig.controller.slots().has(2));
    rig.saveIn(2, "sim/view/circle");
    CHECK(rig.controller.slots().has(2));
    CHECK(rig.controller.slots().viewCommand(2) == "sim/view/chase");
}

TEST_CASE("view save: still allowed in the cockpit, the free camera and the chase view") {
    ViewRig rig;
    rig.saveIn(2, "sim/view/free_camera");
    rig.saveIn(3, "sim/view/chase");
    CHECK(rig.controller.slots().has(2));
    CHECK(rig.controller.slots().has(3));
    rig.controller.onViewSelected("sim/view/circle");
    rig.sim.viewExternal = false;  // a cockpit save does not depend on the last outside view
    rig.sim.head = makePose(0.1, 0.2, 0.3, 0, 10.0, 0, 1.0);
    rig.controller.saveSlot(4);
    CHECK(rig.controller.slots().has(4));
}

TEST_CASE("view hand-over: an older slot that stored a no-save view falls back to the free camera") {
    ViewRig first;
    first.saveIn(3, "sim/view/chase");
    std::string file = first.sim.files[kCessnaFile];
    const std::size_t at = file.find("sim/view/chase");
    REQUIRE(at != std::string::npos);
    file.replace(at, std::string("sim/view/chase").size(), "sim/view/circle");
    ViewRig second;
    second.sim.files[kCessnaFile] = file;
    second.controller.onAircraftLoaded("Cessna_172SP.acf");
    REQUIRE(second.controller.slots().has(3));
    second.goAndHandOver(3);
    REQUIRE(second.sim.viewCommandsRun.size() == 1);
    CHECK(second.sim.viewCommandsRun[0] == "sim/view/free_camera");
}

TEST_CASE("view hand-over: goes to the view type stored in the slot") {
    const char* const views[] = {"sim/view/chase", "sim/view/free_camera"};
    for (const char* view : views) {
        ViewRig rig;
        INFO(view);
        rig.saveIn(2, view);
        rig.sim.logs.clear();
        rig.goAndHandOver(2);
        REQUIRE(rig.sim.viewCommandsRun.size() == 1);
        CHECK(rig.sim.viewCommandsRun[0] == view);
        CHECK(rig.sim.freeCameraCommandCalls == (std::string(view) == "sim/view/free_camera" ? 1 : 0));
        CHECK(rig.sim.ownedAtViewCommand);  // the command ran first, then the camera was released
        CHECK(rig.sim.releaseCalls == 1);
        CHECK_FALSE(rig.sim.owned);
        CHECK(rig.controller.state() == CameraState::Idle);
        CHECK_FALSE(rig.sim.mouseCapture);
        REQUIRE(rig.sim.logs.size() == 1);
        CHECK(rig.sim.logs[0].find(std::string("handed over to the X-Plane ") + view + " view for 'Outside view 1'") !=
              std::string::npos);
    }
}

TEST_CASE("view hand-over: the stored view survives a restart") {
    ViewRig first;
    first.saveIn(3, "sim/view/chase");
    ViewRig second;
    second.sim.files[kCessnaFile] = first.sim.files[kCessnaFile];
    second.controller.onAircraftLoaded("Cessna_172SP.acf");
    REQUIRE(second.controller.slots().has(3));
    second.goAndHandOver(3);
    REQUIRE(second.sim.viewCommandsRun.size() == 1);
    CHECK(second.sim.viewCommandsRun[0] == "sim/view/chase");
}

TEST_CASE("view hand-over: falls back to the free camera without a stored view") {
    ViewRig rig;
    rig.saveIn(2, "");
    rig.sim.logs.clear();
    rig.goAndHandOver(2);
    REQUIRE(rig.sim.viewCommandsRun.size() == 1);
    CHECK(rig.sim.viewCommandsRun[0] == "sim/view/free_camera");
    CHECK(rig.sim.logCount("handed over to the X-Plane sim/view/free_camera view for 'Outside view 1'") == 1);
    CHECK(rig.controller.state() == CameraState::Idle);
}

TEST_CASE("view hand-over: a stored name that is not an allowed outside view falls back to the free camera") {
    const char* const names[] = {"sim/view/quick_look_3", "sim/view/default_view", "sim/view/not_a_real_view",
                                 "sim/view/cinema_verite"};
    for (const char* name : names) {
        ViewRig rig;
        INFO(name);
        rig.sim.files[kCessnaFile] =
            std::string("mykam_slots=1\nslot2.x=10\nslot2.y=5\nslot2.z=0\nslot2.pitch=0\nslot2.heading=0\n"
                        "slot2.roll=0\nslot2.zoom=1\nslot2.view=") + name + "\n";
        rig.controller.onAircraftLoaded("Cessna_172SP.acf");
        REQUIRE(rig.controller.slots().has(2));
        rig.goAndHandOver(2);
        REQUIRE(rig.sim.viewCommandsRun.size() == 1);
        CHECK(rig.sim.viewCommandsRun[0] == "sim/view/free_camera");
        CHECK(rig.controller.state() == CameraState::Idle);
    }
}

TEST_CASE("view hand-over: a stored view X-Plane cannot find falls back to the free camera with one warning") {
    ViewRig rig;
    rig.saveIn(2, "sim/view/chase");
    rig.sim.missingCommands.insert("sim/view/chase");
    rig.sim.logs.clear();
    rig.goAndHandOver(2);
    REQUIRE(rig.sim.viewCommandsRun.size() == 2);
    CHECK(rig.sim.viewCommandsRun[0] == "sim/view/chase");
    CHECK(rig.sim.viewCommandsRun[1] == "sim/view/free_camera");
    CHECK(rig.sim.logCount("warning") == 1);
    CHECK(rig.sim.logCount("handed over to the X-Plane sim/view/free_camera view for 'Outside view 1'") == 1);
    CHECK(rig.controller.state() == CameraState::Idle);
    CHECK_FALSE(rig.sim.owned);
}

TEST_CASE("view hand-over: when the free camera is missing too the camera stays held") {
    ViewRig rig;
    rig.saveIn(2, "sim/view/chase");
    rig.sim.missingCommands.insert("sim/view/chase");
    rig.sim.freeCameraCommandWorks = false;
    rig.sim.logs.clear();
    rig.goAndHandOver(2);
    CHECK(rig.controller.state() == CameraState::Holding);
    CHECK(rig.sim.owned);
    CHECK(rig.sim.releaseCalls == 0);
    CHECK(rig.sim.logCount("handed over") == 0);
    CHECK(rig.sim.logCount("keeping the camera at 'Outside view 1'") == 1);
}

TEST_CASE("view hand-over: handoff_view = free always uses the free camera") {
    ViewRig rig("handoff_view = free\n");
    rig.saveIn(2, "sim/view/chase");
    rig.goAndHandOver(2);
    REQUIRE(rig.sim.viewCommandsRun.size() == 1);
    CHECK(rig.sim.viewCommandsRun[0] == "sim/view/free_camera");
    CHECK(rig.controller.state() == CameraState::Idle);
}

TEST_CASE("view hand-over: handoff_view = off keeps holding even when a view is stored") {
    ViewRig rig("handoff_view = off\n");
    rig.saveIn(2, "sim/view/chase");
    rig.goAndHandOver(2);
    CHECK(rig.sim.viewCommandsRun.empty());
    CHECK(rig.sim.handoffFrameRequests == 0);
    CHECK(rig.controller.state() == CameraState::Holding);
    CHECK(rig.sim.owned);
    CHECK(rig.sim.mouseCapture);
}

TEST_CASE("view hand-over: the old free_camera_handoff = 0 still keeps holding, 1 uses the saved view") {
    ViewRig off("free_camera_handoff = 0\n");
    off.saveIn(2, "sim/view/chase");
    off.goAndHandOver(2);
    CHECK(off.sim.viewCommandsRun.empty());
    CHECK(off.controller.state() == CameraState::Holding);

    ViewRig on("free_camera_handoff = 1\n");
    on.saveIn(2, "sim/view/chase");
    on.goAndHandOver(2);
    REQUIRE(on.sim.viewCommandsRun.size() == 1);
    CHECK(on.sim.viewCommandsRun[0] == "sim/view/chase");
}

TEST_CASE("view hand-over: a cockpit slot never runs a view command, whatever the last command was") {
    ViewRig rig;
    rig.controller.onViewSelected("sim/view/chase");
    rig.sim.viewExternal = false;
    rig.sim.head = makePose(0.1, 0.2, 0.3, 0, 10.0, 0, 1.0);
    rig.controller.saveSlot(1);
    rig.controller.goToSlot(1);
    Pose out;
    CHECK_FALSE(rig.controller.tick(0.016, out));
    rig.controller.performHandoff();
    CHECK(rig.sim.viewCommandsRun.empty());
    CHECK(rig.sim.handoffFrameRequests == 0);
}

// ---------------------------------------------------------------- views kept in the aircraft folder

namespace {

const char* const kA320Folder = "X-Plane 12/Aircraft/A320";
const char* const kA320Views = "X-Plane 12/Aircraft/A320/mykam_views.txt";
const char* const kA320Legacy = "plug/cameras/a320.slots.txt";

}  // namespace

TEST_CASE("aircraft folder: views are saved next to the .acf in mykam_views.txt") {
    Rig rig("0", false);
    rig.controller.onAircraftLoaded("a320.acf", kA320Folder);
    CHECK(rig.controller.viewFilePath() == kA320Views);

    rig.sim.view = makePose(-18.0, 6.0, 12.0, -8.0, 140.0, 0.0, 1.0);
    rig.controller.saveSlot(2);
    CHECK(rig.sim.files.count(kA320Views) == 1);
    CHECK(rig.sim.files.count(kA320Legacy) == 0);  // nothing written to the plugin folder
    CHECK(rig.sim.files[kA320Views].find("format=2") != std::string::npos);
}

TEST_CASE("aircraft folder: an older file in the plugin cameras folder is read once and moved on the next save") {
    Rig first("0", false);
    first.controller.onAircraftLoaded("a320.acf");  // an earlier layout: no aircraft folder known
    first.sim.view = makePose(-18.0, 6.0, 12.0, -8.0, 140.0, 0.0, 1.0);
    first.controller.saveSlot(3);
    REQUIRE(first.sim.files.count(kA320Legacy) == 1);

    Rig rig("0", false);
    rig.sim.files[kA320Legacy] = first.sim.files[kA320Legacy];
    rig.controller.onAircraftLoaded("a320.acf", kA320Folder);
    CHECK(rig.controller.slots().has(3));
    bool noted = false;
    for (const std::string& line : rig.sim.logs) {
        noted = noted || line.find("views read from the old cameras folder") != std::string::npos;
    }
    CHECK(noted);
    CHECK(rig.sim.files.count(kA320Views) == 0);  // loading writes nothing

    rig.sim.view = makePose(-10.0, 4.0, 8.0, -5.0, 100.0, 0.0, 1.0);
    rig.controller.saveSlot(4);
    REQUIRE(rig.sim.files.count(kA320Views) == 1);
    CHECK(rig.sim.files[kA320Views].find(".hotkey=3") != std::string::npos);  // the old view moved too
    CHECK(rig.sim.files[kA320Views].find(".hotkey=4") != std::string::npos);
    CHECK(rig.sim.files[kA320Legacy] == first.sim.files[kA320Legacy]);  // the old file is left as it was
}

TEST_CASE("aircraft folder: the file in the aircraft folder wins over an older plugin file") {
    Rig a("0", false);
    a.controller.onAircraftLoaded("a320.acf", kA320Folder);
    a.sim.view = makePose(-18.0, 6.0, 12.0, -8.0, 140.0, 0.0, 1.0);
    a.controller.saveSlot(5);

    Rig rig("0", false);
    rig.sim.files[kA320Views] = a.sim.files[kA320Views];
    rig.sim.files[kA320Legacy] = "garbage that must not be read\n";
    rig.controller.onAircraftLoaded("a320.acf", kA320Folder);
    CHECK(rig.controller.slots().has(5));
    for (const std::string& line : rig.sim.logs) {
        CHECK(line.find("old cameras folder") == std::string::npos);
        CHECK(line.find("warning") == std::string::npos);
    }
}

TEST_CASE("aircraft folder: two aircraft with the same .acf name keep separate views") {
    Rig rig("0", false);
    rig.controller.onAircraftLoaded("a320.acf", "Aircraft/ToLiss A320");
    rig.sim.view = makePose(-18.0, 6.0, 12.0, -8.0, 140.0, 0.0, 1.0);
    rig.controller.saveSlot(1);

    rig.controller.onAircraftLoaded("a320.acf", "Aircraft/Other A320");
    CHECK_FALSE(rig.controller.slots().has(1));
    CHECK(rig.sim.files.count("Aircraft/ToLiss A320/mykam_views.txt") == 1);
    CHECK(rig.sim.files.count("Aircraft/Other A320/mykam_views.txt") == 0);
}

TEST_CASE("aircraft folder: folderOfPath splits Windows and forward-slash paths") {
    CHECK(mykam::folderOfPath("D:\\X-Plane 12\\Aircraft\\A320\\a320.acf") == "D:\\X-Plane 12\\Aircraft\\A320");
    CHECK(mykam::folderOfPath("Aircraft/A320/a320.acf") == "Aircraft/A320");
    CHECK(mykam::folderOfPath("a320.acf").empty());
    CHECK(mykam::folderOfPath("").empty());
}
