#include "xd_menu.hpp"
#include "game_ui.hpp"
#include "macro_editor.hpp"
#include "clickbot_layer.hpp"
#include "../hacks/coin_finder.hpp"
#include "../hacks/show_trajectory.hpp"
#include "../hacks/clickbot.hpp"

#include <Geode/ui/GeodeUI.hpp>
#include <Geode/utils/web.hpp>
#include <Geode/utils/file.hpp>

#include <imgui-cocos.hpp>

#include <algorithm>
#include <cctype>
#include <ctime>
#include <sys/stat.h>

namespace fs = std::filesystem;

// =============================================================================================
//  Helpers + state
// =============================================================================================

namespace {

Mod* M() { return Global::get().mod; }

ImVec4 mul(ImVec4 c, float f, float a = -1.f) { return ImVec4(c.x * f, c.y * f, c.z * f, a < 0.f ? c.w : a); }
ImU32 u32(ImVec4 c) { return ImGui::ColorConvertFloat4ToU32(c); }

void notify(std::string const& text, NotificationIcon icon = NotificationIcon::Success) {
    Notification::create(text, icon)->show();
}

// ---- window visibility ------------------------------------------------------------------

bool s_initialized = false;
bool s_open = false;          // the menu windows (the floating button is always drawn)

bool s_showNoclip = false;
bool s_showTrajectory = false;
bool s_showMirror = false;
bool s_showAutoclicker = false;
bool s_showAutosave = false;
bool s_showSave = false;
bool s_showLoad = false;
bool s_showInfo = false;

// ---- look ---------------------------------------------------------------------------------

ImVec4 s_accent = ImVec4(0.54f, 0.42f, 1.00f, 1.00f);
float s_userScale = 0.f;      // 0 = automatic
float s_opacity = 0.94f;
float s_scale = 1.f;          // effective scale
float s_fontPx = 13.f;        // pixel size the font atlas was baked at (13 = ImGui default font)

float s_appliedScale = -1.f;
float s_appliedOpacity = -1.f;
ImVec4 s_appliedAccent = ImVec4(-1, -1, -1, -1);

// ---- floating button ----------------------------------------------------------------------

ImVec2 s_btnPos = ImVec2(-1.f, -1.f);
ImVec2 s_btnDragStart = ImVec2(0.f, 0.f);
bool s_btnDragging = false;

// ---- save window --------------------------------------------------------------------------

char s_saveName[128] = "";
char s_saveAuthor[64] = "";
char s_saveDesc[256] = "";
bool s_saveJson = false;
bool s_saveNeedsInit = true;

// ---- load window --------------------------------------------------------------------------

struct MacroEntry {
    fs::path path;
    std::string name;
    std::string ext;
    std::time_t date = 0;
};

std::vector<MacroEntry> s_macros;
bool s_loadAutosaves = false;
bool s_loadNeedsRefresh = true;
int s_confirmDelete = -1;
int s_confirmLoad = -1;
char s_search[64] = "";
geode::async::TaskHolder<geode::utils::file::PickResult> s_pickHolder;

// ---- misc ---------------------------------------------------------------------------------

bool savedBool(char const* id) { return M()->getSavedValue<bool>(id); }

float uiScaleAuto() {
    float h = ImGui::GetIO().DisplaySize.y;
    return std::clamp(h / 700.f, 0.85f, 2.5f);
}

bool ensureFolder(fs::path const& path) {
    std::error_code ec;
    if (fs::exists(path, ec)) return true;
    return geode::utils::file::createDirectoryAll(toAspPath(path)).isOk();
}

std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
    return s;
}

std::string sanitizeName(std::string name) {
    for (char& c : name) {
        if (std::string_view("\\/:*?\"<>|").find(c) != std::string_view::npos) c = '_';
    }
    return name;
}

// Finds "<base> (n)<ext>" that doesn't exist yet.
std::string uniquePath(std::string base, std::string const& ext) {
    std::string path = base + ext;
    int i = 1;
    while (fs::exists(path)) path = fmt::format("{} ({}){}", base, i++, ext);
    return path;
}

// =============================================================================================
//  Style (Megahack-ish: dark, flat, rounded, accent colour, full-width rows)
// =============================================================================================

void applyStyle() {
    ImGuiIO& io = ImGui::GetIO();
    ImGuiStyle& st = ImGui::GetStyle();

    s_scale = s_userScale > 0.01f ? s_userScale : uiScaleAuto();

    st = ImGuiStyle();
    st.WindowRounding = 8.f;
    st.ChildRounding = 6.f;
    st.FrameRounding = 6.f;
    st.PopupRounding = 6.f;
    st.ScrollbarRounding = 8.f;
    st.GrabRounding = 6.f;
    st.WindowBorderSize = 1.f;
    st.FrameBorderSize = 0.f;
    st.WindowPadding = ImVec2(8.f, 8.f);
    st.FramePadding = ImVec2(8.f, 6.f);
    st.ItemSpacing = ImVec2(6.f, 5.f);
    st.ItemInnerSpacing = ImVec2(6.f, 4.f);
    st.ScrollbarSize = 18.f;
    st.GrabMinSize = 20.f;
    st.WindowTitleAlign = ImVec2(0.5f, 0.5f);
    st.ScaleAllSizes(s_scale);

    // Layout metrics assume a ~13px font, so scale relative to what was actually baked.
    io.FontGlobalScale = s_scale * (13.f / s_fontPx);
    io.ConfigWindowsMoveFromTitleBarOnly = true; // body drags must not move windows (touch friendly)

    ImVec4* c = st.Colors;
    c[ImGuiCol_Text]                 = ImVec4(0.93f, 0.93f, 0.96f, 1.00f);
    c[ImGuiCol_TextDisabled]         = ImVec4(0.50f, 0.50f, 0.56f, 1.00f);
    c[ImGuiCol_WindowBg]             = ImVec4(0.065f, 0.065f, 0.085f, s_opacity);
    c[ImGuiCol_ChildBg]              = ImVec4(0.0f, 0.0f, 0.0f, 0.18f);
    c[ImGuiCol_PopupBg]              = ImVec4(0.08f, 0.08f, 0.10f, 0.98f);
    c[ImGuiCol_Border]               = mul(s_accent, 0.55f, 0.55f);
    c[ImGuiCol_BorderShadow]         = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_FrameBg]              = ImVec4(0.13f, 0.13f, 0.17f, 1.00f);
    c[ImGuiCol_FrameBgHovered]       = ImVec4(0.18f, 0.18f, 0.23f, 1.00f);
    c[ImGuiCol_FrameBgActive]        = mul(s_accent, 0.45f, 1.f);
    c[ImGuiCol_TitleBg]              = ImVec4(0.035f, 0.035f, 0.050f, 1.00f);
    c[ImGuiCol_TitleBgActive]        = ImVec4(0.035f, 0.035f, 0.050f, 1.00f);
    c[ImGuiCol_TitleBgCollapsed]     = ImVec4(0.035f, 0.035f, 0.050f, 0.85f);
    c[ImGuiCol_ScrollbarBg]          = ImVec4(0, 0, 0, 0.25f);
    c[ImGuiCol_ScrollbarGrab]        = mul(s_accent, 0.70f, 1.f);
    c[ImGuiCol_ScrollbarGrabHovered] = mul(s_accent, 0.85f, 1.f);
    c[ImGuiCol_ScrollbarGrabActive]  = s_accent;
    c[ImGuiCol_CheckMark]            = s_accent;
    c[ImGuiCol_SliderGrab]           = s_accent;
    c[ImGuiCol_SliderGrabActive]     = mul(s_accent, 1.15f, 1.f);
    c[ImGuiCol_Button]               = ImVec4(0.15f, 0.15f, 0.20f, 1.00f);
    c[ImGuiCol_ButtonHovered]        = mul(s_accent, 0.60f, 1.f);
    c[ImGuiCol_ButtonActive]         = s_accent;
    c[ImGuiCol_Header]               = mul(s_accent, 0.45f, 1.f);
    c[ImGuiCol_HeaderHovered]        = mul(s_accent, 0.60f, 1.f);
    c[ImGuiCol_HeaderActive]         = s_accent;
    c[ImGuiCol_Separator]            = ImVec4(0.20f, 0.20f, 0.27f, 1.00f);
    c[ImGuiCol_TextSelectedBg]       = mul(s_accent, 0.50f, 0.50f);

    s_appliedScale = s_userScale;
    s_appliedOpacity = s_opacity;
    s_appliedAccent = s_accent;
}

