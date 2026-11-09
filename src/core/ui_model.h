#pragma once

#include <array>
#include <cstddef>
#include <string>
#include <vector>

#include "controller.h"
#include "slot_store.h"

// Everything the windows decide, as plain data and actions on the Controller. The ImGui code in the
// plugin only reads these values and calls these actions; it holds no rules of its own. Pure code, no
// X-Plane calls, tested with the fake ISim.

namespace mykam {

enum class RowType { Cockpit, FreeCamera, Chase };

// One saved view in the Internal or External list.
struct ViewRow {
    int id = 0;
    std::string name;
    RowType type = RowType::FreeCamera;
    std::string typeLabel;  // "Cockpit", "Free camera", "Chase"
    bool favourite = false;
    bool active = false;
    std::string shortName;  // the name cut to kChipNameChars with "..." at the end, for the quick bar chips
    int hotkey = 0;         // 0 = no key
    std::string keyBadge;   // from mykam.ini key_hint_go, "{n}" replaced; "" when there is no key or no hint
    std::string label;      // quick bar label (1 to 2 characters, uppercase), "" when none
    std::string keyText;    // the custom key ("Ctrl+Shift+W"), else "Num N", else "" (the window shows a dash)
    bool customKey = false;
    XprMode xpr = XprMode::Keep;  // what XPRealistic does on arrival
};

// Which kind of key the edit dialog assigns.
enum class KeyMode { None, Numpad, Custom };

// The edit dialog's fields while it is open. Nothing is stored until Save.
struct EditDraft {
    int viewId = 0;  // 0 = no such view
    std::string name;
    std::string label;
    KeyMode keyMode = KeyMode::None;
    int numpad = 0;     // 1..10 (10 = numpad 0) when keyMode is Numpad
    CustomKey custom;   // the captured key when keyMode is Custom
    bool favourite = false;
    XprMode xpr = XprMode::Keep;  // XPRealistic on arrival
};

// What is wrong with a draft, one text per field ("" = fine). Save is allowed when all are empty.
struct EditCheck {
    std::string name;
    std::string label;
    std::string key;
    bool ok() const { return name.empty() && label.empty() && key.empty(); }
};

// One of the nine hotkey buttons in the quick bar.
struct HotkeyButton {
    int hotkey = 0;
    int viewId = 0;       // the view that has this key, 0 when free
    bool assigned = false;
    bool active = false;
    std::string tooltip;  // the view name, or "Key N (free)"
};

// A button in the "X-Plane views" tab, running one sim/view/* command (all on the tracked list).
struct XPlaneViewButton {
    const char* label;
    const char* command;
};

inline constexpr XPlaneViewButton kXPlaneViewButtons[] = {
    {"3D cockpit", "sim/view/3d_cockpit_cmnd_look"},
    {"Free camera", "sim/view/free_camera"},
    {"Chase", "sim/view/chase"},
    {"Circle", "sim/view/circle"},
    {"Tower", "sim/view/tower"},
    {"Runway", "sim/view/runway"},
    {"Linear spot", "sim/view/linear_spot"},
    {"Still spot", "sim/view/still_spot"},
    {"Ride-along", "sim/view/ridealong"},
    {"Wingman", "sim/view/wingman"},
};
inline constexpr std::size_t kXPlaneViewButtonCount = sizeof(kXPlaneViewButtons) / sizeof(kXPlaneViewButtons[0]);

// One X-Plane view button of the quick bar, chosen by the quick_bar_views setting.
struct QuickViewButton {
    std::size_t index = 0;        // into kXPlaneViewButtons and kQuickViewNames
    std::string name;             // the short name in mykam.ini ("chase")
    const char* label = "";       // the full view name for the tooltip ("Chase")
    const char* command = "";     // the sim/view command it runs
    bool active = false;         // X-Plane is in this view now
};

// The quick bar's three default view buttons (indexes into kXPlaneViewButtons).
inline constexpr std::size_t kQuickViewCockpit = 0;
inline constexpr std::size_t kQuickViewFree = 1;
inline constexpr std::size_t kQuickViewChase = 2;

// A quick bar chip shows at most this many characters of a name (the last three are "..." when cut).
inline constexpr std::size_t kChipNameChars = 14;

// What the X-Plane views page explains about saving.
inline constexpr const char* kSaveNote = "Saving is available in cockpit, free camera and chase views.";

class UiModel {
public:
    explicit UiModel(Controller& controller);

    // Rebuild the cached values when the controller changed since the last call. Call once per frame;
    // it does nothing (and allocates nothing) when nothing changed.
    void refresh();

