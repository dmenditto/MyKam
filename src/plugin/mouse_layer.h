#pragma once

#include "xplm_includes.h"

// A full-screen, transparent, undecorated window that exists only while mykam holds an outside
// camera. It passes left clicks through and takes the right mouse button and the wheel, which it
// forwards to the Controller. Thin on purpose: no decisions are made here.

namespace mykam {

class Controller;

class MouseLayer {
public:
    MouseLayer() = default;
    MouseLayer(const MouseLayer&) = delete;
    MouseLayer& operator=(const MouseLayer&) = delete;
    ~MouseLayer() { destroy(); }

    // Where mouse events go. Set before the first setActive(true).
    void setController(Controller* controller) { controller_ = controller; }

    // Create the window (true) or destroy it (false). Does nothing when already in that state.
    // If the window cannot be created, one line is logged the first time and the layer stays off.
    void setActive(bool on);
    void destroy() { setActive(false); }

private:
    static void draw(XPLMWindowID window, void* refcon);
    static int leftClick(XPLMWindowID window, int x, int y, XPLMMouseStatus status, void* refcon);
    static int rightClick(XPLMWindowID window, int x, int y, XPLMMouseStatus status, void* refcon);
    static void key(XPLMWindowID window, char key, XPLMKeyFlags flags, char virtualKey, void* refcon, int losingFocus);
    static XPLMCursorStatus cursor(XPLMWindowID window, int x, int y, void* refcon);
    static int wheel(XPLMWindowID window, int x, int y, int wheel, int clicks, void* refcon);

    Controller* controller_ = nullptr;
    XPLMWindowID window_ = nullptr;
    bool failureLogged_ = false;
};

}  // namespace mykam