void updateStyleIfNeeded() {
    bool changed = s_appliedScale != s_userScale
        || s_appliedOpacity != s_opacity
        || s_appliedAccent.x != s_accent.x || s_appliedAccent.y != s_accent.y || s_appliedAccent.z != s_accent.z;

    // auto scale follows the display size
    if (!changed && s_userScale <= 0.01f && std::fabs(uiScaleAuto() - s_scale) > 0.001f) changed = true;

    if (changed) applyStyle();
}

void loadPrefs() {
    Mod* mod = M();

    if (!mod->setSavedValue("xd_ui_defaults", true)) {
        mod->setSavedValue("xd_ui_scale", 0.f);
        mod->setSavedValue("xd_ui_opacity", 0.94f);
        mod->setSavedValue("xd_ui_accent", ccc3(138, 107, 255));
        mod->setSavedValue("xd_hide_button", false);
        mod->setSavedValue("xd_btn_x", -1.f);
        mod->setSavedValue("xd_btn_y", -1.f);
    }

    s_userScale = static_cast<float>(mod->getSavedValue<double>("xd_ui_scale"));
    s_opacity = static_cast<float>(mod->getSavedValue<double>("xd_ui_opacity"));

    ccColor3B col = mod->getSavedValue<ccColor3B>("xd_ui_accent");
    s_accent = ImVec4(col.r / 255.f, col.g / 255.f, col.b / 255.f, 1.f);

    s_btnPos = ImVec2(
        static_cast<float>(mod->getSavedValue<double>("xd_btn_x")),
        static_cast<float>(mod->getSavedValue<double>("xd_btn_y"))
    );
}

void saveAccent() {
    M()->setSavedValue("xd_ui_accent", ccc3(
        static_cast<GLubyte>(std::clamp(s_accent.x, 0.f, 1.f) * 255.f),
        static_cast<GLubyte>(std::clamp(s_accent.y, 0.f, 1.f) * 255.f),
        static_cast<GLubyte>(std::clamp(s_accent.z, 0.f, 1.f) * 255.f)
    ));
}

// Optional: drop a "font.ttf" in the mod's resources folder for crisp text instead of the pixel font.
void loadFont() {
    ImGuiIO& io = ImGui::GetIO();
    fs::path ttf = toStdPath(M()->getResourcesDir()) / "font.ttf";

    std::error_code ec;
    if (fs::exists(ttf, ec)) {
        if (io.Fonts->AddFontFromFileTTF(ttf.string().c_str(), 26.f)) {
            s_fontPx = 26.f;
            return;
        }
    }
    s_fontPx = 13.f;
}

// =============================================================================================
//  Widgets
// =============================================================================================

