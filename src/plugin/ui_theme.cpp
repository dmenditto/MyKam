#include "ui_theme.h"

#include <array>
#include <fstream>
#include <vector>

#include "xplm_includes.h"

namespace mykam::theme {

namespace {

const Fonts* g_active = nullptr;
bool g_warned = false;  // one font warning per X-Plane session

// Glyph ranges must outlive the atlas build, so they are static.
const ImWchar kLatinRange[] = {0x0020, 0x00FF, 0};
const ImWchar kDigitRange[] = {0x0020, 0x0020, 0x0030, 0x0039, 0};

const ImWchar* iconRange() {
    // One pair per icon actually used: the atlas holds nothing else of the 2000+ icons in the font.
    static std::vector<ImWchar> range;
    if (range.empty()) {
        for (const Icon& icon : kAllIcons) {
            range.push_back(static_cast<ImWchar>(icon.codepoint));
            range.push_back(static_cast<ImWchar>(icon.codepoint));
        }
        range.push_back(0);
    }
    return range.data();
}

// True when the file opens and starts like a TrueType font (stb_truetype would otherwise assert).
bool looksLikeTrueType(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return false;
    }
    char magic[4] = {};
    file.read(magic, 4);
    if (file.gcount() != 4) {
        return false;
    }
    const bool isTtf = magic[0] == 0 && magic[1] == 1 && magic[2] == 0 && magic[3] == 0;
    const bool isTrue = magic[0] == 't' && magic[1] == 'r' && magic[2] == 'u' && magic[3] == 'e';
    return isTtf || isTrue;
}

ImFont* addFont(ImGuiIO& io, const std::string& path, float pixels, const ImWchar* ranges, std::string& missing) {
    if (!looksLikeTrueType(path)) {
        if (missing.find(path) == std::string::npos) {
            missing += (missing.empty() ? "" : ", ") + path;
        }
        return nullptr;
    }
    ImFontConfig config;
    config.OversampleH = 2;  // crisp text
    config.OversampleV = 2;
    config.PixelSnapH = false;
    return io.Fonts->AddFontFromFileTTF(path.c_str(), pixels, &config, ranges);
}

}  // namespace

Fonts buildFonts(ImGuiIO& io, const std::string& fontDirectory, float atlasScale) {
    const float k = atlasScale >= 2.0f ? 2.0f : 1.0f;
    io.FontGlobalScale = 1.0f / k;
    const std::string dir = fontDirectory.empty() ? std::string() : fontDirectory + "/";
    // The post-build step copies the TTFs flat into <plugin>/fonts.
    const std::string flatRegular = dir + "Inter-Regular.ttf";
    const std::string flatSemibold = dir + "Inter-SemiBold.ttf";
    const std::string flatIcons = dir + "MaterialIcons-Regular.ttf";

    Fonts out;
    std::string missing;
    out.body = addFont(io, flatRegular, theme::kFontBody * k, kLatinRange, missing);  // first font = the default
    if (out.body == nullptr) {
        // No Inter: the whole window runs on the built-in font.
        ImFontConfig fallback;
        fallback.SizePixels = theme::kFontBody * k;
        ImFont* builtin = io.Fonts->AddFontDefault(&fallback);
        out.body = builtin;
        out.small = builtin;
        out.card = builtin;
        out.section = builtin;
        out.number = builtin;
        out.iconSmall = builtin;
        out.iconMedium = builtin;
        out.iconLarge = builtin;
        out.hasIcons = false;
        out.fellBack = true;
        if (!g_warned) {
            g_warned = true;
            const std::string line = "[MyKam] warning: font file missing or unreadable (" + missing +
                                     "), using the default ImGui font\n";
            XPLMDebugString(line.c_str());
        }
        return out;
    }
    out.small = addFont(io, flatRegular, theme::kFontSmall * k, kLatinRange, missing);
    out.card = addFont(io, flatSemibold, theme::kFontCard * k, kLatinRange, missing);
    out.section = addFont(io, flatSemibold, theme::kFontSection * k, kLatinRange, missing);
    out.number = addFont(io, flatSemibold, theme::kFontNumber * k, kDigitRange, missing);
    out.iconSmall = addFont(io, flatIcons, 14.0f * k, iconRange(), missing);
    out.iconMedium = addFont(io, flatIcons, 18.0f * k, iconRange(), missing);
    out.iconLarge = addFont(io, flatIcons, 24.0f * k, iconRange(), missing);
    out.hasIcons = out.iconSmall != nullptr && out.iconMedium != nullptr && out.iconLarge != nullptr;
    // A missing SemiBold file falls back to Regular at the same size; a missing icon file to plain letters.
    if (out.small == nullptr) out.small = out.body;
    if (out.card == nullptr) out.card = out.body;
    if (out.section == nullptr) out.section = out.body;
    if (out.number == nullptr) out.number = out.body;
    if (out.iconSmall == nullptr) out.iconSmall = out.small;
    if (out.iconMedium == nullptr) out.iconMedium = out.body;
    if (out.iconLarge == nullptr) out.iconLarge = out.card;
    if (!missing.empty()) {
        out.fellBack = true;
        if (!g_warned) {
            g_warned = true;
            const std::string line = "[MyKam] warning: font file missing or unreadable (" + missing +
                                     "), using fallback fonts for it\n";
            XPLMDebugString(line.c_str());
        }
    }
    return out;
}

