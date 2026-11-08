#pragma once

#include "pose.h"

// X-Plane's own view commands (move, rotate, zoom) applied to a camera that mykam holds. While
// mykam places the camera X-Plane's view movement has no effect, so the plugin intercepts those
// commands and feeds them here. Pure code, no allocation: safe for the per-frame path.

namespace mykam {

enum class ViewAction {
    Left, Right, Up, Down, Forward, Backward,
    RotLeft, RotRight, RotUp, RotDown,
    ZoomIn, ZoomOut
};
constexpr int kViewActionCount = 12;

// X-Plane has a plain, a _fast and a _slow variant of each command.
enum class ViewSpeed { Normal, Fast, Slow };
constexpr int kViewSpeedCount = 3;

// The phase of an X-Plane command: pressed, still held, released.
enum class CommandPhase { Begin, Continue, End };

// Movement rates while a command is held, at normal speed.
constexpr double kMoveMetresPerSecond = 5.0;
constexpr double kTurnDegreesPerSecond = 40.0;
constexpr double kZoomDoublingsPerSecond = 0.75;  // zoom is multiplied by 2^(0.75 * t)
constexpr double kFastFactor = 4.0;
constexpr double kSlowFactor = 0.25;

double speedFactor(ViewSpeed speed);

// Which view commands are held right now, and what they do to a pose over a frame.
class ViewMotion {
public:
    void press(ViewAction action, ViewSpeed speed);
    // Returns true when that command was held (so the caller knows it consumed the begin).
    bool release(ViewAction action, ViewSpeed speed);
    bool isPressed(ViewAction action, ViewSpeed speed) const;
    bool any() const;
    void clear();

    // The pose after dtSeconds of every held command. Movement is along the camera's own axes,
    // so `body` should be in the frame the caller wants adjusted (the aircraft body frame).
    // Position, zoom and angles stay inside the ranges the slot file accepts.
    Pose apply(const Pose& body, double dtSeconds) const;

private:
    static int index(ViewAction action, ViewSpeed speed) {
        return static_cast<int>(action) * kViewSpeedCount + static_cast<int>(speed);
    }
    bool pressed_[kViewActionCount * kViewSpeedCount] = {};
};

}  // namespace mykam
