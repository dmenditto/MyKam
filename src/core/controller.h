#pragma once

#include <functional>
#include <string>
#include <vector>

#include "config.h"
#include "pose.h"
#include "sim.h"
#include "slot_store.h"
#include "transition.h"
#include "view_control.h"

// The camera state machine: idle, holding a saved view, or transitioning to one. Driven by the user
// commands and a per-frame tick. Talks to the world only through ISim, so it runs in unit tests.

namespace mykam {

enum class CameraState { Idle, Holding, Transitioning };

// Outcome of renaming a view. `Reset` means an empty name put a default name ("Cockpit view N" /
// "Outside view N") back.
enum class RenameResult { Renamed, Reset, TooLong, NotPrintable, UnknownView };

// What saving the current X-Plane view would store: a cockpit view (Internal group), an outside camera
// (External group), or nothing because saving is refused in this view (circle, tower and the like).
enum class SaveContext { Cockpit, External, Unavailable };

// Which side of the sim the user is looking from: inside the cockpit, or at an outside view (free camera,
// chase, circle, tower and so on, or a camera mykam holds).
enum class ViewSide { Cockpit, Outside };

// Outcome of applying settings. `NotSaved`: the setting is active but mykam.ini could not be written.
enum class ApplyResult { Saved, NotSaved, Invalid };

// Everything the edit dialog changes in one go. `numpad` is 0 (none) or 1..9; `key` is unset or a valid
// custom key.
struct ViewEdit {
    std::string name;
    std::string label;
    int numpad = 0;
    CustomKey key;
    bool favourite = false;
};
enum class EditResult { Saved, UnknownView, BadName, BadLabel, BadNumpad, BadKey };

class Controller {
public:
    // pluginFolder is the folder holding mykam.ini; saved cameras go in its "cameras" subfolder.
    Controller(ISim& sim, std::string pluginFolder);

    // Plugin start: read mykam.ini (FR7) and log the start line. A missing file means default settings.
    void start();
    // Plugin stop or disable: give the camera back and log (FR1.1, NFR2.3).
    void stop();

    // A new user aircraft was loaded (or the plugin was enabled with one already loaded):
    // release the camera, cancel any transition and load that aircraft's views (NFR2.4, FR5.1).
    // Views live in `<aircraftFolder>/mykam_views.txt`, next to the .acf. When that file does not
    // exist yet, the older per-plugin file `<plugin>/cameras/<name>.slots.txt` is read once instead
    // (it is never changed); the next save writes to the aircraft folder. Without a folder only the
    // plugin file is used.
    void onAircraftLoaded(const std::string& aircraftFileName, const std::string& aircraftFolder = std::string());

    // The file the views of the current aircraft are written to.
    std::string viewFilePath() const { return slotFilePath(); }

    // The hotkey commands (FR2.1, FR2.2, FR2.3); `hotkey` is 1 to 9, anything else is ignored.
    // go_slot_N goes to the view that has hotkey N (nothing: logs one line). save_slot_N saves the
    // current view into the view that has hotkey N (its name, favourite and hotkey stay), or creates a
    // new view with hotkey N when none has it.
    void saveSlot(int hotkey);
    void goToSlot(int hotkey);
    void release();

