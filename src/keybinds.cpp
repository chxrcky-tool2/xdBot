
#include "includes.hpp"
#include "ui/record_layer.hpp"
#include "ui/game_ui.hpp"
#include "ui/clickbot_layer.hpp"
#include "ui/macro_editor.hpp"
#include "hacks/layout_mode.hpp"
#include "hacks/show_trajectory.hpp"

#include <Geode/modify/CCKeyboardDispatcher.hpp>
#include <Geode/modify/CCTouchDispatcher.hpp>


const std::vector<std::string> keybindIDs = {
    "open_menu", "toggle_recording", "toggle_playing",
    "toggle_speedhack", "toggle_frame_stepper", "step_frame",
    "toggle_noclip", "show_trajectory"
};

class $modify(CCKeyboardDispatcher) {
  bool dispatchKeyboardMSG(enumKeyCodes key, bool isKeyDown, bool isKeyRepeat, double timestamp) {
  
    auto& g = Global::get();

    int keyInt = static_cast<int>(key);
    if (g.allKeybinds.contains(keyInt) && !isKeyRepeat) {
      for (size_t i = 0; i < 6; i++) {
        if (std::find(g.keybinds[i].begin(), g.keybinds[i].end(), keyInt) != g.keybinds[i].end())
          g.heldButtons[i] = isKeyDown;
      }
    }

    // if (key == enumKeyCodes::KEY_L && !isKeyRepeat && isKeyDown) {
    // }

    // if (key == enumKeyCodes::KEY_F && !isKeyRepeat && isKeyDown && PlayLayer::get()) {
    //   log::debug("POS DEBUG {}", PlayLayer::get()->m_player1->getPosition());
    //   log::debug("POS2 DEBUG {}", PlayLayer::get()->m_player2->getPosition());
    // }


    // if (key == enumKeyCodes::KEY_J && !isKeyRepeat && isKeyDown && PlayLayer::get()) {
    //   std::string str = ZipUtils::decompressString(PlayLayer::get()->m_level->m_levelString.c_str(), true, 0);
    //   log::debug("{}", str);
    // }

    return CCKeyboardDispatcher::dispatchKeyboardMSG(key, isKeyDown, isKeyRepeat, timestamp);
  }
};


namespace keybinds {

  struct ActionID {};

};


using namespace keybinds;

void onKeybind(bool down, ActionID id) {

}

$execute{

}