    // Cached values (valid after refresh).
    // The rows of one group in group order (the order field); starred views are not placed first.
    const std::vector<ViewRow>& rows(ViewGroup group) const {
        return group == ViewGroup::Internal ? internal_ : external_;
    }
    // The starred rows of one group, in group order: what the quick bar shows.
    const std::vector<ViewRow>& starredRows(ViewGroup group) const {
        return group == ViewGroup::Internal ? starredInternal_ : starredExternal_;
    }
    int rowCount(ViewGroup group) const { return static_cast<int>(rows(group).size()); }
    const std::array<HotkeyButton, kHotkeyCount>& hotkeyButtons() const { return hotkeys_; }
    // "<name> - <type>" of the active view, or "X-Plane in control".
    const std::string& headerState() const { return header_; }
    // Name and type label of the active view; both empty when X-Plane is in control.
    const std::string& activeName() const { return activeName_; }
    const std::string& activeTypeLabel() const { return activeType_; }
    int activeView() const { return controller_.activeView(); }
    bool anySaved() const { return controller_.slots().count() > 0; }
    const Config& settings() const { return controller_.config(); }
    // Hidden while MyKam is off, whatever the setting says.
    bool quickBarVisible() const { return controller_.config().showQuickBar && controller_.active(); }
    bool active() const { return controller_.active(); }
    void toggleActive() { controller_.toggleActive(); }
    // Other plugins. The window shows a warning while another camera plugin runs, and the edit dialog's
    // XPRealistic choice while XPRealistic is loaded (or the view already has a choice).
    const std::string& otherCameraPlugins() const { return controller_.otherCameraPlugins(); }
    std::string cameraConflictNote() const;
    bool xprAvailable() const { return controller_.xprAvailable(); }
    // The quick bar's XPRealistic button: shown while XPRealistic is loaded; "running" when its master
    // switch is on and it is not paused. Click pauses or resumes.
    bool xprRunning() const { return controller_.xprMasterOn() && !controller_.xprFrozen(); }
    bool xprMasterOn() const { return controller_.xprMasterOn(); }
    bool toggleXpr() { return controller_.toggleXprFreeze(); }
    // The tooltip of that button.
    std::string xprButtonTooltip() const;
    // "XPR on" / "XPR off" for a row whose view sets XPRealistic, "" for Leave as is.
    static const char* xprRowText(XprMode mode);
    bool showXprChoice(const EditDraft& draft) const {
        return controller_.xprAvailable() || draft.xpr != XprMode::Keep;
    }
    // The X-Plane view command the user last chose (empty if none yet); the window highlights its tile.
    const std::string& lastXPlaneView() const { return controller_.lastViewCommand(); }
    bool quickBarAtTop() const { return controller_.config().quickBarPosition == QuickBarPosition::Top; }

    // The quick bar group. Call once per frame (and before reading the group): the group follows the
    // current view side (Internal in a cockpit view, External in an outside view). A manual choice
    // (setBarGroup, toggleBarGroup) holds until the view side changes, then auto-follow resumes.
    void updateBar();
    ViewGroup barGroup() const { return barGroup_; }
    bool barManual() const { return barManual_; }
    // The chips of the quick bar: the starred views of the shown group, in group order.
    const std::vector<ViewRow>& barRows() const { return starredRows(barGroup_); }
    // The "+" button: saves the current view as a new, starred view of the shown group, enabled by the
    // same rule as the window. Returns the new id, 0 when nothing was saved.
    bool canSaveToBar() const { return canSaveCurrent(barGroup_); }
    int saveToBar();
    void setBarGroup(ViewGroup group);
    void toggleBarGroup();
    // `name` cut to kChipNameChars characters, "..." replacing the end when it was longer.
    static std::string chipName(const std::string& name);

    // Live values (read from X-Plane's current view each call, so they are not cached).
    // True when the current X-Plane view can be saved into this group: a cockpit view for Internal, the
    // free camera or chase view (or a camera mykam holds) for External.
    bool canSaveCurrent(ViewGroup group) const;
    // What to tell the user when canSaveCurrent is false.
    static const char* saveDisabledReason(ViewGroup group);

    // View actions.
    void go(int id) { controller_.goToView(id); }
    // Save the current view as a new view of this group. 0 (and nothing saved) when the current view does
    // not fit the group, the list is full or the value cannot be stored.
    int saveCurrent(ViewGroup group);
    // Replace a view with the current view (name, favourite and key stay); false when it does not fit.
    bool replace(int id) { return controller_.replaceView(id); }
    // True when "Update to current view" applies to this view: it exists and the current X-Plane view fits
    // its group.
    bool canReplace(int id) const;
    // Starring puts a view in the quick bar; removing the star takes it out (the view stays in its list).
    bool removeFromBar(int id) { return controller_.setFavourite(id, false); }
    // Order inside the group. Up and down move one place in the whole group list; left and right move
    // past the next starred view in the quick bar. `dropOn` puts a dragged view in the place of another
    // one of the same group. All false when nothing moved.
    bool moveUp(int id) { return controller_.moveViewStep(id, -1, false); }
    bool moveDown(int id) { return controller_.moveViewStep(id, 1, false); }
    bool moveLeft(int id) { return controller_.moveViewStep(id, -1, true); }
    bool moveRight(int id) { return controller_.moveViewStep(id, 1, true); }
    bool dropOn(int id, int targetId) { return controller_.moveView(id, targetId); }
    bool canMove(int id, int direction, bool starredOnly) const;

