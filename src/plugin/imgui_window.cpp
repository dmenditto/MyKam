#include "imgui_window.h"

#include <algorithm>

#pragma warning(push, 0)
#include <GL/gl.h>
#pragma warning(pop)

namespace mykam {

namespace {

// X-Plane virtual key to ImGui key, ImGuiKey_None when we do not forward it.
ImGuiKey mapKey(unsigned char vk) {
    switch (vk) {
        case XPLM_VK_BACK: return ImGuiKey_Backspace;
        case XPLM_VK_TAB: return ImGuiKey_Tab;
        case XPLM_VK_RETURN: return ImGuiKey_Enter;
        case XPLM_VK_ESCAPE: return ImGuiKey_Escape;
        case XPLM_VK_SPACE: return ImGuiKey_Space;
        case XPLM_VK_PRIOR: return ImGuiKey_PageUp;
        case XPLM_VK_NEXT: return ImGuiKey_PageDown;
        case XPLM_VK_END: return ImGuiKey_End;
        case XPLM_VK_HOME: return ImGuiKey_Home;
        case XPLM_VK_LEFT: return ImGuiKey_LeftArrow;
        case XPLM_VK_UP: return ImGuiKey_UpArrow;
        case XPLM_VK_RIGHT: return ImGuiKey_RightArrow;
        case XPLM_VK_DOWN: return ImGuiKey_DownArrow;
        case XPLM_VK_INSERT: return ImGuiKey_Insert;
        case XPLM_VK_DELETE: return ImGuiKey_Delete;
        case XPLM_VK_A: return ImGuiKey_A;  // select all, copy, paste and so on inside a text field
        case XPLM_VK_C: return ImGuiKey_C;
        case XPLM_VK_V: return ImGuiKey_V;
        case XPLM_VK_X: return ImGuiKey_X;
        default: return ImGuiKey_None;
    }
}

std::string g_fontDirectory;

struct Bounds {
    int left = 0;
    int top = 0;
    int right = 0;
    int bottom = 0;
};

void addMonitor(int, int left, int top, int right, int bottom, void* refcon) {
    Bounds* all = static_cast<Bounds*>(refcon);
    all->left = std::min(all->left, left);
    all->top = std::max(all->top, top);
    all->right = std::max(all->right, right);
    all->bottom = std::min(all->bottom, bottom);
}

// The union of the main screen and every monitor X-Plane covers, in global boxels (y up).
Bounds desktopBounds() {
    Bounds all;
    XPLMGetScreenBoundsGlobal(&all.left, &all.top, &all.right, &all.bottom);
    XPLMGetAllMonitorBoundsGlobal(&addMonitor, &all);
    return all;
}

}  // namespace

void ImguiWindow::setFontDirectory(const std::string& directory) {
    g_fontDirectory = directory;
}

ImguiWindow::~ImguiWindow() {
    destroy();
}

bool ImguiWindow::create(const Params& p) {
    if (window_ != nullptr) {
        return true;
    }
    XPLMCreateWindow_t params{};
    params.structSize = sizeof(params);
    params.left = p.left;
    params.top = p.top;
    params.right = p.right;
    params.bottom = p.bottom;
    params.visible = p.visible ? 1 : 0;
    params.drawWindowFunc = &ImguiWindow::drawCallback;
    params.handleMouseClickFunc = &ImguiWindow::leftClickCallback;
    params.handleKeyFunc = &ImguiWindow::keyCallback;
    params.handleCursorFunc = &ImguiWindow::cursorCallback;
    params.handleMouseWheelFunc = &ImguiWindow::wheelCallback;
    params.refcon = this;
    params.decorateAsFloatingWindow =
        p.selfDecorated ? xplm_WindowDecorationSelfDecoratedResizable : xplm_WindowDecorationNone;
    params.layer = xplm_WindowLayerFloatingWindows;
    params.handleRightClickFunc = &ImguiWindow::rightClickCallback;
    window_ = XPLMCreateWindowEx(&params);
    if (window_ == nullptr) {
        return false;
    }
    if (p.selfDecorated) {
        XPLMSetWindowTitle(window_, p.title);  // shown by the operating system when the window is popped out
    }
    XPLMSetWindowPositioningMode(window_, xplm_WindowPositionFree, -1);
    return true;
}

void ImguiWindow::destroy() {
    if (window_ != nullptr) {
        if (XPLMHasKeyboardFocus(window_) != 0) {
            XPLMTakeKeyboardFocus(nullptr);
        }
        XPLMDestroyWindow(window_);
        window_ = nullptr;
    }
    releaseContext();
}

bool ImguiWindow::visible() const {
    return window_ != nullptr && XPLMGetWindowIsVisible(window_) != 0;
}

void ImguiWindow::setVisible(bool v) {
    if (window_ == nullptr) {
        return;
    }
    if (!v && XPLMHasKeyboardFocus(window_) != 0) {
        XPLMTakeKeyboardFocus(nullptr);
    }
    XPLMSetWindowIsVisible(window_, v ? 1 : 0);
}

void ImguiWindow::bringToFront() {
    if (window_ != nullptr) {
        XPLMBringWindowToFront(window_);
    }
}

void ImguiWindow::setGeometry(int left, int top, int right, int bottom) {
    if (window_ != nullptr) {
        XPLMSetWindowGeometry(window_, left, top, right, bottom);
    }
}

void ImguiWindow::setResizingLimits(int minWidth, int minHeight, int maxWidth, int maxHeight) {
    if (window_ != nullptr) {
        XPLMSetWindowResizingLimits(window_, minWidth, minHeight, maxWidth, maxHeight);
    }
}

bool ImguiWindow::ensureContext() {
    if (context_ != nullptr) {
        return true;
    }
    if (contextFailed_) {
        return false;
    }
    context_ = ImGui::CreateContext();
    if (context_ == nullptr) {
        contextFailed_ = true;
        return false;
    }
    ImGui::SetCurrentContext(context_);
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;  // no settings file: the windows never touch the disk
    io.LogFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;  // X-Plane owns the cursor
    io.MouseDragThreshold = 12.0f;  // a slightly shaky click stays a click instead of starting a drag
    // Fonts at 2x when X-Plane draws 2 pixels per boxel (high-DPI), otherwise 1x; crisp either way.
    {
        GLint viewport[4] = {0, 0, 0, 0};
        glGetIntegerv(GL_VIEWPORT, viewport);
        int boxelsWide = 0;
        if (XPLMWindowIsPoppedOut(window_) != 0) {
            int l = 0;
            int t = 0;
            int r = 0;
            int b = 0;
            XPLMGetWindowGeometry(window_, &l, &t, &r, &b);
            boxelsWide = r - l;
        } else {
            int sl = 0;
            int st = 0;
            int sr = 0;
            int sb = 0;
            XPLMGetScreenBoundsGlobal(&sl, &st, &sr, &sb);
            boxelsWide = sr - sl;
        }
        const float ratio = boxelsWide > 0 ? static_cast<float>(viewport[2]) / static_cast<float>(boxelsWide) : 1.0f;
        fonts_ = theme::buildFonts(io, g_fontDirectory, ratio >= 1.5f ? 2.0f : 1.0f);
    }
    theme::applyStyle(ImGui::GetStyle());
    styleContext();
    if (!ImGui_ImplOpenGL2_Init()) {
        ImGui::DestroyContext(context_);
        context_ = nullptr;
        contextFailed_ = true;
        return false;
    }
    backendReady_ = true;
    return true;
}

void ImguiWindow::releaseContext() {
    if (context_ == nullptr) {
        return;
    }
    ImGui::SetCurrentContext(context_);
    if (backendReady_) {
        ImGui_ImplOpenGL2_Shutdown();  // frees the font texture
        backendReady_ = false;
    }
    ImGui::DestroyContext(context_);
    context_ = nullptr;
}

void ImguiWindow::toImgui(int gx, int gy, float& x, float& y) const {
    int l = 0;
    int t = 0;
    int r = 0;
    int b = 0;
    XPLMGetWindowGeometry(window_, &l, &t, &r, &b);
    if (XPLMWindowIsPoppedOut(window_) != 0) {
        x = static_cast<float>(gx - l);
        y = static_cast<float>(t - gy);
        return;
    }
    int sl = 0;
    int st = 0;
    int sr = 0;
    int sb = 0;
    XPLMGetScreenBoundsGlobal(&sl, &st, &sr, &sb);
    x = static_cast<float>(gx - sl);
    y = static_cast<float>(st - gy);
}

void ImguiWindow::drawFrame() {
    if (!ensureContext()) {
        return;
    }
    ImGui::SetCurrentContext(context_);
    theme::setActiveFonts(&fonts_);
    XPLMSetGraphicsState(0, 1, 0, 0, 1, 0, 0);
    if (pendingPop_ != 0) {
        XPLMSetWindowPositioningMode(window_, pendingPop_ == 1 ? xplm_WindowPopOut : xplm_WindowPositionFree, -1);
        pendingPop_ = 0;
    }

    int l = 0;
    int t = 0;
    int r = 0;
    int b = 0;
    XPLMGetWindowGeometry(window_, &l, &t, &r, &b);
    const float width = static_cast<float>(std::max(r - l, 1));
    const float height = static_cast<float>(std::max(t - b, 1));
    popped_ = XPLMWindowIsPoppedOut(window_) != 0;
    ImGuiIO& io = ImGui::GetIO();
    if (popped_) {
        displayW_ = width;
        displayH_ = height;
        hostX_ = 0.0f;
        hostY_ = 0.0f;
    } else {
        int sl = 0;
        int st = 0;
        int sr = 0;
        int sb = 0;
        XPLMGetScreenBoundsGlobal(&sl, &st, &sr, &sb);
        displayW_ = static_cast<float>(std::max(sr - sl, 1));
        displayH_ = static_cast<float>(std::max(st - sb, 1));
        hostX_ = static_cast<float>(l - sl);
        hostY_ = static_cast<float>(st - t);
    }
    io.DisplaySize = ImVec2(displayW_, displayH_);
    // The OpenGL viewport is in pixels; X-Plane's boxels may be larger with UI scaling.
    GLint viewport[4] = {0, 0, 0, 0};
    glGetIntegerv(GL_VIEWPORT, viewport);
    io.DisplayFramebufferScale = viewport[2] > 0 ? ImVec2(static_cast<float>(viewport[2]) / displayW_,
                                                          static_cast<float>(viewport[3]) / displayH_)
                                                 : ImVec2(1.0f, 1.0f);

    const std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
    float dt = 1.0f / 60.0f;
    if (haveLastFrame_) {
        dt = std::chrono::duration<float>(now - lastFrame_).count();
    }
    lastFrame_ = now;
    haveLastFrame_ = true;
    io.DeltaTime = std::min(std::max(dt, 0.0001f), 0.1f);

    // The real mouse position every frame, so hover and tooltips work next to the window as well.
    int mx = 0;
    int my = 0;
    XPLMGetMouseLocationGlobal(&mx, &my);
    float ix = 0.0f;
    float iy = 0.0f;
    toImgui(mx, my, ix, iy);
    io.AddMousePosEvent(ix, iy);

    ImGui_ImplOpenGL2_NewFrame();
    ImGui::NewFrame();
    {
        // A click outside an open (non-modal) popup: ImGui would only close the popup and swallow the click,
        // so the user had to click again. Close it here, before anything is drawn, so this same click reaches
        // the item under the mouse.
        ImGuiContext& g = *ImGui::GetCurrentContext();
        if ((io.MouseClicked[0] || io.MouseClicked[1]) && g.OpenPopupStack.Size > 0) {
            bool insidePopup = false;
            bool modal = false;
            for (const ImGuiPopupData& data : g.OpenPopupStack) {
                if (data.Window == nullptr) {
                    continue;
                }
                if ((data.Window->Flags & ImGuiWindowFlags_Modal) != 0) {
                    modal = true;
                }
                if (data.Window->Rect().Contains(io.MousePos)) {
                    insidePopup = true;
                }
            }
            if (!insidePopup && !modal) {
                ImGui::ClosePopupsExceptModals();
            }
        }
    }
    if (blur_) {
        blur_ = false;
        // X-Plane took the keyboard away: stop the active text field from listening, but leave window and
        // popup focus alone. (SetWindowFocus(nullptr) here closed the edit dialog before Save.) Clicking the
        // field again takes the keyboard back.
        // Only a text field: the focus change often comes with a click, and the active item is then the
        // button being pressed (clearing that ate the first click on Save).
        ImGuiContext& g = *ImGui::GetCurrentContext();
        if (g.ActiveId != 0 && ImGui::GetInputTextState(g.ActiveId) != nullptr) {
            ImGui::ClearActiveID();
        }
    }
    ImGui::SetNextWindowPos(ImVec2(hostX_, hostY_));
    ImGui::SetNextWindowSize(ImVec2(width, height));
    const int flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                      ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNav |
                      ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar |
                      ImGuiWindowFlags_NoScrollWithMouse | hostFlags();
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, hostPadding());
    const bool open = ImGui::Begin("##mykam_host", nullptr, flags);
    ImGui::PopStyleVar();
    if (open) {
        try {
            build();
        } catch (...) {
        }
    }
    ImGui::End();
    ImGui::Render();
    XPLMSetGraphicsState(0, 1, 0, 0, 1, 0, 0);
    ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());

    // Keyboard focus only while a text field is active, so X-Plane's own keys work otherwise.
    const bool wantsText = io.WantTextInput || capturing_;
    const bool hasFocus = XPLMHasKeyboardFocus(window_) != 0;
    if (wantsText && !hasFocus) {
        XPLMTakeKeyboardFocus(window_);
    } else if (!wantsText && hasFocus) {
        XPLMTakeKeyboardFocus(nullptr);
    }
}

