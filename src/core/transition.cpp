#include "transition.h"

namespace mykam {

double easeInOut(double t) {
    if (!(t > 0.0)) {  // also catches NaN
        return 0.0;
    }
    if (t >= 1.0) {
        return 1.0;
    }
    return t * t * (3.0 - 2.0 * t);
}

namespace {

double lerp(double a, double b, double e) {
    return a + (b - a) * e;
}

// Interpolate angles along the shorter direction. The result is not wrapped.
double lerpAngle(double a, double b, double e) {
    return a + wrap180(b - a) * e;
}

}  // namespace

Pose blendPoses(const Pose& from, const Pose& to, double e) {
    Pose p;
    p.x = lerp(from.x, to.x, e);
    p.y = lerp(from.y, to.y, e);
    p.z = lerp(from.z, to.z, e);
    p.pitch = lerp(from.pitch, to.pitch, e);
    p.heading = wrap360(lerpAngle(from.heading, to.heading, e));
    p.roll = lerpAngle(from.roll, to.roll, e);
    p.zoom = lerp(from.zoom, to.zoom, e);
    return p;
}

void Transition::begin(const Pose& from, const Pose& to, double durationSeconds) {
    from_ = from;
    to_ = to;
    elapsed_ = 0.0;
    if (durationSeconds > 0.0) {
        duration_ = durationSeconds;
        active_ = true;
    } else {
        duration_ = 0.0;
        active_ = false;
    }
}

void Transition::update(double dtSeconds) {
    if (!active_) {
        return;
    }
    if (dtSeconds > 0.0) {
        elapsed_ += dtSeconds;
    }
    if (elapsed_ >= duration_) {
        elapsed_ = duration_;
        active_ = false;
    }
}

Pose Transition::current() const {
    if (!active_) {
        return to_;
    }
    return blendPoses(from_, to_, easeInOut(elapsed_ / duration_));
}

}  // namespace mykam
