#pragma once

#include <string>
#include <vector>

#include "companions.h"
#include "pose.h"

// Saved views for one aircraft: a list of up to 99 views (cockpit or external) kept in the aircraft
// body frame, with a validated text format (FR5, NFR2.5, NFR2.6, NFR2.7). Pure code, no X-Plane calls.
//
// A view has a stable id (never reused within a file), a name, a kind, a pose, an optional X-Plane view
// command, a favourite flag and an optional hotkey 1..10 (unique per aircraft), where 10 is numpad 0.
// The hotkey is what the commands mykam/go_slot_N and mykam/save_slot_N address (N is the key's digit,
// 0 for hotkey 10); a view without one is reached from the window.

namespace mykam {

constexpr int kMaxViews = 99;                          // saved views per aircraft
constexpr int kHotkeyCount = 10;                       // hotkeys 1..9, and 10 for numpad 0
constexpr int kHotkeyZero = 10;                        // the hotkey of numpad 0 (0 itself means "no key")
// The digit a hotkey is pressed with: 1..9, and 0 for kHotkeyZero.
constexpr int hotkeyDigit(int hotkey) { return hotkey == kHotkeyZero ? 0 : hotkey; }
inline std::string hotkeyText(int hotkey) { return std::to_string(hotkeyDigit(hotkey)); }
constexpr int kMaxViewId = 999999;                     // ids stay small so the file text stays small
constexpr std::size_t kMaxSlotFileBytes = 64 * 1024;  // NFR2.6
// The per-aircraft view file, kept in the aircraft's own folder next to its .acf.
inline constexpr const char* kAircraftViewFileName = "mykam_views.txt";

// Accepted ranges when reading or storing a pose (NFR2.5).
constexpr double kMaxPositionMetres = 10000.0;
constexpr double kMaxAngleDegrees = 360.0;
constexpr double kMinZoom = 0.1;
constexpr double kMaxZoom = 10.0;

// External views hold a camera pose (aircraft body frame) that mykam places while it holds the camera.
// Cockpit views hold X-Plane's pilot head position and angles (metres and degrees relative to the
// aircraft); going to one never takes the camera.
enum class ViewKind { External, Cockpit };
using SlotKind = ViewKind;  // the older name, kept so the earlier code and tests read the same

// The two groups of the window: Cockpit views are Internal, everything else is External.
enum class ViewGroup { Internal, External };
inline ViewGroup groupOf(ViewKind kind) {
    return kind == ViewKind::Cockpit ? ViewGroup::Internal : ViewGroup::External;
}

// True when every value is finite and inside the accepted range.
bool isValidPose(const Pose& pose);

// Name: printable ASCII (0x20 to 0x7E), trimmed, 1 to 32 characters.
constexpr std::size_t kMaxSlotNameLength = 32;
enum class NameCheck { Ok, Empty, TooLong, NotPrintable };
// Checks `raw`; on Ok, `trimmed` holds the name to store. A name made only of blanks is Empty.
NameCheck checkSlotName(const std::string& raw, std::string& trimmed);

// Quick bar label: 0 (none) to 2 characters, letters A-Z (stored uppercase) or digits.
constexpr std::size_t kMaxLabelLength = 2;
enum class LabelCheck { Ok, TooLong, BadCharacter };
// Checks `raw` (surrounding blanks are ignored; an empty label is Ok and means none). On Ok, `stored`
// holds the label to keep: uppercase, 0 to 2 characters.
LabelCheck checkLabel(const std::string& raw, std::string& stored);

// A custom key combination: an X-Plane virtual key code plus modifier flags. The flag bits are the same
// numbers as XPLMKeyFlags (xplm_ShiftFlag 1, xplm_OptionAltFlag 2, xplm_ControlFlag 4); the plugin checks
// that at compile time. vkey 0 means no key.
constexpr int kKeyShift = 1;
constexpr int kKeyAlt = 2;   // option / alt
constexpr int kKeyCtrl = 4;  // control
constexpr int kKeyFlagMask = kKeyShift | kKeyAlt | kKeyCtrl;
constexpr int kMaxVirtualKey = 255;
struct CustomKey {
    int vkey = 0;
    int flags = 0;
    bool set() const { return vkey != 0; }
    bool operator==(const CustomKey& o) const { return vkey == o.vkey && flags == o.flags; }
    bool operator!=(const CustomKey& o) const { return !(*this == o); }
};
// A usable key: vkey 1..255 and only known flag bits. Escape (cancels a capture) and Backspace (clears
// it) are refused so the capture field stays unambiguous.
bool isValidKey(const CustomKey& key);
// "vk:87,flags:shift+ctrl" (flag words in the order shift, ctrl, alt; "flags:none" for no modifier).
std::string encodeKey(const CustomKey& key);
// Reads the text form back. Flag words are case-insensitive: shift, ctrl or control, alt or option.
// False (and `key` untouched) for anything that is not a valid key.
bool decodeKey(const std::string& text, CustomKey& key);

struct SavedView {
    int id = 0;
    std::string name;
    ViewKind kind = ViewKind::External;
    Pose pose;
    std::string viewCommand;  // optional X-Plane view command, external views only
    bool favourite = false;   // shown in the quick bar
    int hotkey = 0;           // 0 = none, else 1..10 (numpad commands; 10 = numpad 0)
    std::string label;        // quick bar label, "" or 1 to 2 characters A-Z / 0-9
    CustomKey key;            // optional custom key combination
    int order = 0;            // position inside its group (1 = first), see ViewStore::ordered
    XprMode xpr = XprMode::Keep;  // what XPRealistic does on arrival (stored as viewN.xprealistic=on|off)
};

// One custom key the plugin should have registered with X-Plane, as decided by the core.
struct WantedKey {
    CustomKey key;
    int viewId = 0;
    std::string description;  // "MyKam: <view name>"
    bool operator==(const WantedKey& o) const {
        return key == o.key && viewId == o.viewId && description == o.description;
    }
};
struct KeyDiff {
    std::vector<WantedKey> remove;  // registered now, not wanted (or wanted differently)
    std::vector<WantedKey> add;     // wanted, not registered yet
};
// What has to change to go from the registered keys to the wanted keys. An entry that differs in key,
// view or description counts as removed and added.
KeyDiff diffKeys(const std::vector<WantedKey>& registered, const std::vector<WantedKey>& wanted);

struct ViewParseResult {
    int loaded = 0;                     // views accepted
    bool migrated = false;              // the text was an old slot file (no format line) and was converted
    std::vector<std::string> problems;  // one short text per rejected line, view or file
};

class ViewStore {
public:
    void clear();

