#include "controller.h"

#include <cmath>
#include <utility>

#include "mouse_orbit.h"
#include "version.h"
#include "view_commands.h"

namespace mykam {

namespace {

constexpr const char* kLogPrefix = "[MyKam] ";
constexpr const char* kIniFileName = "mykam.ini";
constexpr std::size_t kMaxConfigBytes = 64 * 1024;

// Keep log lines to a single readable line, whatever the aircraft file name holds.
std::string printable(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        const unsigned char u = static_cast<unsigned char>(c);
        out.push_back(u < 0x20 || u == 0x7F ? '?' : c);
        if (out.size() >= 120) {
            out += "...";
            break;
        }
    }
    return out;
}

// "'Name'" for a saved view, "view <id>" when it is gone.
std::string viewLabel(const ViewStore& store, int id) {
    const SavedView* v = store.find(id);
    return v == nullptr ? "view " + std::to_string(id) : "'" + printable(v->name) + "'";
}

}  // namespace

Controller::Controller(ISim& sim, std::string pluginFolder) : sim_(sim), folder_(std::move(pluginFolder)) {}

void Controller::log(const std::string& message) {
    sim_.log(std::string(kLogPrefix) + message + "\n");
}

std::string Controller::slotFilePath() const {
    if (!aircraftFolder_.empty()) {
        return joinPath(aircraftFolder_, kAircraftViewFileName);
    }
    return legacySlotFilePath();
}

std::string Controller::legacySlotFilePath() const {
    return joinPath(joinPath(folder_, "cameras"), slotFileName(aircraft_));
}

void Controller::start() {
    const std::string iniPath = joinPath(folder_, kIniFileName);
    FileReadResult file = sim_.readFile(iniPath, kMaxConfigBytes);
    ConfigResult cfg;
    switch (file.status) {
        case FileStatus::Ok:
            cfg = parseConfig(file.text);
            break;
        case FileStatus::Missing:
            cfg = defaultConfig("file not found");
            break;
        case FileStatus::TooLarge:
            cfg = defaultConfig("file is larger than 64 KB");
            break;
        default:
            cfg = defaultConfig("file could not be read");
            break;
    }
    config_ = cfg.config;
    touch();
    log(std::string("started, version ") + kVersion);
    if (!cfg.warning.empty()) {
        log("warning: " + cfg.warning);
    }
}

void Controller::releaseIfHeld() {
    if (state_ != CameraState::Idle) {
        sim_.releaseCamera();
    }
    state_ = CameraState::Idle;
    transition_ = Transition();
    motion_.clear();
    cockpitActive_ = false;  // a running head animation ends; nothing is held afterwards
    handoffArmed_ = false;
    handoffPending_ = false;
    activeView_ = 0;
    touch();
    syncMouseCapture();
}

void Controller::syncMouseCapture() {
    const bool wanted = state_ != CameraState::Idle;
    if (!wanted) {
        dragActive_ = false;
    }
    if (wanted != mouseCaptureOn_) {
        mouseCaptureOn_ = wanted;
        sim_.setMouseCapture(wanted);
    }
}

void Controller::takeOverTransition() {
    if (state_ == CameraState::Transitioning) {
        held_ = transition_.current();
        transition_ = Transition();
        state_ = CameraState::Holding;
    }
}

Pose Controller::currentBody() const {
    return state_ == CameraState::Transitioning ? transition_.current() : held_;
}

void Controller::stop() {
    releaseIfHeld();
    log("stopped");
}

