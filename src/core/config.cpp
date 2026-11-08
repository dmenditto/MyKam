#include "config.h"

#include <algorithm>
#include <cctype>
#include <cmath>

#include "text_util.h"
#include "view_commands.h"

namespace mykam {

ConfigResult defaultConfig(const std::string& reason) {
    ConfigResult r;
    r.warning = "mykam.ini: " + reason + ", using transition_seconds=1.5";
    return r;
}

namespace {

ConfigResult parseTransition(const std::string& text);

void addWarning(ConfigResult& r, const std::string& w) {
    r.warning = r.warning.empty() ? w : r.warning + "; " + w;
}

std::string shortened(const std::string& s) {
    return s.size() > 24 ? s.substr(0, 24) + "..." : s;
}

std::string toLowerCopy(std::string s) {
    for (char& c : s) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return s;
}

// The value of the last `key = value` line (the last occurrence wins), if any.
bool lastValue(const std::string& text, const std::string& key, std::string& value) {
    bool found = false;
    for (const std::string& rawLine : splitLines(text)) {
        const std::string line = trim(rawLine);
        if (line.empty() || line[0] == '#' || line[0] == ';') {
            continue;
        }
        const std::size_t eq = line.find('=');
        if (eq == std::string::npos) {
            continue;
        }
        if (trim(line.substr(0, eq)) == key) {
            value = trim(line.substr(eq + 1));
            found = true;
        }
    }
    return found;
}

bool validHint(const std::string& hint) {
    if (hint.size() > kMaxKeyHintLength) {
        return false;
    }
    for (char ch : hint) {
        const unsigned char u = static_cast<unsigned char>(ch);
        if (u < 0x20 || u > 0x7E) {
            return false;
        }
    }
    return true;
}

// Quick bar and key hint settings. Missing keys keep the defaults quietly; invalid ones warn.
void parseInterface(const std::string& text, ConfigResult& r) {
    std::string v;
    if (lastValue(text, "show_quick_bar", v)) {
        if (v == "1") {
            r.config.showQuickBar = true;
        } else if (v == "0") {
            r.config.showQuickBar = false;
        } else {
            addWarning(r, "mykam.ini: show_quick_bar '" + shortened(v) + "' is not 0 or 1, using show_quick_bar=1");
        }
    }
    if (lastValue(text, "quick_bar_position", v)) {
        const std::string l = toLowerCopy(v);
        if (l == "top") {
            r.config.quickBarPosition = QuickBarPosition::Top;
        } else if (l != "bottom") {
            addWarning(r, "mykam.ini: quick_bar_position '" + shortened(v) +
                              "' is not bottom or top, using quick_bar_position=bottom");
        }
    }
    if (lastValue(text, "quick_bar_views", v)) {
        r.config.quickBarViews.clear();
        std::string unknown;
        std::size_t start = 0;
        while (start <= v.size()) {
            std::size_t comma = v.find(',', start);
            if (comma == std::string::npos) {
                comma = v.size();
            }
            const std::string name = toLowerCopy(trim(v.substr(start, comma - start)));
            start = comma + 1;
            if (name.empty()) {
                continue;
            }
            if (quickViewIndex(name) < 0) {
                unknown += (unknown.empty() ? "" : ", ") + shortened(name);
                continue;
            }
            if (std::find(r.config.quickBarViews.begin(), r.config.quickBarViews.end(), name) ==
                r.config.quickBarViews.end()) {
                r.config.quickBarViews.push_back(name);
            }
        }
        if (!unknown.empty()) {
            addWarning(r, "mykam.ini: quick_bar_views ignores unknown name(s): " + unknown);
        }
    }
    if (lastValue(text, "key_hint_go", v)) {
        if (validHint(v)) {
            r.config.keyHintGo = v;
        } else {
            addWarning(r, "mykam.ini: key_hint_go is not 32 printable ASCII characters or fewer, using the default");
        }
    }
    if (lastValue(text, "key_hint_save", v)) {
        if (validHint(v)) {
            r.config.keyHintSave = v;
        } else {
            addWarning(r, "mykam.ini: key_hint_save is not 32 printable ASCII characters or fewer, using the default");
        }
    }
}

const char* handoffName(HandoffView v) {
    switch (v) {
        case HandoffView::Free: return "free";
        case HandoffView::Off: return "off";
        default: return "saved";
    }
}

}  // namespace

ConfigResult parseConfig(const std::string& text) {
    bool viewFound = false;
    std::string viewRaw;
    bool handoffFound = false;
    std::string handoffRaw;
    for (const std::string& rawLine : splitLines(text)) {
        const std::string line = trim(rawLine);
        if (line.empty() || line[0] == '#' || line[0] == ';') {
            continue;
        }
        const std::size_t eq = line.find('=');
        if (eq == std::string::npos) {
            continue;
        }
        const std::string key = trim(line.substr(0, eq));
        if (key == "handoff_view") {
            viewFound = true;
            viewRaw = trim(line.substr(eq + 1));  // the last occurrence wins
        } else if (key == "free_camera_handoff") {
            handoffFound = true;
            handoffRaw = trim(line.substr(eq + 1));
        }
    }
    HandoffView handoff = HandoffView::Saved;
    std::string handoffWarning;
    if (viewFound) {
        // handoff_view wins over the old key whenever it is present.
        std::string lowered = viewRaw;
        for (char& c : lowered) {
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        if (lowered == "free") {
            handoff = HandoffView::Free;
        } else if (lowered == "off") {
            handoff = HandoffView::Off;
        } else if (lowered != "saved") {
            const std::string shown = viewRaw.size() > 24 ? viewRaw.substr(0, 24) + "..." : viewRaw;
            handoffWarning = "mykam.ini: handoff_view '" + shown + "' is not saved, free or off, using handoff_view=saved";
        }
    } else if (handoffFound) {
        if (handoffRaw == "0") {
            handoff = HandoffView::Off;
        } else if (handoffRaw != "1") {
            const std::string shown = handoffRaw.size() > 24 ? handoffRaw.substr(0, 24) + "..." : handoffRaw;
            handoffWarning = "mykam.ini: free_camera_handoff '" + shown + "' is not 0 or 1, using handoff_view=saved";
        }
    }
    ConfigResult r = parseTransition(text);
    r.config.handoffView = handoff;
    if (!handoffWarning.empty()) {
        r.warning = r.warning.empty() ? handoffWarning : r.warning + "; " + handoffWarning;
    }
    parseInterface(text, r);
    return r;
}

namespace {

ConfigResult parseTransition(const std::string& text) {
    bool keyFound = false;
    std::string rawValue;
    for (const std::string& rawLine : splitLines(text)) {
        const std::string line = trim(rawLine);
        if (line.empty() || line[0] == '#' || line[0] == ';') {
            continue;
        }
        const std::size_t eq = line.find('=');
        if (eq == std::string::npos) {
            continue;
        }
        if (trim(line.substr(0, eq)) == "transition_seconds") {
            keyFound = true;
            rawValue = trim(line.substr(eq + 1));  // the last occurrence wins
        }
    }
    if (!keyFound) {
        return defaultConfig("transition_seconds is missing");
    }
    double value = 0.0;
    if (!parseDouble(rawValue, value)) {
        std::string shown = rawValue.size() > 24 ? rawValue.substr(0, 24) + "..." : rawValue;
        return defaultConfig("transition_seconds '" + shown + "' is not a number");
    }
    if (value < kMinTransitionSeconds || value > kMaxTransitionSeconds) {
        return defaultConfig("transition_seconds " + formatDouble(value) + " is outside 0 to 10");
    }
    ConfigResult r;
    r.config.transitionSeconds = value;
    return r;
}

}  // namespace

bool validateConfig(const Config& config, std::string& problem) {
    if (!std::isfinite(config.transitionSeconds) || config.transitionSeconds < kMinTransitionSeconds ||
        config.transitionSeconds > kMaxTransitionSeconds) {
        problem = "transition time must be between 0 and 10 seconds";
        return false;
    }
    if (config.handoffView != HandoffView::Saved && config.handoffView != HandoffView::Free &&
        config.handoffView != HandoffView::Off) {
        problem = "after-the-glide choice must be saved, free or off";
        return false;
    }
    if (config.quickBarPosition != QuickBarPosition::Bottom && config.quickBarPosition != QuickBarPosition::Top) {
        problem = "quick bar position must be bottom or top";
        return false;
    }
    if (!validHint(config.keyHintGo) || !validHint(config.keyHintSave)) {
        problem = "key hints must be 32 printable ASCII characters or fewer";
        return false;
    }
    for (std::size_t i = 0; i < config.quickBarViews.size(); ++i) {
        const std::string& name = config.quickBarViews[i];
        if (quickViewIndex(name) < 0) {
            problem = "quick bar view '" + shortened(name) + "' is not a known view name";
            return false;
        }
        if (std::find(config.quickBarViews.begin(), config.quickBarViews.begin() + static_cast<std::ptrdiff_t>(i),
                      name) != config.quickBarViews.begin() + static_cast<std::ptrdiff_t>(i)) {
            problem = "quick bar view '" + name + "' is listed twice";
            return false;
        }
    }
    return true;
}

std::string quickBarViewsText(const std::vector<std::string>& names) {
    std::string out;
    for (const std::string& name : names) {
        out += (out.empty() ? "" : ",") + name;
    }
    return out;
}

std::string updateConfigText(const std::string& oldText, const Config& config) {
    struct Setting {
        const char* key;
        std::string value;
        const char* comment;
        bool written;
    };
    Setting settings[5] = {
        {"transition_seconds", formatCompact(config.transitionSeconds, 2),
         "# transition_seconds: how long a camera switch takes, 0 (instant cut) to 10 seconds.", false},
        {"handoff_view", handoffName(config.handoffView),
         "# handoff_view: after the glide, saved (the slot's X-Plane view), free (free camera) or off (hold).", false},
        {"show_quick_bar", config.showQuickBar ? "1" : "0", "# show_quick_bar: 1 shows the quick bar, 0 hides it.", false},
        {"quick_bar_position", config.quickBarPosition == QuickBarPosition::Top ? "top" : "bottom",
         "# quick_bar_position: bottom or top.", false},
        {"quick_bar_views", quickBarViewsText(config.quickBarViews),
         "# quick_bar_views: the X-Plane view buttons of the quick bar, in order (cockpit, free, chase, circle, "
         "tower, runway, linear_spot, still_spot, ridealong, wingman). Empty means none.",
         false},
    };

    std::string out;
    if (trim(oldText).empty()) {
        out += "# MyKam settings, written by MyKam. Other lines you add here are kept.\n";
    }
    for (const std::string& rawLine : splitLines(oldText)) {
        const std::string line = trim(rawLine);
        bool replaced = false;
        if (!line.empty() && line[0] != '#' && line[0] != ';') {
            const std::size_t eq = line.find('=');
            if (eq != std::string::npos) {
                const std::string key = trim(line.substr(0, eq));
                for (Setting& s : settings) {
                    if (key == s.key) {
                        out += s.value.empty() ? key + " =\n" : key + " = " + s.value + "\n";
                        s.written = true;
                        replaced = true;
                        break;
                    }
                }
            }
        }
        if (!replaced) {
            out += rawLine + "\n";
        }
    }
    for (const Setting& s : settings) {
        if (!s.written) {
            out += std::string(s.comment) + "\n" + s.key + (s.value.empty() ? " =\n" : " = " + s.value + "\n");
        }
    }
    return out;
}

}  // namespace mykam
