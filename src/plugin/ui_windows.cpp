#include "ui_windows.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

#include "core/bar_layout.h"
#include "core/file_io.h"
#include "core/version.h"
#include "core/view_commands.h"
#include "ui_theme.h"
#include "ui_widgets.h"
#include "xplm_includes.h"

namespace mykam {

namespace {

namespace w = widgets;
using theme::fonts;

const char* const kDigits[kHotkeyCount] = {"1", "2", "3", "4", "5", "6", "7", "8", "9", "0"};  // index hotkey - 1
const char* const kDragPayload = "MYKAM_VIEW";  // the id of the view being dragged inside its list or the bar
const char* const kDot = "\xC2\xB7";  // U+00B7 middle dot

constexpr float kPopupWidth = 312.0f;
constexpr float kPopupHeight = 250.0f;
constexpr float kViewRowHeight = 52.0f;
constexpr float kSaveRowHeight = 44.0f;  // the "+ Save current view" button and its gap
constexpr float kGap = 12.0f;               // gap between cards and tiles
constexpr float kTitleBlock = 64.0f;        // page title + subtitle + space
constexpr float kAreaPad = 8.0f;            // padding inside scroll areas, room for glows
constexpr float kScrollbar = 8.0f;
constexpr float kRowHeight = 64.0f;
constexpr int kMinWidth = 660;
constexpr int kMinHeight = 460;
constexpr int kMaxWidth = 1800;
constexpr int kMaxHeight = 1400;

const theme::Icon& typeIcon(RowType type) {
    switch (type) {
        case RowType::Cockpit: return theme::kIconCockpit;
        case RowType::Chase: return theme::kIconChase;
        case RowType::FreeCamera:
        default: return theme::kIconFree;
    }
}

// One tile per kXPlaneViewButtons entry, in the same order.
const theme::Icon* const kTileIcons[] = {
    &theme::kIconCockpit, &theme::kIconFree,   &theme::kIconChase, &theme::kIconCircle, &theme::kIconTower,
    &theme::kIconRunway,  &theme::kIconLinear, &theme::kIconStill, &theme::kIconRide,   &theme::kIconWingman,
};
static_assert(sizeof(kTileIcons) / sizeof(kTileIcons[0]) == kXPlaneViewButtonCount, "one icon per X-Plane view");

// Names may only hold printable ASCII, whatever the user types or pastes.
int nameFilter(ImGuiInputTextCallbackData* data) {
    const unsigned int c = static_cast<unsigned int>(data->EventChar);
    return (c >= 0x20 && c < 0x7F) ? 0 : 1;
}

// Quick bar labels: letters (shown uppercase) and digits only.
int labelFilter(ImGuiInputTextCallbackData* data) {
    const unsigned int c = static_cast<unsigned int>(data->EventChar);
    if (c >= 'a' && c <= 'z') {
        data->EventChar = static_cast<ImWchar>(c - 'a' + 'A');
        return 0;
    }
    return ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) ? 0 : 1;
}

// A round badge holding a 1 to 2 character label, centred on `centre`.
void drawLabelBadge(ImDrawList* list, ImVec2 centre, float radius, const std::string& text, ImU32 fill,
                    ImU32 textColour) {
    const theme::Fonts& f = fonts();
    list->AddCircleFilled(centre, radius, w::col(fill), 20);
    const ImVec2 size = w::textSize(f.small, text.c_str());
    w::drawText(list, f.small, ImVec2(centre.x - size.x * 0.5f, centre.y - size.y * 0.5f), textColour, text.c_str());
}

float clampLow(float value, float low, float high) {
    // Like clamp, but when the range is empty the low end wins.
    return std::max(low, std::min(value, high));
}

ImVec2 offset(ImVec2 p, float dx, float dy) {
    return ImVec2(p.x + dx, p.y + dy);
}

// A scrollable, transparent child at `pos` with a little padding all round (the padding keeps glows
// from being cut off). The caller must call ImGui::EndChild() whatever this returns.
bool beginArea(const char* id, ImVec2 pos, ImVec2 size) {
    ImGui::SetCursorScreenPos(offset(pos, -kAreaPad, -6.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(kAreaPad, 6.0f));
    const bool open = ImGui::BeginChild(id, ImVec2(size.x + 2.0f * kAreaPad, size.y + 6.0f),
                                        ImGuiChildFlags_AlwaysUseWindowPadding, ImGuiWindowFlags_NoBackground);
    ImGui::PopStyleVar();
    return open;
}

// Wrapped muted text drawn at `pos`; returns its height.
float wrappedText(ImDrawList* list, ImFont* font, ImVec2 pos, float width, ImU32 colour, const char* text) {
    const float px = w::fontPixels(font);
    list->AddText(font, px, pos, w::col(colour), text, nullptr, width);
    return font->CalcTextSizeA(px, FLT_MAX, width, text).y;
}

}  // namespace

// ------------------------------------------------------------------------------------ main window

std::string MainWindow::statusText() const {
    if (model_.activeView() != 0 && !model_.activeName().empty()) {
        return model_.activeName() + " " + kDot + " " + model_.activeTypeLabel();
    }
    return "X-Plane in control";
}

const ViewRow* MainWindow::findRow(int viewId) const {
    for (ViewGroup group : {ViewGroup::Internal, ViewGroup::External}) {
        for (const ViewRow& row : model_.rows(group)) {
            if (row.id == viewId) {
                return &row;
            }
        }
    }
    return nullptr;
}

void MainWindow::setMinimised(bool minimised) {
    if (minimised == minimised_) {
        return;
    }
    if (minimised) {
        restoreHeight_ = height();
        minimised_ = true;
        const int h = static_cast<int>(theme::kHeaderHeight + 2.0f * theme::kShadowMargin);
        setResizingLimits(kMinWidth, h, kMaxWidth, h);
        setHeight(h);
    } else {
        minimised_ = false;
        setResizingLimits(kMinWidth, kMinHeight, kMaxWidth, kMaxHeight);
        setHeight(restoreHeight_ > 0 ? restoreHeight_ : 700);
    }
}

void MainWindow::styleContext() {
    // The edit dialog dims the window itself; the screen behind the window must stay as it is.
    ImGui::GetStyle().Colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
}

void MainWindow::openEditDialog(int viewId) {
    editRequest_ = viewId;
}

void MainWindow::closeEditDialog() {
    if (editCapturing_) {
        endKeyCapture();
        editCapturing_ = false;
    }
    editView_ = 0;
    editOpenNow_ = false;
    editLoggedOpen_ = false;
}

void MainWindow::build() {
    model_.refresh();
    if (minimised_ && poppedOut()) {
        setMinimised(false);  // an OS window has no use for a collapsed header
    }
    if (editRequest_ != 0) {
        const EditDraft draft = model_.beginEdit(editRequest_);
        editRequest_ = 0;
        if (draft.viewId != 0) {
            closeEditDialog();
            setMinimised(false);  // the dialog needs the full window
            draft_ = draft;
            editView_ = draft.viewId;
            std::snprintf(nameBuffer_, sizeof(nameBuffer_), "%s", draft.name.c_str());
            std::snprintf(labelBuffer_, sizeof(labelBuffer_), "%s", draft.label.c_str());
            editKeyText_ = model_.customKeyText(draft.custom);
            editNote_.clear();
            editOpenNow_ = true;
            editFocusName_ = true;
        }
    }
    const ImVec2 hostPos = ImGui::GetWindowPos();
    const ImVec2 hostSize = ImGui::GetWindowSize();
    windowX_ = hostPos.x;
    windowY_ = hostPos.y;
    windowW_ = hostSize.x;
    windowH_ = hostSize.y;
    const float margin = poppedOut() ? 0.0f : theme::kShadowMargin;
    const ImVec2 pmin(hostPos.x + margin, hostPos.y + margin);
    const ImVec2 pmax(hostPos.x + hostSize.x - margin, hostPos.y + hostSize.y - margin);
    drawBackground(pmin, pmax);
    drawHeader(pmin, pmax);
    if (!minimised_) {
        drawSidebar(pmin, pmax);
        drawContent(pmin, pmax);
        drawEditDialog();
    }
}

void MainWindow::drawBackground(ImVec2 pmin, ImVec2 pmax) {
    ImDrawList* list = ImGui::GetWindowDrawList();
    const float rounding = poppedOut() ? 0.0f : theme::kWindowRounding;
    if (!poppedOut()) {
        w::shadowRoundedRect(list, pmin, pmax, rounding, theme::kShadowMargin - 2.0f);
    }
    list->AddRectFilled(pmin, pmax, w::col(theme::kGlass), rounding);
    list->AddRect(offset(pmin, 0.5f, 0.5f), offset(pmax, -0.5f, -0.5f), w::col(theme::kBorder), rounding, 0, 1.0f);
}

void MainWindow::drawHeader(ImVec2 pmin, ImVec2 pmax) {
    const theme::Fonts& f = fonts();
    ImDrawList* list = ImGui::GetWindowDrawList();
    const float h = theme::kHeaderHeight;
    const bool popped = poppedOut();
    constexpr float kButton = 30.0f;
    constexpr float kButtonGap = 4.0f;
    const int buttons = popped ? 2 : 3;
    const float buttonsWidth = static_cast<float>(buttons) * kButton + static_cast<float>(buttons - 1) * kButtonGap;
    const float rightX = pmax.x - 14.0f - buttonsWidth;

    // The drag area: the whole header left of the buttons.
    ImGui::SetCursorScreenPos(pmin);
    ImGui::InvisibleButton("##drag", ImVec2(std::max(rightX - pmin.x - 6.0f, 1.0f), h));
    dragWindow(ImGui::IsItemActive());

    const std::string logo = theme::iconText(theme::kIconCamera);
    const ImVec2 logoSize = w::textSize(f.iconLarge, logo.c_str());
    w::drawText(list, f.iconLarge, ImVec2(pmin.x + 18.0f, pmin.y + (h - logoSize.y) * 0.5f), theme::kCyan,
                logo.c_str());
    const float titleX = pmin.x + 18.0f + logoSize.x + 10.0f;
    const ImVec2 titleSize = w::textSize(f.card, "MyKam");
    w::drawText(list, f.card, ImVec2(titleX, pmin.y + (h - titleSize.y) * 0.5f), theme::kText, "MyKam");

    // Status pill: a green dot when X-Plane is in control, cyan when a saved camera is.
    const bool slotActive = model_.activeView() != 0 && statusText() != "X-Plane in control";
    const float pillX = titleX + titleSize.x + 16.0f;
    const float pillMax = rightX - 10.0f;
    if (pillMax - pillX > 60.0f) {
        const std::string text = w::ellipsize(f.body, statusText(), pillMax - pillX - 40.0f);
        const ImVec2 textSize = w::textSize(f.body, text.c_str());
        const float pillH = 26.0f;
        const float pillW = textSize.x + 36.0f;
        const float pillY = pmin.y + (h - pillH) * 0.5f;
        const ImU32 dot = slotActive ? theme::kCyan : theme::kGreen;
        list->AddRectFilled(ImVec2(pillX, pillY), ImVec2(pillX + pillW, pillY + pillH),
                            w::col(w::withAlpha(dot, 0.12f)), pillH * 0.5f);
        list->AddCircleFilled(ImVec2(pillX + 14.0f, pillY + pillH * 0.5f), 4.0f, w::col(dot), 16);
        w::drawText(list, f.body, ImVec2(pillX + 26.0f, pillY + (pillH - textSize.y) * 0.5f), theme::kText,
                    text.c_str());
    }

    const float by = pmin.y + (h - kButton) * 0.5f;
    float bx = rightX;
    ImGui::SetCursorScreenPos(ImVec2(bx, by));
    if (w::iconButton("##popout", theme::kIconPopOut, kButton, w::IconStyle::Ghost)) {
        requestPopOut(!popped);
    }
    w::tooltipText(popped ? "Bring the window back into X-Plane" : "Pop out into its own window");
    bx += kButton + kButtonGap;
    if (!popped) {
        ImGui::SetCursorScreenPos(ImVec2(bx, by));
        if (w::iconButton("##minimise", minimised_ ? theme::kIconRestore : theme::kIconMinimise, kButton,
                          w::IconStyle::Ghost)) {
            setMinimised(!minimised_);
        }
        w::tooltipText(minimised_ ? "Show the window" : "Minimise to the header");
        bx += kButton + kButtonGap;
    }
    ImGui::SetCursorScreenPos(ImVec2(bx, by));
    if (w::iconButton("##close", theme::kIconClose, kButton, w::IconStyle::Danger)) {
        setVisible(false);
    }
    w::tooltipText("Close (Plugins > MyKam > Show window opens it again)");

    if (!minimised_) {
        list->AddLine(ImVec2(pmin.x + 14.0f, pmin.y + h), ImVec2(pmax.x - 14.0f, pmin.y + h), w::col(theme::kHairline));
    }
}

void MainWindow::drawSidebar(ImVec2 pmin, ImVec2 pmax) {
    const theme::Fonts& f = fonts();
    ImDrawList* list = ImGui::GetWindowDrawList();
    const float top = pmin.y + theme::kHeaderHeight;
    const float rounding = poppedOut() ? 0.0f : theme::kWindowRounding;
    list->AddRectFilled(ImVec2(pmin.x, top + 1.0f), ImVec2(pmin.x + theme::kSidebarWidth, pmax.y), w::col(theme::kSidebar),
                        rounding, ImDrawFlags_RoundCornersBottomLeft);
    list->AddLine(ImVec2(pmin.x + theme::kSidebarWidth, top + 1.0f), ImVec2(pmin.x + theme::kSidebarWidth, pmax.y),
                  w::col(theme::kHairline));

    struct Item {
        const theme::Icon* icon;
        const char* label;
        Page page;
        int badge;            // the number of views in a group, -1 for none
        bool dividerBefore;
    };
    const Item items[] = {
        {&theme::kIconCockpit, "Internal", Page::Internal, model_.rowCount(ViewGroup::Internal), false},
        {&theme::kIconFree, "External", Page::External, model_.rowCount(ViewGroup::External), false},
        {&theme::kIconEye, "X-Plane views", Page::XPlaneViews, -1, true},
        {&theme::kIconSettings, "Settings", Page::Settings, -1, false},
    };
    float y = top + 16.0f;
    for (const Item& item : items) {
        if (item.dividerBefore) {
            list->AddLine(ImVec2(pmin.x + 20.0f, y + 2.0f), ImVec2(pmin.x + theme::kSidebarWidth - 20.0f, y + 2.0f),
                          w::col(theme::kHairline));
            y += 10.0f;
        }
        const ImVec2 a(pmin.x + 10.0f, y);
        const ImVec2 size(theme::kSidebarWidth - 20.0f, theme::kNavItemHeight);
        ImGui::SetCursorScreenPos(a);
        if (ImGui::InvisibleButton(item.label, size)) {
            page_ = item.page;
        }
        const bool hovered = ImGui::IsItemHovered();
        const bool selected = page_ == item.page;
        const ImVec2 b(a.x + size.x, a.y + size.y);
        if (selected) {
            list->AddRectFilled(a, b, w::col(theme::kNavSelected), 10.0f);
            list->AddRectFilled(ImVec2(pmin.x, y + 9.0f), ImVec2(pmin.x + 4.0f, y + size.y - 9.0f), w::col(theme::kCyan),
                                2.0f, ImDrawFlags_RoundCornersRight);
        } else if (hovered) {
            list->AddRectFilled(a, b, w::col(theme::kHoverWash), 10.0f);
        }
        const std::string glyph = theme::iconText(*item.icon);
        const ImVec2 glyphSize = w::textSize(f.iconMedium, glyph.c_str());
        const ImVec2 labelSize = w::textSize(f.body, item.label);
        w::drawText(list, f.iconMedium, ImVec2(a.x + 14.0f, a.y + (size.y - glyphSize.y) * 0.5f),
                    selected ? theme::kCyan : (hovered ? theme::kText : theme::kMuted), glyph.c_str());
        w::drawText(list, f.body, ImVec2(a.x + 44.0f, a.y + (size.y - labelSize.y) * 0.5f),
                    selected || hovered ? theme::kText : theme::kMuted, item.label);
        if (item.badge >= 0) {
            const std::string count = std::to_string(item.badge);
            const float badgeWidth = std::max(w::textChipWidth(count.c_str()), 26.0f);
            const ImVec2 countSize = w::textSize(f.small, count.c_str());
            const ImVec2 badgePos(b.x - 10.0f - badgeWidth, a.y + (size.y - 22.0f) * 0.5f);
            list->AddRectFilled(badgePos, ImVec2(badgePos.x + badgeWidth, badgePos.y + 22.0f),
                                w::col(selected ? w::withAlpha(theme::kCyan, 0.22f) : theme::kField), 11.0f);
            w::drawText(list, f.small,
                        ImVec2(badgePos.x + (badgeWidth - countSize.x) * 0.5f, badgePos.y + (22.0f - countSize.y) * 0.5f),
                        selected ? theme::kCyan : theme::kMuted, count.c_str());
        }
        y += theme::kNavItemHeight + 4.0f;
    }

    // Divider and the version at the bottom.
    list->AddLine(ImVec2(pmin.x + 20.0f, pmax.y - 46.0f), ImVec2(pmin.x + theme::kSidebarWidth - 20.0f, pmax.y - 46.0f),
                  w::col(theme::kHairline));
    const std::string version = std::string("MyKam v") + kVersion;
    const ImVec2 versionSize = w::textSize(f.small, version.c_str());
    w::drawText(list, f.small, ImVec2(pmin.x + 22.0f, pmax.y - 24.0f - versionSize.y * 0.5f), theme::kDim,
                version.c_str());
}

void MainWindow::drawPageTitle(ImVec2 origin, const char* title, const char* subtitle) {
    const theme::Fonts& f = fonts();
    ImDrawList* list = ImGui::GetWindowDrawList();
    w::drawText(list, f.section, origin, theme::kText, title);
    const float titleHeight = w::textSize(f.section, title).y;
    const float maxWidth = windowW_ - (origin.x - windowX_) - 24.0f;
    const std::string text = w::ellipsize(f.body, subtitle, maxWidth);
    w::drawText(list, f.body, ImVec2(origin.x, origin.y + titleHeight + 4.0f), theme::kMuted, text.c_str());
}

void MainWindow::drawContent(ImVec2 pmin, ImVec2 pmax) {
    const float left = pmin.x + theme::kSidebarWidth + theme::kContentPad;
    const float top = pmin.y + theme::kHeaderHeight + 20.0f;
    const float width = std::max(pmax.x - theme::kContentPad - left, 100.0f);
    const float height = std::max(pmax.y - 16.0f - top - kTitleBlock, 60.0f);
    const ImVec2 titleAt(left, top);
    const ImVec2 bodyAt(left, top + kTitleBlock);
    switch (page_) {
        case Page::Internal:
            drawPageTitle(titleAt, "Internal",
                          "Cockpit views. Click to go, right-click for more, drag to reorder. A star puts a view in the quick bar.");
            drawGroupPage(ViewGroup::Internal, bodyAt, width, height);
            break;
        case Page::External:
            drawPageTitle(titleAt, "External",
                          "Free camera and chase views. Click to go, right-click for more, drag to reorder. A star puts a view in the quick bar.");
            drawGroupPage(ViewGroup::External, bodyAt, width, height);
            break;
        case Page::XPlaneViews:
            drawPageTitle(titleAt, "X-Plane views",
                          "Switch to one of X-Plane's own views. This releases any camera MyKam holds.");
            if (beginArea("##xpviews", bodyAt, ImVec2(width, height))) {
                drawXPlaneViewsPage(width);
            }
            ImGui::EndChild();
            break;
        case Page::Settings:
            drawPageTitle(titleAt, "Settings", "Changes are saved to mykam.ini as soon as you make them.");
            if (beginArea("##settings", bodyAt, ImVec2(width, height))) {
                drawSettingsPage(width);
            }
            ImGui::EndChild();
            break;
    }
}

void MainWindow::drawGroupPage(ViewGroup group, ImVec2 origin, float width, float height) {
    const theme::Fonts& f = fonts();
    ImDrawList* list = ImGui::GetWindowDrawList();
    const bool internal = group == ViewGroup::Internal;

    // The save button: enabled only when the view X-Plane shows now fits this group.
    const bool canSave = model_.canSaveCurrent(group);
    ImGui::SetCursorScreenPos(origin);
    ImGui::BeginDisabled(!canSave);
    if (w::pillButton("##savecurrent", "+ Save current view", nullptr, ImVec2(0.0f, 34.0f), w::PillStyle::Filled)) {
        model_.saveCurrent(group);
    }
    ImGui::EndDisabled();
    if (!canSave && ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip | ImGuiHoveredFlags_AllowWhenDisabled) &&
        w::beginGlassTooltip()) {
        ImGui::TextUnformatted(UiModel::saveDisabledReason(group));
        w::endGlassTooltip();
    }