void ImguiWindow::dragWindow(bool headerHeld) {
    if (!headerHeld || popped_ || window_ == nullptr) {
        dragging_ = false;
        return;
    }
    int mx = 0;
    int my = 0;
    XPLMGetMouseLocationGlobal(&mx, &my);
    int l = 0;
    int t = 0;
    int r = 0;
    int b = 0;
    XPLMGetWindowGeometry(window_, &l, &t, &r, &b);
    if (!dragging_) {
        dragging_ = true;
        dragMouseX_ = mx;
        dragMouseY_ = my;
        dragLeft_ = l;
        dragTop_ = t;
        dragWidth_ = r - l;
        dragHeight_ = t - b;
        return;
    }
    const Bounds desk = desktopBounds();
    int left = dragLeft_ + (mx - dragMouseX_);
    int top = dragTop_ + (my - dragMouseY_);
    // Keep part of the header on the desktop so the window cannot be lost.
    left = std::max(desk.left - dragWidth_ + 120, std::min(left, desk.right - 120));
    top = std::max(desk.bottom + 60, std::min(top, desk.top));
    XPLMSetWindowGeometry(window_, left, top, left + dragWidth_, top - dragHeight_);
}

void ImguiWindow::requestPopOut(bool popOut) {
    pendingPop_ = popOut ? 1 : 2;
}