    // The edit dialog. `beginEdit` fills a draft from the view (an unknown id gives viewId 0). `checkEdit`
    // says what is wrong; `canSaveEdit` is true when nothing is. `saveEdit` stores everything with one
    // write; on a bad draft nothing changes and it returns the matching problem.
    EditDraft beginEdit(int id) const;
    EditCheck checkEdit(const EditDraft& draft) const;
    bool canSaveEdit(const EditDraft& draft) const { return draft.viewId != 0 && checkEdit(draft).ok(); }
    EditResult saveEdit(const EditDraft& draft);
    // The name of another view that already has the key the draft picks ("" when it is free or the draft
    // has no key). Saving moves the key here.
    std::string editKeyConflict(const EditDraft& draft) const;
    // "Ctrl+Shift+W" for a custom key, "" when unset.
    std::string customKeyText(const CustomKey& key) { return controller_.keyText(key); }
    // The label as it will be stored (uppercase) for a live preview; "" when it is empty or invalid.
    static std::string labelPreview(const std::string& raw);
    // The note under the custom key field.
    static const char* customKeyNote();
    RenameResult rename(int id, const std::string& text) { return controller_.renameView(id, text); }
    bool toggleFavourite(int id);
    // Key 1..9, or 0 for none. A key another view has moves to this one.
    bool assignKey(int id, int hotkey) { return controller_.setHotkey(id, hotkey); }
    // The name of the view that has this key, "" when it is free or `exceptId` has it.
    std::string keyOwnerName(int hotkey, int exceptId) const;
    bool remove(int id) { return controller_.deleteView(id); }
    // The quick bar: left click goes to the view with this key, right click saves into it.
    void goHotkey(int hotkey) { controller_.goToSlot(hotkey); }
    void saveHotkey(int hotkey) { controller_.saveSlot(hotkey); }
    void next() { controller_.next(); }
    void previous() { controller_.previous(); }
    void release() { controller_.release(); }
    // Index into kXPlaneViewButtons; false for a bad index.
    bool selectXPlaneView(std::size_t index);

    // The X-Plane view buttons of the quick bar in bar order (the quick_bar_views setting), each with
    // whether X-Plane is in that view now: no saved view is active and the last chosen X-Plane view is
    // this one. Rebuilt on every call (the last view can change without a controller revision).
    const std::vector<QuickViewButton>& quickViewButtons() const;
    // True when the short name is in the quick bar.
    bool quickViewShown(const std::string& name) const;

    // Settings actions, each saved to mykam.ini at once. The transition time is rounded to 0.1 s.
    ApplyResult setTransitionSeconds(double seconds);
    ApplyResult setHandoffView(HandoffView view);
    ApplyResult setShowQuickBar(bool show);
    ApplyResult setQuickBarPosition(QuickBarPosition position);
    // The whole list of quick bar view names (invalid names or repeats give Invalid, nothing changes).
    ApplyResult setQuickBarViews(const std::vector<std::string>& names);
    // Adds the view at the end of the bar when it is not there, removes it when it is. Invalid for an
    // unknown name.
    ApplyResult toggleQuickBarView(const std::string& name);
    // Moves a shown view one place earlier (direction -1) or later (+1) in the bar. Invalid when it is
    // not shown or already at that end; nothing changes then.
    ApplyResult moveQuickBarView(const std::string& name, int direction);

    // Plain-text message for a rename outcome ("" when it went through).
    static const char* renameProblem(RenameResult result);

private:
    static std::string hintFor(const std::string& pattern, int hotkey);
    ViewRow makeRow(const SavedView& view, int activeId) const;

    Controller& controller_;
    unsigned seenRevision_ = 0;
    std::vector<ViewRow> internal_;
    std::vector<ViewRow> external_;
    std::vector<ViewRow> starredInternal_;
    std::vector<ViewRow> starredExternal_;
    std::array<HotkeyButton, kHotkeyCount> hotkeys_{};
    std::string header_;
    std::string activeName_;
    std::string activeType_;
    mutable std::vector<QuickViewButton> quickViews_;
    ViewGroup barGroup_ = ViewGroup::Internal;
    ViewSide barSide_ = ViewSide::Cockpit;
    bool barSideKnown_ = false;
    bool barManual_ = false;
};

}  // namespace mykam
