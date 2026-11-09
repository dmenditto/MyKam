#pragma once

#include <functional>
#include <memory>
#include <utility>

#include "core/ui_model.h"
#include "imgui_window.h"

// The mykam main window and the quick bar. Both only read core::UiModel and call its actions; every
// decision (what a card shows, which slot is next, whether a name is valid) lives in the core.

namespace mykam {

// The floating window: our own glass frame and header, a sidebar with the Internal, External, X-Plane
// views and Settings pages, and the content area.
class MainWindow : public ImguiWindow {
public:
    explicit MainWindow(UiModel& model) : model_(model) {}
    // Called after a setting that the quick bar shows (visible, position) changed.
    void setOnQuickBarSettingsChanged(std::function<void()> callback) { onQuickBarChanged_ = std::move(callback); }
    // Open the edit dialog for a view on the next frame (the caller shows the window; a collapsed window
    // is expanded). The dialog lives in this window because X-Plane only delivers clicks inside a window.
    void openEditDialog(int viewId);

protected:
    void build() override;
    void styleContext() override;

private:
    enum class PopupMode { Menu, ConfirmDelete };
    enum class Page { Internal, External, XPlaneViews, Settings };

    void drawBackground(ImVec2 min, ImVec2 max);
    void drawHeader(ImVec2 min, ImVec2 max);
    void drawSidebar(ImVec2 min, ImVec2 max);
    void drawContent(ImVec2 min, ImVec2 max);
    void drawPageTitle(ImVec2 origin, const char* title, const char* subtitle);
    void drawGroupPage(ViewGroup group, ImVec2 origin, float width, float height);
    void drawViewRow(const ViewRow& row, float width);
    void drawViewPopup();
    void drawEditDialog();
    void closeEditDialog();
    void drawFooter(float width);
    void drawXPlaneViewsPage(float width);
    void drawSettingsPage(float width);
    void drawTransitionSlider(const char* id, float width);
    void setMinimised(bool minimised);
    std::string statusText() const;
    void openPopupFor(int viewId, ViewGroup group);
    void noteApply(ApplyResult result);
    const ViewRow* findRow(int viewId) const;

    UiModel& model_;
    std::function<void()> onQuickBarChanged_;
    Page page_ = Page::Internal;
    bool minimised_ = false;
    int restoreHeight_ = 0;
    float transitionEdit_ = 1.5f;
    bool transitionDragging_ = false;
    PopupMode popupMode_ = PopupMode::Menu;
    int popupView_ = 0;  // id of the view the popup is about, 0 when closed
    ViewGroup popupGroup_ = ViewGroup::Internal;
    bool openRequested_ = false;
    float popupX_ = 0.0f;
    float popupY_ = 0.0f;
    // The edit dialog.
    int editRequest_ = 0;       // a view to open the dialog for on the next frame
    int editView_ = 0;          // the view being edited, 0 when the dialog is closed
    bool editOpenNow_ = false;  // OpenPopup is due
    bool editLoggedOpen_ = false;  // the "dialog open" log line was written for this dialog
    bool editFocusName_ = false;
    bool editCapturing_ = false;  // the "Press a key" field waits for a key
    EditDraft draft_;
    std::string editKeyText_;  // the draft's custom key as text, kept so no frame asks X-Plane for it
    std::string editNote_;     // a problem found while capturing a key
    char nameBuffer_[kMaxSlotNameLength + 1] = {};
    char labelBuffer_[kMaxLabelLength + 1] = {};
    bool settingsNotSaved_ = false;
    float windowX_ = 0.0f;  // host window rectangle in ImGui space, set each frame
    float windowY_ = 0.0f;
    float windowW_ = 0.0f;
    float windowH_ = 0.0f;
};

// The slim bar near the bottom (or top) centre of the main screen: an Internal | External switch, the
// views of the shown group as chips, a + button, and the X-Plane view buttons. Its width follows its
// content (chips scroll when there are many); the window is resized only when that size changes.
class QuickBarWindow : public ImguiWindow {
public:
    explicit QuickBarWindow(UiModel& model) : model_(model) {}
    // Pixel height of the bar itself (the window is taller only while the right-click popup is open).
    static int barHeight();
    // The size the window should have, from the last drawn frame (a starting guess before the first).
    int wantedWidth() const { return wantedWidth_; }
    int wantedHeight() const { return wantedHeight_; }
    // The menu button toggles the main window.
    void setOnMenu(std::function<void()> callback) { onMenu_ = std::move(callback); }
    // Called (from the draw callback) when wantedWidth or wantedHeight changed: place the window again.
    void setOnResize(std::function<void()> callback) { onResize_ = std::move(callback); }
    // "Edit..." in a chip's menu: the owner shows the main window and opens the dialog there.
    void setOnEdit(std::function<void(int)> callback) { onEdit_ = std::move(callback); }

protected:
    void build() override;
    int hostFlags() const override;

private:
    enum class PopupMode { Menu, ConfirmDelete };
    void openPopupFor(int viewId, float anchorX);
    void drawPopup(ImVec2 origin, ImVec2 extent, bool atTop);

    UiModel& model_;
    std::function<void()> onMenu_;
    std::function<void()> onResize_;
    std::function<void(int)> onEdit_;
    float fade_ = 0.6f;  // 0.6 while the mouse is elsewhere, 1 over the bar
    int wantedWidth_ = 640;
    int wantedHeight_ = 46;
    int resyncTries_ = 0;   // times the window was placed again while its size was not the wanted one
    int narrowFrames_ = 0;  // frames skipped because the window was narrower than the bar
    float scroll_ = 0.0f;  // chip area scroll offset in pixels
    ViewGroup shownGroup_ = ViewGroup::Internal;
    int shownActive_ = -1;  // the active view id last scrolled into view
    PopupMode popupMode_ = PopupMode::Menu;
    int popupView_ = 0;  // id of the view the popup is about, 0 when closed
    bool openRequested_ = false;
    float popupX_ = 0.0f;
};

// Owns both windows and the Plugins > mykam menu. Created at plugin enable, destroyed at disable.
class UiWindows {
public:
    UiWindows() = default;
    UiWindows(const UiWindows&) = delete;
    UiWindows& operator=(const UiWindows&) = delete;
    ~UiWindows();

    bool create(UiModel& model);
    void destroy();

    void toggleMainWindow();
    // Turn MyKam on or off: off hides the window and the quick bar, on brings the quick bar back.
    void toggleActive();
    void showMainWindow();
    void hideMainWindow();
    // Show the main window (even when hidden) and open the edit dialog for a view there.
    void editView(int viewId);
    // Show or hide and place the quick bar from the current settings.
    void syncQuickBar();

private:
    static void menuHandler(void* menuRef, void* itemRef);
    void syncActiveMenu();  // the check mark of the "MyKam on" menu item

    UiModel* model_ = nullptr;
    std::unique_ptr<MainWindow> main_;
    std::unique_ptr<QuickBarWindow> bar_;
    XPLMMenuID pluginsMenu_ = nullptr;
    XPLMMenuID menu_ = nullptr;
    int menuItem_ = -1;
    int activeItem_ = -1;
};

}  // namespace mykam