    const std::vector<ViewRow>& rows = model_.rows(group);
    const float listTop = origin.y + kSaveRowHeight;
    const float listHeight = std::max(height - kSaveRowHeight - theme::kFooterHeight, 60.0f);
    if (beginArea(internal ? "##internal" : "##external", ImVec2(origin.x, listTop), ImVec2(width, listHeight))) {
        const ImVec2 start = ImGui::GetCursorScreenPos();
        const float usable = width - kScrollbar;
        if (rows.empty()) {
            const char* heading = internal ? "No internal views yet" : "No external views yet";
            const char* hint = internal ? "Switch to a cockpit view in X-Plane, look the way you like, then press "
                                          "+ Save current view. A key for the view is optional."
                                        : "Switch to the free camera or the chase view in X-Plane, move to the spot "
                                          "you like, then press + Save current view. A key for the view is optional.";
            w::drawText(list, f.card, ImVec2(start.x + 4.0f, start.y + 16.0f), theme::kText, heading);
            const float hintHeight =
                wrappedText(list, f.body, ImVec2(start.x + 4.0f, start.y + 44.0f), usable - 8.0f, theme::kMuted, hint);
            ImGui::SetCursorScreenPos(ImVec2(start.x, start.y + 44.0f + hintHeight + 8.0f));
        } else {
            float y = start.y;
            for (const ViewRow& row : rows) {
                ImGui::SetCursorScreenPos(ImVec2(start.x, y));
                drawViewRow(row, usable);
                y += kViewRowHeight + 8.0f;
            }
            ImGui::SetCursorScreenPos(ImVec2(start.x, y));
        }
        ImGui::Dummy(ImVec2(1.0f, 4.0f));
    }
    ImGui::EndChild();

