#pragma once

#include "features/inspection/render/PreviewLayout.h"

// Pure durability-overlay math for the preview grid. Deliberately free of
// game types so it can be unit-tested without LeviLamina or the game.
//
// The bar mirrors the vanilla slot durability overlay: a black strip near the
// bottom of the 16x16 icon with a coloured fill whose width tracks remaining
// durability (green -> yellow -> red, the same hue ramp vanilla uses: full =
// green, half = yellow, empty = red). Geometry and rounding were measured
// against the real vanilla slot, see the constants below.
namespace lamium::inspection::render {

/// Remaining durability in [0, 1]: 1 = undamaged, 0 = no uses left.
/// Non-positive maxDamage cannot happen for a damageable item; treat it as
/// full so no caller can divide by zero.
[[nodiscard]] constexpr float durabilityRatio(int damageValue, int maxDamage) {
    if (maxDamage <= 0) {
        return 1.0f;
    }
    float const ratio = static_cast<float>(maxDamage - damageValue) / static_cast<float>(maxDamage);
    if (ratio <= 0.0f) {
        return 0.0f;
    }
    if (ratio >= 1.0f) {
        return 1.0f;
    }
    return ratio;
}

/// Vanilla shows the bar only once the item has taken damage. Undamaged
/// items (and non-damageables) get no bar.
[[nodiscard]] constexpr bool shouldShowDurabilityBar(bool damageable, int damageValue, int maxDamage) {
    return damageable && maxDamage > 0 && damageValue > 0;
}

struct DurabilityRgb {
    float r{0.0f};
    float g{0.0f};
    float b{0.0f};
};

/// Vanilla hue ramp: hue = ratio / 3 (red at 0, green at full), full
/// saturation and value. Implemented directly so the result is constexpr.
[[nodiscard]] constexpr DurabilityRgb durabilityColor(float ratio) {
    float h = ratio / 3.0f;
    if (h <= 0.0f) {
        return DurabilityRgb{1.0f, 0.0f, 0.0f};
    }
    if (h >= 1.0f / 3.0f) {
        return DurabilityRgb{0.0f, 1.0f, 0.0f};
    }
    float const h6 = h * 6.0f;
    int const   i  = static_cast<int>(h6);
    float const f  = h6 - static_cast<float>(i);
    // s = v = 1, so p = 0, q = 1 - f, t = f.
    if (i == 0) {
        return DurabilityRgb{1.0f, f, 0.0f};
    }
    if (i == 1) {
        return DurabilityRgb{1.0f - f, 1.0f, 0.0f};
    }
    // Unreachable: h in (0, 1/3) gives h6 in (0, 2), so i is 0 or 1. Kept as
    // a value return (rather than [[unreachable]]) so floating-point edge
    // cases slipping past the guards still yield a sane green instead of UB.
    return DurabilityRgb{0.0f, 1.0f, f};
}

// Bar shape, in the same GUI units as PreviewLayout, measured pixel-exactly
// against the vanilla inventory slot on 1.26.51 (2 px per GUI unit, damaged
// tools/armour in the player inventory next to the preview). Vanilla's bar is
// `common.durability_bar`: a `progress_bar_renderer` of size [12, 1] at
// offset [0, 5] from the centre of the 18-unit cell, with `drop_shadow` and
// `round_value`. On screen that is, relative to the 16x16 icon:
//   - a black strip 13 units wide x 2 tall, from x = 2 and y = 12.5
//     (the 1-unit bar plus its 1-unit drop shadow below/right of it);
//   - the coloured fill 1 unit tall on the strip's top row, left-aligned,
//     round(12 * ratio) units wide - so a nearly-broken item shows only the
//     black strip, exactly like the vanilla slot.
constexpr float kDurabilityBarLeft             = 2.0f;
constexpr float kDurabilityBarTop              = 12.5f;
constexpr float kDurabilityBarBackgroundWidth  = 13.0f;
constexpr float kDurabilityBarBackgroundHeight = 2.0f;
constexpr float kDurabilityBarForegroundWidth  = 12.0f;
constexpr float kDurabilityBarForegroundHeight = 1.0f;

/// Black strip the coloured fill is drawn over (vanilla bar + drop shadow).
[[nodiscard]] constexpr Rect durabilityBackground(Rect icon) {
    return Rect{
        icon.x0 + kDurabilityBarLeft,
        icon.y0 + kDurabilityBarTop,
        icon.x0 + kDurabilityBarLeft + kDurabilityBarBackgroundWidth,
        icon.y0 + kDurabilityBarTop + kDurabilityBarBackgroundHeight
    };
}

/// Coloured fill for `ratio` remaining durability: the top row of the strip,
/// left-aligned, a whole number of units wide (vanilla rounds the 12-unit bar
/// to the nearest unit, so ratios below 1/24 draw no fill at all).
[[nodiscard]] constexpr Rect durabilityForeground(Rect background, float ratio) {
    float units = 0.0f;
    if (ratio > 0.0f) {
        units = static_cast<float>(static_cast<int>(kDurabilityBarForegroundWidth * ratio + 0.5f));
    }
    if (units > kDurabilityBarForegroundWidth) {
        units = kDurabilityBarForegroundWidth;
    }
    return Rect{background.x0, background.y0, background.x0 + units, background.y0 + kDurabilityBarForegroundHeight};
}

} // namespace lamium::inspection::render

