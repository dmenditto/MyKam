#include "doctest.h"

#include <cmath>

#include "pose.h"
#include "transition.h"

using mykam::Pose;
using mykam::Transition;

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

double angleDiff(double a, double b) {
    return std::fabs(mykam::wrap180(a - b));
}

}  // namespace

TEST_CASE("transition: ease curve has fixed endpoints, a centre and clamps (FR4)") {
    CHECK(mykam::easeInOut(0.0) == doctest::Approx(0.0));
    CHECK(mykam::easeInOut(1.0) == doctest::Approx(1.0));
    CHECK(mykam::easeInOut(0.5) == doctest::Approx(0.5));
    CHECK(mykam::easeInOut(0.25) < 0.25);  // slow start
    CHECK(mykam::easeInOut(0.75) > 0.75);  // slow finish
    CHECK(mykam::easeInOut(-3.0) == doctest::Approx(0.0));
    CHECK(mykam::easeInOut(7.0) == doctest::Approx(1.0));
}

TEST_CASE("transition: runs from the start pose to the target over the duration") {
    const Pose from = makePose(0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0);
    const Pose to = makePose(10.0, 4.0, -6.0, 20.0, 90.0, 10.0, 3.0);
    Transition t;
    t.begin(from, to, 2.0);
    CHECK(t.active());
    CHECK(t.current().x == doctest::Approx(0.0));

    t.update(1.0);  // halfway: the ease is 0.5
    CHECK(t.active());
    const Pose mid = t.current();
    CHECK(mid.x == doctest::Approx(5.0));
    CHECK(mid.y == doctest::Approx(2.0));
    CHECK(mid.z == doctest::Approx(-3.0));
    CHECK(mid.pitch == doctest::Approx(10.0));
    CHECK(angleDiff(mid.heading, 45.0) < 1e-9);
    CHECK(mid.roll == doctest::Approx(5.0));
    CHECK(mid.zoom == doctest::Approx(2.0));

    t.update(1.5);  // past the end: clamps to the target and finishes
    CHECK_FALSE(t.active());
    CHECK(t.current().x == doctest::Approx(10.0));
    CHECK(angleDiff(t.current().heading, 90.0) < 1e-9);
    CHECK(t.current().zoom == doctest::Approx(3.0));
}

TEST_CASE("transition: heading takes the shorter way across north (FR4.2)") {
    Transition t;
    t.begin(makePose(0, 0, 0, 0, 350.0, 0, 1.0), makePose(0, 0, 0, 0, 10.0, 0, 1.0), 1.0);
    t.update(0.5);
    // Halfway between 350 and 10 the short way is north (0), not 180.
    CHECK(angleDiff(t.current().heading, 0.0) < 1e-9);
    t.update(0.1);
    const double h = t.current().heading;
    CHECK((h < 10.0 || h > 350.0));

    Transition back;
    back.begin(makePose(0, 0, 0, 0, 10.0, 0, 1.0), makePose(0, 0, 0, 0, 350.0, 0, 1.0), 1.0);
    back.update(0.5);
    CHECK(angleDiff(back.current().heading, 0.0) < 1e-9);
}

TEST_CASE("transition: a duration of zero is an instant cut (FR4.1)") {
    const Pose to = makePose(1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 2.0);
    Transition t;
    t.begin(makePose(100, 100, 100, 0, 0, 0, 1.0), to, 0.0);
    CHECK_FALSE(t.active());
    CHECK(t.current().x == doctest::Approx(1.0));
    CHECK(t.current().zoom == doctest::Approx(2.0));

    Transition negative;
    negative.begin(makePose(100, 100, 100, 0, 0, 0, 1.0), to, -3.0);
    CHECK_FALSE(negative.active());
    CHECK(negative.current().z == doctest::Approx(3.0));
}

TEST_CASE("transition: starting a new one mid-way continues from the in-between pose (FR4.3)") {
    Transition t;
    t.begin(makePose(0, 0, 0, 0, 0, 0, 1.0), makePose(10, 0, 0, 0, 0, 0, 1.0), 1.0);
    t.update(0.3);
    const Pose inBetween = t.current();
    CHECK(inBetween.x > 0.0);
    CHECK(inBetween.x < 10.0);

    t.begin(inBetween, makePose(-10, 0, 0, 0, 0, 0, 1.0), 1.0);
    CHECK(t.active());
    CHECK(t.current().x == doctest::Approx(inBetween.x));  // no jump
    t.update(1.0);
    CHECK_FALSE(t.active());
    CHECK(t.current().x == doctest::Approx(-10.0));
}

TEST_CASE("transition: negative or zero time steps do not move it and update is safe when idle") {
    Transition t;
    t.update(1.0);  // never started: nothing happens
    CHECK_FALSE(t.active());

    t.begin(makePose(0, 0, 0, 0, 0, 0, 1.0), makePose(8, 0, 0, 0, 0, 0, 1.0), 2.0);
    t.update(-5.0);
    t.update(0.0);
    CHECK(t.elapsed() == doctest::Approx(0.0));
    CHECK(t.active());
    CHECK(t.current().x == doctest::Approx(0.0));
}