void ImguiWindow::setHeight(int newHeight) {
    if (window_ == nullptr) {
        return;
    }
    int l = 0;
    int t = 0;
    int r = 0;
    int b = 0;
    XPLMGetWindowGeometry(window_, &l, &t, &r, &b);
    XPLMSetWindowGeometry(window_, l, t, r, t - newHeight);
}

int ImguiWindow::height() const {
    if (window_ == nullptr) {
        return 0;
    }
    int l = 0;
    int t = 0;
    int r = 0;
    int b = 0;
    XPLMGetWindowGeometry(window_, &l, &t, &r, &b);
    return t - b;
}

void ImguiWindow::mouse(int button, int x, int y, XPLMMouseStatus status) {
    if (context_ == nullptr) {
        return;
    }
    ImGui::SetCurrentContext(context_);
    ImGuiIO& io = ImGui::GetIO();
    float ix = 0.0f;
    float iy = 0.0f;
    toImgui(x, y, ix, iy);
    io.AddMousePosEvent(ix, iy);
    if (status == xplm_MouseDown) {
        io.AddMouseButtonEvent(button, true);
    } else if (status == xplm_MouseUp) {
        io.AddMouseButtonEvent(button, false);
    }
}

void ImguiWindow::beginKeyCapture() {
    capturing_ = true;
    captured_ = CapturedKey();
}

