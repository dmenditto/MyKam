#include "slot_store.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <map>

#include "text_util.h"
#include "view_commands.h"

namespace mykam {

namespace {

constexpr int kFormatVersion = 2;
constexpr int kStoredDecimals = 4;   // 0.1 mm / 0.0001 degrees: far finer than FR3 needs, keeps files small
constexpr std::size_t kMaxStemLength = 64;
constexpr int kFieldCount = 7;
const char* const kFieldNames[kFieldCount] = {"x", "y", "z", "pitch", "heading", "roll", "zoom"};

bool inRange(double v, double limit) {
    return std::isfinite(v) && v >= -limit && v <= limit;
}

double* fieldRef(Pose& p, int index) {
    switch (index) {
        case 0: return &p.x;
        case 1: return &p.y;
        case 2: return &p.z;
        case 3: return &p.pitch;
        case 4: return &p.heading;
        case 5: return &p.roll;
        default: return &p.zoom;
    }
}

bool fieldValid(int index, double v) {
    if (index <= 2) {
        return inRange(v, kMaxPositionMetres);
    }
    if (index <= 5) {
        return inRange(v, kMaxAngleDegrees);
    }
    return std::isfinite(v) && v >= kMinZoom && v <= kMaxZoom;
}

// One view or slot as read from text, before it is checked as a whole.
struct Candidate {
    Pose pose;
    bool seen[kFieldCount] = {false, false, false, false, false, false, false};
    ViewKind kind = ViewKind::External;
    std::string view;
    std::string name;
    bool favourite = false;
    int hotkey = 0;
    std::string label;
    CustomKey key;
    int order = 0;  // 0 = the file gave none
    bool bad = false;
    std::string badReason;
};

constexpr int kMaxOrderValue = 100000;
constexpr int kKeyEscape = 0x1B;     // XPLM_VK_ESCAPE: cancels the capture field
constexpr int kKeyBackspace = 0x08;  // XPLM_VK_BACK: clears the capture field

bool parseWholeNumber(const std::string& s, int limit, int& out) {
    if (s.empty() || s.size() > 7) {
        return false;
    }
    int v = 0;
    for (char c : s) {
        if (c < '0' || c > '9') {
            return false;
        }
        v = v * 10 + (c - '0');
    }
    if (v > limit) {
        return false;
    }
    out = v;
    return true;
}

std::string upper(const std::string& s) {
    std::string r = s;
    for (char& c : r) {
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }
    return r;
}

bool isReservedDeviceName(const std::string& stem) {
    // Windows treats these names as devices even with an extension ("CON.slots.txt").
    const std::size_t dot = stem.find('.');
    const std::string base = upper(dot == std::string::npos ? stem : stem.substr(0, dot));
    if (base == "CON" || base == "PRN" || base == "AUX" || base == "NUL") {
        return true;
    }
    if (base.size() == 4 && (base.compare(0, 3, "COM") == 0 || base.compare(0, 3, "LPT") == 0) &&
        base[3] >= '1' && base[3] <= '9') {
        return true;
    }
    return false;
}

std::string lower(const std::string& s) {
    std::string r = s;
    for (char& c : r) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return r;
}

std::string shorten(const std::string& s) {
    if (s.size() <= 40) {
        return s;
    }
    return s.substr(0, 40) + "...";
}

// Reads the fields both file formats share into `c`. `label` is "slot 3" or "view 3" for messages.
// Returns false when `field` is not one of them.
bool readCommonField(Candidate& c, const std::string& field, const std::string& value, const std::string& label,
                     ViewParseResult& result) {
    if (field == "view") {
        if (isValidViewCommandName(value)) {
            c.view = value;
        } else {
            c.view.clear();  // the last view line decides; a bad one means no stored view
            result.problems.push_back(label + ": view '" + shorten(value) + "' ignored, it is not a sim/view/ command name");
        }
        return true;
    }
    if (field == "name") {
        // Optional: a bad name is ignored and the view still loads.
        std::string clean;
        if (checkSlotName(value, clean) == NameCheck::Ok) {
            c.name = clean;
        } else {
            c.name.clear();
            result.problems.push_back(label + ": name '" + shorten(value) +
                                      "' ignored, it must be 1 to 32 printable ASCII characters");
        }
        return true;
    }
    if (field == "favourite") {
        if (value == "1") {
            c.favourite = true;
        } else if (value == "0") {
            c.favourite = false;
        } else {
            c.favourite = false;
            result.problems.push_back(label + ": favourite '" + shorten(value) + "' ignored, it must be 0 or 1");
        }
        return true;
    }
    if (field == "kind") {
        const std::string lowered = lower(value);
        if (lowered == "cockpit") {
            c.kind = ViewKind::Cockpit;
        } else if (lowered == "external") {
            c.kind = ViewKind::External;
        } else if (!c.bad) {
            c.bad = true;
            c.badReason = label + " skipped: kind '" + shorten(value) + "' is not cockpit or external";
        }
        return true;
    }
    for (int f = 0; f < kFieldCount; ++f) {
        if (field != kFieldNames[f]) {
            continue;
        }
        double v = 0.0;
        if (!parseDouble(value, v)) {
            if (!c.bad) {
                c.bad = true;
                c.badReason = label + " skipped: " + field + " '" + shorten(value) + "' is not a number";
            }
        } else if (!fieldValid(f, v)) {
            if (!c.bad) {
                c.bad = true;
                c.badReason = label + " skipped: " + field + " " + shorten(value) + " is out of range";
            }
        } else {
            *fieldRef(c.pose, f) = v;
            c.seen[f] = true;
        }
        return true;
    }
    return false;
}

// True when the candidate is a complete, valid view. Adds a problem when it is not (a candidate that
// has no pose field at all, such as a lone kind line, stores nothing and is not a problem).
bool candidateUsable(const Candidate& c, const std::string& label, ViewParseResult& result) {
    const bool any = std::any_of(std::begin(c.seen), std::end(c.seen), [](bool b) { return b; });
    if (c.bad) {
        result.problems.push_back(c.badReason);
        return false;
    }
    if (!any) {
        return false;
    }
    const bool all = std::all_of(std::begin(c.seen), std::end(c.seen), [](bool b) { return b; });
    if (!all) {
        result.problems.push_back(label + " skipped: some fields are missing");
        return false;
    }
    return true;
}

// "view12.x" -> id 12, field "x". False for anything else.
bool splitViewKey(const std::string& key, int& id, std::string& field) {
    if (key.compare(0, 4, "view") != 0) {
        return false;
    }
    std::size_t i = 4;
    int value = 0;
    int digits = 0;
    while (i < key.size() && key[i] >= '0' && key[i] <= '9') {
        value = value * 10 + (key[i] - '0');
        ++digits;
        if (digits > 6) {
            return false;
        }
        ++i;
    }
    if (digits == 0 || value < 1 || i + 1 >= key.size() || key[i] != '.') {
        return false;
    }
    id = value;
    field = key.substr(i + 1);
    return true;
}

// Finds the `format` line. `found` false when there is none; `value` is its text.
void findFormatLine(const std::string& text, bool& found, std::string& value) {
    found = false;
    for (const std::string& rawLine : splitLines(text)) {
        const std::string line = trim(rawLine);
        if (line.empty() || line[0] == '#') {
            continue;
        }
        const std::size_t eq = line.find('=');
        if (eq != std::string::npos && trim(line.substr(0, eq)) == "format") {
            found = true;
            value = trim(line.substr(eq + 1));
            return;
        }
    }
}

}  // namespace

NameCheck checkSlotName(const std::string& raw, std::string& trimmed) {
    for (char ch : raw) {
        const unsigned char u = static_cast<unsigned char>(ch);
        if (u < 0x20 || u > 0x7E) {
            return NameCheck::NotPrintable;
        }
    }
    trimmed = trim(raw);
    if (trimmed.empty()) {
        return NameCheck::Empty;
    }
    if (trimmed.size() > kMaxSlotNameLength) {
        trimmed.clear();
        return NameCheck::TooLong;
    }
    return NameCheck::Ok;
}

LabelCheck checkLabel(const std::string& raw, std::string& stored) {
    const std::string t = trim(raw);
    if (t.size() > kMaxLabelLength) {
        return LabelCheck::TooLong;
    }
    std::string out;
    for (char ch : t) {
        const unsigned char c = static_cast<unsigned char>(ch);
        if (c >= 'a' && c <= 'z') {
            out.push_back(static_cast<char>(c - 'a' + 'A'));
        } else if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) {
            out.push_back(static_cast<char>(c));
        } else {
            return LabelCheck::BadCharacter;
        }
    }
    stored = out;
    return LabelCheck::Ok;
}