// Full-width tappable row with a tick box on the right. Optional gear zone for sub settings.
bool toggleRow(char const* label, bool* value, bool* gearPressed = nullptr, bool disabled = false) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImGuiStyle& st = ImGui::GetStyle();

    float h = ImGui::GetFrameHeight() * 1.1f;
    float w = ImGui::GetContentRegionAvail().x;
    float gearW = gearPressed ? h : 0.f;
    ImVec2 p = ImGui::GetCursorScreenPos();

    ImGui::PushID(label);
    if (disabled) ImGui::BeginDisabled();

    bool pressed = ImGui::InvisibleButton("##row", ImVec2(w - gearW, h));
    bool hov = ImGui::IsItemHovered();
    bool held = ImGui::IsItemActive();

    bool gear = false;
    bool gearHov = false;
    bool gearHeld = false;
    if (gearPressed) {
        ImGui::SameLine(0.f, 0.f);
        gear = ImGui::InvisibleButton("##gear", ImVec2(gearW, h));
        gearHov = ImGui::IsItemHovered();
        gearHeld = ImGui::IsItemActive();
    }

    if (disabled) ImGui::EndDisabled();
    ImGui::PopID();

    float alpha = disabled ? 0.45f : 1.f;
    float rounding = st.FrameRounding;

    // row background
    ImVec4 bg = held ? mul(s_accent, 0.35f, 1.f) : hov ? ImVec4(0.17f, 0.17f, 0.22f, 1.f) : ImVec4(0.115f, 0.115f, 0.15f, 1.f);
    bg.w *= alpha;
    dl->AddRectFilled(p, ImVec2(p.x + w, p.y + h), u32(bg), rounding);

    // label
    ImVec4 tc = ImGui::GetStyle().Colors[ImGuiCol_Text];
    tc.w *= alpha;
    dl->AddText(ImVec2(p.x + st.FramePadding.x, p.y + (h - ImGui::GetTextLineHeight()) * 0.5f), u32(tc), label);

    // tick box
    float bs = h * 0.52f;
    ImVec2 b0 = ImVec2(p.x + w - gearW - st.FramePadding.x - bs, p.y + (h - bs) * 0.5f);
    ImVec2 b1 = ImVec2(b0.x + bs, b0.y + bs);
    if (*value) {
        ImVec4 a = s_accent; a.w *= alpha;
        dl->AddRectFilled(b0, b1, u32(a), bs * 0.28f);
        float t = bs * 0.14f;
        ImVec2 c1 = ImVec2(b0.x + bs * 0.24f, b0.y + bs * 0.52f);
        ImVec2 c2 = ImVec2(b0.x + bs * 0.43f, b0.y + bs * 0.72f);
        ImVec2 c3 = ImVec2(b0.x + bs * 0.78f, b0.y + bs * 0.28f);
        dl->AddLine(c1, c2, IM_COL32(255, 255, 255, int(255 * alpha)), t);
        dl->AddLine(c2, c3, IM_COL32(255, 255, 255, int(255 * alpha)), t);
    }
    else {
        dl->AddRect(b0, b1, IM_COL32(120, 120, 140, int(255 * alpha)), bs * 0.28f, 0, std::max(1.f, bs * 0.09f));
    }

    // gear zone (a chevron on a darker cap)
    if (gearPressed) {
        ImVec2 g0 = ImVec2(p.x + w - gearW, p.y);
        ImVec2 g1 = ImVec2(p.x + w, p.y + h);
        ImVec4 gb = gearHeld ? mul(s_accent, 0.55f, 1.f) : gearHov ? mul(s_accent, 0.35f, 1.f) : ImVec4(0.08f, 0.08f, 0.11f, 1.f);
        gb.w *= alpha;
        dl->AddRectFilled(g0, g1, u32(gb), rounding, ImDrawFlags_RoundCornersRight);

        ImVec2 m = ImVec2((g0.x + g1.x) * 0.5f, (g0.y + g1.y) * 0.5f);
        float s = h * 0.17f;
        ImU32 cc = IM_COL32(220, 220, 235, int(255 * alpha));
        dl->AddLine(ImVec2(m.x - s * 0.5f, m.y - s), ImVec2(m.x + s * 0.5f, m.y), cc, std::max(1.5f, h * 0.06f));
        dl->AddLine(ImVec2(m.x + s * 0.5f, m.y), ImVec2(m.x - s * 0.5f, m.y + s), cc, std::max(1.5f, h * 0.06f));

        if (gear) *gearPressed = true;
    }

    if (pressed && !disabled) {
        *value = !*value;
        return true;
    }
    return false;
}

// Row backed by a saved bool setting. Returns true when the user changed it.
bool settingRow(char const* label, char const* id, bool* gearPressed = nullptr, bool disabled = false) {
    bool v = savedBool(id);
    if (toggleRow(label, &v, gearPressed, disabled)) {
        XDMenu::applySetting(id, v);
        return true;
    }
    return false;
}

bool fullButton(char const* label, float widthFraction = 1.f) {
    float avail = ImGui::GetContentRegionAvail().x;
    float spacing = ImGui::GetStyle().ItemSpacing.x;
    float w = widthFraction >= 1.f ? avail : (avail - spacing) * widthFraction;
    return ImGui::Button(label, ImVec2(w, ImGui::GetFrameHeight() * 1.1f));
}

void sectionTitle(char const* text) {
    ImGui::Spacing();
    ImGui::TextColored(mul(s_accent, 1.f, 1.f), "%s", text);
    ImGui::Separator();
}

void smallLabel(char const* text) {
    ImGui::TextDisabled("%s", text);
}

// Window with a fixed width and auto height, cascaded on first use.
bool beginWindow(char const* title, bool* open, int slot, float width, bool floating = false) {
    ImVec2 disp = ImGui::GetIO().DisplaySize;
    float w = width * s_scale;
    ImVec2 pos;
    if (floating) pos = ImVec2((disp.x - w) * 0.5f, disp.y * 0.12f);
    else pos = ImVec2(12.f * s_scale + slot * (w + 10.f * s_scale), 12.f * s_scale);

    ImGui::SetNextWindowPos(pos, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(w, 0.f), ImGuiCond_Always);
    return ImGui::Begin(title, open, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings);
}

// =============================================================================================
//  Macro file helpers
// =============================================================================================

void refreshMacroList() {
    s_macros.clear();
    s_loadNeedsRefresh = false;
    s_confirmDelete = -1;
    s_confirmLoad = -1;

    fs::path dir = getPathSetting(s_loadAutosaves ? "autosaves_folder" : "macros_folder");
    std::error_code ec;
    if (!fs::exists(dir, ec)) return;

    for (auto const& entry : fs::directory_iterator(dir, ec)) {
        std::error_code ec2;
        if (!entry.is_regular_file(ec2)) continue;

        std::string ext = lower(entry.path().extension().string());
        if (ext != ".xd" && ext != ".gdr" && ext != ".json") continue;

        MacroEntry m;
        m.path = entry.path();
        m.ext = ext;
        m.name = entry.path().stem().string();
        if (ext == ".json" && lower(fs::path(m.name).extension().string()) == ".gdr")
            m.name = fs::path(m.name).stem().string();

        struct stat sb;
        if (stat(m.path.string().c_str(), &sb) == 0) m.date = sb.st_mtime;

        s_macros.push_back(std::move(m));
    }

    std::sort(s_macros.begin(), s_macros.end(), [](MacroEntry const& a, MacroEntry const& b) {
        return a.date > b.date;
    });
}

void loadMacroFile(fs::path const& path) {
    bool legacy = false;
    Macro macro = Macro::importFile(path, &legacy);

    if (macro.description == "fail") return notify("There was an error loading this macro.", NotificationIcon::Error);

    auto& g = Global::get();
    g.macro = macro;
    g.currentAction = 0;
    g.currentFrameFix = 0;
    g.restart = true;
    g.macro.canChangeFPS = false;
    g.macro.xdBotMacro = g.macro.botInfo.name == "xdBot";

    if (!PlayLayer::get() && g.state != state::playing)
        XDMenu::togglePlaying();
    else if (g.state == state::recording)
        XDMenu::toggleRecording();

    Macro::updateTPS();

    notify("Macro Loaded");
    if (legacy) notify("Old .xd macro: may not behave exactly the same.", NotificationIcon::Warning);
}

