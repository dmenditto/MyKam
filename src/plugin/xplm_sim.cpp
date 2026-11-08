#include "xplm_sim.h"

#include <algorithm>

#include "core/file_io.h"

namespace mykam {

bool XplmSim::init() {
    localX_ = XPLMFindDataRef("sim/flightmodel/position/local_x");
    localY_ = XPLMFindDataRef("sim/flightmodel/position/local_y");
    localZ_ = XPLMFindDataRef("sim/flightmodel/position/local_z");
    theta_ = XPLMFindDataRef("sim/flightmodel/position/theta");  // pitch, degrees
    phi_ = XPLMFindDataRef("sim/flightmodel/position/phi");      // roll, degrees
    psi_ = XPLMFindDataRef("sim/flightmodel/position/psi");      // true heading, degrees
    viewIsExternal_ = XPLMFindDataRef("sim/graphics/view/view_is_external");
    headX_ = XPLMFindDataRef("sim/graphics/view/pilots_head_x");
    headY_ = XPLMFindDataRef("sim/graphics/view/pilots_head_y");
    headZ_ = XPLMFindDataRef("sim/graphics/view/pilots_head_z");
    headPsi_ = XPLMFindDataRef("sim/graphics/view/pilots_head_psi");
    headThe_ = XPLMFindDataRef("sim/graphics/view/pilots_head_the");
    headPhi_ = XPLMFindDataRef("sim/graphics/view/pilots_head_phi");
    cockpitLook_ = XPLMFindCommand("sim/view/3d_cockpit_cmnd_look");  // may be null: reported when used
    return localX_ && localY_ && localZ_ && theta_ && phi_ && psi_ && viewIsExternal_ && headX_ && headY_ &&
           headZ_ && headPsi_ && headThe_ && headPhi_;
}

bool XplmSim::isViewExternal() {
    return XPLMGetDatai(viewIsExternal_) != 0;
}

Pose XplmSim::readPilotHead() {
    Pose p;
    p.x = static_cast<double>(XPLMGetDataf(headX_));
    p.y = static_cast<double>(XPLMGetDataf(headY_));
    p.z = static_cast<double>(XPLMGetDataf(headZ_));
    p.heading = static_cast<double>(XPLMGetDataf(headPsi_));
    p.pitch = static_cast<double>(XPLMGetDataf(headThe_));
    p.roll = static_cast<double>(XPLMGetDataf(headPhi_));
    p.zoom = 1.0;
    return p;
}

void XplmSim::writePilotHead(const Pose& head) {
    XPLMSetDataf(headX_, static_cast<float>(head.x));
    XPLMSetDataf(headY_, static_cast<float>(head.y));
    XPLMSetDataf(headZ_, static_cast<float>(head.z));
    XPLMSetDataf(headPsi_, static_cast<float>(head.heading));
    XPLMSetDataf(headThe_, static_cast<float>(head.pitch));
    XPLMSetDataf(headPhi_, static_cast<float>(head.roll));
}

bool XplmSim::switchToCockpitView() {
    if (cockpitLook_ == nullptr) {
        return false;
    }
    XPLMCommandOnce(cockpitLook_);
    return true;
}

void XplmSim::requestCockpitFrames() {
    if (headLoop_ == nullptr) {
        return;
    }
    haveLastFrame_ = false;  // the first frame of the animation counts as 0 seconds
    XPLMSetFlightLoopCallbackInterval(headLoop_, -1.0f, 1, nullptr);  // every frame until it returns 0
}

bool XplmSim::runViewCommand(const std::string& name) {
    XPLMCommandRef cmd = XPLMFindCommand(name.c_str());
    if (cmd == nullptr) {
        return false;
    }
    XPLMCommandOnce(cmd);
    return true;
}

void XplmSim::requestHandoffFrame() {
    if (handoffLoop_ == nullptr) {
        return;
    }
    // Called from the camera callback, not from the flight loop itself, so this is allowed.
    XPLMSetFlightLoopCallbackInterval(handoffLoop_, -1.0f, 1, nullptr);  // next frame, then the loop returns 0
}

Pose XplmSim::readView() {
    XPLMCameraPosition_t cam{};
    XPLMReadCameraPosition(&cam);
    Pose p;
    p.x = static_cast<double>(cam.x);
    p.y = static_cast<double>(cam.y);
    p.z = static_cast<double>(cam.z);
    p.pitch = static_cast<double>(cam.pitch);
    p.heading = static_cast<double>(cam.heading);
    p.roll = static_cast<double>(cam.roll);
    p.zoom = static_cast<double>(cam.zoom);
    return p;
}

AircraftState XplmSim::readAircraft() {
    AircraftState a;
    a.x = XPLMGetDatad(localX_);
    a.y = XPLMGetDatad(localY_);
    a.z = XPLMGetDatad(localZ_);
    a.pitch = static_cast<double>(XPLMGetDataf(theta_));
    a.roll = static_cast<double>(XPLMGetDataf(phi_));
    a.heading = static_cast<double>(XPLMGetDataf(psi_));
    return a;
}

bool XplmSim::isCameraHeldByOther() {
    return !owned_ && XPLMIsCameraBeingControlled(nullptr) != 0;
}

bool XplmSim::takeCamera() {
    if (cameraCallback_ == nullptr) {
        return false;
    }
    haveLastFrame_ = false;
    XPLMControlCamera(xplm_ControlCameraUntilViewChanges, cameraCallback_, cameraRefcon_);
    owned_ = true;
    return true;
}

void XplmSim::releaseCamera() {
    if (owned_) {
        XPLMDontControlCamera();
        owned_ = false;
    }
}

FileReadResult XplmSim::readFile(const std::string& path, std::size_t maxBytes) {
    return readTextFile(path, maxBytes);
}

bool XplmSim::writeFile(const std::string& path, const std::string& text, std::string& error) {
    return writeTextFileAtomic(path, text, error);
}

std::string XplmSim::keyName(int virtualKey) {
    if (virtualKey < 1 || virtualKey > 255) {
        return std::string();
    }
    const char* text = XPLMGetVirtualKeyDescription(static_cast<char>(virtualKey));
    return text == nullptr ? std::string() : std::string(text);
}

void XplmSim::log(const std::string& line) {
    XPLMDebugString(line.c_str());
}

double XplmSim::frameDeltaSeconds() {
    const std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
    double dt = 0.0;
    if (haveLastFrame_) {
        dt = std::chrono::duration<double>(now - lastFrame_).count();
    }
    lastFrame_ = now;
    haveLastFrame_ = true;
    return std::min(std::max(dt, 0.0), 0.25);
}

}  // namespace mykam
