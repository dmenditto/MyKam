#pragma once

#include <string>
#include <vector>

#include "slot_store.h"

// The X-Plane commands MyKam registers. Every command is named mykam/<base name>. Pure code: the
// plugin registers what this list says, and a test checks the list.

namespace mykam {

inline constexpr const char* kCommandPrefix = "mykam/";

// The part of a command name after the prefix, for example "go_slot_3" or "release".
inline std::vector<std::string> commandBaseNames() {
    std::vector<std::string> names;
    for (int n = 1; n <= kHotkeyCount; ++n) {
        names.push_back("save_slot_" + std::to_string(n));
        names.push_back("go_slot_" + std::to_string(n));
    }
    names.push_back("release");
    names.push_back("toggle_window");
    names.push_back("next");
    names.push_back("previous");
    names.push_back("toggle_group");
    return names;
}

inline std::string commandName(const std::string& baseName) {
    return std::string(kCommandPrefix) + baseName;
}

}  // namespace mykam