    // Footer: previous, next, release and the transition time.
    const float footerY = listTop + listHeight + 6.0f;
    list->AddLine(ImVec2(origin.x, footerY), ImVec2(origin.x + width, footerY), w::col(theme::kHairline));
    ImGui::SetCursorScreenPos(ImVec2(origin.x, footerY + 14.0f));
    drawFooter(width);
    drawViewPopup();
}

void MainWindow::drawViewRow(const ViewRow& row, float width) {
    const theme::Fonts& f = fonts();
    ImGui::PushID(row.id);
    ImDrawList* list = ImGui::GetWindowDrawList();
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const ImVec2 max(pos.x + width, pos.y + kViewRowHeight);
    ImGui::SetNextItemAllowOverlap();  // the buttons inside the row win the mouse
    const bool clicked = ImGui::InvisibleButton("##row", ImVec2(width, kViewRowHeight));
    const bool hovered = ImGui::IsItemHovered();
    if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
        openPopupFor(row.id, row.type == RowType::Cockpit ? ViewGroup::Internal : ViewGroup::External);
    }
    if (clicked) {
        model_.go(row.id);
    }
    // Drag a row onto another row of the same group to put it in that place.
    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
        ImGui::SetDragDropPayload(kDragPayload, &row.id, sizeof(int));
        ImGui::TextUnformatted(row.name.c_str());
        ImGui::EndDragDropSource();
    }
    bool dropHot = false;
    if (ImGui::BeginDragDropTarget()) {
        const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(
            kDragPayload, ImGuiDragDropFlags_AcceptBeforeDelivery | ImGuiDragDropFlags_AcceptNoDrawDefaultRect);
        if (payload != nullptr && payload->DataSize == static_cast<int>(sizeof(int))) {
            const int from = *static_cast<const int*>(payload->Data);
            dropHot = from != row.id;
            if (dropHot && payload->IsDelivery()) {
                model_.dropOn(from, row.id);
            }
        }
        ImGui::EndDragDropTarget();
    }

    constexpr float kPad = 14.0f;
    if (row.active) {
        w::glowRoundedRect(list, pos, max, theme::kCardRounding, theme::kCyan);
    }
    list->AddRectFilled(pos, max, w::col(hovered ? theme::kCardHover : theme::kCard), theme::kCardRounding);
    if (row.active) {
        list->AddRect(offset(pos, 1.0f, 1.0f), offset(max, -1.0f, -1.0f), w::col(theme::kCyan), theme::kCardRounding,
                      0, 2.0f);
    }

    // Left: the type icon, the label badge when the view has one, and the name.
    const std::string glyph = theme::iconText(typeIcon(row.type));
    const ImVec2 glyphSize = w::textSize(f.iconMedium, glyph.c_str());
    w::drawText(list, f.iconMedium, ImVec2(pos.x + kPad, pos.y + (kViewRowHeight - glyphSize.y) * 0.5f),
                row.active || hovered ? theme::kCyan : theme::kMuted, glyph.c_str());
    float nameX = pos.x + kPad + 34.0f;
    if (!row.label.empty()) {
        constexpr float kBadgeRadius = 12.0f;
        drawLabelBadge(list, ImVec2(nameX + kBadgeRadius, pos.y + kViewRowHeight * 0.5f), kBadgeRadius, row.label,
                       theme::kCyan, theme::kOnCyan);
        nameX += 2.0f * kBadgeRadius + 10.0f;
    }

    // Right, from the edge inwards: the Go pill, the key text, the star and the type chip.
    constexpr float kGoWidth = 64.0f;
    constexpr float kGoHeight = 30.0f;
    const float goX = max.x - 12.0f - kGoWidth;
    const std::string badgeText = w::ellipsize(f.small, row.keyText.empty() ? std::string("-") : row.keyText, 130.0f);
    const float badgeX = goX - 10.0f - w::textChipWidth(badgeText.c_str());
    const float starX = badgeX - 6.0f - 26.0f;
    const float chipWidth = w::textChipWidth(row.typeLabel.c_str());
    const float chipX = starX - 6.0f - chipWidth;
    const bool showChip = chipX - 12.0f - nameX >= 70.0f;
    const float nameRight = showChip ? chipX - 12.0f : starX - 12.0f;

    const std::string name = w::ellipsize(f.card, row.name, std::max(nameRight - nameX, 20.0f));
    const ImVec2 nameSize = w::textSize(f.card, name.c_str());
    w::drawText(list, f.card, ImVec2(nameX, pos.y + (kViewRowHeight - nameSize.y) * 0.5f), theme::kText, name.c_str());
    if (showChip) {
        w::textChip(list, ImVec2(chipX, pos.y + (kViewRowHeight - 22.0f) * 0.5f), row.typeLabel.c_str(), theme::kCyan,
                    true);
    }

    ImGui::SetCursorScreenPos(ImVec2(starX, pos.y + (kViewRowHeight - 26.0f) * 0.5f));
    if (w::starButton("##fav", row.favourite)) {
        model_.toggleFavourite(row.id);
    }
    w::tooltipText(row.favourite ? "Remove from the quick bar" : "Show in the quick bar");

    w::textChip(list, ImVec2(badgeX, pos.y + (kViewRowHeight - 22.0f) * 0.5f), badgeText.c_str(),
                row.keyText.empty() ? theme::kDim : theme::kText, false);

    ImGui::SetCursorScreenPos(ImVec2(goX, pos.y + (kViewRowHeight - kGoHeight) * 0.5f));
    if (w::pillButton("##go", "Go", nullptr, ImVec2(kGoWidth, kGoHeight), w::PillStyle::Filled)) {
        model_.go(row.id);
    }
    if (dropHot) {
        list->AddRect(pos, max, w::col(theme::kCyan), theme::kCardRounding, 0, 2.0f);
    }
    ImGui::PopID();
}

void MainWindow::openPopupFor(int viewId, ViewGroup group) {
    popupView_ = viewId;
    popupGroup_ = group;
    popupMode_ = PopupMode::Menu;
    openRequested_ = true;
    const ImVec2 mouse = ImGui::GetMousePos();
    popupX_ = clampLow(mouse.x, windowX_, windowX_ + windowW_ - kPopupWidth);
    popupY_ = clampLow(mouse.y, windowY_, windowY_ + windowH_ - kPopupHeight);
}

