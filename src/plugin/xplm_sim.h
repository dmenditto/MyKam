#pragma once

#include <chrono>

#include "core/sim.h"
#include "mouse_layer.h"
#include "xplm_includes.h"

// ISim backed by real X-Plane calls. Thin on purpose: no decisions are made here.

namespace mykam {

class Controller;

class XplmSim : public ISim {
public:
    // Look up the datarefs. Returns false (and the plugin refuses to enable) if any is missing.
    bool init();

    // The per-frame camera callback X-Plane is asked to call while we hold the camera.
    void setCameraCallback(XPLMCameraControl_f callback, void* refcon) {
        cameraCallback_ = callback;
        cameraRefcon_ = refcon;
    }

    // The flight loop that drives the cockpit head animation. Registered by the plugin; this class
    // only switches it to every-frame when the controller asks for frames.
    void setHeadFlightLoop(XPLMFlightLoop_f callback) { headLoop_ = callback; }
    // The flight loop that performs the hand-over to the free camera, one frame after it is requested.
    void setHandoffFlightLoop(XPLMFlightLoop_f callback) { handoffLoop_ = callback; }

    Pose readView() override;
    AircraftState readAircraft() override;
    bool isCameraHeldByOther() override;
    bool takeCamera() override;
    void releaseCamera() override;
    bool isViewExternal() override;
    Pose readPilotHead() override;
    void writePilotHead(const Pose& head) override;
    bool switchToCockpitView() override;
    void requestCockpitFrames() override;
    bool runViewCommand(const std::string& name) override;
    void requestHandoffFrame() override;
    void setMouseCapture(bool on) override { mouseLayer_.setActive(on); }
    std::string keyName(int virtualKey) override;
    FileReadResult readFile(const std::string& path, std::size_t maxBytes) override;
    bool writeFile(const std::string& path, const std::string& text, std::string& error) override;
    void log(const std::string& line) override;

    // Where the mouse layer sends mouse events, and removing the layer (plugin disable or stop).
    void setMouseTarget(Controller* controller) { mouseLayer_.setController(controller); }
    void destroyMouseLayer() { mouseLayer_.destroy(); }

    // Called from the camera callback when X-Plane or another plugin took the camera.
    void markCameraLost() { owned_ = false; }
    // Seconds since the previous call (0 on the first call after we took the camera). Clamped to 0.25.
    double frameDeltaSeconds();

private:
    XPLMCameraControl_f cameraCallback_ = nullptr;
    void* cameraRefcon_ = nullptr;
    XPLMDataRef localX_ = nullptr;
    XPLMDataRef localY_ = nullptr;
    XPLMDataRef localZ_ = nullptr;
    XPLMDataRef theta_ = nullptr;
    XPLMDataRef phi_ = nullptr;
    XPLMDataRef psi_ = nullptr;
    XPLMDataRef viewIsExternal_ = nullptr;
    XPLMDataRef headX_ = nullptr;
    XPLMDataRef headY_ = nullptr;
    XPLMDataRef headZ_ = nullptr;
    XPLMDataRef headPsi_ = nullptr;
    XPLMDataRef headThe_ = nullptr;
    XPLMDataRef headPhi_ = nullptr;
    XPLMCommandRef cockpitLook_ = nullptr;
    XPLMFlightLoop_f headLoop_ = nullptr;
    XPLMFlightLoop_f handoffLoop_ = nullptr;
    MouseLayer mouseLayer_;
    bool owned_ = false;
    bool haveLastFrame_ = false;
    std::chrono::steady_clock::time_point lastFrame_{};
};

}  // namespace mykam