    int count() const { return static_cast<int>(views_.size()); }
    int count(ViewGroup group) const;
    bool full() const { return count() >= kMaxViews; }
    // All views in creation order (ascending id).
    const std::vector<SavedView>& all() const { return views_; }
    const SavedView* find(int id) const;
    // The id the next added view will get. Ids are never reused, not even after a delete.
    int nextId() const { return nextId_; }

    // Add a view. Returns its id, or 0 when the store is full, the pose is invalid or ids ran out.
    // `viewCommand` is kept only for external views and only when it is a valid command name. An empty
    // `name` gets the default ("Cockpit view N" / "Outside view N"). A `hotkey` of 1..9 is taken from
    // any other view that has it; anything else means none.
    int add(const Pose& pose, ViewKind kind, const std::string& viewCommand = std::string(), int hotkey = 0,
            const std::string& name = std::string());
    // Replace pose, kind and view command; name, favourite, hotkey, label and key stay. A view that
    // changes group goes to the end of the new group. False for an unknown id or an invalid pose.
    bool update(int id, const Pose& pose, ViewKind kind, const std::string& viewCommand = std::string());
    // Store a name that already passed checkSlotName (trimmed, 1 to 32 printable ASCII).
    bool setName(int id, const std::string& name);
    bool setFavourite(int id, bool favourite);
    // Hotkey 1..9 moves it from the view that had it; 0 clears. False for an unknown id or another number.
    bool setHotkey(int id, int hotkey);
    // Quick bar label: anything checkLabel accepts (stored uppercase, "" clears). False for an unknown
    // id or a bad label.
    bool setLabel(int id, const std::string& label);
    // Custom key: a valid key moves from the view that had it; vkey 0 clears. False for an unknown id or
    // an invalid key.
    bool setKey(int id, const CustomKey& key);
    // What XPRealistic does when the view is reached. False for an unknown id.
    bool setXpr(int id, XprMode mode);
    bool remove(int id);
    // The id of the view with this hotkey, 0 when none.
    int hotkeyOwner(int hotkey) const;
    // The id of the view with this custom key, 0 when none (or the key is not set).
    int keyOwner(const CustomKey& key) const;

