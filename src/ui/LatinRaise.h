#pragma once

namespace lamium::ui {
// How far Latin letters, digits and signs are lifted so they share a line with the language's own glyphs, in GUI
// units (BACKLOG L-319). The setting counts quarter units; -1 follows the game language.
inline constexpr int latinRaiseAuto = -1, latinRaiseMax = 11;
// Lamium's own value for a language: Japanese glyphs sit higher than the Latin ones of the same font.
inline constexpr float languageLatinRaise(bool japanese) { return japanese ? 1.5f : 0.f; }
inline constexpr float latinRaiseUnits(int setting, bool japanese) {
    if (setting < 0) return languageLatinRaise(japanese);
    return static_cast<float>(setting > latinRaiseMax ? latinRaiseMax : setting) * .25f;
}
}
