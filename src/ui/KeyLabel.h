#pragma once
#include <string>
#include <string_view>

// Key names in the settings table's key column (L-235). The game spells the
// long modifier names out; the column is narrow, so Alt, Ctrl and Shift use
// their usual short spelling, and the word the game adds after a key's name
// in Japanese ("C キー") is dropped (L-372).
namespace lamium::ui {
// The translation key naming mouse buttons 1-3 by what they are (L-372); the
// side buttons keep their number ("mouseButton"). Empty: not one of the three.
inline std::string_view mouseNameKey(int button) {
    switch (button) {
    case 1: return "mouse.left";
    case 2: return "mouse.right";
    case 3: return "mouse.middle";
    default: return {};
    }
}
// The game names a key it has no name for "Button" and its code ("Button226": the JIS ろ key); such a
// key takes Windows' name instead, where Windows has one (maintainer 2026-11-23).
inline bool unnamedByGame(std::string_view mapped) {
    constexpr std::string_view prefix = "Button";
    if (!mapped.starts_with(prefix) || mapped.size() == prefix.size()) return false;
    for (char c : mapped.substr(prefix.size())) if (c < '0' || c > '9') return false;
    return true;
}
inline std::string keyLabel(int code, std::string_view mapped) {
    if (code == 0x12 || code == 0xA4 || code == 0xA5) return "ALT";
    if (code == 0x11 || code == 0xA2 || code == 0xA3) return "CTRL";
    if (code == 0x10 || code == 0xA0 || code == 0xA1) return "SHIFT";
    for (std::string_view suffix : {std::string_view{" キー"}, std::string_view{"キー"}})
        if (mapped.size() > suffix.size() && mapped.ends_with(suffix)) {
            mapped.remove_suffix(suffix.size());
            break;
        }
    return std::string{mapped};
}
}