void setActiveFonts(const Fonts* fonts) {
    g_active = fonts;
}

const Fonts& fonts() {
    static const Fonts empty;
    return g_active != nullptr ? *g_active : empty;
}

std::string iconText(const Icon& icon) {
    if (!fonts().hasIcons) {
        return icon.fallback;
    }
    std::string text;
    const unsigned c = icon.codepoint;
    if (c < 0x80) {
        text += static_cast<char>(c);
    } else if (c < 0x800) {
        text += static_cast<char>(0xC0 | (c >> 6));
        text += static_cast<char>(0x80 | (c & 0x3F));
    } else {
        text += static_cast<char>(0xE0 | (c >> 12));
        text += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
        text += static_cast<char>(0x80 | (c & 0x3F));
    }
    return text;
}

void applyStyle(ImGuiStyle& s) {
    ImGui::StyleColorsDark(&s);
    s.Alpha = 1.0f;
    s.WindowRounding = kWindowRounding;
    s.ChildRounding = kCardRounding;
    s.PopupRounding = kCardRounding;
    s.FrameRounding = 8.0f;
    s.GrabRounding = 8.0f;
    s.ScrollbarRounding = 4.0f;
    s.TabRounding = 8.0f;
    s.WindowBorderSize = 0.0f;
    s.ChildBorderSize = 0.0f;
    s.FrameBorderSize = 0.0f;
    s.PopupBorderSize = 1.0f;
    s.WindowPadding = ImVec2(12.0f, 10.0f);
    s.FramePadding = ImVec2(12.0f, 7.0f);
    s.ItemSpacing = ImVec2(10.0f, 8.0f);
    s.ItemInnerSpacing = ImVec2(8.0f, 6.0f);
    s.ScrollbarSize = 8.0f;
    s.GrabMinSize = 14.0f;

    ImVec4* c = s.Colors;
    c[ImGuiCol_Text] = vec4(kText);
    c[ImGuiCol_TextDisabled] = vec4(kMuted);
    c[ImGuiCol_WindowBg] = vec4(kGlass);
    c[ImGuiCol_ChildBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    c[ImGuiCol_PopupBg] = vec4(kGlassPopup);
    c[ImGuiCol_Border] = vec4(kBorder);
    c[ImGuiCol_BorderShadow] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    c[ImGuiCol_FrameBg] = vec4(IM_COL32(0x0A, 0x14, 0x1A, 255));
    c[ImGuiCol_FrameBgHovered] = vec4(IM_COL32(0x10, 0x20, 0x29, 255));
    c[ImGuiCol_FrameBgActive] = vec4(IM_COL32(0x13, 0x26, 0x30, 255));
    c[ImGuiCol_Button] = vec4(kCard);
    c[ImGuiCol_ButtonHovered] = vec4(kCardHover);
    c[ImGuiCol_ButtonActive] = vec4(kNavSelected);
    c[ImGuiCol_Header] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    c[ImGuiCol_HeaderHovered] = vec4(IM_COL32(0x2F, 0xC4, 0xC4, 46));
    c[ImGuiCol_HeaderActive] = vec4(IM_COL32(0x2F, 0xC4, 0xC4, 72));
    c[ImGuiCol_Separator] = vec4(kHairline);
    c[ImGuiCol_SeparatorHovered] = vec4(kBorder);
    c[ImGuiCol_SeparatorActive] = vec4(kCyan);
    c[ImGuiCol_ScrollbarBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    c[ImGuiCol_ScrollbarGrab] = vec4(IM_COL32(0x2F, 0xC4, 0xC4, 90));
    c[ImGuiCol_ScrollbarGrabHovered] = vec4(IM_COL32(0x2F, 0xC4, 0xC4, 150));
    c[ImGuiCol_ScrollbarGrabActive] = vec4(kCyan);
    c[ImGuiCol_TextSelectedBg] = vec4(IM_COL32(0x2F, 0xC4, 0xC4, 90));
    c[ImGuiCol_NavHighlight] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    c[ImGuiCol_ModalWindowDimBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.35f);
}

}  // namespace mykam::theme
