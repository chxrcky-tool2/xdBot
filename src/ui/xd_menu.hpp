#pragma once

#include "../includes.hpp"

namespace XDMenu {

    void init();

    void open();
    void close();
    void toggle();
    bool isOpen();

    void toggleRecording();
    void togglePlaying();

    void applySetting(std::string const& id, bool value);
    bool getSetting(std::string const& id);

    namespace Ui {

        bool toggleRow(char const* label, bool* value, bool* gearPressed = nullptr, bool disabled = false);
        bool settingRow(char const* label, char const* id, bool* gearPressed = nullptr, bool disabled = false);
        bool fullButton(char const* label, float widthFraction = 1.f);
        void sectionTitle(char const* text);
        void smallLabel(char const* text);
        bool beginWindow(char const* title, bool* open, int slot, float width, bool floating = false);
        bool touchSlider(char const* id, float* value, float lo, float hi, char const* format, bool* released = nullptr);
        float textSize();

    }

}
