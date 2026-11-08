#pragma once

#include "pose.h"

// Smooth blend between two camera poses (FR4). Pure code, no allocation: safe for the per-frame path.
// Poses are in the aircraft body frame, so the target keeps tracking the moving aircraft: the caller
// converts the blended pose to world coordinates every frame.

namespace mykam {

// Ease-in-out (smoothstep). t is clamped to [0, 1]; 0 maps to 0 and 1 maps to 1.
double easeInOut(double t);

// Blend two poses. e is the already-eased factor in [0, 1].
// Heading and roll take the shorter way round (FR4.2); position, pitch and zoom blend linearly.
Pose blendPoses(const Pose& from, const Pose& to, double e);

class Transition {
public:
    // Start a blend. A duration of 0 or less is an instant cut (FR4.1): the transition is finished
    // at once and current() returns `to`.
    void begin(const Pose& from, const Pose& to, double durationSeconds);

    // Advance by dt seconds (negative values count as 0).
    void update(double dtSeconds);

    // True while the blend has not reached its end.
    bool active() const { return active_; }

    // The in-between pose now. Starting a new transition from this value is retargeting (FR4.3).
    Pose current() const;

    double duration() const { return duration_; }
    double elapsed() const { return elapsed_; }

private:
    Pose from_{};
    Pose to_{};
    double duration_ = 0.0;
    double elapsed_ = 0.0;
    bool active_ = false;
};

}  // namespace mykam