void MainWindow::drawViewPopup() {
    if (popupView_ == 0) {
        return;
    }
    if (openRequested_) {
        ImGui::OpenPopup("##viewmenu");
        openRequested_ = false;
    }
    ImGui::SetNextWindowPos(ImVec2(popupX_, popupY_));
    if (!ImGui::BeginPopup("##viewmenu")) {
        popupView_ = 0;
        return;
    }
    const ViewRow* row = findRow(popupView_);
    if (row == nullptr) {
        ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
        return;
    }
    const int id = popupView_;
    switch (popupMode_) {
        case PopupMode::Menu:
            ImGui::TextDisabled("%s", row->name.c_str());
            ImGui::Separator();
            if (ImGui::MenuItem("Edit...")) {
                openEditDialog(id);
            }
            if (model_.canReplace(id) && ImGui::MenuItem("Update to current view")) {
                model_.replace(id);
            }
            if (ImGui::MenuItem("Show in quick bar", nullptr, row->favourite)) {
                model_.toggleFavourite(id);
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Move up", nullptr, false, model_.canMove(id, -1, false))) {
                model_.moveUp(id);
            }
            if (ImGui::MenuItem("Move down", nullptr, false, model_.canMove(id, 1, false))) {
                model_.moveDown(id);
            }
            ImGui::Separator();
            if (ImGui::Selectable("Delete...", false, ImGuiSelectableFlags_NoAutoClosePopups)) {
                popupMode_ = PopupMode::ConfirmDelete;
            }
            break;
        case PopupMode::ConfirmDelete:
            ImGui::Text("Delete %s?", row->name.c_str());
            if (w::pillButton("##yes", "Yes", nullptr, ImVec2(80.0f, 30.0f), w::PillStyle::Filled)) {
                model_.remove(id);
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (w::pillButton("##no", "No", nullptr, ImVec2(80.0f, 30.0f), w::PillStyle::Outline)) {
                popupMode_ = PopupMode::Menu;
            }
            break;
    }
    ImGui::EndPopup();
}

void MainWindow::drawFooter(float width) {
    const theme::Fonts& f = fonts();
    ImDrawList* list = ImGui::GetWindowDrawList();
    const ImVec2 start = ImGui::GetCursorScreenPos();
    constexpr float kRow = theme::kRoundButton;

    ImGui::BeginDisabled(!model_.anySaved());
    if (w::iconButton("##previous", theme::kIconPrevious, kRow, w::IconStyle::Soft)) {
        model_.previous();
    }
    w::tooltipText("Previous saved camera");
    ImGui::SetCursorScreenPos(ImVec2(start.x + kRow + 8.0f, start.y));
    if (w::iconButton("##next", theme::kIconNext, kRow, w::IconStyle::Soft)) {
        model_.next();
    }
    w::tooltipText("Next saved camera");
    ImGui::EndDisabled();

    ImGui::SetCursorScreenPos(ImVec2(start.x + 2.0f * kRow + 20.0f, start.y));
    if (w::pillButton("##release", "Release", &theme::kIconRelease, ImVec2(0.0f, kRow), w::PillStyle::Outline)) {
        model_.release();
    }
    const float leftEnd = ImGui::GetItemRectMax().x;
    w::tooltipText("Give the camera back to X-Plane");

    const float rightEdge = start.x + width - kScrollbar;
    const float labelWidth = w::textSize(f.body, "Transition").x;
    float sliderWidth = std::min(260.0f, rightEdge - leftEnd - 24.0f - labelWidth - 12.0f);
    bool showLabel = true;
    if (sliderWidth < 120.0f) {
        showLabel = false;
        sliderWidth = std::min(260.0f, rightEdge - leftEnd - 24.0f);
    }
    if (sliderWidth >= 100.0f) {
        if (showLabel) {
            const ImVec2 labelSize = w::textSize(f.body, "Transition");
            w::drawText(list, f.body, ImVec2(rightEdge - sliderWidth - 12.0f - labelWidth, start.y + (kRow - labelSize.y) * 0.5f),
                        theme::kMuted, "Transition");
        }
        ImGui::SetCursorScreenPos(ImVec2(rightEdge - sliderWidth, start.y + (kRow - 24.0f) * 0.5f));
        drawTransitionSlider("##transition_footer", sliderWidth);
    }
}

void MainWindow::drawTransitionSlider(const char* id, float width) {
    if (!transitionDragging_) {
        transitionEdit_ = static_cast<float>(model_.settings().transitionSeconds);
    }
    char text[24] = {};
    std::snprintf(text, sizeof(text), "%.1f s", static_cast<double>(transitionEdit_));
    const w::SliderResult result = w::slider(id, &transitionEdit_, 0.0f, 10.0f, 0.1f, width, text);
    transitionDragging_ = result.active;
    // Saved once, when the slider is let go, never while it moves.
    if (result.released) {
        noteApply(model_.setTransitionSeconds(static_cast<double>(transitionEdit_)));
    }
}

void MainWindow::noteApply(ApplyResult result) {
    settingsNotSaved_ = result == ApplyResult::NotSaved;
}

void MainWindow::drawXPlaneViewsPage(float width) {
    const theme::Fonts& f = fonts();
    ImDrawList* list = ImGui::GetWindowDrawList();
    const float usable = width - kScrollbar;
    const int columns = usable >= 560.0f ? 3 : 2;
    const float tileWidth = (usable - kGap * static_cast<float>(columns - 1)) / static_cast<float>(columns);
    constexpr float kTileHeight = 92.0f;
    const ImVec2 start = ImGui::GetCursorScreenPos();
    const std::string& lastView = model_.lastXPlaneView();
    const bool xplaneInControl = model_.activeView() == 0;
    for (std::size_t i = 0; i < kXPlaneViewButtonCount; ++i) {
        const int column = static_cast<int>(i) % columns;
        const int row = static_cast<int>(i) / columns;
        const ImVec2 pos(start.x + static_cast<float>(column) * (tileWidth + kGap),
                         start.y + static_cast<float>(row) * (kTileHeight + kGap));
        const ImVec2 max(pos.x + tileWidth, pos.y + kTileHeight);
        ImGui::SetCursorScreenPos(pos);
        ImGui::PushID(static_cast<int>(i));
        if (ImGui::InvisibleButton("##tile", ImVec2(tileWidth, kTileHeight))) {
            model_.selectXPlaneView(i);
        }
        ImGui::PopID();
        const bool hovered = ImGui::IsItemHovered();
        const bool active = xplaneInControl && lastView == kXPlaneViewButtons[i].command;
        if (active) {
            w::glowRoundedRect(list, pos, max, theme::kCardRounding, theme::kCyan);
        }
        list->AddRectFilled(pos, max, w::col(active ? theme::kNavSelected : (hovered ? theme::kCardHover : theme::kCard)),
                            theme::kCardRounding);
        if (active) {
            list->AddRect(offset(pos, 1.0f, 1.0f), offset(max, -1.0f, -1.0f), w::col(theme::kCyan),
                          theme::kCardRounding, 0, 2.0f);
        }
        const std::string glyph = theme::iconText(*kTileIcons[i]);
        const ImVec2 glyphSize = w::textSize(f.iconLarge, glyph.c_str());
        w::drawText(list, f.iconLarge, ImVec2(pos.x + (tileWidth - glyphSize.x) * 0.5f, pos.y + 18.0f),
                    active || hovered ? theme::kCyan : theme::kMuted, glyph.c_str());
        const std::string label = w::ellipsize(f.body, kXPlaneViewButtons[i].label, tileWidth - 16.0f);
        const ImVec2 labelSize = w::textSize(f.body, label.c_str());
        w::drawText(list, f.body, ImVec2(pos.x + (tileWidth - labelSize.x) * 0.5f, pos.y + 58.0f), theme::kText,
                    label.c_str());
    }
    const int rows = (static_cast<int>(kXPlaneViewButtonCount) + columns - 1) / columns;
    const float noteY = start.y + static_cast<float>(rows) * (kTileHeight + kGap) + 4.0f;
    const float noteHeight = wrappedText(list, f.small, ImVec2(start.x, noteY), usable, theme::kMuted, kSaveNote);
    ImGui::SetCursorScreenPos(ImVec2(start.x, noteY + noteHeight + 8.0f));
    ImGui::Dummy(ImVec2(1.0f, 4.0f));
}

void MainWindow::drawSettingsPage(float width) {
    const theme::Fonts& f = fonts();
    const Config& config = model_.settings();
    ImDrawList* list = ImGui::GetWindowDrawList();
    const float panelWidth = width - kScrollbar;
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    float y = origin.y;

    // Draws the title/description of row `index` of the panel starting at `panelTop`; returns the row top.
    auto row = [&](float panelTop, int index, const char* title, const char* description, float controlWidth) {
        const float top = panelTop + static_cast<float>(index) * kRowHeight;
        if (index > 0) {
            list->AddLine(ImVec2(origin.x + 16.0f, top), ImVec2(origin.x + panelWidth - 16.0f, top),
                          w::col(theme::kHairline));
        }
        const float textWidth = panelWidth - 32.0f - controlWidth - 16.0f;
        const std::string t = w::ellipsize(f.card, title, textWidth);
        const std::string d = w::ellipsize(f.small, description, textWidth);
        w::drawText(list, f.card, ImVec2(origin.x + 16.0f, top + 11.0f), theme::kText, t.c_str());
        w::drawText(list, f.small, ImVec2(origin.x + 16.0f, top + 36.0f), theme::kMuted, d.c_str());
        return top;
    };
    auto sectionHeader = [&](const char* text) {
        w::drawText(list, f.small, ImVec2(origin.x + 4.0f, y), theme::kCyan, text);
        y += 24.0f;
    };
    auto controlAt = [&](float rowTop, float controlWidth, float controlHeight) {
        ImGui::SetCursorScreenPos(
            ImVec2(origin.x + panelWidth - 16.0f - controlWidth, rowTop + (kRowHeight - controlHeight) * 0.5f));
    };

    // Group 1: camera motion.
    sectionHeader("CAMERA MOTION");
    float panelTop = y;
    list->AddRectFilled(ImVec2(origin.x, panelTop), ImVec2(origin.x + panelWidth, panelTop + 2.0f * kRowHeight),
                        w::col(theme::kCard), theme::kCardRounding);
    const float sliderWidth = std::min(260.0f, panelWidth * 0.42f);
    float rowTop = row(panelTop, 0, "Transition time", "How long the camera glides between saved views.", sliderWidth);
    controlAt(rowTop, sliderWidth, 24.0f);
    drawTransitionSlider("##transition_settings", sliderWidth);

    const char* const handoffLabels[3] = {"Saved view", "Free camera", "Hold"};
    const float handoffWidth = w::segmentedWidth(handoffLabels, 3);
    rowTop = row(panelTop, 1, "After the glide", "What the camera does when it arrives at an outside view.", handoffWidth);
    controlAt(rowTop, handoffWidth, 32.0f);
    int handoff = config.handoffView == HandoffView::Saved ? 0 : (config.handoffView == HandoffView::Free ? 1 : 2);
    int handoffHover = -1;
    if (w::segmented("##handoff", handoffLabels, 3, &handoff, &handoffHover)) {
        noteApply(model_.setHandoffView(handoff == 0 ? HandoffView::Saved
                                                     : (handoff == 1 ? HandoffView::Free : HandoffView::Off)));
    }
    if (handoffHover >= 0 && ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip) && w::beginGlassTooltip()) {
        ImGui::TextUnformatted(handoffHover == 0   ? "Switch to the X-Plane view the slot was saved in"
                               : handoffHover == 1 ? "Always end in X-Plane's free camera"
                                                   : "Keep holding the camera with MyKam's own mouse and key control");
        w::endGlassTooltip();
    }
    y = panelTop + 2.0f * kRowHeight + 22.0f;

    // Group 2: quick bar.
    sectionHeader("QUICK BAR");
    panelTop = y;
    list->AddRectFilled(ImVec2(origin.x, panelTop), ImVec2(origin.x + panelWidth, panelTop + 2.0f * kRowHeight),
                        w::col(theme::kCard), theme::kCardRounding);
    rowTop = row(panelTop, 0, "Show quick bar", "The slim bar of slot buttons on the main screen.", 44.0f);
    controlAt(rowTop, 44.0f, 24.0f);
    bool showBar = config.showQuickBar;
    if (w::toggle("##showbar", &showBar)) {
        noteApply(model_.setShowQuickBar(showBar));
        if (onQuickBarChanged_) {
            onQuickBarChanged_();
        }
    }
    const char* const positionLabels[2] = {"Bottom", "Top"};
    const float positionWidth = w::segmentedWidth(positionLabels, 2);
    rowTop = row(panelTop, 1, "Quick bar position", "Bottom or top centre of the main screen.", positionWidth);
    controlAt(rowTop, positionWidth, 32.0f);
    int position = config.quickBarPosition == QuickBarPosition::Bottom ? 0 : 1;
    if (w::segmented("##barposition", positionLabels, 2, &position)) {
        noteApply(model_.setQuickBarPosition(position == 0 ? QuickBarPosition::Bottom : QuickBarPosition::Top));
        if (onQuickBarChanged_) {
            onQuickBarChanged_();
        }
    }
    y = panelTop + 2.0f * kRowHeight + 22.0f;

    // Group 3: which X-Plane view buttons the quick bar shows, and in what order. The shown views come
    // first, in bar order, with arrows to move them; the rest follow in the default order.
    sectionHeader("QUICK BAR BUTTONS");
    y += wrappedText(list, f.small, ImVec2(origin.x + 4.0f, y), panelWidth - 8.0f, theme::kMuted,
                     "Choose the X-Plane views the quick bar shows. Release and the menu button always stay.") +
         8.0f;
    {
        constexpr float kQuickRow = 44.0f;
        std::vector<std::string> order;
        for (const std::string& name : config.quickBarViews) {
            if (quickViewIndex(name) >= 0) {
                order.push_back(name);
            }
        }
        const std::size_t shownCount = order.size();
        for (std::size_t i = 0; i < kQuickViewNameCount; ++i) {
            if (std::find(order.begin(), order.end(), std::string(kQuickViewNames[i].name)) == order.end()) {
                order.push_back(kQuickViewNames[i].name);
            }
        }
        panelTop = y;
        list->AddRectFilled(ImVec2(origin.x, panelTop),
                            ImVec2(origin.x + panelWidth, panelTop + static_cast<float>(order.size()) * kQuickRow),
                            w::col(theme::kCard), theme::kCardRounding);
        std::string toggleName;
        std::string moveName;
        int moveDirection = 0;
        for (std::size_t i = 0; i < order.size(); ++i) {
            const std::size_t index = static_cast<std::size_t>(quickViewIndex(order[i]));
            const bool shown = i < shownCount;
            const float top = panelTop + static_cast<float>(i) * kQuickRow;
            if (i > 0) {
                list->AddLine(ImVec2(origin.x + 16.0f, top), ImVec2(origin.x + panelWidth - 16.0f, top),
                              w::col(theme::kHairline));
            }
            const std::string glyph = theme::iconText(*kTileIcons[index]);
            const ImVec2 glyphSize = w::textSize(f.iconMedium, glyph.c_str());
            w::drawText(list, f.iconMedium, ImVec2(origin.x + 16.0f, top + (kQuickRow - glyphSize.y) * 0.5f),
                        shown ? theme::kCyan : theme::kMuted, glyph.c_str());
            const ImVec2 labelSize = w::textSize(f.card, kXPlaneViewButtons[index].label);
            w::drawText(list, f.card, ImVec2(origin.x + 52.0f, top + (kQuickRow - labelSize.y) * 0.5f),
                        shown ? theme::kText : theme::kMuted, kXPlaneViewButtons[index].label);

            ImGui::PushID(static_cast<int>(index));
            const float toggleX = origin.x + panelWidth - 16.0f - 44.0f;
            ImGui::SetCursorScreenPos(ImVec2(toggleX, top + (kQuickRow - 24.0f) * 0.5f));
            bool on = shown;
            if (w::toggle("##show", &on)) {
                toggleName = order[i];
            }
            if (shown) {
                constexpr float kArrow = 26.0f;
                const float downX = toggleX - 12.0f - kArrow;
                const float upX = downX - 6.0f - kArrow;
                const float arrowY = top + (kQuickRow - kArrow) * 0.5f;
                ImGui::SetCursorScreenPos(ImVec2(upX, arrowY));
                ImGui::BeginDisabled(i == 0);
                if (w::iconButton("##earlier", theme::kIconUp, kArrow, w::IconStyle::Ghost)) {
                    moveName = order[i];
                    moveDirection = -1;
                }
                ImGui::EndDisabled();
                w::tooltipText("Move earlier in the quick bar");
                ImGui::SetCursorScreenPos(ImVec2(downX, arrowY));
                ImGui::BeginDisabled(i + 1 >= shownCount);
                if (w::iconButton("##later", theme::kIconDown, kArrow, w::IconStyle::Ghost)) {
                    moveName = order[i];
                    moveDirection = 1;
                }
                ImGui::EndDisabled();
                w::tooltipText("Move later in the quick bar");
            }
            ImGui::PopID();
        }
        y = panelTop + static_cast<float>(order.size()) * kQuickRow + 16.0f;
        // Applied after the loop: the list above is a copy, so the rows never change under the mouse.
        if (!toggleName.empty()) {
            noteApply(model_.toggleQuickBarView(toggleName));
        } else if (!moveName.empty()) {
            noteApply(model_.moveQuickBarView(moveName, moveDirection));
        }
        if ((!toggleName.empty() || !moveName.empty()) && onQuickBarChanged_) {
            onQuickBarChanged_();
        }
    }

    if (settingsNotSaved_) {
        y += wrappedText(list, f.body, ImVec2(origin.x + 4.0f, y), panelWidth - 8.0f, theme::kDanger,
                         "mykam.ini could not be saved. The change works until X-Plane restarts.") +
             8.0f;
    }
    ImGui::SetCursorScreenPos(ImVec2(origin.x, y));
    ImGui::Dummy(ImVec2(1.0f, 4.0f));
}

