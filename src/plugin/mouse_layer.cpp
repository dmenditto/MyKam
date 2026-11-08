#include "mouse_layer.h"

#include "core/controller.h"

namespace mykam {

namespace {

MouseLayer* layerOf(void* refcon) {
    return static_cast<MouseLayer*>(refcon);
}

}  // namespace

void MouseLayer::setActive(bool on) {
    if (on == (window_ != nullptr)) {
        return;
    }
    if (!on) {
        XPLMDestroyWindow(window_);
        window_ = nullptr;
        return;
    }
    int left = 0;
    int top = 0;
    int right = 0;
    int bottom = 0;
    XPLMGetScreenBoundsGlobal(&left, &top, &right, &bottom);

    XPLMCreateWindow_t params{};
    params.structSize = sizeof(params);
    params.left = left;
    params.top = top;
    params.right = right;
    params.bottom = bottom;
    params.visible = 1;
    params.drawWindowFunc = &MouseLayer::draw;
    params.handleMouseClickFunc = &MouseLayer::leftClick;
    params.handleKeyFunc = &MouseLayer::key;
    params.handleCursorFunc = &MouseLayer::cursor;
    params.handleMouseWheelFunc = &MouseLayer::wheel;
    params.refcon = this;
    params.decorateAsFloatingWindow = xplm_WindowDecorationNone;
    params.layer = xplm_WindowLayerFlightOverlay;
    params.handleRightClickFunc = &MouseLayer::rightClick;
    window_ = XPLMCreateWindowEx(&params);
    if (window_ == nullptr && !failureLogged_) {
        failureLogged_ = true;
        XPLMDebugString("[MyKam] warning: could not create the mouse layer, the mouse will not move the outside camera\n");
    }
}

void MouseLayer::draw(XPLMWindowID, void*) {
    // Transparent: draws nothing.
}

int MouseLayer::leftClick(XPLMWindowID, int, int, XPLMMouseStatus, void*) {
    return 0;  // pass through so X-Plane menus, windows and panels keep working
}

int MouseLayer::rightClick(XPLMWindowID, int x, int y, XPLMMouseStatus status, void* refcon) {
    MouseLayer* self = layerOf(refcon);
    if (self == nullptr || self->controller_ == nullptr) {
        return 1;
    }
    try {
        switch (status) {
            case xplm_MouseDown:
                self->controller_->onMouseDown(x, y);
                break;
            case xplm_MouseDrag:
                self->controller_->onMouseDrag(x, y);
                break;
            case xplm_MouseUp:
                self->controller_->onMouseUp();
                break;
            default:
                break;
        }
    } catch (...) {
    }
    return 1;
}

void MouseLayer::key(XPLMWindowID, char, XPLMKeyFlags, char, void*, int) {
    // No keyboard handling: keys go to X-Plane as usual.
}

XPLMCursorStatus MouseLayer::cursor(XPLMWindowID, int, int, void*) {
    return xplm_CursorDefault;
}

int MouseLayer::wheel(XPLMWindowID, int, int, int wheelAxis, int clicks, void* refcon) {
    if (wheelAxis != 0) {
        return 0;  // only the vertical wheel is used
    }
    MouseLayer* self = layerOf(refcon);
    if (self == nullptr || self->controller_ == nullptr) {
        return 0;
    }
    try {
        return self->controller_->onMouseWheel(clicks) ? 1 : 0;
    } catch (...) {
        return 0;
    }
}

}  // namespace mykam