    // Order inside a group. Moves view `id` to the place of view `targetId` (both in the same group; the
    // views in between shift by one) and renumbers the group 1..n. False when nothing moved.
    bool moveTo(int id, int targetId);
    // The view before (direction -1) or after (+1) `id` in its group order; with `favouritesOnly` the
    // neighbours that are not favourites are skipped. 0 when there is none.
    int neighbour(int id, int direction, bool favouritesOnly) const;
    // The name a new view of this kind gets: the lowest "Cockpit view N" / "Outside view N" not in use.
    // `exceptId` names a view whose own name does not count as used (renaming it back to the default).
    std::string defaultName(ViewKind kind, int exceptId = 0) const;

    // Ids of one group sorted by the order field (ties by id). Favourites get no special place.
    std::vector<int> ordered(ViewGroup group) const;
    // The favourite views of one group, in the same order: what the quick bar shows.
    std::vector<int> favourites(ViewGroup group) const;
    // Every id in step order: Internal group, then External group, each as `ordered`.
    std::vector<int> allOrdered() const;
    // The keys to register, one per view that has a custom key.
    std::vector<WantedKey> wantedKeys() const;

    // Lookup by hotkey (the number of the older fixed slots). Empty or invalid hotkeys are never found.
    bool has(int hotkey) const;
    const Pose* get(int hotkey) const;
    ViewKind kind(int hotkey) const;
    const std::string& viewCommand(int hotkey) const;

    // Text form (format 2): `format=2`, `next_id=N`, then per view `viewID.field=value` lines: x, y, z,
    // pitch, heading, roll, zoom, kind, name, order, and the optional view, favourite, hotkey, label
    // (`viewN.label=A1`) and key (`viewN.key=vk:87,flags:shift+ctrl`). Files without order, label or key
    // lines still load: a view without order gets its place from the old display order (favourites first,
    // then creation order).
    std::string serialize() const;

    // Replace the contents from text. Text over 64 KB is ignored entirely. A text with a `format` line
    // other than 2 is ignored. A text with no `format` line is an old slot file (`slotN.field`, N 1..9):
    // each slot becomes a view with id N and hotkey N, keeping kind, name, favourite and view command
    // (`migrated` is set; nothing is written here). Good views are kept, broken ones are skipped and
    // described in `problems`. Never throws on bad input.
    ViewParseResult parse(const std::string& text);

private:
    int indexOf(int id) const;
    int nextOrder(ViewGroup group) const;
    void renumber(ViewGroup group);
    // After loading: gives views without an order a place (old display order: favourites first, then
    // creation order) and renumbers every group 1..n.
    void normalizeOrder();
    std::vector<SavedView> views_;  // ascending id
    int nextId_ = 1;
};

// The view after `currentId` (direction +1) or before it (direction -1) in step order (Internal then
// External), wrapping round. `currentId` 0 or an unknown id means no position yet: +1 gives the first
// view, -1 the last. Returns 0 when there are no views.
int stepView(const ViewStore& store, int currentId, int direction);

// "Cockpit", "Chase" or "Free camera" (an external view is Chase when its stored view is sim/view/chase).
const char* viewTypeLabel(const SavedView& view);

// Turn an aircraft file name (e.g. "Cessna_172SP.acf") into a safe file stem (NFR2.7):
// strips a trailing ".acf", replaces every character other than letters, digits, '-', '_' and '.'
// with '_', avoids leading dots, empty names and Windows reserved device names, and caps the length.
std::string sanitizeAircraftName(const std::string& aircraftFileName);

// "<sanitised name>.slots.txt"
std::string slotFileName(const std::string& aircraftFileName);

}  // namespace mykam