// ------------------------------------------------------------------------------------ quick bar

namespace {

constexpr float kBarSectionGap = 10.0f;     // between the switch, the chips, the + button and the icon buttons
constexpr float kBarChipGap = 6.0f;         // between two chips
constexpr float kBarChipPad = 11.0f;
constexpr float kBarArrow = 22.0f;           // the small scroll arrow buttons
constexpr float kBarBadge = 18.0f;           // the key badge inside a chip
constexpr float kBarMaxChipShare = 0.6f;     // the chip area is at most this share of the main screen width
constexpr float kBarWheelStep = 48.0f;       // pixels scrolled per wheel click
constexpr float kBarPopupWidth = 232.0f;
constexpr float kBarPopupReserve = 236.0f;   // extra window height while the right-click popup is open
constexpr float kBarPad = 12.0f;
constexpr float kBarGap = 6.0f;              // between two round icon buttons
constexpr float kBarButton = 30.0f;
constexpr int kBarMaxResyncTries = 3;        // times the window is placed again while its width is not the wanted one
constexpr float kBarVerticalPad = 8.0f;
constexpr float kBarFadedAlpha = 0.6f;

constexpr float kBarSeparator = 9.0f;
constexpr const char* kBarEmptyText = "Star a view to show it here";

// Width of the Internal | External switch: two segments, each an icon and a short label.
struct SwitchMetrics {
    float segment[2] = {0.0f, 0.0f};
    float total = 0.0f;
};

SwitchMetrics switchMetrics() {
    const theme::Fonts& f = fonts();
    const char* const labels[2] = {"Internal", "External"};
    const theme::Icon* const icons[2] = {&theme::kIconCockpit, &theme::kIconFree};
    SwitchMetrics m;
    m.total = 6.0f;
    for (int i = 0; i < 2; ++i) {
        const std::string glyph = theme::iconText(*icons[i]);
        m.segment[i] = 12.0f + w::textSize(f.iconSmall, glyph.c_str()).x + 6.0f + w::textSize(f.body, labels[i]).x + 12.0f;
        m.total += m.segment[i];
    }
    return m;
}

// The two-segment pill switch. Returns true when a segment was clicked and puts it in `clicked`.
bool groupSwitch(ViewGroup selected, ViewGroup* clicked) {
    const theme::Fonts& f = fonts();
    const char* const labels[2] = {"Internal", "External"};
    const theme::Icon* const icons[2] = {&theme::kIconCockpit, &theme::kIconFree};
    const char* const tips[2] = {"Show the Internal (cockpit) views", "Show the External (free camera and chase) views"};
    const SwitchMetrics m = switchMetrics();
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    ImDrawList* list = ImGui::GetWindowDrawList();
    list->AddRectFilled(pos, ImVec2(pos.x + m.total, pos.y + kBarButton), w::col(theme::kField), kBarButton * 0.5f);
    bool changed = false;
    float x = pos.x + 3.0f;
    for (int i = 0; i < 2; ++i) {
        ImGui::SetCursorScreenPos(ImVec2(x, pos.y + 3.0f));
        ImGui::PushID(i);
        const bool pressed = ImGui::InvisibleButton("##seg", ImVec2(m.segment[i], kBarButton - 6.0f));
        const bool hovered = ImGui::IsItemHovered();
        ImGui::PopID();
        const bool on = (i == 0) == (selected == ViewGroup::Internal);
        const ImVec2 a(x, pos.y + 3.0f);
        const ImVec2 b(x + m.segment[i], pos.y + kBarButton - 3.0f);
        if (on) {
            list->AddRectFilled(a, b, w::col(hovered ? theme::kCyanHover : theme::kCyan), (b.y - a.y) * 0.5f);
        } else if (hovered) {
            list->AddRectFilled(a, b, w::col(theme::kHoverWash), (b.y - a.y) * 0.5f);
        }
        const ImU32 colour = on ? theme::kOnCyan : (hovered ? theme::kText : theme::kMuted);
        const std::string glyph = theme::iconText(*icons[i]);
        const ImVec2 glyphSize = w::textSize(f.iconSmall, glyph.c_str());
        const ImVec2 labelSize = w::textSize(f.body, labels[i]);
        w::drawText(list, f.iconSmall, ImVec2(x + 12.0f, a.y + (b.y - a.y - glyphSize.y) * 0.5f), colour, glyph.c_str());
        w::drawText(list, f.body, ImVec2(x + 12.0f + glyphSize.x + 6.0f, a.y + (b.y - a.y - labelSize.y) * 0.5f), colour,
                    labels[i]);
        w::tooltipText(tips[i]);
        if (pressed) {
            *clicked = i == 0 ? ViewGroup::Internal : ViewGroup::External;
            changed = true;
        }
        x += m.segment[i];
    }
    ImGui::SetCursorScreenPos(ImVec2(pos.x + m.total, pos.y));
    return changed;
}

constexpr float kChipBadgeRadius = 10.0f;  // the round label badge on a chip

float chipWidthFor(const ViewRow& row) {
    const theme::Fonts& f = fonts();
    const std::string glyph = theme::iconText(typeIcon(row.type));
    if (!row.label.empty()) {
        // A view with a label shows only the label, in a perfect circle the size of the other round
        // buttons; the name is in the tooltip.
        return kBarButton;
    }
    const float lead = w::textSize(f.iconSmall, glyph.c_str()).x;
    return kBarChipPad + lead + 6.0f + w::textSize(f.body, row.shortName.c_str()).x + kBarChipPad;
}

// The look of one chip at `pos`: the label in a round badge (or the type icon), then the short name.
void drawChipBody(ImDrawList* list, ImVec2 pos, float width, const ViewRow& row, bool hovered) {
    const theme::Fonts& f = fonts();
    // Everything is drawn inside the item rectangle, inset by kBarCircleInset, so a neighbour, a hover
    // ring or the clip edge never touches it.
    const ImVec2 inMin(pos.x + kBarCircleInset, pos.y + kBarCircleInset);
    const ImVec2 inMax(pos.x + width - kBarCircleInset, pos.y + kBarButton - kBarCircleInset);
    const float radius = barCircleRadius(kBarButton);
    ImU32 textColour = theme::kText;
    ImU32 iconColour = theme::kMuted;
    if (!row.label.empty()) {
        // A true circle, drawn the same way as the round icon buttons next to it.
        const ImVec2 centre(pos.x + width * 0.5f, pos.y + kBarButton * 0.5f);
        if (row.active) {
            list->AddCircleFilled(centre, radius, w::col(hovered ? theme::kCyanHover : theme::kCyan), 48);
        } else {
            list->AddCircleFilled(centre, radius, w::col(hovered ? theme::kCardHover : theme::kCard), 48);
            list->AddCircle(centre, radius - 0.5f, w::col(hovered ? theme::kCyan : theme::kHairline), 48, 1.0f);
        }
    } else if (row.active) {
        list->AddRectFilled(inMin, inMax, w::col(hovered ? theme::kCyanHover : theme::kCyan), radius);
        textColour = theme::kOnCyan;
        iconColour = theme::kOnCyan;
    } else {
        list->AddRectFilled(inMin, inMax, w::col(hovered ? theme::kCardHover : theme::kCard), radius);
        list->AddRect(offset(inMin, 0.5f, 0.5f), offset(inMax, -0.5f, -0.5f), w::col(hovered ? theme::kCyan : theme::kHairline),
                      radius, 0, 1.0f);
        if (hovered) {
            iconColour = theme::kCyan;
        }
    }
    if (!row.label.empty()) {
        // Label only, centred in the circle; a 2-character label that is too wide uses the body font.
        ImFont* font = f.section;
        ImVec2 labelSize = w::textSize(font, row.label.c_str());
        if (labelSize.x > width - 10.0f) {
            font = f.body;
            labelSize = w::textSize(font, row.label.c_str());
            if (labelSize.x > width - 8.0f) {
                font = f.small;
                labelSize = w::textSize(font, row.label.c_str());
            }
        }
        w::drawText(list, font,
                    ImVec2(pos.x + (width - labelSize.x) * 0.5f, pos.y + (kBarButton - labelSize.y) * 0.5f),
                    row.active ? theme::kOnCyan : (hovered ? theme::kCyan : theme::kText), row.label.c_str());
        return;
    }
    float x = pos.x + kBarChipPad;
    {
        const std::string glyph = theme::iconText(typeIcon(row.type));
        const ImVec2 glyphSize = w::textSize(f.iconSmall, glyph.c_str());
        w::drawText(list, f.iconSmall, ImVec2(x, pos.y + (kBarButton - glyphSize.y) * 0.5f), iconColour, glyph.c_str());
        x += glyphSize.x + 6.0f;
    }
    const ImVec2 nameSize = w::textSize(f.body, row.shortName.c_str());
    w::drawText(list, f.body, ImVec2(x, pos.y + (kBarButton - nameSize.y) * 0.5f), textColour, row.shortName.c_str());
}

// One view chip. True when clicked; `rightClicked` when right-clicked. Leaves the chip as the last item,
// so the caller can add a tooltip and drag and drop to it.
bool viewChip(const ViewRow& row, float width, bool* rightClicked) {
    ImGui::PushID(row.id);
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const bool clicked = ImGui::InvisibleButton("##chip", ImVec2(width, kBarButton));
    *rightClicked = ImGui::IsItemClicked(ImGuiMouseButton_Right);
    const bool hovered = ImGui::IsItemHovered();
    ImGui::PopID();
    drawChipBody(ImGui::GetWindowDrawList(), pos, width, row, hovered);
    return clicked;
}

}  // namespace

