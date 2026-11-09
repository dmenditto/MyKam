#include "doctest.h"

#include <set>
#include <string>
#include <vector>

#include "command_names.h"

TEST_CASE("commands: the list holds the 26 MyKam commands, each once") {
    const std::vector<std::string> bases = mykam::commandBaseNames();
    // 10 save + 10 go (numpad 1..9 and 0) + release, toggle_window, toggle_active, next, previous, toggle_group
    CHECK(bases.size() == 26);
    const std::set<std::string> unique(bases.begin(), bases.end());
    CHECK(unique.size() == bases.size());
    for (const char* expected : {"save_slot_1", "save_slot_9", "save_slot_0", "go_slot_1", "go_slot_9", "go_slot_0",
                                 "release", "toggle_window", "toggle_active", "next", "previous", "toggle_group"}) {
        INFO(expected);
        CHECK(unique.count(expected) == 1);
    }
    CHECK(unique.count("go_slot_10") == 0);  // numpad 0 is named by its digit
}

TEST_CASE("commands: numpad 0 (hotkey 10) is go_slot_0 / save_slot_0") {
    CHECK(mykam::goSlotBaseName(mykam::kHotkeyZero) == "go_slot_0");
    CHECK(mykam::saveSlotBaseName(mykam::kHotkeyZero) == "save_slot_0");
    CHECK(mykam::goSlotBaseName(3) == "go_slot_3");
}

TEST_CASE("commands: all 26 commands use the mykam/ prefix and no other prefix exists") {
    std::set<std::string> all;
    for (const std::string& base : mykam::commandBaseNames()) {
        INFO(base);
        const std::string name = mykam::commandName(base);
        CHECK(name.rfind("mykam/", 0) == 0);
        CHECK(name.substr(6) == base);  // the part after the prefix is the base name
        CHECK(base.find('/') == std::string::npos);  // no second prefix hidden in a base name
        all.insert(name);
    }
    CHECK(all.size() == 26);
    CHECK(std::string(mykam::kCommandPrefix) == "mykam/");
}
