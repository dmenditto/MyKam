#include "pose.h"

#include <cmath>

namespace mykam {

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kDegToRad = kPi / 180.0;
constexpr double kRadToDeg = 180.0 / kPi;

// Row-major 3x3 rotation matrix acting on column vectors (x, y, z).
struct Mat3 {
    double m[3][3];
};

Mat3 multiply(const Mat3& a, const Mat3& b) {
    Mat3 r{};
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            double s = 0.0;
            for (int k = 0; k < 3; ++k) {
                s += a.m[i][k] * b.m[k][j];
            }
            r.m[i][j] = s;
        }
    }
    return r;
}

Mat3 transpose(const Mat3& a) {
    Mat3 r{};
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            r.m[i][j] = a.m[j][i];
        }
    }
    return r;
}

// Orientation of an object with the given Euler angles: R = Ry(heading) * Rx(pitch) * Rz(roll).
// Heading turns clockwise seen from above, pitch lifts the nose, roll drops the right wing.
Mat3 orientation(double pitchDeg, double headingDeg, double rollDeg) {
    const double cp = std::cos(pitchDeg * kDegToRad);
    const double sp = std::sin(pitchDeg * kDegToRad);
    const double ch = std::cos(headingDeg * kDegToRad);
    const double sh = std::sin(headingDeg * kDegToRad);
    const double cr = std::cos(rollDeg * kDegToRad);
    const double sr = std::sin(rollDeg * kDegToRad);
    Mat3 r{};
    r.m[0][0] = ch * cr + sh * sp * sr;
    r.m[0][1] = ch * sr - sh * sp * cr;
    r.m[0][2] = -sh * cp;
    r.m[1][0] = -cp * sr;
    r.m[1][1] = cp * cr;
    r.m[1][2] = -sp;
    r.m[2][0] = sh * cr - ch * sp * sr;
    r.m[2][1] = sh * sr + ch * sp * cr;
    r.m[2][2] = ch * cp;
    return r;
}

struct Euler {
    double pitch;
    double heading;
    double roll;
};

Euler toEuler(const Mat3& r) {
    double s = -r.m[1][2];
    if (s > 1.0) {
        s = 1.0;
    } else if (s < -1.0) {
        s = -1.0;
    }
    Euler e{};
    e.pitch = std::asin(s) * kRadToDeg;
    const double cosPitch = std::sqrt(r.m[0][2] * r.m[0][2] + r.m[2][2] * r.m[2][2]);
    if (cosPitch > 1e-9) {
        e.heading = std::atan2(-r.m[0][2], r.m[2][2]) * kRadToDeg;
        e.roll = std::atan2(-r.m[1][0], r.m[1][1]) * kRadToDeg;
    } else {
        // Looking straight up or down: heading and roll are not separable; fold roll into heading.
        e.heading = std::atan2(r.m[2][0], r.m[0][0]) * kRadToDeg;
        e.roll = 0.0;
    }
    e.heading = wrap360(e.heading);
    return e;
}

Mat3 aircraftOrientation(const AircraftState& a) {
    return orientation(a.pitch, a.heading, a.roll);
}

}  // namespace

double wrap360(double degrees) {
    double r = std::fmod(degrees, 360.0);
    if (r < 0.0) {
        r += 360.0;
    }
    if (r >= 360.0) {
        r = 0.0;
    }
    return r;
}

double wrap180(double degrees) {
    double r = wrap360(degrees);
    if (r > 180.0) {
        r -= 360.0;
    }
    return r;
}

Pose worldToBody(const Pose& world, const AircraftState& aircraft) {
    const Mat3 rAc = aircraftOrientation(aircraft);
    const Mat3 rAcInv = transpose(rAc);
    const double dx = world.x - aircraft.x;
    const double dy = world.y - aircraft.y;
    const double dz = world.z - aircraft.z;

    Pose body;
    body.x = rAcInv.m[0][0] * dx + rAcInv.m[0][1] * dy + rAcInv.m[0][2] * dz;
    body.y = rAcInv.m[1][0] * dx + rAcInv.m[1][1] * dy + rAcInv.m[1][2] * dz;
    body.z = rAcInv.m[2][0] * dx + rAcInv.m[2][1] * dy + rAcInv.m[2][2] * dz;

    const Mat3 rCam = orientation(world.pitch, world.heading, world.roll);
    const Euler e = toEuler(multiply(rAcInv, rCam));
    body.pitch = e.pitch;
    body.heading = e.heading;
    body.roll = e.roll;
    body.zoom = world.zoom;
    return body;
}

Pose moveInCameraFrame(const Pose& pose, double right, double up, double forward) {
    const Mat3 r = orientation(pose.pitch, pose.heading, pose.roll);
    // Camera axes: +x right, +y up, and it looks along -z, so forward is -z.
    const double lx = right;
    const double ly = up;
    const double lz = -forward;
    Pose out = pose;
    out.x += r.m[0][0] * lx + r.m[0][1] * ly + r.m[0][2] * lz;
    out.y += r.m[1][0] * lx + r.m[1][1] * ly + r.m[1][2] * lz;
    out.z += r.m[2][0] * lx + r.m[2][1] * ly + r.m[2][2] * lz;
    return out;
}

Pose rotateInCameraFrame(const Pose& pose, double yawDegrees, double pitchDegrees) {
    const Mat3 r = orientation(pose.pitch, pose.heading, pose.roll);
    const Euler e = toEuler(multiply(r, orientation(pitchDegrees, yawDegrees, 0.0)));
    Pose out = pose;
    out.pitch = e.pitch;
    out.heading = e.heading;
    out.roll = e.roll;
    return out;
}

Pose bodyToWorld(const Pose& body, const AircraftState& aircraft) {
    const Mat3 rAc = aircraftOrientation(aircraft);

    Pose world;
    world.x = aircraft.x + rAc.m[0][0] * body.x + rAc.m[0][1] * body.y + rAc.m[0][2] * body.z;
    world.y = aircraft.y + rAc.m[1][0] * body.x + rAc.m[1][1] * body.y + rAc.m[1][2] * body.z;
    world.z = aircraft.z + rAc.m[2][0] * body.x + rAc.m[2][1] * body.y + rAc.m[2][2] * body.z;

    const Mat3 rBody = orientation(body.pitch, body.heading, body.roll);
    const Euler e = toEuler(multiply(rAc, rBody));
    world.pitch = e.pitch;
    world.heading = e.heading;
    world.roll = e.roll;
    world.zoom = body.zoom;
    return world;
}

}  // namespace mykam