void Controller::onAircraftLoaded(const std::string& aircraftFileName, const std::string& aircraftFolder) {
    releaseIfHeld();
    aircraft_ = aircraftFileName;
    aircraftFolder_ = aircraftFolder;
    slots_.clear();
    heldView_ = 0;
    touch();

    const std::string shown = printable(aircraftFileName);
    FileReadResult file = sim_.readFile(slotFilePath(), kMaxSlotFileBytes);
    if (file.status == FileStatus::Missing && !aircraftFolder_.empty()) {
        // Views kept in the older per-plugin cameras folder (generic legacy layout): only read, never
        // changed; the next save writes the aircraft folder file.
        FileReadResult legacy = sim_.readFile(legacySlotFilePath(), kMaxSlotFileBytes);
        if (legacy.status != FileStatus::Missing) {
            file = std::move(legacy);
            if (file.status == FileStatus::Ok) {
                log("aircraft '" + shown + "': views read from the old cameras folder; the next save writes " +
                    kAircraftViewFileName + " in the aircraft folder");
            }
        }
    }
    switch (file.status) {
        case FileStatus::Ok: {
            const ViewParseResult r = slots_.parse(file.text);
            log("aircraft '" + shown + "': " + std::to_string(r.loaded) + " saved camera(s) loaded");
            if (r.migrated) {
                log("converted " + std::to_string(r.loaded) +
                    " view(s) from the old slot file format; the file is rewritten in the new format on the next save");
            }
            if (!r.problems.empty()) {
                log("warning: slot file for '" + shown + "' has " + std::to_string(r.problems.size()) +
                    " problem(s), first: " + r.problems.front());
            }
            break;
        }
        case FileStatus::Missing:
            log("aircraft '" + shown + "': 0 saved camera(s) loaded");
            break;
        case FileStatus::TooLarge:
            log("warning: slot file for '" + shown + "' is larger than 64 KB and was ignored");
            break;
        default:
            log("warning: slot file for '" + shown + "' could not be read");
            break;
    }
    notifyKeys();
}

SaveContext Controller::saveContext() const {
    if (state_ != CameraState::Idle) {
        return SaveContext::External;  // we are placing the camera: the view the user sees is an outside one
    }
    if (!sim_.isViewExternal()) {
        return SaveContext::Cockpit;
    }
    // Circle, tower, spot and similar views: X-Plane places these itself, so a saved camera could not be
    // handed back to them. Saving there is refused (by design).
    return isNoSaveViewCommand(lastView_) ? SaveContext::Unavailable : SaveContext::External;
}

ViewSide Controller::viewSide() const {
    if (cockpitActive_) {
        return ViewSide::Cockpit;  // a head animation runs: the view is a cockpit one
    }
    if (state_ != CameraState::Idle || handoffPending_) {
        return ViewSide::Outside;  // we hold an outside camera, or are about to hand it over
    }
    return sim_.isViewExternal() ? ViewSide::Outside : ViewSide::Cockpit;
}

bool Controller::captureCurrent(const std::string& what, Captured& out) {
    out = Captured();
    // The X-Plane outside view the user was last in, kept so the hand-off can return to it. Only a
    // known outside view type is stored; cockpit views store nothing.
    out.viewCommand = isHandoffViewCommand(lastView_) ? lastView_ : std::string();
    switch (saveContext()) {
        case SaveContext::Unavailable:
            log(what + " not saved: saving is not available in the " + lastView_ +
                " view, use the cockpit, the free camera or the chase view");
            return false;
        case SaveContext::Cockpit:
            // Cockpit view: store X-Plane's own pilot head values.
            out.body = sim_.readPilotHead();
            out.body.zoom = 1.0;
            out.kind = ViewKind::Cockpit;
            break;
        case SaveContext::External:
            if (state_ != CameraState::Idle) {
                // We are placing the camera: the in-between or adjusted view is what the user sees (FR2.1).
                out.body = currentBody();
            } else {
                Pose view = sim_.readView();
                if (!std::isfinite(view.zoom) || view.zoom <= 0.0) {
                    view.zoom = 1.0;
                }
                out.body = worldToBody(view, sim_.readAircraft());
            }
            break;
    }
    if (!isValidPose(out.body)) {
        log("warning: " + what + " not saved: the view is outside the accepted range");
        return false;
    }
    return true;
}

void Controller::saveSlot(int hotkey) {
    if (hotkey < 1 || hotkey > kHotkeyCount) {
        return;
    }
    const std::string what = "slot " + hotkeyText(hotkey);
    if (!active_) {
        log("MyKam is off, " + what + " not saved");
        return;
    }
    const int owner = slots_.hotkeyOwner(hotkey);
    if (owner == 0 && slots_.full()) {
        log(what + " not saved: the limit of " + std::to_string(kMaxViews) + " saved views is reached");
        return;
    }
    Captured c;
    if (!captureCurrent(what, c)) {
        return;
    }
    if (owner != 0) {
        if (!slots_.update(owner, c.body, c.kind, c.viewCommand)) {
            log("warning: " + what + " not saved: the view is outside the accepted range");
            return;
        }
    } else if (slots_.add(c.body, c.kind, c.viewCommand, hotkey) == 0) {
        log("warning: " + what + " not saved: the view is outside the accepted range");
        return;
    }
    touch();
    persistSlots("saved camera to " + what + (c.kind == ViewKind::Cockpit ? " (cockpit view)" : ""));
}

