// Focused checks for the pure durability-overlay math used by the preview.
// Built with `xmake build LamiumTests`, run with `xmake run LamiumTests`.

#include "features/inspection/render/DurabilityBar.h"

#include <cmath>
#include <cstdio>

using lamium::inspection::render::DurabilityRgb;
using lamium::inspection::render::Rect;
using lamium::inspection::render::durabilityBackground;
using lamium::inspection::render::durabilityColor;
using lamium::inspection::render::durabilityForeground;
using lamium::inspection::render::durabilityRatio;
using lamium::inspection::render::shouldShowDurabilityBar;

namespace {

int gFailures = 0;

bool near(float a, float b) { return std::fabs(a - b) < 1e-4f; }

bool nearRgb(DurabilityRgb const& actual, float r, float g, float b) {
    return near(actual.r, r) && near(actual.g, g) && near(actual.b, b);
}

void check(bool ok, char const* what, int line) {
    if (!ok) {
        ++gFailures;
        std::printf("FAIL (line %d): %s\n", line, what);
    }
}

#define CHECK(expr) check((expr), #expr, __LINE__)

void testRatioTracksRemainingDurability() {
    // A diamond pickaxe (max 1561): undamaged is full, half-used is half,
    // one use left is nearly zero, fully used is zero.
    CHECK(near(durabilityRatio(0, 1561), 1.0f));
    CHECK(near(durabilityRatio(781, 1562), 0.5f));
    CHECK(durabilityRatio(1560, 1561) > 0.0f);
    CHECK(near(durabilityRatio(1561, 1561), 0.0f));
}

void testRatioClampsAndGuards() {
    CHECK(near(durabilityRatio(-5, 100), 1.0f));   // over-repaired clamps to full
    CHECK(near(durabilityRatio(150, 100), 0.0f));  // over-damaged clamps to empty
    CHECK(near(durabilityRatio(5, 0), 1.0f));      // degenerate max: no bar path divides by zero
    CHECK(near(durabilityRatio(5, -10), 1.0f));
}

void testVisibilityMatchesVanillaRule() {
    CHECK(!shouldShowDurabilityBar(true, 0, 1561));    // undamaged: no bar
    CHECK(shouldShowDurabilityBar(true, 1, 1561));     // first hit: bar appears
    CHECK(shouldShowDurabilityBar(true, 1561, 1561));  // fully used: bar still shown
    CHECK(!shouldShowDurabilityBar(false, 0, 0));      // non-damageable (dirt): no bar
    CHECK(!shouldShowDurabilityBar(false, 10, 0));     // damage value without max: no bar
    CHECK(!shouldShowDurabilityBar(true, 5, 0));       // degenerate max: no bar
}

void testColorRampIsGreenYellowRed() {
    CHECK(nearRgb(durabilityColor(1.0f), 0.0f, 1.0f, 0.0f)); // full: green
    CHECK(nearRgb(durabilityColor(0.5f), 1.0f, 1.0f, 0.0f)); // half: yellow
    CHECK(nearRgb(durabilityColor(0.0f), 1.0f, 0.0f, 0.0f)); // empty: red
    // Nearly-broken must stay red-dominant (distinguishable warning color).
    DurabilityRgb const low = durabilityColor(0.05f);
    CHECK(low.r > 0.9f && low.g < 0.4f && near(low.b, 0.0f));
}

void testBarGeometryMatchesVanillaSlot() {
    // Measured on 1.26.51 (2 px/unit): black strip x 2..15, y 12.5..14.5 of
    // the 16x16 icon.
    Rect const icon{10.0f, 20.0f, 26.0f, 36.0f};
    Rect const bg = durabilityBackground(icon);
    CHECK(near(bg.x0, icon.x0 + 2.0f) && near(bg.x1, icon.x0 + 15.0f));
    CHECK(near(bg.y0, icon.y0 + 12.5f) && near(bg.y1, icon.y0 + 14.5f));
    CHECK(near(bg.width(), 13.0f) && near(bg.height(), 2.0f));
}

void testForegroundWidthRoundsLikeVanilla() {
    Rect const bg{0.0f, 0.0f, 13.0f, 2.0f};
    // Fill is the strip's top row only, left-aligned.
    Rect const half = durabilityForeground(bg, 0.5f);
    CHECK(near(half.x0, bg.x0) && near(half.y0, bg.y0) && near(half.height(), 1.0f));
    // Widths observed in the vanilla slot for the fixtures used in-game:
    CHECK(near(durabilityForeground(bg, 0.5f).width(), 6.0f));            // iron chestplate 120/240
    CHECK(near(durabilityForeground(bg, 284.0f / 384.0f).width(), 9.0f)); // bow 100/384
    CHECK(near(durabilityForeground(bg, 29.0f / 59.0f).width(), 6.0f));   // wooden shovel 30/59
    CHECK(near(durabilityForeground(bg, 1261.0f / 1561.0f).width(), 10.0f)); // diamond sword 300/1561
    CHECK(near(durabilityForeground(bg, 50.0f / 250.0f).width(), 2.0f));  // iron sword 200/250
    CHECK(near(durabilityForeground(bg, 1.0f / 32.0f).width(), 0.0f));    // golden pickaxe 31/32: strip only
    // Full width never exceeds the 12-unit bar; zero remaining draws no fill.
    CHECK(near(durabilityForeground(bg, 1.0f).width(), 12.0f));
    CHECK(near(durabilityForeground(bg, 0.0f).width(), 0.0f));
    CHECK(near(durabilityForeground(bg, 0.0f).x0, bg.x0));
}

} // namespace

int runDurabilityBarTests() {
    testRatioTracksRemainingDurability();
    testRatioClampsAndGuards();
    testVisibilityMatchesVanillaRule();
    testColorRampIsGreenYellowRed();
    testBarGeometryMatchesVanillaSlot();
    testForegroundWidthRoundsLikeVanilla();
    if (gFailures == 0) {
        std::printf("DurabilityBar tests: all passed\n");
    } else {
        std::printf("DurabilityBar tests: %d failure(s)\n", gFailures);
    }
    return gFailures;
}