void importMacro() {
    geode::utils::file::FilePickOptions::Filter filter;
    filter.description = "Macro Files";
    filter.files = { "*.xd", "*.gdr", "*.json" };

    s_pickHolder.spawn(
        geode::utils::file::pick(geode::utils::file::PickMode::OpenFile, { dirs::getGameDir(), { filter } }),
        [](geode::utils::file::PickResult res) {
            if (!res.isOk() || !res.unwrap().has_value()) return;

            fs::path path = toStdPath(*res.unwrap());
            bool legacy = false;
            Macro macro = Macro::importFile(path, &legacy);
            if (macro.description == "fail") return notify("There was an error importing this macro.", NotificationIcon::Error);

            fs::path folder = getPathSetting("macros_folder");
            if (!ensureFolder(folder)) return notify("Couldn't open the macros folder.", NotificationIcon::Error);

            std::string out = uniquePath((folder / path.stem()).string(), ".xd");
            auto data = macro.exportData(false);

            std::ofstream f(out, std::ios::binary);
            if (!f) return notify("Couldn't write the imported macro.", NotificationIcon::Error);
            f.write(reinterpret_cast<char const*>(data.data()), data.size());
            f.close();

            s_loadNeedsRefresh = true;
            notify("Macro Imported");
            if (legacy) notify("Old .xd macro: may not behave exactly the same.", NotificationIcon::Warning);
        }
    );
}

void openLoadWindow(bool autosaves) {
    s_loadAutosaves = autosaves;
    s_loadNeedsRefresh = true;
    s_showLoad = true;
}

void openSaveWindow() {
    auto& g = Global::get();
    if (g.macro.inputs.empty()) return notify("You can't save an empty macro.", NotificationIcon::Error);

    fs::path folder = getPathSetting("macros_folder");
    if (!ensureFolder(folder)) return notify("There was an error getting the macros folder.", NotificationIcon::Error);

    s_saveNeedsInit = true;
    s_showSave = true;
}

// Legacy cocos popups (editor, button editor, clickbot) are opened from here; menu closes so they're usable.
void openLegacy(std::function<void()> fn) {
    s_open = false;
    Loader::get()->queueInMainThread(std::move(fn));
}

// =============================================================================================
//  Floating "xd" button
// =============================================================================================

void drawFloatingButton() {
    if (true) return;
    
    ImGuiIO& io = ImGui::GetIO();
    ImVec2 disp = io.DisplaySize;
    float d = 54.f * s_scale;
    float r = d * 0.5f;

    if (s_btnPos.x < 0.f || s_btnPos.y < 0.f)
        s_btnPos = ImVec2(14.f * s_scale, disp.y * 0.30f);

    s_btnPos.x = std::clamp(s_btnPos.x, 0.f, std::max(0.f, disp.x - d));
    s_btnPos.y = std::clamp(s_btnPos.y, 0.f, std::max(0.f, disp.y - d));

    ImGui::SetNextWindowPos(s_btnPos);
    ImGui::SetNextWindowSize(ImVec2(d, d));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));

    ImGui::Begin("##xd_fab", nullptr,
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav |
        ImGuiWindowFlags_NoScrollWithMouse);

    ImGui::InvisibleButton("##fab", ImVec2(d, d));
    bool active = ImGui::IsItemActive();
    bool hovered = ImGui::IsItemHovered();

    if (ImGui::IsItemActivated()) {
        s_btnDragStart = io.MousePos;
        s_btnDragging = false;
    }

    if (active) {
        float dx = io.MousePos.x - s_btnDragStart.x;
        float dy = io.MousePos.y - s_btnDragStart.y;
        if (!s_btnDragging && (dx * dx + dy * dy) > (8.f * s_scale) * (8.f * s_scale))
            s_btnDragging = true;

        if (s_btnDragging) {
            s_btnPos.x += io.MouseDelta.x;
            s_btnPos.y += io.MouseDelta.y;
        }
    }

    if (ImGui::IsItemDeactivated()) {
        if (s_btnDragging) {
            M()->setSavedValue("xd_btn_x", s_btnPos.x);
            M()->setSavedValue("xd_btn_y", s_btnPos.y);
        }
        else if (hovered) {
            XDMenu::toggle();
        }
        s_btnDragging = false;
    }

    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);

    // drawn on the foreground list so it follows the finger without a frame of lag
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    ImVec2 c = ImVec2(s_btnPos.x + r, s_btnPos.y + r);

    auto& g = Global::get();
    ImVec4 ring = s_accent;
    if (g.state == state::recording) ring = ImVec4(1.00f, 0.28f, 0.28f, 1.f);
    else if (g.state == state::playing) ring = ImVec4(0.30f, 0.90f, 0.45f, 1.f);

    float pulse = (active || s_open) ? 1.f : 0.80f;

    dl->AddCircleFilled(ImVec2(c.x, c.y + r * 0.06f), r * 1.04f, IM_COL32(0, 0, 0, 70), 48);           // soft shadow
    dl->AddCircleFilled(c, r, u32(ImVec4(0.055f, 0.055f, 0.075f, 0.92f * pulse + 0.06f)), 48);          // body
    dl->AddCircleFilled(c, r * 0.86f, u32(mul(ring, 0.18f, 0.55f)), 48);                                  // tint
    dl->AddCircle(c, r - 1.f, u32(ImVec4(ring.x, ring.y, ring.z, pulse)), 48, std::max(2.f, d * 0.05f)); // ring

    ImFont* font = ImGui::GetFont();
    float fs = r * 0.95f;
    ImVec2 ts = font->CalcTextSizeA(fs, FLT_MAX, 0.f, "xd");
    ImVec2 tp = ImVec2(c.x - ts.x * 0.5f, c.y - ts.y * 0.5f);
    ImU32 tc = IM_COL32(245, 245, 255, int(255 * pulse));
    float o = std::max(0.6f, fs * 0.035f);
    dl->AddText(font, fs, ImVec2(tp.x + o, tp.y), tc, "xd");   // faux bold
    dl->AddText(font, fs, tp, tc, "xd");
}

// =============================================================================================
//  Windows
// =============================================================================================