// The edit dialog: a glass modal inside the main window (X-Plane only sends clicks to the window they
// happen in, so it cannot live in the quick bar). Fields are a draft in the core; nothing is stored until
// Save, which writes everything once.
void MainWindow::drawEditDialog() {
    if (editView_ == 0) {
        return;
    }
    const ViewRow* row = findRow(editView_);
    if (row == nullptr) {
        closeEditDialog();  // the view was deleted meanwhile
        return;
    }
    const theme::Fonts& f = fonts();
    constexpr float kDialogWidth = 460.0f;
    constexpr float kButtonH = 32.0f;
    constexpr float kDialogPad = 22.0f;
    const float margin = poppedOut() ? 0.0f : theme::kShadowMargin;

    // Dim the window behind the dialog.
    ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(windowX_ + margin, windowY_ + margin),
                                              ImVec2(windowX_ + windowW_ - margin, windowY_ + windowH_ - margin),
                                              IM_COL32(0, 0, 0, 120), poppedOut() ? 0.0f : theme::kWindowRounding);
    if (editOpenNow_) {
        ImGui::OpenPopup("##editview");
        editOpenNow_ = false;
    }
    ImGui::SetNextWindowPos(ImVec2(windowX_ + windowW_ * 0.5f, windowY_ + windowH_ * 0.5f), ImGuiCond_Always,
                            ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(kDialogWidth, 0.0f));
    ImGui::SetNextWindowSizeConstraints(ImVec2(kDialogWidth, 0.0f),
                                        ImVec2(kDialogWidth, std::max(windowH_ - 2.0f * margin - 24.0f, 200.0f)));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(kDialogPad, 18.0f));
    const bool open = ImGui::BeginPopupModal("##editview", nullptr,
                                             ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove |
                                                 ImGuiWindowFlags_NoSavedSettings);
    ImGui::PopStyleVar();
    if (!open) {
        XPLMDebugString("[MyKam] edit dialog closed by the UI before Save\n");
        closeEditDialog();  // closed by ImGui itself
        return;
    }
    if (!editLoggedOpen_) {
        editLoggedOpen_ = true;
        XPLMDebugString("[MyKam] edit dialog open\n");
    }

    const EditCheck check = model_.checkEdit(draft_);
    const float contentW = ImGui::GetContentRegionAvail().x;
    const auto caption = [&](const char* text) {
        ImGui::Dummy(ImVec2(1.0f, 2.0f));
        ImGui::TextColored(theme::vec4(theme::kMuted), "%s", text);
    };
    const auto problem = [&](const std::string& text) {
        if (!text.empty()) {
            ImGui::TextColored(theme::vec4(theme::kDanger), "%s", text.c_str());
        }
    };

    w::drawText(ImGui::GetWindowDrawList(), f.section, ImGui::GetCursorScreenPos(), theme::kText, "Edit view");
    ImGui::Dummy(ImVec2(1.0f, w::textSize(f.section, "Edit view").y + 4.0f));

    // Name.
    caption("Name");
    if (editFocusName_) {
        ImGui::SetKeyboardFocusHere();
        editFocusName_ = false;
    }
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::InputText("##editname", nameBuffer_, sizeof(nameBuffer_), ImGuiInputTextFlags_CallbackCharFilter,
                         nameFilter)) {
        draft_.name = nameBuffer_;
    }
    problem(check.name);

    // Quick bar label with a live preview of the chip.
    caption("Quick bar label (1 or 2 letters or digits)");
    ImGui::SetNextItemWidth(84.0f);
    if (ImGui::InputText("##editlabel", labelBuffer_, sizeof(labelBuffer_), ImGuiInputTextFlags_CallbackCharFilter,
                         labelFilter)) {
        draft_.label = labelBuffer_;
    }
    ImGui::SameLine();
    {
        ViewRow preview = *row;
        preview.label = UiModel::labelPreview(draft_.label);
        preview.shortName = UiModel::chipName(draft_.name);
        preview.active = false;
        const float chipW = chipWidthFor(preview);
        const ImVec2 at = ImGui::GetCursorScreenPos();
        drawChipBody(ImGui::GetWindowDrawList(), ImVec2(at.x, at.y + 1.0f), chipW, preview, false);
        ImGui::Dummy(ImVec2(chipW, kBarButton));
    }
    problem(check.label);

    // Key: none, a numpad slot, or a custom key.
    caption("Key");
    const char* const modeLabels[3] = {"None", "Numpad slot", "Custom key"};
    int mode = static_cast<int>(draft_.keyMode);
    if (w::segmented("##keymode", modeLabels, 3, &mode)) {
        if (editCapturing_) {
            endKeyCapture();
            editCapturing_ = false;
        }
        draft_.keyMode = static_cast<KeyMode>(mode);
        editNote_.clear();
        if (draft_.keyMode == KeyMode::Numpad && draft_.numpad == 0) {
            draft_.numpad = row->hotkey != 0 ? row->hotkey : 1;
            for (int n = 1; n <= kHotkeyCount && row->hotkey == 0; ++n) {
                if (model_.keyOwnerName(n, editView_).empty()) {
                    draft_.numpad = n;
                    break;
                }
            }
        }
    }
    if (draft_.keyMode == KeyMode::Numpad) {
        // Like the numeric keypad: 7 8 9 / 4 5 6 / 1 2 3 / a wide 0 under 1 and 2.
        const int layout[kHotkeyCount] = {7, 8, 9, 4, 5, 6, 1, 2, 3, kHotkeyZero};
        const float spacing = ImGui::GetStyle().ItemSpacing.x;
        const float cell = (ImGui::GetContentRegionAvail().x - 2.0f * spacing) / 3.0f;
        for (int i = 0; i < kHotkeyCount; ++i) {
            const int n = layout[i];
            if (i % 3 != 0) {
                ImGui::SameLine();
            }
            const float width = n == kHotkeyZero ? 2.0f * cell + spacing : cell;
            const std::string owner = model_.keyOwnerName(n, editView_);
            const std::string text = w::ellipsize(f.body, owner.empty() ? std::string(kDigits[n - 1])
                                                                        : std::string(kDigits[n - 1]) + " " + owner,
                                                  width - 24.0f);
            ImGui::PushID(n);
            if (w::pillButton("##np", text.c_str(), nullptr, ImVec2(width, kButtonH),
                              draft_.numpad == n ? w::PillStyle::Filled : w::PillStyle::Outline)) {
                draft_.numpad = n;
            }
            if (!owner.empty() && ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip) && w::beginGlassTooltip()) {
                ImGui::Text("Used by %s. Saving moves the key here.", owner.c_str());
                w::endGlassTooltip();
            }
            ImGui::PopID();
        }
    } else if (draft_.keyMode == KeyMode::Custom) {
        if (editCapturing_) {
            const CapturedKey got = takeCapturedKey();
            if (got.event != CaptureEvent::None) {
                endKeyCapture();
                editCapturing_ = false;
                if (got.event == CaptureEvent::Clear) {
                    draft_.custom = CustomKey();
                    editKeyText_.clear();
                    editNote_.clear();
                } else if (got.event == CaptureEvent::Key) {
                    CustomKey key;
                    key.vkey = got.vkey;
                    key.flags = got.flags;
                    if (isValidKey(key)) {
                        draft_.custom = key;
                        editKeyText_ = model_.customKeyText(key);
                        editNote_.clear();
                    } else {
                        editNote_ = "That key cannot be used.";
                    }
                }
            }
        }
        const char* shown = editCapturing_ ? "Press a key..."
                                           : (draft_.custom.set() ? editKeyText_.c_str() : "Click, then press a key");
        if (w::pillButton("##capture", shown, nullptr, ImVec2(contentW - 86.0f, kButtonH),
                          editCapturing_ ? w::PillStyle::Filled : w::PillStyle::Outline)) {
            if (editCapturing_) {
                endKeyCapture();
            } else {
                beginKeyCapture();
                editNote_.clear();
            }
            editCapturing_ = !editCapturing_;
        }
        ImGui::SameLine();
        if (w::pillButton("##clearkey", "Clear", nullptr, ImVec2(76.0f, kButtonH), w::PillStyle::Outline)) {
            draft_.custom = CustomKey();
            editKeyText_.clear();
            if (editCapturing_) {
                endKeyCapture();
                editCapturing_ = false;
            }
        }
        ImGui::TextColored(theme::vec4(theme::kDim), "Esc cancels, Backspace clears.");
        ImGui::PushStyleColor(ImGuiCol_Text, theme::vec4(theme::kMuted));
        ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + contentW);
        ImGui::TextUnformatted(UiModel::customKeyNote());
        ImGui::PopTextWrapPos();
        ImGui::PopStyleColor();
        problem(editNote_);
    }
    const std::string conflict = model_.editKeyConflict(draft_);
    if (!conflict.empty()) {
        ImGui::TextColored(theme::vec4(theme::kAmber), "Used by %s. Saving moves the key here.", conflict.c_str());
    }
    problem(check.key);

    // Quick bar star.
    ImGui::Dummy(ImVec2(1.0f, 6.0f));
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Show in quick bar");
    ImGui::SameLine(kDialogPad + contentW - 44.0f, 0.0f);
    w::toggle("##editfav", &draft_.favourite);

    // Buttons.
    ImGui::Dummy(ImVec2(1.0f, 8.0f));
    draft_.name = nameBuffer_;
    draft_.label = labelBuffer_;
    const bool canSave = model_.canSaveEdit(draft_);
    if (!canSave) {
        const EditCheck why = model_.checkEdit(draft_);
        const std::string& reason = !why.name.empty() ? why.name : !why.label.empty() ? why.label : why.key;
        if (!reason.empty()) {
            ImGui::TextColored(theme::vec4(theme::kDanger), "Save needs: %s", reason.c_str());
        }
    }
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(contentW - 2.0f * 96.0f - 10.0f, 0.0f));
    const bool cancel = w::pillButton("##editcancel", "Cancel", nullptr, ImVec2(96.0f, kButtonH), w::PillStyle::Outline);
    ImGui::SameLine();
    ImGui::BeginDisabled(!canSave);
    const bool save = w::pillButton("##editsave", "Save", nullptr, ImVec2(96.0f, kButtonH), w::PillStyle::Filled);
    ImGui::EndDisabled();
    if (save && !canSave) {
        XPLMDebugString("[MyKam] edit dialog: Save clicked but the draft is not valid\n");
    }
    if (save && canSave) {
        // Take the text straight from the fields, so an edit ImGui did not report still counts.
        draft_.name = nameBuffer_;
        draft_.label = labelBuffer_;
        const std::string line = "[MyKam] edit dialog: Save clicked, name '" + draft_.name + "', label '" +
                                 draft_.label + "'\n";
        XPLMDebugString(line.c_str());
        model_.saveEdit(draft_);
        closeEditDialog();
        ImGui::CloseCurrentPopup();
    } else if (cancel) {
        XPLMDebugString("[MyKam] edit dialog: Cancel clicked\n");
        closeEditDialog();
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

int QuickBarWindow::barHeight() {
    return static_cast<int>(kBarButton + 2.0f * kBarVerticalPad);
}

int QuickBarWindow::hostFlags() const {
    return 0;
}

void QuickBarWindow::openPopupFor(int viewId, float anchorX) {
    popupView_ = viewId;
    popupMode_ = PopupMode::Menu;
    openRequested_ = true;
    popupX_ = anchorX;
}

void QuickBarWindow::drawPopup(ImVec2 origin, ImVec2 extent, bool atTop) {
    if (popupView_ == 0) {
        return;
    }
    if (openRequested_) {
        ImGui::OpenPopup("##barmenu");
        openRequested_ = false;
    }
    // The popup sits in the extra room the window gains while it is open: above the bar at the bottom of
    // the screen, below it at the top.
    const float x = clampLow(popupX_, origin.x + 4.0f, origin.x + extent.x - kBarPopupWidth - 4.0f);
    if (atTop) {
        ImGui::SetNextWindowPos(ImVec2(x, origin.y + static_cast<float>(barHeight()) + 4.0f), ImGuiCond_Always,
                                ImVec2(0.0f, 0.0f));
    } else {
        ImGui::SetNextWindowPos(ImVec2(x, origin.y + extent.y - static_cast<float>(barHeight()) - 4.0f),
                                ImGuiCond_Always, ImVec2(0.0f, 1.0f));
    }
    ImGui::SetNextWindowSize(ImVec2(kBarPopupWidth, 0.0f));
    if (!ImGui::BeginPopup("##barmenu")) {
        popupView_ = 0;
        return;
    }
    const ViewRow* row = nullptr;
    for (const ViewRow& candidate : model_.barRows()) {
        if (candidate.id == popupView_) {
            row = &candidate;
            break;
        }
    }
    // The popup lives inside the window: close it when the mouse leaves.
    const bool inside = ImGui::IsMouseHoveringRect(origin, ImVec2(origin.x + extent.x, origin.y + extent.y), false);
    if (row == nullptr || !inside) {
        ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
        popupView_ = 0;
        return;
    }
    const theme::Fonts& f = fonts();
    const int id = popupView_;
    switch (popupMode_) {
        case PopupMode::Menu:
            ImGui::TextDisabled("%s", w::ellipsize(f.body, row->name, kBarPopupWidth - 24.0f).c_str());
            ImGui::Separator();
            if (model_.canReplace(id) && ImGui::MenuItem("Update to current view")) {
                model_.replace(id);
            }
            if (ImGui::MenuItem("Edit...") && onEdit_) {
                onEdit_(id);  // the dialog opens in the main window, which is shown if it was hidden
            }
            if (ImGui::MenuItem("Move left", nullptr, false, model_.canMove(id, -1, true))) {
                model_.moveLeft(id);
            }
            if (ImGui::MenuItem("Move right", nullptr, false, model_.canMove(id, 1, true))) {
                model_.moveRight(id);
            }
            if (ImGui::MenuItem("Remove from quick bar")) {
                model_.removeFromBar(id);
            }
            ImGui::Separator();
            if (ImGui::Selectable("Delete...", false, ImGuiSelectableFlags_NoAutoClosePopups)) {
                popupMode_ = PopupMode::ConfirmDelete;
            }
            break;
        case PopupMode::ConfirmDelete:
            ImGui::Text("Delete %s?", w::ellipsize(f.body, row->name, kBarPopupWidth - 70.0f).c_str());
            if (w::pillButton("##yes", "Yes", nullptr, ImVec2(80.0f, 28.0f), w::PillStyle::Filled)) {
                model_.remove(id);
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (w::pillButton("##no", "No", nullptr, ImVec2(80.0f, 28.0f), w::PillStyle::Outline)) {
                popupMode_ = PopupMode::Menu;
            }
            break;
    }
    ImGui::EndPopup();
}

void QuickBarWindow::build() {
    model_.refresh();
    model_.updateBar();
    const theme::Fonts& f = fonts();
    const ImGuiIO& io = ImGui::GetIO();
    const ImVec2 origin = ImGui::GetWindowPos();
    const ImVec2 extent = ImGui::GetWindowSize();
    const bool atTop = model_.quickBarAtTop();
    const ViewGroup group = model_.barGroup();
    const std::vector<ViewRow>& rows = model_.barRows();
    const float barH = static_cast<float>(barHeight());

    // ---- measure: every width of the bar, from its content
    const SwitchMetrics sw = switchMetrics();
    // The chips: one layout, used for the width below, the scroll follow and the drawing.
    std::vector<float> chipWidths;
    chipWidths.reserve(rows.size());
    for (const ViewRow& row : rows) {
        chipWidths.push_back(chipWidthFor(row));
    }
    const BarLayout chipLayout = layoutRow(chipWidths, kBarChipGap);
    const float chipsContent = chipLayout.total;
    const float noViewsWidth = w::textSize(f.body, kBarEmptyText).x;
    const float maxArea = std::max(io.DisplaySize.x * kBarMaxChipShare, 160.0f);
    const bool scrolls = !rows.empty() && chipsContent > maxArea;
    const float areaWidth = rows.empty() ? noViewsWidth : (scrolls ? maxArea : chipsContent);
    const float viewWidth = scrolls ? areaWidth - 2.0f * (kBarArrow + 2.0f) : areaWidth;  // the chips' own window
    const float maxScroll = std::max(chipsContent - viewWidth, 0.0f);

    // The whole bar as one layout: the switch, the chip area, +, the separator, the X-Plane view buttons
    // chosen in the settings, release and the menu button, with the padding at both ends. Drawing places
    // every element from these slots, so the width and the picture cannot disagree.
    const std::vector<QuickViewButton>& xviews = model_.quickViewButtons();
    constexpr std::size_t kSlotSwitch = 0;
    constexpr std::size_t kSlotChips = 1;
    constexpr std::size_t kSlotAdd = 2;
    constexpr std::size_t kSlotSeparator = 3;
    constexpr std::size_t kSlotFirstView = 4;
    std::vector<BarElement> elements;
    elements.reserve(kSlotFirstView + xviews.size() + 3);
    elements.push_back(BarElement{sw.total, kBarPad});
    elements.push_back(BarElement{areaWidth, kBarSectionGap});
    elements.push_back(BarElement{kBarButton, kBarSectionGap});
    elements.push_back(BarElement{kBarSeparator, kBarSectionGap});
    for (std::size_t i = 0; i < xviews.size(); ++i) {
        elements.push_back(BarElement{kBarButton, i == 0 ? kBarSectionGap : kBarGap});
    }
    const std::size_t slotRelease = elements.size();
    elements.push_back(BarElement{kBarButton, xviews.empty() ? kBarSectionGap : kBarGap});
    elements.push_back(BarElement{kBarButton, kBarGap});  // the menu button
    elements.push_back(BarElement{0.0f, kBarPad});        // the right padding
    const BarLayout barLayout = layoutBar(elements);
    const float barW = barLayout.total;

    // ---- size: from the content, applied to the X-Plane window when it changed. The layout above is in
    // ImGui units, which are X-Plane boxels (DisplaySize is the screen in boxels and the font scale
    // already divides out the 2x atlas), so the same number is the window width. A window that is not
    // the wanted size (the move has not landed yet, or the screen changed) is placed again a few times.
    const bool popupOpen = popupView_ != 0;
    const int wantW = static_cast<int>(std::ceil(popupOpen ? std::max(barW, kBarPopupWidth + 16.0f) : barW));
    const int wantH = static_cast<int>(std::ceil(barH + (popupOpen ? kBarPopupReserve : 0.0f)));
    const bool sizeChanged = wantW != wantedWidth_ || wantH != wantedHeight_;
    const bool windowOff = std::lround(extent.x) != wantW || std::lround(extent.y) != wantH;
    if (sizeChanged) {
        wantedWidth_ = wantW;
        wantedHeight_ = wantH;
        resyncTries_ = 0;
        if (onResize_) {
            onResize_();
        }
    } else if (windowOff && resyncTries_ < kBarMaxResyncTries) {
        ++resyncTries_;
        if (onResize_) {
            onResize_();
        }
    } else if (!windowOff) {
        resyncTries_ = 0;
    }
    // A window narrower than the bar would cut it off: wait for the new size instead of drawing a
    // clipped bar (at most a few frames, so a window X-Plane refuses to widen still shows the bar).
    if (extent.x + 0.5f < barW && narrowFrames_ < kBarMaxResyncTries) {
        ++narrowFrames_;
        return;
    }
    if (extent.x + 0.5f >= barW) {
        narrowFrames_ = 0;
    }

    // ---- where the bar sits inside the window: centred, at the bottom (bar at the bottom of the screen)
    // or the top. The window is taller than the bar only while the right-click popup is open.
    const float offX = std::max((extent.x - barW) * 0.5f, 0.0f);
    const float offY = atTop ? 0.0f : std::max(extent.y - barH, 0.0f);
    const ImVec2 barMin(origin.x + offX, origin.y + offY);
    const ImVec2 barMax(barMin.x + barW, barMin.y + barH);

    // ---- scroll: reset on a group change, follow the active chip, mouse wheel over the bar
    if (group != shownGroup_) {
        shownGroup_ = group;
        scroll_ = 0.0f;
        shownActive_ = -1;  // bring the active chip of the new group into view
    }
    const bool hoverBar = ImGui::IsMouseHoveringRect(barMin, barMax, false);
    if (scrolls && hoverBar) {
        scroll_ -= (io.MouseWheel + io.MouseWheelH) * kBarWheelStep;
    }
    int activeId = 0;
    float activeLeft = 0.0f;
    float activeRight = 0.0f;
    for (std::size_t i = 0; i < rows.size(); ++i) {
        if (rows[i].active) {
            activeId = rows[i].id;
            activeLeft = chipLayout.slots[i].x;
            activeRight = chipLayout.slots[i].right();
        }
    }
    if (activeId != shownActive_) {
        shownActive_ = activeId;
        if (activeId != 0 && scrolls) {
            if (activeLeft < scroll_) {
                scroll_ = activeLeft;
            } else if (activeRight > scroll_ + viewWidth) {
                scroll_ = activeRight - viewWidth;
            }
        }
    }
    scroll_ = std::min(std::max(scroll_, 0.0f), maxScroll);

    // ---- fade to 60 percent while the mouse is elsewhere, back to full on hover (or while a popup is open)
    const bool over = hoverBar || popupView_ != 0;
    const float target = over ? 1.0f : kBarFadedAlpha;
    fade_ += (target - fade_) * std::min(1.0f, io.DeltaTime * 10.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, fade_);

    ImDrawList* list = ImGui::GetWindowDrawList();
    const float rounding = barH * 0.5f;
    list->AddRectFilled(barMin, barMax, w::col(theme::kGlass), rounding);
    list->AddRect(offset(barMin, 0.5f, 0.5f), offset(barMax, -0.5f, -0.5f), w::col(theme::kBorder), rounding, 0, 1.0f);

    const float top = barMin.y + (barH - kBarButton) * 0.5f;
    const auto slotX = [&](std::size_t slot) { return barMin.x + barLayout.slots[slot].x; };

    // 1. the Internal | External switch
    ImGui::SetCursorScreenPos(ImVec2(slotX(kSlotSwitch), top));
    ViewGroup clickedGroup = group;
    if (groupSwitch(group, &clickedGroup)) {
        model_.setBarGroup(clickedGroup);
    }

    // 2. the view chips (or the empty text), scrolling inside their own area
    const float x = slotX(kSlotChips);
    if (rows.empty()) {
        w::drawText(list, f.body, ImVec2(x, top + (kBarButton - w::textSize(f.body, kBarEmptyText).y) * 0.5f),
                    theme::kMuted, kBarEmptyText);
    } else {
        float viewLeft = x;
        if (scrolls) {
            ImGui::SetCursorScreenPos(ImVec2(x, top + (kBarButton - kBarArrow) * 0.5f));
            ImGui::BeginDisabled(scroll_ <= 0.0f);
            if (w::iconButton("##scrollleft", theme::kIconChevronLeft, kBarArrow, w::IconStyle::Ghost)) {
                scroll_ = std::max(scroll_ - viewWidth * 0.6f, 0.0f);
            }
            ImGui::EndDisabled();
            ImGui::SetCursorScreenPos(ImVec2(x + areaWidth - kBarArrow, top + (kBarButton - kBarArrow) * 0.5f));
            ImGui::BeginDisabled(scroll_ >= maxScroll);
            if (w::iconButton("##scrollright", theme::kIconChevronRight, kBarArrow, w::IconStyle::Ghost)) {
                scroll_ = std::min(scroll_ + viewWidth * 0.6f, maxScroll);
            }
            ImGui::EndDisabled();
            viewLeft = x + kBarArrow + 2.0f;
        }
        ImGui::PushClipRect(ImVec2(viewLeft, top), ImVec2(viewLeft + viewWidth, top + kBarButton), true);
        for (std::size_t i = 0; i < rows.size(); ++i) {
            const ViewRow& row = rows[i];
            const float cw = chipLayout.slots[i].width;
            const float cx = viewLeft - scroll_ + chipLayout.slots[i].x;
            ImGui::SetCursorScreenPos(ImVec2(cx, top));
            bool rightClicked = false;
            if (viewChip(row, cw, &rightClicked)) {
                model_.go(row.id);
            }
            if (rightClicked) {
                openPopupFor(row.id, cx);
            }
            // Drag a chip onto another chip to put it in that place (the group order changes).
            if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
                ImGui::SetDragDropPayload(kDragPayload, &row.id, sizeof(int));
                ImGui::TextUnformatted(row.name.c_str());
                ImGui::EndDragDropSource();
            }
            if (ImGui::BeginDragDropTarget()) {
                const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(
                    kDragPayload, ImGuiDragDropFlags_AcceptBeforeDelivery | ImGuiDragDropFlags_AcceptNoDrawDefaultRect);
                if (payload != nullptr && payload->DataSize == static_cast<int>(sizeof(int))) {
                    const int from = *static_cast<const int*>(payload->Data);
                    if (from != row.id) {
                        // Inside the chip's rectangle (the 2 px stroke is drawn on the inner side).
                        list->AddRect(ImVec2(cx + 1.0f, top + 1.0f), ImVec2(cx + cw - 1.0f, top + kBarButton - 1.0f),
                                      w::col(theme::kCyan), barCircleRadius(kBarButton), 0, 2.0f);
                        if (payload->IsDelivery()) {
                            model_.dropOn(from, row.id);
                        }
                    }
                }
                ImGui::EndDragDropTarget();
            }
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip) && w::beginGlassTooltip()) {
                ImGui::TextUnformatted(row.name.c_str());
                ImGui::TextDisabled("%s, key: %s", row.typeLabel.c_str(),
                                    row.keyText.empty() ? "none" : row.keyText.c_str());
                ImGui::TextDisabled("Click: go. Right-click: more. Drag: reorder.");
                w::endGlassTooltip();
            }
        }
        ImGui::PopClipRect();
    }

    // 3. the + button: save the current view as a new view of the shown group
    const bool canSave = model_.canSaveToBar();
    ImGui::SetCursorScreenPos(ImVec2(slotX(kSlotAdd), top));
    ImGui::BeginDisabled(!canSave);
    if (w::iconButton("##addview", theme::kIconAdd, kBarButton, w::IconStyle::Filled)) {
        model_.saveToBar();
    }
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip | ImGuiHoveredFlags_AllowWhenDisabled) &&
        w::beginGlassTooltip()) {
        ImGui::TextUnformatted(canSave ? "Save the current view as a new view and show it here"
                                       : UiModel::saveDisabledReason(group));
        w::endGlassTooltip();
    }

    // 4. a thin separator, then the X-Plane view buttons chosen in the settings, release and the window toggle
    const float sepX = slotX(kSlotSeparator);
    list->AddLine(ImVec2(sepX + kBarSeparator * 0.5f, top + 5.0f), ImVec2(sepX + kBarSeparator * 0.5f, top + kBarButton - 5.0f),
                  w::col(theme::kHairline));
    for (std::size_t i = 0; i < xviews.size(); ++i) {
        const QuickViewButton& view = xviews[i];
        ImGui::SetCursorScreenPos(ImVec2(slotX(kSlotFirstView + i), top));
        ImGui::PushID(static_cast<int>(view.index));
        // The active X-Plane view is the filled cyan circle; the others are the soft round buttons.
        if (w::iconButton("##xpview", *kTileIcons[view.index], kBarButton,
                          view.active ? w::IconStyle::Filled : w::IconStyle::Soft)) {
            model_.selectXPlaneView(view.index);
        }
        ImGui::PopID();
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip) && w::beginGlassTooltip()) {
            ImGui::Text("X-Plane view: %s", view.label);
            w::endGlassTooltip();
        }
    }
    ImGui::SetCursorScreenPos(ImVec2(slotX(slotRelease), top));
    if (w::iconButton("##release", theme::kIconRelease, kBarButton, w::IconStyle::Soft)) {
        model_.release();
    }
    w::tooltipText("Give the camera back to X-Plane");
    ImGui::SetCursorScreenPos(ImVec2(slotX(slotRelease + 1), top));
    if (w::iconButton("##menu", theme::kIconMenu, kBarButton, w::IconStyle::Soft) && onMenu_) {
        onMenu_();
    }
    w::tooltipText("Show or hide the MyKam window");
    ImGui::PopStyleVar();

    drawPopup(origin, extent, atTop);
}

