#include "xd.labels.hpp"
#include "xd_menu.hpp"

#include <imgui-cocos.hpp>

#include <algorithm>

namespace {

constexpr float kDefaultOpacity = 0.5f;
constexpr float kDefaultScale = 1.25f;

struct Tally {
    input const* data = nullptr;
    size_t index = 0;
    int inputs = 0;
    int releases = 0;
    bool hasSignature = false;
    input signature;
};

Tally s_tally;

void resetTally(input const* data) {
    s_tally = Tally();
    s_tally.data = data;
}

XDLabels::Counts countInputs(std::vector<input> const& list, size_t target) {
    target = std::min(target, list.size());

    bool stale = list.data() != s_tally.data
        || s_tally.index > list.size()
        || (s_tally.hasSignature && s_tally.index > 0 && !(list[s_tally.index - 1] == s_tally.signature));

    if (stale) resetTally(list.data());

    while (s_tally.index < target) {
        if (list[s_tally.index].down) s_tally.inputs++;
        else s_tally.releases++;
        s_tally.index++;
    }

    while (s_tally.index > target) {
        s_tally.index--;
        if (list[s_tally.index].down) s_tally.inputs--;
        else s_tally.releases--;
    }

    s_tally.hasSignature = s_tally.index > 0;
    if (s_tally.hasSignature) s_tally.signature = list[s_tally.index - 1];

    return { s_tally.inputs, s_tally.releases };
}

bool savedFlag(char const* id) {
    return Mod::get()->getSavedValue<bool>(id);
}

}

bool XDLabels::overlayEnabled() {
    return Mod::get()->getSavedValue<bool>("xd_overlay_enabled", true);
}

float XDLabels::overlayOpacity() {
    return std::clamp(static_cast<float>(Mod::get()->getSavedValue<double>("xd_overlay_opacity", kDefaultOpacity)), 0.05f, 1.f);
}

float XDLabels::overlayScale() {
    return std::clamp(static_cast<float>(Mod::get()->getSavedValue<double>("xd_overlay_scale", kDefaultScale)), 0.5f, 6.f);
}

int XDLabels::overlayCorner() {
    return std::clamp(static_cast<int>(Mod::get()->getSavedValue<int64_t>("xd_overlay_corner", 0)), 0, 3);
}

bool XDLabels::overlayVisible() {
    if (!overlayEnabled()) return false;
    if (!PlayLayer::get()) return false;

    auto& g = Global::get();

    if (g.state == state::none) return false;
    if (savedFlag("macro_hide_labels")) return false;
    if (g.state == state::recording && savedFlag("macro_hide_recording_label")) return false;
    if (g.state == state::playing && savedFlag("macro_hide_playing_label")) return false;

    return true;
}

XDLabels::Counts XDLabels::getCounts() {
    auto& g = Global::get();
    auto const& list = g.macro.inputs;

    size_t target = g.state == state::playing ? g.currentAction : list.size();
    return countInputs(list, target);
}

std::string XDLabels::getText() {
    auto& g = Global::get();
    Counts counts = getCounts();

    return fmt::format("{} {}/{}", g.state == state::recording ? "Recording" : "Playing", counts.inputs, counts.releases);
}

void XDLabels::drawOverlay() {
    if (!overlayVisible()) return;

    std::string text = getText();

    float alpha = overlayOpacity();
    float size = XDMenu::Ui::textSize() * overlayScale();
    float margin = size * 0.6f;

    ImVec2 display = ImGui::GetIO().DisplaySize;
    ImFont* font = ImGui::GetFont();
    ImVec2 textSize = font->CalcTextSizeA(size, FLT_MAX, 0.f, text.c_str());

    int corner = overlayCorner();
    bool right = corner == 0 || corner == 2;
    bool bottom = corner == 0 || corner == 1;

    ImVec2 pos = ImVec2(
        right ? display.x - margin - textSize.x : margin,
        bottom ? display.y - margin - textSize.y : margin
    );

    int a = static_cast<int>(255.f * alpha);
    float offset = std::max(1.f, size * 0.07f);

    ImDrawList* dl = ImGui::GetForegroundDrawList();
    dl->AddText(font, size, ImVec2(pos.x + offset, pos.y + offset), IM_COL32(0, 0, 0, a), text.c_str());
    dl->AddText(font, size, pos, IM_COL32(255, 255, 255, a), text.c_str());
}

void XDLabels::drawWindow() {
    namespace Ui = XDMenu::Ui;

    if (!Ui::beginWindow("Labels", nullptr, 4, 250.f)) { ImGui::End(); return; }

    Ui::sectionTitle("Labels");

    Ui::settingRow("Show Frame Label", "macro_show_frame_label");
    Ui::settingRow("Hide Playing Label", "macro_hide_playing_label");
    Ui::settingRow("Hide Recording Label", "macro_hide_recording_label");

    Ui::sectionTitle("Input Overlay");

    bool enabled = overlayEnabled();
    if (Ui::toggleRow("Input Overlay", &enabled))
        XDMenu::applySetting("xd_overlay_enabled", enabled);

    Counts counts = getCounts();
    ImGui::TextDisabled("Macro: %d inputs / %d releases", counts.inputs, counts.releases);

    if (!enabled) ImGui::BeginDisabled();

    Ui::smallLabel("Opacity");
    {
        float v = overlayOpacity();
        if (Ui::touchSlider("##overlay_opacity", &v, 0.05f, 1.f, "%.2fx"))
            Mod::get()->setSavedValue("xd_overlay_opacity", static_cast<double>(v));
    }

    Ui::smallLabel("Scale");
    {
        float v = overlayScale();
        if (Ui::touchSlider("##overlay_scale", &v, 0.5f, 6.f, "%.2fx"))
            Mod::get()->setSavedValue("xd_overlay_scale", static_cast<double>(v));
    }

    Ui::smallLabel("Position");
    {
        int corner = overlayCorner();
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::Combo("##overlay_corner", &corner, "Bottom Right\0Bottom Left\0Top Right\0Top Left\0"))
            Mod::get()->setSavedValue("xd_overlay_corner", static_cast<int64_t>(corner));
    }

    if (Ui::fullButton("Reset Overlay")) {
        Mod::get()->setSavedValue("xd_overlay_opacity", static_cast<double>(kDefaultOpacity));
        Mod::get()->setSavedValue("xd_overlay_scale", static_cast<double>(kDefaultScale));
        Mod::get()->setSavedValue("xd_overlay_corner", static_cast<int64_t>(0));
    }

    if (!enabled) ImGui::EndDisabled();

    ImGui::End();
}
