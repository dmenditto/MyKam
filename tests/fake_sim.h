#pragma once

// In-memory ISim for controller tests. Records camera take/release calls, log lines, and every file
// read and write, and serves view, aircraft and file contents from plain values.

#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "sim.h"

class FakeSim : public mykam::ISim {
public:
    // Inputs the test sets.
    mykam::Pose view;
    mykam::AircraftState aircraft;
    bool heldByOther = false;
    bool takeResult = true;
    bool failWrites = false;
    bool viewExternal = true;  // existing tests describe external views; cockpit tests clear this
    bool cockpitCommandWorks = true;
    mykam::Pose head;  // the pilot head datarefs
    std::map<std::string, std::string> files;               // path -> contents
    std::map<std::string, mykam::FileStatus> forcedStatus;  // path -> status to report instead

    // Records the test inspects.
    bool owned = false;
    int takeCalls = 0;
    int releaseCalls = 0;
    int readFileCalls = 0;
    int writeFileCalls = 0;
    int readViewCalls = 0;
    int readHeadCalls = 0;
    int cockpitCommandCalls = 0;
    int cockpitFrameRequests = 0;
    bool freeCameraCommandWorks = true;
    int freeCameraCommandCalls = 0;
    int handoffFrameRequests = 0;
    bool ownedAtFreeCameraCommand = false;
    bool ownedAtViewCommand = false;
    std::set<std::string> missingCommands;       // view commands (other than free_camera) X-Plane does not know
    std::vector<std::string> viewCommandsRun;    // every runViewCommand name, in order
    std::function<void(const std::string&)> onCommand;  // set by a test to play X-Plane's handler call
    bool mouseCapture = false;  // the mouse layer is shown
    int mouseCaptureCalls = 0;
    std::vector<mykam::Pose> headWrites;
    std::vector<std::string> writtenPaths;
    std::vector<std::string> logs;

    mykam::Pose readView() override {
        ++readViewCalls;
        return view;
    }
    mykam::AircraftState readAircraft() override { return aircraft; }
    bool isCameraHeldByOther() override { return heldByOther && !owned; }
    bool takeCamera() override {
        ++takeCalls;
        if (takeResult) {
            owned = true;
        }
        return takeResult;
    }
    void releaseCamera() override {
        ++releaseCalls;
        owned = false;
    }
    bool isViewExternal() override { return viewExternal; }
    mykam::Pose readPilotHead() override {
        ++readHeadCalls;
        return head;
    }
    void writePilotHead(const mykam::Pose& h) override {
        head = h;
        headWrites.push_back(h);
    }
    bool switchToCockpitView() override {
        ++cockpitCommandCalls;
        if (cockpitCommandWorks) {
            viewExternal = false;
            if (onCommand) {
                onCommand("sim/view/3d_cockpit_cmnd_look");
            }
        }
        return cockpitCommandWorks;
    }
    void requestCockpitFrames() override { ++cockpitFrameRequests; }
    bool runViewCommand(const std::string& name) override {
        viewCommandsRun.push_back(name);
        const bool isFree = name == "sim/view/free_camera";
        if (isFree) {
            ++freeCameraCommandCalls;
        }
        const bool works = isFree ? freeCameraCommandWorks : missingCommands.count(name) == 0;
        if (works) {
            ownedAtViewCommand = owned;  // the command must run while we still hold the camera
            if (isFree) {
                ownedAtFreeCameraCommand = owned;
            }
            if (onCommand) {
                onCommand(name);  // like X-Plane calling the plugin's before-handler
            }
        }
        return works;
    }
    void requestHandoffFrame() override { ++handoffFrameRequests; }
    std::set<std::string> enabledPlugins;          // signatures the test says are loaded and enabled
    std::vector<std::string> pluginCommandsRun;    // every runPluginCommand name, in order
    bool pluginCommandsWork = true;
    std::function<void(const std::string&)> onPluginCommand;  // plays the before-handler X-Plane would call
    bool runPluginCommand(const std::string& name) override {
        pluginCommandsRun.push_back(name);
        if (pluginCommandsWork && onPluginCommand) {
            onPluginCommand(name);
        }
        return pluginCommandsWork;
    }
    bool isPluginEnabled(const std::string& signature) override { return enabledPlugins.count(signature) != 0; }
    std::map<std::string, int> pluginDatarefs;  // other plugins' datarefs that exist, with their value
    int readPluginDataref(const std::string& name, int missing) override {
        const auto it = pluginDatarefs.find(name);
        return it == pluginDatarefs.end() ? missing : it->second;
    }
    void setMouseCapture(bool on) override {
        mouseCapture = on;
        ++mouseCaptureCalls;
    }
    // Like X-Plane for letters and digits (their ASCII), "Key N" for the rest.
    std::string keyName(int virtualKey) override {
        if ((virtualKey >= '0' && virtualKey <= '9') || (virtualKey >= 'A' && virtualKey <= 'Z')) {
            return std::string(1, static_cast<char>(virtualKey));
        }
        return "Key " + std::to_string(virtualKey);
    }
    mykam::FileReadResult readFile(const std::string& path, std::size_t maxBytes) override {
        ++readFileCalls;
        mykam::FileReadResult r;
        const auto forced = forcedStatus.find(path);
        if (forced != forcedStatus.end()) {
            r.status = forced->second;
            return r;
        }
        const auto it = files.find(path);
        if (it == files.end()) {
            r.status = mykam::FileStatus::Missing;
        } else if (it->second.size() > maxBytes) {
            r.status = mykam::FileStatus::TooLarge;
        } else {
            r.status = mykam::FileStatus::Ok;
            r.text = it->second;
        }
        return r;
    }
    bool writeFile(const std::string& path, const std::string& text, std::string& error) override {
        ++writeFileCalls;
        if (failWrites) {
            error = "disk full";
            return false;
        }
        files[path] = text;
        writtenPaths.push_back(path);
        return true;
    }
    void log(const std::string& line) override { logs.push_back(line); }

    // Number of log lines containing `text`.
    int logCount(const std::string& text) const {
        int n = 0;
        for (const std::string& l : logs) {
            if (l.find(text) != std::string::npos) {
                ++n;
            }
        }
        return n;
    }
    int fileOps() const { return readFileCalls + writeFileCalls; }
};