// ------------------------------------------------------------------------------------ both windows

UiWindows::~UiWindows() {
    destroy();
}

bool UiWindows::create(UiModel& model) {
    if (main_) {
        return true;
    }
    model_ = &model;
    int sl = 0;
    int st = 0;
    int sr = 0;
    int sb = 0;
    XPLMGetScreenBoundsGlobal(&sl, &st, &sr, &sb);

    // The fonts live in <plugin folder>/fonts, next to win_x64.
    char binary[2048] = {};
    XPLMGetPluginInfo(XPLMGetMyID(), nullptr, binary, nullptr, nullptr);
    binary[sizeof(binary) - 1] = '\0';
    ImguiWindow::setFontDirectory(pluginFolderFromBinaryPath(binary) + "/fonts");

    main_ = std::make_unique<MainWindow>(model);
    ImguiWindow::Params mainParams;
    mainParams.title = "MyKam";
    mainParams.left = sl + 80;
    mainParams.top = st - 100;
    mainParams.right = mainParams.left + 860;
    mainParams.bottom = mainParams.top - 700;
    mainParams.visible = false;  // hidden until the user asks for it
    mainParams.selfDecorated = true;
    if (!main_->create(mainParams)) {
        main_.reset();
        return false;
    }
    main_->setResizingLimits(kMinWidth, kMinHeight, kMaxWidth, kMaxHeight);
    main_->setOnQuickBarSettingsChanged([this]() { syncQuickBar(); });

    bar_ = std::make_unique<QuickBarWindow>(model);
    bar_->setOnEdit([this](int viewId) { editView(viewId); });
    ImguiWindow::Params barParams;
    barParams.title = "MyKam quick bar";
    barParams.left = sl;
    barParams.top = sb + QuickBarWindow::barHeight();
    barParams.right = sl + bar_->wantedWidth();
    barParams.bottom = sb;
    barParams.visible = false;
    barParams.selfDecorated = false;
    if (bar_->create(barParams)) {
        bar_->setOnMenu([this]() { toggleMainWindow(); });
        bar_->setOnResize([this]() { syncQuickBar(); });
        syncQuickBar();
    } else {
        bar_.reset();  // the main window still works without the bar
    }

    pluginsMenu_ = XPLMFindPluginsMenu();
    if (pluginsMenu_ != nullptr) {
        menuItem_ = XPLMAppendMenuItem(pluginsMenu_, "MyKam", nullptr, 1);
        menu_ = XPLMCreateMenu("MyKam", pluginsMenu_, menuItem_, &UiWindows::menuHandler, this);
        if (menu_ != nullptr) {
            XPLMAppendMenuItem(menu_, "Show window", reinterpret_cast<void*>(static_cast<std::intptr_t>(1)), 1);
            XPLMAppendMenuItem(menu_, "Hide window", reinterpret_cast<void*>(static_cast<std::intptr_t>(2)), 1);
            activeItem_ = XPLMAppendMenuItem(menu_, "MyKam on", reinterpret_cast<void*>(static_cast<std::intptr_t>(3)), 1);
            syncActiveMenu();
        }
    }
    return true;
}