void drawMainWindow() {
    auto& g = Global::get();

    if (!beginWindow("xdBot", nullptr, 0, 250.f)) { ImGui::End(); return; }

    ImGui::TextDisabled("%s", xdBotVersion.c_str());

    if (g.frameOffset != 0) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 0.78f, 0.25f, 1.f));
        ImGui::TextWrapped("Frame offset: %d (recording / playing is shifted)", static_cast<int>(g.frameOffset));
        ImGui::PopStyleColor();
    }

    sectionTitle("Macro");

    bool rec = g.state == state::recording;
    if (toggleRow("Record", &rec)) XDMenu::toggleRecording();

    bool play = g.state == state::playing;
    if (toggleRow("Play", &play)) XDMenu::togglePlaying();

    ImGui::TextDisabled("Actions: %d", static_cast<int>(g.macro.inputs.size()));

    if (fullButton("Save", 0.5f)) openSaveWindow();
    ImGui::SameLine();
    if (fullButton("Load", 1.f)) openLoadWindow(false);

    if (fullButton("Edit", 0.5f)) {
        openLegacy([] { MacroEditLayer::open(true); });
    }
    ImGui::SameLine();
    if (fullButton("Info", 1.f)) s_showInfo = true;

    sectionTitle("More");

    if (fullButton("Autosaves", 0.5f)) openLoadWindow(true);
    ImGui::SameLine();
    if (fullButton("Buttons", 1.f)) {
        openLegacy([] { Interface::openButtonEditor(); });
    }

    if (fullButton("More Settings", 1.f)) {
        geode::openSettingsPopup(M(), false);
    }

    if (fullButton("Discord", 1.f)) {
        geode::utils::web::openLinkInBrowser("https://discord.gg/w6yvdzVzBd");
    }

    ImGui::End();
}

void drawHacksWindow() {
    auto& g = Global::get();
    Mod* mod = M();

    if (!beginWindow("Hacks", nullptr, 1, 270.f)) { ImGui::End(); return; }

    bool tpsLocked = g.state != state::none && !g.macro.inputs.empty();

    sectionTitle("Speed");

    // TPS bypass
    settingRow("TPS Bypass", "macro_tps_enabled", nullptr, tpsLocked);
    {
        float tps = static_cast<float>(mod->getSavedValue<double>("macro_tps"));
        if (tpsLocked) ImGui::BeginDisabled();
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::InputFloat("##tps", &tps, 1.f, 10.f, "%.3f TPS")) {
            tps = std::clamp(tps, 0.f, 999999.f);
            mod->setSavedValue("macro_tps", static_cast<double>(tps));
            g.tps = tps;
            g.leftOver = 0.f;
        }
        if (tpsLocked) ImGui::EndDisabled();
    }

    // Speedhack
    settingRow("Speedhack", "macro_speedhack_enabled");
    {
        float sh = geode::utils::numFromString<float>(mod->getSavedValue<std::string>("macro_speedhack")).unwrapOr(1.f);
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::InputFloat("##sh", &sh, 0.05f, 0.5f, "%.3fx")) {
            sh = std::clamp(sh, 0.01f, 10.f);
            mod->setSavedValue("macro_speedhack", Utils::getSimplifiedString(fmt::format("{:.3f}", sh)));
        }
    }

    // Seed
    settingRow("Seed", "macro_seed_enabled");
    {
        unsigned long long seed = geode::utils::numFromString<unsigned long long>(mod->getSavedValue<std::string>("macro_seed")).unwrapOr(1ull);
        unsigned long long one = 1ull, ten = 10ull;
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::InputScalar("##seed", ImGuiDataType_U64, &seed, &one, &ten)) {
            mod->setSavedValue("macro_seed", std::to_string(seed));
        }
    }

    // Respawn time
    settingRow("Respawn Time", "respawn_time_enabled");
    {
        double rt = mod->getSavedValue<double>("respawn_time");
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::InputDouble("##respawn", &rt, 0.1, 0.5, "%.2f sec")) {
            rt = std::clamp(rt, 0.0, 99.0);
            mod->setSavedValue("respawn_time", rt);
        }
    }

    sectionTitle("Player");

    {
        bool gear = false;
        settingRow("Noclip", "macro_noclip", &gear);
        if (gear) s_showNoclip = true;
    }
    {
        bool gear = false;
        settingRow("Show Trajectory", "macro_show_trajectory", &gear);
        if (gear) s_showTrajectory = true;
    }
    settingRow("Frame Stepper", "macro_frame_stepper");
    settingRow("Coin Finder", "macro_coin_finder");
    settingRow("Layout Mode", "macro_layout_mode");
    settingRow("Auto Safe Mode", "macro_auto_safe_mode");

    sectionTitle("Death / Respawn");

    settingRow("Instant Respawn", "macro_instant_respawn");
    settingRow("No Death Effect", "macro_no_death_effect");
    settingRow("No Respawn Flash", "macro_no_respawn_flash");

    sectionTitle("Misc");

    settingRow("Always Practice Fixes", "macro_always_practice_fixes");
    settingRow("Ignore Inputs", "macro_ignore_inputs");
    settingRow("Disable Shaders", "disable_shaders");
    settingRow("Instant Mirror Portal", "instant_mirror_portal");
    settingRow("No Mirror Portal", "no_mirror_portal");
    {
        bool gear = false;
        settingRow("Input Mirror", "p2_input_mirror", &gear);
        if (gear) s_showMirror = true;
    }

    ImGui::End();
}

void drawExtrasWindow() {
    if (!beginWindow("Extras", nullptr, 2, 250.f)) { ImGui::End(); return; }

    sectionTitle("Bots");

    {
        bool gear = false;
        settingRow("Clickbot", "clickbot_enabled", &gear);
        if (gear) openLegacy([] { ClickbotLayer::create()->show(); });
    }
    {
        bool gear = false;
        settingRow("Autoclicker", "autoclicker_enabled", &gear);
        if (gear) s_showAutoclicker = true;
    }
    {
        bool gear = false;
        settingRow("Auto Saving", "macro_auto_save", &gear);
        if (gear) s_showAutosave = true;
    }

    sectionTitle("Labels & Buttons");

    settingRow("Show Frame Label", "macro_show_frame_label");
    settingRow("Hide Playing Label", "macro_hide_playing_label");
    settingRow("Hide Recording Label", "macro_hide_recording_label");
    settingRow("Always Show Buttons", "macro_always_show_buttons");
    settingRow("Hide Speedhack Button", "macro_hide_speedhack");
    settingRow("Hide Stepper Button", "macro_hide_stepper");
    settingRow("Speedhack Audio", "macro_speedhack_audio");

    ImGui::End();
}

