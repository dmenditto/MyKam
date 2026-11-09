#include "ui_model.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include "view_commands.h"

namespace mykam {

UiModel::UiModel(Controller& controller) : controller_(controller) {
    refresh();
}

std::string UiModel::hintFor(const std::string& pattern, int hotkey) {
    std::string out;
    for (std::size_t i = 0; i < pattern.size(); ++i) {
        if (pattern.compare(i, 3, "{n}") == 0) {
            out += hotkeyText(hotkey);
            i += 2;
        } else {
            out.push_back(pattern[i]);
        }
    }
    return out;
}

ViewRow UiModel::makeRow(const SavedView& view, int activeId) const {
    ViewRow row;
    row.id = view.id;
    row.name = view.name;
    row.shortName = chipName(view.name);
    row.typeLabel = viewTypeLabel(view);
    if (view.kind == ViewKind::Cockpit) {
        row.type = RowType::Cockpit;
    } else {
        row.type = row.typeLabel == "Chase" ? RowType::Chase : RowType::FreeCamera;
    }
    row.favourite = view.favourite;
    row.active = view.id == activeId;
    row.hotkey = view.hotkey;
    if (view.hotkey != 0) {
        row.keyBadge = hintFor(controller_.config().keyHintGo, view.hotkey);
    }
    row.label = view.label;
    row.customKey = view.key.set();
    row.xpr = view.xpr;
    row.keyText = controller_.viewKeyText(view);
    return row;
}

void UiModel::refresh() {
    if (seenRevision_ == controller_.revision() && seenRevision_ != 0) {
        return;
    }
    seenRevision_ = controller_.revision();
    const ViewStore& store = controller_.slots();
    const int active = controller_.activeView();

    internal_.clear();
    external_.clear();
    starredInternal_.clear();
    starredExternal_.clear();
    for (int id : store.ordered(ViewGroup::Internal)) {
        internal_.push_back(makeRow(*store.find(id), active));
        if (internal_.back().favourite) {
            starredInternal_.push_back(internal_.back());
        }
    }
    for (int id : store.ordered(ViewGroup::External)) {
        external_.push_back(makeRow(*store.find(id), active));
        if (external_.back().favourite) {
            starredExternal_.push_back(external_.back());
        }
    }

    for (int n = 1; n <= kHotkeyCount; ++n) {
        HotkeyButton& button = hotkeys_[static_cast<std::size_t>(n - 1)];
        button = HotkeyButton();
        button.hotkey = n;
        const SavedView* view = store.find(store.hotkeyOwner(n));
        if (view == nullptr) {
            button.tooltip = "Key " + hotkeyText(n) + " (free)";
            continue;
        }
        button.viewId = view->id;
        button.assigned = true;
        button.active = view->id == active;
        button.tooltip = view->name;
    }

    const SavedView* activeView = active != 0 ? store.find(active) : nullptr;
    if (activeView != nullptr) {
        activeName_ = activeView->name;
        activeType_ = viewTypeLabel(*activeView);
    } else {
        activeName_.clear();
        activeType_.clear();
    }
    header_ = controller_.stateText();
}

std::string UiModel::xprButtonTooltip() const {
    if (!controller_.xprMasterOn()) {
        return "XPRealistic is switched off (its master switch)";
    }
    return controller_.xprFrozen() ? "XPRealistic effects paused - click to resume"
                                   : "XPRealistic effects on - click to pause";
}

const char* UiModel::xprRowText(XprMode mode) {
    switch (mode) {
        case XprMode::On: return "XPR on";
        case XprMode::Off: return "XPR off";
        case XprMode::Keep: break;
    }
    return "";
}

std::string UiModel::cameraConflictNote() const {
    const std::string& others = controller_.otherCameraPlugins();
    if (others.empty()) {
        return std::string();
    }
    return others + " is running too and can move the camera back after MyKam moves it. "
                    "Disable one of them in Plugins > Plugin Admin.";
}

std::string UiModel::chipName(const std::string& name) {
    if (name.size() <= kChipNameChars) {
        return name;
    }
    return name.substr(0, kChipNameChars - 3) + "...";
}

void UiModel::updateBar() {
    const ViewSide side = controller_.viewSide();
    if (barSideKnown_ && side == barSide_) {
        return;  // same kind of view: keep the group, a manual choice included
    }
    barSideKnown_ = true;
    barSide_ = side;
    barManual_ = false;  // the view kind changed: auto-follow resumes
    barGroup_ = side == ViewSide::Cockpit ? ViewGroup::Internal : ViewGroup::External;
}

void UiModel::setBarGroup(ViewGroup group) {
    updateBar();  // note the current view side first, so the choice is held against it
    if (group != barGroup_) {
        barGroup_ = group;
        barManual_ = true;
    }
}

void UiModel::toggleBarGroup() {
    updateBar();
    setBarGroup(barGroup_ == ViewGroup::Internal ? ViewGroup::External : ViewGroup::Internal);
}

bool UiModel::canSaveCurrent(ViewGroup group) const {
    const SaveContext context = controller_.saveContext();
    return (group == ViewGroup::Internal && context == SaveContext::Cockpit) ||
           (group == ViewGroup::External && context == SaveContext::External);
}

const char* UiModel::saveDisabledReason(ViewGroup group) {
    return group == ViewGroup::Internal ? "Switch to a cockpit view in X-Plane to save it here."
                                        : "Switch to the free camera or the chase view in X-Plane to save it here.";
}

int UiModel::saveCurrent(ViewGroup group) {
    if (!canSaveCurrent(group)) {
        return 0;
    }
    return controller_.saveCurrentAsNew();
}

int UiModel::saveToBar() {
    if (!canSaveCurrent(barGroup_)) {
        return 0;
    }
    return controller_.saveCurrentAsNew(true);
}

bool UiModel::canReplace(int id) const {
    const SavedView* view = controller_.slots().find(id);
    return view != nullptr && canSaveCurrent(groupOf(view->kind));
}

bool UiModel::canMove(int id, int direction, bool starredOnly) const {
    return controller_.slots().neighbour(id, direction, starredOnly) != 0;
}

EditDraft UiModel::beginEdit(int id) const {
    EditDraft draft;
    const SavedView* view = controller_.slots().find(id);
    if (view == nullptr) {
        return draft;
    }
    draft.viewId = id;
    draft.name = view->name;
    draft.label = view->label;
    draft.favourite = view->favourite;
    draft.xpr = view->xpr;
    if (view->key.set()) {
        draft.keyMode = KeyMode::Custom;
        draft.custom = view->key;
    } else if (view->hotkey != 0) {
        draft.keyMode = KeyMode::Numpad;
        draft.numpad = view->hotkey;
    }
    return draft;
}

EditCheck UiModel::checkEdit(const EditDraft& draft) const {
    EditCheck check;
    std::string clean;
    switch (checkSlotName(draft.name, clean)) {
        case NameCheck::Ok: break;
        case NameCheck::Empty: check.name = "Enter a name."; break;
        case NameCheck::TooLong: check.name = "The name can have at most 32 characters."; break;
        case NameCheck::NotPrintable: check.name = "Use printable ASCII characters only."; break;
    }
    std::string label;
    switch (checkLabel(draft.label, label)) {
        case LabelCheck::Ok: break;
        case LabelCheck::TooLong: check.label = "Use 1 or 2 characters."; break;
        case LabelCheck::BadCharacter: check.label = "Use letters A to Z or digits only."; break;
    }
    if (draft.keyMode == KeyMode::Numpad && (draft.numpad < 1 || draft.numpad > kHotkeyCount)) {
        check.key = "Pick a numpad slot from 0 to 9.";
    } else if (draft.keyMode == KeyMode::Custom && !isValidKey(draft.custom)) {
        check.key = "Press the key you want to use.";
    }
    return check;
}

EditResult UiModel::saveEdit(const EditDraft& draft) {
    const EditCheck check = checkEdit(draft);
    if (controller_.slots().find(draft.viewId) == nullptr) {
        return EditResult::UnknownView;
    }
    if (!check.name.empty()) {
        return EditResult::BadName;
    }
    if (!check.label.empty()) {
        return EditResult::BadLabel;
    }
    if (!check.key.empty()) {
        return draft.keyMode == KeyMode::Numpad ? EditResult::BadNumpad : EditResult::BadKey;
    }
    ViewEdit edit;
    edit.name = draft.name;
    edit.label = draft.label;
    edit.favourite = draft.favourite;
    edit.xpr = draft.xpr;
    if (draft.keyMode == KeyMode::Numpad) {
        edit.numpad = draft.numpad;
    } else if (draft.keyMode == KeyMode::Custom) {
        edit.key = draft.custom;
    }
    return controller_.editView(draft.viewId, edit);
}

std::string UiModel::editKeyConflict(const EditDraft& draft) const {
    const ViewStore& store = controller_.slots();
    int owner = 0;
    if (draft.keyMode == KeyMode::Custom && isValidKey(draft.custom)) {
        owner = store.keyOwner(draft.custom);
    } else if (draft.keyMode == KeyMode::Numpad) {
        owner = store.hotkeyOwner(draft.numpad);
    }
    const SavedView* view = owner != 0 && owner != draft.viewId ? store.find(owner) : nullptr;
    return view == nullptr ? std::string() : view->name;
}

std::string UiModel::labelPreview(const std::string& raw) {
    std::string stored;
    return checkLabel(raw, stored) == LabelCheck::Ok ? stored : std::string();
}

const char* UiModel::customKeyNote() {
    return "X-Plane does not tell plugins which keys it uses; if this key is bound in X-Plane, the X-Plane "
           "binding may stop working while MyKam uses it.";
}

bool UiModel::toggleFavourite(int id) {
    const SavedView* view = controller_.slots().find(id);
    return view != nullptr && controller_.setFavourite(id, !view->favourite);
}

std::string UiModel::keyOwnerName(int hotkey, int exceptId) const {
    const SavedView* view = controller_.slots().find(controller_.slots().hotkeyOwner(hotkey));
    return (view == nullptr || view->id == exceptId) ? std::string() : view->name;
}

bool UiModel::selectXPlaneView(std::size_t index) {
    if (index >= kXPlaneViewButtonCount) {
        return false;
    }
    return controller_.selectXPlaneView(kXPlaneViewButtons[index].command);
}

const std::vector<QuickViewButton>& UiModel::quickViewButtons() const {
    quickViews_.clear();
    const bool xplaneInControl = controller_.activeView() == 0;
    const std::string& last = controller_.lastViewCommand();
    for (const std::string& name : controller_.config().quickBarViews) {
        const int index = quickViewIndex(name);
        if (index < 0 || index >= static_cast<int>(kXPlaneViewButtonCount)) {
            continue;
        }
        QuickViewButton button;
        button.index = static_cast<std::size_t>(index);
        button.name = name;
        button.label = kXPlaneViewButtons[button.index].label;
        button.command = kXPlaneViewButtons[button.index].command;
        button.active = xplaneInControl && last == button.command;
        quickViews_.push_back(button);
    }
    return quickViews_;
}

bool UiModel::quickViewShown(const std::string& name) const {
    const std::vector<std::string>& names = controller_.config().quickBarViews;
    return std::find(names.begin(), names.end(), name) != names.end();
}

ApplyResult UiModel::setQuickBarViews(const std::vector<std::string>& names) {
    Config c = controller_.config();
    c.quickBarViews = names;
    return controller_.applySettings(c);
}

ApplyResult UiModel::toggleQuickBarView(const std::string& name) {
    if (quickViewIndex(name) < 0) {
        return ApplyResult::Invalid;
    }
    std::vector<std::string> names = controller_.config().quickBarViews;
    const std::vector<std::string>::iterator found = std::find(names.begin(), names.end(), name);
    if (found != names.end()) {
        names.erase(found);
    } else {
        names.push_back(name);
    }
    return setQuickBarViews(names);
}

ApplyResult UiModel::moveQuickBarView(const std::string& name, int direction) {
    std::vector<std::string> names = controller_.config().quickBarViews;
    const std::vector<std::string>::iterator found = std::find(names.begin(), names.end(), name);
    if (found == names.end() || (direction != -1 && direction != 1)) {
        return ApplyResult::Invalid;
    }
    const std::ptrdiff_t from = found - names.begin();
    const std::ptrdiff_t to = from + direction;
    if (to < 0 || to >= static_cast<std::ptrdiff_t>(names.size())) {
        return ApplyResult::Invalid;
    }
    std::swap(names[static_cast<std::size_t>(from)], names[static_cast<std::size_t>(to)]);
    return setQuickBarViews(names);
}

ApplyResult UiModel::setTransitionSeconds(double seconds) {
    Config c = controller_.config();
    // Round to 0.1 s (the slider step); NaN and infinity pass through to the validation.
    c.transitionSeconds = std::isfinite(seconds) ? std::round(seconds * 10.0) / 10.0 : seconds;
    return controller_.applySettings(c);
}

ApplyResult UiModel::setHandoffView(HandoffView view) {
    Config c = controller_.config();
    c.handoffView = view;
    return controller_.applySettings(c);
}

ApplyResult UiModel::setShowQuickBar(bool show) {
    Config c = controller_.config();
    c.showQuickBar = show;
    return controller_.applySettings(c);
}

ApplyResult UiModel::setQuickBarPosition(QuickBarPosition position) {
    Config c = controller_.config();
    c.quickBarPosition = position;
    return controller_.applySettings(c);
}

const char* UiModel::renameProblem(RenameResult result) {
    switch (result) {
        case RenameResult::TooLong: return "The name can have at most 32 characters.";
        case RenameResult::NotPrintable: return "Use printable ASCII characters only.";
        case RenameResult::UnknownView: return "This view no longer exists.";
        default: return "";
    }
}

}  // namespace mykam