    // The window and quick bar actions. View-changing ones write the file at once; none of them runs
    // per frame.
    // What saving the current X-Plane view would store right now.
    SaveContext saveContext() const;
    // Cockpit while X-Plane shows a cockpit view or mykam animates a cockpit view; Outside while X-Plane
    // shows an outside view or mykam holds (or is handing over) an outside camera.
    ViewSide viewSide() const;
    // Save the current view as a new view with the default name, at the end of its group; with
    // `favourite` it is starred (shown in the quick bar) in the same write. Returns its id; 0 when nothing
    // was saved (refused view, 99 views already, value out of range), with one log line.
    int saveCurrentAsNew(bool favourite = false);
    // Replace the pose of a view with the current view; name, favourite and hotkey stay. Only when the
    // current view fits the view's group. False when nothing changed (one log line).
    bool replaceView(int id);
    void goToView(int id);
    // Rename: the name is trimmed and must be 1 to 32 printable ASCII characters; an empty name puts a
    // default name back.
    RenameResult renameView(int id, const std::string& name);
    // Remove a view from memory and from the file. False for an unknown id.
    bool deleteView(int id);
    bool setFavourite(int id, bool favourite);
    // Give the view hotkey 1..9 (taking it from the view that had it) or 0 to clear. False for an unknown
    // id or another number.
    bool setHotkey(int id, int hotkey);
    // Quick bar label (1 to 2 letters or digits, "" clears); false for an unknown id or a bad label.
    bool setLabel(int id, const std::string& label);
    // Custom key combination (a key another view has moves here); an unset key clears. False for an
    // unknown id or an invalid key. The plugin is told through the keys-changed callback.
    bool setViewKey(int id, const CustomKey& key);
    // The edit dialog's Save: checks every field first and, when all are fine, applies them with one write.
    // Nothing changes on any other result.
    EditResult editView(int id, const ViewEdit& edit);
    // Reordering inside a group (persisted at once). `moveView` puts `id` in the place of `targetId` (same
    // group); `moveViewStep` moves it one place before (-1) or after (+1), jumping over non-favourites when
    // `favouritesOnly` (the quick bar). False when nothing moved.
    bool moveView(int id, int targetId);
    bool moveViewStep(int id, int direction, bool favouritesOnly);
    // "Ctrl+Shift+W": modifiers (Ctrl, Shift, Alt) then X-Plane's name for the key; "" when unset.
    std::string keyText(const CustomKey& key);
    // The key shown for a view: its custom key text, else "Num N" for hotkey N, else "".
    std::string viewKeyText(const SavedView& view);
    // The plugin registers X-Plane hot keys from this. The callback runs (never per frame) when the set of
    // wanted keys changed: after loading an aircraft, a key edit, a rename, a delete. Setting it forgets the
    // keys reported so far, so the first change reports everything again.
    void setOnKeysChanged(std::function<void()> callback);
    std::vector<WantedKey> wantedKeys() const { return slots_.wantedKeys(); }
    // Go to the next / previous view in group order (Internal, then External; favourites first), wrapping
    // round. Without an active view, next gives the first and previous the last.
    void next();
    void previous();
    // Run one of the X-Plane view commands (sim/view/chase and so on) the way the user would: the
    // camera is released first. Only names on the tracked list are run. False if not run.
    bool selectXPlaneView(const std::string& command);
    // Validate, apply and save the settings in mykam.ini (other lines of the file are kept).
    ApplyResult applySettings(const Config& wanted);

    // The id of the view the user last went to, while it is still the view in use: 0 when X-Plane is in
    // control (after release, a chosen X-Plane view, a lost camera or a new aircraft).
    int activeView() const { return activeView_; }
    // One line for the window header: "<name> - Free camera", or "X-Plane in control".
    std::string stateText() const;
    // Grows with every change a window shows (views, active view, settings), so a model can cache.
    unsigned revision() const { return revision_; }
    const Config& config() const { return config_; }

    // X-Plane or another plugin took the camera away from us.
    void onCameraLost();

    // X-Plane's own view commands (move, rotate, zoom). Returns true when the command was used on the held external camera and X-Plane must not also act on it; false means pass it through (nothing is held, or the command began before we held the camera).
    bool onViewCommand(ViewAction action, ViewSpeed speed, CommandPhase phase);

    // Mouse control of the held external camera, from the full-screen mouse layer the controller
    // shows through ISim::setMouseCapture. Positions are in any consistent unit (pixels or boxels).
    // Each returns true when a held external camera used the event; false (nothing held) means the
    // event was ignored. Right button: down remembers the position, drag orbits around the aircraft
    // reference point, up ends the drag. Wheel: positive clicks move closer, negative farther.
    bool onMouseDown(int x, int y);
    bool onMouseDrag(int x, int y);
    bool onMouseUp();
    bool onMouseWheel(int clicks);

