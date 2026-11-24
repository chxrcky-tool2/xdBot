#include <imgui-cocos.hpp>
#include <imgui_internal.h>

#include "../includes.hpp"

#include <Geode/modify/CCTouchDispatcher.hpp>

namespace {

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

// True if the point is on top of a visible, input-receiving ImGui window.
// Uses Active only (not WasActive) so a window that was just closed can't eat one more tap.
bool overImGuiWindow(ImVec2 p) {
    ImGuiContext* ctx = ImGui::GetCurrentContext();
    if (!ctx) return false;

    constexpr ImGuiWindowFlags skip =
        ImGuiWindowFlags_ChildWindow | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_Tooltip;

    for (ImGuiWindow* w : ctx->Windows) {
        if (!w || !w->Active || w->Hidden) continue;
        if (w->Flags & skip) continue;
        if (w->Rect().Contains(p)) return true;
    }
    return false;
}

} // namespace

class $modify(XDTouchDispatcher, CCTouchDispatcher) {
    void touches(CCSet* set, CCEvent* event, unsigned int type) {
        // Never throw away a touch-end for a finger ImGui owns, even if ImGui is in a weird state.
        bool const imguiReady = ImGui::GetCurrentContext() && ImGui::GetIO().DisplaySize.x > 0.f;

        if (!set || set->count() == 0 || !imguiReady) {
            s_capturedId = -1; // don't leave a capture dangling if ImGui went away
            return CCTouchDispatcher::touches(set, event, type);
        }

        ImGuiIO& io = ImGui::GetIO();

        // Touches GD should still see. Built as a NEW set: the engine owns `set` and the
        // CCTouch objects in it (its touch table holds raw pointers), so we never remove
        // from it.
        CCSet* forGame = nullptr;
        size_t total = 0, taken = 0;

        for (auto it = set->begin(); it != set->end(); ++it) {
            auto* touch = static_cast<CCTouch*>(*it);
            if (!touch) continue;
            ++total;

            bool imguiTouch = false;
            ImVec2 p = toImGui(touch->getLocation());

            if (type == kTouchBegan) {
                if (s_capturedId == -1 && overImGuiWindow(p)) {
                    s_capturedId = touch->getID();
                    io.AddMousePosEvent(p.x, p.y);
                    io.AddMouseButtonEvent(0, true);
                    imguiTouch = true;
                }
            }
            else if (touch->getID() == s_capturedId) {
                io.AddMousePosEvent(p.x, p.y);

                if (type == kTouchEnded || type == kTouchCancelled) {
                    io.AddMouseButtonEvent(0, false);
                    io.AddMousePosEvent(-FLT_MAX, -FLT_MAX); // no stuck hover after lifting the finger
                    s_capturedId = -1;
                }
                imguiTouch = true;
            }

            if (imguiTouch) {
                ++taken;
            }
            else {
                if (!forGame) forGame = CCSet::create(); // autoreleased
                forGame->addObject(touch);               // retains; balanced when forGame dies
            }
        }

        // Nothing was ImGui's -> hand GD the original, untouched set.
        if (taken == 0)
            return CCTouchDispatcher::touches(set, event, type);

        // Everything was ImGui's -> GD never sees it.
        if (!forGame)
            return;

        // Multi-touch: GD gets only its own fingers.
        CCTouchDispatcher::touches(forGame, event, type);
    }
};