int Controller::saveCurrentAsNew(bool favourite) {
    if (!active_) {
        log("MyKam is off, view not saved");
        return 0;
    }
    if (slots_.full()) {
        log("view not saved: the limit of " + std::to_string(kMaxViews) + " saved views is reached");
        return 0;
    }
    Captured c;
    if (!captureCurrent("view", c)) {
        return 0;
    }
    const int id = slots_.add(c.body, c.kind, c.viewCommand);
    if (id == 0) {
        log("warning: view not saved: the view is outside the accepted range");
        return 0;
    }
    if (favourite) {
        slots_.setFavourite(id, true);
    }
    touch();
    persistSlots("saved camera as " + viewLabel(slots_, id) + (c.kind == ViewKind::Cockpit ? " (cockpit view)" : "") +
                 (favourite ? " (shown in the quick bar)" : ""));
    return id;
}

bool Controller::replaceView(int id) {
    const SavedView* view = slots_.find(id);
    if (view == nullptr) {
        return false;
    }
    if (!active_) {
        log("MyKam is off, " + viewLabel(slots_, id) + " not replaced");
        return false;
    }
    const ViewGroup group = groupOf(view->kind);
    const SaveContext context = saveContext();
    const bool fits = (group == ViewGroup::Internal && context == SaveContext::Cockpit) ||
                      (group == ViewGroup::External && context == SaveContext::External);
    if (!fits) {
        log("view " + viewLabel(slots_, id) + " not replaced: the current view does not fit the " +
            (group == ViewGroup::Internal ? "Internal" : "External") + " group");
        return false;
    }
    Captured c;
    if (!captureCurrent("view", c)) {
        return false;
    }
    if (!slots_.update(id, c.body, c.kind, c.viewCommand)) {
        log("warning: view not saved: the view is outside the accepted range");
        return false;
    }
    touch();
    persistSlots("replaced the camera of " + viewLabel(slots_, id));
    return true;
}

bool Controller::persistSlots(const std::string& what) {
    std::string error;
    if (sim_.writeFile(slotFilePath(), slots_.serialize(), error)) {
        log(what);
        return true;
    }
    log("warning: " + what + " (kept in memory only, could not write the slot file: " + printable(error) + ")");
    return false;
}

RenameResult Controller::renameView(int id, const std::string& name) {
    const SavedView* view = slots_.find(id);
    if (view == nullptr) {
        return RenameResult::UnknownView;
    }
    std::string clean;
    switch (checkSlotName(name, clean)) {
        case NameCheck::TooLong:
            return RenameResult::TooLong;
        case NameCheck::NotPrintable:
            return RenameResult::NotPrintable;
        case NameCheck::Empty: {
            const std::string fresh = slots_.defaultName(view->kind, id);
            if (fresh != view->name) {
                slots_.setName(id, fresh);
                touch();
                persistSlots(viewLabel(slots_, id) + " name reset to the default");
            }
            return RenameResult::Reset;
        }
        case NameCheck::Ok:
            break;
    }
    if (view->name != clean) {
        slots_.setName(id, clean);
        touch();
        persistSlots("view " + std::to_string(id) + " renamed to " + viewLabel(slots_, id));
        notifyKeys();
    }
    return RenameResult::Renamed;
}

void Controller::setOnKeysChanged(std::function<void()> callback) {
    onKeysChanged_ = std::move(callback);
    reportedKeys_.clear();
}

std::vector<WantedKey> Controller::wantedKeys() const {
    // While MyKam is off its custom keys are given back, so X-Plane sees those keys again.
    return active_ ? slots_.wantedKeys() : std::vector<WantedKey>();
}

