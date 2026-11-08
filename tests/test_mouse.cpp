#include "doctest.h"

#include <cmath>
#include <string>

#include "controller.h"
#include "fake_sim.h"
#include "mouse_orbit.h"
#include "pose.h"
#include "slot_store.h"

using mykam::AircraftState;
using mykam::CameraState;
using mykam::Controller;
using mykam::Pose;
using mykam::SlotKind;

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kRad = kPi / 180.0;

double dist(const Pose& p) {
    return std::sqrt(p.x * p.x + p.y * p.y + p.z * p.z);
}

// A camera at azimuth/elevation/distance from the body origin, looking straight at the origin.
Pose lookingAtOrigin(double azimuthDeg, double elevationDeg, double d) {
    Pose p;
    p.x = -d * std::sin(azimuthDeg * kRad) * std::cos(elevationDeg * kRad);
    p.y = d * std::sin(elevationDeg * kRad);
    p.z = d * std::cos(azimuthDeg * kRad) * std::cos(elevationDeg * kRad);
    p.pitch = -elevationDeg;
    p.heading = mykam::wrap360(azimuthDeg);
    p.roll = 0.0;
    p.zoom = 1.0;
    return p;
}

// Elevation of the camera position above the horizontal plane through the origin, in degrees.
double elevationOf(const Pose& p) {
    return std::asin(p.y / dist(p)) / kRad;
}

// The body point `metres` straight ahead of the camera; the origin when the camera looks at it.
Pose pointAhead(const Pose& cam, double metres) {
    return mykam::moveInCameraFrame(cam, 0.0, 0.0, metres);
}

bool nearZero(const Pose& p, double tolerance) {
    return std::fabs(p.x) < tolerance && std::fabs(p.y) < tolerance && std::fabs(p.z) < tolerance;
}

// Cosine of the angle between the camera's forward direction and the direction to the origin.
double alignment(const Pose& cam) {
    const Pose ahead = pointAhead(cam, 1.0);
    const double fx = ahead.x - cam.x;
    const double fy = ahead.y - cam.y;
    const double fz = ahead.z - cam.z;
    const double d = dist(cam);
    return (fx * -cam.x + fy * -cam.y + fz * -cam.z) / d;
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

    // Save the pose as an external slot (aircraft at the origin, level, so world equals body).
    void saveExternal(int slot, const Pose& view) {
        sim.viewExternal = true;
        sim.view = view;
        controller.saveSlot(slot);
    }
    void saveCockpit(int slot) {
        sim.viewExternal = false;
        sim.head = Pose();
        controller.saveSlot(slot);
        sim.viewExternal = true;
    }
    // The camera the controller would place now, in world coordinates.
    Pose placed() {
        Pose w;
        REQUIRE(controller.tick(0.0, w));
        return w;
    }
};

}  // namespace

// ---------------------------------------------------------------- orbit maths

TEST_CASE("orbit: keeps the distance and keeps the origin centred on screen") {
    const Pose start = lookingAtOrigin(20.0, 10.0, 30.0);
    const Pose out = mykam::orbitAroundOrigin(start, 35.0, -20.0);

    CHECK(dist(out) == doctest::Approx(30.0));
    CHECK(nearZero(pointAhead(out, 30.0), 1e-6));  // still looks at the same body point
    CHECK(elevationOf(out) == doctest::Approx(-10.0));
    CHECK(out.heading == doctest::Approx(55.0));
    CHECK(out.pitch == doctest::Approx(10.0));
    CHECK(out.zoom == doctest::Approx(1.0));
}

TEST_CASE("orbit: azimuth moves the camera round the vertical axis at constant height") {
    const Pose start = lookingAtOrigin(0.0, 30.0, 10.0);  // behind and above
    const Pose out = mykam::orbitAroundOrigin(start, 90.0, 0.0);
    // Heading 90 means the camera now stands on the aircraft's left (-x), looking right.
    CHECK(out.x == doctest::Approx(-10.0 * std::cos(30.0 * kRad)));
    CHECK(out.y == doctest::Approx(start.y));
    CHECK(std::fabs(out.z) < 1e-9);
    CHECK(out.heading == doctest::Approx(90.0));
}

