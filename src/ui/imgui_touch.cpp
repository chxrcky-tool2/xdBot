#include "../includes.hpp"

#include <Geode/modify/CCTouchDispatcher.hpp>

#include <imgui-cocos.hpp>
#include <imgui_internal.h>

// =============================================================================================
//  Touch bridge for the ImGui overlay (Android)
//
//  Problem: taps never reached ImGui, and since ImGui never had a mouse position, GD itself
//  received every tap (so "Noclip" in the menu pressed the Mega Hack search bar behind it).
//
//  Fix: look at every touch BEFORE GD does.
//    - if the finger lands on an ImGui window -> feed it to ImGui as a left mouse button and
//      hide it from GD (the touch never reaches the game / other mods)
//    - otherwise -> let it pass through to GD untouched
//  The "is it on an ImGui window" test is done right here with the window rectangles, so it
//  doesn't depend on io.WantCaptureMouse (which is always one frame late for touch input).
// =============================================================================================

namespace {

// cocos touch types
constexpr unsigned int kTouchBegan = 0;
constexpr unsigned int kTouchEnded = 2;
constexpr unsigned int kTouchCancelled = 3;

// id of the touch ImGui currently owns (-1 = none)
int s_capturedId = -1;

// cocos (GL, y up, design resolution) -> ImGui display space (y down)
ImVec2 toImGui(CCPoint const& gl) {
    ImVec2 disp = ImGui::GetIO().DisplaySize;
    CCSize win = CCDirector::get()->getWinSize();
    if (win.width <= 0.f || win.height <= 0.f) return ImVec2(-FLT_MAX, -FLT_MAX);
    return ImVec2(gl.x / win.width * disp.x, (1.f - gl.y / win.height) * disp.y);
}

// True if the point is on top of any visible ImGui window (including popups like the colour picker).
bool overImGuiWindow(ImVec2 p) {
    ImGuiContext* ctx = ImGui::GetCurrentContext();
    if (!ctx) return false;

    for (ImGuiWindow* w : ctx->Windows) {
        if (!w->Active && !w->WasActive) continue;
        if (w->Flags & (ImGuiWindowFlags_ChildWindow | ImGuiWindowFlags_NoInputs)) continue;
        if (w->Rect().Contains(p)) return true;
    }
    return false;
}

} // namespace

class $modify(XDTouchDispatcher, CCTouchDispatcher) {
    void touches(CCSet* set, CCEvent* event, unsigned int type) {
        if (!set || set->count() == 0 || !ImGui::GetCurrentContext() || ImGui::GetIO().DisplaySize.x <= 0.f)
            return CCTouchDispatcher::touches(set, event, type);

        ImGuiIO& io = ImGui::GetIO();
        std::vector<CCTouch*> mine;

        for (auto it = set->begin(); it != set->end(); ++it) {
            auto* touch = static_cast<CCTouch*>(*it);
            if (!touch) continue;

            ImVec2 p = toImGui(touch->getLocation());

            if (type == kTouchBegan) {
                if (s_capturedId == -1 && overImGuiWindow(p)) {
                    s_capturedId = touch->getID();
                    io.AddMousePosEvent(p.x, p.y);
                    io.AddMouseButtonEvent(0, true);
                    mine.push_back(touch);
                    log::debug("[xdBot] ImGui took touch at {:.0f}, {:.0f}", p.x, p.y);
                }
            }
            else if (touch->getID() == s_capturedId) {
                io.AddMousePosEvent(p.x, p.y);

                if (type == kTouchEnded || type == kTouchCancelled) {
                    io.AddMouseButtonEvent(0, false);
                    io.AddMousePosEvent(-FLT_MAX, -FLT_MAX); // no stuck hover highlight after lifting the finger
                    s_capturedId = -1;
                }
                mine.push_back(touch);
            }
        }

        // nothing was ImGui's -> GD handles it as usual
        if (mine.empty())
            return CCTouchDispatcher::touches(set, event, type);

        // everything was ImGui's -> GD never sees it
        if (static_cast<size_t>(set->count()) == mine.size())
            return;

        // mixed (multi-touch): only hide ImGui's finger from GD
        for (auto* t : mine) set->removeObject(t);
        CCTouchDispatcher::touches(set, event, type);
    }
};
