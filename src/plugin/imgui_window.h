#pragma once

#include "imgui_includes.h"
#include "ui_theme.h"
#include "xplm_includes.h"

#include <chrono>
#include <string>

// One X-Plane window (XPLMCreateWindowEx) whose contents are drawn by Dear ImGui. Every window owns
// its own ImGui context (and font atlas), so two windows never share popup or focus state. The
// X-Plane draw callback runs one complete ImGui frame; mouse, wheel and key callbacks feed that
// window's context. Subclasses only submit widgets in build().
//
// Coordinates: X-Plane gives global desktop boxels with y up. ImGui gets the whole X-Plane screen as
// its display (y down from the top edge) and the window as a borderless host window at the right
// spot, so tooltips and popups may leave the window rectangle. A popped-out window gets its own size
// as the display instead.

namespace mykam {

class ImguiWindow {
public:
    ImguiWindow() = default;
    ImguiWindow(const ImguiWindow&) = delete;
    ImguiWindow& operator=(const ImguiWindow&) = delete;
    virtual ~ImguiWindow();

    struct Params {
        const char* title = "";
        int left = 0;
        int top = 0;
        int right = 0;
        int bottom = 0;
        bool visible = false;
        // true: an undecorated, resizable X-Plane window (xplm_WindowDecorationSelfDecoratedResizable): the
        // plugin draws the frame, the header and the drag area itself. false: no decoration, click-through
        // outside the drawn area.
        bool selfDecorated = false;
    };
    // Creates the X-Plane window. The ImGui context is made on the first draw (it needs OpenGL).
    bool create(const Params& params);
    // Destroys the window and the ImGui context (call while the plugin is still enabled).
    void destroy();

    bool created() const { return window_ != nullptr; }
    bool visible() const;
    void setVisible(bool visible);
    void bringToFront();
    void setGeometry(int left, int top, int right, int bottom);
    void setResizingLimits(int minWidth, int minHeight, int maxWidth, int maxHeight);

    // The folder the Inter and Material Icons TTF files are loaded from (<plugin>/fonts). Set once before
    // the first window is created; a missing folder or file falls back to the built-in font.
    static void setFontDirectory(const std::string& directory);

protected:
    // Submit the widgets inside the host window. Called once per drawn frame, never while hidden.
    virtual void build() = 0;
    // Extra ImGuiWindowFlags for the host window (it is always borderless, fixed and unsaved).
    virtual int hostFlags() const { return 0; }
    // Style tweaks, called once after the context exists.
    virtual void styleContext() {}
    // WindowPadding of the host window (the window code positions everything itself).
    virtual ImVec2 hostPadding() const { return ImVec2(0.0f, 0.0f); }

    // For windows that draw their own header: call every frame with true while the mouse holds the header
    // drag area. Moves the X-Plane window with the mouse (not when popped out; the OS does that).
    void dragWindow(bool headerHeld);
    bool poppedOut() const { return popped_; }
    // Pop the window out into its own operating system window, or back in (applied before the next frame).
    void requestPopOut(bool popOut);

    // "Press a key" capture. While it is on, the window takes keyboard focus and the next key down is
    // kept (with its modifiers) instead of going to ImGui; Escape reports Cancel, Backspace Clear. The key
    // arrives from the X-Plane key callback, so it is read in the next frame with `takeCapturedKey`.
    enum class CaptureEvent { None, Key, Cancel, Clear };
    struct CapturedKey {
        CaptureEvent event = CaptureEvent::None;
        int vkey = 0;
        int flags = 0;  // XPLMKeyFlags modifier bits (shift, option/alt, control)
    };
    void beginKeyCapture();
    void endKeyCapture();
    bool keyCaptureActive() const { return capturing_; }
    CapturedKey takeCapturedKey();
    // Keeps left, top and width; sets the window height in boxels.
    void setHeight(int height);
    int height() const;

private:
    bool ensureContext();
    void drawFrame();
    void releaseContext();
    // XPLM global boxels (y up) to host-window-relative ImGui coordinates.
    void toImgui(int gx, int gy, float& x, float& y) const;
    void mouse(int button, int x, int y, XPLMMouseStatus status);
    void keyEvent(char key, XPLMKeyFlags flags, char virtualKey, int losingFocus);

    static void drawCallback(XPLMWindowID window, void* refcon);
    static int leftClickCallback(XPLMWindowID window, int x, int y, XPLMMouseStatus status, void* refcon);
    static int rightClickCallback(XPLMWindowID window, int x, int y, XPLMMouseStatus status, void* refcon);
    static void keyCallback(XPLMWindowID window, char key, XPLMKeyFlags flags, char virtualKey, void* refcon,
                            int losingFocus);
    static XPLMCursorStatus cursorCallback(XPLMWindowID window, int x, int y, void* refcon);
    static int wheelCallback(XPLMWindowID window, int x, int y, int wheel, int clicks, void* refcon);

    XPLMWindowID window_ = nullptr;
    ImGuiContext* context_ = nullptr;
    bool backendReady_ = false;
    bool blur_ = false;  // keyboard focus left the window: drop any active text field next frame
    bool capturing_ = false;
    CapturedKey captured_;
    bool contextFailed_ = false;
    // Where the host window sits in ImGui space and the size of the ImGui display, from the last frame.
    float hostX_ = 0.0f;
    float hostY_ = 0.0f;
    float displayW_ = 0.0f;
    float displayH_ = 0.0f;
    bool popped_ = false;
    theme::Fonts fonts_;
    int pendingPop_ = 0;  // 0 nothing, 1 pop out, 2 pop back in
    bool dragging_ = false;
    int dragMouseX_ = 0;
    int dragMouseY_ = 0;
    int dragLeft_ = 0;
    int dragTop_ = 0;
    int dragWidth_ = 0;
    int dragHeight_ = 0;
    std::chrono::steady_clock::time_point lastFrame_{};
    bool haveLastFrame_ = false;
};

}  // namespace mykam
