#pragma once

#include <cstddef>
#include <string>
#include <vector>

// Reads and updates `mykam.ini` (FR7). Pure code: the caller supplies and stores the text.

namespace mykam {

constexpr double kDefaultTransitionSeconds = 1.5;
constexpr double kMinTransitionSeconds = 0.0;
constexpr double kMaxTransitionSeconds = 10.0;
constexpr std::size_t kMaxKeyHintLength = 32;

// What happens once an outside camera has arrived:
//   Saved = hand over to the X-Plane view type stored in the slot (free camera if none), the default;
//   Free  = always hand over to the X-Plane free camera;
//   Off   = keep holding the camera with mykam's mouse and key control.
enum class HandoffView { Saved, Free, Off };

// Where the quick bar sits on the main screen.
enum class QuickBarPosition { Bottom, Top };

struct Config {
    double transitionSeconds = kDefaultTransitionSeconds;
    HandoffView handoffView = HandoffView::Saved;
    bool showQuickBar = true;
    QuickBarPosition quickBarPosition = QuickBarPosition::Bottom;
    // The X-Plane view buttons of the quick bar, in bar order: short names from kQuickViewNames
    // (cockpit, free, chase, circle, tower, runway, linear_spot, still_spot, ridealong, wingman), each
    // at most once. Empty means no X-Plane view buttons. Release and the menu button always stay.
    std::vector<std::string> quickBarViews = {"cockpit", "free", "chase"};
    // Text shown on the slot cards; "{n}" becomes the slot number. Display only: the real keys are
    // bound in X-Plane. An empty text hides the hint.
    std::string keyHintGo = "Numpad {n}";
    std::string keyHintSave = "Ctrl+Alt+Numpad {n}";
};

struct ConfigResult {
    Config config;
    std::string warning;  // empty when the file was fine; otherwise one line to log
};

// Parse file text. Lines look like `transition_seconds = 1.5`; `#` and `;` start comment lines.
// A missing key, a non-number or a value outside 0 to 10 gives the default and a warning.
// `handoff_view = saved`, `free` or `off`: a missing key means saved with no warning; anything else
// gives saved and a warning. The older `free_camera_handoff` (0 = off, 1 = saved) is read only when
// handoff_view is absent; an invalid value there also gives saved and a warning.
// `show_quick_bar = 0 or 1` and `quick_bar_position = bottom or top`: missing means the default
// (on, bottom) with no warning; invalid gives the default and a warning. `key_hint_go` and
// `key_hint_save`: printable ASCII, at most 32 characters; invalid gives the default and a warning.
// `quick_bar_views`: comma separated short names, any letter case. Missing means cockpit,free,chase with
// no warning; an empty value means no buttons; the order is kept, repeats are dropped quietly, and
// unknown names are ignored with one warning.
ConfigResult parseConfig(const std::string& text);

// Result to use when the file does not exist or could not be read.
ConfigResult defaultConfig(const std::string& reason);

// True when every setting is inside its accepted range (the same rules parseConfig applies). When it
// is not, `problem` names the first bad setting.
bool validateConfig(const Config& config, std::string& problem);

// The names of `quickBarViews` joined with commas ("cockpit,free,chase"); empty for none.
std::string quickBarViewsText(const std::vector<std::string>& names);

// The ini text with the five user settings (transition_seconds, handoff_view, show_quick_bar,
// quick_bar_position, quick_bar_views) set to the values in `config`. Every other line, comment and unknown key is
// kept as it was; a setting that is not in the text yet is appended. Empty `oldText` gives a fresh
// file with a short header. Lines end in "\n".
std::string updateConfigText(const std::string& oldText, const Config& config);

}  // namespace mykam
