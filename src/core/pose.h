#pragma once

// Pure camera math. No X-Plane headers (NFR4.1).
//
// Coordinate conventions (the same ones X-Plane uses for local/OpenGL coordinates):
//   world: +x east, +y up, +z south. Metres.
//   body (FR2): +x right of the aircraft, +y up, +z back. Metres from the aircraft reference point.
// Angles are degrees. Heading is clockwise from north, pitch is nose up, roll is right wing down.
// A camera with all angles zero looks along -z (north in world, forward in body).

namespace mykam {

struct Pose {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    double pitch = 0.0;
    double heading = 0.0;
    double roll = 0.0;
    double zoom = 1.0;
};

// Aircraft position (world metres) and attitude (theta = pitch, psi = heading, phi = roll).
struct AircraftState {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    double pitch = 0.0;
    double heading = 0.0;
    double roll = 0.0;
};

// Wrap an angle to [0, 360).
double wrap360(double degrees);
// Wrap an angle to (-180, 180].
double wrap180(double degrees);

// Convert a world-space camera into the aircraft body frame, and back (FR2.1, FR3).
// Zoom is copied unchanged. Heading is returned in [0, 360).
Pose worldToBody(const Pose& world, const AircraftState& aircraft);
Pose bodyToWorld(const Pose& body, const AircraftState& aircraft);

// Move a camera pose (any frame) along its own axes: right, up and forward are metres in the
// camera's frame. Angles and zoom are unchanged.
Pose moveInCameraFrame(const Pose& pose, double right, double up, double forward);
// Turn a camera pose about its own axes: yaw (positive = to the right) about the camera's up axis,
// then pitch (positive = up) about its right axis, both in degrees. Position and zoom are unchanged.
Pose rotateInCameraFrame(const Pose& pose, double yawDegrees, double pitchDegrees);

}  // namespace mykam