TEST_CASE("orbit: an off-centre camera keeps its aim offset and its horizon level (no roll from dragging)") {
    Pose start = lookingAtOrigin(-40.0, 15.0, 25.0);
    start.zoom = 2.5;
    start.heading = mykam::wrap360(start.heading + 20.0);  // the aircraft is off-centre on screen
    start.pitch += 5.0;
    REQUIRE(start.roll == doctest::Approx(0.0));

    // Pure vertical drag: the bug was a rolling horizon here.
    const Pose up = mykam::orbitAroundOrigin(start, 0.0, 20.0);
    CHECK(dist(up) == doctest::Approx(25.0));
    CHECK(up.roll == doctest::Approx(0.0));
    CHECK(up.heading == doctest::Approx(start.heading));
    CHECK(up.pitch == doctest::Approx(start.pitch - 20.0));
    CHECK(elevationOf(up) == doctest::Approx(35.0));

    // Pure horizontal drag: heading follows the azimuth, pitch and roll stay.
    const Pose side = mykam::orbitAroundOrigin(start, 50.0, 0.0);
    CHECK(dist(side) == doctest::Approx(25.0));
    CHECK(side.roll == doctest::Approx(0.0));
    CHECK(side.pitch == doctest::Approx(start.pitch));
    CHECK(side.heading == doctest::Approx(mykam::wrap360(start.heading + 50.0)));

    const Pose out = mykam::orbitAroundOrigin(start, 50.0, 20.0);
    CHECK(out.roll == doctest::Approx(0.0));
    CHECK(out.zoom == doctest::Approx(2.5));

    Pose rolled = lookingAtOrigin(10.0, 5.0, 12.0);
    rolled.roll = 15.0;
    const Pose rolledOut = mykam::orbitAroundOrigin(rolled, -30.0, 10.0);
    CHECK(rolledOut.roll == doctest::Approx(15.0).epsilon(1e-6));
    CHECK(nearZero(pointAhead(rolledOut, 12.0), 1e-6));
}

TEST_CASE("orbit: elevation is limited to minus 85 and plus 85 degrees") {
    const Pose start = lookingAtOrigin(0.0, 60.0, 40.0);
    const Pose up = mykam::orbitAroundOrigin(start, 0.0, 500.0);
    CHECK(elevationOf(up) == doctest::Approx(85.0));
    CHECK(dist(up) == doctest::Approx(40.0));
    CHECK(nearZero(pointAhead(up, 40.0), 1e-6));

    const Pose down = mykam::orbitAroundOrigin(start, 0.0, -500.0);
    CHECK(elevationOf(down) == doctest::Approx(-85.0));
    CHECK(dist(down) == doctest::Approx(40.0));

    // A small step inside the limit is not clamped.
    CHECK(elevationOf(mykam::orbitAroundOrigin(start, 0.0, 10.0)) == doctest::Approx(70.0));
}

TEST_CASE("orbit: a full turn comes back, zero motion changes nothing, a camera at the origin is left alone") {
    const Pose start = lookingAtOrigin(33.0, 12.0, 18.0);
    const Pose round = mykam::orbitAroundOrigin(start, 360.0, 0.0);
    CHECK(round.x == doctest::Approx(start.x));
    CHECK(round.y == doctest::Approx(start.y));
    CHECK(round.z == doctest::Approx(start.z));

    const Pose same = mykam::orbitAroundOrigin(start, 0.0, 0.0);
    CHECK(same.x == doctest::Approx(start.x));
    CHECK(same.heading == doctest::Approx(start.heading));
    CHECK(same.pitch == doctest::Approx(start.pitch));

    Pose atOrigin;
    atOrigin.heading = 70.0;
    const Pose unchanged = mykam::orbitAroundOrigin(atOrigin, 30.0, 30.0);
    CHECK(unchanged.heading == doctest::Approx(70.0));
    CHECK(dist(unchanged) == 0.0);
}

// ---------------------------------------------------------------- wheel maths