bool isValidKey(const CustomKey& key) {
    // 0x10 to 0x12 are the Windows codes for the Shift, Control and Alt keys alone: a modifier is not a key.
    return key.vkey >= 1 && key.vkey <= kMaxVirtualKey && key.vkey != kKeyEscape && key.vkey != kKeyBackspace &&
           (key.vkey < 0x10 || key.vkey > 0x12) && (key.flags & ~kKeyFlagMask) == 0;
}

std::string encodeKey(const CustomKey& key) {
    std::string flags;
    const auto add = [&flags](const char* word) {
        flags += flags.empty() ? "" : "+";
        flags += word;
    };
    if ((key.flags & kKeyShift) != 0) {
        add("shift");
    }
    if ((key.flags & kKeyCtrl) != 0) {
        add("ctrl");
    }
    if ((key.flags & kKeyAlt) != 0) {
        add("alt");
    }
    return "vk:" + std::to_string(key.vkey) + ",flags:" + (flags.empty() ? "none" : flags);
}

bool decodeKey(const std::string& text, CustomKey& key) {
    const std::string t = trim(text);
    const std::size_t comma = t.find(',');
    const std::string vkPart = trim(comma == std::string::npos ? t : t.substr(0, comma));
    if (vkPart.compare(0, 3, "vk:") != 0) {
        return false;
    }
    CustomKey result;
    if (!parseWholeNumber(trim(vkPart.substr(3)), kMaxVirtualKey, result.vkey)) {
        return false;
    }
    if (comma != std::string::npos) {
        const std::string flagPart = trim(t.substr(comma + 1));
        if (flagPart.compare(0, 6, "flags:") != 0) {
            return false;
        }
        const std::string words = lower(trim(flagPart.substr(6)));
        if (words.empty()) {
            return false;  // "flags:" with nothing after it; "flags:none" is the way to say no modifier
        }
        std::size_t start = 0;
        while (start <= words.size()) {
            const std::size_t plus = words.find('+', start);
            const std::string word = trim(words.substr(start, plus == std::string::npos ? std::string::npos : plus - start));
            if (word == "shift") {
                result.flags |= kKeyShift;
            } else if (word == "ctrl" || word == "control") {
                result.flags |= kKeyCtrl;
            } else if (word == "alt" || word == "option") {
                result.flags |= kKeyAlt;
            } else if (word != "none") {
                return false;
            }
            if (plus == std::string::npos) {
                break;
            }
            start = plus + 1;
        }
    }
    if (!isValidKey(result)) {
        return false;
    }
    key = result;
    return true;
}