void drawMenuSettingsWindow() {
    if (!beginWindow("Menu", nullptr, 3, 250.f)) { ImGui::End(); return; }

    sectionTitle("Floating Button");

    {
        bool hidden = savedBool("xd_hide_button");
        bool shown = !hidden;
        if (toggleRow("Show \"xd\" Button", &shown)) {
            M()->setSavedValue("xd_hide_button", !shown);
            if (!shown) notify("Button hidden. Reopen via Geode > xdBot > Open Menu.", NotificationIcon::Warning);
        }
    }
    if (fullButton("Reset Button Position")) {
        s_btnPos = ImVec2(-1.f, -1.f);
        M()->setSavedValue("xd_btn_x", -1.f);
        M()->setSavedValue("xd_btn_y", -1.f);
    }

    sectionTitle("Appearance");

    smallLabel("UI Scale (0 = auto)");
    {
        float v = s_userScale;
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::SliderFloat("##scale", &v, 0.f, 3.f, v < 0.05f ? "Auto" : "%.2fx")) {
            s_userScale = v < 0.05f ? 0.f : std::max(v, 0.6f);
            M()->setSavedValue("xd_ui_scale", static_cast<double>(s_userScale));
        }
    }

    smallLabel("Window Opacity");
    {
        float v = s_opacity;
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::SliderFloat("##opacity", &v, 0.3f, 1.f, "%.2f")) {
            s_opacity = v;
            M()->setSavedValue("xd_ui_opacity", static_cast<double>(s_opacity));
        }
    }

    smallLabel("Accent Color");
    {
        float col[3] = { s_accent.x, s_accent.y, s_accent.z };
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::ColorEdit3("##accent", col, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel)) {
            s_accent = ImVec4(col[0], col[1], col[2], 1.f);
            saveAccent();
        }
    }

    ImGui::End();
}

// ---- sub windows ----------------------------------------------------------------------------

void drawNoclipWindow() {
    if (!beginWindow("Noclip", &s_showNoclip, 0, 220.f, true)) { ImGui::End(); return; }

    for (auto const& [label, id] : { std::pair{ "Player 1", "macro_noclip_p1" }, std::pair{ "Player 2", "macro_noclip_p2" } }) {
        bool v = savedBool(id);
        if (toggleRow(label, &v)) M()->setSavedValue(id, v);
    }

    ImGui::End();
}

void drawMirrorWindow() {
    if (!beginWindow("Input Mirror", &s_showMirror, 0, 220.f, true)) { ImGui::End(); return; }

    settingRow("Inverted", "p2_input_mirror_inverted");

    ImGui::End();
}

void drawTrajectoryWindow() {
    if (!beginWindow("Show Trajectory", &s_showTrajectory, 0, 260.f, true)) { ImGui::End(); return; }

    ShowTrajectory& t = ShowTrajectory::get();

    settingRow("Both Sides", "macro_trajectory_both_sides");

    smallLabel("Length");
    {
        int length = geode::utils::numFromString<int>(M()->getSavedValue<std::string>("trajectory_length")).unwrapOr(240);
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::InputInt("##tlen", &length, 10, 100)) {
            length = std::clamp(length, 1, 2560);
            t.length = length;
            M()->setSavedValue("trajectory_length", std::to_string(length));
        }
    }

    auto colorEdit = [&](char const* label, cocos2d::ccColor4F& color) {
        smallLabel(label);
        float col[3] = { color.r, color.g, color.b };
        ImGui::SetNextItemWidth(-FLT_MIN);
        std::string id = std::string("##") + label;
        if (ImGui::ColorEdit3(id.c_str(), col, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel)) {
            color.r = col[0];
            color.g = col[1];
            color.b = col[2];
            t.updateMergedColor();
            M()->setSavedValue("trajectory_color1", ShowTrajectory::ccc3BFromccc4F(t.color1));
            M()->setSavedValue("trajectory_color2", ShowTrajectory::ccc3BFromccc4F(t.color2));
        }
    };

    colorEdit("Color 1", t.color1);
    colorEdit("Color 2", t.color2);

    if (fullButton("Restore Defaults")) {
        t.color1 = ccc4FFromccc3B(ccc3(74, 226, 85));
        t.color2 = ccc4FFromccc3B(ccc3(130, 8, 8));
        t.length = 240;
        t.updateMergedColor();
        M()->setSavedValue("trajectory_color1", ccc3(74, 226, 85));
        M()->setSavedValue("trajectory_color2", ccc3(130, 8, 8));
        M()->setSavedValue("trajectory_length", std::to_string(240));
    }

    ImGui::End();
}

void drawAutoclickerWindow() {
    auto& g = Global::get();

    if (!beginWindow("Autoclicker", &s_showAutoclicker, 0, 280.f, true)) { ImGui::End(); return; }

    float dt = 1.f / Global::getTPS();

    auto player = [&](char const* title, char const* toggleId, bool& flag, int& hold, int& release, char const* holdId, char const* releaseId, int idx) {
        ImGui::PushID(idx);
        sectionTitle(title);

        bool on = flag;
        if (toggleRow("Enabled", &on)) {
            flag = on;
            M()->setSavedValue(toggleId, on);
        }

        if (!flag) ImGui::BeginDisabled();

        smallLabel("Hold For (frames)");
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::InputInt("##hold", &hold, 1, 10)) {
            hold = std::max(hold, 0);
            M()->setSavedValue(holdId, hold);
        }
        ImGui::TextDisabled("%.3fs", dt * hold);

        smallLabel("Release For (frames)");
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::InputInt("##release", &release, 1, 10)) {
            release = std::max(release, 0);
            M()->setSavedValue(releaseId, release);
        }
        ImGui::TextDisabled("%.3fs", dt * release);

        if (!flag) ImGui::EndDisabled();
        ImGui::PopID();
    };

    player("Player 1", "autoclicker_p1", g.autoclickerP1, g.holdFor, g.releaseFor, "autoclicker_hold_for", "autoclicker_release_for", 1);
    player("Player 2", "autoclicker_p2", g.autoclickerP2, g.holdFor2, g.releaseFor2, "autoclicker_hold_for2", "autoclicker_release_for2", 2);

    ImGui::End();
}

