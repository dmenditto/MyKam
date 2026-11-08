#include "view_control.h"

#include <algorithm>
#include <cmath>

#include "slot_store.h"

namespace mykam {

namespace {

double clampTo(double v, double limit) {
    return std::min(std::max(v, -limit), limit);
}

}  // namespace

double speedFactor(ViewSpeed speed) {
    switch (speed) {
        case ViewSpeed::Fast: return kFastFactor;
        case ViewSpeed::Slow: return kSlowFactor;
        default: return 1.0;
    }
}

void ViewMotion::press(ViewAction action, ViewSpeed speed) {
    pressed_[index(action, speed)] = true;
}

bool ViewMotion::release(ViewAction action, ViewSpeed speed) {
    const bool was = pressed_[index(action, speed)];
    pressed_[index(action, speed)] = false;
    return was;
}

bool ViewMotion::isPressed(ViewAction action, ViewSpeed speed) const {
    return pressed_[index(action, speed)];
}

bool ViewMotion::any() const {
    for (bool b : pressed_) {
        if (b) {
            return true;
        }
    }
    return false;
}

void ViewMotion::clear() {
    for (bool& b : pressed_) {
        b = false;
    }
}

Pose ViewMotion::apply(const Pose& body, double dtSeconds) const {
    if (!(dtSeconds > 0.0) || !any()) {
        return body;
    }
    double right = 0.0;
    double up = 0.0;
    double forward = 0.0;
    double yaw = 0.0;
    double pitch = 0.0;
    double zoomDoublings = 0.0;
    for (int a = 0; a < kViewActionCount; ++a) {
        for (int s = 0; s < kViewSpeedCount; ++s) {
            if (!pressed_[a * kViewSpeedCount + s]) {
                continue;
            }
            const double f = speedFactor(static_cast<ViewSpeed>(s)) * dtSeconds;
            const double move = kMoveMetresPerSecond * f;
            const double turn = kTurnDegreesPerSecond * f;
            switch (static_cast<ViewAction>(a)) {
                case ViewAction::Left: right -= move; break;
                case ViewAction::Right: right += move; break;
                case ViewAction::Up: up += move; break;
                case ViewAction::Down: up -= move; break;
                case ViewAction::Forward: forward += move; break;
                case ViewAction::Backward: forward -= move; break;
                case ViewAction::RotLeft: yaw -= turn; break;
                case ViewAction::RotRight: yaw += turn; break;
                case ViewAction::RotUp: pitch += turn; break;
                case ViewAction::RotDown: pitch -= turn; break;
                case ViewAction::ZoomIn: zoomDoublings += kZoomDoublingsPerSecond * f; break;
                case ViewAction::ZoomOut: zoomDoublings -= kZoomDoublingsPerSecond * f; break;
            }
        }
    }

    Pose out = body;
    if (right != 0.0 || up != 0.0 || forward != 0.0) {
        out = moveInCameraFrame(out, right, up, forward);
        out.x = clampTo(out.x, kMaxPositionMetres);
        out.y = clampTo(out.y, kMaxPositionMetres);
        out.z = clampTo(out.z, kMaxPositionMetres);
    }
    if (yaw != 0.0 || pitch != 0.0) {
        out = rotateInCameraFrame(out, yaw, pitch);
    }
    if (zoomDoublings != 0.0) {
        out.zoom = std::min(std::max(out.zoom * std::pow(2.0, zoomDoublings), kMinZoom), kMaxZoom);
    }
    return out;
}

}  // namespace mykam
