#pragma once

#include <cstddef>
#include <string>

// Other X-Plane plugins MyKam knows about. Pure code: names only, the plugin asks X-Plane about them.

namespace mykam {

// Camera plugins that also place the camera; running one next to MyKam can move the camera back after
// MyKam moved it (issue #1). Signatures as reported by XPLMGetPluginInfo.
struct CameraPluginInfo {
    const char* signature;
    const char* name;
    // A dataref the plugin publishes that is 0 while it is switched off from its own controls (the
    // plugin then stays enabled in Plugin Admin); nullptr when there is none.
    const char* activeDataref;
};
inline constexpr CameraPluginInfo kOtherCameraPlugins[] = {
    {"SRS.X-Camera", "X-Camera", "SRS/X-Camera/integration/X-Camera_enabled"},
};
inline constexpr std::size_t kOtherCameraPluginCount = sizeof(kOtherCameraPlugins) / sizeof(kOtherCameraPlugins[0]);

// XPRealistic Pro (issue #7). It offers toggles only and no dataref with its state, so MyKam counts the
// toggles itself, starting from "master on, not frozen" when XPRealistic is found.
inline constexpr const char* kXprSignature = "com.rkApps.xprealistic";
inline constexpr const char* kXprMasterCommand = "XPRealistic/MasterSwitch";
inline constexpr const char* kXprFreezeCommand = "XPRealistic/FreezeMode";

// What a saved view asks of XPRealistic on arrival. Keep changes nothing; Off freezes its effects, On
// lets them run (unfreezes).
enum class XprMode { Keep, On, Off };

inline const char* xprModeText(XprMode mode) {
    switch (mode) {
        case XprMode::On: return "on";
        case XprMode::Off: return "off";
        case XprMode::Keep: break;
    }
    return "keep";
}

// "on" or "off" (any case); "keep" or "" means Keep. False for anything else (mode unchanged).
inline bool parseXprMode(const std::string& text, XprMode& mode) {
    std::string t;
    for (char c : text) {
        t.push_back(static_cast<char>(c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c));
    }
    if (t == "on") {
        mode = XprMode::On;
    } else if (t == "off") {
        mode = XprMode::Off;
    } else if (t == "keep" || t.empty()) {
        mode = XprMode::Keep;
    } else {
        return false;
    }
    return true;
}

}  // namespace mykam
