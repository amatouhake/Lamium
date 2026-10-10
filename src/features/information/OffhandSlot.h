#pragma once
#include <cmath>
#include <optional>

namespace lamium::information::offhand {
// The offhand slot beside the hotbar (BACKLOG L-75, docs/demos/offhand-slot.html).
// Pure: placement from the hotbar control the game laid out this frame.
struct Box { float x, y, w, h; };

// The vanilla hotbar is 22 units tall; a taller one (Pocket UI) scales the slot.
inline constexpr float hotbarUnits = 22, slotUnits = 22, gapUnits = 6;

// Left of the hotbar, same height and bottom line. Nothing when the hotbar's
// box is unusable or the slot would leave the screen: the game decides where
// the hotbar is, and an unexpected layout must not put a slot somewhere odd.
inline std::optional<Box> slotBox(Box hotbar, float screenW, float screenH) {
    for (float v : {hotbar.x, hotbar.y, hotbar.w, hotbar.h, screenW, screenH})
        if (!std::isfinite(v)) return std::nullopt;
    if (hotbar.w <= 0 || hotbar.h <= 0 || screenW <= 0 || screenH <= 0) return std::nullopt;
    float unit = hotbar.h / hotbarUnits;
    Box slot{hotbar.x - (gapUnits + slotUnits) * unit, hotbar.y, slotUnits * unit, slotUnits * unit};
    if (slot.x < 0 || slot.y < 0 || slot.y + slot.h > screenH + 1 || hotbar.x + hotbar.w > screenW + 1)
        return std::nullopt;
    return slot;
}
// The 16x16 icon sits where the hotbar puts it: one cap unit, then the
// 18x18 cell in the 20x22 slot image. Vertically the hotbar's icons sit one
// unit above center (seen against the hotbar, 2026-10-10).
inline Box iconBox(Box slot) {
    float unit = slot.h / slotUnits;
    return {slot.x + 3 * unit, slot.y + 2 * unit, 16 * unit, 16 * unit};
}
inline bool shown(bool enabled, bool holding, bool emptyFrame) { return enabled && (holding || emptyFrame); }
}