void Controller::setActive(bool active) {
    if (active == active_) {
        return;
    }
    if (!active) {
        releaseIfHeld();  // X-Plane gets the camera back, and nothing of ours keeps running
    }
    active_ = active;
    touch();
    notifyKeys();
    log(active ? "turned on" : "turned off, X-Plane has the camera");
}

void Controller::notifyKeys() {
    const std::vector<WantedKey> wanted = wantedKeys();
    if (wanted == reportedKeys_) {
        return;
    }
    reportedKeys_ = wanted;
    if (onKeysChanged_) {
        onKeysChanged_();
    }
}

std::string Controller::keyText(const CustomKey& key) {
    if (!key.set()) {
        return std::string();
    }
    std::string text;
    if ((key.flags & kKeyCtrl) != 0) {
        text += "Ctrl+";
    }
    if ((key.flags & kKeyShift) != 0) {
        text += "Shift+";
    }
    if ((key.flags & kKeyAlt) != 0) {
        text += "Alt+";
    }
    const std::string name = printable(sim_.keyName(key.vkey));
    return text + (name.empty() ? "Key " + std::to_string(key.vkey) : name);
}

std::string Controller::viewKeyText(const SavedView& view) {
    if (view.key.set()) {
        return keyText(view.key);
    }
    return view.hotkey != 0 ? "Num " + hotkeyText(view.hotkey) : std::string();
}

bool Controller::setLabel(int id, const std::string& label) {
    const SavedView* view = slots_.find(id);
    std::string stored;
    if (view == nullptr || checkLabel(label, stored) != LabelCheck::Ok) {
        return false;
    }
    if (view->label != stored) {
        slots_.setLabel(id, stored);
        touch();
        persistSlots(stored.empty() ? "quick bar label removed from " + viewLabel(slots_, id)
                                    : "quick bar label " + stored + " set on " + viewLabel(slots_, id));
    }
    return true;
}

bool Controller::setViewKey(int id, const CustomKey& key) {
    const SavedView* view = slots_.find(id);
    if (view == nullptr || (key.set() && !isValidKey(key))) {
        return false;
    }
    if (view->key != key) {
        slots_.setKey(id, key);
        touch();
        persistSlots(key.set() ? "key " + keyText(key) + " assigned to " + viewLabel(slots_, id)
                               : "custom key removed from " + viewLabel(slots_, id));
        notifyKeys();
    }
    return true;
}

EditResult Controller::editView(int id, const ViewEdit& edit) {
    const SavedView* view = slots_.find(id);
    if (view == nullptr) {
        return EditResult::UnknownView;
    }
    std::string name;
    if (checkSlotName(edit.name, name) != NameCheck::Ok) {
        return EditResult::BadName;
    }
    std::string label;
    if (checkLabel(edit.label, label) != LabelCheck::Ok) {
        return EditResult::BadLabel;
    }
    if (edit.numpad < 0 || edit.numpad > kHotkeyCount) {
        return EditResult::BadNumpad;
    }
    if (edit.key.set() && !isValidKey(edit.key)) {
        return EditResult::BadKey;
    }
    bool changed = false;
    if (view->name != name) {
        slots_.setName(id, name);
        changed = true;
    }
    if (view->label != label) {
        slots_.setLabel(id, label);
        changed = true;
    }
    if (view->hotkey != edit.numpad) {
        slots_.setHotkey(id, edit.numpad);
        changed = true;
    }
    if (view->key != edit.key) {
        slots_.setKey(id, edit.key);
        changed = true;
    }
    if (view->favourite != edit.favourite) {
        slots_.setFavourite(id, edit.favourite);
        changed = true;
    }
    if (changed) {
        touch();
        persistSlots("edited " + viewLabel(slots_, id));
        notifyKeys();
    } else {
        log("edit of " + viewLabel(slots_, id) + " saved with no changes");
    }
    return EditResult::Saved;
}

bool Controller::moveView(int id, int targetId) {
    if (!slots_.moveTo(id, targetId)) {
        return false;
    }
    touch();
    persistSlots("moved " + viewLabel(slots_, id));
    return true;
}