void drawAutosaveWindow() {
    auto& g = Global::get();

    if (!beginWindow("Auto Saving", &s_showAutosave, 0, 260.f, true)) { ImGui::End(); return; }

    {
        bool on = g.autosaveIntervalEnabled;
        if (toggleRow("Save Every Interval", &on)) {
            g.autosaveIntervalEnabled = on;
            M()->setSavedValue("autosave_interval_enabled", on);
        }
    }

    if (!g.autosaveIntervalEnabled) ImGui::BeginDisabled();
    smallLabel("Interval (minutes)");
    {
        float mins = geode::utils::numFromString<float>(M()->getSavedValue<std::string>("autosave_interval")).unwrapOr(0.f);
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::InputFloat("##interval", &mins, 1.f, 5.f, "%.2f")) {
            mins = std::clamp(mins, 0.f, 9999.f);
            g.autosaveInterval = static_cast<int>(mins * 60);
            M()->setSavedValue("autosave_interval", Utils::getSimplifiedString(fmt::format("{:.2f}", mins)));
        }
    }
    if (!g.autosaveIntervalEnabled) ImGui::EndDisabled();

    {
        bool v = savedBool("autosave_checkpoint_enabled");
        if (toggleRow("Save On Checkpoint", &v)) M()->setSavedValue("autosave_checkpoint_enabled", v);
    }
    {
        bool v = savedBool("autosave_levelend_enabled");
        if (toggleRow("Save On Level End", &v)) M()->setSavedValue("autosave_levelend_enabled", v);
    }

    if (fullButton("Open Autosaves")) openLoadWindow(true);

    ImGui::End();
}

void drawSaveWindow() {
    auto& g = Global::get();

    if (s_saveNeedsInit) {
        s_saveNeedsInit = false;

        std::string level = g.macro.levelInfo.name.empty() ? std::string("macro") : g.macro.levelInfo.name;
        std::string user = GJAccountManager::sharedState() ? std::string(GJAccountManager::sharedState()->m_username) : std::string();

        std::snprintf(s_saveName, sizeof(s_saveName), "%s", sanitizeName(level).c_str());
        std::snprintf(s_saveAuthor, sizeof(s_saveAuthor), "%s", user.c_str());
        s_saveDesc[0] = '\0';
        s_saveJson = false;
    }

    if (!beginWindow("Save Macro", &s_showSave, 0, 300.f, true)) { ImGui::End(); return; }

    smallLabel("Name");
    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::InputText("##name", s_saveName, sizeof(s_saveName));

    smallLabel("Author (optional)");
    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::InputText("##author", s_saveAuthor, sizeof(s_saveAuthor));

    smallLabel("Description (optional)");
    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::InputText("##desc", s_saveDesc, sizeof(s_saveDesc));

    toggleRow("Readable JSON inside .xd", &s_saveJson);

    ImGui::TextDisabled("Saved as .xd - %d actions", static_cast<int>(g.macro.inputs.size()));

    if (fullButton("Save")) {
        std::string name = sanitizeName(s_saveName);
        if (name.empty()) {
            notify("Give a name to the macro.", NotificationIcon::Warning);
        }
        else {
            fs::path path = getPathSetting("macros_folder") / name;
            int result = Macro::save(s_saveAuthor, s_saveDesc, path.string(), s_saveJson);

            if (result != 0) {
                notify("There was an error saving the macro. ID: " + std::to_string(result), NotificationIcon::Error);
            }
            else {
                notify("Macro Saved");
                s_showSave = false;
                s_loadNeedsRefresh = true;
            }
        }
    }

    ImGui::End();
}

void drawLoadWindow() {
    if (s_loadNeedsRefresh) refreshMacroList();

    char const* title = s_loadAutosaves ? "Autosaves" : "Load Macro";
    if (!beginWindow(title, &s_showLoad, 0, 340.f, true)) { ImGui::End(); return; }

    if (fullButton(s_loadAutosaves ? "Show Macros" : "Show Autosaves", 0.5f)) {
        s_loadAutosaves = !s_loadAutosaves;
        s_loadNeedsRefresh = true;
    }
    ImGui::SameLine();
    if (fullButton("Refresh", 1.f)) s_loadNeedsRefresh = true;

    if (fullButton("Import", 0.5f)) importMacro();
    ImGui::SameLine();
    if (fullButton("Open Folder", 1.f)) {
        fs::path dir = getPathSetting(s_loadAutosaves ? "autosaves_folder" : "macros_folder");
        if (ensureFolder(dir)) geode::utils::file::openFolder(toAspPath(dir));
    }

    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::InputTextWithHint("##search", "Search", s_search, sizeof(s_search));

    std::string needle = lower(s_search);
    auto& g = Global::get();

    float listH = std::min(ImGui::GetIO().DisplaySize.y * 0.45f, 360.f * s_scale);
    ImGui::BeginChild("##list", ImVec2(0.f, listH), true);

    int shown = 0;
    for (int i = 0; i < static_cast<int>(s_macros.size()); i++) {
        MacroEntry const& m = s_macros[i];
        if (!needle.empty() && lower(m.name).find(needle) == std::string::npos) continue;
        shown++;

        ImGui::PushID(i);

        char date[32] = "";
        if (m.date) {
            std::tm* tm = std::localtime(&m.date);
            if (tm) std::strftime(date, sizeof(date), "%Y-%m-%d %H:%M", tm);
        }

        ImGui::TextUnformatted(m.name.c_str());
        ImGui::TextDisabled("%s  %s", m.ext.c_str(), date);

        bool confirmLoad = s_confirmLoad == i;
        bool confirmDelete = s_confirmDelete == i;

        if (fullButton(confirmLoad ? "Replace?" : "Load", 0.5f)) {
            if (confirmLoad || g.macro.inputs.empty()) {
                loadMacroFile(m.path);
                s_confirmLoad = -1;
            }
            else {
                s_confirmLoad = i;
                s_confirmDelete = -1;
            }
        }
        ImGui::SameLine();
        if (fullButton(confirmDelete ? "Sure?" : "Delete", 1.f)) {
            if (confirmDelete) {
                std::error_code ec;
                fs::remove(m.path, ec);
                if (ec) notify("There was an error deleting this macro.", NotificationIcon::Error);
                else notify("Macro Deleted");
                s_loadNeedsRefresh = true;
            }
            else {
                s_confirmDelete = i;
                s_confirmLoad = -1;
            }
        }

        ImGui::Separator();
        ImGui::PopID();

        if (s_loadNeedsRefresh) break; // list changed under us
    }

    if (shown == 0) ImGui::TextDisabled(s_loadAutosaves ? "No Autosaves" : "No Macros");

    ImGui::EndChild();
    ImGui::End();
}