TEST_CASE("wheel: one click in is 90 percent of the distance, one click out is 1 over 0.9, direction kept") {
    const Pose start = lookingAtOrigin(25.0, 20.0, 50.0);
    const Pose in = mykam::dollyTowardOrigin(start, 1);
    CHECK(dist(in) == doctest::Approx(45.0));
    CHECK(nearZero(pointAhead(in, 45.0), 1e-6));  // still on the line to the origin, still looking at it
    CHECK(in.heading == doctest::Approx(start.heading));
    CHECK(in.pitch == doctest::Approx(start.pitch));
    CHECK(in.zoom == doctest::Approx(start.zoom));

    CHECK(dist(mykam::dollyTowardOrigin(start, 3)) == doctest::Approx(50.0 * 0.9 * 0.9 * 0.9));
    CHECK(dist(mykam::dollyTowardOrigin(start, -1)) == doctest::Approx(50.0 / 0.9));
    CHECK(dist(mykam::dollyTowardOrigin(start, 0)) == doctest::Approx(50.0));
    // One click in and one out return to the start.
    CHECK(dist(mykam::dollyTowardOrigin(in, -1)) == doctest::Approx(50.0));
}

TEST_CASE("wheel: the distance stops at 2 m and at 2000 m") {
    const Pose closeCam = lookingAtOrigin(0.0, 0.0, 2.1);
    CHECK(dist(mykam::dollyTowardOrigin(closeCam, 5)) == doctest::Approx(2.0));
    const Pose atMin = lookingAtOrigin(0.0, 0.0, 2.0);
    CHECK(dist(mykam::dollyTowardOrigin(atMin, 1)) == doctest::Approx(2.0));
    CHECK(dist(mykam::dollyTowardOrigin(atMin, 1000)) == doctest::Approx(2.0));  // no overflow, no NaN

    const Pose distantCam = lookingAtOrigin(0.0, 0.0, 1900.0);
    CHECK(dist(mykam::dollyTowardOrigin(distantCam, -5)) == doctest::Approx(2000.0));
    const Pose atMax = lookingAtOrigin(0.0, 0.0, 2000.0);
    CHECK(dist(mykam::dollyTowardOrigin(atMax, -1)) == doctest::Approx(2000.0));
    CHECK(dist(mykam::dollyTowardOrigin(atMax, -1000)) == doctest::Approx(2000.0));
}

TEST_CASE("wheel: a camera already outside the limits can still move back towards them, never further out") {
    const Pose inside = lookingAtOrigin(0.0, 0.0, 1.0);  // closer than 2 m, e.g. a slot at the wing
    CHECK(dist(mykam::dollyTowardOrigin(inside, 1)) == doctest::Approx(1.0));  // not pushed out to 2 m
    CHECK(dist(mykam::dollyTowardOrigin(inside, -1)) == doctest::Approx(1.0 / 0.9));

    const Pose beyond = lookingAtOrigin(0.0, 0.0, 3000.0);
    CHECK(dist(mykam::dollyTowardOrigin(beyond, -1)) == doctest::Approx(3000.0));
    CHECK(dist(mykam::dollyTowardOrigin(beyond, 1)) == doctest::Approx(2700.0));

    Pose atOrigin;
    CHECK(dist(mykam::dollyTowardOrigin(atOrigin, -3)) == 0.0);
}

// ---------------------------------------------------------------- controller: mouse capture

TEST_CASE("capture: on only while an external camera is held, once, and off on release") {
    Rig rig("0");
    rig.saveExternal(2, lookingAtOrigin(0.0, 15.0, 20.0));
    CHECK_FALSE(rig.sim.mouseCapture);

    rig.controller.goToSlot(2);
    CHECK(rig.controller.state() == CameraState::Holding);
    CHECK(rig.sim.mouseCapture);
    CHECK(rig.sim.mouseCaptureCalls == 1);

    rig.controller.goToSlot(2);  // retarget while held: still on, no repeated call
    CHECK(rig.sim.mouseCapture);
    CHECK(rig.sim.mouseCaptureCalls == 1);

    rig.controller.release();
    CHECK_FALSE(rig.sim.mouseCapture);
    CHECK(rig.sim.mouseCaptureCalls == 2);
    rig.controller.release();  // already off: nothing more
    CHECK(rig.sim.mouseCaptureCalls == 2);
}