void ImguiWindow::endKeyCapture() {
    capturing_ = false;
    captured_ = CapturedKey();
}

ImguiWindow::CapturedKey ImguiWindow::takeCapturedKey() {
    const CapturedKey result = captured_;
    captured_ = CapturedKey();
    return result;
}

void ImguiWindow::keyEvent(char key, XPLMKeyFlags flags, char virtualKey, int losingFocus) {
    if (context_ == nullptr) {
        return;
    }
    ImGui::SetCurrentContext(context_);
    ImGuiIO& io = ImGui::GetIO();
    if (losingFocus != 0) {
        io.ClearInputKeys();
        blur_ = true;
        if (capturing_ && captured_.event == CaptureEvent::None) {
            captured_.event = CaptureEvent::Cancel;  // focus went elsewhere: stop waiting for a key
        }
        return;
    }
    if (capturing_) {
        // The next key down is the answer; key repeats and releases are ignored. Nothing reaches ImGui.
        const int vkey = static_cast<unsigned char>(virtualKey);
        if ((flags & xplm_UpFlag) == 0 && captured_.event == CaptureEvent::None && vkey != 0) {
            if (vkey == XPLM_VK_ESCAPE) {
                captured_.event = CaptureEvent::Cancel;
            } else if (vkey == XPLM_VK_BACK) {
                captured_.event = CaptureEvent::Clear;
            } else {
                captured_.event = CaptureEvent::Key;
                captured_.vkey = vkey;
                captured_.flags = flags & (xplm_ShiftFlag | xplm_OptionAltFlag | xplm_ControlFlag);
            }
        }
        return;
    }
    const bool down = (flags & xplm_UpFlag) == 0;
    const bool ctrl = (flags & xplm_ControlFlag) != 0;
    io.AddKeyEvent(ImGuiMod_Ctrl, ctrl);
    io.AddKeyEvent(ImGuiMod_Shift, (flags & xplm_ShiftFlag) != 0);
    io.AddKeyEvent(ImGuiMod_Alt, (flags & xplm_OptionAltFlag) != 0);
    const ImGuiKey mapped = mapKey(static_cast<unsigned char>(virtualKey));
    if (mapped != ImGuiKey_None) {
        io.AddKeyEvent(mapped, down);
    }
    const unsigned char ch = static_cast<unsigned char>(key);
    if (down && !ctrl && ch >= 0x20 && ch < 0x7F) {
        io.AddInputCharacter(ch);
    }
}

