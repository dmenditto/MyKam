#pragma once

#include <string>
#include <vector>

#include "slot_store.h"

// The X-Plane commands MyKam registers. Every command is named mykam/<base name>. Pure code: the
// plugin registers what this list says, and a test checks the list.

namespace mykam {

inline constexpr const char* kCommandPrefix = "mykam/";

// go_slot_N / save_slot_N for a hotkey: N is the key's digit, so hotkey 10 (numpad 0) is go_slot_0.
inline std::string goSlotBaseName(int hotkey) { return "go_slot_" + hotkeyText(hotkey); }
inline std::string saveSlotBaseName(int hotkey) { return "save_slot_" + hotkeyText(hotkey); }

// The part of a command name after the prefix, for example "go_slot_3" or "release".
inline std::vector<std::string> commandBaseNames() {
    std::vector<std::string> names;
    for (int n = 1; n <= kHotkeyCount; ++n) {
        names.push_back(saveSlotBaseName(n));
        names.push_back(goSlotBaseName(n));
    }
    names.push_back("release");
    names.push_back("toggle_window");
    names.push_back("toggle_active");
    names.push_back("next");
    names.push_back("previous");
    names.push_back("toggle_group");
    return names;
}

inline std::string commandName(const std::string& baseName) {
    return std::string(kCommandPrefix) + baseName;
}

}  // namespace mykam