    // Per-frame update of the cockpit head animation (flight loop). Returns true while it should be called again.
    bool tickCockpit(double dtSeconds);

    // Per-frame update. Returns true and fills `outWorld` while the camera should be placed.
    // Returns false when idle. Does no file access, no logging and no allocation (NFR1.3, NFR1.4, NFR5.3).
    bool tick(double dtSeconds, Pose& outWorld);

    // The user ran this X-Plane view command (sim/view/chase and so on). Remembered as the last view
    // command while it is on the tracked list; ignored for names off the list and for the commands
    // mykam runs itself (hand-off, switch to the cockpit view), which X-Plane reports back here.
    void onViewSelected(const std::string& command);
    const std::string& lastViewCommand() const { return lastView_; }

    CameraState state() const { return state_; }
    const ViewStore& slots() const { return slots_; }
    double transitionSeconds() const { return config_.transitionSeconds; }
    const std::string& aircraftName() const { return aircraft_; }
    bool cockpitAnimating() const { return cockpitActive_; }
    bool handoffPending() const { return handoffPending_; }

    // Flight loop, one frame after the camera arrived at an outside slot with handoff_view not off:
    // run the slot's stored view command (handoff_view = saved, when it is a known outside view and
    // exists in X-Plane) or else sim/view/free_camera, give the camera back and go Idle (logs one
    // line). If the free camera command is missing too, logs one warning and keeps holding. Does
    // nothing when no hand-over is pending.
    void performHandoff();

private:
    void log(const std::string& message);
    std::string slotFilePath() const;
    std::string legacySlotFilePath() const;
    // Write the view file after a change and log `what` (or a warning). False on a write error.
    bool persistSlots(const std::string& what);
    void touch() { ++revision_; }
    void notifyKeys();
    void step(int direction);
    // What the current X-Plane view is, as a pose in the body frame. False (with one log line naming
    // `what`, e.g. "slot 2" or "view") when saving is refused or the value cannot be stored.
    struct Captured {
        Pose body;
        ViewKind kind = ViewKind::External;
        std::string viewCommand;
    };
    bool captureCurrent(const std::string& what, Captured& out);
    void releaseIfHeld();
    // Show the mouse layer exactly while an external camera is held (holding or transitioning).
    void syncMouseCapture();
    // The user takes over a running blend: stop where it is and hold that view.
    void takeOverTransition();
    // The user moved the held camera: cancel a planned hand-over.
    void userTookCamera();
    bool goToCockpitView(const std::string& name, const Pose& target);
    Pose currentBody() const;
    // Run a view command for mykam's own purposes: X-Plane's report of it does not change lastView_.
    bool runOwnViewCommand(const std::string& name);

    ISim& sim_;
    std::string folder_;
    std::string aircraft_;
    std::string aircraftFolder_;  // the folder holding the aircraft's .acf; empty when unknown
    ViewStore slots_;
    std::function<void()> onKeysChanged_;
    std::vector<WantedKey> reportedKeys_;
    Config config_;
    Transition transition_;
    Pose held_;  // the external camera pose (body frame) while Holding
    ViewMotion motion_;
    Transition cockpit_;
    bool cockpitActive_ = false;
    bool mouseCaptureOn_ = false;
    bool dragActive_ = false;
    bool handoffArmed_ = false;    // an outside slot was chosen and the hand-over has not been requested yet
    bool handoffPending_ = false;  // requested; performHandoff does it on the next frame
    bool ownCommand_ = false;      // mykam itself is running a view command right now
    std::string lastView_;         // last tracked view command the user ran, empty if none yet
    int heldView_ = 0;
    int activeView_ = 0;
    unsigned revision_ = 1;
    int dragX_ = 0;
    int dragY_ = 0;
    CameraState state_ = CameraState::Idle;
};

}  // namespace mykam