void ImguiWindow::drawCallback(XPLMWindowID, void* refcon) {
    ImguiWindow* self = static_cast<ImguiWindow*>(refcon);
    if (self == nullptr) {
        return;
    }
    try {
        self->drawFrame();
    } catch (...) {
    }
}

int ImguiWindow::leftClickCallback(XPLMWindowID, int x, int y, XPLMMouseStatus status, void* refcon) {
    ImguiWindow* self = static_cast<ImguiWindow*>(refcon);
    if (self != nullptr) {
        self->mouse(0, x, y, status);
    }
    return 1;  // the window consumes clicks inside its own rectangle only; X-Plane never sends others
}

int ImguiWindow::rightClickCallback(XPLMWindowID, int x, int y, XPLMMouseStatus status, void* refcon) {
    ImguiWindow* self = static_cast<ImguiWindow*>(refcon);
    if (self != nullptr) {
        self->mouse(1, x, y, status);
    }
    return 1;
}

void ImguiWindow::keyCallback(XPLMWindowID, char key, XPLMKeyFlags flags, char virtualKey, void* refcon,
                              int losingFocus) {
    ImguiWindow* self = static_cast<ImguiWindow*>(refcon);
    if (self != nullptr) {
        self->keyEvent(key, flags, virtualKey, losingFocus);
    }
}

XPLMCursorStatus ImguiWindow::cursorCallback(XPLMWindowID, int, int, void*) {
    return xplm_CursorArrow;
}

int ImguiWindow::wheelCallback(XPLMWindowID, int, int, int wheel, int clicks, void* refcon) {
    ImguiWindow* self = static_cast<ImguiWindow*>(refcon);
    if (self != nullptr && self->context_ != nullptr) {
        ImGui::SetCurrentContext(self->context_);
        ImGuiIO& io = ImGui::GetIO();
        if (wheel == 0) {
            io.AddMouseWheelEvent(0.0f, static_cast<float>(clicks));
        } else {
            io.AddMouseWheelEvent(static_cast<float>(clicks), 0.0f);
        }
    }
    return 1;
}

}  // namespace mykam