bool Controller::moveViewStep(int id, int direction, bool favouritesOnly) {
    const int target = slots_.neighbour(id, direction, favouritesOnly);
    return target != 0 && moveView(id, target);
}

bool Controller::deleteView(int id) {
    const std::string label = viewLabel(slots_, id);
    if (!slots_.remove(id)) {
        return false;
    }
    if (activeView_ == id) {
        activeView_ = 0;
    }
    touch();
    persistSlots("deleted view " + label);
    notifyKeys();
    return true;
}

bool Controller::setFavourite(int id, bool favourite) {
    const SavedView* view = slots_.find(id);
    if (view == nullptr) {
        return false;
    }
    if (view->favourite != favourite) {
        slots_.setFavourite(id, favourite);
        touch();
        persistSlots(viewLabel(slots_, id) + (favourite ? " added to the quick bar" : " removed from the quick bar"));
    }
    return true;
}

bool Controller::setHotkey(int id, int hotkey) {
    const SavedView* view = slots_.find(id);
    if (view == nullptr || hotkey < 0 || hotkey > kHotkeyCount) {
        return false;
    }
    if (view->hotkey != hotkey) {
        slots_.setHotkey(id, hotkey);
        touch();
        persistSlots(hotkey == 0 ? "key removed from " + viewLabel(slots_, id)
                                 : "key " + hotkeyText(hotkey) + " assigned to " + viewLabel(slots_, id));
    }
    return true;
}

void Controller::step(int direction) {
    if (!active_) {
        log(std::string("MyKam is off, ") + (direction > 0 ? "next" : "previous") + " ignored");
        return;
    }
    const int target = stepView(slots_, activeView_, direction);
    if (target == 0) {
        log("no saved cameras to step through");
        return;
    }
    goToView(target);
}

void Controller::next() {
    step(1);
}

void Controller::previous() {
    step(-1);
}

bool Controller::selectXPlaneView(const std::string& command) {
    if (!isTrackedViewCommand(command)) {
        return false;
    }
    releaseIfHeld();  // X-Plane's own view takes over; no leftover camera control of ours
    if (!sim_.runViewCommand(command)) {
        log("warning: command " + command + " not found");
        return false;
    }
    onViewSelected(command);  // a user choice: remembered as the last view (a repeat of X-Plane's report is harmless)
    return true;
}

ApplyResult Controller::applySettings(const Config& wanted) {
    std::string problem;
    if (!validateConfig(wanted, problem)) {
        log("warning: settings not applied: " + problem);
        return ApplyResult::Invalid;
    }
    config_ = wanted;
    touch();
    const std::string iniPath = joinPath(folder_, kIniFileName);
    FileReadResult file = sim_.readFile(iniPath, kMaxConfigBytes);
    if (file.status != FileStatus::Ok && file.status != FileStatus::Missing) {
        // The existing file cannot be read back, so it is not overwritten: the settings stay active
        // until the plugin restarts.
        log("warning: settings applied but mykam.ini could not be read, so it was not changed");
        return ApplyResult::NotSaved;
    }
    std::string error;
    if (!sim_.writeFile(iniPath, updateConfigText(file.text, wanted), error)) {
        log("warning: settings applied but mykam.ini could not be written: " + printable(error));
        return ApplyResult::NotSaved;
    }
    log("settings saved to mykam.ini");
    return ApplyResult::Saved;
}

std::string Controller::stateText() const {
    if (!active_) {
        return "MyKam is off";
    }
    const SavedView* view = activeView_ != 0 ? slots_.find(activeView_) : nullptr;
    if (view != nullptr) {
        return view->name + " - " + viewTypeLabel(*view);
    }
    if (state_ != CameraState::Idle) {
        return "MyKam is holding the camera";
    }
    return "X-Plane in control";
}

void Controller::goToSlot(int hotkey) {
    if (hotkey < 1 || hotkey > kHotkeyCount) {
        return;
    }
    if (!active_) {
        log("MyKam is off, go to slot " + hotkeyText(hotkey) + " ignored");
        return;
    }
    const int id = slots_.hotkeyOwner(hotkey);
    if (id == 0) {
        log("slot " + hotkeyText(hotkey) + " is empty");
        return;
    }
    goToView(id);
}

