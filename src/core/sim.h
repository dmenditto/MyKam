#pragma once

#include <cstddef>
#include <string>

#include "file_io.h"
#include "pose.h"

// Everything the controller needs from the outside world. The X-Plane adapter implements it with
// XPLM calls; the unit tests implement it with an in-memory fake (NFR4.1).

namespace mykam {

class ISim {
public:
    virtual ~ISim() = default;

    // The camera X-Plane currently shows, in world coordinates, with zoom (1.0 = normal).
    virtual Pose readView() = 0;
    // The user's aircraft position and attitude in world coordinates.
    virtual AircraftState readAircraft() = 0;

    // True when some other plugin controls the camera right now (never true while we hold it).
    virtual bool isCameraHeldByOther() = 0;
    // Ask X-Plane to call us every frame to place the camera. False if that could not be done.
    virtual bool takeCamera() = 0;
    // Give the camera back to X-Plane.
    virtual void releaseCamera() = 0;

    // True when X-Plane shows an external view (sim/graphics/view/view_is_external != 0).
    virtual bool isViewExternal() = 0;
    // X-Plane's pilot head in the cockpit view, as a Pose: x, y, z in metres relative to the
    // aircraft CG in body axes, heading/pitch/roll = pilots_head_psi/the/phi in degrees relative to
    // the aircraft. Zoom is always 1.
    virtual Pose readPilotHead() = 0;
    // Write the pilot head datarefs (zoom is ignored).
    virtual void writePilotHead(const Pose& head) = 0;
    // Run X-Plane's own "3-D cockpit look" command. False if the command could not be found.
    virtual bool switchToCockpitView() = 0;
    // Ask for Controller::tickCockpit to be called every frame until it returns false.
    virtual void requestCockpitFrames() = 0;

    // Run the named X-Plane command once (XPLMFindCommand, then XPLMCommandOnce). False if no command
    // with that name exists. The controller only passes names from the tracked sim/view/ list.
    virtual bool runViewCommand(const std::string& name) = 0;
    // Run another plugin's command once, the same way. Only names from companions.h are passed.
    virtual bool runPluginCommand(const std::string& name) = 0;
    // True when a plugin with this signature is loaded and enabled (XPLMFindPluginBySignature,
    // XPLMIsPluginEnabled).
    virtual bool isPluginEnabled(const std::string& signature) = 0;
    // The value of another plugin's int or float dataref, rounded; `missing` when it does not exist.
    virtual int readPluginDataref(const std::string& name, int missing) = 0;
    // Ask for Controller::performHandoff to be called once on the next frame, outside the camera
    // callback (a flight loop).
    virtual void requestHandoffFrame() = 0;

    // Show (true) or hide (false) the full-screen mouse layer that lets the right mouse button and
    // the wheel move the held outside camera. On only while an external camera is held.
    virtual void setMouseCapture(bool on) = 0;

    // The name X-Plane gives a virtual key code (XPLMGetVirtualKeyDescription), "" when it has none.
    virtual std::string keyName(int virtualKey) = 0;

    virtual FileReadResult readFile(const std::string& path, std::size_t maxBytes) = 0;
    virtual bool writeFile(const std::string& path, const std::string& text, std::string& error) = 0;

    // One complete log line, already prefixed with "[MyKam] " and ending in a newline.
    virtual void log(const std::string& line) = 0;
};

}  // namespace mykam