void UiWindows::destroy() {
    if (menu_ != nullptr) {
        XPLMDestroyMenu(menu_);
        menu_ = nullptr;
    }
    if (pluginsMenu_ != nullptr && menuItem_ >= 0) {
        XPLMRemoveMenuItem(pluginsMenu_, menuItem_);
    }
    pluginsMenu_ = nullptr;
    menuItem_ = -1;
    activeItem_ = -1;
    if (bar_) {
        bar_->destroy();
        bar_.reset();
    }
    if (main_) {
        main_->destroy();
        main_.reset();
    }
    model_ = nullptr;
}

void UiWindows::toggleMainWindow() {
    if (!main_) {
        return;
    }
    if (main_->visible()) {
        hideMainWindow();
    } else {
        showMainWindow();
    }
}

void UiWindows::toggleActive() {
    if (model_ == nullptr) {
        return;
    }
    model_->toggleActive();
    if (!model_->active()) {
        hideMainWindow();
    }
    syncQuickBar();
    syncActiveMenu();
}

void UiWindows::syncActiveMenu() {
    if (menu_ != nullptr && activeItem_ >= 0 && model_ != nullptr) {
        XPLMCheckMenuItem(menu_, activeItem_, model_->active() ? xplm_Menu_Checked : xplm_Menu_Unchecked);
    }
}

void UiWindows::showMainWindow() {
    if (model_ != nullptr && !model_->active()) {
        model_->toggleActive();  // asking for the window turns MyKam back on
        syncQuickBar();
        syncActiveMenu();
    }
    if (main_) {
        main_->setVisible(true);
        main_->bringToFront();
    }
}

void UiWindows::editView(int viewId) {
    if (main_) {
        main_->openEditDialog(viewId);
        showMainWindow();
    }
}

void UiWindows::hideMainWindow() {
    if (main_) {
        main_->setVisible(false);
    }
}

void UiWindows::syncQuickBar() {
    if (!bar_ || model_ == nullptr) {
        return;
    }
    int sl = 0;
    int st = 0;
    int sr = 0;
    int sb = 0;
    XPLMGetScreenBoundsGlobal(&sl, &st, &sr, &sb);
    constexpr int kMargin = 48;
    const int width = bar_->wantedWidth();
    const int height = bar_->wantedHeight();
    const int left = sl + (sr - sl - width) / 2;
    int top = 0;
    if (model_->quickBarAtTop()) {
        top = st - kMargin;  // the bar stays at the top edge; a popup grows the window downwards
    } else {
        top = sb + kMargin + height;  // the bar stays at the bottom edge; a popup grows the window upwards
    }
    bar_->setGeometry(left, top, left + width, top - height);
    bar_->setVisible(model_->quickBarVisible());
}

void UiWindows::menuHandler(void* menuRef, void* itemRef) {
    UiWindows* self = static_cast<UiWindows*>(menuRef);
    if (self == nullptr) {
        return;
    }
    try {
        const std::intptr_t item = reinterpret_cast<std::intptr_t>(itemRef);
        if (item == 1) {
            self->showMainWindow();
        } else if (item == 2) {
            self->hideMainWindow();
        } else if (item == 3) {
            self->toggleActive();
        }
    } catch (...) {
    }
}

}  // namespace mykam