void drawInfoWindow() {
    auto& g = Global::get();
    Macro& m = g.macro;

    if (!beginWindow("Current Macro", &s_showInfo, 0, 300.f, true)) { ImGui::End(); return; }

    auto line = [](char const* k, std::string const& v) {
        ImGui::TextDisabled("%s", k);
        ImGui::SameLine();
        ImGui::TextUnformatted(v.c_str());
    };

    float duration = (!m.inputs.empty() && m.framerate > 0.f) ? m.inputs.back().frame / m.framerate : 0.f;

    sectionTitle("General");
    line("Author:", m.author.empty() ? "N/A" : m.author);
    line("Description:", m.description.empty() ? "N/A" : m.description);
    line("Framerate:", Utils::getSimplifiedString(fmt::format("{:.3f}", m.framerate)));
    line("Duration:", fmt::format("{:.2f}s", duration));
    line("Game Version:", m.gameVersion != 0 ? Utils::getSimplifiedString(fmt::format("{:.3f}", m.gameVersion)) : std::string("N/A"));
    line("Seed:", m.seed != 0 ? std::to_string(m.seed) : std::string("N/A"));
    line("Used LDM:", m.ldm ? "true" : "false");
    line("Level:", fmt::format("{} ({})", m.levelInfo.name.empty() ? std::string("N/A") : m.levelInfo.name, static_cast<int>(m.levelInfo.id)));
    line("Bot:", fmt::format("{} {}", m.botInfo.name, m.botInfo.version));

    int counts[2][3][2] = {};
    for (auto const& in : m.inputs) {
        if (in.button < 1 || in.button > 3) continue;
        counts[in.player2 ? 1 : 0][in.button - 1][in.down ? 1 : 0]++;
    }

    sectionTitle("Inputs");
    line("Total:", std::to_string(m.inputs.size()));
    line("Frame fixes:", std::to_string(m.frameFixes.size()));

    char const* names[3] = { "Jump", "Left", "Right" };
    for (int p = 0; p < 2; p++) {
        for (int b = 0; b < 3; b++) {
            if (counts[p][b][0] + counts[p][b][1] == 0) continue;
            line(fmt::format("P{} {}:", p + 1, names[b]).c_str(), fmt::format("{} down / {} up", counts[p][b][1], counts[p][b][0]));
        }
    }

    ImGui::End();
}

void drawAll() {
    updateStyleIfNeeded();

    drawFloatingButton();

    if (!s_open) return;

    drawMainWindow();
    drawHacksWindow();
    drawExtrasWindow();
    drawMenuSettingsWindow();

    if (s_showNoclip) drawNoclipWindow();
    if (s_showMirror) drawMirrorWindow();
    if (s_showTrajectory) drawTrajectoryWindow();
    if (s_showAutoclicker) drawAutoclickerWindow();
    if (s_showAutosave) drawAutosaveWindow();
    if (s_showSave) drawSaveWindow();
    if (s_showLoad) drawLoadWindow();
    if (s_showInfo) drawInfoWindow();
}

} // namespace

// =============================================================================================
//  Public API
// =============================================================================================

void XDMenu::init() {
    if (s_initialized) return;
    s_initialized = true;

    loadPrefs();

    ImGuiCocos::get().setup([] {
        loadFont();
        applyStyle();
    }).draw([] {
        drawAll();
    });

    ImGuiCocos::get().setVisible(true);
}

void XDMenu::open() { s_open = true; }
void XDMenu::close() { s_open = false; }
void XDMenu::toggle() { s_open = !s_open; }
bool XDMenu::isOpen() { return s_open; }

bool XDMenu::getSetting(std::string const& id) {
    return M()->getSavedValue<bool>(id);
}

void XDMenu::toggleRecording() {
    auto& g = Global::get();

    if (Global::hasIncompatibleMods()) return;

    g.state = g.state == state::recording ? state::none : state::recording;

    if (g.state == state::recording) {
        g.currentAction = 0;
        g.currentFrameFix = 0;
        g.restart = true;

        PlayLayer* pl = PlayLayer::get();
        if (pl && !pl->m_isPaused)
            pl->pauseGame(false);
    }

    Interface::updateLabels();
    Interface::updateButtons();
    Macro::updateTPS();

    g.lastAutoSaveMS = std::chrono::steady_clock::now();
}

void XDMenu::togglePlaying() {
    auto& g = Global::get();

    if (Global::hasIncompatibleMods()) return;

    g.state = g.state == state::playing ? state::none : state::playing;

    if (g.state == state::playing) {
        g.currentAction = 0;
        g.currentFrameFix = 0;

        g.macro.xdBotMacro = g.macro.botInfo.name == "xdBot";

        PlayLayer* pl = PlayLayer::get();
        if (pl) {
            if (!pl->m_isPaused && !pl->m_levelEndAnimationStarted)
                pl->m_levelSettings->m_platformerMode ? pl->resetLevelFromStart() : pl->resetLevel();
            else
                g.restart = true;
        }
    }

    Interface::updateLabels();
    Interface::updateButtons();
    Macro::updateTPS();
}

// Port of the old RecordLayer::toggleSetting without the cocos toggler.
void XDMenu::applySetting(std::string const& id, bool value) {
    auto& g = Global::get();

    g.mod->setSavedValue(id, value);

    // Some of these get checked every frame so they're cached in Global.
    if (id == "macro_seed_enabled") g.seedEnabled = value;
    if (id == "macro_speedhack_enabled") g.speedhackEnabled = value;
    if (id == "macro_speedhack_audio") g.speedhackAudio = value;
    if (id == "p2_input_mirror") g.p2mirror = value;
    if (id == "clickbot_enabled") g.clickbotEnabled = value;
    if (id == "clickbot_playing_only") g.clickbotOnlyPlaying = value;
    if (id == "clickbot_holding_only") g.clickbotOnlyHolding = value;
    if (id == "macro_tps_enabled") g.tpsEnabled = value;
    if (id == "autoclicker_enabled") g.autoclicker = value;
    if (id == "disable_shaders") g.disableShaders = value;
    if (id == "macro_auto_save") g.autosaveEnabled = value;
    if (id == "macro_trajectory_both_sides") g.trajectoryBothSides = value;

    if (id == "macro_show_trajectory") {
        g.showTrajectory = value;
        if (!value) ShowTrajectory::trajectoryOff();
    }

    if (id == "macro_coin_finder") {
        g.coinFinder = value;
        if (!value) CoinFinder::finderOff();
    }

    if (id == "macro_show_frame_label") {
        g.frameLabel = value;
        Interface::updateLabels();
    }

    if (id == "macro_frame_stepper") {
        g.frameStepper = value;
        Interface::updateButtons();
    }

    if (id == "clickbot_enabled" || id == "clickbot_playing_only")
        Clickbot::updateSounds();

    if (id == "macro_hide_recording_label" || id == "macro_hide_playing_label")
        Interface::updateLabels();

    if (id == "macro_hide_speedhack" || id == "macro_hide_stepper" || id == "macro_always_show_buttons")
        Interface::updateButtons();
}

$on_mod(Loaded) {
    XDMenu::init();
}
