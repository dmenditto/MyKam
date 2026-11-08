#include "doctest.h"

#include <set>
#include <string>
#include <vector>

#include "command_names.h"

TEST_CASE("commands: the list holds the 23 MyKam commands, each once") {
    const std::vector<std::string> bases = mykam::commandBaseNames();
    CHECK(bases.size() == 23);  // 9 save + 9 go + release, toggle_window, next, previous, toggle_group
    const std::set<std::string> unique(bases.begin(), bases.end());
    CHECK(unique.size() == bases.size());
    for (const char* expected : {"save_slot_1", "save_slot_9", "go_slot_1", "go_slot_9", "release", "toggle_window", "next",
                                 "previous", "toggle_group"}) {
        INFO(expected);
        CHECK(unique.count(expected) == 1);
    }
}

TEST_CASE("commands: all 23 commands use the mykam/ prefix and no other prefix exists") {
    std::set<std::string> all;
    for (const std::string& base : mykam::commandBaseNames()) {
        INFO(base);
        const std::string name = mykam::commandName(base);
        CHECK(name.rfind("mykam/", 0) == 0);
        CHECK(name.substr(6) == base);  // the part after the prefix is the base name
        CHECK(base.find('/') == std::string::npos);  // no second prefix hidden in a base name
        all.insert(name);
    }
    CHECK(all.size() == 23);
    CHECK(std::string(mykam::kCommandPrefix) == "mykam/");
}
