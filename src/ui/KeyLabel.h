#pragma once
#include <string>
#include <string_view>

// Key names in the settings table's key column (L-235). The game spells the
// two long modifier names out; the column is narrow, so Alt and Ctrl use
// their usual short spelling. Shift already fits and keeps the game's name.
namespace lamium::ui {
// Mouse buttons 1-3 are the game font's own mouse glyphs (U+E070-E072, the light set, which reads on a
// dark cap; wiki.bedrock.dev "Emojis"): short everywhere text goes, the key column, footers and the
// search field alike (L-372). The side buttons have no glyph and keep their number. Empty: not one.
inline std::string mouseGlyph(int button) {
    switch (button) {
    case 1: return "\uE070";
    case 2: return "\uE071";
    case 3: return "\uE072";
    default: return {};
    }
}
inline std::string keyLabel(int code, std::string_view mapped) {
    if (code == 0x12 || code == 0xA4 || code == 0xA5) return "ALT";
    if (code == 0x11 || code == 0xA2 || code == 0xA3) return "CTRL";
    return std::string{mapped};
}
}
