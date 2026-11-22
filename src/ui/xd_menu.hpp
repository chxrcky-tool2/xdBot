#pragma once

#include "../includes.hpp"

// ImGui replacement for the old cocos2d RecordLayer panel.
// Everything the panel used to do (record / play / settings pages / save / load / info)
// now lives in xd_menu.cpp. Other files only need the small API below.
namespace XDMenu {

    // Hooks the ImGui draw callback. Called once from $on_mod(Loaded).
    void init();

    // Menu windows (not the floating button)
    void open();
    void close();
    void toggle();
    bool isOpen();

    // Macro controls (used by keybinds, the macro loader, hacks, ...)
    void toggleRecording();
    void togglePlaying();

    // Writes the saved value and updates the cached Global flag + side effects.
    // This is the old RecordLayer::toggleSetting logic without the cocos toggler.
    void applySetting(std::string const& id, bool value);
    bool getSetting(std::string const& id);

}