TEST_CASE("capture: also on during a transition, and stays on when it ends") {
    Rig rig("2");
    rig.saveExternal(2, lookingAtOrigin(0.0, 15.0, 20.0));
    rig.controller.goToSlot(2);
    CHECK(rig.controller.state() == CameraState::Transitioning);
    CHECK(rig.sim.mouseCapture);

    Pose w;
    rig.controller.tick(5.0, w);
    CHECK(rig.controller.state() == CameraState::Holding);
    CHECK(rig.sim.mouseCapture);
    CHECK(rig.sim.mouseCaptureCalls == 1);
}

TEST_CASE("capture: a cockpit slot never turns it on, and turns it off when an outside camera was held") {
    Rig rig("0");
    rig.saveCockpit(1);
    rig.saveExternal(2, lookingAtOrigin(0.0, 15.0, 20.0));

    rig.sim.viewExternal = false;
    rig.controller.goToSlot(1);
    CHECK_FALSE(rig.sim.mouseCapture);
    CHECK(rig.sim.mouseCaptureCalls == 0);

    rig.controller.goToSlot(2);
    REQUIRE(rig.sim.mouseCapture);
    rig.controller.goToSlot(1);
    CHECK(rig.controller.state() == CameraState::Idle);
    CHECK_FALSE(rig.sim.mouseCapture);
}

TEST_CASE("capture: off on aircraft change, on stop and when X-Plane takes the camera away") {
    Rig rig("0");
    rig.saveExternal(2, lookingAtOrigin(0.0, 15.0, 20.0));

    rig.controller.goToSlot(2);
    REQUIRE(rig.sim.mouseCapture);
    rig.controller.onAircraftLoaded("Baron_58.acf");
    CHECK_FALSE(rig.sim.mouseCapture);

    rig.saveExternal(2, lookingAtOrigin(0.0, 15.0, 20.0));
    rig.controller.goToSlot(2);
    REQUIRE(rig.sim.mouseCapture);
    rig.controller.stop();
    CHECK_FALSE(rig.sim.mouseCapture);

    rig.controller.onAircraftLoaded("Cessna_172SP.acf");
    rig.controller.goToSlot(2);
    REQUIRE(rig.sim.mouseCapture);
    rig.controller.onCameraLost();
    CHECK(rig.controller.state() == CameraState::Idle);
    CHECK_FALSE(rig.sim.mouseCapture);
}

TEST_CASE("capture: stays off when the camera could not be taken or another plugin holds it") {
    Rig rig("0");
    rig.saveExternal(2, lookingAtOrigin(0.0, 15.0, 20.0));

    rig.sim.takeResult = false;
    rig.controller.goToSlot(2);
    CHECK_FALSE(rig.sim.mouseCapture);

    rig.sim.takeResult = true;
    rig.sim.heldByOther = true;
    rig.controller.goToSlot(2);
    CHECK_FALSE(rig.sim.mouseCapture);
    CHECK(rig.sim.mouseCaptureCalls == 0);
}

// ---------------------------------------------------------------- controller: mouse events

TEST_CASE("mouse: nothing held means the events are ignored and change nothing") {
    Rig rig("0");
    CHECK_FALSE(rig.controller.onMouseDown(100, 100));
    CHECK_FALSE(rig.controller.onMouseDrag(150, 120));
    CHECK_FALSE(rig.controller.onMouseUp());
    CHECK_FALSE(rig.controller.onMouseWheel(3));
    CHECK(rig.controller.state() == CameraState::Idle);
    CHECK(rig.sim.takeCalls == 0);
}