void Controller::goToView(int id) {
    const SavedView* view = slots_.find(id);
    if (view == nullptr) {
        return;
    }
    if (!active_) {
        log("MyKam is off, go to " + viewLabel(slots_, id) + " ignored");
        return;
    }
    const Pose target = view->pose;
    const ViewKind kind = view->kind;
    const std::string label = viewLabel(slots_, id);
    const bool wasIdle = state_ == CameraState::Idle;
    if (wasIdle && sim_.isCameraHeldByOther()) {
        log("another plugin controls the camera, go to " + label + " ignored");
        return;
    }
    if (kind == ViewKind::Cockpit) {
        if (goToCockpitView(label, target)) {
            activeView_ = id;
            touch();
        }
        return;
    }
    cockpitActive_ = false;  // an external view ends any running head animation
    Pose from;
    if (wasIdle) {
        Pose current = sim_.readView();
        if (!std::isfinite(current.zoom) || current.zoom <= 0.0) {
            current.zoom = 1.0;
        }
        from = worldToBody(current, sim_.readAircraft());
        if (!sim_.takeCamera()) {
            log("warning: could not take camera control, go to " + label + " ignored");
            return;
        }
    } else {
        from = currentBody();  // retarget from the in-between or adjusted view (FR4.3)
    }
    transition_.begin(from, target, config_.transitionSeconds);
    held_ = target;
    heldView_ = id;
    handoffArmed_ = config_.handoffView != HandoffView::Off;  // handed over once the camera has arrived (tick)
    handoffPending_ = false;
    state_ = transition_.active() ? CameraState::Transitioning : CameraState::Holding;
    activeView_ = id;
    touch();
    syncMouseCapture();
}

bool Controller::goToCockpitView(const std::string& label, const Pose& target) {
    // A cockpit view never takes the camera: give back one we hold, then move X-Plane's pilot head.
    releaseIfHeld();
    if (sim_.isViewExternal()) {
        ownCommand_ = true;  // X-Plane reports this command back to onViewSelected: not the user's choice
        const bool switched = sim_.switchToCockpitView();
        ownCommand_ = false;
        if (!switched) {
            log("warning: could not switch to the cockpit view, go to " + label + " ignored");
            return false;
        }
    }
    cockpit_.begin(sim_.readPilotHead(), target, config_.transitionSeconds);
    if (cockpit_.active()) {
        cockpitActive_ = true;
        sim_.requestCockpitFrames();
    } else {
        Pose head = target;
        head.heading = wrap180(head.heading);
        sim_.writePilotHead(head);
    }
    return true;
}

bool Controller::tickCockpit(double dtSeconds) {
    if (!cockpitActive_) {
        return false;
    }
    cockpit_.update(dtSeconds);
    Pose head = cockpit_.current();
    head.heading = wrap180(head.heading);
    sim_.writePilotHead(head);
    if (!cockpit_.active()) {
        cockpitActive_ = false;  // done: nothing is held, X-Plane's own controls work again
        return false;
    }
    return true;
}

bool Controller::onViewCommand(ViewAction action, ViewSpeed speed, CommandPhase phase) {
    switch (phase) {
        case CommandPhase::Begin:
            if (state_ == CameraState::Idle) {
                return false;  // we hold nothing: X-Plane handles it
            }
            // The user takes over a blend: stop where it is and let the commands move that view.
            userTookCamera();
            takeOverTransition();
            motion_.press(action, speed);
            return true;
        case CommandPhase::Continue:
            return motion_.isPressed(action, speed);
        case CommandPhase::End:
            return motion_.release(action, speed);
    }
    return false;
}

void Controller::release() {
    releaseIfHeld();
}

void Controller::userTookCamera() {
    handoffArmed_ = false;  // the user is moving the camera: keep it, do not hand over
    handoffPending_ = false;
}

