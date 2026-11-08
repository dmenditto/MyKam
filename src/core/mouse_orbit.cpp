#include "mouse_orbit.h"

#include <algorithm>
#include <cmath>

namespace mykam {

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kDegToRad = kPi / 180.0;
constexpr double kRadToDeg = 180.0 / kPi;
constexpr double kTinyDistance = 1e-6;

double distanceOf(const Pose& p) {
    return std::sqrt(p.x * p.x + p.y * p.y + p.z * p.z);
}

// Camera position for azimuth a, elevation e, distance d:
// x = -d sin(a) cos(e), y = d sin(e), z = d cos(a) cos(e); looking at the origin from there means
// heading a and pitch -e.

}  // namespace

Pose orbitAroundOrigin(const Pose& body, double azimuthDegrees, double elevationDegrees) {
    const double d = distanceOf(body);
    if (!(d > kTinyDistance) || !std::isfinite(azimuthDegrees) || !std::isfinite(elevationDegrees)) {
        return body;
    }
    const double az = std::atan2(-body.x, body.z) * kRadToDeg;
    const double el = std::asin(std::min(std::max(body.y / d, -1.0), 1.0)) * kRadToDeg;
    const double newAz = az + azimuthDegrees;
    const double newEl = std::min(std::max(el + elevationDegrees, kMinOrbitElevationDegrees), kMaxOrbitElevationDegrees);

    // Move the camera on the sphere around the origin, then turn its view by the same amounts:
    // heading follows the azimuth change and pitch follows the elevation change (raising the camera
    // tilts it down). Roll is never touched, so the horizon stays level while dragging, as in
    // X-Plane's own outside views. A camera aimed at the origin keeps aiming at it; one aimed off
    // centre keeps the same heading and pitch offset.
    const double dAz = newAz - az;
    const double dEl = newEl - el;
    const double cosEl = std::cos(newEl * kDegToRad);
    Pose out = body;
    out.x = -d * std::sin(newAz * kDegToRad) * cosEl;
    out.y = d * std::sin(newEl * kDegToRad);
    out.z = d * std::cos(newAz * kDegToRad) * cosEl;
    out.heading = wrap360(body.heading + dAz);
    out.pitch = std::min(std::max(body.pitch - dEl, -89.0), 89.0);
    return out;
}

Pose lookAroundInPlace(const Pose& body, const AircraftState& aircraft, double yawDegrees, double pitchDegrees) {
    if (!std::isfinite(yawDegrees) || !std::isfinite(pitchDegrees)) {
        return body;
    }
    Pose world = bodyToWorld(body, aircraft);
    world.heading = wrap360(world.heading + yawDegrees);
    world.pitch = std::min(std::max(world.pitch + pitchDegrees, -kMaxLookPitchDegrees), kMaxLookPitchDegrees);
    Pose out = worldToBody(world, aircraft);
    // Position and zoom are untouched; keep them exactly rather than through the round trip.
    out.x = body.x;
    out.y = body.y;
    out.z = body.z;
    out.zoom = body.zoom;
    return out;
}

Pose dollyTowardOrigin(const Pose& body, int clicks) {
    const double d = distanceOf(body);
    if (!(d > kTinyDistance) || clicks == 0) {
        return body;
    }
    const double factor = clicks > 0 ? std::pow(kWheelClickFactor, static_cast<double>(clicks))
                                     : std::pow(1.0 / kWheelClickFactor, -static_cast<double>(clicks));
    // Never push a camera that is already too close or too far back in by the wheel moving it the
    // other way: the limits only stop it from going further out of range.
    const double lowest = std::min(d, kMinCameraDistanceMetres);
    const double highest = std::max(d, kMaxCameraDistanceMetres);
    const double newD = std::min(std::max(d * factor, lowest), highest);
    const double scale = newD / d;
    Pose out = body;
    out.x *= scale;
    out.y *= scale;
    out.z *= scale;
    return out;
}

}  // namespace mykam
