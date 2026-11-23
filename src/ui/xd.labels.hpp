#pragma once

#include "../includes.hpp"

namespace XDLabels {

    struct Counts {
        int inputs = 0;
        int releases = 0;
    };

    bool overlayEnabled();

    float overlayOpacity();
    float overlayScale();
    int overlayCorner();

    bool overlayVisible();

    Counts getCounts();
    std::string getText();

    void drawOverlay();
    void drawWindow();

}
