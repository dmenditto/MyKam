#pragma once

#include <cstddef>
#include <string>

// The X-Plane view-selecting commands mykam watches (it only listens, X-Plane still acts), and the
// rules for a command name stored in a slot file. Pure code, no X-Plane calls.

namespace mykam {

struct ViewCommandInfo {
    const char* name;
    // True for a view type that mykam may hand a saved outside camera back to. Cockpit views and the
    // two camera-behaviour switches are tracked but never used as a hand-off target.
    bool outside;
    // True for an outside view type X-Plane positions itself (it does not start from the current
    // camera, so a hand-off back to it jumps). Saving a camera in one of these is refused; these views
    // were tested in X-Plane 12 and saving there is not offered.
    bool noSave;
};

inline constexpr const char* kFreeCameraCommand = "sim/view/free_camera";

inline constexpr ViewCommandInfo kViewCommands[] = {
    {"sim/view/free_camera", true, false},
    {"sim/view/default_view", false, false},
    {"sim/view/forward_with_2d_panel", false, false},
    {"sim/view/forward_with_hud", false, false},
    {"sim/view/forward_with_nothing", false, false},
    {"sim/view/linear_spot", false, true},
    {"sim/view/still_spot", false, true},
    {"sim/view/runway", false, true},
    {"sim/view/circle", false, true},
    {"sim/view/tower", false, true},
    {"sim/view/ridealong", false, true},
    {"sim/view/track_weapon", false, true},
    {"sim/view/chase", true, false},
    {"sim/view/wingman", false, true},
    {"sim/view/3d_cockpit_cmnd_look", false, false},
    {"sim/view/g_loaded_camera", false, false},
    {"sim/view/cinema_verite", false, false},
};
inline constexpr std::size_t kViewCommandCount = sizeof(kViewCommands) / sizeof(kViewCommands[0]);

constexpr std::size_t kMaxViewCommandNameLength = 64;

// The entry for a tracked command, or nullptr.
inline const ViewCommandInfo* findViewCommand(const std::string& name) {
    for (const ViewCommandInfo& info : kViewCommands) {
        if (name == info.name) {
            return &info;
        }
    }
    return nullptr;
}

// The short names the `quick_bar_views` setting in mykam.ini uses, each with its sim/view command. The
// order is the order of the "X-Plane views" page (kXPlaneViewButtons in ui_model.h); a test keeps the two
// lists in step.
struct QuickViewName {
    const char* name;
    const char* command;
};

inline constexpr QuickViewName kQuickViewNames[] = {
    {"cockpit", "sim/view/3d_cockpit_cmnd_look"},
    {"free", "sim/view/free_camera"},
    {"chase", "sim/view/chase"},
    {"circle", "sim/view/circle"},
    {"tower", "sim/view/tower"},
    {"runway", "sim/view/runway"},
    {"linear_spot", "sim/view/linear_spot"},
    {"still_spot", "sim/view/still_spot"},
    {"ridealong", "sim/view/ridealong"},
    {"wingman", "sim/view/wingman"},
};
inline constexpr std::size_t kQuickViewNameCount = sizeof(kQuickViewNames) / sizeof(kQuickViewNames[0]);

// The index of a short name (lowercase) in kQuickViewNames, or -1.
inline int quickViewIndex(const std::string& name) {
    for (std::size_t i = 0; i < kQuickViewNameCount; ++i) {
        if (name == kQuickViewNames[i].name) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

inline bool isTrackedViewCommand(const std::string& name) {
    return findViewCommand(name) != nullptr;
}

// True when saving a camera is refused in this view type.
inline bool isNoSaveViewCommand(const std::string& name) {
    const ViewCommandInfo* info = findViewCommand(name);
    return info != nullptr && info->noSave;
}

// True when the command is a view type a saved outside camera can be handed back to.
inline bool isHandoffViewCommand(const std::string& name) {
    const ViewCommandInfo* info = findViewCommand(name);
    return info != nullptr && info->outside;
}

// Shape check for a name read from a slot file: starts with "sim/view/", then only letters, digits,
// underscore and slash, at most 64 characters in all.
inline bool isValidViewCommandName(const std::string& name) {
    static const std::string prefix = "sim/view/";
    if (name.size() <= prefix.size() || name.size() > kMaxViewCommandNameLength ||
        name.compare(0, prefix.size(), prefix) != 0) {
        return false;
    }
    for (char ch : name) {
        const bool ok = (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') ||
                        ch == '_' || ch == '/';
        if (!ok) {
            return false;
        }
    }
    return true;
}

}  // namespace mykam
