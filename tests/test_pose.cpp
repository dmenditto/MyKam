#include "doctest.h"

#include <cmath>

#include "pose.h"

using mykam::AircraftState;
using mykam::Pose;

namespace {

double angleDiff(double a, double b) {
    return std::fabs(mykam::wrap180(a - b));
}

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

void checkSamePose(const Pose& a, const Pose& b) {
    CHECK(std::fabs(a.x - b.x) < 0.01);  // within 1 cm (FR3)
    CHECK(std::fabs(a.y - b.y) < 0.01);
    CHECK(std::fabs(a.z - b.z) < 0.01);
    CHECK(angleDiff(a.pitch, b.pitch) < 0.1);  // within 0.1 degrees (FR3)
    CHECK(angleDiff(a.heading, b.heading) < 0.1);
    CHECK(angleDiff(a.roll, b.roll) < 0.1);
    CHECK(a.zoom == doctest::Approx(b.zoom));
}

}  // namespace

TEST_CASE("pose: world to body and back returns the original view (FR3)") {
    const AircraftState ac = makeAircraft(12000.0, 850.0, -34000.0, 12.0, 135.0, -20.0);
    const Pose world = makePose(12010.0, 860.0, -33980.0, -15.0, 200.0, 5.0, 1.8);
    const Pose body = mykam::worldToBody(world, ac);
    const Pose back = mykam::bodyToWorld(body, ac);
    checkSamePose(world, back);
}

TEST_CASE("pose: round trip holds across many aircraft attitudes") {
    for (double heading = 0.0; heading < 360.0; heading += 47.0) {
        for (double pitch = -80.0; pitch <= 80.0; pitch += 40.0) {
            for (double roll = -170.0; roll <= 170.0; roll += 85.0) {
                const AircraftState ac = makeAircraft(-500.0, 100.0, 2500.0, pitch, heading, roll);
                const Pose world = makePose(-480.0, 108.0, 2520.0, 10.0, 333.0, -7.0, 2.5);
                checkSamePose(world, mykam::bodyToWorld(mykam::worldToBody(world, ac), ac));
            }
        }
    }
}

TEST_CASE("pose: body axes are right, up and back of the aircraft") {
    SUBCASE("level aircraft facing north") {
        const AircraftState ac = makeAircraft(100.0, 50.0, 200.0, 0.0, 0.0, 0.0);
        // 3 m east, 2 m up, 5 m south of the aircraft = 3 right, 2 up, 5 back.
        const Pose body = mykam::worldToBody(makePose(103.0, 52.0, 205.0, 0.0, 0.0, 0.0, 1.0), ac);
        CHECK(body.x == doctest::Approx(3.0));
        CHECK(body.y == doctest::Approx(2.0));
        CHECK(body.z == doctest::Approx(5.0));
    }
    SUBCASE("aircraft heading east: its right side points south") {
        const AircraftState ac = makeAircraft(0.0, 0.0, 0.0, 0.0, 90.0, 0.0);
        const Pose body = mykam::worldToBody(makePose(0.0, 0.0, 10.0, 0.0, 90.0, 0.0, 1.0), ac);
        CHECK(body.x == doctest::Approx(10.0));
        CHECK(std::fabs(body.z) < 1e-9);
    }
    SUBCASE("aircraft heading east: its tail is to the west") {
        const AircraftState ac = makeAircraft(0.0, 0.0, 0.0, 0.0, 90.0, 0.0);
        const Pose body = mykam::worldToBody(makePose(-10.0, 0.0, 0.0, 0.0, 90.0, 0.0, 1.0), ac);
        CHECK(body.z == doctest::Approx(10.0));
    }
}

TEST_CASE("pose: camera angles are relative to the aircraft attitude") {
    const AircraftState ac = makeAircraft(0.0, 0.0, 0.0, 0.0, 90.0, 0.0);
    SUBCASE("camera looking the same way as the aircraft has zero body angles") {
        const Pose body = mykam::worldToBody(makePose(0.0, 0.0, 0.0, 0.0, 90.0, 0.0, 1.0), ac);
        CHECK(angleDiff(body.heading, 0.0) < 1e-6);
        CHECK(angleDiff(body.pitch, 0.0) < 1e-6);
        CHECK(angleDiff(body.roll, 0.0) < 1e-6);
    }
    SUBCASE("a body heading of 90 looks to the right of an aircraft heading east, so south") {
        const Pose world = mykam::bodyToWorld(makePose(0.0, 0.0, 0.0, 0.0, 90.0, 0.0, 1.0), ac);
        CHECK(angleDiff(world.heading, 180.0) < 1e-6);
    }
}

TEST_CASE("pose: heading wraps across north") {
    CHECK(mykam::wrap360(-1.0) == doctest::Approx(359.0));
    CHECK(mykam::wrap360(361.0) == doctest::Approx(1.0));
    CHECK(mykam::wrap360(360.0) == doctest::Approx(0.0));
    CHECK(mykam::wrap180(190.0) == doctest::Approx(-170.0));
    CHECK(mykam::wrap180(-190.0) == doctest::Approx(170.0));

    const AircraftState ac = makeAircraft(0.0, 0.0, 0.0, 0.0, 1.0, 0.0);
    const Pose body = mykam::worldToBody(makePose(0.0, 0.0, 0.0, 0.0, 359.0, 0.0, 1.0), ac);
    CHECK(angleDiff(body.heading, 358.0) < 1e-6);
    CHECK(body.heading >= 0.0);
    CHECK(body.heading < 360.0);
}

TEST_CASE("pose: zoom is kept and a zero attitude aircraft only shifts the position") {
    const AircraftState ac = makeAircraft(10.0, 20.0, 30.0, 0.0, 0.0, 0.0);
    const Pose body = mykam::worldToBody(makePose(11.0, 22.0, 33.0, 5.0, 10.0, 15.0, 3.5), ac);
    CHECK(body.zoom == doctest::Approx(3.5));
    CHECK(body.x == doctest::Approx(1.0));
    CHECK(body.y == doctest::Approx(2.0));
    CHECK(body.z == doctest::Approx(3.0));
    CHECK(body.pitch == doctest::Approx(5.0));
    CHECK(angleDiff(body.heading, 10.0) < 1e-6);
    CHECK(body.roll == doctest::Approx(15.0));
}

TEST_CASE("pose: looking straight down stays straight down on a turned aircraft") {
    const AircraftState ac = makeAircraft(0.0, 0.0, 0.0, 0.0, 120.0, 0.0);
    const Pose world = mykam::bodyToWorld(makePose(0.0, 5.0, 0.0, -90.0, 0.0, 0.0, 1.0), ac);
    CHECK(world.pitch == doctest::Approx(-90.0));
    const Pose body = mykam::worldToBody(world, ac);
    CHECK(body.pitch == doctest::Approx(-90.0));
    CHECK(std::isfinite(body.heading));
    CHECK(std::isfinite(body.roll));
}
