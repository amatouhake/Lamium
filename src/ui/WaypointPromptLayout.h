#pragma once
#include <algorithm>

namespace lamium::ui {
// The waypoint add prompt (BACKLOG L-60 step 5, docs/demos/waypoints.html):
// a small panel centered over the world, with the normal/temporary switch
// (L-261). GUI units; pure geometry.
struct WaypointPromptLayout {
    static constexpr float width = 214, height = 111, pad = 7;
    static constexpr float fieldHeight = 14, swatch = 11, swatchGap = 4, buttonWidth = 74, buttonHeight = 13;
    static constexpr float rowHeight = 13, switchWidth = 18;
    static constexpr int swatches = 12;
    float left = 0, top = 0;
    static WaypointPromptLayout at(float screenW, float screenH) {
        return {std::max(0.f, (screenW - width) / 2), std::max(0.f, (screenH - height) / 2)};
    }
    float inner() const { return width - 2 * pad; }
    float titleY() const { return top + pad; }
    float whereY() const { return top + pad + 12; }
    float fieldY() const { return top + pad + 25; }
    float swatchY() const { return top + pad + 43; }
    float temporaryY() const { return top + pad + 58; }
    float buttonY() const { return top + pad + 74; }
    float hintY() const { return top + pad + 91; }
    float switchX() const { return left + width - pad - switchWidth; }
    float swatchX(int i) const { return left + pad + i * (swatch + swatchGap); }
    float cancelX() const { return left + width - pad - buttonWidth; }
    float addX() const { return cancelX() - 4 - buttonWidth; }
    enum class Part { None, Field, Swatch, Temporary, Add, Cancel };
    struct Hit { Part part = Part::None; int swatch = -1; };
    Hit hit(float x, float y) const {
        auto in = [&](float bx, float by, float bw, float bh) { return x >= bx && x < bx + bw && y >= by && y < by + bh; };
        if (in(left + pad, fieldY(), inner(), fieldHeight)) return {Part::Field};
        for (int i = 0; i < swatches; ++i)
            if (in(swatchX(i) - swatchGap / 2, swatchY() - 1, swatch + swatchGap, swatch + 2)) return {Part::Swatch, i};
        if (in(left + pad, temporaryY(), inner(), rowHeight)) return {Part::Temporary};
        if (in(addX(), buttonY(), buttonWidth, buttonHeight)) return {Part::Add};
        if (in(cancelX(), buttonY(), buttonWidth, buttonHeight)) return {Part::Cancel};
        return {};
    }
};
}