TEST_CASE("mouse: dragging turns the held camera in place like X-Plane's free camera") {
    Rig rig("0");
    const Pose saved = lookingAtOrigin(0.0, 15.0, 20.0);
    rig.saveExternal(2, saved);
    rig.controller.goToSlot(2);

    REQUIRE(rig.controller.onMouseDown(500, 400));
    CHECK(dist(rig.placed()) == doctest::Approx(20.0));  // pressing alone moves nothing
    CHECK(rig.placed().heading == doctest::Approx(0.0));

    // 100 pixels right and 40 pixels up: 25 degrees to the right, 10 degrees up, same position.
    REQUIRE(rig.controller.onMouseDrag(600, 440));
    Pose w = rig.placed();
    CHECK(w.x == doctest::Approx(saved.x));
    CHECK(w.y == doctest::Approx(saved.y));
    CHECK(w.z == doctest::Approx(saved.z));
    CHECK(w.heading == doctest::Approx(25.0));
    CHECK(w.pitch == doctest::Approx(saved.pitch + 10.0));
    CHECK(w.roll == doctest::Approx(0.0));

    // The next drag step is relative to the previous position, not to the press.
    REQUIRE(rig.controller.onMouseDrag(600, 440));
    CHECK(rig.placed().heading == doctest::Approx(25.0));
    REQUIRE(rig.controller.onMouseDrag(560, 440));  // 40 pixels left: back 10 degrees
    CHECK(rig.placed().heading == doctest::Approx(15.0));

    CHECK(rig.controller.onMouseUp());
    REQUIRE(rig.controller.onMouseDrag(0, 0));  // a stray drag after the release only re-anchors
    CHECK(rig.placed().heading == doctest::Approx(15.0));
    CHECK(rig.controller.state() == CameraState::Holding);
}

TEST_CASE("mouse: dragging far up or down stops at straight up and straight down") {
    Rig rig("0");
    rig.saveExternal(2, lookingAtOrigin(0.0, 15.0, 20.0));
    rig.controller.goToSlot(2);
    rig.controller.onMouseDown(0, 0);
    rig.controller.onMouseDrag(0, 2000);  // up is +y in X-Plane
    CHECK(rig.placed().pitch == doctest::Approx(89.0));
    rig.controller.onMouseDrag(0, -2000);
    CHECK(rig.placed().pitch == doctest::Approx(-89.0));
    CHECK(dist(rig.placed()) == doctest::Approx(20.0));  // it never moved
}

TEST_CASE("mouse: looking around keeps the horizon level on a banked, pitched aircraft") {
    Rig rig("0");
    rig.saveExternal(2, lookingAtOrigin(0.0, 15.0, 20.0));
    rig.controller.goToSlot(2);

    rig.sim.aircraft.heading = 90.0;
    rig.sim.aircraft.pitch = 5.0;
    rig.sim.aircraft.roll = -10.0;
    const Pose before = rig.placed();

    rig.controller.onMouseDown(0, 0);
    rig.controller.onMouseDrag(0, 40);  // straight up: 10 degrees
    const Pose upWorld = rig.placed();
    CHECK(upWorld.x == doctest::Approx(before.x));
    CHECK(upWorld.y == doctest::Approx(before.y));
    CHECK(upWorld.z == doctest::Approx(before.z));
    CHECK(upWorld.heading == doctest::Approx(before.heading));
    CHECK(upWorld.roll == doctest::Approx(before.roll));
    CHECK(upWorld.pitch == doctest::Approx(before.pitch + 10.0));

    rig.controller.onMouseDrag(80, 40);  // straight right: 20 degrees
    const Pose rightWorld = rig.placed();
    CHECK(rightWorld.heading == doctest::Approx(mykam::wrap360(before.heading + 20.0)));
    CHECK(rightWorld.pitch == doctest::Approx(before.pitch + 10.0));
    CHECK(rightWorld.roll == doctest::Approx(before.roll));
}

TEST_CASE("mouse: the turned camera keeps following the aircraft") {
    Rig rig("0");
    rig.saveExternal(2, lookingAtOrigin(0.0, 15.0, 20.0));
    rig.controller.goToSlot(2);
    rig.controller.onMouseDown(0, 0);
    rig.controller.onMouseDrag(80, 0);  // 20 degrees to the right

    rig.sim.aircraft.x = 1000.0;
    rig.sim.aircraft.y = 300.0;
    rig.sim.aircraft.z = -500.0;
    rig.sim.aircraft.heading = 90.0;

    const Pose world = rig.placed();
    const Pose body = mykam::worldToBody(world, rig.sim.aircraft);
    CHECK(dist(body) == doctest::Approx(20.0));
    CHECK(body.heading == doctest::Approx(20.0));
}

