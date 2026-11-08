#pragma once

#include "pose.h"

// Mouse control of a camera that mykam holds outside the aircraft: orbit around the aircraft
// reference point (the body-frame origin) and move closer or farther along the line to it. Pure
// code, no allocation, safe for the mouse event path.

namespace mykam {

constexpr double kOrbitDegreesPerPixel = 0.25;
constexpr double kMinOrbitElevationDegrees = -85.0;
constexpr double kMaxOrbitElevationDegrees = 85.0;
constexpr double kMinCameraDistanceMetres = 2.0;
constexpr double kMaxCameraDistanceMetres = 2000.0;
constexpr double kWheelClickFactor = 0.9;  // one click in = 90 percent of the distance; out = 1 / 0.9

// Turn the body-frame camera around the body origin: azimuth moves it around the vertical axis,
// elevation up or down, keeping the distance. The camera turns by the same amounts, so whatever it
// looked at (the origin when it looks at it) stays where it is on screen. Elevation is limited to
// -85 to +85 degrees. Positive azimuth moves the camera towards the aircraft's left (the camera
// heading grows); positive elevation raises it. A camera at the origin is returned unchanged.
Pose orbitAroundOrigin(const Pose& body, double azimuthDegrees, double elevationDegrees);

// Look around in place, like X-Plane's free camera: the camera stays where it is and turns its view.
// Yaw (positive = to the right) changes the heading and pitch (positive = up) the tilt, both measured
// against the real horizon (world frame), so the horizon stays level whatever the aircraft does.
// World pitch is limited to -89 to +89 degrees; roll is kept. The result is in the body frame again.
constexpr double kLookDegreesPerPixel = 0.25;
constexpr double kMaxLookPitchDegrees = 89.0;
Pose lookAroundInPlace(const Pose& body, const AircraftState& aircraft, double yawDegrees, double pitchDegrees);

// Move the body-frame camera along the line to the body origin. Positive clicks go closer (each
// click to 90 percent of the distance), negative go farther (each click by 1 / 0.9). The distance
// ends between 2 m and 2000 m; a camera already outside that range is never pushed back towards
// it by the wheel in the other direction. Angles and zoom are unchanged. A camera at the origin
// is returned unchanged.
Pose dollyTowardOrigin(const Pose& body, int clicks);

}  // namespace mykam