KeyDiff diffKeys(const std::vector<WantedKey>& registered, const std::vector<WantedKey>& wanted) {
    KeyDiff diff;
    for (const WantedKey& r : registered) {
        if (std::find(wanted.begin(), wanted.end(), r) == wanted.end()) {
            diff.remove.push_back(r);
        }
    }
    for (const WantedKey& w : wanted) {
        if (std::find(registered.begin(), registered.end(), w) == registered.end()) {
            diff.add.push_back(w);
        }
    }
    return diff;
}

bool isValidPose(const Pose& p) {
    return fieldValid(0, p.x) && fieldValid(1, p.y) && fieldValid(2, p.z) && fieldValid(3, p.pitch) &&
           fieldValid(4, p.heading) && fieldValid(5, p.roll) && fieldValid(6, p.zoom);
}

// ------------------------------------------------------------------------------------ the store

void ViewStore::clear() {
    views_.clear();
    nextId_ = 1;
}

int ViewStore::indexOf(int id) const {
    for (std::size_t i = 0; i < views_.size(); ++i) {
        if (views_[i].id == id) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

const SavedView* ViewStore::find(int id) const {
    const int i = indexOf(id);
    return i < 0 ? nullptr : &views_[static_cast<std::size_t>(i)];
}

int ViewStore::count(ViewGroup group) const {
    int n = 0;
    for (const SavedView& v : views_) {
        if (groupOf(v.kind) == group) {
            ++n;
        }
    }
    return n;
}

int ViewStore::hotkeyOwner(int hotkey) const {
    if (hotkey < 1 || hotkey > kHotkeyCount) {
        return 0;
    }
    for (const SavedView& v : views_) {
        if (v.hotkey == hotkey) {
            return v.id;
        }
    }
    return 0;
}

std::string ViewStore::defaultName(ViewKind kind, int exceptId) const {
    const std::string prefix = kind == ViewKind::Cockpit ? "Cockpit view " : "Outside view ";
    for (int n = 1; n <= kMaxViews + 1; ++n) {
        const std::string candidate = prefix + std::to_string(n);
        const bool used = std::any_of(views_.begin(), views_.end(),
                                      [&](const SavedView& v) { return v.id != exceptId && v.name == candidate; });
        if (!used) {
            return candidate;
        }
    }
    return prefix + std::to_string(nextId_);  // not reachable with at most 99 views
}

int ViewStore::add(const Pose& pose, ViewKind kind, const std::string& viewCommand, int hotkey,
                   const std::string& name) {
    if (full() || nextId_ > kMaxViewId || !isValidPose(pose)) {
        return 0;
    }
    SavedView v;
    v.id = nextId_++;
    v.pose = pose;
    v.kind = kind;
    v.viewCommand = (kind == ViewKind::External && isValidViewCommandName(viewCommand)) ? viewCommand : std::string();
    v.name = name.empty() ? defaultName(kind) : name;
    v.order = nextOrder(groupOf(kind));
    if (hotkey >= 1 && hotkey <= kHotkeyCount) {
        const int owner = hotkeyOwner(hotkey);
        if (owner != 0) {
            views_[static_cast<std::size_t>(indexOf(owner))].hotkey = 0;
        }
        v.hotkey = hotkey;
    }
    views_.push_back(v);
    return v.id;
}

bool ViewStore::update(int id, const Pose& pose, ViewKind kind, const std::string& viewCommand) {
    const int i = indexOf(id);
    if (i < 0 || !isValidPose(pose)) {
        return false;
    }
    SavedView& v = views_[static_cast<std::size_t>(i)];
    const bool groupChanged = groupOf(v.kind) != groupOf(kind);
    v.pose = pose;
    v.kind = kind;
    v.viewCommand = (kind == ViewKind::External && isValidViewCommandName(viewCommand)) ? viewCommand : std::string();
    if (groupChanged) {
        v.order = 0;  // its old number means nothing in the new group
        v.order = nextOrder(groupOf(kind));  // the end of the new group
    }
    return true;
}

int ViewStore::nextOrder(ViewGroup group) const {
    int highest = 0;
    for (const SavedView& v : views_) {
        if (groupOf(v.kind) == group) {
            highest = std::max(highest, v.order);
        }
    }
    return highest + 1;
}

void ViewStore::renumber(ViewGroup group) {
    int n = 1;
    for (int id : ordered(group)) {
        views_[static_cast<std::size_t>(indexOf(id))].order = n++;
    }
}

void ViewStore::normalizeOrder() {
    for (ViewGroup group : {ViewGroup::Internal, ViewGroup::External}) {
        std::vector<SavedView*> missing;
        int highest = 0;
        for (SavedView& v : views_) {
            if (groupOf(v.kind) != group) {
                continue;
            }
            if (v.order > 0) {
                highest = std::max(highest, v.order);
            } else {
                missing.push_back(&v);
            }
        }
        std::stable_sort(missing.begin(), missing.end(), [](const SavedView* a, const SavedView* b) {
            return a->favourite != b->favourite ? a->favourite : a->id < b->id;
        });
        for (SavedView* v : missing) {
            v->order = ++highest;
        }
        renumber(group);
    }
}

bool ViewStore::setLabel(int id, const std::string& label) {
    const int i = indexOf(id);
    std::string stored;
    if (i < 0 || checkLabel(label, stored) != LabelCheck::Ok) {
        return false;
    }
    views_[static_cast<std::size_t>(i)].label = stored;
    return true;
}

int ViewStore::keyOwner(const CustomKey& key) const {
    if (!key.set()) {
        return 0;
    }
    for (const SavedView& v : views_) {
        if (v.key == key) {
            return v.id;
        }
    }
    return 0;
}

bool ViewStore::setKey(int id, const CustomKey& key) {
    const int i = indexOf(id);
    if (i < 0 || (key.set() && !isValidKey(key))) {
        return false;
    }
    if (key.set()) {
        const int owner = keyOwner(key);
        if (owner != 0 && owner != id) {
            views_[static_cast<std::size_t>(indexOf(owner))].key = CustomKey();
        }
        views_[static_cast<std::size_t>(i)].key = key;
    } else {
        views_[static_cast<std::size_t>(i)].key = CustomKey();
    }
    return true;
}

bool ViewStore::moveTo(int id, int targetId) {
    const SavedView* a = find(id);
    const SavedView* b = find(targetId);
    if (a == nullptr || b == nullptr || id == targetId || groupOf(a->kind) != groupOf(b->kind)) {
        return false;
    }
    const ViewGroup group = groupOf(a->kind);
    std::vector<int> ids = ordered(group);
    const auto from = std::find(ids.begin(), ids.end(), id);
    const auto to = std::find(ids.begin(), ids.end(), targetId);
    const std::ptrdiff_t toIndex = to - ids.begin();
    ids.erase(from);
    ids.insert(ids.begin() + toIndex, id);
    int n = 1;
    for (int each : ids) {
        views_[static_cast<std::size_t>(indexOf(each))].order = n++;
    }
    return true;
}

int ViewStore::neighbour(int id, int direction, bool favouritesOnly) const {
    const SavedView* v = find(id);
    if (v == nullptr || direction == 0) {
        return 0;
    }
    const std::vector<int> ids = ordered(groupOf(v->kind));
    const int step = direction < 0 ? -1 : 1;
    int pos = -1;
    for (std::size_t i = 0; i < ids.size(); ++i) {
        if (ids[i] == id) {
            pos = static_cast<int>(i);
        }
    }
    for (int i = pos + step; pos >= 0 && i >= 0 && i < static_cast<int>(ids.size()); i += step) {
        const SavedView* other = find(ids[static_cast<std::size_t>(i)]);
        if (!favouritesOnly || other->favourite) {
            return other->id;
        }
    }
    return 0;
}

bool ViewStore::setName(int id, const std::string& name) {
    const int i = indexOf(id);
    std::string clean;
    if (i < 0 || checkSlotName(name, clean) != NameCheck::Ok || clean != name) {
        return false;  // callers pass the trimmed result of checkSlotName
    }
    views_[static_cast<std::size_t>(i)].name = clean;
    return true;
}

bool ViewStore::setFavourite(int id, bool favourite) {
    const int i = indexOf(id);
    if (i < 0) {
        return false;
    }
    views_[static_cast<std::size_t>(i)].favourite = favourite;
    return true;
}

bool ViewStore::setHotkey(int id, int hotkey) {
    const int i = indexOf(id);
    if (i < 0 || hotkey < 0 || hotkey > kHotkeyCount) {
        return false;
    }
    if (hotkey != 0) {
        const int owner = hotkeyOwner(hotkey);
        if (owner != 0 && owner != id) {
            views_[static_cast<std::size_t>(indexOf(owner))].hotkey = 0;
        }
    }
    views_[static_cast<std::size_t>(i)].hotkey = hotkey;
    return true;
}

bool ViewStore::remove(int id) {
    const int i = indexOf(id);
    if (i < 0) {
        return false;
    }
    views_.erase(views_.begin() + i);
    return true;
}

std::vector<int> ViewStore::ordered(ViewGroup group) const {
    std::vector<const SavedView*> inGroup;
    for (const SavedView& v : views_) {
        if (groupOf(v.kind) == group) {
            inGroup.push_back(&v);
        }
    }
    std::stable_sort(inGroup.begin(), inGroup.end(), [](const SavedView* a, const SavedView* b) {
        return a->order != b->order ? a->order < b->order : a->id < b->id;
    });
    std::vector<int> ids;
    ids.reserve(inGroup.size());
    for (const SavedView* v : inGroup) {
        ids.push_back(v->id);
    }
    return ids;
}

std::vector<int> ViewStore::favourites(ViewGroup group) const {
    std::vector<int> ids;
    for (int id : ordered(group)) {
        if (find(id)->favourite) {
            ids.push_back(id);
        }
    }
    return ids;
}

std::vector<WantedKey> ViewStore::wantedKeys() const {
    std::vector<WantedKey> keys;
    for (const SavedView& v : views_) {
        if (v.key.set()) {
            WantedKey w;
            w.key = v.key;
            w.viewId = v.id;
            w.description = "MyKam: " + v.name;
            keys.push_back(w);
        }
    }
    return keys;
}

std::vector<int> ViewStore::allOrdered() const {
    std::vector<int> ids = ordered(ViewGroup::Internal);
    const std::vector<int> external = ordered(ViewGroup::External);
    ids.insert(ids.end(), external.begin(), external.end());
    return ids;
}

bool ViewStore::has(int hotkey) const {
    return hotkeyOwner(hotkey) != 0;
}

const Pose* ViewStore::get(int hotkey) const {
    const SavedView* v = find(hotkeyOwner(hotkey));
    return v == nullptr ? nullptr : &v->pose;
}

ViewKind ViewStore::kind(int hotkey) const {
    const SavedView* v = find(hotkeyOwner(hotkey));
    return v == nullptr ? ViewKind::External : v->kind;
}

const std::string& ViewStore::viewCommand(int hotkey) const {
    static const std::string none;
    const SavedView* v = find(hotkeyOwner(hotkey));
    return v == nullptr ? none : v->viewCommand;
}

std::string ViewStore::serialize() const {
    std::string out;
    out += "format=" + std::to_string(kFormatVersion) + "\n";
    out += "next_id=" + std::to_string(nextId_) + "\n";
    for (const SavedView& v : views_) {
        const std::string prefix = "view" + std::to_string(v.id) + ".";
        Pose p = v.pose;
        for (int f = 0; f < kFieldCount; ++f) {
            out += prefix + kFieldNames[f] + "=" + formatCompact(*fieldRef(p, f), kStoredDecimals) + "\n";
        }
        out += prefix + "kind=" + (v.kind == ViewKind::Cockpit ? "cockpit" : "external") + "\n";
        out += prefix + "name=" + v.name + "\n";
        out += prefix + "order=" + std::to_string(v.order) + "\n";
        if (!v.viewCommand.empty()) {
            out += prefix + "view=" + v.viewCommand + "\n";
        }
        if (v.favourite) {
            out += prefix + "favourite=1\n";
        }
        if (v.hotkey != 0) {
            out += prefix + "hotkey=" + hotkeyText(v.hotkey) + "\n";
        }
        if (!v.label.empty()) {
            out += prefix + "label=" + v.label + "\n";
        }
        if (v.key.set()) {
            out += prefix + "key=" + encodeKey(v.key) + "\n";
        }
    }
    return out;
}

ViewParseResult ViewStore::parse(const std::string& text) {
    ViewParseResult result;
    clear();
    if (text.size() > kMaxSlotFileBytes) {
        result.problems.push_back("file is larger than 64 KB and was ignored");
        return result;
    }

    bool formatFound = false;
    std::string formatValue;
    findFormatLine(text, formatFound, formatValue);
    if (formatFound) {
        double v = 0.0;
        if (!parseDouble(formatValue, v) || v != static_cast<double>(kFormatVersion)) {
            result.problems.push_back("unknown file format version '" + shorten(formatValue) + "', file ignored");
            return result;
        }
    }

    std::map<int, Candidate> candidates;  // view id (new format) or slot number (old format) -> fields
    int fileNextId = 0;
    int lineNumber = 0;

    for (const std::string& rawLine : splitLines(text)) {
        ++lineNumber;
        const std::string line = trim(rawLine);
        if (line.empty() || line[0] == '#') {
            continue;
        }
        const std::size_t eq = line.find('=');
        if (eq == std::string::npos) {
            result.problems.push_back("line " + std::to_string(lineNumber) + " has no '='");
            continue;
        }
        const std::string key = trim(line.substr(0, eq));
        const std::string value = trim(line.substr(eq + 1));

        if (formatFound) {
            if (key == "format") {
                continue;
            }
            if (key == "next_id") {
                double v = 0.0;
                if (parseDouble(value, v) && v >= 1.0 && v <= static_cast<double>(kMaxViewId) + 1.0 && v == std::floor(v)) {
                    fileNextId = static_cast<int>(v);
                } else {
                    result.problems.push_back("next_id '" + shorten(value) + "' ignored, it must be a whole number of 1 or more");
                }
                continue;
            }
            int id = 0;
            std::string field;
            if (splitViewKey(key, id, field)) {
                Candidate& c = candidates[id];
                const std::string label = "view " + std::to_string(id);
                if (readCommonField(c, field, value, label, result)) {
                    continue;
                }
                if (field == "hotkey") {
                    // The file stores the key's digit: 1..9, and 0 for numpad 0 (kHotkeyZero).
                    if (value == "0") {
                        c.hotkey = kHotkeyZero;
                    } else if (value.size() == 1 && value[0] >= '1' && value[0] <= '9') {
                        c.hotkey = value[0] - '0';
                    } else {
                        c.hotkey = 0;
                        result.problems.push_back(label + ": hotkey '" + shorten(value) + "' ignored, it must be 0 to 9");
                    }
                    continue;
                }
                if (field == "label") {
                    std::string stored;
                    if (checkLabel(value, stored) == LabelCheck::Ok) {
                        c.label = stored;
                    } else {
                        c.label.clear();
                        result.problems.push_back(label + ": label '" + shorten(value) +
                                                  "' ignored, it must be 1 or 2 letters or digits");
                    }
                    continue;
                }
                if (field == "key") {
                    if (!decodeKey(value, c.key)) {
                        c.key = CustomKey();
                        result.problems.push_back(label + ": key '" + shorten(value) +
                                                  "' ignored, it must look like vk:87,flags:shift+ctrl");
                    }
                    continue;
                }
                if (field == "order") {
                    int n = 0;
                    if (parseWholeNumber(value, kMaxOrderValue, n) && n >= 1) {
                        c.order = n;
                    } else {
                        c.order = 0;
                        result.problems.push_back(label + ": order '" + shorten(value) +
                                                  "' ignored, it must be a whole number of 1 or more");
                    }
                    continue;
                }
            }
        } else {
            // Legacy per-slot format: mykam_slots=1 header and slot<N>.<field> lines, N 1 to 9.
            if (key == "mykam_slots") {
                double v = 0.0;
                if (!parseDouble(value, v) || static_cast<int>(v) != 1) {
                    result.problems.push_back("unknown file format version '" + shorten(value) + "', file ignored");
                    clear();
                    return result;
                }
                continue;
            }
            if (key.size() > 6 && key.compare(0, 4, "slot") == 0 && key[4] >= '1' && key[4] <= '9' && key[5] == '.') {
                const int slot = key[4] - '0';
                Candidate& c = candidates[slot];
                if (readCommonField(c, key.substr(6), value, "slot " + std::to_string(slot), result)) {
                    continue;
                }
            }
        }
        result.problems.push_back("line " + std::to_string(lineNumber) + ": unknown entry '" + shorten(key) + "'");
    }

    int highestSeen = 0;
    for (const auto& entry : candidates) {
        const int id = entry.first;
        const Candidate& c = entry.second;
        highestSeen = std::max(highestSeen, id);
        const std::string label = (formatFound ? "view " : "slot ") + std::to_string(id);
        if (!candidateUsable(c, label, result)) {
            continue;
        }
        if (count() >= kMaxViews) {
            result.problems.push_back(label + " skipped: a file holds at most " + std::to_string(kMaxViews) + " views");
            continue;
        }
        SavedView v;
        v.id = id;
        v.pose = c.pose;
        v.kind = c.kind;
        v.viewCommand = c.kind == ViewKind::External ? c.view : std::string();
        v.favourite = c.favourite;
        if (formatFound) {
            v.hotkey = c.hotkey;
            if (v.hotkey != 0 && hotkeyOwner(v.hotkey) != 0) {
                result.problems.push_back(label + ": hotkey " + hotkeyText(v.hotkey) + " is already used by view " +
                                          std::to_string(hotkeyOwner(v.hotkey)) + ", ignored");
                v.hotkey = 0;
            }
        } else {
            v.hotkey = id;  // an old slot N becomes a view with hotkey N
        }
        v.name = c.name.empty() ? defaultName(c.kind) : c.name;
        if (formatFound) {
            v.label = c.label;
            v.order = c.order;
            if (c.key.set()) {
                if (keyOwner(c.key) != 0) {
                    result.problems.push_back(label + ": key " + encodeKey(c.key) + " is already used by view " +
                                              std::to_string(keyOwner(c.key)) + ", ignored");
                } else {
                    v.key = c.key;
                }
            }
        }
        views_.push_back(v);
        ++result.loaded;
    }
    normalizeOrder();
    nextId_ = std::max(highestSeen + 1, fileNextId);
    if (nextId_ > kMaxViewId + 1) {
        nextId_ = kMaxViewId + 1;
    }
    result.migrated = !formatFound && result.loaded > 0;
    return result;
}

int stepView(const ViewStore& store, int currentId, int direction) {
    const std::vector<int> ids = store.allOrdered();
    if (ids.empty()) {
        return 0;
    }
    const int step = direction < 0 ? -1 : 1;
    const int size = static_cast<int>(ids.size());
    int position = -1;
    for (int i = 0; i < size; ++i) {
        if (ids[static_cast<std::size_t>(i)] == currentId) {
            position = i;
            break;
        }
    }
    if (position < 0) {
        return step > 0 ? ids.front() : ids.back();
    }
    return ids[static_cast<std::size_t>((position + step + size) % size)];
}

const char* viewTypeLabel(const SavedView& view) {
    if (view.kind == ViewKind::Cockpit) {
        return "Cockpit";
    }
    return view.viewCommand == "sim/view/chase" ? "Chase" : "Free camera";
}

std::string sanitizeAircraftName(const std::string& aircraftFileName) {
    std::string name = aircraftFileName;
    // Drop a trailing ".acf" (any case).
    if (name.size() >= 4 && upper(name.substr(name.size() - 4)) == ".ACF") {
        name.erase(name.size() - 4);
    }
    std::string stem;
    stem.reserve(name.size());
    for (char ch : name) {
        const unsigned char c = static_cast<unsigned char>(ch);
        const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' ||
                        c == '_' || c == '.';
        stem.push_back(ok ? static_cast<char>(c) : '_');
    }
    // No leading dots: no hidden files, no "." or "..".
    for (char& c : stem) {
        if (c != '.') {
            break;
        }
        c = '_';
    }
    if (stem.size() > kMaxStemLength) {
        stem.erase(kMaxStemLength);
    }
    if (stem.empty()) {
        stem = "unnamed";
    }
    if (isReservedDeviceName(stem)) {
        stem = "_" + stem;
    }
    return stem;
}

std::string slotFileName(const std::string& aircraftFileName) {
    return sanitizeAircraftName(aircraftFileName) + ".slots.txt";
}

}  // namespace mykam