TEST_CASE("mouse: the wheel moves the held camera closer and farther, within the limits") {
    Rig rig("0");
    rig.saveExternal(2, lookingAtOrigin(10.0, 15.0, 20.0));
    rig.controller.goToSlot(2);

    CHECK(rig.controller.onMouseWheel(2));
    CHECK(dist(rig.placed()) == doctest::Approx(20.0 * 0.81));
    CHECK(nearZero(pointAhead(rig.placed(), dist(rig.placed())), 1e-4));
    CHECK(rig.controller.onMouseWheel(-2));
    CHECK(dist(rig.placed()) == doctest::Approx(20.0));

    CHECK(rig.controller.onMouseWheel(200));
    CHECK(dist(rig.placed()) == doctest::Approx(2.0));
    CHECK(rig.controller.onMouseWheel(-400));
    CHECK(dist(rig.placed()) == doctest::Approx(2000.0));
}

TEST_CASE("mouse: grabbing the camera during a transition stops the blend where it is and holds that view") {
    Rig rig("2");
    rig.saveExternal(2, lookingAtOrigin(0.0, 15.0, 20.0));
    rig.sim.view = lookingAtOrigin(90.0, 15.0, 20.0);  // the view we start from
    rig.controller.goToSlot(2);
    REQUIRE(rig.controller.state() == CameraState::Transitioning);

    Pose w;
    rig.controller.tick(1.0, w);  // halfway
    REQUIRE(rig.controller.state() == CameraState::Transitioning);

    REQUIRE(rig.controller.onMouseWheel(1));
    CHECK(rig.controller.state() == CameraState::Holding);
    CHECK(rig.sim.mouseCapture);
    const Pose held = rig.placed();
    rig.controller.tick(5.0, w);  // the old blend must not resume
    CHECK(w.x == doctest::Approx(held.x));
    CHECK(w.heading == doctest::Approx(held.heading));
}

TEST_CASE("mouse: saving after a look-around drag and a wheel move stores the adjusted pose") {
    Rig rig("0");
    rig.saveExternal(2, lookingAtOrigin(0.0, 15.0, 20.0));
    rig.controller.goToSlot(2);
    rig.controller.onMouseDown(0, 0);
    rig.controller.onMouseDrag(120, 0);  // 30 degrees to the right, in place
    rig.controller.onMouseWheel(2);
    rig.controller.onMouseUp();
    const Pose expected = mykam::dollyTowardOrigin(
        mykam::lookAroundInPlace(lookingAtOrigin(0.0, 15.0, 20.0), rig.sim.aircraft, 30.0, 0.0), 2);

    rig.sim.view = lookingAtOrigin(0.0, 0.0, 1.0);  // X-Plane's own view must not be what gets saved
    rig.controller.saveSlot(3);

    REQUIRE(rig.controller.slots().has(3));
    CHECK(rig.controller.slots().kind(3) == SlotKind::External);
    const Pose* saved = rig.controller.slots().get(3);
    CHECK(saved->x == doctest::Approx(expected.x).epsilon(1e-6));
    CHECK(saved->y == doctest::Approx(expected.y).epsilon(1e-6));
    CHECK(saved->z == doctest::Approx(expected.z).epsilon(1e-6));
    CHECK(saved->heading == doctest::Approx(expected.heading).epsilon(1e-6));
    CHECK(saved->pitch == doctest::Approx(expected.pitch).epsilon(1e-6));

    // The adjusted pose really differs from the first one, and goes back to it later.
    CHECK(saved->heading == doctest::Approx(30.0));
    CHECK(dist(*saved) == doctest::Approx(20.0 * 0.81));
    CHECK(rig.sim.files["plug/cameras/Cessna_172SP.slots.txt"].find(".hotkey=3\n") != std::string::npos);
}

TEST_CASE("mouse: no file access or logging for mouse events") {
    Rig rig("0");
    rig.saveExternal(2, lookingAtOrigin(0.0, 15.0, 20.0));
    rig.controller.goToSlot(2);
    rig.sim.logs.clear();
    const int ops = rig.sim.fileOps();

    rig.controller.onMouseDown(0, 0);
    for (int i = 1; i <= 50; ++i) {
        rig.controller.onMouseDrag(i * 3, i);
    }
    rig.controller.onMouseWheel(1);
    rig.controller.onMouseUp();

    CHECK(rig.sim.fileOps() == ops);
    CHECK(rig.sim.logs.empty());
}