void Controller::performHandoff() {
    if (!handoffPending_) {
        return;
    }
    handoffPending_ = false;
    if (state_ != CameraState::Holding) {
        return;
    }
    const std::string label = viewLabel(slots_, heldView_);
    std::string command = kFreeCameraCommand;
    if (config_.handoffView == HandoffView::Saved) {
        const SavedView* held = slots_.find(heldView_);
        if (held != nullptr && isHandoffViewCommand(held->viewCommand)) {
            command = held->viewCommand;
        }
    }
    bool ran = runOwnViewCommand(command);
    if (!ran && command != kFreeCameraCommand) {
        log("warning: command " + command + " not found, using " + kFreeCameraCommand + " for " + label);
        command = kFreeCameraCommand;
        ran = runOwnViewCommand(command);
    }
    if (!ran) {
        log("warning: command " + command + " not found, keeping the camera at " + label);
        return;
    }
    lastView_ = command;   // the view X-Plane is in now
    sim_.releaseCamera();  // X-Plane's own view now owns the camera
    state_ = CameraState::Idle;
    transition_ = Transition();
    motion_.clear();
    syncMouseCapture();
    log("handed over to the X-Plane " + command + " view for " + label);
}

bool Controller::runOwnViewCommand(const std::string& name) {
    ownCommand_ = true;
    const bool ran = sim_.runViewCommand(name);
    ownCommand_ = false;
    return ran;
}

void Controller::onViewSelected(const std::string& command) {
    if (ownCommand_ || !isTrackedViewCommand(command)) {
        return;
    }
    lastView_ = command;
    if (activeView_ != 0) {
        activeView_ = 0;  // the user chose an X-Plane view: no saved view is the view in use any more
        touch();
    }
}

void Controller::onCameraLost() {
    if (!ownCommand_ && activeView_ != 0) {
        activeView_ = 0;  // MyKam's own hand-over keeps its view active; anyone else taking the camera does not
        touch();
    }
    handoffArmed_ = false;
    handoffPending_ = false;
    state_ = CameraState::Idle;
    transition_ = Transition();
    motion_.clear();
    syncMouseCapture();
}

bool Controller::onMouseDown(int x, int y) {
    if (state_ == CameraState::Idle) {
        return false;
    }
    dragActive_ = true;
    dragX_ = x;
    dragY_ = y;
    return true;
}

bool Controller::onMouseDrag(int x, int y) {
    if (state_ == CameraState::Idle) {
        return false;
    }
    if (!dragActive_) {
        dragActive_ = true;  // a drag whose press we missed starts here
        dragX_ = x;
        dragY_ = y;
        return true;
    }
    const int dx = x - dragX_;
    const int dy = y - dragY_;
    dragX_ = x;
    dragY_ = y;
    if (dx == 0 && dy == 0) {
        return true;
    }
    userTookCamera();
    takeOverTransition();
    // Like X-Plane's free camera: the camera stays put and looks around. Dragging right turns the
    // view right, dragging up (+y in X-Plane) tilts it up; the horizon stays level.
    held_ = lookAroundInPlace(held_, sim_.readAircraft(), dx * kLookDegreesPerPixel, dy * kLookDegreesPerPixel);
    return true;
}

bool Controller::onMouseUp() {
    dragActive_ = false;
    return state_ != CameraState::Idle;
}

bool Controller::onMouseWheel(int clicks) {
    if (state_ == CameraState::Idle) {
        return false;
    }
    if (clicks != 0) {
        userTookCamera();
        takeOverTransition();
        held_ = dollyTowardOrigin(held_, clicks);
    }
    return true;
}

bool Controller::tick(double dtSeconds, Pose& outWorld) {
    if (state_ == CameraState::Idle) {
        return false;
    }
    if (state_ == CameraState::Transitioning) {
        transition_.update(dtSeconds);
        if (!transition_.active()) {
            state_ = CameraState::Holding;
        }
    } else if (motion_.any()) {
        held_ = motion_.apply(held_, dtSeconds);  // X-Plane's view commands move the held camera
    }
    if (handoffArmed_ && state_ == CameraState::Holding) {
        // The camera has arrived and is placed below, so this frame renders there. The hand-over
        // itself runs from a flight loop on the next frame, never inside the camera callback.
        handoffArmed_ = false;
        handoffPending_ = true;
        sim_.requestHandoffFrame();
    }
    // The blend runs in the aircraft body frame, so the target keeps following the aircraft.
    outWorld = bodyToWorld(currentBody(), sim_.readAircraft());
    return true;
}

}  // namespace mykam
