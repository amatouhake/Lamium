#pragma once
#include <algorithm>

namespace lamium::ui {
// The schematic save prompt (BACKLOG L-93, docs/demos/schematic.html): a
// panel centered over the world with both corners as − value + steppers, the
// name, "Include entities", a status line, then Clear area / Save / Cancel.
// GUI units; pure geometry.
struct SavePromptLayout {
    static constexpr float width = 340, pad = 7, rowHeight = 13, labelWidth = 56, step = 11, gap = 4;
    static constexpr float fieldHeight = 14, buttonWidth = 80, buttonHeight = 13, switchWidth = 22;
    float left = 0, top = 0;
    static SavePromptLayout at(float screenW, float screenH) {
        return {std::max(0.f, (screenW - width) / 2), std::max(0.f, (screenH - height()) / 2)};
    }
    static constexpr float height() { return pad + 14 + 2 * 16 + 12 + 17 + 12 + 13 + 12 + 17 + 36 + 10 + pad; }
    float inner() const { return width - 2 * pad; }
    float titleY() const { return top + pad; }
    float cornerY(int corner) const { return top + pad + 14 + 16 * corner; }
    float sizeY() const { return cornerY(1) + 17; }
    float fieldY() const { return sizeY() + 12; }
    float whereY() const { return fieldY() + 17; }
    float entitiesY() const { return whereY() + 12; }
    float statusY() const { return entitiesY() + 15; }
    float buttonY() const { return statusY() + 12; }
    float hintY() const { return buttonY() + 17; }
    float keysY() const { return hintY() + 36; } // three hint lines, 12 apart
    float cellWidth() const { return (inner() - labelWidth - 2 * gap) / 3; }
    float cellX(int axis) const { return left + pad + labelWidth + axis * (cellWidth() + gap); }
    float switchX() const { return left + width - pad - switchWidth; }
    float cancelX() const { return left + width - pad - buttonWidth; }
    float saveX() const { return cancelX() - 4 - buttonWidth; }
    float clearX() const { return left + pad; }
    enum class Part { None, Field, Minus, Plus, Value, Entities, Save, Cancel, Clear };
    struct Hit { Part part = Part::None; int corner = -1, axis = -1; };
    Hit hit(float x, float y) const {
        auto in = [&](float bx, float by, float bw, float bh) { return x >= bx && x < bx + bw && y >= by && y < by + bh; };
        for (int corner = 0; corner < 2; ++corner)
            for (int axis = 0; axis < 3; ++axis) {
                if (in(cellX(axis), cornerY(corner), step, rowHeight)) return {Part::Minus, corner, axis};
                if (in(cellX(axis) + cellWidth() - step, cornerY(corner), step, rowHeight)) return {Part::Plus, corner, axis};
                if (in(cellX(axis) + step, cornerY(corner), cellWidth() - 2 * step, rowHeight)) return {Part::Value, corner, axis};
            }
        if (in(left + pad, fieldY(), inner(), fieldHeight)) return {Part::Field};
        if (in(left + pad, entitiesY() - 1, inner(), rowHeight)) return {Part::Entities};
        if (in(saveX(), buttonY(), buttonWidth, buttonHeight)) return {Part::Save};
        if (in(cancelX(), buttonY(), buttonWidth, buttonHeight)) return {Part::Cancel};
        if (in(clearX(), buttonY(), buttonWidth, buttonHeight)) return {Part::Clear};
        return {};
    }
};
}
